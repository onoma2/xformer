#include "UnitTest.h"

#include "apps/sequencer/engine/StochasticGenerator.h"
#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/StochasticTrack.h"
#include "apps/sequencer/model/Scale.h"
#include "core/utils/Random.h"

// Unified pitch picker. Shaping factors: degree tickets, complexity kernel,
// marbles bias/spread. Pitch-centrality (Steps law) is no longer a factor here —
// it acts at trigger time via maskMelody/tiltMelody.

UNIT_TEST("StochasticGenerator") {

CASE("tickets_steer_the_distribution") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(1);

    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    // Two heavy tickets — pick should land mostly on degrees 0 and 1.
    seq.setDegreeTicket(0, 100);
    seq.setDegreeTicket(1, 100);

    // Neutral shaping
    seq.setComplexity(50);
    seq.setMarblesBias(50);
    seq.setMarblesSpread(100);

    StochasticTrack track;
    track.clear();

    Random rng(42);
    int lastDegree = -1;
    int hit01 = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, lastDegree, rng);
        int degInOct = deg % 12;
        if (degInOct == 0 || degInOct == 1) hit01++;
        lastDegree = deg;
    }

    expectTrue(hit01 >= 140, "tickets should dominate (>=70% on weighted degrees)");
}

CASE("marbles_distribution_always_runs_with_transparent_defaults") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(1);

    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 10); // flat

    // Transparent defaults — bias center, spread wide, steps open.
    seq.setMarblesBias(50);
    seq.setMarblesSpread(100);
    seq.setComplexity(50);

    StochasticTrack track;
    track.clear();

    Random rng(42);
    int lastDegree = -1;
    int uniqueDegrees = 0;
    int seen[12] = {};
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, lastDegree, rng);
        int dio = deg % 12;
        if (!seen[dio]) { seen[dio] = 1; uniqueDegrees++; }
    }

    expectTrue(uniqueDegrees >= 5,
               "transparent defaults should produce a wide spread of degrees");
}

CASE("complexity_kernel_narrows_movement_at_low_values") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(2);

    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 10); // flat

    seq.setMarblesBias(50);
    seq.setMarblesSpread(100);
    // Low complexity — kernel tight, picks should cluster near lastDegree
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(42);
    int lastDegree = 7;
    int nearLast = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int prev = lastDegree;
        int deg = StochasticGenerator::generateDegree(seq, track, scale, lastDegree, rng);
        int dist = deg > prev ? deg - prev : prev - deg;
        if (dist <= 2) nearLast++;
        // lastDegree already updated by generateDegree via reference
    }

    expectTrue(nearLast >= 120, "low complexity should keep picks within ~2 scale steps");
}

CASE("produces_valid_output_at_all_defaults") {
    StochasticSequence seq;
    seq.clear();
    seq.setRange(1);

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

    expectEqual(outCount, 100, "default config should always produce a valid degree");
}

} // UNIT_TEST("StochasticGenerator")
