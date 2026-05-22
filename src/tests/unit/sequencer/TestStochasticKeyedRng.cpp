// Phase 14B foundational invariant test.
//
// Proves three properties of the keyed-RNG approach in apps/sequencer/engine/
// KeyedRng.h. These three together are what makes seed+log viable: cell N's
// output depends only on (seed, N, knobs), not on stream-consumption order.
//
//   1. Determinism — same (seed, idx, knobs) -> bit-identical cell output.
//   2. Order independence — generating cells in any order yields the same
//      per-cell outputs.
//   3. Mutation stability — applying REGEN_CELL to one cell index changes
//      that cell's output AND leaves every other cell's output untouched.

#include "UnitTest.h"

#include "apps/sequencer/engine/KeyedRng.h"
#include "core/utils/Random.h"

#include <cstdint>
#include <cstring>

namespace {

// Mock per-cell generator. Mirrors the shape of generateRhythmEvent: a handful
// of draws against a single Random instance, conditional branches that consume
// a variable number of draws. The test only cares that two calls with the same
// (seed, idx, knobs) produce identical Cells, regardless of call order.
struct Cell {
    int  durationIndex;
    bool rest;
    bool legato;
    bool slide;
    int  childCount;
    int  burstSpacingSlot;

    bool operator==(const Cell &o) const {
        return durationIndex == o.durationIndex
            && rest == o.rest
            && legato == o.legato
            && slide == o.slide
            && childCount == o.childCount
            && burstSpacingSlot == o.burstSpacingSlot;
    }
};

struct Knobs {
    int restProb;      // 0..100
    int legatoProb;
    int slideProb;
    int burstProb;
    int burstCountKnob;
    int burstRateKnob;
};

// Tiny imitation of pickFromLutTriangular — just enough to consume variable
// draws so the test exercises burst-conditional RNG consumption like the real
// generator.
int pickWeighted(const int *lut, int lutSize, int knob, Random &rng) {
    int weights[8];
    int total = 0;
    for (int i = 0; i < lutSize; ++i) {
        int center = (i * 100) / (lutSize - 1);
        int dist = knob > center ? knob - center : center - knob;
        int w = 50 - dist;
        if (w < 0) w = 0;
        weights[i] = w;
        total += w;
    }
    if (total <= 0) {
        return lut[rng.nextRange(uint32_t(lutSize))];
    }
    int roll = int(rng.nextRange(uint32_t(total)));
    int sum = 0;
    for (int i = 0; i < lutSize; ++i) {
        sum += weights[i];
        if (roll < sum) return lut[i];
    }
    return lut[lutSize - 1];
}

Cell generateMockCell(uint32_t baseSeed, uint32_t cellIdx, uint32_t mutationGen, const Knobs &k) {
    Random rng = Random(keyed_rng::cellSeedAfterRegen(baseSeed, cellIdx, mutationGen));

    Cell c;
    // Match real generator's draw pattern: duration, rest, legato, slide,
    // burst-gate, then optional burst-count + burst-spacing.
    c.durationIndex = int(rng.nextRange(8));
    c.rest          = int(rng.nextRange(100)) < k.restProb;
    c.legato        = int(rng.nextRange(100)) < k.legatoProb;
    c.slide         = int(rng.nextRange(100)) < k.slideProb;

    bool burstFires = int(rng.nextRange(100)) < k.burstProb;
    c.childCount       = 0;
    c.burstSpacingSlot = 0;
    if (burstFires) {
        static const int kCountLut[]   = { 2, 3, 4, 5 };
        static const int kSpacingLut[] = { 2, 3, 4, 5, 6 };
        c.childCount       = pickWeighted(kCountLut,   4, k.burstCountKnob, rng);
        c.burstSpacingSlot = pickWeighted(kSpacingLut, 5, k.burstRateKnob,  rng);
    }
    return c;
}

const Knobs kDefaultKnobs = { 30, 25, 15, 40, 50, 50 };
const uint32_t kSeed = 0xdeadbeef;
const int kCellCount = 32;

} // anonymous namespace


UNIT_TEST("StochasticKeyedRng") {

CASE("hash_is_deterministic") {
    // mix32 and cellSeed are pure functions — calling them twice with the same
    // input must produce the same output. Trivial but worth one assertion.
    expectEqual(keyed_rng::mix32(0xdeadbeef), keyed_rng::mix32(0xdeadbeef));
    expectEqual(keyed_rng::cellSeed(kSeed, 7), keyed_rng::cellSeed(kSeed, 7));
}

CASE("adjacent_cells_get_different_seeds") {
    // Golden-ratio multiplier scatters neighbours — cells 0,1,2,... must not
    // produce neighbouring RNG states. Test ten adjacent pairs.
    for (uint32_t idx = 0; idx < 10; ++idx) {
        uint32_t a = keyed_rng::cellSeed(kSeed, idx);
        uint32_t b = keyed_rng::cellSeed(kSeed, idx + 1);
        expectTrue(a != b, "adjacent cell seeds collide");
        // Stronger: differ in many bits. Hamming distance > 8 is loose but
        // catches degenerate mixers (e.g. dropping the mix32 step would give
        // tiny deltas).
        uint32_t x = a ^ b;
        int bits = 0;
        while (x) { bits += x & 1; x >>= 1; }
        expectTrue(bits > 8, "adjacent seeds too similar — mixer is weak");
    }
}

CASE("determinism_same_inputs_same_output") {
    // Property 1: generateMockCell is pure with respect to (seed, idx, knobs).
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        Cell a = generateMockCell(kSeed, idx, 0, kDefaultKnobs);
        Cell b = generateMockCell(kSeed, idx, 0, kDefaultKnobs);
        expectTrue(a == b, "same (seed,idx,knobs) gave different outputs");
    }
}

CASE("order_independence_forward_vs_reverse") {
    // Property 2: generating cells in any order must produce the same per-cell
    // outputs. Generate forward (0..N-1), then reverse (N-1..0), then a
    // shuffled permutation, and confirm every cell matches.
    Cell forward[kCellCount];
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        forward[idx] = generateMockCell(kSeed, idx, 0, kDefaultKnobs);
    }

    Cell reverse[kCellCount];
    for (int idx = kCellCount - 1; idx >= 0; --idx) {
        reverse[idx] = generateMockCell(kSeed, uint32_t(idx), 0, kDefaultKnobs);
    }
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        expectTrue(forward[idx] == reverse[idx], "reverse-order generation diverged");
    }

    // Pseudo-random order: idx * 7 mod kCellCount visits every index once for
    // any prime relatively-coprime to kCellCount; 7 is coprime to 32.
    Cell shuffled[kCellCount];
    for (uint32_t k = 0; k < kCellCount; ++k) {
        uint32_t idx = (k * 7u) % uint32_t(kCellCount);
        shuffled[idx] = generateMockCell(kSeed, idx, 0, kDefaultKnobs);
    }
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        expectTrue(forward[idx] == shuffled[idx], "shuffled-order generation diverged");
    }
}

CASE("mutation_isolates_to_target_cell") {
    // Property 3: REGEN_CELL on cell N changes cell N's output and leaves
    // every other cell untouched. This is the foundational invariant that
    // makes the mutation log viable without RNG stream replay.
    Cell baseline[kCellCount];
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        baseline[idx] = generateMockCell(kSeed, idx, 0, kDefaultKnobs);
    }

    const uint32_t kTarget = 17;
    Cell regenerated = generateMockCell(kSeed, kTarget, 1, kDefaultKnobs);

    // Cell N's output changed (with overwhelming probability — collision is
    // possible but rare; in 0xdeadbeef the test passes deterministically).
    expectTrue(!(regenerated == baseline[kTarget]),
               "REGEN_CELL produced an identical cell — mutationGen had no effect");

    // Every other cell unchanged.
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        if (idx == kTarget) continue;
        Cell c = generateMockCell(kSeed, idx, 0, kDefaultKnobs);
        expectTrue(c == baseline[idx], "REGEN_CELL leaked into another cell");
    }
}

CASE("multiple_regens_keep_progressing") {
    // mutationGen 0,1,2,3 produce a chain of distinct outputs for the same
    // cell. Catches accidental modular collisions in the mixer where two
    // mutationGens fold to the same seed.
    Cell prior = generateMockCell(kSeed, 5, 0, kDefaultKnobs);
    int distinctCount = 0;
    for (uint32_t gen = 1; gen <= 8; ++gen) {
        Cell next = generateMockCell(kSeed, 5, gen, kDefaultKnobs);
        if (!(next == prior)) ++distinctCount;
        prior = next;
    }
    // 8 successive regens must produce many distinct cells (occasional
    // duplicates are possible but most should differ). Threshold 5 is loose.
    expectTrue(distinctCount >= 5, "successive REGEN_CELLs produced too few distinct cells");
}

CASE("seed_change_propagates_to_all_cells") {
    // Changing the base seed must shift cell outputs broadly. If only a few
    // cells changed, the mixer is over-reliant on the cellIdx and the seed
    // would be functionally ignored.
    Cell a[kCellCount];
    Cell b[kCellCount];
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        a[idx] = generateMockCell(kSeed,          idx, 0, kDefaultKnobs);
        b[idx] = generateMockCell(kSeed ^ 0x55aa, idx, 0, kDefaultKnobs);
    }
    int changedCount = 0;
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        if (!(a[idx] == b[idx])) ++changedCount;
    }
    // Of 32 cells, most should change under a different seed.
    expectTrue(changedCount > kCellCount * 2 / 3,
               "seed change barely affected output — mixer too weak");
}

CASE("knobs_affect_output_at_fixed_seed") {
    // Different knobs at the same (seed, idx) must produce different cells.
    // Sanity check that the generator actually reads knobs and isn't just
    // returning seed-derived noise.
    Knobs hot  = { 90, 90, 90, 90, 90, 90 };
    Knobs cold = {  5,  5,  5,  5,  5,  5 };
    int divergent = 0;
    for (uint32_t idx = 0; idx < kCellCount; ++idx) {
        Cell h = generateMockCell(kSeed, idx, 0, hot);
        Cell c = generateMockCell(kSeed, idx, 0, cold);
        if (!(h == c)) ++divergent;
    }
    // Hot vs cold knobs should diverge for most cells.
    expectTrue(divergent > kCellCount * 2 / 3,
               "knobs barely affected output — generator decoupled from knob inputs");
}

} // UNIT_TEST("StochasticKeyedRng")
