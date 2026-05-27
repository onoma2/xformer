#pragma once

#include "TT2Evaluator.h"

// Run one script line-by-line through the native v2 evaluator.
//
// - Validates scriptIndex and script length bounds.
// - Pushes one exec frame on entry, pops it on return (success or error).
// - Executes lines 0 .. script.length-1; zero-length lines are skipped.
// - Stops on first evaluator/op error and returns it.
// - Supports nested SCRIPT calls up to TT2_EXEC_DEPTH frames.
inline TT2EvalResult runScript(const TeletypeProgram &program,
                               TT2Runtime &runtime,
                               TT2OutputState &output,
                               uint8_t scriptIndex) {
    if (scriptIndex >= TT2_SCRIPT_COUNT) {
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

    // Push one execution frame.
    TT2ExecFrame &frame = runtime.exec.frames[runtime.exec.depth];
    memset(&frame, 0, sizeof(TT2ExecFrame));
    frame.script_number = scriptIndex;
    runtime.exec.depth++;

    for (uint8_t line = 0; line < script.length; ++line) {
        const TT2Command &cmd = script.commands[line];
        if (cmd.length == 0) {
            continue; // blank line is a no-op
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
