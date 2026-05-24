// Phase 14B Patch A — engine cache parity test.
//
// Proves that stochastic_cache::regenerateCacheFromEvents produces a cell
// stream matching today's playback semantics for the rhythm domain. CV
// resolution is deferred to Patch B; this test only validates timing,
// gate shape, burst-child expansion, flag propagation, window respect,
// and deterministic rank assignment.

#include "UnitTest.h"

#include "apps/sequencer/engine/StochasticCache.h"
#include "apps/sequencer/engine/StochasticGenerator.h"
#include "apps/sequencer/engine/StochasticTrackEngine.h"
#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/StochasticTrack.h"
#include "apps/sequencer/model/StochasticTypes.h"
#include "apps/sequencer/model/Scale.h"

#include <cstdint>

using namespace stochastic_cache;

namespace {

// Reproduces evaluateBurst's spacing math for the parity check. Kept local
// so we're not silently agreeing with the cache implementation by sharing the
// same constants — the duplication is intentional.
static const int kSpacingLutForTest[] = { 2, 3, 4, 5, 6 };

int expectedChildOffsets(uint32_t parentTicks, int childCount, int spacingSlot, uint32_t out[5]) {
    constexpr uint32_t kMinBurst = 96;
    constexpr uint32_t kMinChildGate = 6;
    if (parentTicks < kMinBurst) return 0;
    if (spacingSlot < 0) spacingSlot = 0;
    if (spacingSlot >= 5) spacingSlot = 4;

    float spacing = float(parentTicks) / float(kSpacingLutForTest[spacingSlot]);
    if (spacing < float(kMinChildGate + 1)) spacing = float(kMinChildGate + 1);

    int emitted = 0;
    for (int c = 0; c < childCount && c < 5; ++c) {
        uint32_t off = uint32_t((c + 1) * spacing);
        uint32_t g   = uint32_t(spacing * 0.5f);
        if (g < kMinChildGate) g = kMinChildGate;
        if (off + g >= parentTicks) {
            if (off + kMinChildGate < parentTicks) {
                /* trimmed gate is still audible */
            } else {
                break;
            }
        }
        out[emitted++] = off;
    }
    return emitted;
}

void clearAllEvents(StochasticSequence &seq) {
    for (int i = 0; i < int(seq.events().size()); ++i) {
        seq.events()[i].clear();
    }
}

} // anonymous namespace


UNIT_TEST("StochasticCacheParity") {

CASE("cell_pack_roundtrip") {
    // Verify the bit-packed cell encoding survives a roundtrip for the full
    // useful value range. This is the load-bearing invariant of the 4-byte
    // pack — if it fails, nothing else in this test means anything.
    //
    // Phase 16 P6 (2026-05-23): the 12-bit slot now stores per-cell duration
    // (was absolute cycle position). Same bit width, new semantics.
    for (uint32_t dur : { 1u, 48u, 96u, 768u, 2047u, 4095u }) {
        for (uint8_t deg : { uint8_t(0), uint8_t(1), uint8_t(63), uint8_t(127) }) {
            for (uint8_t oct : { uint8_t(0), uint8_t(1), uint8_t(7), uint8_t(15) }) {
                for (uint8_t g : { uint8_t(0), uint8_t(31), uint8_t(63) }) {
                    for (bool slide : { false, true }) {
                        for (bool legato : { false, true }) {
                            for (bool audible : { false, true }) {
                                auto c = CachedCell::make(dur, deg, oct, g, slide, legato, audible);
                                expectEqual(uint32_t(c.durationTicks()), dur);
                                expectEqual(int(c.degree()), int(deg));
                                expectEqual(int(c.octave()), int(oct));
                                expectEqual(int(c.gateLen()), int(g));
                                expectEqual(c.slide(), slide);
                                expectEqual(c.legato(), legato);
                                expectEqual(c.audible(), audible);
                            }
                        }
                    }
                }
            }
        }
    }
}

CASE("cell_duration_clamps_at_kMaxCellDuration") {
    // Phase 16 P6 (2026-05-23): individual cell duration is capped by the
    // 12-bit field width. CachedCell::make must clamp gracefully, not wrap.
    auto c = CachedCell::make(8000, 0, 0, 32, false, false, true);
    expectEqual(int(c.durationTicks()), int(kMaxCellDuration));
}

CASE("empty_sequence_yields_empty_cache") {
    StochasticSequence seq;
    seq.clear();
    seq.setSize(2);                  // setSize clamps min to 2 (after Last collapsed into Size)
    seq.setFirst(0);
    clearAllEvents(seq);
    // Two slots, both cleared. With rest=false by default both cells are audible.

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, /*divisor*/ 48, /*seed*/ 0xdeadbeef);
    expectEqual(n, 2);
    expectEqual(int(cache.count), 2);
    expectTrue(cache.cells[0].audible(), "default cleared event has rest=false → audible");
}

CASE("cache_stores_event_degree_and_octave") {
    // Patch C step 1 (2026-05-22): the cell's pitch fields must match the
    // source event's degree/octave so the engine's scale lookup at trigger
    // time produces the same audible CV whether reading the cache or the
    // legacy event tape.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setSize(4);

    // StochasticSourceEvent stores octave as 5-bit unsigned (0..31); the
    // engine layers _jumpRegister + track.octave() at trigger time for the
    // signed offset case. Match that range here.
    seq.events()[0].setDegree(0);  seq.events()[0].setOctave(0);  seq.events()[0].setRhythmValid(true); seq.events()[0].setRest(false);
    seq.events()[1].setDegree(7);  seq.events()[1].setOctave(1);  seq.events()[1].setRhythmValid(true); seq.events()[1].setRest(false);
    seq.events()[2].setDegree(11); seq.events()[2].setOctave(2);  seq.events()[2].setRhythmValid(true); seq.events()[2].setRest(false);
    seq.events()[3].setDegree(64); seq.events()[3].setOctave(3);  seq.events()[3].setRhythmValid(true); seq.events()[3].setRest(false);

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0xfeed);

    for (int slot = 0; slot < 4; ++slot) {
        uint8_t cIdx = cache.parentCacheIdx[slot];
        expectEqual(int(cache.cells[cIdx].degree()), int(seq.events()[slot].degree()),
                    "cache cell must store same degree as source event");
        expectEqual(int(cache.cells[cIdx].octave()), int(seq.events()[slot].octave()),
                    "cache cell must store same octave as source event");
    }
}

CASE("single_parent_event_relTick_and_gateLen") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(2);                  // setSize min is 2 (Last collapsed into Size)
    seq.setFirst(0);

    auto &ev = seq.events()[0];
    ev.setDurationIndex(5);  // ×1 of divisor
    ev.setRest(false);
    ev.setRhythmValid(true);
    // Slide / Legato bits on the event are dead under the cache-owned
    // contract (2026-05-24) — cache picks both per cell from cellRng,
    // gated by sequence.slide() / sequence.legatoProb() knobs. Both knobs
    // default to 0, so cache picks come out false regardless of the event.
    ev.setSlide(true);     // intentionally set, must NOT propagate
    ev.setLegato(true);    // intentionally set, must NOT propagate
    seq.setSlide(0);
    seq.setLegatoProb(0);

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, /*divisor*/ 48, /*seed*/ 0x1234);
    expectEqual(n, 2);
    expectEqual(int(cache.cells[0].durationTicks()), 48);
    expectEqual(int(cache.cells[0].gateLen()), 32);
    expectFalse(cache.cells[0].slide(),  "slide is cache-picked, not propagated from event");
    expectFalse(cache.cells[0].legato(), "legato is cache-picked, not propagated from event");
    expectTrue(cache.cells[0].audible(), "non-rest event is audible");
}

CASE("rest_event_audible_false") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(2);
    seq.setFirst(0);

    auto &ev = seq.events()[0];
    ev.setDurationIndex(5);
    ev.setRest(true);
    ev.setRhythmValid(true);

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0);
    expectEqual(int(cache.count), 2);
    expectFalse(cache.cells[0].audible(), "rest event must be marked non-audible");
}

CASE("slot_keyed_cache_writes_full_size_extent_regardless_of_first") {
    // Slot-keyed cache (2026-05-24): cell content is keyed by slot index,
    // not by walk position. First does not affect what gets written — it
    // bounds playback at the engine, not the cache build. Cache always
    // covers 0..size-1.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(5);
    seq.setFirst(2);
    seq.setRhythmMode(StochasticSourceMode::Loop);
    seq.setNoteDuration(5);
    seq.setVariation(0);
    seq.setBurst(0);

    for (int i = 0; i < int(seq.events().size()); ++i) {
        auto &ev = seq.events()[i];
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, 48, 0);
    expectEqual(n, 5);  // all slots 0..4 written
    for (int i = 0; i < 5; ++i) {
        expectEqual(int(cache.cells[i].durationTicks()), 48);
        expectEqual(int(cache.parentCacheIdx[i]), i);
    }
    expectEqual(int(cache.cycleTicks), 240);
}

CASE("slot_keyed_cache_is_first_independent") {
    // Same seed + same events + same Size → identical cells regardless of First.
    StochasticSequence seqA;
    StochasticSequence seqB;
    seqA.clear(); seqB.clear();
    clearAllEvents(seqA); clearAllEvents(seqB);
    seqA.setSize(8); seqB.setSize(8);
    seqA.setFirst(0); seqB.setFirst(3);     // different windows
    seqA.setRhythmMode(StochasticSourceMode::Loop);
    seqB.setRhythmMode(StochasticSourceMode::Loop);
    seqA.setNoteDuration(50); seqB.setNoteDuration(50);
    seqA.setVariation(40); seqB.setVariation(40);
    seqA.setBurst(30); seqB.setBurst(30);

    for (int i = 0; i < int(seqA.events().size()); ++i) {
        seqA.events()[i].setRest(false); seqA.events()[i].setRhythmValid(true);
        seqB.events()[i].setRest(false); seqB.events()[i].setRhythmValid(true);
    }

    Cache cacheA{};
    Cache cacheB{};
    regenerateCacheFromEvents(cacheA, seqA, 48, 0xcafe);
    regenerateCacheFromEvents(cacheB, seqB, 48, 0xcafe);

    for (int i = 0; i < int(cacheA.count); ++i) {
        expectEqual(int(cacheA.cells[i].durationTicks()),
                    int(cacheB.cells[i].durationTicks()));
    }
}

CASE("burst_knob_at_100_produces_cluster_cells") {
    // Phase 16 P7 (2026-05-23): cluster cells are driven by sequence.burst()
    // rolling per-cell at cache build, not by event.childCount written at
    // mutate time. With burst=100, every cell tries to start a cluster.
    // Variation locked to 0 so durations are deterministic ×1 LUT slots,
    // making the cluster cells (shorter than the LUT pick) easy to identify.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);
    seq.setSize(8);
    seq.setRhythmMode(StochasticSourceMode::Loop);   // exercise the cache-pick path
    seq.setNoteDuration(1);    // ×4 = 192 ticks at divisor=48
    seq.setVariation(0);
    seq.setBurst(100);
    seq.setBurstCount(20);     // bias the LUT pick low (count=2 region)
    seq.setBurstRate(60);      // mid-range denom

    for (int i = 0; i < 8; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, 48, /*seed*/ 0xdead);
    expectEqual(n, 8);

    // First cell sets the baseline duration. At least one of the remaining
    // cells must be shorter — that's the cluster signature.
    const uint32_t baselineDur = cache.cells[0].durationTicks();
    bool sawShorterCell = false;
    for (int i = 1; i < int(cache.count); ++i) {
        if (cache.cells[i].durationTicks() < baselineDur) {
            sawShorterCell = true;
            break;
        }
    }
    expectTrue(sawShorterCell,
               "burst=100 must produce at least one shorter cluster cell");
}

CASE("burst_knob_at_zero_produces_no_cluster_cells") {
    // burst=0 means the per-cell roll never lands → no cluster cells.
    // Every cell must equal the noteDuration pick (variation=0 locks it).
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);
    seq.setSize(8);
    seq.setRhythmMode(StochasticSourceMode::Loop);
    seq.setNoteDuration(3);    // ×2 = 96 ticks at divisor=48
    seq.setVariation(0);
    seq.setBurst(0);

    for (int i = 0; i < 8; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0xfeed);
    for (int i = 0; i < int(cache.count); ++i) {
        expectEqual(int(cache.cells[i].durationTicks()), 96,
                    "burst=0 → every cell at noteDuration's natural LUT pick");
    }
}

CASE("cluster_cell_duration_never_below_minimum") {
    // The cluster eligibility check (prev_dur / denom >= kMinChildGate)
    // suppresses clusters that would produce sub-audible cells. Drive a
    // short noteDuration + high burst rate denominator and confirm no
    // cluster cell falls below the minimum floor.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);
    seq.setSize(8);
    seq.setRhythmMode(StochasticSourceMode::Loop);
    seq.setNoteDuration(7);    // ×1/2 = shortest natural slot, ~24 ticks at divisor=48
    seq.setVariation(0);
    seq.setBurst(100);
    seq.setBurstRate(100);     // bias toward largest denominator (denser → shorter cluster cells)

    for (int i = 0; i < 8; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0xbaadf00d);
    constexpr uint32_t kMinCellTicks = 6;
    for (int i = 0; i < int(cache.count); ++i) {
        expectTrue(cache.cells[i].durationTicks() >= kMinCellTicks,
                   "no cell may fall below the audible floor");
    }
}

CASE("rank_is_deterministic_per_seed") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);

    for (int i = 0; i < 8; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(i);  // 0..7 spans the LUT
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    // Two runs with same seed must produce identical event ranks.
    StochasticGenerator::generateMaskRanks(seq, 8, 0xaabbccdd);
    int firstPass[8];
    for (int i = 0; i < 8; ++i) firstPass[i] = seq.events()[i].densityRank();

    StochasticGenerator::generateMaskRanks(seq, 8, 0xaabbccdd);
    for (int i = 0; i < 8; ++i) {
        expectEqual(int(seq.events()[i].densityRank()), firstPass[i], "same seed → same rank");
    }
}

CASE("rank_changes_when_seed_changes") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);

    // Use equal durations across all events so the only sort-discriminator is
    // the random jitter from seed. Different seeds → different tie-break order
    // → ranks differ.
    for (int i = 0; i < 8; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(3);  // all events same duration
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    StochasticGenerator::generateMaskRanks(seq, 8, 0xaabbccdd);
    int ranksA[8];
    for (int i = 0; i < 8; ++i) ranksA[i] = seq.events()[i].densityRank();

    StochasticGenerator::generateMaskRanks(seq, 8, 0x12345678);
    int divergent = 0;
    for (int i = 0; i < 8; ++i) {
        if (seq.events()[i].densityRank() != ranksA[i]) ++divergent;
    }
    expectTrue(divergent >= 4, "seed change should reshuffle most ranks (equal-duration tie-break)");
}

CASE("mask_rank_long_event_gets_rank_zero") {
    // 2026-05-24: ranks are assigned by generateMaskRanks at RENEW / Patience /
    // mutateRhythmOne. Sort is Tilt-free — longest event by durationIndex gets
    // rank 0, shortest gets rank size-1. Tilt is applied at trigger time only,
    // not at rank assignment.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);
    for (int i = 0; i < 8; ++i) {
        // durationIndex 0 = longest LUT slot (×8), 7 = shortest (×1/2). So
        // event[0] is the longest, event[7] is the shortest.
        seq.events()[i].setDurationIndex(i);
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }

    StochasticGenerator::generateMaskRanks(seq, 8, 0xaabbccdd);

    int rankLong  = seq.events()[0].densityRank();
    int rankShort = seq.events()[7].densityRank();
    expectTrue(rankLong < rankShort, "longest event gets a low rank (survives Mask cut by default)");
}

CASE("mask_rank_is_tilt_independent") {
    // Rank assignment must NOT use Tilt — the value is now applied at trigger
    // time only. Running generateMaskRanks twice with the seed but with the
    // sequence's Tilt set to opposite values must produce identical ranks.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);
    for (int i = 0; i < 8; ++i) {
        seq.events()[i].setDurationIndex(i);
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }

    seq.setTilt(100);
    StochasticGenerator::generateMaskRanks(seq, 8, 0xaabbccdd);
    int ranksAtPlusTilt[8];
    for (int i = 0; i < 8; ++i) ranksAtPlusTilt[i] = seq.events()[i].densityRank();

    seq.setTilt(-100);
    StochasticGenerator::generateMaskRanks(seq, 8, 0xaabbccdd);
    for (int i = 0; i < 8; ++i) {
        expectEqual(seq.events()[i].densityRank(), ranksAtPlusTilt[i],
                    "rank stays identical regardless of Tilt value");
    }
}

CASE("gate_length_zero_locks_gate_at_50_percent") {
    // Phase 16 P8 (2026-05-23): cache bakes per-cell gateFrac from
    // sequence.gateLength() via keyed RNG. gateLength=0 → pct = 50 exactly
    // → gateFrac = 32 (50% of cell duration in 64ths) on every cell.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);
    seq.setSize(8);
    seq.setNoteDuration(5);
    seq.setVariation(0);
    seq.setBurst(0);
    seq.setGateLength(0);   // tight gate kernel
    for (int i = 0; i < 8; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }
    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0xfeed);
    for (int i = 0; i < int(cache.count); ++i) {
        expectEqual(int(cache.cells[i].gateLen()), 32,
                    "gateLength=0 locks every cell to 50% gate fraction (32/64)");
    }
}

CASE("gate_length_high_varies_gate_per_cell") {
    // gateLength=100 widens the triangular kernel to its full 10..100% range.
    // Two cells with different keyed RNG must produce visibly different
    // gate fractions (occasionally the picker hits the same value, but a
    // batch of 16 cells must not all coincide).
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(16);
    seq.setFirst(0);
    seq.setSize(16);
    seq.setNoteDuration(5);
    seq.setVariation(0);
    seq.setBurst(0);
    seq.setGateLength(100);
    for (int i = 0; i < 16; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }
    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0xaaaa);
    int distinctValues = 0;
    uint8_t seen[64] = { 0 };
    for (int i = 0; i < int(cache.count); ++i) {
        const uint8_t g = cache.cells[i].gateLen();
        if (g < 64 && !seen[g]) { seen[g] = 1; ++distinctValues; }
    }
    expectTrue(distinctValues >= 3,
               "gateLength=100 must produce at least 3 distinct gate fractions across 16 cells");
}

CASE("burst_pitch_hold_keeps_parent_pitch_in_cluster_tail") {
    // BurstHold == Hold: cluster tail cells carry the same degree/octave
    // as the cluster starter (which itself uses the source event's pitch).
    const Scale &scale = Scale::get(0);
    StochasticTrack track;
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(4);
    seq.setFirst(0);
    seq.setSize(4);
    seq.setNoteDuration(1);    // ×4
    seq.setVariation(0);
    seq.setBurst(100);          // every cell tries to start a cluster
    seq.setBurstCount(50);
    seq.setBurstRate(50);
    seq.setBurstHold(StochasticBurstHold::Hold);
    for (int i = 0; i < 4; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
        seq.events()[i].setDegree(3);
        seq.events()[i].setOctave(1);
    }
    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0xbeef, &scale, &track);
    for (int i = 0; i < int(cache.count); ++i) {
        expectEqual(int(cache.cells[i].degree()), 3,
                    "Hold mode: every cell keeps source-event degree");
        expectEqual(int(cache.cells[i].octave()), 1,
                    "Hold mode: every cell keeps source-event octave");
    }
}

CASE("burst_pitch_roll_rerolls_cluster_tail_pitch") {
    // BurstHold == Roll: cluster tail cells reroll degree via
    // generateDegree. With deterministic keyed RNG, at least one tail cell
    // must differ from the starter's degree (otherwise the reroll is a noop).
    const Scale &scale = Scale::get(0);
    StochasticTrack track;
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);
    seq.setSize(8);
    seq.setRhythmMode(StochasticSourceMode::Loop);   // cluster cells form via cache
    seq.setNoteDuration(1);
    seq.setVariation(0);
    seq.setBurst(100);
    seq.setBurstCount(100);    // bias toward longer clusters → more tail cells
    seq.setBurstRate(50);
    seq.setBurstHold(StochasticBurstHold::Roll);
    seq.setRange(3);           // wider candidate pool so reroll has room to differ
    for (int i = 0; i < 8; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
        seq.events()[i].setDegree(0);
        seq.events()[i].setOctave(0);
    }
    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0xcafe, &scale, &track);
    bool sawNonZero = false;
    for (int i = 0; i < int(cache.count); ++i) {
        if (cache.cells[i].degree() != 0 || cache.cells[i].octave() != 0) {
            sawNonZero = true;
            break;
        }
    }
    expectTrue(sawNonZero,
               "Roll mode: at least one cluster tail cell must reroll to a different degree");
}

CASE("parent_cache_idx_maps_slot_to_parent_cell") {
    // The engine's mask filter looks up cache rank via parentCacheIdx[readIndex].
    // Verify that mapping is correct even when bursts insert extra cells
    // between parents.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(4);
    seq.setFirst(0);

    // slot 0: short, no burst (durationIndex=7 → 24 ticks @ divisor=48, below kMinBurstParentTicks=96)
    seq.events()[0].setDurationIndex(7);
    seq.events()[0].setRest(false);
    seq.events()[0].setRhythmValid(true);
    // slot 1: long, burst with 2 children
    seq.events()[1].setDurationIndex(1);  // ×4 = 192 ticks
    seq.events()[1].setRest(false);
    seq.events()[1].setRhythmValid(true);
    seq.events()[1].setChildCount(2);
    seq.events()[1].setBurstRate(2);
    // slot 2: short, no burst
    seq.events()[2].setDurationIndex(6);
    seq.events()[2].setRest(false);
    seq.events()[2].setRhythmValid(true);
    // slot 3: long, burst with 3 children
    seq.events()[3].setDurationIndex(1);
    seq.events()[3].setRest(false);
    seq.events()[3].setRhythmValid(true);
    seq.events()[3].setChildCount(3);
    seq.events()[3].setBurstRate(2);

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0xa5a5);

    // Phase 16 flat (2026-05-23): cells are 1:1 with event slots. No
    // separate burst-child cells. parentCacheIdx[K] = K - first for K in
    // [first..last]. Outside the window, 0xff.
    expectEqual(int(cache.parentCacheIdx[0]), 0);
    expectEqual(int(cache.parentCacheIdx[1]), 1);
    expectEqual(int(cache.parentCacheIdx[2]), 2);
    expectEqual(int(cache.parentCacheIdx[3]), 3);

    // Slot outside the window is unmapped.
    expectEqual(int(cache.parentCacheIdx[10]), 0xff);
}

CASE("cache_cap_truncates_silently") {
    // Worst-case: every parent has 5 children, 64-cell cap stops cleanly.
    // Use 16 parents × 5 children each = 80 attempted cells; cap at 64.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setSize(16);  // 16 slots

    for (int i = 0; i < 16; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(1);  // ×4 → 192 ticks, allows bursts
        ev.setRest(false);
        ev.setRhythmValid(true);
        ev.setChildCount(5);
        ev.setBurstRate(0);  // densest spacing
    }

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, 48, 0);
    expectTrue(n <= kCellCap, "cache must not exceed cell cap");
    expectEqual(n, int(cache.count));
}

CASE("parents_keep_priority_under_burst_truncation") {
    // Codex adversarial review 2026-05-22 finding #2: cell-cap truncation can
    // leave late parent slots unmapped, mixing deterministic-cache ranks for
    // early parents with legacy noisy ranks for late ones. Fix: parents have
    // priority; bursts only consume capacity that doesn't starve later parents.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(32);   // 32-slot extent
    seq.setFirst(0);

    // Every event requests 5 burst children: 32 × (1 + 5) = 192 cells if all
    // fit, but cap is 64. Without parent priority, the first ~10 slots would
    // consume the entire cap and slots 11..31 would be unmapped.
    for (int i = 0; i < 32; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(1);  // ×4 = 192 ticks, allows bursts
        ev.setRest(false);
        ev.setRhythmValid(true);
        ev.setChildCount(5);
        ev.setBurstRate(0);
    }

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, 48, 0xabcdef);
    expectTrue(n <= kCellCap, "cap respected");

    // Every parent slot in the active window MUST map to a parent cell —
    // never 0xff. This is the property that prevents mixed rank semantics.
    for (int slot = 0; slot <= 31; ++slot) {
        expectTrue(cache.parentCacheIdx[slot] != 0xff,
                   "every window slot must map to a parent cell");
        expectTrue(cache.parentCacheIdx[slot] < kCellCap,
                   "mapped index must point inside the cache");
    }
}

CASE("feel_scale_in_detent_is_unity") {
    // Phase 16 P5 (2026-05-23): Feel knob in detent [45..55] = no scaling.
    // computeFeelScaleQ16 should return 0x10000 (1.0 in Q16.16) for any
    // input in that range, regardless of naturalSum.
    for (int feel = 45; feel <= 55; ++feel) {
        uint32_t scale = computeFeelScaleQ16(feel, /*naturalSum*/ 768, /*beatTicks*/ 192);
        expectEqual(int(scale), 0x10000, "detent must be unity scale");
    }
}

CASE("feel_scale_endpoints_target_3_and_5_beats") {
    // Feel=0 should target 3 beats (3/4 feel). Feel=100 should target 5 beats.
    // Verify with naturalSum = 4 beats (default at Size=16 × ×1 LUT × divisor).
    // With CONFIG_PPQN as beat ticks:
    //   feel=0   → target 3 → scale = 3*PPQN / (4*PPQN) = 0.75 = 0xC000 in Q16.16
    //   feel=100 → target 5 → scale = 5*PPQN / (4*PPQN) = 1.25 = 0x14000 in Q16.16
    uint32_t naturalSum_4beats = 4 * 192;   // assume PPQN=192
    uint32_t scaleLow  = computeFeelScaleQ16(/*feel*/ 0,   naturalSum_4beats, /*beatTicks*/ 192);
    uint32_t scaleHigh = computeFeelScaleQ16(/*feel*/ 100, naturalSum_4beats, /*beatTicks*/ 192);

    // Allow small rounding tolerance.
    int expectedLow  = int(0.75f * 65536.f + 0.5f);   // = 49152
    int expectedHigh = int(1.25f * 65536.f + 0.5f);   // = 81920
    int deltaLow  = int(scaleLow)  > expectedLow  ? int(scaleLow)  - expectedLow  : expectedLow  - int(scaleLow);
    int deltaHigh = int(scaleHigh) > expectedHigh ? int(scaleHigh) - expectedHigh : expectedHigh - int(scaleHigh);
    expectTrue(deltaLow  < 16, "Feel=0 → scale ≈ 0.75");
    expectTrue(deltaHigh < 16, "Feel=100 → scale ≈ 1.25");
}

CASE("feel_scale_clamped_at_extreme_naturalSum") {
    // If naturalSum is wildly larger than target, scale would compress
    // catastrophically. computeFeelScaleQ16 clamps to [0.25, 4.0].
    // naturalSum = 100 beats; feel=0 wants 3 beats → 0.03 raw → clamped to 0.25.
    uint32_t huge = 100 * 192;
    uint32_t scale = computeFeelScaleQ16(/*feel*/ 0, huge, /*beatTicks*/ 192);
    int expectedClamp = int(0.25f * 65536.f);
    int delta = int(scale) > expectedClamp ? int(scale) - expectedClamp : expectedClamp - int(scale);
    expectTrue(delta < 32, "extreme compression clamped at 0.25 scale");
}

CASE("feel_scale_div_by_zero_safe") {
    // Empty cycle → naturalSum=0 should not div-by-zero. Return unity.
    uint32_t scale = computeFeelScaleQ16(/*feel*/ 0, /*naturalSum*/ 0, /*beatTicks*/ 192);
    expectEqual(int(scale), 0x10000);
}

CASE("applyFeelScale_unity_is_identity") {
    // Applying scale=1.0 (0x10000) must not change tick values (within
    // integer rounding from the >>16 shift).
    expectEqual(int(applyFeelScale(0,    0x10000)), 0);
    expectEqual(int(applyFeelScale(48,   0x10000)), 48);
    expectEqual(int(applyFeelScale(768,  0x10000)), 768);
    expectEqual(int(applyFeelScale(4095, 0x10000)), 4095);
}

CASE("applyFeelScale_quarter_compresses") {
    // Scale=0.25 (the clamp floor): 96-tick parent becomes 24-tick.
    uint32_t quarter = uint32_t(0.25f * 65536.f);
    expectEqual(int(applyFeelScale(96, quarter)), 24);
    expectEqual(int(applyFeelScale(768, quarter)), 192);
}

CASE("cycle_longer_than_4095_ticks_preserves_per_cell_durations") {
    // Phase 16 P6 (2026-05-23): regression for Codex audit #6. The old
    // 12-bit relTick field stored absolute cycle position and clamped at
    // 4095 — cells past that mark collapsed to identical positions and
    // their playback duration (derived from next-this delta) decayed to 1.
    // With per-cell durations, total cycle length is unbounded; each cell
    // stores its own duration up to kMaxCellDuration.
    //
    // Phase 16 P7 (2026-05-23): cache picks duration from sequence.noteDuration
    // + variation knobs, not from event.durationIndex. Set the knob, lock
    // variation to 0 so every cell picks the same LUT slot.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(32);
    seq.setFirst(0);
    seq.setRhythmMode(StochasticSourceMode::Loop);
    seq.setNoteDuration(1);   // LUT slot 1 = ×4
    seq.setVariation(0);       // tight kernel → always picks the center slot
    seq.setBurst(0);           // no clusters
    for (int i = 0; i < 32; ++i) {
        auto &ev = seq.events()[i];
        ev.setRhythmValid(true);
        ev.setRest(false);
    }
    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, /*divisor*/ 192, /*seed*/ 0xfeedf00d);
    expectEqual(n, 32);

    // Every cell holds its own 768-tick duration (×4 × 192 = 768). Pre-fix,
    // cells past index 5 (4095 / 768) would clamp to relTick=4095 and the
    // engine's delta math would return duration=1 for every later cell.
    for (int i = 0; i < 32; ++i) {
        expectEqual(int(cache.cells[i].durationTicks()), 768,
                    "every cell must keep its full duration past the 4095-tick mark");
    }
}

CASE("cache_walk_picks_duration_from_noteDuration_knob") {
    // Phase 16 P7 (2026-05-23): cache reads sequence.noteDuration + variation,
    // NOT event.durationIndex. Confirm a knob change reshapes the next cache.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(4);
    seq.setFirst(0);
    seq.setSize(4);
    seq.setRhythmMode(StochasticSourceMode::Loop);
    seq.setVariation(0);
    seq.setBurst(0);
    for (int i = 0; i < 4; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
        // Event-stored durationIndex deliberately a random slot — should be
        // ignored by cache in Loop mode.
        seq.events()[i].setDurationIndex(7);
    }

    seq.setNoteDuration(5);     // ×1 = 48 at divisor 48
    Cache cacheA{};
    regenerateCacheFromEvents(cacheA, seq, /*divisor*/ 48, /*seed*/ 0xabc);
    for (int i = 0; i < 4; ++i) {
        expectEqual(int(cacheA.cells[i].durationTicks()), 48,
                    "knob=5 (×1) should produce 48-tick cells");
    }

    seq.setNoteDuration(3);     // ×2 = 96 at divisor 48
    Cache cacheB{};
    regenerateCacheFromEvents(cacheB, seq, /*divisor*/ 48, /*seed*/ 0xabc);
    for (int i = 0; i < 4; ++i) {
        expectEqual(int(cacheB.cells[i].durationTicks()), 96,
                    "knob=3 (×2) should produce 96-tick cells");
    }
}

CASE("burst_fires_at_factory_defaults") {
    // Codex audit finding #1+#2: pre-P7 the generator gated burst on the
    // event's own duration ≥ 96 ticks, and bursts were baked at mutate time
    // so the Burst knob did nothing until a regeneration cycle ran. With B3
    // landed, raising the Burst knob and calling refreshCache produces
    // visible cluster cells immediately, at factory-default divisor/duration.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    // Mirror the cleared default: size=32, noteDuration=5 (×1), variation=16,
    // divisor=12 (1/16 at PPQN=192 = 48 ticks per cell). Knob defaults except
    // burst itself — bring it up so the per-cell roll lands frequently.
    seq.setRhythmMode(StochasticSourceMode::Loop);
    seq.setBurst(100);          // every roll lands → every cell tries to start a cluster
    seq.setBurstCount(50);      // medium count from the LUT
    seq.setBurstRate(50);       // medium denom from the LUT
    for (int i = 0; i < 32; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, /*divisor*/ 48, /*seed*/ 0x515ce11);
    expectEqual(int(cache.count), 32);

    // At default 1/16 = 48 ticks per cell. Cluster cells = 48 / denom where
    // denom ∈ {2,3,4,5,6}, so cluster cells fall in [8, 24] ticks. Any cell
    // with durationTicks < 48 must be a cluster cell. Pre-fix this count was
    // 0 because the generator gate suppressed every burst.
    int clusterCells = 0;
    for (int i = 0; i < int(cache.count); ++i) {
        if (cache.cells[i].durationTicks() < 48) ++clusterCells;
    }
    expectTrue(clusterCells > 0,
               "factory-default burst must produce at least one cluster cell");
}

CASE("cache_rebuild_is_deterministic_at_fixed_seed") {
    // Phase 16 P7 (2026-05-23): cache shaping decisions now consume keyed
    // RNG, so two rebuilds at the same seed must produce identical cells.
    // This is what keeps Loop replay stable when knob edits trigger a
    // refreshCache.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(16);
    seq.setFirst(0);
    seq.setSize(16);
    seq.setBurst(80);
    seq.setVariation(50);
    for (int i = 0; i < 16; ++i) {
        seq.events()[i].setRhythmValid(true);
        seq.events()[i].setRest(false);
    }

    Cache cacheA{};
    Cache cacheB{};
    regenerateCacheFromEvents(cacheA, seq, 48, /*seed*/ 0xc0ffee);
    regenerateCacheFromEvents(cacheB, seq, 48, /*seed*/ 0xc0ffee);

    expectEqual(int(cacheA.count), int(cacheB.count));
    for (int i = 0; i < int(cacheA.count); ++i) {
        expectEqual(int(cacheA.cells[i].durationTicks()),
                    int(cacheB.cells[i].durationTicks()),
                    "same seed must produce same per-cell durations");
    }
}

CASE("live_rhythm_reads_event_durationIndex_and_burst_fields") {
    // Live (ui) rhythm contract: every trigger the engine writes fresh
    // content into the events array via writeLiveRhythmShadow. The cache
    // must read those event fields verbatim — not pick via keyed RNG —
    // so each event is genuinely fresh, not seed-locked across cycles.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setRhythmMode(StochasticSourceMode::Live);
    seq.setSize(4);
    seq.setFirst(0);
    seq.setSize(4);

    // Sequence-level knobs deliberately different from what events store —
    // the test proves cache ignores them in Live and uses event fields.
    seq.setNoteDuration(0);    // would map to LUT slot 0 = ×8 if used
    seq.setVariation(0);
    seq.setBurst(0);

    for (int i = 0; i < 4; ++i) {
        auto &ev = seq.events()[i];
        ev.setRhythmValid(true);
        ev.setRest(false);
        ev.setDurationIndex(5);   // ×1 = 48 ticks at divisor=48
    }
    // Slot 1 carries a burst spec — cluster of 2 cells with denom 3.
    seq.events()[1].setChildCount(1);
    seq.events()[1].setBurstRate(1);     // index 1 → kBurstSpacingLut[1] = 3

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, /*divisor*/ 48, /*seed*/ 0xfeed);
    expectEqual(int(cache.count), 4);

    // Slot 0: ev.durationIndex=5 → 48 ticks (not slot 0 = ×8 which knob would give).
    expectEqual(int(cache.cells[0].durationTicks()), 48);
    // Slot 1: cluster anchor. cluster_dur = prev(48) / 3 = 16 ticks.
    expectEqual(int(cache.cells[1].durationTicks()), 16);
    // Slot 2: cluster tail, same cluster duration.
    expectEqual(int(cache.cells[2].durationTicks()), 16);
    // Slot 3: cluster ended (childCount=1 = anchor + 1 tail). Natural pick again.
    expectEqual(int(cache.cells[3].durationTicks()), 48);
}

CASE("rank_assignment_is_stable_across_runs_at_same_seed") {
    // Replaces the old "Tilt changes ranks" test — Tilt no longer influences
    // rank assignment. The new contract is that ranks are deterministic per
    // (seed, durationIndex distribution).
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setSize(8);

    for (int i = 0; i < 8; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(i);
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    StochasticGenerator::generateMaskRanks(seq, 8, 0xfeedface);
    uint8_t firstPass[8];
    for (int i = 0; i < 8; ++i) firstPass[i] = seq.events()[i].densityRank();

    StochasticGenerator::generateMaskRanks(seq, 8, 0xfeedface);
    for (int i = 0; i < 8; ++i) {
        expectEqual(int(seq.events()[i].densityRank()), int(firstPass[i]),
                    "same seed + same durations must produce same ranks");
    }
}

} // UNIT_TEST("StochasticCacheParity")
