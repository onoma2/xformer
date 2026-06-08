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
#include "ParamTableTuesday.h"
#include "ParamTableStochastic.h"
#include "ParamTableDiscreteMap.h"
#include "ParamTableIndexed.h"
#include "ParamTableMidiCv.h"
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

namespace RouteResolve {

    // Bridge the route's legacy Routing::Target to a shared ParamKey number.
    // Covers every target that backs a Note or PhaseFlux table row; None otherwise
    // (None -> not migrated -> old writeTarget path).
    inline uint8_t targetToParamKey(Routing::Target target) {
        switch (target) {
        case Routing::Target::SlideTime:                 return ParamKey::SlideTime;
        case Routing::Target::Octave:                    return ParamKey::Octave;
        case Routing::Target::Transpose:                 return ParamKey::Transpose;
        case Routing::Target::Rotate:                    return ParamKey::Rotate;
        case Routing::Target::Offset:                    return ParamKey::Offset;
        case Routing::Target::ShapeProbabilityBias:      return ParamKey::ShapeProbabilityBias;
        case Routing::Target::GateProbabilityBias:       return ParamKey::GateProbabilityBias;
        case Routing::Target::RetriggerProbabilityBias:  return ParamKey::RetriggerProbabilityBias;
        case Routing::Target::LengthBias:                return ParamKey::LengthBias;
        case Routing::Target::NoteProbabilityBias:       return ParamKey::NoteProbabilityBias;
        case Routing::Target::Scale:                     return ParamKey::Scale;
        case Routing::Target::RootNote:                  return ParamKey::RootNote;
        case Routing::Target::Divisor:                   return ParamKey::Divisor;
        case Routing::Target::ClockMult:                 return ParamKey::ClockMultiplier;
        case Routing::Target::Run:                       return ParamKey::Run;
        case Routing::Target::Reset:                     return ParamKey::Reset;
        case Routing::Target::CvOutputRotate:            return ParamKey::CvOutputRotate;
        case Routing::Target::GateOutputRotate:          return ParamKey::GateOutputRotate;
        case Routing::Target::RunMode:                   return ParamKey::RunMode;
        case Routing::Target::FirstStep:                 return ParamKey::FirstStep;
        case Routing::Target::LastStep:                  return ParamKey::LastStep;
        case Routing::Target::CurveRate:                 return ParamKey::CurveRate;
        case Routing::Target::WavefolderFold:            return ParamKey::WavefolderFold;
        case Routing::Target::WavefolderGain:            return ParamKey::WavefolderGain;
        case Routing::Target::DjFilter:                  return ParamKey::DjFilter;
        case Routing::Target::ChaosAmount:               return ParamKey::ChaosAmount;
        case Routing::Target::ChaosRate:                 return ParamKey::ChaosRate;
        case Routing::Target::ChaosParam1:               return ParamKey::ChaosParam1;
        case Routing::Target::ChaosParam2:               return ParamKey::ChaosParam2;
        case Routing::Target::Algorithm:                 return ParamKey::Algorithm;
        case Routing::Target::Flow:                      return ParamKey::Flow;
        case Routing::Target::Ornament:                  return ParamKey::Ornament;
        case Routing::Target::Power:                     return ParamKey::Power;
        case Routing::Target::Glide:                     return ParamKey::Glide;
        case Routing::Target::Trill:                     return ParamKey::Trill;
        case Routing::Target::StepTrill:                 return ParamKey::StepTrill;
        case Routing::Target::GateLength:                return ParamKey::GateLength;
        case Routing::Target::GateOffset:                return ParamKey::GateOffset;
        // Stochastic signature block (legacy Target names diverge from ParamKey)
        case Routing::Target::StochasticComplexity:      return ParamKey::Complexity;
        case Routing::Target::StochasticVariation:       return ParamKey::Variation;
        case Routing::Target::StochasticRest:            return ParamKey::Rest;
        case Routing::Target::StochasticSlide:           return ParamKey::Slide;
        case Routing::Target::StochasticBurst:           return ParamKey::Burst;
        case Routing::Target::StochasticSleep:           return ParamKey::Sleep;
        case Routing::Target::StochasticMutate:          return ParamKey::Mutate;
        case Routing::Target::StochasticJump:            return ParamKey::Jump;
        case Routing::Target::StochasticContour:         return ParamKey::Contour;
        case Routing::Target::StochasticRange:           return ParamKey::Range;
        case Routing::Target::StochasticMarblesBias:     return ParamKey::MarblesBias;
        case Routing::Target::StochasticMarblesSpread:   return ParamKey::MarblesSpread;
        case Routing::Target::StochasticBurstCount:      return ParamKey::BurstCount;
        case Routing::Target::StochasticBurstRate:       return ParamKey::BurstRate;
        case Routing::Target::StochasticMask:            return ParamKey::MaskRhythm;
        case Routing::Target::StochasticTilt:            return ParamKey::TiltRhythm;
        case Routing::Target::StochasticGateLength:      return ParamKey::StochasticGateLength;
        case Routing::Target::StochasticPatienceRhythm:  return ParamKey::PatienceRhythm;
        case Routing::Target::StochasticNoteDuration:    return ParamKey::NoteDuration;
        case Routing::Target::StochasticFeel:            return ParamKey::Feel;
        // DiscreteMap inlets + signature block (Octave/Transpose/SlideTime/Offset/
        // Divisor/ClockMult/Scale/RootNote bridged above via the shared keys)
        case Routing::Target::DiscreteMapInput:          return ParamKey::DiscreteMapInput;
        case Routing::Target::DiscreteMapScanner:        return ParamKey::DiscreteMapScanner;
        case Routing::Target::DiscreteMapRangeHigh:      return ParamKey::DiscreteMapRangeHigh;
        case Routing::Target::DiscreteMapRangeLow:       return ParamKey::DiscreteMapRangeLow;
        // Indexed inlets (Octave/Transpose/SlideTime/Divisor/ClockMult/Scale/RootNote/
        // FirstStep/RunMode bridged above via the shared keys)
        case Routing::Target::IndexedA:                  return ParamKey::IndexedA;
        case Routing::Target::IndexedB:                  return ParamKey::IndexedB;
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

    // Bus lanes are base-0 sinks on the unified model: one signed depthPct + combine,
    // no min/max window. Returns volts to sum onto the lane (read clamps to +/-5V).
    // Modulate = bipolar around 0V, Absolute = sweep from 0V; depth 100 = full rail.
    inline float busDelta(float sourceValue, Routing::Shaper shaper, int depthPct,
                          RouteApply::Combine combine) {
        float h = RouteShaper::shape(shaper, sourceValue);
        return RouteApply::delta(h, 1.f, combine, depthPct, 5.f);
    }

    // The per-type param table for a track mode, or nullptr for modes with no
    // engine table (default/None). One source of truth for the mode->table map.
    inline const RouteParam::Table *tableForMode(Track::TrackMode mode) {
        switch (mode) {
        case Track::TrackMode::Note:      return &NoteParamTable::table();
        case Track::TrackMode::PhaseFlux: return &PhaseFluxParamTable::table();
        case Track::TrackMode::Curve:     return &CurveParamTable::table();
        case Track::TrackMode::Tuesday:   return &TuesdayParamTable::table();
        case Track::TrackMode::Stochastic: return &StochasticParamTable::table();
        case Track::TrackMode::DiscreteMap: return &DiscreteMapParamTable::table();
        case Track::TrackMode::Indexed:   return &IndexedParamTable::table();
        case Track::TrackMode::MidiCv:    return &MidiCvParamTable::table();
        default:                          return nullptr;
        }
    }

    // True when (trackMode, target) is a per-track override param; fills paramKey
    // and the param's real range. The override path covers every engine's sequence
    // params (Note/PhaseFlux/Curve/Tuesday/Stochastic/DiscreteMap/Indexed/MidiCv).
    inline bool overrideParam(Track::TrackMode mode, Routing::Target target,
                         uint8_t &paramKey, RouteParam::Range &range) {
        const RouteParam::Table *table = tableForMode(mode);
        if (!table) {
            return false;
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
    // same way overrideParam() does — the engine writes the override at GlobalTrack.
    inline bool overrideParamGlobal(Routing::Target target, uint8_t &paramKey, RouteParam::Range &range) {
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

} // namespace RouteResolve
