#pragma once

#include "model/TeletypeProgram.h"
#include "model/TeletypeRuntime.h"
#include "TeletypeOutputState.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

extern "C" {
#include "ops/op_enum.h"
}

enum class TT2EvalError : uint8_t {
    None = 0,
    UnsupportedMod,
    UnsupportedSeparator,
    StackUnderflow,
    StackOverflow,
    UnknownOp,
    UnsupportedOp,
    OutOfRange,
    InvalidScriptIndex,
    ScriptLengthOverflow,
    InvalidModArity,
    NoTrack,
};

// Guarded stack helpers used by the evaluator and native op handlers.
inline bool pushStack(int16_t *stack, uint8_t &stackSize, int16_t value,
                      TT2EvalError &error) {
    if (stackSize >= TT2_COMMAND_MAX_LENGTH) {
        error = TT2EvalError::StackOverflow;
        return false;
    }
    stack[stackSize++] = value;
    return true;
}

inline bool popStack(int16_t *stack, uint8_t &stackSize, int16_t &value,
                     TT2EvalError &error) {
    if (stackSize == 0) {
        error = TT2EvalError::StackUnderflow;
        return false;
    }
    value = stack[--stackSize];
    return true;
}

struct TT2EvalResult {
    TT2EvalError error;
    int16_t value;    // top of stack if stackSize > 0, else 0
    uint8_t stackSize;
};

// Native v2 op handler signature.
// - isSetPosition is true when the OP is the first token of its sub-segment.
// - Handlers read/write TT2Runtime and TT2OutputState directly.
// - Set vs get is decided by each handler: set when isSetPosition && there
//   are remaining args on the stack, otherwise get.
using TT2OpFunc = void (*)(TT2Runtime &runtime, TT2OutputState &output,
                           int16_t *stack, uint8_t &stackSize,
                           bool isSetPosition, TT2EvalError &error);

// Global native op table indexed by parser op enum (tele_op_idx_t).
// Size is E_OP__LENGTH; unimplemented entries are nullptr.
extern const TT2OpFunc *tt2NativeOpTable;
extern const size_t tt2NativeOpCount;

// Evaluate a contiguous token range [start, end) as a flat segment.
// Only NUMBER / XNUMBER / BNUMBER / RNUMBER and OP tokens are allowed.
// MOD, PRE_SEP, SUB_SEP must not appear in this range.
// Right-to-left evaluation, fresh stack.
inline TT2EvalResult evaluateSegment(const TT2Command &cmd,
                                     uint8_t start, uint8_t end,
                                     TT2Runtime &runtime,
                                     TT2OutputState &output,
                                     const TT2OpFunc *table,
                                     size_t tableCount) {
    int16_t stack[TT2_COMMAND_MAX_LENGTH];
    uint8_t stackSize = 0;
    TT2EvalError error = TT2EvalError::None;

    for (int16_t idx = end - 1; idx >= int16_t(start); --idx) {
        if (error != TT2EvalError::None) {
            break;
        }

        uint8_t tag = cmd.tag[idx];
        int16_t value = cmd.value[idx];

        if (tag == uint8_t(NUMBER) || tag == uint8_t(XNUMBER) ||
            tag == uint8_t(BNUMBER) || tag == uint8_t(RNUMBER)) {
            pushStack(stack, stackSize, value, error);
        } else if (tag == uint8_t(OP)) {
            if (value < 0 || static_cast<size_t>(value) >= tableCount) {
                error = TT2EvalError::UnknownOp;
            } else {
                const TT2OpFunc op = table[value];
                if (op == nullptr) {
                    error = TT2EvalError::UnsupportedOp;
                } else {
                    bool isSet = (idx == start);
                    op(runtime, output, stack, stackSize, isSet, error);
                }
            }
        } else {
            // Should not happen in a properly extracted segment.
            error = TT2EvalError::UnsupportedSeparator;
        }
    }

    int16_t topValue =
        (stackSize > 0 && error == TT2EvalError::None) ? stack[stackSize - 1] : 0;
    return {error, topValue, stackSize};
}

// Convenience wrapper using the global native op table.
inline TT2EvalResult evaluateSegment(const TT2Command &cmd,
                                     uint8_t start, uint8_t end,
                                     TT2Runtime &runtime,
                                     TT2OutputState &output) {
    return evaluateSegment(cmd, start, end, runtime, output,
                           tt2NativeOpTable, tt2NativeOpCount);
}

// Evaluate one flat TT2Command through the native v2 op table.
//
// - Splits the command at SUB_SEP (`;`) into segments, executes left-to-right.
// - Each segment gets its own stack (no carry-over between segments).
// - Within a segment, PRE_SEP (`:`) splits prefix (mod condition) from body.
// - Supported mods: IF (condition != 0 executes body).
// - Unsupported mods return UnsupportedMod; prefix is not evaluated.
// - Stops at first error and returns it.
inline TT2EvalResult evaluateCommand(const TT2Command &cmd,
                                     TT2Runtime &runtime,
                                     TT2OutputState &output) {
    // Split at SUB_SEP into segments.
    uint8_t segStart[TT2_COMMAND_MAX_LENGTH];
    uint8_t segEnd[TT2_COMMAND_MAX_LENGTH];
    uint8_t segCount = 0;

    uint8_t currentStart = 0;
    for (uint8_t i = 0; i < cmd.length; ++i) {
        if (cmd.tag[i] == uint8_t(SUB_SEP)) {
            segStart[segCount] = currentStart;
            segEnd[segCount] = i;
            segCount++;
            currentStart = i + 1;
        }
    }
    // Last segment (or whole command if no SUB_SEP).
    segStart[segCount] = currentStart;
    segEnd[segCount] = cmd.length;
    segCount++;

    TT2EvalResult lastResult = {TT2EvalError::None, 0, 0};

    for (uint8_t seg = 0; seg < segCount; ++seg) {
        uint8_t s = segStart[seg];
        uint8_t e = segEnd[seg];

        // Skip empty segments (e.g. trailing `;`).
        if (s >= e) {
            continue;
        }

        // Find PRE_SEP within this segment.
        uint8_t preSepPos = e; // not found sentinel
        for (uint8_t i = s; i < e; ++i) {
            if (cmd.tag[i] == uint8_t(PRE_SEP)) {
                preSepPos = i;
                break;
            }
        }

        if (preSepPos < e) {
            // Prefix [s .. preSepPos), body [preSepPos+1 .. e).
            if (cmd.tag[s] != uint8_t(MOD)) {
                return {TT2EvalError::UnsupportedMod, 0, 0};
            }
            int16_t modValue = cmd.value[s];

            // Reject unsupported mods before evaluating prefix.
            if (modValue != E_MOD_IF) {
                return {TT2EvalError::UnsupportedMod, 0, 0};
            }

            // Evaluate prefix expression (skip the MOD token itself).
            auto prefix = evaluateSegment(cmd, s + 1, preSepPos, runtime, output);
            if (prefix.error != TT2EvalError::None) {
                return prefix;
            }

            // Enforce mod arity: prefix must produce exactly one value.
            if (prefix.stackSize != 1) {
                return {TT2EvalError::InvalidModArity, 0, 0};
            }

            bool executeBody = (prefix.value != 0);
            if (executeBody) {
                lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                             runtime, output);
                if (lastResult.error != TT2EvalError::None) {
                    return lastResult;
                }
            }
        } else {
            // No PRE_SEP: plain segment.
            lastResult = evaluateSegment(cmd, s, e, runtime, output);
            if (lastResult.error != TT2EvalError::None) {
                return lastResult;
            }
        }
    }

    return lastResult;
}

// Run one script line-by-line through the native evaluator.
//
// - Validates scriptIndex and script length bounds.
// - Sets execution context (script_number, line_number) in TT2ExecState.
// - Executes lines 0 .. script.length-1; zero-length lines are skipped.
// - Stops on first evaluator/op error and returns the result.
// - No nested script calls, no delay.
inline TT2EvalResult runScript(const TeletypeProgram &program,
                               TT2Runtime &runtime,
                               TT2OutputState &output,
                               uint8_t scriptIndex) {
    if (scriptIndex >= TT2_SCRIPT_COUNT) {
        return {TT2EvalError::InvalidScriptIndex, 0, 0};
    }

    const TT2Script &script = program.scripts[scriptIndex];
    if (script.length > TT2_COMMANDS_PER_SCRIPT) {
        return {TT2EvalError::ScriptLengthOverflow, 0, 0};
    }

    // Push one execution frame for the script context.
    runtime.exec.depth = 1;
    runtime.exec.overflow = 0;
    TT2ExecFrame &frame = runtime.exec.frames[0];
    memset(&frame, 0, sizeof(TT2ExecFrame));
    frame.script_number = scriptIndex;

    for (uint8_t line = 0; line < script.length; ++line) {
        const TT2Command &cmd = script.commands[line];
        if (cmd.length == 0) {
            continue; // blank line is a no-op
        }

        frame.line_number = line;
        TT2EvalResult result = evaluateCommand(cmd, runtime, output);
        if (result.error != TT2EvalError::None) {
            return result;
        }
    }

    return {TT2EvalError::None, 0, 0};
}
