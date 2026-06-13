#include "UnitTest.h"

#include "apps/sequencer/model/RouteResolve.h"
#include "apps/sequencer/model/RouteBrowse.h"
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
// overrideParam(mode, target) gates on the staged per-type tables: a target maps to a
// paramKey (targetToParamKey), and that key resolving in the mode's table means
// "this mode reads it via a migrated getter." Nudges/Phase have no legacy Target,
// so they are unreachable here and excluded this slice.

namespace {

using RouteApply::Combine;
using Mode = Track::TrackMode;

bool near(float a, float b) { return std::fabs(a - b) < 1e-4f; }

} // namespace

UNIT_TEST("RouteResolve") {

CASE("inferRange: bipolar uses half-span, unipolar uses full span") {
    expectTrue(near(RouteResolve::inferRange({ -60.f, 60.f }), 60.f), "Transpose +/-60 -> 60");
    expectTrue(near(RouteResolve::inferRange({ -10.f, 10.f }), 10.f), "Octave +/-10 -> 10");
    expectTrue(near(RouteResolve::inferRange({ 0.f, 100.f }), 100.f), "SlideTime 0..100 -> 100");
    expectTrue(near(RouteResolve::inferRange({ 0.f, 23.f }), 23.f), "Scale 0..23 -> 23");
    expectTrue(near(RouteResolve::inferRange({ 0.f, 1.f }), 1.f), "Phase 0..1 -> 1");
    expectTrue(near(RouteResolve::inferRange({ -64.f, 64.f }), 64.f), "nudge +/-64 -> 64");
}

CASE("targetToParamKey bridges migrated targets + the folded output/shell targets") {
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::Scale)), int(ParamKey::Scale), "Scale");
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::Transpose)), int(ParamKey::Transpose), "Transpose");
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::ClockMult)), int(ParamKey::ClockMultiplier), "ClockMult");
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::FirstStep)), int(ParamKey::FirstStep), "FirstStep");
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::Tempo)), int(ParamKey::Tempo), "Tempo (global) mapped");
    // Output/shell targets now bridge so the matrix can surface them (Clock/Global bands).
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::CvOutputRotate)), int(ParamKey::CvOutputRotate), "CvOutputRotate mapped");
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::GateOutputRotate)), int(ParamKey::GateOutputRotate), "GateOutputRotate mapped");
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::Reset)), int(ParamKey::Reset), "Reset mapped");
    expectEqual(int(RouteResolve::targetToParamKey(Routing::Target::Run)), int(ParamKey::Run), "Run mapped");
}

CASE("Note migrates all 15 of its per-track params") {
    uint8_t key; RouteParam::Range range;
    for (auto t : { Routing::Target::SlideTime, Routing::Target::Octave, Routing::Target::Transpose,
                    Routing::Target::Rotate, Routing::Target::GateProbabilityBias,
                    Routing::Target::RetriggerProbabilityBias, Routing::Target::LengthBias,
                    Routing::Target::NoteProbabilityBias, Routing::Target::Scale, Routing::Target::RootNote,
                    Routing::Target::Divisor, Routing::Target::ClockMult, Routing::Target::RunMode,
                    Routing::Target::FirstStep, Routing::Target::LastStep }) {
        expectTrue(RouteResolve::overrideParam(Mode::Note, t, key, range), "Note target migrated");
    }
}

CASE("Note migration returns the param key and real range") {
    uint8_t key = 0; RouteParam::Range range{};
    expectTrue(RouteResolve::overrideParam(Mode::Note, Routing::Target::Scale, key, range), "migrated");
    expectEqual(int(key), int(ParamKey::Scale), "key");
    expectTrue(near(range.min, 0.f) && near(range.max, 23.f), "Scale range 0..23");

    expectTrue(RouteResolve::overrideParam(Mode::Note, Routing::Target::Transpose, key, range), "migrated");
    expectTrue(near(range.min, -60.f) && near(range.max, 60.f), "Transpose range -60..60");
}

CASE("PhaseFlux migrates only its Target-backed params") {
    uint8_t key; RouteParam::Range range;
    for (auto t : { Routing::Target::Scale, Routing::Target::RootNote, Routing::Target::Divisor,
                    Routing::Target::ClockMult, Routing::Target::Octave, Routing::Target::Transpose,
                    Routing::Target::SlideTime }) {
        expectTrue(RouteResolve::overrideParam(Mode::PhaseFlux, t, key, range), "PhaseFlux Target-backed migrated");
    }
    // Note-only params are not in the PhaseFlux table -> not migrated for PhaseFlux.
    expectFalse(RouteResolve::overrideParam(Mode::PhaseFlux, Routing::Target::Rotate, key, range), "Rotate not PF");
    expectFalse(RouteResolve::overrideParam(Mode::PhaseFlux, Routing::Target::RunMode, key, range), "RunMode not PF");
    expectFalse(RouteResolve::overrideParam(Mode::PhaseFlux, Routing::Target::FirstStep, key, range), "FirstStep not PF");
}

CASE("Curve migrates its sequence-level and track-level Target-backed params") {
    uint8_t key = 0; RouteParam::Range range{};
    expectTrue(RouteResolve::overrideParam(Mode::Curve, Routing::Target::Divisor, key, range), "Curve Divisor migrated");
    expectEqual(int(key), int(ParamKey::Divisor), "Divisor key");
    expectTrue(near(range.min, 1.f) && near(range.max, 768.f), "Divisor range 1..768");

    expectTrue(RouteResolve::overrideParam(Mode::Curve, Routing::Target::CurveRate, key, range), "Curve CurveRate migrated");
    expectEqual(int(key), int(ParamKey::CurveRate), "CurveRate key");
    expectTrue(near(range.min, 0.f) && near(range.max, 400.f), "CurveRate range 0..400");

    // A target with no row in the Curve table -> not migrated for Curve.
    expectFalse(RouteResolve::overrideParam(Mode::Curve, Routing::Target::IndexedA, key, range), "IndexedA not Curve");
}

CASE("Phase routes for both Curve and PhaseFlux (globalPhase, shared key)") {
    uint8_t key = 0; RouteParam::Range range{};
    for (auto mode : { Mode::Curve, Mode::PhaseFlux }) {
        expectTrue(RouteResolve::overrideParam(mode, Routing::Target::Phase, key, range), "Phase migrated");
        expectEqual(int(key), int(ParamKey::Phase), "Phase key");
        expectTrue(near(range.min, 0.f) && near(range.max, 1.f), "Phase range 0..1");
    }
    expectEqual(int(RouteBrowse::paramKeyToTarget(ParamKey::Phase)), int(Routing::Target::Phase), "Phase reverse-resolves");
}

CASE("modes not wired into overrideParam() stay on the old path") {
    uint8_t key; RouteParam::Range range;
    // Teletype has no overrideParam() case (no routable params), so every target
    // stays legacy. All other engines are now migrated.
    expectFalse(RouteResolve::overrideParam(Mode::Teletype, Routing::Target::Scale, key, range), "Teletype non-migrated");
    expectFalse(RouteResolve::overrideParam(Mode::Teletype, Routing::Target::Transpose, key, range), "Teletype non-migrated");
}

CASE("unmapped + special targets are never migrated") {
    uint8_t key; RouteParam::Range range;
    expectFalse(RouteResolve::overrideParam(Mode::Note, Routing::Target::Tempo, key, range), "Tempo");
    expectFalse(RouteResolve::overrideParam(Mode::Note, Routing::Target::CvOutputRotate, key, range), "CvOutputRotate");
    expectFalse(RouteResolve::overrideParam(Mode::Note, Routing::Target::Reset, key, range), "Reset");
    expectFalse(RouteResolve::overrideParam(Mode::Note, Routing::Target::Run, key, range), "Run");
}

// computeDelta is the exact source->delta composition updateSinks() runs per
// migrated track (shape -> Modulate delta over inferRange). Pinning it here
// covers the live engine seam in sim; only the loop/skip wiring is left to hardware.
CASE("computeDelta: source-center is neutral for any shaper/depth (base-anchored)") {
    RouteParam::Range transpose{ -60.f, 60.f };
    expectTrue(near(RouteResolve::computeDelta(0.5f, Routing::Shaper::None, 100, transpose), 0.f), "None center");
    expectTrue(near(RouteResolve::computeDelta(0.5f, Routing::Shaper::TriangleFold, 100, transpose), 0.f), "TriFold center");
    expectTrue(near(RouteResolve::computeDelta(0.5f, Routing::Shaper::None, 37, transpose), 0.f), "any depth");
}

CASE("computeDelta: full swing and depth scaling over the inferred range") {
    RouteParam::Range transpose{ -60.f, 60.f };   // bipolar -> 60
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 100, transpose), 60.f),  "src1 -> +range");
    expectTrue(near(RouteResolve::computeDelta(0.0f, Routing::Shaper::None, 100, transpose), -60.f), "src0 -> -range");
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 50, transpose), 30.f),   "depth 50 -> half");
    RouteParam::Range scale{ 0.f, 23.f };          // unipolar -> 23
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 100, scale), 23.f),      "unipolar full");
}

CASE("computeDelta: TriangleFold reshapes the source before Modulate") {
    RouteParam::Range transpose{ -60.f, 60.f };
    expectTrue(near(RouteResolve::computeDelta(0.75f, Routing::Shaper::TriangleFold, 100, transpose), 60.f),  "0.75 -> +full");
    expectTrue(near(RouteResolve::computeDelta(0.25f, Routing::Shaper::TriangleFold, 100, transpose), -60.f), "0.25 -> -full");
}

CASE("computeDelta: combine Absolute sweeps from base (raw source), Modulate centers") {
    RouteParam::Range transpose{ -60.f, 60.f };   // inferRange -> 60
    // Absolute: u = raw source -> source 0 = base (delta 0), 0.5 = half, 1 = full
    expectTrue(near(RouteResolve::computeDelta(0.0f, Routing::Shaper::None, 100, transpose, Combine::Absolute), 0.f),  "abs src0 -> base");
    expectTrue(near(RouteResolve::computeDelta(0.5f, Routing::Shaper::None, 100, transpose, Combine::Absolute), 30.f), "abs src0.5 -> half");
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 100, transpose, Combine::Absolute), 60.f), "abs src1 -> full");
    // Modulate (default) centers: 0.5 neutral, 0 = -full
    expectTrue(near(RouteResolve::computeDelta(0.5f, Routing::Shaper::None, 100, transpose, Combine::Modulate), 0.f),   "mod src0.5 -> 0");
    expectTrue(near(RouteResolve::computeDelta(0.0f, Routing::Shaper::None, 100, transpose, Combine::Modulate), -60.f), "mod src0 -> -full");
    // default arg is Modulate (back-compat with the 4-arg call sites)
    expectTrue(near(RouteResolve::computeDelta(0.5f, Routing::Shaper::None, 100, transpose), 0.f), "default = Modulate");
}

CASE("computeDelta: scaleValue is a unipolar gain on depth (None->1.0 identity)") {
    RouteParam::Range transpose{ -60.f, 60.f };   // inferRange -> 60
    // scaleValue 1.0 = identity (no scale source), default arg matches
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 100, transpose, Combine::Modulate, 1.0f), 60.f), "scale 1.0 -> full");
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 100, transpose), 60.f), "default scale = 1.0");
    // scaleValue 0.5 = half deviation -> half delta
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 100, transpose, Combine::Modulate, 0.5f), 30.f), "scale 0.5 -> half");
    // scaleValue 0 = collapse to center -> no modulation
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 100, transpose, Combine::Modulate, 0.0f), 0.f), "scale 0 -> none");
    // center stays neutral for any scaleValue
    expectTrue(near(RouteResolve::computeDelta(0.5f, Routing::Shaper::None, 100, transpose, Combine::Modulate, 0.5f), 0.f), "center neutral under scale");
}

CASE("busDelta: scaleValue gains the lane delta (None->1.0 identity)") {
    auto none = Routing::Shaper::None;
    expectTrue(near(RouteResolve::busDelta(1.0f, none, 100, Combine::Modulate, 1.0f), 5.f), "scale 1.0 -> +5V");
    expectTrue(near(RouteResolve::busDelta(1.0f, none, 100, Combine::Modulate), 5.f), "default scale = 1.0");
    expectTrue(near(RouteResolve::busDelta(1.0f, none, 100, Combine::Modulate, 0.5f), 2.5f), "scale 0.5 -> +2.5V");
    expectTrue(near(RouteResolve::busDelta(1.0f, none, 100, Combine::Modulate, 0.0f), 0.f), "scale 0 -> 0V");
}

CASE("computeDelta: range comes from the migrated row") {
    uint8_t key; RouteParam::Range range;
    RouteResolve::overrideParam(Mode::Note, Routing::Target::Transpose, key, range);
    expectTrue(near(RouteResolve::computeDelta(1.0f, Routing::Shaper::None, 100, range), 60.f), "Transpose row -> 60");
}

CASE("migratedGlobal: project targets map to key + global-table range") {
    uint8_t key; RouteParam::Range range;
    expectTrue(RouteResolve::overrideParamGlobal(Routing::Target::Tempo, key, range), "Tempo migrates");
    expectEqual(int(key), int(ParamKey::Tempo), "Tempo key");
    expectTrue(near(range.min, 1.f) && near(range.max, 1000.f), "Tempo range 1..1000");

    expectTrue(RouteResolve::overrideParamGlobal(Routing::Target::Swing, key, range), "Swing migrates");
    expectEqual(int(key), int(ParamKey::Swing), "Swing key");
    expectTrue(near(range.min, 50.f) && near(range.max, 75.f), "Swing range 50..75");

    expectTrue(RouteResolve::overrideParamGlobal(Routing::Target::CvRouteScan, key, range), "CVR Scan migrates");
    expectEqual(int(key), int(ParamKey::CvRouteScan), "CVR Scan key");
    expectTrue(RouteResolve::overrideParamGlobal(Routing::Target::CvRouteRoute, key, range), "CVR Route migrates");
    expectEqual(int(key), int(ParamKey::CvRouteRoute), "CVR Route key");
}

CASE("migratedGlobal: non-project targets do not migrate here") {
    uint8_t key; RouteParam::Range range;
    expectFalse(RouteResolve::overrideParamGlobal(Routing::Target::Transpose, key, range), "per-track not global");
    expectFalse(RouteResolve::overrideParamGlobal(Routing::Target::Mute, key, range), "playstate not global");
    expectFalse(RouteResolve::overrideParamGlobal(Routing::Target::Play, key, range), "engine trigger not global");
}

CASE("busDelta: unified base-0 +/-5V delta, Modulate bipolar / Absolute sweep") {
    auto none = Routing::Shaper::None;
    // Modulate: neutral at source-center, full +/-5V rail at depth 100
    expectTrue(near(RouteResolve::busDelta(0.5f, none, 100, Combine::Modulate), 0.f), "MOD center -> 0V");
    expectTrue(near(RouteResolve::busDelta(1.0f, none, 100, Combine::Modulate), 5.f), "MOD src 1 -> +5V");
    expectTrue(near(RouteResolve::busDelta(0.0f, none, 100, Combine::Modulate), -5.f), "MOD src 0 -> -5V");
    // Absolute: sweep from base 0, source 0 -> 0V, source 1 -> +5V at depth 100
    expectTrue(near(RouteResolve::busDelta(0.0f, none, 100, Combine::Absolute), 0.f), "ABS src 0 -> 0V");
    expectTrue(near(RouteResolve::busDelta(1.0f, none, 100, Combine::Absolute), 5.f), "ABS src 1 -> +5V");
    // depth scales linearly; depth 0 = inert
    expectTrue(near(RouteResolve::busDelta(1.0f, none, 0, Combine::Modulate), 0.f), "depth 0 -> 0V");
    expectTrue(near(RouteResolve::busDelta(1.0f, none, 50, Combine::Modulate), 2.5f), "MOD depth 50 -> +2.5V");
}

} // UNIT_TEST
