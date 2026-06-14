#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

// Parity harness: evaluate a curated, deterministic set of command lines (the
// op families used across the teletype-codex scenes) through BOTH the legacy
// C VM (TT1: process_command on a scene_state_t) and the native TT2 runner,
// asserting the pushed result matches. RNG/stateful ops (RAND/DRUNK/O/FLIP/...)
// are excluded — their two implementations cannot agree bit-for-bit.

namespace {

int16_t tt1Eval(const char *text, scene_state_t *ss, exec_state_t *es) {
    tele_command_t cmd = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &cmd, errMsg);
    process_result_t r = process_command(ss, es, &cmd);
    return r.has_value ? r.value : 0;
}

int16_t tt2Eval(const char *text, TT2Runtime &runtime, TT2OutputState &output) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output).value;
}

} // namespace

UNIT_TEST("TeletypeV2Parity") {

    static const char *lines[] = {
        // arithmetic
        "ADD 100 23", "SUB 50 8", "MUL 6 7", "DIV 100 7", "MOD 100 7",
        "MIN 3 9", "MAX 3 9", "ABS -5", "SGN -3", "SGN 7",
        // nested expressions
        "ADD MUL 2 3 4", "MUL ADD 1 2 3", "SUB MUL 5 5 ADD 1 1",
        // comparison
        "EQ 5 5", "EQ 5 4", "NE 5 3", "LT 3 5", "GT 3 5",
        "LTE 5 5", "GTE 4 5", "GTE 5 5",
        // logic
        "AND 1 0", "OR 1 0", "AND 1 1", "OR 0 0", "NZ 5", "NZ 0", "EZ 0", "EZ 9",
        // range
        "LIM 50 0 10", "WRAP 12 0 10", "WRAP 8 0 10", "AVG 10 20",
        "INR 0 5 10", "OUTR 0 5 10",
        // pitch
        "N 0", "N 12", "N 24", "N -12", "V 1", "V 5", "V -2",
        "VN 1638", "VN 0", "VV 100", "VV 250", "VV -100",
        "HZ 0", "HZ 1638",
        // QT is intentionally excluded: the legacy C VM returns 0 whenever QT
        // should round up to (c+1)*step (e.g. QT 100 250 / 251), a TT1 bug. The
        // native QT is correct and covered by TestTeletypeV2Pitch.
        // scale
        "SCALE 0 100 0 1000 50", "SCALE 0 100 0 1000 0", "SCALE0 100 1000 50",
        // bitwise / shift
        "| 12 10", "& 12 10", "^ 12 10", "~ 5", "~ 0",
        "BSET 0 8", "BGET 0 9", "BGET 1 9", "BCLR 0 9", "BTOG 0 9",
        "BREV 1", "RSH 8 1", "LSH 8 1", "RSH 8 -1", "LROT 1 1", "RROT 1 1",
        // P1 ported ops (deterministic)
        "EXP 0", "EXP 8000", "EXP 16383", "EXP -8000",
        "JI 3 2", "JI 5 4", "JI 1 1", "JI 7 4", "JI 9 8",
        ">< 0 5 10", "<> 0 5 10", ">=< 0 5 10", "<=> 0 5 10", "! 0", "! 5",
        "<<< 1 1", ">>> 1 1", "&&& 1 1 0", "||| 0 0 1", "&&&& 1 1 1 0", "|||| 0 0 0 1",
        // P2 euclid / drum (deterministic; reuse linked C helpers)
        "ER 4 16 0", "ER 4 16 1", "ER 3 8 2", "ER 5 13 7",
        "NR 0 0 1 0", "NR 5 1 3 7",
        "DR.T 0 0 0 16 0", "DR.P 1 0 0", "DR.P 1 0 3", "DR.V 0 0",
        // P2b scale-bitmask (deterministic)
        "N.S 0 0 1", "N.S 0 0 3", "N.S 12 1 5", "N.S -12 0 2",
        "N.C 0 0 1", "N.C 0 1 2", "N.C 24 2 3",
        "N.CS 0 0 1 1", "N.CS 12 1 3 2",
        "QT.S 1638 0 0", "QT.S 2000 0 1", "QT.CS 1638 0 0 1 3",
        // NOTE: N.B/N.BX/QT.B/QT.BX (scale-bank get-degree path) implemented but
        // not yet bridge-parity-verified — degree-walk semantics need a follow-up.
    };

    CASE("deterministic_op_parity") {
        static scene_state_t ss;
        ss_init(&ss);
        exec_state_t es;
        es_init(&es);

        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);

        for (auto *line : lines) {
            int16_t a = tt1Eval(line, &ss, &es);
            int16_t b = tt2Eval(line, runtime, output);
            if (a != b) print("MISMATCH '%s' tt1=%d tt2=%d\n", line, int(a), int(b));
            expectEqual(int(b), int(a), line);
        }
    }
}
