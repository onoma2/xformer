#pragma once

#include "Routing.h"
#include "RouteParam.h"
#include "RouteParamKey.h"
#include "RouteShaper.h"
#include "RouteApply.h"
#include "Track.h"
#include "ParamTableNote.h"
#include "ParamTablePhaseFlux.h"
#include "ParamTableCurve.h"
#include "ParamTableGlobal.h"

// Apply-fork decision for the routing mod-matrix slice (plan 005, step 3).
//
// updateSinks() loops per route, then per masked track, before writeTarget(). The
// fork is decided here, per resolved (trackMode, target): a migrated Note/PhaseFlux
// per-track param is diverted to the override table (RouteShaper + RouteApply ->
// writeRouteOverride) and skips old writeTarget; everything else stays live.
//
// "Migrated" gates on the staged per-type tables: targetToParamKey() bridges the
// route's legacy Target to a shared ParamKey, and that key resolving in the mode's
// table means the mode reads it via a (to-be-)migrated getter. The table also gives
// the param's real range for the bias-free delta. Nudges/Phase have no legacy
// Target so they are unreachable here (excluded this slice).
//
// See docs/plans/2026-06-03-005-sliced-cutover-plan.md.

namespace RouteFork {

    // Bridge the route's legacy Routing::Target to a shared ParamKey number.
    // Covers every target that backs a Note or PhaseFlux table row; None otherwise
    // (None -> not migrated -> old writeTarget path).
    inline uint8_t targetToParamKey(Routing::Target target) {
        switch (target) {
        case Routing::Target::SlideTime:                 return ParamKey::SlideTime;
        case Routing::Target::Octave:                    return ParamKey::Octave;
        case Routing::Target::Transpose:                 return ParamKey::Transpose;
        case Routing::Target::Rotate:                    return ParamKey::Rotate;
        case Routing::Target::GateProbabilityBias:       return ParamKey::GateProbabilityBias;
        case Routing::Target::RetriggerProbabilityBias:  return ParamKey::RetriggerProbabilityBias;
        case Routing::Target::LengthBias:                return ParamKey::LengthBias;
        case Routing::Target::NoteProbabilityBias:       return ParamKey::NoteProbabilityBias;
        case Routing::Target::Scale:                     return ParamKey::Scale;
        case Routing::Target::RootNote:                  return ParamKey::RootNote;
        case Routing::Target::Divisor:                   return ParamKey::Divisor;
        case Routing::Target::ClockMult:                 return ParamKey::ClockMultiplier;
        case Routing::Target::RunMode:                   return ParamKey::RunMode;
        case Routing::Target::FirstStep:                 return ParamKey::FirstStep;
        case Routing::Target::LastStep:                  return ParamKey::LastStep;
        case Routing::Target::CurveRate:                 return ParamKey::CurveRate;
        // global (project) targets — no track dimension
        case Routing::Target::Tempo:                     return ParamKey::Tempo;
        case Routing::Target::Swing:                     return ParamKey::Swing;
        case Routing::Target::CvRouteScan:               return ParamKey::CvRouteScan;
        case Routing::Target::CvRouteRoute:              return ParamKey::CvRouteRoute;
        default:                                         return ParamKey::None;
        }
    }

    // The param's d=100% displacement: bipolar (min<0) uses the half-span, unipolar
    // the full span. Mirrors RouteApply's `range` contract.
    inline float inferRange(const RouteParam::Range &r) {
        return r.min < 0.f ? (r.max - r.min) * 0.5f : (r.max - r.min);
    }

    // The per-track value composition updateSinks() runs for a migrated target:
    // bias-free shape of the normalized source, then the Modulate delta over the
    // param's inferred range (depthPct = the per-track d gain, scaleSource = None).
    inline float computeDelta(float sourceValue, Routing::Shaper shaper, int depthPct,
                              const RouteParam::Range &range,
                              RouteApply::Combine combine = RouteApply::Combine::Modulate) {
        float h = RouteShaper::shape(shaper, sourceValue);
        return RouteApply::delta(h, 1.f, combine, depthPct, inferRange(range));
    }

    // True when (trackMode, target) is a migrated per-track param; fills paramKey
    // and the param's real range. Only Note + PhaseFlux migrate this slice.
    inline bool migrated(Track::TrackMode mode, Routing::Target target,
                         uint8_t &paramKey, RouteParam::Range &range) {
        const RouteParam::Table *table = nullptr;
        switch (mode) {
        case Track::TrackMode::Note:      table = &NoteParamTable::table(); break;
        case Track::TrackMode::PhaseFlux: table = &PhaseFluxParamTable::table(); break;
        case Track::TrackMode::Curve:     table = &CurveParamTable::table(); break;
        default:                          return false;
        }
        uint8_t key = targetToParamKey(target);
        const RouteParam::Row *row = table->find(key);
        if (!row) {
            return false;
        }
        paramKey = key;
        range = row->range;
        return true;
    }

    // Global (project) variant: Tempo/Swing/CVR have no track dimension. Gates on
    // the project-target range + the global param table, filling key + range the
    // same way migrated() does — the engine writes the override at GlobalTrack.
    inline bool migratedGlobal(Routing::Target target, uint8_t &paramKey, RouteParam::Range &range) {
        if (!Routing::isProjectTarget(target)) {
            return false;
        }
        uint8_t key = targetToParamKey(target);
        const RouteParam::Row *row = GlobalParamTable::table().find(key);
        if (!row) {
            return false;
        }
        paramKey = key;
        range = row->range;
        return true;
    }

} // namespace RouteFork
