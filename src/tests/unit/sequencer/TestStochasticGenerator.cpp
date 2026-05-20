#include "UnitTest.h"

#include "apps/sequencer/engine/StochasticGenerator.h"
#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/StochasticTrack.h"
#include "apps/sequencer/model/Scale.h"
#include "core/utils/Random.h"

UNIT_TEST("StochasticGenerator") {

CASE("with_tickets_active_complexity_does_not_modify_selection") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(1);
    seq.setMinDegree(0);
    seq.setMaxDegree(127);

    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 50);
    seq.setDegreeTicket(1, 50);

    seq.setComplexity(1);
    seq.setContour(0);
    seq.setLinearity(100);

    StochasticTrack track;
    track.clear();

    Random rng(42);
    int lastDegree = 0;
    int degree0Count = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, lastDegree, rng);
        if (deg % 12 == 0) degree0Count++;
        lastDegree = deg;
    }

    expectTrue(degree0Count >= 70 && degree0Count <= 130,
               "tickets should give ~50-50 split between degree 0 and 1 (not heavily biased by linearity)");
}

CASE("with_tickets_active_marbles_is_bypassed") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(1);
    seq.setMinDegree(0);
    seq.setMaxDegree(127);

    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(3, 100);

    seq.setMarblesMode(MarblesMode::On);
    seq.setMarblesSpread(0);
    seq.setMarblesBias(0);

    StochasticTrack track;
    track.clear();

    Random rng(42);
    int lastDegree = -1;
    int degree3Count = 0;
    for (int i = 0; i < 100; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, lastDegree, rng);
        if (deg % 12 == 3) degree3Count++;
    }

    expectEqual(degree3Count, 100, "tickets should force degree 3 every time (bypassing marbles bias)");
}

CASE("with_tickets_marbles_still_bypasses_complexity") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(1);
    seq.setMinDegree(0);
    seq.setMaxDegree(127);

    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(3, 100);

    seq.setMarblesMode(MarblesMode::On);
    seq.setMarblesSpread(0);
    seq.setMarblesBias(0);
    seq.setComplexity(1);
    seq.setContour(0);
    seq.setLinearity(100);

    StochasticTrack track;
    track.clear();

    Random rng(42);
    int lastDegree = 0;
    int degree3Count = 0;
    for (int i = 0; i < 100; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, lastDegree, rng);
        if (deg % 12 == 3) degree3Count++;
        lastDegree = deg;
    }

    expectEqual(degree3Count, 100, "tickets bypass marbles, which bypasses complexity");
}

CASE("with_no_tickets_marbles_produces_output") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(1);
    seq.setMinDegree(0);
    seq.setMaxDegree(127);

    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);

    seq.setMarblesMode(MarblesMode::On);
    seq.setMarblesSpread(50);
    seq.setMarblesBias(50);

    StochasticTrack track;
    track.clear();

    Random rng(42);
    int lastDegree = -1;
    int outCount = 0;
    for (int i = 0; i < 100; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, lastDegree, rng);
        if (deg >= 0) outCount++;
    }

    expectEqual(outCount, 100, "marbles path should produce valid degrees every time");
}

CASE("with_no_tickets_no_marbles_complexity_produces_output") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(1);
    seq.setMinDegree(0);
    seq.setMaxDegree(127);

    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);

    seq.setComplexity(50);
    seq.setContour(0);
    seq.setLinearity(0);
    seq.setMarblesMode(MarblesMode::Off);

    StochasticTrack track;
    track.clear();

    Random rng(42);
    int lastDegree = -1;
    int outCount = 0;
    for (int i = 0; i < 100; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, lastDegree, rng);
        if (deg >= 0) outCount++;
    }

    expectEqual(outCount, 100, "complexity path should produce valid degrees every time");
}

} // UNIT_TEST("StochasticGenerator")
