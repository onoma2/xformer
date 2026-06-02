#include "UnitTest.h"

#include "model/Project.h"
#include "model/ParamTableStochastic.h"
#include "model/RouteParamKey.h"

// U4 of the routing mod-matrix overhaul: the Stochastic per-type param table.
// Most rows are characterized against Routing::writeTarget (parity). Two
// intentional non-parity points: Feel (dead-slot fix, tested directly) and
// Scale/RootNote/Divisor (base-write, mirrored for now; defect closes at U6b).
// Track scope, scope.object = Track* (Stochastic mode).

namespace {

void makeStochTrack0(Project &p) {
    p.clear();
    p.setTrackMode(0, Track::TrackMode::Note);    // force a clean container init
    p.setTrackMode(0, Track::TrackMode::Stochastic);
}

Project &stochProject() {
    static Project p;
    makeStochTrack0(p);
    return p;
}

} // namespace

UNIT_TEST("ParamTableStochastic") {

CASE("parity rows match writeTarget for every normalized input") {
    struct Spec {
        Routing::Target target;
        uint8_t key;
        bool routedWrite;            // routed slot (setRouted to read) vs base
        float (*read)(Project &);
        const char *name;
    };
    const Spec specs[] = {
        // track-level routed
        { Routing::Target::Octave,    ParamKey::Octave,          true,  [](Project &p){ return float(p.track(0).stochasticTrack().octave()); },    "Octave" },
        { Routing::Target::Transpose, ParamKey::Transpose,       true,  [](Project &p){ return float(p.track(0).stochasticTrack().transpose()); }, "Transpose" },
        { Routing::Target::SlideTime, ParamKey::SlideTime,       true,  [](Project &p){ return float(p.track(0).stochasticTrack().slideTime()); }, "SlideTime" },
        // sequence-level routed
        { Routing::Target::StochasticComplexity,     ParamKey::Complexity,     true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).complexity()); },     "Complexity" },
        { Routing::Target::StochasticVariation,      ParamKey::Variation,      true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).variation()); },      "Variation" },
        { Routing::Target::StochasticRest,           ParamKey::Rest,           true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).rest()); },           "Rest" },
        { Routing::Target::StochasticSlide,          ParamKey::Slide,          true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).slide()); },          "Slide" },
        { Routing::Target::StochasticBurst,          ParamKey::Burst,          true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).burst()); },          "Burst" },
        { Routing::Target::StochasticSleep,          ParamKey::Sleep,          true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).sleep()); },          "Sleep" },
        { Routing::Target::StochasticMutate,         ParamKey::Mutate,         true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).mutate()); },         "Mutate" },
        { Routing::Target::StochasticJump,           ParamKey::Jump,           true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).jump()); },           "Jump" },
        { Routing::Target::StochasticContour,        ParamKey::Contour,        true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).contour()); },        "Contour" },
        { Routing::Target::StochasticMask,           ParamKey::MaskRhythm,     true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).maskRhythm()); },     "MaskRhythm" },
        { Routing::Target::StochasticTilt,           ParamKey::TiltRhythm,     true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).tiltRhythm()); },     "TiltRhythm" },
        { Routing::Target::StochasticGateLength,     ParamKey::StochasticGateLength, true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).gateLength()); }, "GateLength" },
        { Routing::Target::StochasticPatienceRhythm, ParamKey::PatienceRhythm, true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).patienceRhythm()); }, "Patience" },
        { Routing::Target::StochasticNoteDuration,   ParamKey::NoteDuration,   true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).noteDuration()); },   "NoteDuration" },
        { Routing::Target::StochasticFeel,           ParamKey::Feel,           true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).feel()); },           "Feel" },
        { Routing::Target::Rotate,                   ParamKey::Rotate,         true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).rotate()); },         "Rotate" },
        { Routing::Target::ClockMult,                ParamKey::ClockMultiplier,true, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).clockMultiplier()); }, "ClockMult" },
        // sequence-level base write (defect-parity)
        { Routing::Target::Scale,    ParamKey::Scale,    false, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).scale()); },    "Scale" },
        { Routing::Target::RootNote, ParamKey::RootNote, false, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).rootNote()); }, "RootNote" },
        { Routing::Target::Divisor,  ParamKey::Divisor,  false, [](Project &p){ return float(p.track(0).stochasticTrack().sequence(0).divisor()); },  "Divisor" },
    };
    const float inputs[] = { 0.f, 0.25f, 0.5f, 0.75f, 1.f };

    for (const auto &s : specs) {
        Routing::setRouted(s.target, 0xFF, s.routedWrite);

        for (float n : inputs) {
            Project &oldP = stochProject();
            oldP.routing().writeTarget(s.target, 1 << 0, n);

            static Project newP;
            makeStochTrack0(newP);
            RouteParam::Scope scope;
            scope.kind = RouteParam::Scope::Kind::Track;
            scope.object = &newP.track(0);
            scope.trackIndex = 0;
            bool applied = StochasticParamTable::table().applyRouted(scope, s.key, n);
            expectTrue(applied, s.name);
            expectEqual(s.read(newP), s.read(oldP), s.name);
        }
    }
}

} // UNIT_TEST
