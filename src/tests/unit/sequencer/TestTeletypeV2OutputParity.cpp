#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

// Output-VALUE parity harness (the bridge-deletion confidence gate, headless layer).
//
// Runs identical command text through BOTH the legacy C VM (process_command on a
// scene_state_t) and the native TT2 runner (evaluateCommand on a TT2OutputState),
// then asserts the **CV/gate target values match**: legacy ss.variables.cv[i]/tr[i]
// vs native out.cv[i].targetRaw / out.tr[i].level.
//
// This is the correct equivalence bar for deleting the bridge: both engines must
// COMPUTE the same outputs for a script. It deliberately does NOT compare the
// tick-shaped trajectory (slew curve / pulse timing) — TT2's output shaping is a
// native reimplementation, intentionally not bit-identical to the legacy bridge, and
// is specified + tested on its own in TestTeletypeV2OutputShaping. (Engine-level tick
// comparison would need a full Engine for both backends and would fail on intended
// shaping differences — wrong bar.)

namespace {

void tt1Run(const char *text, scene_state_t *ss, exec_state_t *es) {
    tele_command_t cmd = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &cmd, err);
    process_command(ss, es, &cmd);
}

void tt2Run(const char *text, TT2Runtime &rt, TT2OutputState &out) {
    tele_command_t src = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, err);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    evaluateCommand(cmd, rt, out);
}

} // namespace

UNIT_TEST("TeletypeV2OutputParity") {

// Parity holds only over the shared output range: legacy teletype has 4 CV / 4 TR
// (CV_COUNT/TR_COUNT); TT2 deliberately extends to 8 (Performer). Outputs 5-8 are a
// TT2-only feature with no legacy counterpart, so they're outside the parity bar.

CASE("CV output targets match the legacy VM (shared 1-4)") {
    static const char *scripts[] = {
        "CV 1 5", "CV 2 100", "CV 3 16383", "CV 4 4096",
        "CV 1 V 1", "CV 1 V 5", "CV 1 V -2",
        "CV 1 N 0", "CV 1 N 12", "CV 1 N -12",
        "CV 4 ADD 100 5", "CV 3 MUL 6 7", "CV 2 LIM 50 0 10",
    };
    static scene_state_t ss;
    for (auto *s : scripts) {
        ss_init(&ss);
        exec_state_t es; es_init(&es);
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        tt1Run(s, &ss, &es);
        tt2Run(s, rt, out);
        for (int i = 0; i < CV_COUNT; ++i) {
            expectEqual(int(out.cv[i].targetRaw), int(ss.variables.cv[i]), s);
        }
    }
}

CASE("TR gate state matches the legacy VM (shared 1-4)") {
    static const char *scripts[] = { "TR 1 1", "TR 2 1", "TR 4 1", "TR 1 0", "TR 3 1" };
    static scene_state_t ss;
    for (auto *s : scripts) {
        ss_init(&ss);
        exec_state_t es; es_init(&es);
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        tt1Run(s, &ss, &es);
        tt2Run(s, rt, out);
        for (int i = 0; i < TR_COUNT; ++i) {
            // gate high/low state (legacy stores 0/1; native level is non-zero=high)
            expectEqual(int(out.tr[i].level != 0), int(ss.variables.tr[i] != 0), s);
        }
    }
}

CASE("multi-command script accumulates the same CV/gate output") {
    static scene_state_t ss; ss_init(&ss);
    exec_state_t es; es_init(&es);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    const char *lines[] = { "CV 1 100", "CV 2 200", "CV 3 V 2", "TR 1 1", "TR 3 1" };
    for (auto *l : lines) { tt1Run(l, &ss, &es); tt2Run(l, rt, out); }
    for (int i = 0; i < CV_COUNT; ++i) {
        expectEqual(int(out.cv[i].targetRaw), int(ss.variables.cv[i]), "cv accum");
    }
    for (int i = 0; i < TR_COUNT; ++i) {
        expectEqual(int(out.tr[i].level != 0), int(ss.variables.tr[i] != 0), "tr accum");
    }
}

}
