#pragma once

#include "Routing.h"
#include "RouteParamKey.h"
#include "Scale.h"

#include "core/math/Math.h"

#include <cmath>

// Resolves a possibly-Default scale/rootNote to a concrete value at the point the
// project scale is known. A Default-follow track (sentinel -1) with a nonzero route
// delta modulates RELATIVE to the resolved project value, so it keeps tracking
// project-scale changes instead of detaching to an absolute index. Concrete values
// (>= 0) pass through untouched. See .tasks/review-fixes-tdd/PLAN.md 3.1.
namespace ScaleResolve {

    inline int resolveScale(int seqScale, int trackIndex, int projectScale) {
        if (seqScale >= 0) {
            return seqScale;
        }
        float delta;
        if (Routing::routeOverride(ParamKey::Scale, trackIndex, delta)) {
            return clamp(projectScale + int(std::lround(delta)), 0, Scale::Count - 1);
        }
        return projectScale;
    }

    inline int resolveRootNote(int seqRootNote, int trackIndex, int projectRootNote) {
        if (seqRootNote >= 0) {
            return seqRootNote;
        }
        float delta;
        if (Routing::routeOverride(ParamKey::RootNote, trackIndex, delta)) {
            return clamp(projectRootNote + int(std::lround(delta)), 0, 11);
        }
        return projectRootNote;
    }

} // namespace ScaleResolve
