#pragma once

#include "TT2Evaluator.h"

// Run one script line-by-line through the native v2 evaluator.
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
