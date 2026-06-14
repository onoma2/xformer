#include "UnitTest.h"

#include "apps/sequencer/model/Modulator.h"

#include <cstring>

using Param = Modulator::Param;
using Shape = Modulator::Shape;

UNIT_TEST("ModulatorParam") {

CASE("paramCount covers all 15 persisted fields") {
    expectEqual(Modulator::paramCount(), 15, "15 addressable params");
    expectEqual(int(Param::Shape), 0, "shape is address 0");
}

CASE("core addrs roundtrip and agree with named getters") {
    Modulator m; m.clear();
    m.paramSet(int(Param::Depth), 100);
    expectEqual(m.paramGet(int(Param::Depth)), 100, "depth via dict");
    expectEqual(m.depth(), 100, "depth via named getter agrees");

    m.paramSet(int(Param::Shape), int(Shape::SawDown));
    expectEqual(m.paramGet(int(Param::Shape)), int(Shape::SawDown), "shape via dict");
    expectEqual(int(m.shape()), int(Shape::SawDown), "shape named getter agrees");

    m.paramSet(int(Param::Mode), 2);
    expectEqual(m.paramGet(int(Param::Mode)), 2, "mode via dict");

    m.paramSet(int(Param::Offset), -32);
    expectEqual(m.paramGet(int(Param::Offset)), -32, "offset via dict");
    expectEqual(m.offset(), -32, "offset named getter agrees");
}

CASE("long-tail addrs roundtrip via the dictionary") {
    Modulator m; m.clear();
    m.paramSet(int(Param::Attack), 500);
    expectEqual(m.paramGet(int(Param::Attack)), 500, "attack via dict");
    expectEqual(m.attack(), 500, "attack named getter agrees");

    m.paramSet(int(Param::Amplitude), 90);
    expectEqual(m.paramGet(int(Param::Amplitude)), 90, "amplitude via dict");
    expectEqual(m.amplitude(), 90, "amplitude named getter agrees");

    m.paramSet(int(Param::GateSource), int(Routing::Source::CvIn2));
    expectEqual(m.paramGet(int(Param::GateSource)), int(Routing::Source::CvIn2),
                "gateSource ordinal via dict");
    expectEqual(int(m.gateSource()), int(Routing::Source::CvIn2),
                "gateSource named getter agrees");

    m.paramSet(int(Param::Invert), 1);
    expectEqual(m.paramGet(int(Param::Invert)), 1, "invert 1 via dict");
    m.paramSet(int(Param::Invert), 0);
    expectEqual(m.paramGet(int(Param::Invert)), 0, "invert 0 via dict");
}

CASE("paramSet honors each field's own clamp") {
    Modulator m; m.clear();
    m.paramSet(int(Param::Depth), 200);
    expectEqual(m.paramGet(int(Param::Depth)), 127, "depth clamps to 127");
    m.paramSet(int(Param::Sustain), -5);
    expectEqual(m.paramGet(int(Param::Sustain)), 0, "sustain clamps to 0");
}

CASE("paramName is per-shape (Spring relabels)") {
    expectTrue(std::strcmp(Modulator::paramName(int(Param::Rate), Shape::Sine),
                           Modulator::paramName(int(Param::Rate), Shape::Spring)) != 0,
               "RATE label differs for Spring (STRIKE)");
    expectTrue(std::strcmp(Modulator::paramName(int(Param::Shape), Shape::Sine), "SHAPE") == 0,
               "shape label is SHAPE");
}

CASE("out-of-range addr is a safe no-op") {
    Modulator m; m.clear();
    int before = m.paramGet(int(Param::Depth));
    expectEqual(m.paramGet(int(Param::Last)), 0, "get of Last returns 0");
    expectEqual(m.paramGet(99), 0, "get of 99 returns 0");
    m.paramSet(int(Param::Last), 50);
    m.paramSet(99, 50);
    expectEqual(m.paramGet(int(Param::Depth)), before, "set of bad addr changes nothing");
}

}
