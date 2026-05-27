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
    OrphanElse,
    OrphanElif,
    DuplicateElse,
};

// Guarded stack helpers used by the native v2 evaluator and op handlers.
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
    int16_t underTop; // second value if stackSize >= 2, else 0
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

// Global native v2 op table indexed by parser op enum (tele_op_idx_t).
// Size is E_OP__LENGTH; unimplemented entries are nullptr.
extern const TT2OpFunc *tt2NativeOpTable;
extern const size_t tt2NativeOpCount;

// Evaluate a contiguous token range [start, end) as a flat segment.
// Only NUMBER / XNUMBER / BNUMBER / RNUMBER and OP tokens are allowed.
// MOD, PRE_SEP, SUB_SEP must not appear in this range.
// Right-to-left evaluation, fresh stack.
// When forceGet is true, the first (leftmost) op is treated as a getter
// regardless of context; used for mod prefix expressions.
inline TT2EvalResult evaluateSegment(const TT2Command &cmd,
                                     uint8_t start, uint8_t end,
                                     TT2Runtime &runtime,
                                     TT2OutputState &output,
                                     const TT2OpFunc *table,
                                     size_t tableCount,
                                     bool forceGet = false) {
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
                    bool isSet = forceGet ? false : (idx == start);
                    op(runtime, output, stack, stackSize, isSet, error);
                }
            }
        } else {
            error = TT2EvalError::UnsupportedSeparator;
        }
    }

    int16_t topValue =
        (stackSize > 0 && error == TT2EvalError::None) ? stack[stackSize - 1] : 0;
    int16_t underTopValue =
        (stackSize >= 2 && error == TT2EvalError::None) ? stack[stackSize - 2] : 0;
    return {error, topValue, underTopValue, stackSize};
}

// Convenience wrapper using the global native v2 op table.
inline TT2EvalResult evaluateSegment(const TT2Command &cmd,
                                     uint8_t start, uint8_t end,
                                     TT2Runtime &runtime,
                                     TT2OutputState &output) {
    return evaluateSegment(cmd, start, end, runtime, output,
                           tt2NativeOpTable, tt2NativeOpCount, false);
}

// Convenience: evaluate a mod prefix expression (force-get all ops).
inline TT2EvalResult evaluateModPrefix(const TT2Command &cmd,
                                       uint8_t start, uint8_t end,
                                       TT2Runtime &runtime,
                                       TT2OutputState &output) {
    return evaluateSegment(cmd, start, end, runtime, output,
                           tt2NativeOpTable, tt2NativeOpCount, true);
}

// Evaluate one flat TT2Command through the native v2 op table.
//
// - Splits the command at SUB_SEP (`;`) into segments, executes left-to-right.
// - Each segment gets its own stack (no carry-over between segments).
// - Within a segment, PRE_SEP (`:`) splits prefix (mod condition) from body.
// - Supported mods: IF, ELIF, ELSE, PROB, L.
//   IF starts a new conditional chain; ELIF and ELSE continue it.
//   First true branch runs, later branches are skipped.
//   L start end: body — loop body (remaining segments) with I = start..end.
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

    enum class ChainState : uint8_t {
        None,     // not inside a conditional chain
        Pending,  // inside chain, no branch taken yet
        Taken,    // a branch was already executed
        ElseSeen, // ELSE encountered (branch executed or skipped)
    };

    TT2EvalResult lastResult = {TT2EvalError::None, 0, 0, 0};
    ChainState chainState = ChainState::None;

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
                return {TT2EvalError::UnsupportedMod, 0, 0, 0};
            }
            int16_t modValue = cmd.value[s];

            if (modValue == E_MOD_IF) {
                // IF always starts a fresh conditional chain.
                chainState = ChainState::None;

                // Evaluate prefix expression (skip the MOD token itself).
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output);
                if (prefix.error != TT2EvalError::None) {
                    return prefix;
                }
                if (prefix.stackSize != 1) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }

                if (prefix.value != 0) {
                    lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                                 runtime, output);
                    if (lastResult.error != TT2EvalError::None) {
                        return lastResult;
                    }
                    chainState = ChainState::Taken;
                } else {
                    chainState = ChainState::Pending;
                }
            } else if (modValue == E_MOD_ELIF) {
                if (chainState == ChainState::None) {
                    return {TT2EvalError::OrphanElif, 0, 0, 0};
                }
                if (chainState == ChainState::Taken ||
                    chainState == ChainState::ElseSeen) {
                    // A prior branch was already selected; skip this one.
                    continue;
                }
                // chainState == Pending
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output);
                if (prefix.error != TT2EvalError::None) {
                    return prefix;
                }
                if (prefix.stackSize != 1) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }

                if (prefix.value != 0) {
                    lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                                 runtime, output);
                    if (lastResult.error != TT2EvalError::None) {
                        return lastResult;
                    }
                    chainState = ChainState::Taken;
                }
                // else remain Pending
            } else if (modValue == E_MOD_ELSE) {
                if (chainState == ChainState::None) {
                    return {TT2EvalError::OrphanElse, 0, 0, 0};
                }
                if (chainState == ChainState::ElseSeen) {
                    return {TT2EvalError::DuplicateElse, 0, 0, 0};
                }
                if (chainState == ChainState::Taken) {
                    chainState = ChainState::ElseSeen;
                    continue;
                }
                // chainState == Pending
                lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                             runtime, output);
                if (lastResult.error != TT2EvalError::None) {
                    return lastResult;
                }
                chainState = ChainState::ElseSeen;
            } else if (modValue == E_MOD_PROB) {
                // PROB participates in conditional chains like IF/ELIF.
                if (chainState == ChainState::Taken ||
                    chainState == ChainState::ElseSeen) {
                    continue;
                }

                // PROB n: body  —  execute body with probability n%.
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output);
                if (prefix.error != TT2EvalError::None) {
                    return prefix;
                }
                if (prefix.stackSize != 1) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }

                int16_t pct = prefix.value;
                bool executeBody = false;
                if (pct >= 100) {
                    executeBody = true;
                } else if (pct > 0) {
                    uint32_t roll = tt2RngRange(runtime.rng,
                                                TT2RngSlot::Prob, 100);
                    executeBody = (roll < static_cast<uint32_t>(pct));
                }
                // pct <= 0: executeBody stays false

                if (executeBody) {
                    lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                                 runtime, output);
                    if (lastResult.error != TT2EvalError::None) {
                        return lastResult;
                    }
                    chainState = ChainState::Taken;
                } else {
                    chainState = ChainState::Pending;
                }
            } else if (modValue == E_MOD_L) {
                // L start end: body  —  loop body (remaining segments)
                // with I stepping from start to end, inclusive.
                // Reversed and equal bounds are valid.
                // I is set once before the loop; body can read and mutate it.
                // I is left at the final in-range value (not restored).
                // Body consumes remaining segments; outer loop must stop.
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output);
                if (prefix.error != TT2EvalError::None) {
                    return prefix;
                }
                if (prefix.stackSize != 2) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }

                int16_t startVal = prefix.value;
                int16_t endVal = prefix.underTop;

                int16_t step = (endVal >= startVal) ? 1 : -1;

                // Build body command from after ':' through end of command.
                TT2Command bodyCmd = {};
                uint8_t bodyLen = 0;
                for (uint8_t pos = preSepPos + 1;
                     pos < cmd.length && bodyLen < TT2_COMMAND_MAX_LENGTH;
                     ++pos) {
                    bodyCmd.tag[bodyLen] = cmd.tag[pos];
                    bodyCmd.value[bodyLen] = cmd.value[pos];
                    bodyLen++;
                }
                bodyCmd.length = bodyLen;

                runtime.variables.i = startVal;
                // Use int32_t to handle range edge cases (e.g. end == 32767).
                if (step > 0) {
                    for (int32_t l = startVal; l <= endVal; ++l) {
                        lastResult = evaluateCommand(bodyCmd, runtime,
                                                     output);
                        if (lastResult.error != TT2EvalError::None) {
                            return lastResult;
                        }
                        runtime.variables.i += step;
                    }
                } else {
                    for (int32_t l = startVal; l >= endVal; --l) {
                        lastResult = evaluateCommand(bodyCmd, runtime,
                                                     output);
                        if (lastResult.error != TT2EvalError::None) {
                            return lastResult;
                        }
                        runtime.variables.i += step;
                    }
                }
                // Back off the final post-loop increment; leave I at
                // the last in-range value, matching Teletype behavior.
                runtime.variables.i -= step;

                // L consumed all remaining segments; return directly.
                return lastResult;
            } else {
                // Unsupported mod — reject before prefix side effects.
                return {TT2EvalError::UnsupportedMod, 0, 0, 0};
            }
        } else {
            // No PRE_SEP: plain segment ends any active conditional chain.
            if (chainState != ChainState::None) {
                chainState = ChainState::None;
            }
            lastResult = evaluateSegment(cmd, s, e, runtime, output);
            if (lastResult.error != TT2EvalError::None) {
                return lastResult;
            }
        }
    }

    return lastResult;
}
