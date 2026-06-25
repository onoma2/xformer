#pragma once

#include "TT2Evaluator.h"

// Run one script line-by-line through the native v2 evaluator.
//
// - Validates scriptIndex and script length bounds.
// - Pushes one exec frame on entry, pops it on return (success or error).
// - Executes lines 0 .. script.length-1; zero-length lines are skipped.
// - Stops on first evaluator/op error and returns it.
// - Supports nested SCRIPT calls up to TT2_EXEC_DEPTH frames.
template<typename Cfg>
inline TT2EvalResult runScript(const TeletypeProgramT<Cfg> &program,
                               TT2RuntimeT<Cfg> &runtime,
                               TT2OutputState &output,
                               uint8_t scriptIndex) {
    if (scriptIndex >= Cfg::ScriptCount) {
        return {TT2EvalError::InvalidScriptIndex, 0, 0, 0};
    }

    const TT2Script &script = program.scripts[scriptIndex];
    if (script.length > TT2_COMMANDS_PER_SCRIPT) {
        return {TT2EvalError::ScriptLengthOverflow, 0, 0, 0};
    }

    if (runtime.exec.depth >= TT2_EXEC_DEPTH) {
        runtime.exec.overflow = 1;
        return {TT2EvalError::ExecDepthOverflow, 0, 0, 0};
    }

    // Stamp the run time for LAST (clockMs advances in the engine update).
    runtime.scriptLastMs[scriptIndex] = runtime.clockMs;

    // Push one execution frame.
    TT2ExecFrame &frame = runtime.exec.frames[runtime.exec.depth];
    memset(&frame, 0, sizeof(TT2ExecFrame));
    frame.script_number = scriptIndex;
    runtime.exec.depth++;

    for (uint8_t line = 0; line < script.length; ++line) {
        const TT2Command &cmd = script.commands[line];
        if (cmd.length == 0 || cmd.commented) {
            continue; // blank line or commented-out line is a no-op
        }

        frame.line_number = line;
        TT2EvalResult result = evaluateCommand(cmd, runtime, output, &program);
        if (result.error != TT2EvalError::None) {
            runtime.exec.depth--;
            return result;
        }
    }

    runtime.exec.depth--;
    return {TT2EvalError::None, 0, 0, 0};
}

// Run a script as a function: push a frame carrying the input params, execute
// all lines, return the frame's fresult (0 if the body never set FR). Mirrors
// upstream execute_function (es_push_fparams -> run -> fresult).
template<typename Cfg>
inline int16_t tt2RunFunction(const TeletypeProgramT<Cfg> &program, TT2RuntimeT<Cfg> &runtime,
                              TT2OutputState &output, uint8_t scriptIndex,
                              int16_t param1, int16_t param2) {
    if (scriptIndex >= Cfg::ScriptCount) return 0;
    const TT2Script &script = program.scripts[scriptIndex];
    if (script.length > TT2_COMMANDS_PER_SCRIPT) return 0;
    if (runtime.exec.depth >= TT2_EXEC_DEPTH) { runtime.exec.overflow = 1; return 0; }

    TT2ExecFrame &frame = runtime.exec.frames[runtime.exec.depth];
    memset(&frame, 0, sizeof(TT2ExecFrame));
    frame.script_number = scriptIndex;
    frame.fparam1 = param1;
    frame.fparam2 = param2;
    runtime.scriptLastMs[scriptIndex] = runtime.clockMs;
    runtime.exec.depth++;

    for (uint8_t line = 0; line < script.length; ++line) {
        const TT2Command &cmd = script.commands[line];
        if (cmd.length == 0 || cmd.commented) continue;
        frame.line_number = line;
        TT2EvalResult result = evaluateCommand(cmd, runtime, output, &program);
        if (result.error != TT2EvalError::None) break;
    }

    int16_t fresult = frame.fresult_set ? frame.fresult : 0;
    runtime.exec.depth--;
    return fresult;
}

// Run a single line of a script as a function (upstream execute_function_line).
template<typename Cfg>
inline int16_t tt2RunFunctionLine(const TeletypeProgramT<Cfg> &program, TT2RuntimeT<Cfg> &runtime,
                                  TT2OutputState &output, uint8_t scriptIndex,
                                  uint8_t line, int16_t param1, int16_t param2) {
    if (scriptIndex >= Cfg::ScriptCount) return 0;
    const TT2Script &script = program.scripts[scriptIndex];
    if (line >= script.length) return 0;
    if (runtime.exec.depth >= TT2_EXEC_DEPTH) { runtime.exec.overflow = 1; return 0; }

    TT2ExecFrame &frame = runtime.exec.frames[runtime.exec.depth];
    memset(&frame, 0, sizeof(TT2ExecFrame));
    frame.script_number = scriptIndex;
    frame.fparam1 = param1;
    frame.fparam2 = param2;
    frame.line_number = line;
    runtime.exec.depth++;

    const TT2Command &cmd = script.commands[line];
    if (cmd.length != 0 && !cmd.commented) {
        evaluateCommand(cmd, runtime, output, &program);
    }

    int16_t fresult = frame.fresult_set ? frame.fresult : 0;
    runtime.exec.depth--;
    return fresult;
}

// Advance the delay queue by deltaMs (real elapsed milliseconds). Each live
// slot's remaining time is decremented; due bodies fire via the evaluator with
// the caller's origin context restored. Faithful to upstream tele_tick():
// the firing slot is held busy (time=1) while its body runs so a body that
// re-enqueues a delay won't reuse the slot, then freed.
template<typename Cfg>
inline void tt2AdvanceDelays(const TeletypeProgramT<Cfg> &program, TT2RuntimeT<Cfg> &runtime,
                             TT2OutputState &output, int deltaMs) {
    if (deltaMs <= 0) {
        return;
    }
    for (int i = 0; i < Cfg::DelayDepth; ++i) {
        TT2DelayEntry &e = runtime.delay.entries[i];
        if (e.time == 0) {
            continue;
        }
        int32_t remaining = int32_t(e.time) - deltaMs;
        if (remaining > 0) {
            e.time = int16_t(remaining);
            continue;
        }
        // Due — snapshot body + origin, hold the slot busy, run, then free.
        e.time = 1;
        TT2RuntimeCommand body = e.command;
        uint8_t originScript = e.originScript;
        int16_t originI = e.originI;
        int16_t fparam1 = e.originFParam1;
        int16_t fparam2 = e.originFParam2;

        if (runtime.exec.depth < TT2_EXEC_DEPTH) {
            TT2ExecFrame &frame = runtime.exec.frames[runtime.exec.depth];
            memset(&frame, 0, sizeof(TT2ExecFrame));
            frame.script_number = originScript;
            frame.i = originI;
            frame.fparam1 = fparam1;
            frame.fparam2 = fparam2;
            frame.delayed = 1;
            ++runtime.exec.depth;

            TT2Command cmd;
            cmd.length = body.length;
            cmd.commented = 0;
            for (int k = 0; k < TT2_COMMAND_MAX_LENGTH; ++k) {
                cmd.tag[k] = body.tag[k];
                cmd.value[k] = body.value[k];
            }
            evaluateCommand(cmd, runtime, output, &program);
            --runtime.exec.depth;
        }

        e.time = 0;
        if (runtime.delay.count > 0) {
            --runtime.delay.count;
        }
    }
}

// Advance the metro by deltaMs and fire the METRO script every M ms while
// active. interval = variables.m (>=2ms), gated by variables.m_act; the
// accumulator resets when inactive (re-enable starts fresh). In clock-sync mode
// (M.C n d -> metroSyncDen>0) the ms period is derived from live BPM each tick:
// ms = num/den of a whole note = num * 240000 / (den * bpm). Empty metro = no-op.
template<typename Cfg>
inline void tt2AdvanceMetro(const TeletypeProgramT<Cfg> &program, TT2RuntimeT<Cfg> &runtime,
                            TT2OutputState &output, int deltaMs, int &metroAccumMs,
                            float bpm) {
    if (!runtime.variables.m_act) {
        metroAccumMs = 0;
        return;
    }
    if (program.scripts[Cfg::MetroScript].length == 0) {
        return;
    }
    if (runtime.variables.metroSyncDen > 0 && bpm > 0.f) {
        double ms = double(runtime.variables.metroSyncNum) * 240000.0
                  / (double(runtime.variables.metroSyncDen) * double(bpm));
        int derived = int(ms + 0.5);
        if (derived < 2) derived = 2;
        if (derived > 32767) derived = 32767;
        runtime.variables.m = int16_t(derived);
    }
    int interval = runtime.variables.m;
    if (interval < 2) interval = 2;
    metroAccumMs += deltaMs;
    int guard = 0;
    while (metroAccumMs >= interval && guard < 64) {
        metroAccumMs -= interval;
        runScript(program, runtime, output, uint8_t(Cfg::MetroScript));
        ++guard;
    }
}
