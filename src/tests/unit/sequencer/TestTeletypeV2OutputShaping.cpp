#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"

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
}
