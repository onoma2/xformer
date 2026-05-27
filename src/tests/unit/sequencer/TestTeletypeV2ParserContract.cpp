#include "UnitTest.h"

// Pre-include model/Types.h in C++ mode so that when teletype.h pulls in
// state.h -> types.h inside extern "C", the C++ templates are already
// processed and skipped by #pragma once.
#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
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

void expectToken(const tele_command_t &cmd, uint8_t idx, tele_word_t tag,
                 int16_t value, const char *msg) {
    expectEqual(int(cmd_tag(&cmd, idx)), int(tag), msg);
    expectEqual(cmd_value(&cmd, idx), value, msg);
}

} // namespace

UNIT_TEST("TeletypeV2ParserContract") {

    CASE("numbers_decimal_and_negative") {
        auto r = tryParse("42");
        expectEqual(int(r.error), int(E_OK), "42 parses");
        expectEqual(int(r.cmd.length), 1, "42 length");
        expectToken(r.cmd, 0, NUMBER, 42, "42 value");

        r = tryParse("-7");
        expectEqual(int(r.error), int(E_OK), "-7 parses");
        expectEqual(int(r.cmd.length), 1, "-7 length");
        expectToken(r.cmd, 0, NUMBER, -7, "-7 value");
    }

    CASE("numbers_hex_binary_reversed") {
        auto r = tryParse("XFF");
        expectEqual(int(r.error), int(E_OK), "XFF parses");
        expectEqual(int(r.cmd.length), 1, "XFF length");
        expectToken(r.cmd, 0, XNUMBER, 255, "XFF = 255");

        r = tryParse("B1010");
        expectEqual(int(r.error), int(E_OK), "B1010 parses");
        expectEqual(int(r.cmd.length), 1, "B1010 length");
        expectToken(r.cmd, 0, BNUMBER, 10, "B1010 = 10");

        r = tryParse("R1100");
        expectEqual(int(r.error), int(E_OK), "R1100 parses");
        expectEqual(int(r.cmd.length), 1, "R1100 length");
        expectToken(r.cmd, 0, RNUMBER, 3, "R1100 = 3");
    }

    CASE("variables_and_core_ops") {
        auto r = tryParse("A");
        expectEqual(int(r.error), int(E_OK), "A parses");
        expectToken(r.cmd, 0, OP, E_OP_A, "A op");

        r = tryParse("X");
        expectEqual(int(r.error), int(E_OK), "X parses");
        expectToken(r.cmd, 0, OP, E_OP_X, "X op");

        r = tryParse("DRUNK");
        expectEqual(int(r.error), int(E_OK), "DRUNK parses");
        expectToken(r.cmd, 0, OP, E_OP_DRUNK, "DRUNK op");

        r = tryParse("TIME.ACT");
        expectEqual(int(r.error), int(E_OK), "TIME.ACT parses");
        expectToken(r.cmd, 0, OP, E_OP_TIME_ACT, "TIME.ACT op");

        r = tryParse("O.INC");
        expectEqual(int(r.error), int(E_OK), "O.INC parses");
        expectToken(r.cmd, 0, OP, E_OP_O_INC, "O.INC op");
    }

    CASE("math_symbols_and_aliases") {
        // Aliases must resolve to the same enum as their canonical name.
        auto r = tryParse("ADD");
        expectToken(r.cmd, 0, OP, E_OP_ADD, "ADD op");

        r = tryParse("+");
        expectEqual(int(r.error), int(E_OK), "+ parses");
        expectToken(r.cmd, 0, OP, E_OP_SYM_PLUS, "+ alias of ADD");

        r = tryParse("SUB");
        expectToken(r.cmd, 0, OP, E_OP_SUB, "SUB op");

        r = tryParse("-");
        expectEqual(int(r.error), int(E_OK), "- parses");
        expectToken(r.cmd, 0, OP, E_OP_SYM_DASH, "- alias of SUB");

        r = tryParse("MUL");
        expectToken(r.cmd, 0, OP, E_OP_MUL, "MUL op");

        r = tryParse("*");
        expectEqual(int(r.error), int(E_OK), "* parses");
        expectToken(r.cmd, 0, OP, E_OP_SYM_STAR, "* alias of MUL");

        r = tryParse("DIV");
        expectToken(r.cmd, 0, OP, E_OP_DIV, "DIV op");

        r = tryParse("/");
        expectEqual(int(r.error), int(E_OK), "/ parses");
        expectToken(r.cmd, 0, OP, E_OP_SYM_FORWARD_SLASH, "/ alias of DIV");

        r = tryParse("RAND");
        expectToken(r.cmd, 0, OP, E_OP_RAND, "RAND op");

        r = tryParse("RND");
        expectEqual(int(r.error), int(E_OK), "RND parses");
        expectToken(r.cmd, 0, OP, E_OP_RND, "RND alias of RAND");

        r = tryParse("RRAND");
        expectToken(r.cmd, 0, OP, E_OP_RRAND, "RRAND op");

        r = tryParse("RRND");
        expectEqual(int(r.error), int(E_OK), "RRND parses");
        expectToken(r.cmd, 0, OP, E_OP_RRND, "RRND alias of RRAND");

        r = tryParse("SCALE");
        expectToken(r.cmd, 0, OP, E_OP_SCALE, "SCALE op");

        r = tryParse("SCL");
        expectEqual(int(r.error), int(E_OK), "SCL parses");
        expectToken(r.cmd, 0, OP, E_OP_SCL, "SCL alias of SCALE");

        r = tryParse("?");
        expectEqual(int(r.error), int(E_OK), "? parses");
        expectToken(r.cmd, 0, OP, E_OP_TIF, "? op");
    }

    CASE("mods") {
        auto r = tryParse("IF");
        expectEqual(int(r.error), int(E_OK), "IF parses");
        expectToken(r.cmd, 0, MOD, E_MOD_IF, "IF mod");

        r = tryParse("ELSE");
        expectEqual(int(r.error), int(E_OK), "ELSE parses");
        expectToken(r.cmd, 0, MOD, E_MOD_ELSE, "ELSE mod");

        r = tryParse("L");
        expectEqual(int(r.error), int(E_OK), "L parses");
        expectToken(r.cmd, 0, MOD, E_MOD_L, "L mod");

        r = tryParse("W");
        expectEqual(int(r.error), int(E_OK), "W parses");
        expectToken(r.cmd, 0, MOD, E_MOD_W, "W mod");

        r = tryParse("EVERY");
        expectEqual(int(r.error), int(E_OK), "EVERY parses");
        expectToken(r.cmd, 0, MOD, E_MOD_EVERY, "EVERY mod");

        r = tryParse("PROB");
        expectEqual(int(r.error), int(E_OK), "PROB parses");
        expectToken(r.cmd, 0, MOD, E_MOD_PROB, "PROB mod");

        r = tryParse("DEL");
        expectEqual(int(r.error), int(E_OK), "DEL parses");
        expectToken(r.cmd, 0, MOD, E_MOD_DEL, "DEL mod");
    }

    CASE("cv_family") {
        auto r = tryParse("CV 1 5000");
        expectEqual(int(r.error), int(E_OK), "CV 1 5000 parses");
        expectEqual(int(r.cmd.length), 3, "CV length");
        expectToken(r.cmd, 0, OP, E_OP_CV, "CV op");
        expectToken(r.cmd, 1, NUMBER, 1, "CV arg 1");
        expectToken(r.cmd, 2, NUMBER, 5000, "CV arg 5000");

        r = tryParse("CV.SLEW 2 100");
        expectEqual(int(r.error), int(E_OK), "CV.SLEW parses");
        expectEqual(int(r.cmd.length), 3, "CV.SLEW length");
        expectToken(r.cmd, 0, OP, E_OP_CV_SLEW, "CV.SLEW op");

        r = tryParse("CV.OFF 3 1000");
        expectEqual(int(r.error), int(E_OK), "CV.OFF parses");
        expectToken(r.cmd, 0, OP, E_OP_CV_OFF, "CV.OFF op");

        r = tryParse("CV.GET 1");
        expectEqual(int(r.error), int(E_OK), "CV.GET parses");
        expectEqual(int(r.cmd.length), 2, "CV.GET length");
        expectToken(r.cmd, 0, OP, E_OP_CV_GET, "CV.GET op");

        r = tryParse("CV.SET 2 3000");
        expectEqual(int(r.error), int(E_OK), "CV.SET parses");
        expectEqual(int(r.cmd.length), 3, "CV.SET length");
        expectToken(r.cmd, 0, OP, E_OP_CV_SET, "CV.SET op");
    }

    CASE("tr_family") {
        auto r = tryParse("TR 1 1");
        expectEqual(int(r.error), int(E_OK), "TR 1 1 parses");
        expectEqual(int(r.cmd.length), 3, "TR length");
        expectToken(r.cmd, 0, OP, E_OP_TR, "TR op");
        expectToken(r.cmd, 1, NUMBER, 1, "TR arg 1");
        expectToken(r.cmd, 2, NUMBER, 1, "TR arg 1");

        r = tryParse("TR.PULSE 1");
        expectEqual(int(r.error), int(E_OK), "TR.PULSE parses");
        expectEqual(int(r.cmd.length), 2, "TR.PULSE length");
        expectToken(r.cmd, 0, OP, E_OP_TR_PULSE, "TR.PULSE op");

        r = tryParse("TR.TOG 2");
        expectEqual(int(r.error), int(E_OK), "TR.TOG parses");
        expectToken(r.cmd, 0, OP, E_OP_TR_TOG, "TR.TOG op");

        r = tryParse("TR.TIME 1 50");
        expectEqual(int(r.error), int(E_OK), "TR.TIME parses");
        expectToken(r.cmd, 0, OP, E_OP_TR_TIME, "TR.TIME op");

        r = tryParse("TR.P 1");
        expectEqual(int(r.error), int(E_OK), "TR.P parses");
        expectToken(r.cmd, 0, OP, E_OP_TR_P, "TR.P op");

        r = tryParse("TR.POL 1 1");
        expectEqual(int(r.error), int(E_OK), "TR.POL parses");
        expectToken(r.cmd, 0, OP, E_OP_TR_POL, "TR.POL op");
    }

    CASE("metro_family") {
        auto r = tryParse("M 750");
        expectEqual(int(r.error), int(E_OK), "M 750 parses");
        expectEqual(int(r.cmd.length), 2, "M length");
        expectToken(r.cmd, 0, OP, E_OP_M, "M op");
        expectToken(r.cmd, 1, NUMBER, 750, "M arg");

        r = tryParse("M!");
        expectEqual(int(r.error), int(E_OK), "M! parses");
        expectEqual(int(r.cmd.length), 1, "M! length");
        expectToken(r.cmd, 0, OP, E_OP_M_SYM_EXCLAMATION, "M! op");

        r = tryParse("M.ACT 1");
        expectEqual(int(r.error), int(E_OK), "M.ACT parses");
        expectToken(r.cmd, 0, OP, E_OP_M_ACT, "M.ACT op");

        r = tryParse("M.RESET");
        expectEqual(int(r.error), int(E_OK), "M.RESET parses");
        expectToken(r.cmd, 0, OP, E_OP_M_RESET, "M.RESET op");
    }

    CASE("wact_alias") {
        // W.ACT and WR.ACT must both lower to the same op enum.
        auto r = tryParse("W.ACT 1");
        expectEqual(int(r.error), int(E_OK), "W.ACT parses");
        expectEqual(int(r.cmd.length), 2, "W.ACT length");
        expectToken(r.cmd, 0, OP, E_OP_WR_ACT, "W.ACT -> WR_ACT");

        r = tryParse("WR.ACT 1");
        expectEqual(int(r.error), int(E_OK), "WR.ACT parses");
        expectEqual(int(r.cmd.length), 2, "WR.ACT length");
        expectToken(r.cmd, 0, OP, E_OP_WR_ACT, "WR.ACT -> WR_ACT");
    }

    CASE("separator_pre") {
        auto r = tryParse("IF 1: X 1");
        expectEqual(int(r.error), int(E_OK), "IF with pre-sep parses");
        expectEqual(int(r.cmd.length), 5, "pre-sep length");
        expectEqual(int(r.cmd.separator), 2, "pre-sep at index 2");
        expectToken(r.cmd, 0, MOD, E_MOD_IF, "IF mod");
        expectToken(r.cmd, 1, NUMBER, 1, "IF condition");
        expectToken(r.cmd, 2, PRE_SEP, 0, ": separator");
        expectToken(r.cmd, 3, OP, E_OP_X, "X op");
        expectToken(r.cmd, 4, NUMBER, 1, "X arg");
    }

    CASE("separator_sub") {
        auto r = tryParse("X 1; Y 2");
        expectEqual(int(r.error), int(E_OK), "sub-sep parses");
        expectEqual(int(r.cmd.length), 5, "sub-sep length");
        // SUB_SEP does not set cmd.separator; only PRE_SEP does.
        expectEqual(int(r.cmd.separator), -1, "sub-sep leaves separator -1");
        expectToken(r.cmd, 0, OP, E_OP_X, "X");
        expectToken(r.cmd, 1, NUMBER, 1, "1");
        expectToken(r.cmd, 2, SUB_SEP, 0, "; separator");
        expectToken(r.cmd, 3, OP, E_OP_Y, "Y");
        expectToken(r.cmd, 4, NUMBER, 2, "2");
    }

    CASE("separator_combined") {
        auto r = tryParse("IF 1: X 1; Y 2");
        expectEqual(int(r.error), int(E_OK), "combined sep parses");
        expectEqual(int(r.cmd.length), 8, "combined length");
        expectEqual(int(r.cmd.separator), 2, "pre-sep at index 2");
        expectToken(r.cmd, 0, MOD, E_MOD_IF, "IF");
        expectToken(r.cmd, 1, NUMBER, 1, "1");
        expectToken(r.cmd, 2, PRE_SEP, 0, ":");
        expectToken(r.cmd, 3, OP, E_OP_X, "X");
        expectToken(r.cmd, 4, NUMBER, 1, "1");
        expectToken(r.cmd, 5, SUB_SEP, 0, ";");
        expectToken(r.cmd, 6, OP, E_OP_Y, "Y");
        expectToken(r.cmd, 7, NUMBER, 2, "2");
    }

    CASE("unsupported_hardware_rejected") {
        // Monome hardware families are not in the current parser and must be rejected.
        auto r = tryParse("ES.CLOCK 1");
        expectEqual(int(r.error), int(E_PARSE), "ES.CLOCK rejected");

        r = tryParse("WW.MUTE1 0");
        expectEqual(int(r.error), int(E_PARSE), "WW.MUTE1 rejected");

        r = tryParse("MP.RESET 1");
        expectEqual(int(r.error), int(E_PARSE), "MP.RESET rejected");

        r = tryParse("TO.CV 1 5000");
        expectEqual(int(r.error), int(E_PARSE), "TO.CV rejected");

        r = tryParse("TI.PRM 1");
        expectEqual(int(r.error), int(E_PARSE), "TI.PRM rejected");

        r = tryParse("GRID.INIT");
        expectEqual(int(r.error), int(E_PARSE), "GRID.INIT rejected");

        r = tryParse("CROW.OUT 1 5000");
        expectEqual(int(r.error), int(E_PARSE), "CROW.OUT rejected");

        r = tryParse("SC.CV 1 5000");
        expectEqual(int(r.error), int(E_PARSE), "SC.CV rejected");
    }

    CASE("unknown_token_rejected") {
        auto r = tryParse("UNKNOWN_TOKEN");
        expectEqual(int(r.error), int(E_PARSE), "unknown token rejected");

        r = tryParse("FOO.BAR 1 2");
        expectEqual(int(r.error), int(E_PARSE), "foo.bar rejected");
    }

    CASE("performer_added_keep_tokens") {
        // W* transport / time ops
        auto r = tryParse("WBPM 120");
        expectEqual(int(r.error), int(E_OK), "WBPM parses");
        expectToken(r.cmd, 0, OP, E_OP_WBPM, "WBPM op");

        r = tryParse("WMS");
        expectEqual(int(r.error), int(E_OK), "WMS parses");
        expectToken(r.cmd, 0, OP, E_OP_WMS, "WMS op");

        r = tryParse("BAR");
        expectEqual(int(r.error), int(E_OK), "BAR parses");
        expectToken(r.cmd, 0, OP, E_OP_BAR, "BAR op");

        r = tryParse("WR");
        expectEqual(int(r.error), int(E_OK), "WR parses");
        expectToken(r.cmd, 0, OP, E_OP_WR, "WR op");

        r = tryParse("RT");
        expectEqual(int(r.error), int(E_OK), "RT parses");
        expectToken(r.cmd, 0, OP, E_OP_RT, "RT op");

        // Routing / readback
        r = tryParse("BUS 1");
        expectEqual(int(r.error), int(E_OK), "BUS parses");
        expectToken(r.cmd, 0, OP, E_OP_BUS, "BUS op");

        r = tryParse("PRM");
        expectEqual(int(r.error), int(E_OK), "PRM parses");
        expectToken(r.cmd, 0, OP, E_OP_PRM, "PRM op");

        r = tryParse("MUTE 1");
        expectEqual(int(r.error), int(E_OK), "MUTE parses");
        expectToken(r.cmd, 0, OP, E_OP_MUTE, "MUTE op");

        r = tryParse("STATE 1");
        expectEqual(int(r.error), int(E_OK), "STATE parses");
        expectToken(r.cmd, 0, OP, E_OP_STATE, "STATE op");

        // Native modules (smoke — one per family)
        r = tryParse("E.A 1 500");
        expectEqual(int(r.error), int(E_OK), "E.A parses");
        expectToken(r.cmd, 0, OP, E_OP_E_A, "E.A op");

        r = tryParse("LFO.R 1 1000");
        expectEqual(int(r.error), int(E_OK), "LFO.R parses");
        expectToken(r.cmd, 0, OP, E_OP_LFO_R, "LFO.R op");

        r = tryParse("G.TIME 1");
        expectEqual(int(r.error), int(E_OK), "G.TIME parses");
        expectToken(r.cmd, 0, OP, E_OP_G_TIME, "G.TIME op");
    }

    CASE("midi_family_smoke") {
        auto r = tryParse("MI.N 1");
        expectEqual(int(r.error), int(E_OK), "MI.N parses");
        expectToken(r.cmd, 0, OP, E_OP_MI_N, "MI.N op");

        r = tryParse("MI.V 1");
        expectEqual(int(r.error), int(E_OK), "MI.V parses");
        expectToken(r.cmd, 0, OP, E_OP_MI_V, "MI.V op");

        r = tryParse("MI.CC 1 1");
        expectEqual(int(r.error), int(E_OK), "MI.CC parses");
        expectToken(r.cmd, 0, OP, E_OP_MI_CC, "MI.CC op");
    }

    CASE("pattern_ops_smoke") {
        auto r = tryParse("P.NEXT");
        expectEqual(int(r.error), int(E_OK), "P.NEXT parses");
        expectToken(r.cmd, 0, OP, E_OP_P_NEXT, "P.NEXT op");

        r = tryParse("PN 0 0");
        expectEqual(int(r.error), int(E_OK), "PN parses");
        expectToken(r.cmd, 0, OP, E_OP_PN, "PN op");

        r = tryParse("P.INS 0 1");
        expectEqual(int(r.error), int(E_OK), "P.INS parses");
        expectToken(r.cmd, 0, OP, E_OP_P_INS, "P.INS op");
    }

    CASE("turtle_ops_smoke") {
        auto r = tryParse("@");
        expectEqual(int(r.error), int(E_OK), "@ parses");
        expectToken(r.cmd, 0, OP, E_OP_TURTLE, "@ op");

        r = tryParse("@X");
        expectEqual(int(r.error), int(E_OK), "@X parses");
        expectToken(r.cmd, 0, OP, E_OP_TURTLE_X, "@X op");

        r = tryParse("@MOVE 1 2");
        expectEqual(int(r.error), int(E_OK), "@MOVE parses");
        expectToken(r.cmd, 0, OP, E_OP_TURTLE_MOVE, "@MOVE op");
    }

    CASE("v2_drop_tokens_currently_parse") {
        // These tokens are in the current parser but are scheduled for v2 drop.
        // The contract documents that they parse today; v2 lowering will reject them.
        auto r = tryParse("INIT.SCENE");
        expectEqual(int(r.error), int(E_OK), "INIT.SCENE parses (drop noted)");
        expectToken(r.cmd, 0, OP, E_OP_INIT_SCENE, "INIT.SCENE op");

        r = tryParse("SCENE 1");
        expectEqual(int(r.error), int(E_OK), "SCENE parses (drop noted)");
        expectToken(r.cmd, 0, OP, E_OP_SCENE, "SCENE op");

        r = tryParse("SCENE.G 1");
        expectEqual(int(r.error), int(E_OK), "SCENE.G parses (drop noted)");
        expectToken(r.cmd, 0, OP, E_OP_SCENE_G, "SCENE.G op");

        r = tryParse("LIVE.GRID");
        expectEqual(int(r.error), int(E_OK), "LIVE.GRID parses (drop noted)");
        expectToken(r.cmd, 0, OP, E_OP_LIVE_GRID, "LIVE.GRID op");

        r = tryParse("LIVE.G");
        expectEqual(int(r.error), int(E_OK), "LIVE.G parses (drop noted)");
        expectToken(r.cmd, 0, OP, E_OP_LIVE_G, "LIVE.G op");
    }

    CASE("init_family_smoke") {
        auto r = tryParse("INIT");
        expectEqual(int(r.error), int(E_OK), "INIT parses");
        expectToken(r.cmd, 0, OP, E_OP_INIT, "INIT op");

        r = tryParse("INIT.SCRIPT");
        expectEqual(int(r.error), int(E_OK), "INIT.SCRIPT parses");
        expectToken(r.cmd, 0, OP, E_OP_INIT_SCRIPT, "INIT.SCRIPT op");

        r = tryParse("INIT.CV.ALL");
        expectEqual(int(r.error), int(E_OK), "INIT.CV.ALL parses");
        expectToken(r.cmd, 0, OP, E_OP_INIT_CV_ALL, "INIT.CV.ALL op");
    }

    CASE("control_flow_smoke") {
        auto r = tryParse("SCRIPT 1");
        expectEqual(int(r.error), int(E_OK), "SCRIPT parses");
        expectToken(r.cmd, 0, OP, E_OP_SCRIPT, "SCRIPT op");

        r = tryParse("$ 1");
        expectEqual(int(r.error), int(E_OK), "$ parses");
        expectToken(r.cmd, 0, OP, E_OP_SYM_DOLLAR, "$ op");

        r = tryParse("KILL");
        expectEqual(int(r.error), int(E_OK), "KILL parses");
        expectToken(r.cmd, 0, OP, E_OP_KILL, "KILL op");

        r = tryParse("BREAK");
        expectEqual(int(r.error), int(E_OK), "BREAK parses");
        expectToken(r.cmd, 0, OP, E_OP_BREAK, "BREAK op");

        r = tryParse("SYNC");
        expectEqual(int(r.error), int(E_OK), "SYNC parses");
        expectToken(r.cmd, 0, OP, E_OP_SYNC, "SYNC op");
    }

    CASE("queue_and_stack_smoke") {
        auto r = tryParse("Q.AVG");
        expectEqual(int(r.error), int(E_OK), "Q.AVG parses");
        expectToken(r.cmd, 0, OP, E_OP_Q_AVG, "Q.AVG op");

        r = tryParse("S.ALL");
        expectEqual(int(r.error), int(E_OK), "S.ALL parses");
        expectToken(r.cmd, 0, OP, E_OP_S_ALL, "S.ALL op");

        r = tryParse("S.POP");
        expectEqual(int(r.error), int(E_OK), "S.POP parses");
        expectToken(r.cmd, 0, OP, E_OP_S_POP, "S.POP op");
    }

    CASE("seed_family_smoke") {
        auto r = tryParse("SEED 1");
        expectEqual(int(r.error), int(E_OK), "SEED parses");
        expectToken(r.cmd, 0, OP, E_OP_SEED, "SEED op");

        r = tryParse("RAND.SEED 1");
        expectEqual(int(r.error), int(E_OK), "RAND.SEED parses");
        expectToken(r.cmd, 0, OP, E_OP_RAND_SEED, "RAND.SEED op");

        r = tryParse("RAND.SD 1");
        expectEqual(int(r.error), int(E_OK), "RAND.SD parses");
        expectToken(r.cmd, 0, OP, E_OP_SYM_RAND_SD, "RAND.SD op");
    }
}
