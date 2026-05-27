#pragma once

#include "model/TeletypeProgram.h"
#include "model/TeletypeRuntime.h"
#include "TeletypeOutputState.h"

#include <cstddef>
#include <cstdint>

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
// - isSetPosition is true when the OP is the first token of its sub-segment
//   (for flat commands this is idx == 0).
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

// Evaluate a single flat TT2Command through the native v2 op table.
//
// - Processes tokens right-to-left to match Teletype postfix semantics.
// - NUMBER / XNUMBER / BNUMBER / RNUMBER push their value.
// - OP dispatches through tt2NativeOpTable.
// - MOD, PRE_SEP, and SUB_SEP are rejected (unsupported in this step).
// - No scene_state_t, no tele_*, no bridge callbacks.
inline TT2EvalResult evaluateCommand(const TT2Command &cmd,
                                     TT2Runtime &runtime,
                                     TT2OutputState &output) {
    int16_t stack[TT2_COMMAND_MAX_LENGTH];
    uint8_t stackSize = 0;
    TT2EvalError error = TT2EvalError::None;

    // Reject commands with mods or separators early.
    for (uint8_t i = 0; i < cmd.length; ++i) {
        uint8_t tag = cmd.tag[i];
        if (tag == uint8_t(MOD)) {
            return {TT2EvalError::UnsupportedMod, 0, 0};
        }
        if (tag == uint8_t(PRE_SEP) || tag == uint8_t(SUB_SEP)) {
            return {TT2EvalError::UnsupportedSeparator, 0, 0};
        }
    }

    // Evaluate right-to-left.
    // For flat commands the sub-segment starts at index 0.
    for (int16_t idx = cmd.length - 1; idx >= 0; --idx) {
        if (error != TT2EvalError::None) {
            break;
        }

        uint8_t tag = cmd.tag[idx];
        int16_t value = cmd.value[idx];

        if (tag == uint8_t(NUMBER) || tag == uint8_t(XNUMBER) ||
            tag == uint8_t(BNUMBER) || tag == uint8_t(RNUMBER)) {
            pushStack(stack, stackSize, value, error);
        } else if (tag == uint8_t(OP)) {
            if (value < 0 || static_cast<size_t>(value) >= tt2NativeOpCount) {
                error = TT2EvalError::UnknownOp;
            } else {
                const TT2OpFunc op = tt2NativeOpTable[value];
                if (op == nullptr) {
                    error = TT2EvalError::UnsupportedOp;
                } else {
                    bool isSet = (idx == 0);
                    op(runtime, output, stack, stackSize, isSet, error);
                }
            }
        }
    }

    int16_t topValue =
        (stackSize > 0 && error == TT2EvalError::None) ? stack[stackSize - 1] : 0;
    return {error, topValue, stackSize};
}

// Run one script line-by-line through the native evaluator.
//
// - Validates scriptIndex and script length bounds.
// - Sets execution context (script_number, line_number) in TT2ExecState.
// - Executes lines 0 .. script.length-1; zero-length lines are skipped.
// - Stops on first evaluator/op error and returns the result.
// - No nested script calls, no mods, no separators, no delay.
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
