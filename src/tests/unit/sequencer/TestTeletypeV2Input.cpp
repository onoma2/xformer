#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
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
        expectEqual(int(p.cvInputSource[int(TT2CvInput::X)]),     int(TT2CvInputSource::CvIn3), "X -> CvIn3");
        expectEqual(int(p.cvInputSource[int(TT2CvInput::Y)]),     int(TT2CvInputSource::CvIn4), "Y -> CvIn4");
        expectEqual(int(p.cvInputSource[int(TT2CvInput::Z)]),     int(TT2CvInputSource::None),  "Z -> None");
        expectEqual(int(p.cvInputSource[int(TT2CvInput::T)]),     int(TT2CvInputSource::None),  "T -> None");
    }
}
