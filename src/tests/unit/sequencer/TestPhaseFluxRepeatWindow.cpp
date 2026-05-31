#include "UnitTest.h"

#include "apps/sequencer/model/PhaseFluxMath.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"

#include <cmath>

// §14.2 Repeat + Window helpers. Pipeline order: Window → Repeat → ... .
// Window cropping uses original phi. Repeat scales phi × R then takes fmod.
// Bug surface: combining Focus / Polarize with x2..x5 — verify both gates
// and the resulting post-Window-post-Repeat phi values.

using Rep = PhaseFluxSequence::RepeatType;
using Win = PhaseFluxSequence::WindowType;

static bool approxEq(float a, float b, float tol = 1e-5f) {
    return std::fabs(a - b) <= tol;
}

UNIT_TEST("PhaseFluxRepeatWindow") {

CASE("repeat_multiplier_values") {
    expectEqual(PhaseFluxMath::repeatMultiplier(Rep::x1), 1, "x1 = 1");
    expectEqual(PhaseFluxMath::repeatMultiplier(Rep::x2), 2, "x2 = 2");
    expectEqual(PhaseFluxMath::repeatMultiplier(Rep::x3), 3, "x3 = 3");
    expectEqual(PhaseFluxMath::repeatMultiplier(Rep::x5), 5, "x5 = 5");
}

CASE("window_off_always_visible") {
    expectEqual(PhaseFluxMath::isInWindow(0.0f, Win::Off), true,  "phi=0 Off");
    expectEqual(PhaseFluxMath::isInWindow(0.5f, Win::Off), true,  "phi=0.5 Off");
    expectEqual(PhaseFluxMath::isInWindow(1.0f, Win::Off), true,  "phi=1 Off");
}

CASE("window_focus_70_visible_center") {
    // Center 70% = [0.15, 0.85]
    expectEqual(PhaseFluxMath::isInWindow(0.10f, Win::Focus70), false, "phi=0.10 out");
    expectEqual(PhaseFluxMath::isInWindow(0.15f, Win::Focus70), true,  "phi=0.15 in (edge)");
    expectEqual(PhaseFluxMath::isInWindow(0.50f, Win::Focus70), true,  "phi=0.5 in");
    expectEqual(PhaseFluxMath::isInWindow(0.85f, Win::Focus70), true,  "phi=0.85 in (edge)");
    expectEqual(PhaseFluxMath::isInWindow(0.90f, Win::Focus70), false, "phi=0.9 out");
}

CASE("window_focus_50_visible_center") {
    // Center 50% = [0.25, 0.75]
    expectEqual(PhaseFluxMath::isInWindow(0.20f, Win::Focus50), false, "phi=0.20 out");
    expectEqual(PhaseFluxMath::isInWindow(0.25f, Win::Focus50), true,  "phi=0.25 in (edge)");
    expectEqual(PhaseFluxMath::isInWindow(0.50f, Win::Focus50), true,  "phi=0.5 in");
    expectEqual(PhaseFluxMath::isInWindow(0.75f, Win::Focus50), true,  "phi=0.75 in (edge)");
    expectEqual(PhaseFluxMath::isInWindow(0.80f, Win::Focus50), false, "phi=0.80 out");
}

CASE("window_polarize_70_visible_outer") {
    // Outer 70% total = [0, 0.35] ∪ [0.65, 1] ; middle 30% hidden
    expectEqual(PhaseFluxMath::isInWindow(0.00f, Win::Polarize70), true,  "phi=0.00 in (left)");
    expectEqual(PhaseFluxMath::isInWindow(0.35f, Win::Polarize70), true,  "phi=0.35 in (left edge)");
    expectEqual(PhaseFluxMath::isInWindow(0.45f, Win::Polarize70), false, "phi=0.45 out (middle)");
    expectEqual(PhaseFluxMath::isInWindow(0.65f, Win::Polarize70), true,  "phi=0.65 in (right edge)");
    expectEqual(PhaseFluxMath::isInWindow(1.00f, Win::Polarize70), true,  "phi=1.00 in (right)");
}

CASE("window_polarize_50_visible_outer") {
    // Outer 50% total = [0, 0.25] ∪ [0.75, 1] ; middle 50% hidden
    expectEqual(PhaseFluxMath::isInWindow(0.10f, Win::Polarize50), true,  "phi=0.10 in (left)");
    expectEqual(PhaseFluxMath::isInWindow(0.25f, Win::Polarize50), true,  "phi=0.25 in (left edge)");
    expectEqual(PhaseFluxMath::isInWindow(0.30f, Win::Polarize50), false, "phi=0.30 out (middle)");
    expectEqual(PhaseFluxMath::isInWindow(0.50f, Win::Polarize50), false, "phi=0.50 out (middle)");
    expectEqual(PhaseFluxMath::isInWindow(0.75f, Win::Polarize50), true,  "phi=0.75 in (right edge)");
    expectEqual(PhaseFluxMath::isInWindow(0.90f, Win::Polarize50), true,  "phi=0.90 in (right)");
}

CASE("hold_pitch_window_boundary_focus_clamps_to_visible_edges") {
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.10f, Win::Focus50), 0.25f), true,
                "Focus50 low hidden phi holds at low edge");
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.90f, Win::Focus50), 0.75f), true,
                "Focus50 high hidden phi holds at high edge");
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.50f, Win::Focus50), 0.50f), true,
                "Focus50 visible phi passes unchanged");
}

CASE("hold_pitch_window_boundary_polarize_clamps_middle_to_nearest_edge") {
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.40f, Win::Polarize50), 0.25f), true,
                "Polarize50 lower-middle hidden phi holds at left edge");
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.60f, Win::Polarize50), 0.75f), true,
                "Polarize50 upper-middle hidden phi holds at right edge");
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.10f, Win::Polarize50), 0.10f), true,
                "Polarize50 visible left phi passes unchanged");
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.90f, Win::Polarize50), 0.90f), true,
                "Polarize50 visible right phi passes unchanged");
}

CASE("hold_pitch_window_boundary_off_is_identity") {
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.10f, Win::Off), 0.10f), true,
                "Off low phi passes unchanged");
    expectEqual(approxEq(PhaseFluxMath::holdPitchWindowBoundary(0.90f, Win::Off), 0.90f), true,
                "Off high phi passes unchanged");
}

// ---- evalWindowRepeat — combined pipeline ----

CASE("eval_off_x1_identity") {
    float phiOut = -1.f;
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.5f, Win::Off, Rep::x1, phiOut), true, "Off x1 visible");
    expectEqual(approxEq(phiOut, 0.5f), true, "phi unchanged");
}

CASE("eval_off_x2_doubles_phi_modulo") {
    float phiOut = -1.f;
    PhaseFluxMath::evalWindowRepeat(0.10f, Win::Off, Rep::x2, phiOut);
    expectEqual(approxEq(phiOut, 0.20f), true, "phi=0.10 × 2 = 0.20");
    PhaseFluxMath::evalWindowRepeat(0.60f, Win::Off, Rep::x2, phiOut);
    expectEqual(approxEq(phiOut, 0.20f), true, "phi=0.60 × 2 = 1.2 mod 1 = 0.20");
    PhaseFluxMath::evalWindowRepeat(0.95f, Win::Off, Rep::x2, phiOut);
    expectEqual(approxEq(phiOut, 0.90f), true, "phi=0.95 × 2 = 1.9 mod 1 = 0.90");
}

CASE("eval_off_x3_triples_phi") {
    float phiOut = -1.f;
    PhaseFluxMath::evalWindowRepeat(0.10f, Win::Off, Rep::x3, phiOut);
    expectEqual(approxEq(phiOut, 0.30f), true, "phi=0.10 × 3 = 0.30");
    PhaseFluxMath::evalWindowRepeat(0.40f, Win::Off, Rep::x3, phiOut);
    expectEqual(approxEq(phiOut, 0.20f), true, "phi=0.40 × 3 = 1.2 mod 1 = 0.20");
    PhaseFluxMath::evalWindowRepeat(0.70f, Win::Off, Rep::x3, phiOut);
    expectEqual(approxEq(phiOut, 0.10f), true, "phi=0.70 × 3 = 2.1 mod 1 = 0.10");
}

CASE("eval_off_x5_quintuples_phi") {
    float phiOut = -1.f;
    PhaseFluxMath::evalWindowRepeat(0.10f, Win::Off, Rep::x5, phiOut);
    expectEqual(approxEq(phiOut, 0.50f), true, "phi=0.10 × 5 = 0.50");
    PhaseFluxMath::evalWindowRepeat(0.34f, Win::Off, Rep::x5, phiOut);
    expectEqual(approxEq(phiOut, 0.70f), true, "phi=0.34 × 5 = 1.70 mod 1 = 0.70");
}

CASE("eval_focus_50_x1_drops_outside_keeps_inside") {
    float phiOut = -1.f;
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.10f, Win::Focus50, Rep::x1, phiOut), false, "phi=0.10 out");
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.50f, Win::Focus50, Rep::x1, phiOut), true,  "phi=0.50 in");
    expectEqual(approxEq(phiOut, 0.50f), true, "phi unchanged inside");
}

CASE("eval_focus_50_x3_combines_window_then_repeat") {
    // Window check on RAW phi (Focus 50: [0.25, 0.75]).
    // If visible, output = fmod(raw × 3, 1).
    float phiOut = -1.f;

    // phi=0.20 — outside window, no eval.
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.20f, Win::Focus50, Rep::x3, phiOut), false,
                "Focus50 x3: phi=0.20 outside window");

    // phi=0.30 — inside window. 0.30 × 3 = 0.90.
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.30f, Win::Focus50, Rep::x3, phiOut), true,
                "Focus50 x3: phi=0.30 inside");
    expectEqual(approxEq(phiOut, 0.90f), true, "Focus50 x3: 0.30 × 3 = 0.90");

    // phi=0.50 — inside. 0.50 × 3 = 1.50 mod 1 = 0.50.
    PhaseFluxMath::evalWindowRepeat(0.50f, Win::Focus50, Rep::x3, phiOut);
    expectEqual(approxEq(phiOut, 0.50f), true, "Focus50 x3: 0.50 × 3 mod 1 = 0.50");

    // phi=0.70 — inside. 0.70 × 3 = 2.10 mod 1 = 0.10.
    PhaseFluxMath::evalWindowRepeat(0.70f, Win::Focus50, Rep::x3, phiOut);
    expectEqual(approxEq(phiOut, 0.10f), true, "Focus50 x3: 0.70 × 3 mod 1 = 0.10");

    // phi=0.80 — outside, drops.
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.80f, Win::Focus50, Rep::x3, phiOut), false,
                "Focus50 x3: phi=0.80 outside");
}

CASE("eval_polarize_50_x2_drops_middle") {
    // Polarize 50 visible: [0, 0.25] ∪ [0.75, 1].
    float phiOut = -1.f;

    // Left band: phi=0.10 → in window. 0.10 × 2 = 0.20.
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.10f, Win::Polarize50, Rep::x2, phiOut), true,
                "Polarize50 x2: phi=0.10 in left band");
    expectEqual(approxEq(phiOut, 0.20f), true, "phiOut = 0.20");

    // Middle hidden: phi=0.50 → drops.
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.50f, Win::Polarize50, Rep::x2, phiOut), false,
                "Polarize50 x2: phi=0.50 middle hidden");

    // Right band: phi=0.85 → in. 0.85 × 2 = 1.70 mod 1 = 0.70.
    expectEqual(PhaseFluxMath::evalWindowRepeat(0.85f, Win::Polarize50, Rep::x2, phiOut), true,
                "Polarize50 x2: phi=0.85 in right band");
    expectEqual(approxEq(phiOut, 0.70f), true, "phiOut = 1.7 mod 1 = 0.70");
}

CASE("eval_polarize_70_x5_drops_middle") {
    // Polarize 70 visible: [0, 0.35] ∪ [0.65, 1].
    float phiOut = -1.f;

    expectEqual(PhaseFluxMath::evalWindowRepeat(0.30f, Win::Polarize70, Rep::x5, phiOut), true,
                "Polarize70 x5: phi=0.30 left band");
    expectEqual(approxEq(phiOut, 0.50f), true, "phi=0.30 × 5 = 1.50 mod 1 = 0.50");

    expectEqual(PhaseFluxMath::evalWindowRepeat(0.50f, Win::Polarize70, Rep::x5, phiOut), false,
                "Polarize70 x5: phi=0.50 middle hidden");

    expectEqual(PhaseFluxMath::evalWindowRepeat(0.70f, Win::Polarize70, Rep::x5, phiOut), true,
                "Polarize70 x5: phi=0.70 right band");
    expectEqual(approxEq(phiOut, 0.50f), true, "phi=0.70 × 5 = 3.50 mod 1 = 0.50");
}

CASE("eval_focus_70_full_phi_sweep_x2") {
    // Focus 70 visible: [0.15, 0.85]. With x2, the windowed sub-range gets
    // 2 traversals of the curve (because Repeat works on RAW phi domain).
    float phiOut = -1.f;
    int visibleCount = 0;
    const int samples = 100;
    for (int i = 0; i < samples; ++i) {
        float phi = float(i) / float(samples - 1);
        if (PhaseFluxMath::evalWindowRepeat(phi, Win::Focus70, Rep::x2, phiOut)) {
            ++visibleCount;
        }
    }
    // Window 70% → ~70 samples visible. With 100 samples (0..1 incl), 70..71
    // samples in [0.15, 0.85] (inclusive endpoints).
    expectEqual(visibleCount >= 69 && visibleCount <= 72, true,
                "Focus70 keeps ~70% of samples regardless of Repeat value");
}

CASE("eval_polarize_50_full_phi_sweep_x3") {
    // Polarize 50 visible: outer 50% total. 100 samples → ~51 visible.
    float phiOut = -1.f;
    int visibleCount = 0;
    const int samples = 100;
    for (int i = 0; i < samples; ++i) {
        float phi = float(i) / float(samples - 1);
        if (PhaseFluxMath::evalWindowRepeat(phi, Win::Polarize50, Rep::x3, phiOut)) {
            ++visibleCount;
        }
    }
    expectEqual(visibleCount >= 50 && visibleCount <= 54, true,
                "Polarize50 keeps ~50% of samples regardless of Repeat value");
}

}
