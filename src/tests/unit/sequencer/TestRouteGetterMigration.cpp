#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteParamKey.h"

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

} // UNIT_TEST
