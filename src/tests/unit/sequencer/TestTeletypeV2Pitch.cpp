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

TT2EvalResult evalText(const char *text, TT2Runtime &runtime, TT2OutputState &output,
                       const TeletypeProgram *program = nullptr) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output, program);
}

int16_t getv(const char *text, TT2Runtime &runtime, TT2OutputState &output,
             const TeletypeProgram *program = nullptr) {
    return evalText(text, runtime, output, program).value;
}

} // namespace

UNIT_TEST("TeletypeV2Pitch") {

    CASE("scale_linear") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("SCALE 0 100 0 1000 50", runtime, output)), 500, "midpoint");
        expectEqual(int(getv("SCALE 0 100 0 1000 0", runtime, output)), 0, "low end");
        expectEqual(int(getv("SCALE 0 100 0 1000 100", runtime, output)), 1000, "high end");
        expectEqual(int(getv("SCL 0 100 0 1000 50", runtime, output)), 500, "SCL alias");
    }

    CASE("scale0_implicit_zero_origin") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("SCALE0 100 1000 50", runtime, output)), 500, "0..100 -> 0..1000");
        expectEqual(int(getv("SCL0 100 1000 50", runtime, output)), 500, "SCL0 alias");
    }

    CASE("qt_quantize_to_multiple") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("QT 100 240", runtime, output)), 200, "rounds down");
        expectEqual(int(getv("QT 100 260", runtime, output)), 300, "rounds up");
        expectEqual(int(getv("QT 0 5", runtime, output)), 0, "step 0 -> 0");
    }

    CASE("vn_volts_to_note") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("VN 0", runtime, output)), 0, "0V -> note 0");
        expectEqual(int(getv("VN 1638", runtime, output)), 12, "1V -> note 12");
    }

    CASE("vv_hundredths_of_volt") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("VV 100", runtime, output)), 1638, "1.00V");
        expectEqual(int(getv("VV 50", runtime, output)), 819, "0.50V");
        expectEqual(int(getv("VV -100", runtime, output)), -1638, "-1.00V");
    }

    CASE("hz_lowest") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("HZ 0", runtime, output)), 1638, "0V -> table_hzv[36]");
    }

    CASE("p_scale_rescales_pattern_window") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        TeletypeProgram program; init(program);
        evalText("P 0 0", runtime, output, &program);
        evalText("P 1 50", runtime, output, &program);
        evalText("P 2 100", runtime, output, &program);
        evalText("P.SCALE 0 100 0 1000", runtime, output, &program);
        expectEqual(int(getv("P 0", runtime, output, &program)), 0, "0 -> 0");
        expectEqual(int(getv("P 1", runtime, output, &program)), 500, "50 -> 500");
        expectEqual(int(getv("P 2", runtime, output, &program)), 1000, "100 -> 1000");
    }
}
