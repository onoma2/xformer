#include "UnitTest.h"

#include "apps/sequencer/model/PhaseFluxMath.h"

#include <cmath>

static bool nearly(float a, float b, float eps = 1e-5f) {
    return std::fabs(a - b) < eps;
}

UNIT_TEST("PhaseFluxPowerBend") {

CASE("identity_at_zero_p") {
    const float zs[] = { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f };
    for (int i = 0; i < 5; ++i) {
        float y = PhaseFluxMath::powerBend(zs[i], 0.0f);
        expectTrue(nearly(y, zs[i]), "powerBend(z, 0) should be z");
    }
}

CASE("endpoints_preserved") {
    const float ps[] = { -0.9f, -0.5f, 0.0f, 0.5f, 0.9f };
    for (int i = 0; i < 5; ++i) {
        float y0 = PhaseFluxMath::powerBend(0.0f, ps[i]);
        float y1 = PhaseFluxMath::powerBend(1.0f, ps[i]);
        expectTrue(nearly(y0, 0.0f), "powerBend(0, p) must be 0");
        expectTrue(nearly(y1, 1.0f), "powerBend(1, p) must be 1");
    }
}

CASE("positive_p_lifts_output") {
    // p > 0: exponent (1-p)/(1+p) < 1, so z^k > z for z in (0,1). The curve
    // sits above the diagonal — input values map to higher outputs, i.e.
    // domain density is "compressed toward low" per spec §6.
    float y = PhaseFluxMath::powerBend(0.25f, 0.5f);
    expectTrue(y > 0.25f, "p > 0 should lift mid-low z above the input");
    expectTrue(y < 1.0f, "must remain below 1");
}

CASE("negative_p_drops_output") {
    // p < 0: exponent > 1, so z^k < z for z in (0,1). The curve sits below
    // the diagonal — domain density "compressed toward high" per spec §6.
    float y = PhaseFluxMath::powerBend(0.75f, -0.5f);
    expectTrue(y < 0.75f, "p < 0 should drop z below the input");
    expectTrue(y > 0.0f, "must remain positive");
}

CASE("monotonic_in_z") {
    const float ps[] = { -0.5f, 0.0f, 0.5f };
    for (int pi = 0; pi < 3; ++pi) {
        float prev = PhaseFluxMath::powerBend(0.0f, ps[pi]);
        for (int i = 1; i <= 16; ++i) {
            float z = float(i) / 16.0f;
            float cur = PhaseFluxMath::powerBend(z, ps[pi]);
            expectTrue(cur >= prev - 1e-6f, "powerBend must be monotonic non-decreasing in z");
            prev = cur;
        }
    }
}

CASE("encoded_min_max_safe") {
    float pMin = PhaseFluxMath::powerBendKnobToParam(-63);
    float pMax = PhaseFluxMath::powerBendKnobToParam(63);
    expectTrue(nearly(pMin, -63.0f / 64.0f), "encoded -63 must map to -63/64");
    expectTrue(nearly(pMax,  63.0f / 64.0f), "encoded +63 must map to +63/64");
    // The degeneracy guard: both magnitudes are strictly less than 1.
    expectTrue(std::fabs(pMin) < 1.0f, "min encoded must avoid degenerate -1");
    expectTrue(std::fabs(pMax) < 1.0f, "max encoded must avoid degenerate +1");
    // Pin numerically to 63/64 = 0.984375 exactly (no rounding noise).
    expectTrue(nearly(std::fabs(pMin), 0.984375f), "pin -0.984375");
    expectTrue(nearly(std::fabs(pMax), 0.984375f), "pin +0.984375");
}

CASE("encoded_zero_is_identity") {
    float p = PhaseFluxMath::powerBendKnobToParam(0);
    expectTrue(p == 0.0f, "encoded 0 must map to exactly 0.0");
}

CASE("encoded_endpoints_keep_endpoints") {
    // powerBend(0, p_max) and (1, p_max) must still preserve endpoints.
    float pMax = PhaseFluxMath::powerBendKnobToParam(63);
    float pMin = PhaseFluxMath::powerBendKnobToParam(-63);
    expectTrue(nearly(PhaseFluxMath::powerBend(0.0f, pMax), 0.0f), "y(0,pMax) = 0");
    expectTrue(nearly(PhaseFluxMath::powerBend(1.0f, pMax), 1.0f), "y(1,pMax) = 1");
    expectTrue(nearly(PhaseFluxMath::powerBend(0.0f, pMin), 0.0f), "y(0,pMin) = 0");
    expectTrue(nearly(PhaseFluxMath::powerBend(1.0f, pMin), 1.0f), "y(1,pMin) = 1");
}

}
