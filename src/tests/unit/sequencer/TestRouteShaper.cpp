#include "UnitTest.h"

#include "apps/sequencer/model/RouteShaper.h"
#include "apps/sequencer/model/RouteApply.h"
#include "apps/sequencer/model/Routing.h"

#include <cmath>

// Step 2 of the apply-fork slice: the bias-free shaper stage feeding RouteApply.
// Only None + TriangleFold are ported this slice (remaining shapers land later).
// The new model drops bias (d subsumes it), so TriangleFold's center is fixed at
// 0.5 -- h=0.5 maps to 0.5, keeping RouteApply Modulate neutral at source-center.
// Pure: no engine state. Header-only/test-only; nothing wired live.

namespace {

using RouteApply::Combine;

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

constexpr float R = 60.f;

} // namespace

UNIT_TEST("RouteShaper") {

CASE("None is identity") {
    expectTrue(near(RouteShaper::shape(Routing::Shaper::None, 0.0f), 0.0f), "0");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::None, 0.37f), 0.37f), "mid");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::None, 1.0f), 1.0f), "1");
}

CASE("TriangleFold preserves source-center") {
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold, 0.5f), 0.5f), "center fixed at 0.5");
}

CASE("TriangleFold maps the bias-free triangle (endpoints fold to center)") {
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold, 0.0f), 0.5f),  "0 -> 0.5");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold, 0.25f), 0.0f), "0.25 -> 0");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold, 0.75f), 1.0f), "0.75 -> 1");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold, 1.0f), 0.5f),  "1 -> 0.5");
}

CASE("TriangleFold output stays in [0,1]") {
    for (int i = 0; i <= 100; ++i) {
        float h = i / 100.f;
        float out = RouteShaper::shape(Routing::Shaper::TriangleFold, h);
        expectTrue(out >= 0.f && out <= 1.f, "in range");
    }
}

CASE("Crease is a bias-free discontinuous fold at center (+-0.5 wrap)") {
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease, 0.0f), 0.5f),  "0 -> 0.5");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease, 0.5f), 1.0f),  "0.5 -> 1.0 (<= threshold)");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease, 1.0f), 0.5f),  "1 -> 0.5");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease, 0.25f), 0.75f), "0.25 -> 0.75");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease, 0.75f), 0.25f), "0.75 -> 0.25");
}

CASE("off-center fold variants fold at their fixed center, not 0.5") {
    // TriangleFold30/70: source = center -> 0.5 (the fold turning point)
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold30, 0.3f), 0.5f), "F30 center 0.3 -> 0.5");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold70, 0.7f), 0.5f), "F70 center 0.7 -> 0.5");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold30, 0.0f), 0.1f), "F30 at 0 -> 0.1");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::TriangleFold70, 1.0f), 0.9f), "F70 at 1 -> 0.9");
    // Crease10/90: threshold shifts where the +-0.5 wrap flips
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease10, 0.1f), 0.6f), "C10 at threshold 0.1 -> 0.6");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease10, 0.5f), 0.0f), "C10 above threshold -> 0");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease90, 0.9f), 1.0f), "C90 at threshold 0.9 -> 1.0");
    expectTrue(near(RouteShaper::shape(Routing::Shaper::Crease90, 1.0f), 0.5f), "C90 above threshold -> 0.5");
}

CASE("the remaining (stateful) shapers fall through to identity — they move to modulators") {
    for (auto shaper : { Routing::Shaper::Location,
                         Routing::Shaper::Envelope, Routing::Shaper::FrequencyFollower,
                         Routing::Shaper::Activity, Routing::Shaper::ProgressiveDivider,
                         Routing::Shaper::VcaNext }) {
        expectTrue(near(RouteShaper::shape(shaper, 0.37f), 0.37f), "identity");
        expectTrue(near(RouteShaper::shape(shaper, 0.0f), 0.0f), "identity 0");
        expectTrue(near(RouteShaper::shape(shaper, 1.0f), 1.0f), "identity 1");
    }
}

CASE("TriangleFold composes with RouteApply Modulate: center stays neutral") {
    float h = RouteShaper::shape(Routing::Shaper::TriangleFold, 0.5f);
    expectTrue(near(RouteApply::delta(h, 1.0f, Combine::Modulate, 100, R), 0.f), "center -> delta 0");
}

} // UNIT_TEST
