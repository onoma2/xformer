#include "UnitTest.h"

#include "apps/sequencer/model/Routing.h"
#include "apps/sequencer/model/RouteApply.h"

// Slice 6 model half: the value pipeline's persisted per-route controls.
//   combine     — Modulate (centered, neutral-at-center) vs Absolute (sweep-from-base)
//   scaleSource — a Source whose value scales the shaped source (R6, replaces VcaNext)
// (d is the existing per-track _depthPct.) clear()/==/serialize must carry them.

UNIT_TEST("RouteCombineScaleSource") {

CASE("clear defaults: Modulate combine, no scaleSource") {
    Routing::Route r;
    r.clear();
    expectEqual(int(r.combine()), int(RouteApply::Combine::Modulate), "default Modulate");
    expectEqual(int(r.scaleSource()), int(Routing::Source::None), "default scaleSource None");
}

CASE("set/get combine + scaleSource") {
    Routing::Route r;
    r.clear();
    r.setCombine(RouteApply::Combine::Absolute);
    expectEqual(int(r.combine()), int(RouteApply::Combine::Absolute), "set Absolute");
    r.setScaleSource(Routing::Source::CvIn2);
    expectEqual(int(r.scaleSource()), int(Routing::Source::CvIn2), "set scaleSource");
}

CASE("operator== is sensitive to combine") {
    Routing::Route a; a.clear();
    Routing::Route b; b.clear();
    expectTrue(a == b, "cleared routes equal");
    b.setCombine(RouteApply::Combine::Absolute);
    expectFalse(a == b, "combine difference breaks equality");
}

CASE("operator== is sensitive to scaleSource") {
    Routing::Route a; a.clear();
    Routing::Route b; b.clear();
    b.setScaleSource(Routing::Source::CvIn1);
    expectFalse(a == b, "scaleSource difference breaks equality");
}

} // UNIT_TEST
