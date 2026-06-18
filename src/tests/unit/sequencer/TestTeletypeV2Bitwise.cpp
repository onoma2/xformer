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

int16_t getv(const char *text, TT2Runtime &runtime, TT2OutputState &output) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output).value;
}

} // namespace

UNIT_TEST("TeletypeV2Bitwise") {

    CASE("bit_logic") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("| 12 10", runtime, output)), 14, "OR");
        expectEqual(int(getv("& 12 10", runtime, output)), 8, "AND");
        expectEqual(int(getv("^ 12 10", runtime, output)), 6, "XOR (^)");
        expectEqual(int(getv("~ 0", runtime, output)), -1, "NOT");
    }

    CASE("xor_is_not_equal") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("XOR 5 5", runtime, output)), 0, "equal -> 0");
        expectEqual(int(getv("XOR 5 3", runtime, output)), 1, "differ -> 1");
    }

    CASE("bit_set_get_clear_tog") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        // BSET x i — value first, bit index second.
        expectEqual(int(getv("BSET 8 0", runtime, output)), 9, "set bit 0 of 8");
        expectEqual(int(getv("BGET 9 0", runtime, output)), 1, "get bit 0 set");
        expectEqual(int(getv("BGET 9 1", runtime, output)), 0, "get bit 1 clear");
        expectEqual(int(getv("BCLR 9 0", runtime, output)), 8, "clear bit 0");
        expectEqual(int(getv("BTOG 9 0", runtime, output)), 8, "toggle set->clear");
        expectEqual(int(getv("BTOG 9 1", runtime, output)), 11, "toggle clear->set");
    }

    CASE("bit_reverse") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("BREV 1", runtime, output)), -32768, "bit 0 -> bit 15");
    }

    CASE("shifts") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        // RSH x n — value first, shift second.
        expectEqual(int(getv("RSH 8 1", runtime, output)), 4, "RSH by 1");
        expectEqual(int(getv("LSH 8 1", runtime, output)), 16, "LSH by 1");
        expectEqual(int(getv("RSH 8 -1", runtime, output)), 16, "RSH by -1 = LSH");
    }

    CASE("rotates") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        expectEqual(int(getv("LROT 1 1", runtime, output)), 2, "rotate left");
        expectEqual(int(getv("RROT 1 1", runtime, output)), -32768, "rotate right wraps to MSB");
    }
}
