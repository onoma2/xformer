#include "UnitTest.h"

#include "apps/sequencer/model/RouteApply.h"

#include <cmath>

// U5 + U6b combine math: the name-agnostic value pipeline that turns a shaped,
// normalized source into a signed delta added to the param base. Pure function,
// no engine state, built alongside the live old dispatch (nothing wired live).
//
//   hs    = 0.5 + (h - 0.5) * scaleValue          (scaleSource: center-preserving)
//   u     = Modulate ? (hs - 0.5) * 2 : hs        (centered [-1,1] vs raw [0,1])
//   delta = (dPercent / 100) * u * range
//
// Contracts (the plan's neutral-at-center + base-at-clamp): scaleValue = 1.0 means
// "no scale source" (identity). range = the param's d=100% displacement.

namespace {

using RouteApply::Combine;

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

constexpr float R = 60.f;   // e.g. Transpose displacement

} // namespace

UNIT_TEST("RouteApply") {

CASE("Modulate is neutral at source-center for any d / range / scaleValue") {
    expectTrue(near(RouteApply::delta(0.5f, 1.0f, Combine::Modulate, 100, R), 0.f), "d=100");
    expectTrue(near(RouteApply::delta(0.5f, 1.0f, Combine::Modulate, -73, R), 0.f), "d<0");
    expectTrue(near(RouteApply::delta(0.5f, 0.3f, Combine::Modulate, 100, R), 0.f), "with scale source");
    expectTrue(near(RouteApply::delta(0.5f, 1.0f, Combine::Modulate, 100, 1000.f), 0.f), "big range");
}

CASE("Modulate full swing reaches +/- d*range") {
    expectTrue(near(RouteApply::delta(1.0f, 1.0f, Combine::Modulate, 100, R), R),  "source 1 -> +range");
    expectTrue(near(RouteApply::delta(0.0f, 1.0f, Combine::Modulate, 100, R), -R), "source 0 -> -range");
}

CASE("Modulate depth scales with d (signed percent)") {
    expectTrue(near(RouteApply::delta(1.0f, 1.0f, Combine::Modulate, 50, R), 0.5f * R),  "d=50 -> half");
    expectTrue(near(RouteApply::delta(1.0f, 1.0f, Combine::Modulate, -100, R), -R),      "d=-100 flips");
    expectTrue(near(RouteApply::delta(1.0f, 1.0f, Combine::Modulate, 0, R), 0.f),        "d=0 -> nothing");
}

CASE("Absolute sweeps from base (source 0 = base, source 1 = base + d*range)") {
    expectTrue(near(RouteApply::delta(0.0f, 1.0f, Combine::Absolute, 100, R), 0.f),       "source 0 -> base");
    expectTrue(near(RouteApply::delta(1.0f, 1.0f, Combine::Absolute, 100, R), R),         "source 1 -> +range");
    expectTrue(near(RouteApply::delta(0.5f, 1.0f, Combine::Absolute, 100, R), 0.5f * R),  "source 0.5 -> half");
}

CASE("scaleSource scales the depth, preserving Modulate center") {
    expectTrue(near(RouteApply::delta(1.0f, 0.5f, Combine::Modulate, 100, R), 0.5f * R), "half scale -> half depth");
    expectTrue(near(RouteApply::delta(1.0f, 1.0f, Combine::Modulate, 100, R), R),        "scale 1 == no scale");
    expectTrue(near(RouteApply::delta(1.0f, 0.0f, Combine::Modulate, 100, R), 0.f),      "scale 0 closes the VCA");
    expectTrue(near(RouteApply::delta(0.5f, 0.5f, Combine::Modulate, 100, R), 0.f),      "center still neutral under scale");
}

} // UNIT_TEST
