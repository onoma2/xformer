#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

namespace {

TT2EvalResult evalText(const char *text, TT2Runtime &runtime, TT2OutputState &output) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output);
}

} // namespace

UNIT_TEST("TeletypeV2Input") {

    CASE("cv_input_source_defaults") {
        TeletypeProgram p;
        init(p);
        expectEqual(int(p.cvInputSource[int(TT2CvInput::In)]),    int(TT2CvInputSource::CvIn1), "IN -> CvIn1");
        expectEqual(int(p.cvInputSource[int(TT2CvInput::Param)]), int(TT2CvInputSource::CvIn2), "PARAM -> CvIn2");
        expectEqual(int(p.cvInputSource[int(TT2CvInput::X)]),     int(TT2CvInputSource::None),  "X -> None (scratch)");
        expectEqual(int(p.cvInputSource[int(TT2CvInput::Y)]),     int(TT2CvInputSource::None),  "Y -> None (scratch)");
        expectEqual(int(p.cvInputSource[int(TT2CvInput::Z)]),     int(TT2CvInputSource::None),  "Z -> None");
        expectEqual(int(p.cvInputSource[int(TT2CvInput::T)]),     int(TT2CvInputSource::None),  "T -> None");
    }

    CASE("op_in_scaled_read") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        runtime.variables.in = 5000;
        expectEqual(int(evalText("IN", runtime, output).value), 5000, "default scale is identity");
        evalText("IN.SCALE 0 100", runtime, output);
        expectEqual(runtime.variables.in_min, int16_t(0), "IN.SCALE min");
        expectEqual(runtime.variables.in_max, int16_t(100), "IN.SCALE max");
        runtime.variables.in = 16383;
        expectEqual(int(evalText("IN", runtime, output).value), 100, "full input -> scale max");
        runtime.variables.in = 0;
        expectEqual(int(evalText("IN", runtime, output).value), 0, "zero input -> scale min");
    }

    CASE("op_param_scaled_read") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("PARAM.SCALE 0 200", runtime, output);
        runtime.variables.param = 16383;
        expectEqual(int(evalText("PARAM", runtime, output).value), 200, "PARAM scales to max");
    }

    CASE("op_state_reads_latched_level") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        runtime.inputLevel[0] = 1;
        runtime.inputLevel[2] = 0;
        expectEqual(int(evalText("STATE 1", runtime, output).value), 1, "input 1 high");
        expectEqual(int(evalText("STATE 3", runtime, output).value), 0, "input 3 low");
        expectEqual(int(evalText("STATE 9", runtime, output).value), 0, "out of range -> 0");
    }

    CASE("op_mute_set_get") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("MUTE 1 1", runtime, output);
        expectEqual(int(evalText("MUTE 1", runtime, output).value), 1, "input 1 muted");
        expectEqual(int(evalText("MUTE 2", runtime, output).value), 0, "input 2 unmuted");
        evalText("MUTE 1 0", runtime, output);
        expectEqual(int(evalText("MUTE 1", runtime, output).value), 0, "input 1 unmuted");
    }
}
