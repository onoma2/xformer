#include "UnitTest.h"

#include "model/TeletypeProgram.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

namespace {

struct ParseResult {
    tele_error_t error;
    char errorMsg[TELE_ERROR_MSG_LENGTH];
    tele_command_t cmd;
};

ParseResult tryParse(const char *text) {
    ParseResult result;
    result.cmd = {};
    result.errorMsg[0] = '\0';
    result.error = parse(text, &result.cmd, result.errorMsg);
    return result;
}

void expectToken(const TT2Command &cmd, uint8_t idx, uint8_t tag,
                 int16_t value, const char *msg) {
    expectEqual(int(cmd.tag[idx]), int(tag), msg);
    expectEqual(cmd.value[idx], value, msg);
}

} // namespace

UNIT_TEST("TeletypeV2Lowering") {

    CASE("plain_command_cv") {
        auto r = tryParse("CV 1 5000");
        expectEqual(int(r.error), int(E_OK), "parse CV 1 5000");

        TT2Command lowered = {};
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectEqual(int(lowered.length), 3, "lowered length");
        expectToken(lowered, 0, uint8_t(OP), E_OP_CV, "CV op");
        expectToken(lowered, 1, uint8_t(NUMBER), 1, "CV arg 1");
        expectToken(lowered, 2, uint8_t(NUMBER), 5000, "CV arg 5000");
    }

    CASE("symbol_command") {
        auto r = tryParse("A + 1 2");
        expectEqual(int(r.error), int(E_OK), "parse A + 1 2");

        TT2Command lowered = {};
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectEqual(int(lowered.length), 4, "lowered length");
        expectToken(lowered, 0, uint8_t(OP), E_OP_A, "A");
        expectToken(lowered, 1, uint8_t(OP), E_OP_SYM_PLUS, "+");
        expectToken(lowered, 2, uint8_t(NUMBER), 1, "1");
        expectToken(lowered, 3, uint8_t(NUMBER), 2, "2");
    }

    CASE("pre_separator_command") {
        auto r = tryParse("IF 1: X 1");
        expectEqual(int(r.error), int(E_OK), "parse IF 1: X 1");

        TT2Command lowered = {};
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectEqual(int(lowered.length), 5, "lowered length");
        expectToken(lowered, 0, uint8_t(MOD), E_MOD_IF, "IF");
        expectToken(lowered, 1, uint8_t(NUMBER), 1, "1");
        expectToken(lowered, 2, uint8_t(PRE_SEP), 0, "PRE_SEP");
        expectToken(lowered, 3, uint8_t(OP), E_OP_X, "X");
        expectToken(lowered, 4, uint8_t(NUMBER), 1, "1");
    }

    CASE("sub_separator_command") {
        auto r = tryParse("X 1; Y 2");
        expectEqual(int(r.error), int(E_OK), "parse X 1; Y 2");

        TT2Command lowered = {};
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectEqual(int(lowered.length), 5, "lowered length");
        expectToken(lowered, 0, uint8_t(OP), E_OP_X, "X");
        expectToken(lowered, 1, uint8_t(NUMBER), 1, "1");
        expectToken(lowered, 2, uint8_t(SUB_SEP), 0, "SUB_SEP");
        expectToken(lowered, 3, uint8_t(OP), E_OP_Y, "Y");
        expectToken(lowered, 4, uint8_t(NUMBER), 2, "2");
    }

    CASE("combined_pre_and_sub_separator") {
        auto r = tryParse("IF 1: X 1; Y 2");
        expectEqual(int(r.error), int(E_OK), "parse IF 1: X 1; Y 2");

        TT2Command lowered = {};
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectEqual(int(lowered.length), 8, "lowered length");
        expectToken(lowered, 0, uint8_t(MOD), E_MOD_IF, "IF");
        expectToken(lowered, 1, uint8_t(NUMBER), 1, "1");
        expectToken(lowered, 2, uint8_t(PRE_SEP), 0, "PRE_SEP");
        expectToken(lowered, 3, uint8_t(OP), E_OP_X, "X");
        expectToken(lowered, 4, uint8_t(NUMBER), 1, "1");
        expectToken(lowered, 5, uint8_t(SUB_SEP), 0, "SUB_SEP");
        expectToken(lowered, 6, uint8_t(OP), E_OP_Y, "Y");
        expectToken(lowered, 7, uint8_t(NUMBER), 2, "2");
    }

    CASE("wact_alias_preserves_op_value") {
        auto r1 = tryParse("W.ACT 1");
        auto r2 = tryParse("WR.ACT 1");
        expectEqual(int(r1.error), int(E_OK), "parse W.ACT");
        expectEqual(int(r2.error), int(E_OK), "parse WR.ACT");

        TT2Command lowered1 = {};
        TT2Command lowered2 = {};
        expectTrue(lowerCommand(r1.cmd, lowered1), "lower W.ACT");
        expectTrue(lowerCommand(r2.cmd, lowered2), "lower WR.ACT");

        expectEqual(int(lowered1.length), 2, "W.ACT length");
        expectEqual(int(lowered2.length), 2, "WR.ACT length");
        expectToken(lowered1, 0, uint8_t(OP), E_OP_WR_ACT, "W.ACT op");
        expectToken(lowered2, 0, uint8_t(OP), E_OP_WR_ACT, "WR.ACT op");
        expectEqual(lowered1.value[0], lowered2.value[0],
                    "W.ACT and WR.ACT produce same op value");
    }

    CASE("hardware_rejected_at_parse_layer_not_lowering") {
        auto r = tryParse("ES.CLOCK 1");
        expectEqual(int(r.error), int(E_PARSE), "ES.CLOCK rejected at parse");

        // Lowering is never reached for parse failures.
        // The contract is: parse failure stays at the parse layer.
    }

    CASE("trailing_slots_zeroed") {
        auto r = tryParse("CV 1 5000");
        expectEqual(int(r.error), int(E_OK), "parse CV 1 5000");

        TT2Command lowered = {};
        // Pre-pollute trailing slots.
        for (int i = 0; i < TT2_COMMAND_MAX_LENGTH; ++i) {
            lowered.tag[i] = 0xFF;
            lowered.value[i] = 0x7FFF;
        }

        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        // Active slots
        expectToken(lowered, 0, uint8_t(OP), E_OP_CV, "token 0");
        expectToken(lowered, 1, uint8_t(NUMBER), 1, "token 1");
        expectToken(lowered, 2, uint8_t(NUMBER), 5000, "token 2");
        // Trailing slots zeroed
        for (int i = 3; i < TT2_COMMAND_MAX_LENGTH; ++i) {
            expectEqual(int(lowered.tag[i]), 0, "trailing tag zeroed");
            expectEqual(lowered.value[i], int16_t(0), "trailing value zeroed");
        }
    }

    CASE("length_boundary_reject") {
        // tele_command_t.length > TT2_COMMAND_MAX_LENGTH should be rejected.
        // parse() already enforces COMMAND_MAX_LENGTH == 16, so this is a
        // defensive-only path. Fake an oversized command to exercise it.
        tele_command_t oversized = {};
        oversized.length = TT2_COMMAND_MAX_LENGTH + 1;

        TT2Command lowered = {};
        expectFalse(lowerCommand(oversized, lowered),
                    "oversized command rejected");
    }

    CASE("mods_and_numbers_preserved") {
        auto r = tryParse("PROB 50: TR.PULSE 1");
        expectEqual(int(r.error), int(E_OK), "parse PROB command");

        TT2Command lowered = {};
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectEqual(int(lowered.length), 5, "lowered length");
        expectToken(lowered, 0, uint8_t(MOD), E_MOD_PROB, "PROB mod");
        expectToken(lowered, 1, uint8_t(NUMBER), 50, "50");
        expectToken(lowered, 2, uint8_t(PRE_SEP), 0, "PRE_SEP");
        expectToken(lowered, 3, uint8_t(OP), E_OP_TR_PULSE, "TR.PULSE");
        expectToken(lowered, 4, uint8_t(NUMBER), 1, "1");
    }

    CASE("hex_binary_reversed_preserved") {
        auto r = tryParse("CV 1 XFF");
        expectEqual(int(r.error), int(E_OK), "parse XFF");

        TT2Command lowered = {};
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectToken(lowered, 1, uint8_t(NUMBER), 1, "arg 1");
        expectToken(lowered, 2, uint8_t(XNUMBER), 255, "XFF = 255");

        r = tryParse("CV 1 B1010");
        expectEqual(int(r.error), int(E_OK), "parse B1010");
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectToken(lowered, 2, uint8_t(BNUMBER), 10, "B1010 = 10");

        r = tryParse("CV 1 R1100");
        expectEqual(int(r.error), int(E_OK), "parse R1100");
        expectTrue(lowerCommand(r.cmd, lowered), "lowering succeeds");
        expectToken(lowered, 2, uint8_t(RNUMBER), 3, "R1100 = 3");
    }
}
