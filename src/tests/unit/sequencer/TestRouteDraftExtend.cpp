#include "UnitTest.h"

#include "apps/sequencer/model/RouteDraft.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/Project.h"

// 3.4 — extending a per-track modulation onto another track must STAGE in the
// draft; the live route stays untouched until commit, so CANCEL truly reverts.
// The old ListPage path mutated the live route directly, so cancel couldn't undo it.
namespace {
int committedRoute(Routing &routing) {
    auto d0 = RouteDraft::create(routing, Routing::Target::IndexedA, 2);   // inlet, per-track
    d0.route.setSource(Routing::Source::CvIn1);
    d0.route.setDepthPct(5, 0);                 // create() fills inlet depths to 100; zero the extend target
    RouteDraft::commit(routing, d0);
    return d0.routeIndex;
}
} // namespace

UNIT_TEST("RouteDraftExtend") {

CASE("extend stages onto the draft, not the live route") {
    Project project;
    auto &routing = project.routing();
    int idx = committedRoute(routing);

    auto d = RouteDraft::extend(routing, idx, Routing::Target::IndexedA, 5);
    expect((routing.route(idx).tracks() & (1 << 5)) == 0, "live route NOT mutated by extend");
    expect((d.route.tracks() & (1 << 5)) != 0, "draft carries track 5");
    expectEqual(d.route.depthPct(5), 100, "inlet extend defaults depth 100");
    expectFalse(d.isNew, "extend of an existing route is not new");
}

CASE("cancel after extend leaves the live route unchanged") {
    Project project;
    auto &routing = project.routing();
    int idx = committedRoute(routing);

    auto d = RouteDraft::extend(routing, idx, Routing::Target::IndexedA, 5);
    RouteDraft::cancel(routing, d);
    expect((routing.route(idx).tracks() & (1 << 5)) == 0, "cancel: track 5 not added");
    expectEqual(routing.route(idx).depthPct(5), 0, "cancel: track 5 depth stays 0");
}

CASE("extend then commit persists the added track") {
    Project project;
    auto &routing = project.routing();
    int idx = committedRoute(routing);

    auto d = RouteDraft::extend(routing, idx, Routing::Target::IndexedA, 5);
    RouteDraft::commit(routing, d);
    expect((routing.route(idx).tracks() & (1 << 5)) != 0, "commit persists track 5");
    expectEqual(routing.route(idx).depthPct(5), 100, "committed depth 100");
}

}
