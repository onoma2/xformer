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
    expectEqual(int(RouteFork::targetToParamKey(Routing::Target::Tempo)), int(ParamKey::None), "Tempo unmapped");
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

CASE("non-migrated track modes always stay on the old path") {
    uint8_t key; RouteParam::Range range;
    for (auto m : { Mode::Curve, Mode::MidiCv, Mode::Tuesday, Mode::DiscreteMap,
                    Mode::Indexed, Mode::Stochastic }) {
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

CASE("composes into a base-anchored delta via shaper + RouteApply") {
    uint8_t key; RouteParam::Range range;
    RouteFork::migrated(Mode::Note, Routing::Target::Transpose, key, range);
    float modRange = RouteFork::inferRange(range);
    // None shaper, full source, depth 100, Modulate -> +full range.
    float hi = RouteShaper::shape(Routing::Shaper::None, 1.0f);
    expectTrue(near(RouteApply::delta(hi, 1.0f, Combine::Modulate, 100, modRange), 60.f), "full swing +60");
    // Source center -> neutral (base anchored).
    float mid = RouteShaper::shape(Routing::Shaper::None, 0.5f);
    expectTrue(near(RouteApply::delta(mid, 1.0f, Combine::Modulate, 100, modRange), 0.f), "center neutral");
}

} // UNIT_TEST
