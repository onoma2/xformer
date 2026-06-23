#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"
#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteParamKey.h"
#include "apps/sequencer/model/Scale.h"

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
    {
        auto view = seq.selectedScale(0, 0);
        const Scale &expected = Scale::get(8);
        for (int d : {0, 1, 4, 7}) {
            expectEqual(view.noteToVolts(d), expected.noteToVolts(d), "selectedScale follows override");
        }
    }
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

CASE("Curve sequence wavefolderFold: centi-domain override read + round + clamp (0..100)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Curve);
    auto &seq = p.track(0).curveTrack().sequence(0);
    seq.setWavefolderFold(0.30f);                   // base centi 30
    expect(std::fabs(seq.wavefolderFold() - 0.30f) < 1e-4f, "no override -> base");

    Routing::writeRouteOverride(ParamKey::WavefolderFold, 0, 25.f);  // centi delta 25 -> 55
    expect(std::fabs(seq.wavefolderFold() - 0.55f) < 1e-4f, "override -> (base + delta) / 100");

    Routing::writeRouteOverride(ParamKey::WavefolderFold, 0, 0.4f);  // 30 + 0.4 rounds to 30
    expect(std::fabs(seq.wavefolderFold() - 0.30f) < 1e-4f, "sub-centi delta rounds to nearest");

    Routing::writeRouteOverride(ParamKey::WavefolderFold, 0, 999.f);
    expect(std::fabs(seq.wavefolderFold() - 1.0f) < 1e-4f, "clamps hi to 1.0 (centi 100)");

    Routing::clearRouteOverrides();
    expect(std::fabs(seq.wavefolderFold() - 0.30f) < 1e-4f, "stale clear -> base restored");
}

CASE("Curve sequence wavefolderGain: centi-domain override read + clamp (0..200)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Curve);
    auto &seq = p.track(0).curveTrack().sequence(0);
    seq.setWavefolderGain(1.0f);                    // base centi 100
    expect(std::fabs(seq.wavefolderGain() - 1.0f) < 1e-4f, "no override -> base");

    Routing::writeRouteOverride(ParamKey::WavefolderGain, 0, 50.f);  // centi delta 50 -> 150
    expect(std::fabs(seq.wavefolderGain() - 1.5f) < 1e-4f, "override -> (base + delta) / 100");

    Routing::writeRouteOverride(ParamKey::WavefolderGain, 0, 999.f);
    expect(std::fabs(seq.wavefolderGain() - 2.0f) < 1e-4f, "clamps hi to 2.0 (centi 200)");

    Routing::clearRouteOverrides();
    expect(std::fabs(seq.wavefolderGain() - 1.0f) < 1e-4f, "stale clear -> base restored");
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
CASE("Tuesday algorithm: base-anchored read + stale clear (discrete 0..15)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Tuesday);
    auto &seq = p.track(0).tuesdayTrack().sequence(0);
    seq.setAlgorithm(5);
    expectEqual(seq.algorithm(), 5, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Algorithm, 0, 4.f);
    expectEqual(seq.algorithm(), 9, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Algorithm, 0, 100.f);
    expectEqual(seq.algorithm(), 15, "clamps hi to 15");

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

// variation getter abs()'s the BASE before routedValueInt, so a magnitude
// survives the clamp instead of a negative base collapsing to 0 then abs'ing
// to 0. Negative-base legacy storage has no public setter (setVariation clamps
// to 0..100), so the abs path is exercised via setBase on the field snapshot.
CASE("Stochastic variation: negative base reads magnitude under route") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Stochastic);
    auto &seq = p.track(0).stochasticTrack().sequence(0);
    seq.setVariation(30);
    StochasticSequence::ContentSnapshot snap;
    seq.captureContentTo(snap);
    snap.variation = -30;                            // simulate legacy signed storage
    seq.restoreContentFrom(snap);

    Routing::writeRouteOverride(ParamKey::Variation, 0, 0.f);
    expectEqual(seq.variation(), 30, "abs(base) before clamp: -30 -> 30, not 0");

    Routing::clearRouteOverrides();
    expectEqual(seq.variation(), 30, "stale clear -> magnitude preserved");
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

// Saturating the Scale route must reach the highest LOADED index, Scale::Count-1
// (a user-scale slot, beyond the 20 builtins), and never exceed it.
CASE("Stochastic scale: routed clamp tracks Scale::Count-1, not literal 23") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Stochastic);
    auto &seq = p.track(0).stochasticTrack().sequence(0);

    seq.setScale(0);
    Routing::writeRouteOverride(ParamKey::Scale, 0, 1000.f);
    expectEqual(seq.scale(), Scale::Count - 1, "saturates to highest loaded index");
    expectTrue(seq.scale() < Scale::Count, "never exceeds loaded scale count");

    Routing::clearRouteOverrides();
}

// DiscreteMap sequence getters migrate onto routedValueInt/routedValue. Track 0
// in DiscreteMap mode carries trackIndex 0 (set by Project::setTrackMode).
CASE("DiscreteMap octave: signed base-anchored read + clamp (bipolar -10..10)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::DiscreteMap);
    auto &seq = p.track(0).discreteMapTrack().sequence(0);
    seq.setOctave(-3);
    expectEqual(seq.octave(), -3, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Octave, 0, 5.f);
    expectEqual(seq.octave(), 2, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Octave, 0, -100.f);
    expectEqual(seq.octave(), -10, "clamps lo to -10");

    Routing::clearRouteOverrides();
    expectEqual(seq.octave(), -3, "stale clear -> base restored");
}

CASE("DiscreteMap rangeHigh: float base-anchored read via routedValue (bipolar +-5)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::DiscreteMap);
    auto &seq = p.track(0).discreteMapTrack().sequence(0);
    seq.setRangeHigh(2.0f);
    expect(std::fabs(seq.rangeHigh() - 2.0f) < 1e-4f, "no override -> base");

    Routing::writeRouteOverride(ParamKey::DiscreteMapRangeHigh, 0, 1.5f);
    expect(std::fabs(seq.rangeHigh() - 3.5f) < 1e-4f, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::DiscreteMapRangeHigh, 0, 10.f);
    expect(std::fabs(seq.rangeHigh() - 5.0f) < 1e-4f, "clamps hi to +5");

    Routing::clearRouteOverrides();
    expect(std::fabs(seq.rangeHigh() - 2.0f) < 1e-4f, "stale clear -> base restored");
}

// Base-0 inlet: no param-door base, override path feeds the value directly.
CASE("DiscreteMap input inlet: base-0 override read") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::DiscreteMap);
    auto &track = p.track(0).discreteMapTrack();
    expect(std::fabs(track.routedInput() - 0.0f) < 1e-4f, "no override -> 0");

    Routing::writeRouteOverride(ParamKey::DiscreteMapInput, 0, 2.0f);
    expect(std::fabs(track.routedInput() - 2.0f) < 1e-4f, "override -> delta over base 0");

    Routing::clearRouteOverrides();
    expect(std::fabs(track.routedInput() - 0.0f) < 1e-4f, "stale clear -> 0");
}

// Indexed track-level transpose migrates onto routedValueInt (bipolar -60..60).
CASE("Indexed transpose: signed base-anchored read + clamp (bipolar -60..60)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Indexed);
    auto &track = p.track(0).indexedTrack();
    track.setTranspose(-20);
    expectEqual(track.transpose(), -20, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Transpose, 0, 50.f);
    expectEqual(track.transpose(), 30, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Transpose, 0, 300.f);
    expectEqual(track.transpose(), 60, "clamps hi to 60");

    Routing::clearRouteOverrides();
    expectEqual(track.transpose(), -20, "stale clear -> base restored");
}

CASE("Indexed scale: Default(-1) preserved unrouted, clamps to 0..23 under route") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Indexed);
    auto &seq = p.track(0).indexedTrack().sequence(0);
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

// Indexed inlet A: base-0 override in the +-100 domain, getter scales by 0.01.
CASE("Indexed inlet A: base-0 override read, x0.01 normalize") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Indexed);
    auto &seq = p.track(0).indexedTrack().sequence(0);
    expect(std::fabs(seq.routedIndexedA() - 0.0f) < 1e-4f, "no override -> 0.0");

    Routing::writeRouteOverride(ParamKey::IndexedA, 0, 50.f);   // +-100 domain
    expect(std::fabs(seq.routedIndexedA() - 0.5f) < 1e-4f, "override +50 -> 0.5 after x0.01");

    Routing::clearRouteOverrides();
    expect(std::fabs(seq.routedIndexedA() - 0.0f) < 1e-4f, "stale clear -> 0.0");
}

CASE("MidiCv slideTime: base-anchored read + stale clear (unipolar 0..100)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::MidiCv);
    auto &track = p.track(0).midiCvTrack();
    track.setSlideTime(40);
    expectEqual(track.slideTime(), 40, "no override -> base");

    Routing::writeRouteOverride(ParamKey::SlideTime, 0, 30.f);
    expectEqual(track.slideTime(), 70, "override -> base + delta");

    Routing::clearRouteOverrides();
    expectEqual(track.slideTime(), 40, "stale clear -> base restored");
}

CASE("MidiCv transpose: getter clamps to field range +-100, not table +-60") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::MidiCv);
    auto &track = p.track(0).midiCvTrack();
    track.setTranspose(80);
    expectEqual(track.transpose(), 80, "no override -> base (valid >table 60)");

    Routing::writeRouteOverride(ParamKey::Transpose, 0, 30.f);
    expectEqual(track.transpose(), 100, "base 80 + 30 clamps to field 100, not table 60");

    Routing::clearRouteOverrides();
    expectEqual(track.transpose(), 80, "stale clear -> base restored");
}

// Stochastic routing revamp — Range (pitch candidate-set width) becomes routable.
CASE("Stochastic range: int base-anchored read via routedValueInt (0..100)") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Stochastic);
    auto &seq = p.track(0).stochasticTrack().sequence(0);
    seq.setRange(40);
    expectEqual(seq.range(), 40, "no override -> base");

    Routing::writeRouteOverride(ParamKey::Range, 0, 30.f);
    expectEqual(seq.range(), 70, "override -> base + delta");

    Routing::writeRouteOverride(ParamKey::Range, 0, 100.f);
    expectEqual(seq.range(), 100, "clamps hi to 100");

    Routing::clearRouteOverrides();
    expectEqual(seq.range(), 40, "stale clear -> base restored");
}

CASE("Stochastic Bias/Spread/BurstCount/BurstRate: base-anchored routed reads") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::Stochastic);
    auto &seq = p.track(0).stochasticTrack().sequence(0);
    seq.setMarblesBias(20);
    seq.setMarblesSpread(60);
    seq.setBurstCount(10);
    seq.setBurstRate(80);

    Routing::writeRouteOverride(ParamKey::MarblesBias, 0, 15.f);
    Routing::writeRouteOverride(ParamKey::MarblesSpread, 0, -30.f);
    Routing::writeRouteOverride(ParamKey::BurstCount, 0, 5.f);
    Routing::writeRouteOverride(ParamKey::BurstRate, 0, 50.f);
    expectEqual(seq.marblesBias(), 35, "Bias base+delta");
    expectEqual(seq.marblesSpread(), 30, "Spread base+delta");
    expectEqual(seq.burstCount(), 15, "BurstCount base+delta");
    expectEqual(seq.burstRate(), 100, "BurstRate base+delta clamps to 100");

    Routing::clearRouteOverrides();
    expectEqual(seq.marblesBias(), 20, "Bias clear -> base");
    expectEqual(seq.burstRate(), 80, "BurstRate clear -> base");
}

// The override table is keyed by (paramKey, trackIndex), and every sequence under a
// DiscreteMap track shares that track index — so one routed param fans out to all the
// track's pattern slots AND the snapshot slot. Pins the fan-out that lost its direct
// assertion when the apply-hooks were stripped (7029bf0e).
CASE("DiscreteMap: a routed param fans out to both a pattern and the snapshot slot") {
    Routing::clearRouteOverrides();
    Project p;
    p.setTrackMode(0, Track::TrackMode::DiscreteMap);
    auto &track = p.track(0).discreteMapTrack();
    auto &pat = track.sequence(0);                         // pattern slot
    auto &snap = track.sequence(CONFIG_PATTERN_COUNT);     // snapshot slot
    pat.setDivisor(48);
    snap.setDivisor(48);
    expectEqual(pat.divisor(), 48, "pattern base");
    expectEqual(snap.divisor(), 48, "snapshot base");

    Routing::writeRouteOverride(ParamKey::Divisor, 0, 12.f);
    expectEqual(pat.divisor(), 60, "override fans out to the pattern");
    expectEqual(snap.divisor(), 60, "override fans out to the snapshot too (shared track index)");

    Routing::clearRouteOverrides();
    expectEqual(snap.divisor(), 48, "stale clear -> snapshot base restored");
}

// selectedScale resolver: rotate inherits the project rotate when scaleRotate is
// -1, rotates regardless when set; rotate 0 reproduces the base scale's pitch.
CASE("selectedScale resolves rotation: inherit, override, identity") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(8);
    const Scale &base = Scale::get(8);

    seq.setScaleRotate(-1);
    {
        auto inherited = seq.selectedScale(8, 3);   // project rotate 3 flows through
        auto direct    = RotatedScaleView(base, 3);
        for (int d : {0, 1, 4, 7}) {
            expectEqual(inherited.noteToVolts(d), direct.noteToVolts(d), "inherit project rotate");
        }
    }

    seq.setScaleRotate(2);
    {
        auto rotated = seq.selectedScale(8, 0);     // ignores project rotate
        auto direct  = RotatedScaleView(base, 2);
        for (int d : {0, 1, 4, 7}) {
            expectEqual(rotated.noteToVolts(d), direct.noteToVolts(d), "track rotate wins over project");
        }
    }

    seq.setScaleRotate(0);
    {
        auto identity = seq.selectedScale(8, 0);
        for (int d : {0, 1, 4, 7}) {
            expectEqual(identity.noteToVolts(d), base.noteToVolts(d), "rotate 0 == base output");
        }
    }
}

} // UNIT_TEST
