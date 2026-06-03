#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteParamKey.h"

// Step 4 of the apply-fork slice: Note + PhaseFlux getters read clamp(base +
// override) and edit-gate on override presence (not isRouted). The override table
// is a static, so the engine writer is simulated here by driving it directly.
//
// Proves: base-anchored read; stale-clear on route delete; base never mutated
// under routing (PhaseFlux Scale/RootNote defect fix); edit-gating matches the
// old isRouted gate; clamp to the param's routing range.

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

CASE("Note scale: edit-gating blocks edits while overridden") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(5);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 0.f);
    seq.editScale(2, false);
    Routing::clearRouteOverrides();
    expectEqual(seq.scale(), 5, "edit ignored while overridden");

    seq.editScale(2, false);
    expectEqual(seq.scale(), 7, "edit applies when not overridden");
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

CASE("PhaseFlux divisor: override + edit gate") {
    Routing::clearRouteOverrides();
    PhaseFluxSequence seq;
    seq.setTrackIndex(kTrack);
    seq.setDivisor(12);
    Routing::writeRouteOverride(ParamKey::Divisor, kTrack, 0.f);
    seq.editDivisor(1, false);
    Routing::clearRouteOverrides();
    expectEqual(seq.divisor(), 12, "edit blocked while overridden");
}

} // UNIT_TEST
