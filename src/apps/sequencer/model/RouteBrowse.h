#pragma once

#include "Routing.h"
#include "RouteParamKey.h"
#include "RouteFork.h"

// Param-aggregation read model for the tab editor (lens UI). A tab band maps to a
// fixed list of ParamKeys (the registry bands); matches() resolves whether a route
// targets a band param within a scope. Pure helpers: the 16-route loop that uses
// them lives in the page as a thin wrapper. See docs/plans/2026-06-03-009-routing-ui-nav-spec.md.

namespace RouteBrowse {

    enum class Band { Pitch, Clock, Prob, Global };

    // Fill keys[] with the band's ParamKeys; returns the count written.
    inline int bandParams(Band band, uint8_t *keys, int maxKeys) {
        static const uint8_t pitch[]  = { ParamKey::Scale, ParamKey::RootNote, ParamKey::Transpose, ParamKey::Octave };
        static const uint8_t clock[]  = { ParamKey::Divisor, ParamKey::ClockMultiplier };
        static const uint8_t prob[]   = { ParamKey::GateProbabilityBias, ParamKey::RetriggerProbabilityBias,
                                          ParamKey::LengthBias, ParamKey::NoteProbabilityBias };
        static const uint8_t global[] = { ParamKey::Tempo, ParamKey::Swing, ParamKey::CvRouteScan, ParamKey::CvRouteRoute };
        const uint8_t *src; int n;
        switch (band) {
        case Band::Pitch:  src = pitch;  n = 4; break;
        case Band::Clock:  src = clock;  n = 2; break;
        case Band::Prob:   src = prob;   n = 4; break;
        case Band::Global: src = global; n = 4; break;
        default:           return 0;
        }
        if (n > maxKeys) n = maxKeys;
        for (int i = 0; i < n; ++i) keys[i] = src[i];
        return n;
    }

    // Reverse of RouteFork::targetToParamKey for the band keys: the Routing::Target a
    // new route should carry to back paramKey. None for unknown keys.
    inline Routing::Target paramKeyToTarget(uint8_t key) {
        switch (key) {
        case ParamKey::Scale:                    return Routing::Target::Scale;
        case ParamKey::RootNote:                 return Routing::Target::RootNote;
        case ParamKey::Transpose:                return Routing::Target::Transpose;
        case ParamKey::Octave:                   return Routing::Target::Octave;
        case ParamKey::Divisor:                  return Routing::Target::Divisor;
        case ParamKey::ClockMultiplier:          return Routing::Target::ClockMult;
        case ParamKey::GateProbabilityBias:      return Routing::Target::GateProbabilityBias;
        case ParamKey::RetriggerProbabilityBias: return Routing::Target::RetriggerProbabilityBias;
        case ParamKey::LengthBias:               return Routing::Target::LengthBias;
        case ParamKey::NoteProbabilityBias:      return Routing::Target::NoteProbabilityBias;
        case ParamKey::Tempo:                    return Routing::Target::Tempo;
        case ParamKey::Swing:                    return Routing::Target::Swing;
        case ParamKey::CvRouteScan:              return Routing::Target::CvRouteScan;
        case ParamKey::CvRouteRoute:             return Routing::Target::CvRouteRoute;
        default:                                 return Routing::Target::None;
        }
    }

    // True when route is active, its target maps to paramKey, and its scope overlaps
    // trackMask. trackMask 0 = global scope (matches a project/global route only); a
    // non-zero mask = per-track scope (matches a per-track route whose tracks overlap).
    inline bool matches(const Routing::Route &route, uint8_t paramKey, uint8_t trackMask) {
        if (!route.active()) {
            return false;
        }
        if (RouteFork::targetToParamKey(route.target()) != paramKey) {
            return false;
        }
        bool perTrack = Routing::isPerTrackTarget(route.target());
        if (trackMask == 0) {
            return !perTrack;                       // global scope: only non-per-track routes
        }
        return perTrack && (route.tracks() & trackMask);
    }

} // namespace RouteBrowse
