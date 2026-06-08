#pragma once

#include "Routing.h"
#include "RouteParamKey.h"
#include "RouteFork.h"

// Param-aggregation read model for the tab editor (lens UI). A tab band maps to a
// fixed list of ParamKeys (the registry bands); matches() resolves whether a route
// targets a band param within a scope. Pure helpers: the 16-route loop that uses
// them lives in the page as a thin wrapper. See docs/plans/2026-06-03-009-routing-ui-nav-spec.md.

namespace RouteBrowse {

    enum class Band { Pitch, Clock, Global };

    // Fill keys[] with the band's ParamKeys; returns the count written. The fixed bands
    // are the cross-engine params; Prob/Bias is engine-specific and folds into the engine
    // pages (enginePageParams). SlideTime + Rotate ride the Pitch band (no separate tab).
    inline int bandParams(Band band, uint8_t *keys, int maxKeys) {
        static const uint8_t pitch[]  = { ParamKey::Scale, ParamKey::RootNote, ParamKey::Transpose,
                                          ParamKey::Octave, ParamKey::SlideTime, ParamKey::Rotate };
        static const uint8_t clock[]  = { ParamKey::Divisor, ParamKey::ClockMultiplier,
                                          ParamKey::Run, ParamKey::Reset };
        static const uint8_t global[] = { ParamKey::Tempo, ParamKey::Swing, ParamKey::CvRouteScan,
                                          ParamKey::CvRouteRoute, ParamKey::CvOutputRotate, ParamKey::GateOutputRotate };
        const uint8_t *src; int n;
        switch (band) {
        case Band::Pitch:     src = pitch;  n = 6; break;
        case Band::Clock:     src = clock;  n = 4; break;
        case Band::Global:    src = global; n = 6; break;
        default:              return 0;
        }
        if (n > maxKeys) n = maxKeys;
        for (int i = 0; i < n; ++i) keys[i] = src[i];
        return n;
    }

    // Fill keys[] with an engine page's routable ParamKeys: the track mode's per-type
    // table rows minus the keys the fixed Pitch/Clock/SlideTime bands already own
    // (Global keys never appear in engine tables). Preserves table order. Returns count.
    inline int enginePageParams(Track::TrackMode mode, uint8_t *keys, int maxKeys) {
        const RouteParam::Table *table = RouteFork::tableForMode(mode);
        if (!table) {
            return 0;
        }
        static const uint8_t shared[] = {
            ParamKey::Scale, ParamKey::RootNote, ParamKey::Transpose, ParamKey::Octave,
            ParamKey::Divisor, ParamKey::ClockMultiplier, ParamKey::SlideTime, ParamKey::Rotate,
        };
        int n = 0;
        for (size_t i = 0; i < table->count() && n < maxKeys; ++i) {
            uint8_t key = table->rows()[i].key;
            bool isShared = false;
            for (uint8_t s : shared) {
                if (s == key) { isShared = true; break; }
            }
            if (!isShared) {
                keys[n++] = key;
            }
        }
        return n;
    }

    // Compact engine-page label for keys whose full table name overruns the row gutter,
    // reusing the engine's own edit-page wording (Algo, …). nullptr = use the full name.
    inline const char *shortLabel(uint8_t key) {
        switch (key) {
        case ParamKey::Algorithm:                return "Algo";
        case ParamKey::CurveRate:                return "C.Rate";
        case ParamKey::GateProbabilityBias:      return "Gate Bias";
        case ParamKey::RetriggerProbabilityBias: return "Rtg Bias";
        case ParamKey::LengthBias:               return "Len Bias";
        case ParamKey::NoteProbabilityBias:      return "Note Bias";
        case ParamKey::ShapeProbabilityBias:     return "Shp Bias";
        case ParamKey::FirstStep:                return "First St";
        case ParamKey::LastStep:                 return "Last St";
        case ParamKey::StepTrill:                return "Stp Trill";
        case ParamKey::GateLength:               return "Gate Len";
        case ParamKey::GateOffset:               return "Gate Ofs";
        case ParamKey::Run:                      return "Run";
        case ParamKey::Reset:                    return "Reset";
        case ParamKey::CvOutputRotate:           return "CV Rot";
        case ParamKey::GateOutputRotate:         return "Gate Rot";
        default:                                 return nullptr;
        }
    }

    // Reverse of RouteFork::targetToParamKey: the Routing::Target a new route should
    // carry to back paramKey. Inverts the forward map by scan so every band AND engine
    // key resolves (the engine pages route the full per-type tables). None for unknown.
    inline Routing::Target paramKeyToTarget(uint8_t key) {
        if (key == 0) {
            return Routing::Target::None;
        }
        for (int t = 0; t < int(Routing::Target::Last); ++t) {
            auto target = Routing::Target(t);
            if (RouteFork::targetToParamKey(target) == key) {
                return target;
            }
        }
        return Routing::Target::None;
    }

    // Fill out[] with the sources the tab-editor source picker offers for a target:
    // every Source in enum order except the bus that would self-route the target. MIDI
    // is a plain value here; its fields are configured on the MIDI page. Returns the
    // count written (<= maxOut).
    inline int sourceList(Routing::Target target, Routing::Source *out, int maxOut) {
        int n = 0;
        for (int s = 0; s < int(Routing::Source::Last) && n < maxOut; ++s) {
            auto source = Routing::Source(s);
            if (Routing::isBusSelfRoute(source, target)) {
                continue;
            }
            out[n++] = source;
        }
        return n;
    }

    // Fill out[] with the indices of every active route whose source is MIDI, in
    // ascending order — the row set for the MIDI page. Returns the count (<= maxOut).
    inline int midiRouteList(const Routing &routing, uint8_t *out, int maxOut) {
        int n = 0;
        for (int i = 0; i < CONFIG_ROUTE_COUNT && n < maxOut; ++i) {
            const Routing::Route &route = routing.route(i);
            if (route.active() && route.source() == Routing::Source::Midi) {
                out[n++] = uint8_t(i);
            }
        }
        return n;
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
