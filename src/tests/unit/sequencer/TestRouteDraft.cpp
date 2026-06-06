#include "UnitTest.h"

#include "apps/sequencer/model/RouteDraft.h"
#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/Project.h"

UNIT_TEST("RouteDraft") {

    CASE("create allocates an inert slot with phase-1 defaults") {
        Project project;
        auto &routing = project.routing();
        auto d = RouteDraft::create(routing, Routing::Target::Transpose, 2);
        expectTrue(d.routeIndex >= 0, "slot allocated");
        expectTrue(d.isNew, "isNew set");
        expectEqual(int(d.route.target()), int(Routing::Target::Transpose), "target");
        expectEqual(int(d.route.tracks()), int(1 << 2), "tracks = current track only");
        expectEqual(int(d.route.combine()), int(RouteApply::Combine::Modulate), "Modulate");
        expectEqual(int(d.route.source()), int(Routing::Source::None), "source None");
        for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
            expectEqual(d.route.depthPct(t), 0, "depth 0");
        }
    }

    CASE("create defaults inlet targets to full depth (base-0 sink)") {
        Project project;
        auto &routing = project.routing();
        auto in = RouteDraft::create(routing, Routing::Target::DiscreteMapInput, 2);
        expectEqual(in.route.depthPct(2), 100, "DMap Input current track depth 100");
        auto sc = RouteDraft::create(routing, Routing::Target::DiscreteMapScanner, 0);
        expectEqual(sc.route.depthPct(0), 100, "DMap Scanner current track depth 100");
        auto tr = RouteDraft::create(routing, Routing::Target::Transpose, 2);
        expectEqual(tr.route.depthPct(2), 0, "normal target current track depth 0");
    }

    CASE("create returns routeIndex -1 when no empty slot") {
        Project project;
        auto &routing = project.routing();
        for (int i = 0; i < CONFIG_ROUTE_COUNT; ++i) {
            routing.route(i).setTarget(Routing::Target::Transpose);
        }
        auto d = RouteDraft::create(routing, Routing::Target::Octave, 0);
        expectEqual(d.routeIndex, -1, "no slot");
    }

    CASE("canCommit requires a real source") {
        Project project;
        auto &routing = project.routing();
        auto d = RouteDraft::create(routing, Routing::Target::Transpose, 0);
        expectFalse(RouteDraft::canCommit(d), "source None -> cannot commit");
        d.route.setSource(Routing::Source::CvIn1);
        expectTrue(RouteDraft::canCommit(d), "source set -> can commit");
    }

    CASE("editing the draft does NOT touch the live route before commit") {
        Project project;
        auto &routing = project.routing();
        int idx;
        { auto c = RouteDraft::create(routing, Routing::Target::Transpose, 0);
          c.route.setSource(Routing::Source::CvIn1);
          RouteDraft::commit(routing, c); idx = c.routeIndex; }
        auto d = RouteDraft::begin(routing, idx);
        d.route.setDepthPct(0, 75);
        expectEqual(routing.route(idx).depthPct(0), 0, "live unchanged pre-commit");
        RouteDraft::commit(routing, d);
        expectEqual(routing.route(idx).depthPct(0), 75, "live updated on commit");
    }

    CASE("cancel on a new draft frees the slot") {
        Project project;
        auto &routing = project.routing();
        auto d = RouteDraft::create(routing, Routing::Target::Transpose, 0);
        int idx = d.routeIndex;
        RouteDraft::cancel(routing, d);
        expectEqual(int(routing.route(idx).target()), int(Routing::Target::None), "slot freed");
        expectEqual(routing.findEmptyRoute(), idx, "slot reusable");
    }

    CASE("cancel on an existing draft leaves the live route intact") {
        Project project;
        auto &routing = project.routing();
        int idx;
        { auto c = RouteDraft::create(routing, Routing::Target::Transpose, 0);
          c.route.setSource(Routing::Source::CvIn1);
          c.route.setDepthPct(0, 40);
          RouteDraft::commit(routing, c); idx = c.routeIndex; }
        auto d = RouteDraft::begin(routing, idx);
        d.route.setDepthPct(0, 90);
        RouteDraft::cancel(routing, d);
        expectEqual(routing.route(idx).depthPct(0), 40, "live unchanged");
        expectEqual(int(routing.route(idx).source()), int(Routing::Source::CvIn1), "src kept");
    }

    CASE("create rejects Target::None") {
        Project project; auto &routing = project.routing();
        auto d = RouteDraft::create(routing, Routing::Target::None, 0);
        expectEqual(d.routeIndex, -1, "None target -> no draft");
    }

    CASE("create rejects out-of-range trackIndex") {
        Project project; auto &routing = project.routing();
        auto d = RouteDraft::create(routing, Routing::Target::Transpose, CONFIG_TRACK_COUNT);
        expectEqual(d.routeIndex, -1, "track index >= CONFIG_TRACK_COUNT -> no draft");
    }

    CASE("isTrackModulated false before create, true after commit") {
        Project project;
        auto &routing = project.routing();
        expectFalse(RouteDraft::isTrackModulated(routing, Routing::Target::Transpose, 2), "not modulated initially");
        auto c = RouteDraft::create(routing, Routing::Target::Transpose, 2);
        c.route.setSource(Routing::Source::CvIn1);
        RouteDraft::commit(routing, c);
        expectTrue(RouteDraft::isTrackModulated(routing, Routing::Target::Transpose, 2), "modulated after commit");
    }

    CASE("removeTrack on single-track per-track modulation frees the slot") {
        Project project;
        auto &routing = project.routing();
        int idx;
        { auto c = RouteDraft::create(routing, Routing::Target::Transpose, 1);
          c.route.setSource(Routing::Source::CvIn1);
          RouteDraft::commit(routing, c); idx = c.routeIndex; }
        bool removed = RouteDraft::removeTrack(routing, Routing::Target::Transpose, 1);
        expectTrue(removed, "returns true");
        expectEqual(int(routing.route(idx).target()), int(Routing::Target::None), "slot freed");
        expectEqual(routing.findEmptyRoute(), idx, "slot reusable");
    }

    CASE("removeTrack on multi-track modulation clears only the named track") {
        Project project;
        auto &routing = project.routing();
        int idx;
        { auto c = RouteDraft::create(routing, Routing::Target::Transpose, 0);
          c.route.setSource(Routing::Source::CvIn1);
          c.route.setTracks((1 << 0) | (1 << 3));
          c.route.setDepthPct(0, 40);
          c.route.setDepthPct(3, 40);
          RouteDraft::commit(routing, c); idx = c.routeIndex; }
        bool removed = RouteDraft::removeTrack(routing, Routing::Target::Transpose, 0);
        expectTrue(removed, "returns true");
        expectEqual(int(routing.route(idx).target()), int(Routing::Target::Transpose), "route still active");
        expectEqual(int(routing.route(idx).tracks()), int(1 << 3), "only track 3 remains");
        expectEqual(routing.route(idx).depthPct(0), 0, "track 0 depth zeroed");
        expectTrue(RouteDraft::isTrackModulated(routing, Routing::Target::Transpose, 3), "track 3 still modulated");
        expectFalse(RouteDraft::isTrackModulated(routing, Routing::Target::Transpose, 0), "track 0 not modulated");
    }

    CASE("removeTrack on a global modulation frees the slot") {
        Project project;
        auto &routing = project.routing();
        int idx;
        { auto c = RouteDraft::create(routing, Routing::Target::Tempo, 0);
          c.route.setSource(Routing::Source::CvIn1);
          RouteDraft::commit(routing, c); idx = c.routeIndex; }
        bool removed = RouteDraft::removeTrack(routing, Routing::Target::Tempo, 0);
        expectTrue(removed, "returns true");
        expectEqual(int(routing.route(idx).target()), int(Routing::Target::None), "slot freed");
        expectEqual(routing.findEmptyRoute(), idx, "slot reusable");
    }

    CASE("removeTrack returns false when not modulated on that track") {
        Project project;
        auto &routing = project.routing();
        expectFalse(RouteDraft::removeTrack(routing, Routing::Target::Transpose, 0), "no-op returns false");
    }

    CASE("canCommit rejects an out-of-range routeIndex") {
        RouteDraft::Draft d;
        d.routeIndex = CONFIG_ROUTE_COUNT;            // out of range (upper)
        d.route.setTarget(Routing::Target::Transpose);
        d.route.setSource(Routing::Source::CvIn1);
        expectFalse(RouteDraft::canCommit(d), "routeIndex >= CONFIG_ROUTE_COUNT -> cannot commit");
    }

    CASE("isTrackModulated guards out-of-range trackIndex for per-track targets") {
        Project project; auto &routing = project.routing();
        expectFalse(RouteDraft::isTrackModulated(routing, Routing::Target::Transpose, CONFIG_TRACK_COUNT),
                    "out-of-range track -> false");
        expectFalse(RouteDraft::isTrackModulated(routing, Routing::Target::Transpose, -1),
                    "negative track -> false");
    }

    CASE("removeTrack guards out-of-range trackIndex for per-track targets") {
        Project project; auto &routing = project.routing();
        expectFalse(RouteDraft::removeTrack(routing, Routing::Target::Transpose, CONFIG_TRACK_COUNT),
                    "out-of-range track -> false");
        expectFalse(RouteDraft::removeTrack(routing, Routing::Target::Transpose, -1),
                    "negative track -> false");
    }

    CASE("findRouteForTarget returns -1 when no route for the target") {
        Project project; auto &routing = project.routing();
        expectEqual(RouteDraft::findRouteForTarget(routing, Routing::Target::Transpose), -1,
                    "no route -> -1");
    }

    CASE("findRouteForTarget finds the param's route regardless of track queried") {
        Project project;
        auto &routing = project.routing();
        int idx;
        { auto c = RouteDraft::create(routing, Routing::Target::Transpose, 2);
          c.route.setSource(Routing::Source::CvIn1);
          RouteDraft::commit(routing, c); idx = c.routeIndex; }
        expectEqual(RouteDraft::findRouteForTarget(routing, Routing::Target::Transpose), idx,
                    "found by target");
    }

    CASE("findRouteForTarget returns -1 for a different target") {
        Project project;
        auto &routing = project.routing();
        { auto c = RouteDraft::create(routing, Routing::Target::Transpose, 2);
          c.route.setSource(Routing::Source::CvIn1);
          RouteDraft::commit(routing, c); }
        expectEqual(RouteDraft::findRouteForTarget(routing, Routing::Target::Octave), -1,
                    "different target -> -1");
    }
}
