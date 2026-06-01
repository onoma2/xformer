#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTableMidiCv.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the MidiCv per-type param table.
// Characterization: each row's applyRouted hook must mutate the MidiCv track
// identically to Routing::writeTarget. Track scope, scope.object = Track* (MidiCv
// mode). Both rows write the routed slot (gated getters); no fan-out.

namespace {

void makeMidiCvTrack0(Project &p) {
    p.clear();
    p.setTrackMode(0, Track::TrackMode::Note);    // force a clean container init
    p.setTrackMode(0, Track::TrackMode::MidiCv);
}

Project &midiCvProject() {
    static Project p;
    makeMidiCvTrack0(p);
    return p;
}

} // namespace

UNIT_TEST("ParamTableMidiCv") {

CASE("rows match writeTarget for every normalized input") {
    struct Spec {
        Routing::Target target;
        uint8_t key;
        float (*read)(Project &);
        const char *name;
    };
    const Spec specs[] = {
        { Routing::Target::Transpose, ParamKey::Transpose, [](Project &p){ return float(p.track(0).midiCvTrack().transpose()); }, "Transpose" },
        { Routing::Target::SlideTime, ParamKey::SlideTime, [](Project &p){ return float(p.track(0).midiCvTrack().slideTime()); }, "SlideTime" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        Routing::setRouted(s.target, 0xFF, true);   // routed-slot writes; expose via getters

        for (float n : inputs) {
            Project &oldP = midiCvProject();
            oldP.routing().writeTarget(s.target, 1 << 0, n);

            static Project newP;
            makeMidiCvTrack0(newP);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &newP.track(0);
            scope.trackIndex = 0;
            bool applied = MidiCvParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, s.name);
            expectEqual(s.read(newP), s.read(oldP), s.name);
        }
    }
}

} // UNIT_TEST
