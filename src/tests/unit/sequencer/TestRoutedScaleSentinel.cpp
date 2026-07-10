#include "UnitTest.h"

#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/IndexedSequence.h"
#include "apps/sequencer/model/FractalSequence.h"
#include "apps/sequencer/model/ScaleResolve.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteParamKey.h"
#include "apps/sequencer/model/Scale.h"

// 3.1 — a scale/rootNote route must not crush the Default (-1) sentinel. At rest
// (delta 0) Default is preserved; a nonzero delta on a Default track modulates
// RELATIVE to the resolved project scale, so it keeps tracking project changes.
namespace { constexpr int kTrack = 2; }

UNIT_TEST("RoutedScaleSentinel") {

CASE("scale route at rest preserves Default") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(-1);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 0.f);       // route present, delta 0
    expectEqual(seq.scale(), -1, "Default scale preserved under a delta-0 route");

    seq.setRootNote(-1);
    Routing::writeRouteOverride(ParamKey::RootNote, kTrack, 0.f);
    expectEqual(seq.rootNote(), -1, "Default rootNote preserved under a delta-0 route");
    Routing::clearRouteOverrides();
}

CASE("non-Default base still modulates") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(5);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 3.f);
    expectEqual(seq.scale(), 8, "ordinary base modulates as before");
    Routing::clearRouteOverrides();
}

CASE("resolveScale: Default modulates around the resolved project scale") {
    Routing::clearRouteOverrides();
    expectEqual(ScaleResolve::resolveScale(-1, kTrack, 5), 5, "Default, no route -> project scale");

    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 2.f);
    expectEqual(ScaleResolve::resolveScale(-1, kTrack, 5), 7, "Default + delta -> project + delta");
    expectEqual(ScaleResolve::resolveScale(-1, kTrack, 6), 8, "tracks project-scale changes");
    expectEqual(ScaleResolve::resolveScale(9, kTrack, 5), 9, "concrete scale ignores project");

    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 100.f);
    expectEqual(ScaleResolve::resolveScale(-1, kTrack, 20), Scale::Count - 1, "clamps hi");
    Routing::clearRouteOverrides();
}

CASE("selectedScale resolves Default around the project scale") {
    Routing::clearRouteOverrides();
    NoteSequence seq(kTrack);
    seq.setScale(-1);
    Routing::writeRouteOverride(ParamKey::Scale, kTrack, 2.f);       // projectScale 5 + 2 = 7
    RotatedScaleView got = seq.selectedScale(5, 0);
    RotatedScaleView want(Scale::get(7), 0);
    for (int n = 0; n < 5; ++n) {
        expectEqual(int(got.noteToVolts(n) * 1000.f), int(want.noteToVolts(n) * 1000.f), "resolves to project+delta scale");
    }
    Routing::clearRouteOverrides();
}

CASE("Indexed/Fractal rootNote getter preserves Default under a route") {
    Routing::clearRouteOverrides();
    Routing::writeRouteOverride(ParamKey::RootNote, kTrack, 3.f);
    { IndexedSequence seq; seq.setTrackIndex(kTrack); seq.setRootNote(-1); expectEqual(seq.rootNote(), -1, "Indexed Default rootNote preserved under route"); }
    { FractalSequence seq; seq.setTrackIndex(kTrack); seq.setRootNote(-1); expectEqual(seq.rootNote(), -1, "Fractal Default rootNote preserved under route"); }
    Routing::clearRouteOverrides();
}

CASE("resolveRootNote: Default modulates around the project root") {
    Routing::clearRouteOverrides();
    expectEqual(ScaleResolve::resolveRootNote(-1, kTrack, 3), 3, "Default, no route -> project root");

    Routing::writeRouteOverride(ParamKey::RootNote, kTrack, 4.f);
    expectEqual(ScaleResolve::resolveRootNote(-1, kTrack, 3), 7, "Default + delta -> project + delta");
    expectEqual(ScaleResolve::resolveRootNote(2, kTrack, 3), 2, "concrete root passes through");

    Routing::writeRouteOverride(ParamKey::RootNote, kTrack, 50.f);
    expectEqual(ScaleResolve::resolveRootNote(-1, kTrack, 6), 11, "clamps to 11");
    Routing::clearRouteOverrides();
}

}
