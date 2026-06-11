#include "UnitTest.h"

#include "apps/sequencer/model/PhaseFluxMath.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"

#include <cmath>

// v0.2 pipeline order: Warp -> Repeat -> Window -> FlipH -> Curve -> Response -> FlipV.
// evalPitchPhase covers the pre-curve phase domain (Warp->Repeat->Window->FlipH).
// evalResponseFlipV covers the post-curve value tail (Response->FlipV).

using Rep = PhaseFluxSequence::RepeatType;
using Win = PhaseFluxSequence::WindowType;

static bool approxEq(float a, float b, float tol = 1e-5f) {
    return std::fabs(a - b) <= tol;
}

static float bend(float z, int knob) {
    if (knob == 0) return z;
    float c = std::max(0.f, std::min(1.f, z));
    return PhaseFluxMath::powerBend(c, PhaseFluxMath::powerBendKnobToParam(knob));
}

UNIT_TEST("PhaseFluxPipelineOrder") {

// ---- Warp BEFORE Repeat (v0.2) ----
CASE("warp_precedes_repeat") {
    // New order: phi -> warp -> *R -> fmod. Old order warped the post-repeat
    // sawtooth. With warp=+32 and x2, expect fmod(bend(phi)*2, 1).
    const int warp = 32;
    float phi = 0.3f;
    float expected = std::fmod(bend(phi, warp) * 2.f, 1.f);
    float got = PhaseFluxMath::evalPitchPhase(phi, warp, 2, Win::Off, false);
    expectEqual(approxEq(got, expected), true, "warp applied before repeat multiply");
}

CASE("warp_before_repeat_differs_from_repeat_before_warp") {
    const int warp = 40;
    float phi = 0.42f;
    float newOrder = PhaseFluxMath::evalPitchPhase(phi, warp, 3, Win::Off, false);
    float oldOrder = bend(std::fmod(phi * 3.f, 1.f), warp); // repeat then warp (old)
    expectEqual(approxEq(newOrder, oldOrder), false, "order actually changes the value");
}

// ---- Window AFTER Repeat (v0.2) ----
CASE("window_after_repeat_clamps_per_cycle") {
    // x2 tiles phi into two cycles; Focus50 then holds each cycle's phase to
    // [0.25,0.75]. phi=0.1 -> warp off -> *2=0.2 -> Focus50 hold -> 0.25.
    float got = PhaseFluxMath::evalPitchPhase(0.1f, 0, 2, Win::Focus50, false);
    expectEqual(approxEq(got, 0.25f), true, "windowed after repeat -> per-cycle clamp");
}

// ---- FlipH mirrors the windowed phase ----
CASE("flipH_mirrors_post_window_phase") {
    float plain = PhaseFluxMath::evalPitchPhase(0.3f, 0, 1, Win::Off, false);
    float flipped = PhaseFluxMath::evalPitchPhase(0.3f, 0, 1, Win::Off, true);
    expectEqual(approxEq(flipped, 1.f - plain), true, "flipH = 1 - phi_windowed");
}

// ---- Response BEFORE FlipV (v0.2) ----
CASE("response_precedes_flipV") {
    const int resp = 28;
    float p = 0.4f;
    float expected = 1.f - bend(p, resp); // response first, then flipV
    float got = PhaseFluxMath::evalResponseFlipV(p, resp, true);
    expectEqual(approxEq(got, expected), true, "response applied then flipV mirror");
}

// ---- flipH and flipV are INDEPENDENT on Ramp once response sits between ----
CASE("flipH_and_flipV_independent_on_ramp_with_response") {
    // Ramp curve = identity, so phi_input passes through Curve unchanged.
    const int resp = 36;
    float phi = 0.3f;
    // flipH only: phase mirrored, no value flip.
    float hPhase = PhaseFluxMath::evalPitchPhase(phi, 0, 1, Win::Off, true);
    float hOut = PhaseFluxMath::evalResponseFlipV(hPhase, resp, false);
    // flipV only: phase plain, value flipped after response.
    float vPhase = PhaseFluxMath::evalPitchPhase(phi, 0, 1, Win::Off, false);
    float vOut = PhaseFluxMath::evalResponseFlipV(vPhase, resp, true);
    expectEqual(approxEq(hOut, vOut), false, "H and V give distinct results with response");
}

// ---- Temporal Warp BEFORE Repeat (v0.2, decision A): warped sub-section allocation ----
CASE("temporal_alloc_warp0_even_divisible") {
    // pulseCount 6, R 3, warp 0 -> even 2/2/2, each sub at local 0 and 0.5.
    const int pc = 6, R = 3;
    const int expSub[6]   = { 0, 0, 1, 1, 2, 2 };
    const float expLoc[6] = { 0.f, 0.5f, 0.f, 0.5f, 0.f, 0.5f };
    for (int i = 0; i < pc; ++i) {
        int sub; float loc;
        PhaseFluxMath::evalTemporalAlloc(i, pc, R, 0, sub, loc);
        expectEqual(sub, expSub[i], "sub idx warp0");
        expectEqual(approxEq(loc, expLoc[i]), true, "local pos warp0");
    }
}

CASE("temporal_alloc_globally_even_at_warp0") {
    // With Ramp (identity), global position = (sub + local)/R must equal i/pc.
    const int pc = 6, R = 3;
    for (int i = 0; i < pc; ++i) {
        int sub; float loc;
        PhaseFluxMath::evalTemporalAlloc(i, pc, R, 0, sub, loc);
        float global = (float(sub) + loc) / float(R);
        expectEqual(approxEq(global, float(i) / float(pc)), true, "globally even at warp0");
    }
}

CASE("temporal_alloc_warp_shifts_subsection") {
    // Pulse 3 of 6 sits in sub 1 at warp 0; strong positive warp pushes its
    // whole-cell seed up so it crosses into sub 2 — uneven sub-sections.
    int sub0, sub1; float loc0, loc1;
    PhaseFluxMath::evalTemporalAlloc(3, 6, 3, 0,  sub0, loc0);
    PhaseFluxMath::evalTemporalAlloc(3, 6, 3, 50, sub1, loc1);
    expectEqual(sub0, 1, "warp0 -> sub 1");
    expectEqual(sub1 != sub0, true, "warp moves pulse to a different sub-section");
}

}
