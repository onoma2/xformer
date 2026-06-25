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
    ExecDepthOverflow,
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
template<typename Cfg>
using TT2OpFuncT = void (*)(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                           const TeletypeProgramT<Cfg> *program,
                           int16_t *stack, uint8_t &stackSize,
                           bool isSetPosition, TT2EvalError &error);
using TT2OpFunc = TT2OpFuncT<TT2ConfigFull>;

template<typename Cfg> const TT2OpFuncT<Cfg> *tt2OpTable();
template<> const TT2OpFuncT<TT2ConfigFull> *tt2OpTable<TT2ConfigFull>();

// Non-deduced wrapper so Cfg is fixed by `runtime` alone; lets callers pass a
// bare `nullptr` program without breaking template argument deduction.
template<typename T> struct TT2Identity { typedef T type; };

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
template<typename Cfg>
inline TT2EvalResult evaluateSegment(const TT2Command &cmd,
                                     uint8_t start, uint8_t end,
                                     TT2RuntimeT<Cfg> &runtime,
                                     TT2OutputState &output,
                                     const TT2OpFuncT<Cfg> *table,
                                     size_t tableCount,
                                     bool forceGet = false,
                                     const TeletypeProgramT<Cfg> *program = nullptr,
                                     int16_t *outStack = nullptr,
                                     uint8_t *outSize = nullptr) {
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
                const TT2OpFuncT<Cfg> op = table[value];
                if (op == nullptr) {
                    error = TT2EvalError::UnsupportedOp;
                } else {
                    bool isSet = forceGet ? false : (idx == start);
                    op(runtime, output, program, stack, stackSize, isSet, error);
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
    if (outStack && outSize) {
        for (uint8_t i = 0; i < stackSize; ++i) outStack[i] = stack[i];
        *outSize = stackSize;
    }
    return {error, topValue, underTopValue, stackSize};
}

// Convenience wrapper using the global native v2 op table.
template<typename Cfg>
inline TT2EvalResult evaluateSegment(const TT2Command &cmd,
                                     uint8_t start, uint8_t end,
                                     TT2RuntimeT<Cfg> &runtime,
                                     TT2OutputState &output,
                                     const TeletypeProgramT<Cfg> *program = nullptr) {
    return evaluateSegment(cmd, start, end, runtime, output,
                           tt2OpTable<Cfg>(), tt2NativeOpCount, false, program);
}

// Convenience: evaluate a mod prefix expression (force-get all ops).
template<typename Cfg>
inline TT2EvalResult evaluateModPrefix(const TT2Command &cmd,
                                       uint8_t start, uint8_t end,
                                       TT2RuntimeT<Cfg> &runtime,
                                       TT2OutputState &output,
                                       const TeletypeProgramT<Cfg> *program = nullptr) {
    return evaluateSegment(cmd, start, end, runtime, output,
                           tt2OpTable<Cfg>(), tt2NativeOpCount, true, program);
}

// Like evaluateModPrefix but also copies the resulting stack out (bottom..top)
// for mods that need more than 2 args (DEL.G).
template<typename Cfg>
inline TT2EvalResult evaluateModPrefixStack(const TT2Command &cmd,
                                            uint8_t start, uint8_t end,
                                            TT2RuntimeT<Cfg> &runtime,
                                            TT2OutputState &output,
                                            int16_t *outStack, uint8_t *outSize,
                                            const TeletypeProgramT<Cfg> *program = nullptr) {
    return evaluateSegment(cmd, start, end, runtime, output,
                           tt2OpTable<Cfg>(), tt2NativeOpCount, true, program,
                           outStack, outSize);
}

// Capture the post-':' body of a DEL* command and enqueue it at timeMs with
// the active exec frame's origin context. Shared by every DEL variant.
template<typename Cfg>
inline void tt2EnqueueDelayBody(const TT2Command &cmd, uint8_t preSepPos,
                                TT2RuntimeT<Cfg> &runtime, int16_t timeMs) {
    TT2RuntimeCommand bodyCmd = {};
    uint8_t bodyLen = 0;
    for (uint8_t pos = preSepPos + 1;
         pos < cmd.length && bodyLen < TT2_COMMAND_MAX_LENGTH;
         ++pos) {
        bodyCmd.tag[bodyLen] = cmd.tag[pos];
        bodyCmd.value[bodyLen] = cmd.value[pos];
        bodyLen++;
    }
    bodyCmd.length = bodyLen;
    const TT2ExecFrame &f = runtime.exec.frames[
        runtime.exec.depth > 0 ? runtime.exec.depth - 1 : 0];
    tt2DelayAdd(runtime, bodyCmd, timeMs, f.script_number,
                f.i, f.fparam1, f.fparam2);
}

// Capture the post-':' body of an S command and push it onto the command stack
// for later execution via S.ALL / S.POP. Silently drops when the stack is full
// (upstream mod_S behaviour).
template<typename Cfg>
inline void tt2PushStackBody(const TT2Command &cmd, uint8_t preSepPos,
                             TT2RuntimeT<Cfg> &runtime) {
    if (runtime.stack.top >= TT2_STACK_DEPTH) {
        return;
    }
    TT2RuntimeCommand &dst = runtime.stack.commands[runtime.stack.top];
    uint8_t bodyLen = 0;
    for (uint8_t pos = preSepPos + 1;
         pos < cmd.length && bodyLen < TT2_COMMAND_MAX_LENGTH;
         ++pos) {
        dst.tag[bodyLen] = cmd.tag[pos];
        dst.value[bodyLen] = cmd.value[pos];
        bodyLen++;
    }
    dst.length = bodyLen;
    runtime.stack.top++;
}

// Clamp a 32-bit ms deadline into the int16 delay-time range [1, 32767].
inline int16_t tt2ClampDelayMs(int32_t ms) {
    if (ms < 1) ms = 1;
    if (ms > 32767) ms = 32767;
    return int16_t(ms);
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
template<typename Cfg>
inline TT2EvalResult evaluateCommand(const TT2Command &cmd,
                                     TT2RuntimeT<Cfg> &runtime,
                                     TT2OutputState &output,
                                     const typename TT2Identity<TeletypeProgramT<Cfg>>::type *program = nullptr) {
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

    // The IF/ELIF/ELSE chain flag lives on the exec frame (tt2ActiveIfElse),
    // so it persists across script lines exactly like upstream Teletype.
    TT2EvalResult lastResult = {TT2EvalError::None, 0, 0, 0};

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
                // IF cond: body — reset the chain flag, then set it + run the
                // body if cond is non-zero. (upstream mod_IF_func)
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
                if (prefix.error != TT2EvalError::None) {
                    return prefix;
                }
                if (prefix.stackSize != 1) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }
                tt2ActiveIfElse(runtime) = 0;
                if (prefix.value != 0) {
                    tt2ActiveIfElse(runtime) = 1;
                    lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                                 runtime, output, program);
                    if (lastResult.error != TT2EvalError::None) {
                        return lastResult;
                    }
                }
            } else if (modValue == E_MOD_ELIF) {
                // ELIF cond: body — prefix is ALWAYS evaluated (upstream pops
                // unconditionally); body runs only if the chain isn't taken yet
                // and cond is non-zero. No orphan error — a bare ELIF with the
                // flag false (default) just runs. (upstream mod_ELIF_func)
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
                if (prefix.error != TT2EvalError::None) {
                    return prefix;
                }
                if (prefix.stackSize != 1) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }
                if (!tt2ActiveIfElse(runtime) && prefix.value != 0) {
                    tt2ActiveIfElse(runtime) = 1;
                    lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                                 runtime, output, program);
                    if (lastResult.error != TT2EvalError::None) {
                        return lastResult;
                    }
                }
            } else if (modValue == E_MOD_ELSE) {
                // ELSE: body — run the body if the chain isn't taken yet (no
                // prefix). No orphan/duplicate error. (upstream mod_ELSE_func)
                if (!tt2ActiveIfElse(runtime)) {
                    tt2ActiveIfElse(runtime) = 1;
                    lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                                 runtime, output, program);
                    if (lastResult.error != TT2EvalError::None) {
                        return lastResult;
                    }
                }
            } else if (modValue == E_MOD_PROB) {
                // PROB n: body — standalone probability, NOT part of the IF/ELSE
                // chain (upstream mod_PROB_func never touches if_else_condition).
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
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
                if (executeBody) {
                    lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                                 runtime, output, program);
                    if (lastResult.error != TT2EvalError::None) {
                        return lastResult;
                    }
                }
            } else if (modValue == E_MOD_EVERY || modValue == E_MOD_EV ||
                       modValue == E_MOD_SKIP) {
                // EVERY MOD: body  —  tick per-(script,line) counter,
                // count++ then count %= mod, fire when count == 0.
                // SKIP MOD: body  —  same counter, fire when count != 0.
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
                if (prefix.error != TT2EvalError::None) {
                    return prefix;
                }
                if (prefix.stackSize != 1) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }
                int16_t mod = prefix.value;

                bool isSkip = (modValue == E_MOD_SKIP);
                uint8_t scriptNumber = tt2ActiveScriptNumber(runtime);
                uint8_t lineNumber = tt2ActiveLineNumber(runtime);

                if (scriptNumber < Cfg::ScriptCount &&
                    lineNumber < TT2_COMMANDS_PER_SCRIPT) {
                    TT2EveryCount &every =
                        runtime.every.every[scriptNumber][lineNumber];
                    tt2EverySetSkip(every, isSkip);
                    tt2EverySetMod(every, mod);
                    tt2EveryTick(every);
                    bool shouldRun = isSkip
                        ? tt2SkipIsNow(every)
                        : tt2EveryIsNow(every);
                    if (shouldRun) {
                        lastResult = evaluateSegment(cmd, preSepPos + 1, e,
                                                     runtime, output, program);
                        if (lastResult.error != TT2EvalError::None) {
                            return lastResult;
                        }
                    }
                }
            } else if (modValue == E_MOD_L) {
                // L start end: body  —  loop body (remaining segments)
                // with I stepping from start to end, inclusive.
                // Reversed and equal bounds are valid.
                // I is set once before the loop; body can read and mutate it.
                // I is left at the final in-range value (not restored).
                // Body consumes remaining segments; outer loop must stop.
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
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

                tt2ActiveI(runtime) = startVal;
                // Use int32_t to handle range edge cases (e.g. end == 32767).
                if (step > 0) {
                    for (int32_t l = startVal; l <= endVal; ++l) {
                        lastResult = evaluateCommand(bodyCmd, runtime,
                                                     output, program);
                        if (lastResult.error != TT2EvalError::None) {
                            return lastResult;
                        }
                        if (tt2ActiveBreaking(runtime)) break; // BREAK
                        tt2ActiveI(runtime) += step;
                    }
                } else {
                    for (int32_t l = startVal; l >= endVal; --l) {
                        lastResult = evaluateCommand(bodyCmd, runtime,
                                                     output, program);
                        if (lastResult.error != TT2EvalError::None) {
                            return lastResult;
                        }
                        if (tt2ActiveBreaking(runtime)) break; // BREAK
                        tt2ActiveI(runtime) += step;
                    }
                }
                // On normal completion back off the final post-loop increment
                // (leaving I at the last in-range value). On BREAK, I already
                // holds the break value — don't back off. Then clear the flag.
                if (!tt2ActiveBreaking(runtime)) {
                    tt2ActiveI(runtime) -= step;
                }
                tt2ActiveBreaking(runtime) = 0;

                // L consumed all remaining segments; return directly.
                return lastResult;
            } else if (modValue == E_MOD_DEL) {
                // DEL n: body — schedule body n ms later, once.
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
                if (prefix.error != TT2EvalError::None) return prefix;
                if (prefix.stackSize != 1) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }
                tt2EnqueueDelayBody(cmd, preSepPos, runtime,
                                    tt2ClampDelayMs(prefix.value));
                return {TT2EvalError::None, 0, 0, 0};
            } else if (modValue == E_MOD_DEL_X) {
                // DEL.X x delay_time: body — x copies at delay_time, 2·dt, 3·dt …
                // (upstream: num_delays = first pop = x; delay_time = second).
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
                if (prefix.error != TT2EvalError::None) return prefix;
                if (prefix.stackSize != 2) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }
                int16_t count = prefix.value;      // x (leftmost) = number of copies
                int32_t interval = prefix.underTop; // delay_time
                if (interval < 1) interval = 1;    // upstream clamps the operand
                for (int16_t k = 1; k <= count; ++k) {
                    tt2EnqueueDelayBody(cmd, preSepPos, runtime,
                                        tt2ClampDelayMs(int32_t(k) * interval));
                }
                return {TT2EvalError::None, 0, 0, 0};
            } else if (modValue == E_MOD_DEL_R) {
                // DEL.R n x: body — n copies, first at 1ms, then +x each.
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
                if (prefix.error != TT2EvalError::None) return prefix;
                if (prefix.stackSize != 2) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }
                int16_t count = prefix.value;      // n (leftmost)
                int32_t interval = prefix.underTop; // x
                if (interval < 1) interval = 1;    // upstream clamps the operand
                for (int16_t k = 0; k < count; ++k) {
                    tt2EnqueueDelayBody(cmd, preSepPos, runtime,
                                        tt2ClampDelayMs(1 + int32_t(k) * interval));
                }
                return {TT2EvalError::None, 0, 0, 0};
            } else if (modValue == E_MOD_DEL_B) {
                // DEL.B base mask: body — fire at i*base ms for each set bit i
                // of mask (bit 0 → 1ms via the <1 clamp).
                auto prefix = evaluateModPrefix(cmd, s + 1, preSepPos,
                                              runtime, output, program);
                if (prefix.error != TT2EvalError::None) return prefix;
                if (prefix.stackSize != 2) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }
                int32_t base = prefix.value;       // base (leftmost)
                if (base < 1) base = 1;            // upstream clamps base before i·base
                uint16_t mask = uint16_t(prefix.underTop);
                for (int i = 0; i < 16; ++i) {
                    if (mask & (1u << i)) {
                        tt2EnqueueDelayBody(cmd, preSepPos, runtime,
                                            tt2ClampDelayMs(int32_t(i) * base));
                    }
                }
                return {TT2EvalError::None, 0, 0, 0};
            } else if (modValue == E_MOD_DEL_G) {
                // DEL.G x delay_time num denom: body — x copies; first at 1ms,
                // then each deadline += interval and interval scales by num/denom
                // (geometric accel/decel). (upstream mod_DEL_G_func)
                int16_t args[TT2_COMMAND_MAX_LENGTH];
                uint8_t argc = 0;
                auto prefix = evaluateModPrefixStack(cmd, s + 1, preSepPos,
                                                     runtime, output, args, &argc,
                                                     program);
                if (prefix.error != TT2EvalError::None) return prefix;
                if (argc != 4) {
                    return {TT2EvalError::InvalidModArity, 0, 0, 0};
                }
                // Stack bottom..top = [denom, num, delay_time, x]; upstream pops
                // x, delay_time, num, denom (top first).
                int32_t count = args[3];      // x
                int32_t interval = args[2];   // delay_time
                int32_t multNum = args[1];    // num
                int32_t multDenom = args[0];  // denom
                if (interval < 1) interval = 1;
                int32_t deadline = 1;         // first fires immediately
                for (int32_t k = 0; k < count; ++k) {
                    tt2EnqueueDelayBody(cmd, preSepPos, runtime,
                                        tt2ClampDelayMs(deadline));
                    deadline += interval;
                    if (deadline > 32767) deadline = 32767;
                    if (multDenom != 0) {
                        interval = (interval * multNum) / multDenom;
                    }
                }
                return {TT2EvalError::None, 0, 0, 0};
            } else if (modValue == E_MOD_S) {
                // S: body — push the body onto the command stack for later
                // execution via S.ALL / S.POP. No prefix args. (upstream mod_S)
                tt2PushStackBody(cmd, preSepPos, runtime);
                return {TT2EvalError::None, 0, 0, 0};
            } else {
                // Unsupported mod — reject before prefix side effects.
                return {TT2EvalError::UnsupportedMod, 0, 0, 0};
            }
        } else {
            // No PRE_SEP: plain segment. Does NOT reset the IF/ELSE chain —
            // upstream only IF resets if_else_condition.
            lastResult = evaluateSegment(cmd, s, e, runtime, output, program);
            if (lastResult.error != TT2EvalError::None) {
                return lastResult;
            }
        }
    }

    return lastResult;
}
