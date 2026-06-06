#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"
#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteParamKey.h"

#include <cmath>

// Step 4 of the apply-fork slice: Note + PhaseFlux getters read clamp(base +
// override) and edit-gate on override presence (not isRouted). The override table
// is a static, so the engine writer is simulated here by driving it directly.
//
// Proves: base-anchored read; stale-clear on route delete; base never mutated
// under routing (PhaseFlux Scale/RootNote defect fix); clamp to the param's
// routing range. Edits are NOT gated under a route (plan 006): they edit the base
// (anchor), shifting the modulation center, with no lurch by the live override.

namespace {
constexpr int kTrack = 2;
}

UNIT_TEST("RouteGetterMigration") {

CASE("Note scale: base-anchored read + stale clear, base never mutated") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(5);
    expectEqual(seq.scale(), 5, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 3.f);
    expectEqual(seq.scale(), 8, "override -> base + delta");

    Routing::clearRouteOverrides();
    expectEqual(seq.scale(), 5, "stale clear -> base restored (base never mutated)");
}

CASE("Note scale: override clamps to routing range 0..23") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(20);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 10.f);
    expectEqual(seq.scale(), 23, "clamps hi");
}

CASE("Note scale: Default(-1) preserved when unrouted") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(-1);
    expectEqual(seq.scale(), -1, "Default passes through when no override");
}

CASE("Note scale: edit under active route shifts base, no lurch") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(5);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 7.f);
    expectEqual(seq.scale(), 12, "effective = base + override");
    seq.editScale(2, false);                       // edit while routed (no longer gated)
    expectEqual(seq.scale(), 14, "effective shifts by edit amount (base 7 + override 7)");
    Routing::clearRouteOverrides();
    expectEqual(seq.scale(), 7, "base = 5 + 2, NOT 5 + 7 + 2 (edit-from-base, no lurch)");

    seq.editScale(1, false);
    expectEqual(seq.scale(), 8, "unrouted edit unchanged");
}

// Track-level getters (NoteTrack/PhaseFluxTrack transpose/octave/...) use the same
// Routing::routedValueInt helper as the sequence getters; their leaf trackIndex is
// private (friend Project), so the bipolar/rounding path is proven on the helper.
CASE("routedValueInt: bipolar clamp + rounding around base") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::Transpose, kTrack, 60.f);
    expectEqual(Routing::routedValueInt(ParamKey::Transpose, kTrack, 0, -60, 60), 60, "+full");
    Routing::writeRouteOverride(ParamKey::Transpose, kTrack, -60.f);
    expectEqual(Routing::routedValueInt(ParamKey::Transpose, kTrack, 0, -60, 60), -60, "-full");
    Routing::writeRouteOverride(ParamKey::Transpose, kTrack, 100.f);
    expectEqual(Routing::routedValueInt(ParamKey::Transpose, kTrack, 0, -60, 60), 60, "clamps hi");
    Routing::writeRouteOverride(ParamKey::Transpose, kTrack, 2.6f);
    expectEqual(Routing::routedValueInt(ParamKey::Transpose, kTrack, 0, -60, 60), 3, "rounds to nearest");
    Routing::clearRouteOverrides();
    expectEqual(Routing::routedValueInt(ParamKey::Transpose, kTrack, 0, -60, 60), 0, "no override -> base");
}

CASE("Note override is per track index") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(5);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack + 1, 3.f);
    expectEqual(seq.scale(), 5, "other track's override does not leak");
}

CASE("PhaseFlux scale: base never mutated (defect fix) + selectedScale uses override") {
    Routing::clearRouteOverrides();
    PhaseFluxSequence seq;
    seq.setTrackIndex(kTrack);
    seq.setScale(5);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 3.f);
    expectEqual(seq.scale(), 8, "override read");
    expectEqual(&seq.selectedScale(0), &Scale::get(8), "selectedScale follows override");
    Routing::clearRouteOverrides();
    expectEqual(seq.scale(), 5, "base intact after clear");
}

CASE("PhaseFlux scale: edit under route shifts base, no lurch") {
    Routing::clearRouteOverrides();
    PhaseFluxSequence seq;
    seq.setTrackIndex(kTrack);
    seq.setScale(5);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 3.f);
    seq.editScale(2, false);                        // edit while routed
    Routing::clearRouteOverrides();
    expectEqual(seq.scale(), 7, "base = 5 + 2, not 5 + 3 + 2");
}

CASE("Note first/last edit clamps in base domain under route") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setFirstStep(0);
    seq.setLastStep(15);
    Routing::writeRouteOverride(ParamKey::LastStep, kTrack, 16.f);   // lastStep() effective -> 31
    expectEqual(seq.lastStep(), 31, "last effective = 15 + 16");
    seq.editFirstStep(20, false);                  // base first clamps against base last (15)
    Routing::clearRouteOverrides();
    expectEqual(seq.firstStep(), 15, "first clamped to last BASE (15), not routed window (31)");
}

// CurveTrack track-level int getters migrate onto routedValueInt: read
// clamp(base + override) under a committed Curve route, base when unrouted. Track 0
// in Curve mode carries trackIndex 0 (set by Project::setTrackMode).
CASE("Curve slideTime: base-anchored read + stale clear (unipolar 0..100)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Curve);
    auto &track = p.track(0).curveTrack();
    track.setSlideTime(40);
    expectEqual(track.slideTime(), 40, "no override -> base");

    Routing::writeRouteOverride(ParamKey::SlideTime, 0, 30.f);
    expectEqual(track.slideTime(), 70, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::SlideTime, 0, 100.f);
    expectEqual(track.slideTime(), 100, "clamps hi to 100");

    Routing::clearRouteOverrides();
    expectEqual(track.slideTime(), 40, "stale clear -> base restored");
}

CASE("Curve rotate: signed base-anchored read + clamp (bipolar -64..64)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Curve);
    auto &track = p.track(0).curveTrack();
    track.setRotate(-10);
    expectEqual(track.rotate(), -10, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Rotate, 0, 30.f);
    expectEqual(track.rotate(), 20, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Rotate, 0, -100.f);
    expectEqual(track.rotate(), -64, "clamps lo to -64");

    Routing::clearRouteOverrides();
    expectEqual(track.rotate(), -10, "stale clear -> base restored");
}

// curveRate is the float/centi special case: stored 0.0..4.0, routed in the
// 0..400 centi domain (override delta is centi), getter divides by 100.
CASE("Curve curveRate: centi-domain override read + stale clear") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Curve);
    auto &track = p.track(0).curveTrack();
    track.setCurveRate(1.0f);                    // base 1.0 -> centi 100
    expect(std::fabs(track.curveRate() - 1.0f) < 1e-4f, "no override -> base");

    Routing::writeRouteOverride(ParamKey::CurveRate, 0, 150.f);  // centi delta 150 -> 250
    expect(std::fabs(track.curveRate() - 2.5f) < 1e-4f, "override -> (base + delta) / 100");

    Routing::clearRouteOverrides();
    expect(std::fabs(track.curveRate() - 1.0f) < 1e-4f, "stale clear -> base restored");
}

// CurveSequence-level getters migrate onto routedValueInt the same way: read
// clamp(base + override) under a committed route, base when unrouted. The sequence
// inherits the track's trackIndex (0 here).
CASE("Curve sequence divisor: base-anchored read + stale clear (unipolar 1..768)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Curve);
    auto &seq = p.track(0).curveTrack().sequence(0);
    seq.setDivisor(12);
    expectEqual(seq.divisor(), 12, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Divisor, 0, 100.f);
    expectEqual(seq.divisor(), 112, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Divisor, 0, 10000.f);
    expectEqual(seq.divisor(), 768, "clamps hi to 768");

    Routing::clearRouteOverrides();
    expectEqual(seq.divisor(), 12, "stale clear -> base restored");
}

CASE("Curve sequence djFilter: signed centi-domain override read + clamp (bipolar -100..100)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Curve);
    auto &seq = p.track(0).curveTrack().sequence(0);
    seq.setDjFilter(0.2f);                          // base centi 20
    expect(std::fabs(seq.djFilter() - 0.2f) < 1e-4f, "no override -> base");

    Routing::writeRouteOverride(ParamKey::DjFilter, 0, 30.f);  // centi delta 30 -> 50
    expect(std::fabs(seq.djFilter() - 0.5f) < 1e-4f, "override -> (base + delta) / 100");

    Routing::writeRouteOverride(ParamKey::DjFilter, 0, -300.f);
    expect(std::fabs(seq.djFilter() - (-1.0f)) < 1e-4f, "clamps lo to -1.0 (centi -100)");

    Routing::clearRouteOverrides();
    expect(std::fabs(seq.djFilter() - 0.2f) < 1e-4f, "stale clear -> base restored");
}

CASE("Curve sequence chaosAmount: base-anchored read + clamp (unipolar 0..100)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Curve);
    auto &seq = p.track(0).curveTrack().sequence(0);
    seq.setChaosAmount(40);
    expectEqual(seq.chaosAmount(), 40, "no override -> base");

    Routing::writeRouteOverride(ParamKey::ChaosAmount, 0, 30.f);
    expectEqual(seq.chaosAmount(), 70, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::ChaosAmount, 0, 100.f);
    expectEqual(seq.chaosAmount(), 100, "clamps hi to 100");

    Routing::clearRouteOverrides();
    expectEqual(seq.chaosAmount(), 40, "stale clear -> base restored");
}

// Tuesday sequence getters migrate onto routedValueInt the same way. Track 0 in
// Tuesday mode carries trackIndex 0 (set by Project::setTrackMode).
CASE("Tuesday algorithm: base-anchored read + stale clear (discrete 0..14)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Tuesday);
    auto &seq = p.track(0).tuesdayTrack().sequence(0);
    seq.setAlgorithm(5);
    expectEqual(seq.algorithm(), 5, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Algorithm, 0, 4.f);
    expectEqual(seq.algorithm(), 9, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Algorithm, 0, 100.f);
    expectEqual(seq.algorithm(), 14, "clamps hi to 14");

    Routing::clearRouteOverrides();
    expectEqual(seq.algorithm(), 5, "stale clear -> base restored");
}

CASE("Tuesday octave: signed base-anchored read + clamp (bipolar -10..10)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Tuesday);
    auto &seq = p.track(0).tuesdayTrack().sequence(0);
    seq.setOctave(-3);
    expectEqual(seq.octave(), -3, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Octave, 0, 5.f);
    expectEqual(seq.octave(), 2, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Octave, 0, -100.f);
    expectEqual(seq.octave(), -10, "clamps lo to -10");

    Routing::clearRouteOverrides();
    expectEqual(seq.octave(), -3, "stale clear -> base restored");
}

CASE("Tuesday scale: Default(-1) preserved unrouted, clamps to 0..23 under route") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Tuesday);
    auto &seq = p.track(0).tuesdayTrack().sequence(0);
    seq.setScale(-1);
    expectEqual(seq.scale(), -1, "Default passes through when no override");

    Routing::writeRouteOverride(ParamKey::Scale, 0, 5.f);
    expectEqual(seq.scale(), 4, "under route effective = clamp(base(-1) + delta(5), 0, 23)");

    seq.setScale(20);
    Routing::writeRouteOverride(ParamKey::Scale, 0, 10.f);
    expectEqual(seq.scale(), 23, "clamps hi to 23");

    Routing::clearRouteOverrides();
    expectEqual(seq.scale(), 20, "stale clear -> base restored");
}

CASE("Tuesday rotate: routed effective clamps to dynamic loop limit") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Tuesday);
    auto &seq = p.track(0).tuesdayTrack().sequence(0);

    seq.setLoopLength(4);                            // loopLengthValues[4] = 4 -> limit +-3
    seq.setRotate(0);
    Routing::writeRouteOverride(ParamKey::Rotate, 0, 50.f);
    expectEqual(seq.rotate(), 3, "routed effective clamped to loop limit (len-1 = 3)");

    seq.setLoopLength(1);                            // loopLengthValues[1] = 1 -> limit 0
    expectEqual(seq.rotate(), 0, "len 1 collapses routed rotate to 0");

    Routing::clearRouteOverrides();
}

// Stochastic sequence getters migrate onto routedValueInt the same way. Track 0
// in Stochastic mode carries trackIndex 0 (set by Project::setTrackMode).
CASE("Stochastic complexity: base-anchored read + stale clear (unipolar 0..100)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Stochastic);
    auto &seq = p.track(0).stochasticTrack().sequence(0);
    seq.setComplexity(40);
    expectEqual(seq.complexity(), 40, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Complexity, 0, 30.f);
    expectEqual(seq.complexity(), 70, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Complexity, 0, 100.f);
    expectEqual(seq.complexity(), 100, "clamps hi to 100");

    Routing::clearRouteOverrides();
    expectEqual(seq.complexity(), 40, "stale clear -> base restored");
}

// Track-level transpose: field clamps +-100 while the ParamTable row range is
// +-60. The getter must use the FIELD clamp, so a base above 60 is preserved and
// not clamped down to 60 under a route.
CASE("Stochastic transpose: field range +-100, base>60 not clamped to 60 under route") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Stochastic);
    auto &track = p.track(0).stochasticTrack();
    track.setTranspose(80);
    expectEqual(track.transpose(), 80, "no override -> base (field allows >60)");

    Routing::writeRouteOverride(ParamKey::Transpose, 0, 5.f);
    expectEqual(track.transpose(), 85, "base 80 + delta, NOT clamped to 60");

    Routing::writeRouteOverride(ParamKey::Transpose, 0, 100.f);
    expectEqual(track.transpose(), 100, "clamps hi to field range 100");

    Routing::writeRouteOverride(ParamKey::Transpose, 0, -300.f);
    expectEqual(track.transpose(), -100, "clamps lo to field range -100");

    Routing::clearRouteOverrides();
    expectEqual(track.transpose(), 80, "stale clear -> base restored");
}

CASE("Stochastic contour: signed base-anchored read + clamp (bipolar -100..100)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Stochastic);
    auto &seq = p.track(0).stochasticTrack().sequence(0);
    seq.setContour(-20);
    expectEqual(seq.contour(), -20, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Contour, 0, 50.f);
    expectEqual(seq.contour(), 30, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Contour, 0, -300.f);
    expectEqual(seq.contour(), -100, "clamps lo to -100");

    Routing::clearRouteOverrides();
    expectEqual(seq.contour(), -20, "stale clear -> base restored");
}

CASE("Stochastic scale: Default(-1) preserved unrouted, clamps to 0..23 under route") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Stochastic);
    auto &seq = p.track(0).stochasticTrack().sequence(0);
    seq.setScale(-1);
    expectEqual(seq.scale(), -1, "Default passes through when no override");

    Routing::writeRouteOverride(ParamKey::Scale, 0, 5.f);
    expectEqual(seq.scale(), 4, "under route effective = clamp(base(-1) + delta(5), 0, 23)");

    seq.setScale(20);
    Routing::writeRouteOverride(ParamKey::Scale, 0, 10.f);
    expectEqual(seq.scale(), 23, "clamps hi to 23");

    Routing::clearRouteOverrides();
    expectEqual(seq.scale(), 20, "stale clear -> base restored");
}

} // UNIT_TEST
