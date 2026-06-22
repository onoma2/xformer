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

void evalP(const char *text, TT2Runtime &runtime, TT2OutputState &output,
           const TeletypeProgram *program) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    evaluateCommand(cmd, runtime, output, program);
}

int16_t getP(const char *text, TT2Runtime &runtime, TT2OutputState &output,
             const TeletypeProgram *program) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output, program).value;
}

} // namespace

UNIT_TEST("TeletypeV2Turtle") {

    CASE("xy_set_get") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalP("@X 2", runtime, output, nullptr);
        evalP("@Y 5", runtime, output, nullptr);
        expectEqual(int(getP("@X", runtime, output, nullptr)), 2, "x set");
        expectEqual(int(getP("@Y", runtime, output, nullptr)), 5, "y set");
    }

    CASE("value_read_write_pattern_memory") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        TeletypeProgram program; init(program);
        evalP("@X 1", runtime, output, &program);
        evalP("@Y 3", runtime, output, &program);
        evalP("@ 77", runtime, output, &program);
        expectEqual(int(getP("@", runtime, output, &program)), 77, "value at (1,3)");
        // x selects pattern 1, y selects index 3
        expectEqual(int(program.patterns[1].val[3]), 77, "wrote pattern 1 idx 3");
    }

    CASE("move_diagonal") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalP("@X 0", runtime, output, nullptr);
        evalP("@Y 0", runtime, output, nullptr);
        evalP("@MOVE 1 1", runtime, output, nullptr);
        expectEqual(int(getP("@X", runtime, output, nullptr)), 1, "moved +1 x");
        expectEqual(int(getP("@Y", runtime, output, nullptr)), 1, "moved +1 y");
    }

    CASE("fence_individual") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalP("@FX1 1", runtime, output, nullptr);
        evalP("@FX2 3", runtime, output, nullptr);
        evalP("@FY1 2", runtime, output, nullptr);
        evalP("@FY2 9", runtime, output, nullptr);
        expectEqual(int(getP("@FX1", runtime, output, nullptr)), 1, "fx1");
        expectEqual(int(getP("@FX2", runtime, output, nullptr)), 3, "fx2");
        expectEqual(int(getP("@FY1", runtime, output, nullptr)), 2, "fy1");
        expectEqual(int(getP("@FY2", runtime, output, nullptr)), 9, "fy2");
    }

    CASE("speed_and_dir") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalP("@SPEED 50", runtime, output, nullptr);
        expectEqual(int(getP("@SPEED", runtime, output, nullptr)), 50, "speed");
        evalP("@DIR 90", runtime, output, nullptr);
        expectEqual(int(getP("@DIR", runtime, output, nullptr)), 90, "dir");
        evalP("@DIR 450", runtime, output, nullptr);
        expectEqual(int(getP("@DIR", runtime, output, nullptr)), 90, "dir wraps mod 360");
        evalP("@DIR -10", runtime, output, nullptr);
        expectEqual(int(getP("@DIR", runtime, output, nullptr)), 350, "negative dir wraps");
    }

    CASE("mode_flags") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalP("@WRAP 1", runtime, output, nullptr);
        expectEqual(int(getP("@WRAP", runtime, output, nullptr)), 1, "wrap on");
        expectEqual(int(getP("@BUMP", runtime, output, nullptr)), 0, "bump off");
        evalP("@BOUNCE 1", runtime, output, nullptr);
        expectEqual(int(getP("@BOUNCE", runtime, output, nullptr)), 1, "bounce on");
        expectEqual(int(getP("@WRAP", runtime, output, nullptr)), 0, "wrap off after bounce");
    }

    CASE("step_moves_east_at_heading_90") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalP("@X 1", runtime, output, nullptr);
        evalP("@Y 1", runtime, output, nullptr);
        evalP("@DIR 90", runtime, output, nullptr);
        evalP("@SPEED 100", runtime, output, nullptr);
        evalP("@STEP", runtime, output, nullptr);
        expectEqual(int(getP("@X", runtime, output, nullptr)), 2, "stepped one cell east");
    }

    CASE("script_set_get") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalP("@SCRIPT 3", runtime, output, nullptr);
        expectEqual(int(getP("@SCRIPT", runtime, output, nullptr)), 3, "script number");
    }
}
