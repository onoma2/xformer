#include "UnitTest.h"

#include "apps/sequencer/model/RouteFork.h"
#include "apps/sequencer/model/RouteParamKey.h"
#include "apps/sequencer/model/RouteApply.h"
#include "apps/sequencer/model/RouteShaper.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/Track.h"

#include <cmath>

// Step 3 of the apply-fork slice: the per-(trackMode, target) fork decision and
// range inference that updateSinks() uses to divert migrated Note/PhaseFlux
// per-track params to the override table. Pure helpers only; the updateSinks
// wiring is a thin composition of these + the already-tested RouteShaper/RouteApply.
//
// migrated(mode, target) gates on the staged per-type tables: a target maps to a
// paramKey (targetToParamKey), and that key resolving in the mode's table means
// "this mode reads it via a migrated getter." Nudges/Phase have no legacy Target,
// so they are unreachable here and excluded this slice.

namespace {

using RouteApply::Combine;
using Mode = Track::TrackMode;

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

} // namespace

UNIT_TEST("RouteFork") {

CASE("inferRange: bipolar uses half-span, unipolar uses full span") {
    expectTrue(near(RouteFork::inferRange({ -60.f, 60.f }), 60.f), "Transpose +/-60 -> 60");
    expectTrue(near(RouteFork::inferRange({ -10.f, 10.f }), 10.f), "Octave +/-10 -> 10");
    expectTrue(near(RouteFork::inferRange({ 0.f, 100.f }), 100.f), "SlideTime 0..100 -> 100");
    expectTrue(near(RouteFork::inferRange({ 0.f, 23.f }), 23.f), "Scale 0..23 -> 23");
    expectTrue(near(RouteFork::inferRange({ 0.f, 1.f }), 1.f), "Phase 0..1 -> 1");
    expectTrue(near(RouteFork::inferRange({ -64.f, 64.f }), 64.f), "nudge +/-64 -> 64");
}

CASE("targetToParamKey bridges migrated targets, None for the rest") {
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::Scale)), int(ParamKey::Scale), "Scale");
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::Transpose)), int(ParamKey::Transpose), "Transpose");
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::ClockMult)), int(ParamKey::ClockMultiplier), "ClockMult");
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::FirstStep)), int(ParamKey::FirstStep), "FirstStep");
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::Tempo)), int(ParamKey::Tempo), "Tempo (global) mapped");
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::CvOutputRotate)), int(ParamKey::None), "CvOutputRotate unmapped");
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::Reset)), int(ParamKey::None), "Reset unmapped");
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::Run)), int(ParamKey::None), "Run unmapped");
}

CASE("Note migrates all 15 of its per-track params") {
    uint8_t key; RouteParam::Range range;
    for (auto t : { Routing::Target::SlideTime, Routing::Target::Octave, Routing::Target::Transpose,
                    Routing::Target::Rotate, Routing::Target::GateProbabilityBias,
                    Routing::Target::RetriggerProbabilityBias, Routing::Target::LengthBias,
                    Routing::Target::NoteProbabilityBias, Routing::Target::Scale, Routing::Target::RootNote,
                    Routing::Target::Divisor, Routing::Target::ClockMult, Routing::Target::RunMode,
                    Routing::Target::FirstStep, Routing::Target::LastStep }) {
        expectTrue(RouteFork::migrated(Mode::Note, t, key, range), "Note target migrated");
    }
}

CASE("Note migration returns the param key and real range") {
    uint8_t key = 0; RouteParam::Range range{};
    expectTrue(RouteFork::migrated(Mode::Note, Routing::Target::Scale, key, range), "migrated");
    expectEqual(int(key), int(ParamKey::Scale), "key");
    expectTrue(near(range.min, 0.f) && near(range.max, 23.f), "Scale range 0..23");

    expectTrue(RouteFork::migrated(Mode::Note, Routing::Target::Transpose, key, range), "migrated");
    expectTrue(near(range.min, -60.f) && near(range.max, 60.f), "Transpose range -60..60");
}

CASE("PhaseFlux migrates only its Target-backed params") {
    uint8_t key; RouteParam::Range range;
    for (auto t : { Routing::Target::Scale, Routing::Target::RootNote, Routing::Target::Divisor,
                    Routing::Target::ClockMult, Routing::Target::Octave, Routing::Target::Transpose,
                    Routing::Target::SlideTime }) {
        expectTrue(RouteFork::migrated(Mode::PhaseFlux, t, key, range), "PhaseFlux Target-backed migrated");
    }
    // Note-only params are not in the PhaseFlux table -> not migrated for PhaseFlux.
    expectFalse(RouteFork::migrated(Mode::PhaseFlux, Routing::Target::Rotate, key, range), "Rotate not PF");
    expectFalse(RouteFork::migrated(Mode::PhaseFlux, Routing::Target::RunMode, key, range), "RunMode not PF");
    expectFalse(RouteFork::migrated(Mode::PhaseFlux, Routing::Target::FirstStep, key, range), "FirstStep not PF");
}

CASE("Curve migrates its sequence-level and track-level Target-backed params") {
    uint8_t key = 0; RouteParam::Range range{};
    expectTrue(RouteFork::migrated(Mode::Curve, Routing::Target::Divisor, key, range), "Curve Divisor migrated");
    expectEqual(int(key), int(ParamKey::Divisor), "Divisor key");
    expectTrue(near(range.min, 1.f) && near(range.max, 768.f), "Divisor range 1..768");

    expectTrue(RouteFork::migrated(Mode::Curve, Routing::Target::CurveRate, key, range), "Curve CurveRate migrated");
    expectEqual(int(key), int(ParamKey::CurveRate), "CurveRate key");
    expectTrue(near(range.min, 0.f) && near(range.max, 400.f), "CurveRate range 0..400");

    // A target with no row in the Curve table -> not migrated for Curve.
    expectFalse(RouteFork::migrated(Mode::Curve, Routing::Target::IndexedA, key, range), "IndexedA not Curve");
}

CASE("modes not wired into migrated() stay on the old path") {
    uint8_t key; RouteParam::Range range;
    // MidiCv + Teletype have no migrated() case, so every target stays legacy
    // even where a param table owns a matching row.
    for (auto m : { Mode::MidiCv, Mode::Teletype }) {
        expectFalse(RouteFork::migrated(m, Routing::Target::Scale, key, range), "non-migrated mode");
        expectFalse(RouteFork::migrated(m, Routing::Target::Transpose, key, range), "non-migrated mode");
    }
}

CASE("unmapped + special targets are never migrated") {
    uint8_t key; RouteParam::Range range;
    expectFalse(RouteFork::migrated(Mode::Note, Routing::Target::Tempo, key, range), "Tempo");
    expectFalse(RouteFork::migrated(Mode::Note, Routing::Target::CvOutputRotate, key, range), "CvOutputRotate");
    expectFalse(RouteFork::migrated(Mode::Note, Routing::Target::Reset, key, range), "Reset");
    expectFalse(RouteFork::migrated(Mode::Note, Routing::Target::Run, key, range), "Run");
}

// computeDelta is the exact source->delta composition updateSinks() runs per
// migrated track (shape -> Modulate delta over inferRange). Pinning it here
// covers the live engine seam in sim; only the loop/skip wiring is left to hardware.
CASE("computeDelta: source-center is neutral for any shaper/depth (base-anchored)") {
    RouteParam::Range transpose{ -60.f, 60.f };
    expectTrue(near(RouteFork::computeDelta(0.5f, Routing::Shaper::None, 100, transpose), 0.f), "None center");
    expectTrue(near(RouteFork::computeDelta(0.5f, Routing::Shaper::TriangleFold, 100, transpose), 0.f), "TriFold center");
    expectTrue(near(RouteFork::computeDelta(0.5f, Routing::Shaper::None, 37, transpose), 0.f), "any depth");
}

CASE("computeDelta: full swing and depth scaling over the inferred range") {
    RouteParam::Range transpose{ -60.f, 60.f };   // bipolar -> 60
    expectTrue(near(RouteFork::computeDelta(1.0f, Routing::Shaper::None, 100, transpose), 60.f),  "src1 -> +range");
    expectTrue(near(RouteFork::computeDelta(0.0f, Routing::Shaper::None, 100, transpose), -60.f), "src0 -> -range");
    expectTrue(near(RouteFork::computeDelta(1.0f, Routing::Shaper::None, 50, transpose), 30.f),   "depth 50 -> half");
    RouteParam::Range scale{ 0.f, 23.f };          // unipolar -> 23
    expectTrue(near(RouteFork::computeDelta(1.0f, Routing::Shaper::None, 100, scale), 23.f),      "unipolar full");
}

CASE("computeDelta: TriangleFold reshapes the source before Modulate") {
    RouteParam::Range transpose{ -60.f, 60.f };
    expectTrue(near(RouteFork::computeDelta(0.75f, Routing::Shaper::TriangleFold, 100, transpose), 60.f),  "0.75 -> +full");
    expectTrue(near(RouteFork::computeDelta(0.25f, Routing::Shaper::TriangleFold, 100, transpose), -60.f), "0.25 -> -full");
}

CASE("computeDelta: combine Absolute sweeps from base (raw source), Modulate centers") {
    RouteParam::Range transpose{ -60.f, 60.f };   // inferRange -> 60
    // Absolute: u = raw source -> source 0 = base (delta 0), 0.5 = half, 1 = full
    expectTrue(near(RouteFork::computeDelta(0.0f, Routing::Shaper::None, 100, transpose, Combine::Absolute), 0.f),  "abs src0 -> base");
    expectTrue(near(RouteFork::computeDelta(0.5f, Routing::Shaper::None, 100, transpose, Combine::Absolute), 30.f), "abs src0.5 -> half");
    expectTrue(near(RouteFork::computeDelta(1.0f, Routing::Shaper::None, 100, transpose, Combine::Absolute), 60.f), "abs src1 -> full");
    // Modulate (default) centers: 0.5 neutral, 0 = -full
    expectTrue(near(RouteFork::computeDelta(0.5f, Routing::Shaper::None, 100, transpose, Combine::Modulate), 0.f),   "mod src0.5 -> 0");
    expectTrue(near(RouteFork::computeDelta(0.0f, Routing::Shaper::None, 100, transpose, Combine::Modulate), -60.f), "mod src0 -> -full");
    // default arg is Modulate (back-compat with the 4-arg call sites)
    expectTrue(near(RouteFork::computeDelta(0.5f, Routing::Shaper::None, 100, transpose), 0.f), "default = Modulate");
}

CASE("computeDelta: range comes from the migrated row") {
    uint8_t key; RouteParam::Range range;
    RouteFork::migrated(Mode::Note, Routing::Target::Transpose, key, range);
    expectTrue(near(RouteFork::computeDelta(1.0f, Routing::Shaper::None, 100, range), 60.f), "Transpose row -> 60");
}

CASE("migratedGlobal: project targets map to key + global-table range") {
    uint8_t key; RouteParam::Range range;
    expectTrue(RouteFork::migratedGlobal(Routing::Target::Tempo, key, range), "Tempo migrates");
    expectEqual(int(key), int(ParamKey::Tempo), "Tempo key");
    expectTrue(near(range.min, 1.f) && near(range.max, 1000.f), "Tempo range 1..1000");

    expectTrue(RouteFork::migratedGlobal(Routing::Target::Swing, key, range), "Swing migrates");
    expectEqual(int(key), int(ParamKey::Swing), "Swing key");
    expectTrue(near(range.min, 50.f) && near(range.max, 75.f), "Swing range 50..75");

    expectTrue(RouteFork::migratedGlobal(Routing::Target::CvRouteScan, key, range), "CVR Scan migrates");
    expectEqual(int(key), int(ParamKey::CvRouteScan), "CVR Scan key");
    expectTrue(RouteFork::migratedGlobal(Routing::Target::CvRouteRoute, key, range), "CVR Route migrates");
    expectEqual(int(key), int(ParamKey::CvRouteRoute), "CVR Route key");
}

CASE("migratedGlobal: non-project targets do not migrate here") {
    uint8_t key; RouteParam::Range range;
    expectFalse(RouteFork::migratedGlobal(Routing::Target::Transpose, key, range), "per-track not global");
    expectFalse(RouteFork::migratedGlobal(Routing::Target::Mute, key, range), "playstate not global");
    expectFalse(RouteFork::migratedGlobal(Routing::Target::Play, key, range), "engine trigger not global");
}

} // UNIT_TEST
