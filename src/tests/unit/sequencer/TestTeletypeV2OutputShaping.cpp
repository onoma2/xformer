#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"
#include "engine/TT2OutputShaping.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

namespace {

// Parse + lower + evaluate one command line against runtime/output.
TT2EvalResult evalText(const char *text, TT2Runtime &runtime, TT2OutputState &output) {
    tele_command_t src = {};
    char errMsg[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, errMsg);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, runtime, output);
}

} // namespace

// Pure shaping-advance helpers: native linear ms CV ramp + one-shot TR pulse.
// No Engine, no runtime — just the testable core of the output-shaping pass.
UNIT_TEST("TeletypeV2OutputShaping") {

    CASE("cv_slew_linear_midpoint_and_settle") {
        TT2CvOutput cv = {};
        tt2SeedCvSlew(cv, 16383, 100);  // 0 -> full over 100ms
        tt2AdvanceCvSlew(cv, 50);
        expectEqual(int(cv.currentQ >> 8), 8191, "halfway ~ half-scale");
        expectTrue(cv.remainingMs == 50, "half the ramp time remains");
        tt2AdvanceCvSlew(cv, 50);
        expectEqual(int(cv.currentQ >> 8), 16383, "settles exactly at target");
        expectTrue(cv.remainingMs == 0, "ramp complete");
    }

    CASE("hw_parity_output_shaping_defaults") {
        TeletypeProgram p; init(p);
        for (int i = 0; i < TT2_OUTPUT_CV_COUNT; ++i) {
            expectTrue(p.cvOutputRange[i] == Types::VoltageRange::Bipolar5V, "default ±5V");
            expectEqual(int(p.cvOutputOffset[i]), 0, "default offset 0");
            expectEqual(int(p.cvOutputQuantizeScale[i]), -1, "default no quantize");
            expectEqual(int(p.cvOutputRootNote[i]), 0, "default root 0");
        }
    }

    CASE("hw_parity_output_shaping_applied") {
        using R = Types::VoltageRange;
        auto approx = [](float a, float b) { float d = a - b; return (d < 0 ? -d : d) < 1e-3f; };
        // default: Bipolar5V, no offset, no quantize -> identity
        expectTrue(approx(Tt2OutputShaping::shapeCv(2.5f, R::Bipolar5V, 0, -1, 0, 0), 2.5f),
                   "default is identity");
        // Unipolar5V: base 0V (mid) -> 2.5V
        expectTrue(approx(Tt2OutputShaping::shapeCv(0.f, R::Unipolar5V, 0, -1, 0, 0), 2.5f),
                   "0V through Unipolar5V = 2.5V");
        // offset +1V (100 centivolts)
        expectTrue(approx(Tt2OutputShaping::shapeCv(0.f, R::Bipolar5V, 100, -1, 0, 0), 1.0f),
                   "offset +1V");
        // quantize: chromatic scale 0, 0.10V snaps to 1 semitone (1/12 V)
        expectTrue(approx(Tt2OutputShaping::shapeCv(0.10f, R::Bipolar5V, 0, 0, 0, 0), 1.f / 12.f),
                   "0.10V snaps to a semitone");
    }

    CASE("cv_slew_zero_snaps") {
        TT2CvOutput cv = {};
        tt2SeedCvSlew(cv, 9000, 0);
        expectEqual(int(cv.currentQ >> 8), 9000, "slewMs==0 snaps to target");
        expectTrue(cv.remainingMs == 0, "no ramp pending");
    }

    CASE("cv_slew_descending") {
        TT2CvOutput cv = {};
        tt2SeedCvSlew(cv, 16383, 0);    // start at full
        tt2SeedCvSlew(cv, 0, 100);      // ramp down to zero over 100ms
        tt2AdvanceCvSlew(cv, 50);
        expectEqual(int(cv.currentQ >> 8), 8191, "halfway down ~ half-scale");
        tt2AdvanceCvSlew(cv, 50);
        expectEqual(int(cv.currentQ >> 8), 0, "settles at zero");
        expectTrue(cv.remainingMs == 0, "ramp complete");
    }

    CASE("cv_slew_sub_lsb_no_stall") {
        TT2CvOutput cv = {};
        tt2SeedCvSlew(cv, 10, 10000);   // 10 LSB over 10s — coarse stepQ would stall
        int32_t prev = cv.currentQ;
        for (int t = 0; t < 10000; ++t) {
            tt2AdvanceCvSlew(cv, 1);
            expectTrue(cv.currentQ >= prev, "monotonic non-decreasing per ms");
            prev = cv.currentQ;
        }
        expectEqual(int(cv.currentQ >> 8), 10, "reaches target, no stall");
        expectTrue(cv.remainingMs == 0, "ramp complete");
    }

    CASE("tr_pulse_arm_hold_expire") {
        TT2TrOutput tr = {};
        tt2ArmTrPulse(tr, 1, 50);       // pol normal, 50ms
        expectEqual(int(tr.level), 1, "armed to active level");
        expectTrue(tr.pulseRemainingMs == 50, "pulse time armed");
        tt2AdvanceTrPulse(tr, 30);
        expectEqual(int(tr.level), 1, "still active before expiry");
        tt2AdvanceTrPulse(tr, 30);
        expectEqual(int(tr.level), 0, "restored to rest level after expiry");
        expectTrue(tr.pulseRemainingMs == 0, "pulse over");
    }

    CASE("tr_pulse_inverted_polarity") {
        TT2TrOutput tr = {};
        tt2ArmTrPulse(tr, 0, 20);       // pol inverted: active 0, rest 1
        expectEqual(int(tr.level), 0, "inverted active level is low");
        tt2AdvanceTrPulse(tr, 20);
        expectEqual(int(tr.level), 1, "inverted rest level is high");
    }

    CASE("tr_pulse_unarmed_is_noop") {
        TT2TrOutput tr = {};
        tt2AdvanceTrPulse(tr, 100);
        expectEqual(int(tr.level), 0, "unarmed level unchanged");
        expectTrue(tr.pulseRemainingMs == 0, "unarmed remains idle");
    }

    CASE("op_cv_slew_set_get_then_cv_ramps") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("CV.SLEW 1 100", runtime, output);
        expectEqual(runtime.variables.cv_slew[0], int16_t(100), "CV.SLEW set");
        expectEqual(int(evalText("CV.SLEW 1", runtime, output).value), 100, "CV.SLEW get");
        evalText("CV 1 16383", runtime, output);
        expectEqual(int(output.cv[0].targetRaw), 16383, "CV target seeded");
        expectTrue(output.cv[0].remainingMs == 100, "ramp pending, not instant");
    }

    CASE("op_cv_off_applies_to_target") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("CV.OFF 1 1000", runtime, output);
        expectEqual(runtime.variables.cv_off[0], int16_t(1000), "CV.OFF set");
        expectEqual(int(evalText("CV.OFF 1", runtime, output).value), 1000, "CV.OFF get");
        evalText("CV 1 5000", runtime, output);
        expectEqual(int(output.cv[0].targetRaw), 6000, "target = value + offset");
    }

    CASE("op_tr_time_set_get_and_pulse") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("TR.TIME 1 50", runtime, output);
        expectEqual(runtime.variables.tr_time[0], int16_t(50), "TR.TIME set");
        expectEqual(int(evalText("TR.TIME 1", runtime, output).value), 50, "TR.TIME get");
        evalText("TR.P 1", runtime, output);
        expectEqual(int(output.tr[0].level), 1, "TR.P active level (pol 1)");
        expectTrue(output.tr[0].pulseRemainingMs == 50, "pulse armed at TR.TIME");
    }

    CASE("op_tr_pulse_alias_matches_tr_p") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("TR.TIME 1 40", runtime, output);
        evalText("TR.PULSE 1", runtime, output);
        expectEqual(int(output.tr[0].level), 1, "TR.PULSE arms like TR.P");
        expectTrue(output.tr[0].pulseRemainingMs == 40, "TR.PULSE armed");
    }

    CASE("op_tr_pol_set_get_and_inverted_pulse") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("TR.POL 1 0", runtime, output);
        expectEqual(runtime.variables.tr_pol[0], int16_t(0), "TR.POL set 0");
        expectEqual(int(evalText("TR.POL 1", runtime, output).value), 0, "TR.POL get");
        evalText("TR.TIME 1 30", runtime, output);
        evalText("TR.P 1", runtime, output);
        expectEqual(int(output.tr[0].level), 0, "inverted pol active level low");
        tt2AdvanceTrPulse(output.tr[0], 30);
        expectEqual(int(output.tr[0].level), 1, "inverted pol rest level high");
    }

    CASE("op_tr_tog_flips_level") {
        TT2Runtime runtime = {}; init(runtime);
        TT2OutputState output = {}; init(output);
        evalText("TR 1 0", runtime, output);
        evalText("TR.TOG 1", runtime, output);
        expectEqual(int(output.tr[0].level), 1, "TOG 0 -> 1");
        evalText("TR.TOG 1", runtime, output);
        expectEqual(int(output.tr[0].level), 0, "TOG 1 -> 0");
    }
}
