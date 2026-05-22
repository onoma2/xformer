// Phase 14B Patch A — engine cache parity test.
//
// Proves that stochastic_cache::regenerateCacheFromEvents produces a cell
// stream matching today's playback semantics for the rhythm domain. CV
// resolution is deferred to Patch B; this test only validates timing,
// gate shape, burst-child expansion, flag propagation, window respect,
// and deterministic rank assignment.

#include "UnitTest.h"

#include "apps/sequencer/engine/StochasticCache.h"
#include "apps/sequencer/engine/StochasticTrackEngine.h"
#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/StochasticTypes.h"

#include <cstdint>

using namespace stochastic_cache;

namespace {

// Reproduces evaluateChildren's spacing math for the parity check. Kept local
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
    for (uint32_t r : { 0u, 1u, 96u, 768u, 2047u, 4095u }) {
        for (int cv : { -2048, -100, 0, 100, 2047 }) {
            for (uint8_t g : { uint8_t(0), uint8_t(31), uint8_t(63) }) {
                for (bool slide : { false, true }) {
                    for (bool legato : { false, true }) {
                        auto c = CachedCell::make(r, cv, g, slide, legato);
                        expectEqual(uint32_t(c.relTick()), r);
                        expectEqual(c.cv(), cv);
                        expectEqual(int(c.gateLen()), int(g));
                        expectEqual(c.slide(), slide);
                        expectEqual(c.legato(), legato);
                    }
                }
            }
        }
    }
}

CASE("empty_sequence_yields_empty_cache") {
    StochasticSequence seq;
    seq.clear();
    seq.setFirst(0);
    seq.setLast(0);
    clearAllEvents(seq);
    // first == last == 0, single slot. With ev cleared, durationIndex=0 (slot 0
    // = ×8 = longest). Still produces one parent cell — rest=false by default.
    // To test true emptiness, force size=0... but setSize clamps to >=1. We
    // instead test the rest-only branch in a separate case below.

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, /*divisor*/ 48, /*seed*/ 0xdeadbeef);
    expectEqual(n, 1);
    expectEqual(int(cache.count), 1);
    expectTrue(cache.aux[0].audible(), "default cleared event has rest=false → audible");
}

CASE("single_parent_event_relTick_and_gateLen") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(0);

    auto &ev = seq.events()[0];
    ev.setDurationIndex(5);  // ×1 of divisor
    ev.setRest(false);
    ev.setSlide(true);
    ev.setLegato(false);
    ev.setRhythmValid(true);

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, /*divisor*/ 48, /*seed*/ 0x1234);
    expectEqual(n, 1);
    expectEqual(int(cache.cells[0].relTick()), 0);
    expectEqual(int(cache.cells[0].gateLen()), 5);
    expectTrue(cache.cells[0].slide(), "slide flag must propagate");
    expectFalse(cache.cells[0].legato(), "legato flag must propagate (false case)");
    expectTrue(cache.aux[0].audible(), "non-rest event is audible");
    expectFalse(cache.aux[0].burstChild(), "parent is not a burst child");
    expectEqual(int(cache.cycleTicks), 48);  // divisor × (1/1) = 48
}

CASE("rest_event_audible_false") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(0);

    auto &ev = seq.events()[0];
    ev.setDurationIndex(5);
    ev.setRest(true);
    ev.setRhythmValid(true);

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, 0);
    expectEqual(int(cache.count), 1);
    expectFalse(cache.aux[0].audible(), "rest event must be marked non-audible");
}

CASE("window_first_last_respected") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(2);
    seq.setLast(4);

    for (int i = 0; i < int(seq.events().size()); ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(5);
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, 48, 0);
    expectEqual(n, 3);  // slots 2, 3, 4 only
    // First cell's relTick must be 0 — window starts at first(), not slot 0.
    expectEqual(int(cache.cells[0].relTick()), 0);
    expectEqual(int(cache.cells[1].relTick()), 48);
    expectEqual(int(cache.cells[2].relTick()), 96);
    expectEqual(int(cache.cycleTicks), 144);
}

CASE("burst_children_match_reference_math") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(0);

    auto &ev = seq.events()[0];
    ev.setDurationIndex(1);  // ×4 of divisor — 4 × 48 = 192 ticks, above kMinBurstParentTicks
    ev.setRest(false);
    ev.setRhythmValid(true);
    ev.setChildCount(3);
    ev.setBurstRate(2);  // spacingSlot=2 → denom=4
    // parentTicks = 192. spacing = 192/4 = 48. 3 children at offsets 48, 96, 144.

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, 48, 0);
    expectEqual(n, 4);  // parent + 3 children
    expectFalse(cache.aux[0].burstChild(), "first cell is parent");
    expectTrue(cache.aux[1].burstChild(), "cells 1-3 are burst children");
    expectTrue(cache.aux[2].burstChild());
    expectTrue(cache.aux[3].burstChild());

    uint32_t refOffsets[5];
    int refCount = expectedChildOffsets(/*parentTicks*/ 192, /*childCount*/ 3, /*spacingSlot*/ 2, refOffsets);
    expectEqual(refCount, 3);
    expectEqual(int(cache.cells[1].relTick()), int(refOffsets[0]));
    expectEqual(int(cache.cells[2].relTick()), int(refOffsets[1]));
    expectEqual(int(cache.cells[3].relTick()), int(refOffsets[2]));
}

CASE("burst_suppressed_when_parent_too_short") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(0);

    auto &ev = seq.events()[0];
    ev.setDurationIndex(7);  // ×1/2 — 24 ticks at divisor=48, below kMinBurstParentTicks=96
    ev.setRest(false);
    ev.setRhythmValid(true);
    ev.setChildCount(4);
    ev.setBurstRate(2);

    Cache cache{};
    int n = regenerateCacheFromEvents(cache, seq, 48, 0);
    expectEqual(n, 1);  // parent only, no children
}

CASE("rank_is_deterministic_per_seed") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(7);

    for (int i = 0; i < 8; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(i);  // 0..7 spans the LUT
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    Cache cacheA{}, cacheB{};
    regenerateCacheFromEvents(cacheA, seq, 48, /*seed*/ 0xaabbccdd);
    regenerateCacheFromEvents(cacheB, seq, 48, /*seed*/ 0xaabbccdd);

    for (int i = 0; i < int(cacheA.count); ++i) {
        expectEqual(int(cacheA.aux[i].rank), int(cacheB.aux[i].rank), "same seed → same rank");
    }
}

CASE("rank_changes_when_seed_changes") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(7);

    for (int i = 0; i < 8; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(i);
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    Cache cacheA{}, cacheB{};
    regenerateCacheFromEvents(cacheA, seq, 48, /*seed*/ 0xaabbccdd);
    regenerateCacheFromEvents(cacheB, seq, 48, /*seed*/ 0x12345678);

    int divergent = 0;
    for (int i = 0; i < int(cacheA.count); ++i) {
        if (cacheA.aux[i].rank != cacheB.aux[i].rank) ++divergent;
    }
    expectTrue(divergent >= int(cacheA.count) / 2, "seed change should reshuffle most ranks");
}

CASE("tilt_positive_favors_long_durations") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(7);

    for (int i = 0; i < 8; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(i);
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, /*seed*/ 0xaabbccdd);
    recomputeCacheRanks(cache, 0xaabbccdd, /*tilt*/ 100);

    // Mask filter cuts HIGH ranks. "Survive Mask" means LOW rank. So at
    // positive tilt, long cells must get LOW ranks (they survive). Cell 0
    // has durationIndex=0 (longest), cell 7 has durationIndex=7 (shortest).
    int rankLong  = cache.aux[0].rank;  // slot 0 = longest
    int rankShort = cache.aux[7].rank;  // slot 7 = shortest
    expectTrue(rankLong < rankShort, "positive tilt: long cells get LOW ranks (survive Mask)");
}

CASE("tilt_negative_favors_short_durations") {
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(7);

    for (int i = 0; i < 8; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(i);
        ev.setRest(false);
        ev.setRhythmValid(true);
    }

    Cache cache{};
    regenerateCacheFromEvents(cache, seq, 48, /*seed*/ 0xaabbccdd);
    recomputeCacheRanks(cache, 0xaabbccdd, /*tilt*/ -100);

    int rankLong  = cache.aux[0].rank;
    int rankShort = cache.aux[7].rank;
    expectTrue(rankShort < rankLong, "negative tilt: short cells get LOW ranks (survive Mask)");
}

CASE("parent_cache_idx_maps_slot_to_parent_cell") {
    // The engine's mask filter looks up cache rank via parentCacheIdx[readIndex].
    // Verify that mapping is correct even when bursts insert extra cells
    // between parents.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(3);

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

    // Expected cache layout (parent + children interleaved):
    //   idx 0: parent slot 0 (no children)
    //   idx 1: parent slot 1
    //   idx 2,3: slot 1's burst children
    //   idx 4: parent slot 2
    //   idx 5: parent slot 3
    //   idx 6,7,8: slot 3's burst children
    expectEqual(int(cache.parentCacheIdx[0]), 0);
    expectEqual(int(cache.parentCacheIdx[1]), 1);
    expectEqual(int(cache.parentCacheIdx[2]), 4);
    expectEqual(int(cache.parentCacheIdx[3]), 5);

    // Slot outside the window is unmapped.
    expectEqual(int(cache.parentCacheIdx[10]), 0xff);

    // The cells at the mapped indices must NOT be burst children.
    for (int slot = 0; slot <= 3; ++slot) {
        uint8_t cidx = cache.parentCacheIdx[slot];
        expectFalse(cache.aux[cidx].burstChild(), "parentCacheIdx must point at a parent cell");
    }
}

CASE("cache_cap_truncates_silently") {
    // Worst-case: every parent has 5 children, 64-cell cap stops cleanly.
    // Use 16 parents × 5 children each = 80 attempted cells; cap at 64.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(15);  // 16 slots

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

} // UNIT_TEST("StochasticCacheParity")
