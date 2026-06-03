#pragma once

#include "Routing.h"

#include <cmath>

// Bias-free shaper stage for the routing mod-matrix apply path. Maps a normalized
// source [0,1] to a shaped [0,1] that feeds RouteApply as `h`. Pure: no engine
// state, no neighbor/route coupling.
//
// The new model drops the per-track bias term (`d` subsumes it), so the shapers
// here are fed a fixed-center source -- TriangleFold's fold center is locked at
// 0.5. That keeps h=0.5 -> 0.5, so RouteApply Modulate stays neutral at
// source-center. The live RoutingEngine TriangleFold shifts center by bias*0.5;
// that input does not exist in this path.
//
// Only None + TriangleFold are ported in this slice. The remaining (stateful)
// shapers land with the apply-fork expansion; until then any other shaper is
// treated as identity here.
//
// See docs/plans/2026-06-03-005-sliced-cutover-plan.md (shaper stage) and
// model/RouteApply.h (the delta pipeline this feeds).

namespace RouteShaper {

    // Fold around a fixed 0.5 center with a ±0.5 amplitude triangle (center-preserving).
    inline float triangleFold(float h) {
        float x = 2.f * (h - 0.5f);
        float folded = (x > 0.f) ? 1.f - 2.f * std::fabs(x - 0.5f)
                                 : -1.f + 2.f * std::fabs(x + 0.5f);
        return clamp(0.5f + 0.5f * folded, 0.f, 1.f);
    }

    // Shaper stage dispatch: None/TriangleFold ported; others identity this slice.
    inline float shape(Routing::Shaper shaper, float h) {
        switch (shaper) {
        case Routing::Shaper::TriangleFold:
            return triangleFold(h);
        default:
            return h;
        }
    }

} // namespace RouteShaper
