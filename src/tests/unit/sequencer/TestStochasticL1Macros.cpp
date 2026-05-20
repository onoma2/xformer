#include "UnitTest.h"

#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/StochasticTrack.h"

UNIT_TEST("StochasticL1Macros") {

CASE("density_macro_only_writes_density") {
    StochasticSequence seq;
    seq.clear();
    seq.setRest(20);
    seq.setDensity(0);

    expectEqual(seq.density(), 0, "density starts at 0");
    expectEqual(seq.rest(), 20, "rest starts at 20");

    seq.editDensityMacro(30, false);

    expectEqual(seq.density(), 30, "density should be 30 after +30 macro");
    expectEqual(seq.rest(), 20, "rest should remain 20 (unchanged by macro)");
}

CASE("complexity_macro_only_writes_complexity") {
    StochasticSequence seq;
    seq.clear();
    seq.setContour(50);
    seq.setLinearity(70);
    seq.setComplexity(50);

    expectEqual(seq.complexity(), 50, "complexity starts at 50");
    expectEqual(seq.contour(), 50, "contour starts at 50");
    expectEqual(seq.linearity(), 70, "linearity starts at 70");

    seq.editComplexityMacro(10, false);

    expectEqual(seq.complexity(), 60, "complexity should be 60 after +10 macro");
    expectEqual(seq.contour(), 50, "contour should remain 50 (unchanged by macro)");
    expectEqual(seq.linearity(), 70, "linearity should remain 70 (unchanged by macro)");
}

} // UNIT_TEST("StochasticL1Macros")
