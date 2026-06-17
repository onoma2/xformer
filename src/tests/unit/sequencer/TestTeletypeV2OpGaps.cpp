#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

// "Quick op gaps": alias gaps (<<, >>, PRM) wired to existing handlers, plus
// the queue ops Q.2P / Q.P2 (queue <-> pattern) and Q.RND (random element).

namespace {

TT2EvalResult evalP(const char *text, TT2Runtime &rt, TT2OutputState &out,
                    const TeletypeProgram *prog) {
    tele_command_t src = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, err);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, rt, out, prog);
}

TT2EvalResult evalText(const char *text, TT2Runtime &rt, TT2OutputState &out) {
    return evalP(text, rt, out, nullptr);
}

} // namespace

UNIT_TEST("TeletypeV2OpGaps") {

CASE("<< and >> alias LSH/RSH") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    expectEqual(int(evalText("<< 5 1", rt, out).value), 10, "5 << 1 = 10");
    expectEqual(int(evalText(">> 8 1", rt, out).value), 4, "8 >> 1 = 4");
}

CASE("PRM aliases PARAM (same handler)") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    rt.variables.param = 1234;
    expectEqual(int(evalText("PRM", rt, out).value),
                int(evalText("PARAM", rt, out).value), "PRM == PARAM");
}

CASE("Q.2P copies queue to a pattern; Q.P2 copies it back") {
    TeletypeProgram prog; init(prog);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    rt.variables.q_n = 4;
    rt.variables.q[0] = 10; rt.variables.q[1] = 20;
    rt.variables.q[2] = 30; rt.variables.q[3] = 40;
    evalP("Q.2P 0", rt, out, &prog);
    expectEqual(int(prog.patterns[0].len), 4, "pattern 0 len = q length");
    expectEqual(int(prog.patterns[0].val[0]), 10, "val[0]");
    expectEqual(int(prog.patterns[0].val[3]), 40, "val[3]");
    // wipe the queue, then copy the pattern back
    for (int i = 0; i < TT2_Q_LENGTH; ++i) rt.variables.q[i] = 0;
    rt.variables.q_n = 1;
    evalP("Q.P2 0", rt, out, &prog);
    expectEqual(int(rt.variables.q_n), 4, "q length restored");
    expectEqual(int(rt.variables.q[0]), 10, "q[0] restored");
    expectEqual(int(rt.variables.q[3]), 40, "q[3] restored");
}

CASE("Q.RND get returns a queue element; Q.RND x>0 fills random") {
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    rt.variables.q_n = 4;
    rt.variables.q[0] = 11; rt.variables.q[1] = 22;
    rt.variables.q[2] = 33; rt.variables.q[3] = 44;
    int r = int(evalText("Q.RND", rt, out).value);
    expectTrue(r == 11 || r == 22 || r == 33 || r == 44, "random element is from the queue");
    evalText("Q.RND 1", rt, out);  // x>0 -> fill all with one random value
    auto &v = rt.variables;
    expectTrue(v.q[0] == v.q[1] && v.q[1] == v.q[2] && v.q[2] == v.q[3],
               "all active slots set to the same random value");
}

}
