#include "UnitTest.h"

#include "apps/sequencer/engine/StochasticGenerator.h"
#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/StochasticTrack.h"
#include "apps/sequencer/model/Scale.h"
#include "core/utils/Random.h"

// Five-step pitch law — see .tasks/stochastic-track-port/PITCH-LAW-FINAL.md.
//   1. Class roll (tickets × recency penalty, blind to motion).
//   2. Class repeat check (Complexity rejects repeats).
//   3. Pitch candidate set (Range bipolar: =50 single octave, >50 multi-octave,
//      <50 single + per-slot jump chance).
//   4. Region roll (Bias + Spread beta-distribution over the sorted set).
//   5. Direction nudge (Contour swaps for directional alternative).

static void setTransparentDefaults(StochasticSequence &seq) {
    seq.clear();
    seq.setRange(50);
    seq.setComplexity(50);
    seq.setMarblesBias(50);
    seq.setMarblesSpread(80);
    seq.setContour(0);
}

UNIT_TEST("StochasticGenerator") {

CASE("tickets_steer_the_class_distribution") {
    StochasticSequence seq;
    setTransparentDefaults(seq);
    // Flat-zero baseline; only classes 0 and 1 get heavy tickets.
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 100);
    seq.setDegreeTicket(1, 100);
    seq.setComplexity(0);     // accept repeats so recency is the only damp

    StochasticTrack track;
    track.clear();

    Random rng(42);
    StochasticGenerator::PitchGenState state{};
    int hit01 = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        int klass = deg % 12;
        if (klass == 0 || klass == 1) hit01++;
    }
    expectTrue(hit01 >= 150, "tickets should dominate the class roll");
}

CASE("recency_penalty_breaks_single_ticket_lock") {
    // Heavy single ticket on class 5. Without the recency penalty the line
    // would lock; with it, run length stays bounded.
    StochasticSequence seq;
    setTransparentDefaults(seq);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 10);
    seq.setDegreeTicket(5, 100);
    seq.setComplexity(0);     // disable Step 2 reject; isolate recency

    StochasticTrack track;
    track.clear();

    Random rng(42);
    StochasticGenerator::PitchGenState state{};
    int runLen = 0;
    int maxRun = 0;
    int last = -1;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        int klass = deg % 12;
        if (klass == last) runLen++;
        else { last = klass; runLen = 0; }
        if (runLen > maxRun) maxRun = runLen;
    }
    expectTrue(maxRun <= 5, "recency penalty should cap class run length");
}

CASE("complexity_high_forces_movement") {
    StochasticSequence seq;
    setTransparentDefaults(seq);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 10);
    seq.setDegreeTicket(3, 100); // bias toward class 3
    seq.setComplexity(100);   // always reject class repeats

    StochasticTrack track;
    track.clear();

    Random rng(42);
    StochasticGenerator::PitchGenState state{};
    int classRepeats = 0;
    int last = -1;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        int klass = deg % 12;
        if (klass == last) classRepeats++;
        last = klass;
    }
    // Bounded retries (3) means a few repeats slip through when all alternatives
    // were re-rolled into the same class — accept a small residual.
    expectTrue(classRepeats <= 25, "high Complexity should suppress class repeats");
}

CASE("complexity_low_keeps_repeats") {
    StochasticSequence seq;
    setTransparentDefaults(seq);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 10);
    seq.setDegreeTicket(7, 100); // bias toward class 7
    seq.setComplexity(0);     // accept all repeats

    StochasticTrack track;
    track.clear();

    Random rng(42);
    StochasticGenerator::PitchGenState state{};
    int hit7 = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        if ((deg % 12) == 7) hit7++;
    }
    // At Complexity=0, recency is the only damp; class 7 still dominates.
    expectTrue(hit7 >= 70, "low Complexity should let the heavy ticket dominate within recency limits");
}

CASE("range_50_is_single_octave_field") {
    // At Range=50 the candidate set is a single pitch per class — no octave
    // decision. Bias/Spread/Contour are inert.
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(50);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(5, 100);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(7);
    StochasticGenerator::PitchGenState state{};
    int octsSeen[8] = {};
    int uniqueOcts = 0;
    for (int i = 0; i < 100; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        int oct = deg / 12;
        if (oct >= 0 && oct < 8 && !octsSeen[oct]) { octsSeen[oct] = 1; uniqueOcts++; }
    }
    expectEqual(uniqueOcts, 1, "Range=50 should produce a single-octave field");
}

CASE("range_high_fans_across_octaves") {
    // Range=100 → 4-octave field. With one heavy class ticket the same class
    // should appear in multiple octaves.
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(100);
    seq.setMarblesSpread(80); // near-uniform across the set
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 100);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(13);
    StochasticGenerator::PitchGenState state{};
    int octsSeen[8] = {};
    int uniqueOcts = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        int oct = deg / 12;
        if (oct >= 0 && oct < 8 && !octsSeen[oct]) { octsSeen[oct] = 1; uniqueOcts++; }
    }
    expectTrue(uniqueOcts >= 3, "Range>50 should fan the same class across multiple octaves");
}

CASE("range_low_produces_octave_jumps") {
    // Range < 50 keeps single octave + periodic jumps. At Range=0, every slot
    // has 100% jump chance, so jumps should appear frequently.
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(0);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 10);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(101);
    StochasticGenerator::PitchGenState state{};
    int octsSeen[8] = {};
    int uniqueOcts = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        int oct = deg / 12;
        if (oct >= 0 && oct < 8 && !octsSeen[oct]) { octsSeen[oct] = 1; uniqueOcts++; }
    }
    expectTrue(uniqueOcts >= 2, "Range<50 should add occasional octave-displacement jumps");
}

CASE("bias_low_favors_low_pitches") {
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(100);              // 4-octave field
    seq.setMarblesBias(0);          // lowest end
    seq.setMarblesSpread(0);        // collapse to bias
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 100);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(55);
    StochasticGenerator::PitchGenState state{};
    int lowCount = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        if (deg / 12 == 0) lowCount++;
    }
    expectTrue(lowCount >= 170, "Bias=0 with Spread=0 should land on the lowest octave");
}

CASE("bias_high_favors_high_pitches") {
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(100);
    seq.setMarblesBias(100);        // highest end
    seq.setMarblesSpread(0);        // collapse to bias
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 100);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(77);
    StochasticGenerator::PitchGenState state{};
    int highCount = 0;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        if (deg / 12 >= 3) highCount++;
    }
    expectTrue(highCount >= 170, "Bias=100 with Spread=0 should land on the highest octave");
}

CASE("contour_nudges_up") {
    // Contour=+100 with a multi-octave field should bias upward motion.
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(100);
    seq.setMarblesBias(50);
    seq.setMarblesSpread(80);
    seq.setContour(100);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 10);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(33);
    StochasticGenerator::PitchGenState state{};
    int upMoves = 0, downMoves = 0;
    int last = -1;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        if (last >= 0) {
            if (deg > last) upMoves++;
            else if (deg < last) downMoves++;
        }
        last = deg;
    }
    expectTrue(upMoves > downMoves * 2, "Contour=+100 should produce dominantly upward motion");
}

CASE("contour_nudges_down") {
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(100);
    seq.setMarblesBias(50);
    seq.setMarblesSpread(80);
    seq.setContour(-100);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 10);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(33);
    StochasticGenerator::PitchGenState state{};
    int upMoves = 0, downMoves = 0;
    int last = -1;
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        if (last >= 0) {
            if (deg > last) upMoves++;
            else if (deg < last) downMoves++;
        }
        last = deg;
    }
    expectTrue(downMoves > upMoves * 2, "Contour=-100 should produce dominantly downward motion");
}

CASE("heavy_ticket_does_not_collapse_to_single_note") {
    // The anti-collapse promise. One class at ticket=100, ALL others at 0
    // (default-flat). The 0 entries must remain pickable so the line stays
    // varied — this is the Item 6 contract.
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(100);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(0, 100);
    seq.setComplexity(50);

    StochasticTrack track;
    track.clear();

    Random rng(101);
    StochasticGenerator::PitchGenState state{};
    int uniqueDegrees = 0;
    int seen[12 * 8] = {};
    for (int i = 0; i < 200; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        if (deg >= 0 && deg < 96 && !seen[deg]) { seen[deg] = 1; uniqueDegrees++; }
    }
    expectTrue(uniqueDegrees >= 8, "heavy ticket should not collapse to a single repeated pitch");
}

CASE("ticket_zero_is_default_flat_not_excluded") {
    // Item 6 contract: 0 means default-flat. Setting one class to 100 and
    // all others to 0 should NOT silence the others — they get base weight 1,
    // so the line still visits them.
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(50);   // single octave so we see class distribution only
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(3, 100);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(99);
    StochasticGenerator::PitchGenState state{};
    int seenClass[12] = {};
    int uniqueClasses = 0;
    for (int i = 0; i < 400; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        int klass = deg % 12;
        if (klass >= 0 && klass < 12 && !seenClass[klass]) { seenClass[klass] = 1; uniqueClasses++; }
    }
    expectTrue(uniqueClasses >= 8, "ticket=0 classes must remain pickable when another class is emphasized");
}

CASE("ticket_minus_one_still_excludes") {
    // -1 must still hard-exclude (the only path to silence a class).
    StochasticSequence seq;
    setTransparentDefaults(seq);
    seq.setRange(50);
    for (int i = 0; i < 32; ++i) seq.setDegreeTicket(i, 0);
    seq.setDegreeTicket(5, -1);
    seq.setDegreeTicket(7, -1);
    seq.setComplexity(0);

    StochasticTrack track;
    track.clear();

    Random rng(123);
    StochasticGenerator::PitchGenState state{};
    int hit5 = 0, hit7 = 0;
    for (int i = 0; i < 400; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        if ((deg % 12) == 5) hit5++;
        if ((deg % 12) == 7) hit7++;
    }
    expectEqual(hit5, 0, "ticket=-1 must hard-exclude class");
    expectEqual(hit7, 0, "ticket=-1 must hard-exclude class");
}

CASE("produces_valid_output_at_all_defaults") {
    StochasticSequence seq;
    seq.clear();   // factory defaults

    StochasticTrack track;
    track.clear();

    Random rng(42);
    StochasticGenerator::PitchGenState state{};
    int outCount = 0;
    for (int i = 0; i < 100; ++i) {
        const auto &scale = Scale::get(0);
        int deg = StochasticGenerator::generateDegree(seq, track, scale, state, rng);
        if (deg >= 0) outCount++;
    }
    expectEqual(outCount, 100, "default config should always produce a valid degree");
}

} // UNIT_TEST("StochasticGenerator")
