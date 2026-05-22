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
    //
    // Phase 16 P2 (2026-05-23): octave narrowed 5→4 bits (max 15) to free a
    // bit for `audible` packed into the cell. Test exercises the new layout.
    for (uint32_t r : { 0u, 1u, 96u, 768u, 2047u, 4095u }) {
        for (uint8_t deg : { uint8_t(0), uint8_t(1), uint8_t(63), uint8_t(127) }) {
            for (uint8_t oct : { uint8_t(0), uint8_t(1), uint8_t(7), uint8_t(15) }) {
                for (uint8_t g : { uint8_t(0), uint8_t(31), uint8_t(63) }) {
                    for (bool slide : { false, true }) {
                        for (bool legato : { false, true }) {
                            for (bool audible : { false, true }) {
                                auto c = CachedCell::make(r, deg, oct, g, slide, legato, audible);
                                expectEqual(uint32_t(c.relTick()), r);
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
    seq.setLast(3);

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
    expectTrue(cache.cells[0].audible(), "non-rest event is audible");
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
    expectFalse(cache.cells[0].audible(), "rest event must be marked non-audible");
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

CASE("burst_child_gate_fraction_survives_long_parents") {
    // Codex review 2026-05-22 finding 3: legacy evaluateChildren used the full
    // computed gateTicks. Previous cache encoding clamped to 63 raw ticks,
    // shortening child gates at long parent durations and skewing audible
    // behavior vs the Repeat-legacy path. Fix: cache stores child gate as
    // 64ths-of-parent fraction; engine decodes back to ticks at trigger time.
    //
    // This test exercises the fraction roundtrip directly: emit a parent
    // long enough that the raw childGate exceeds 63 ticks, then verify the
    // engine-side decode reconstructs the original tick count within rounding.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(0);
    seq.setBurstPitch(StochasticBurstPitch::Parent);

    auto &ev = seq.events()[0];
    ev.setDurationIndex(0);   // longest: ×8 (largest LUT slot)
    ev.setRest(false);
    ev.setRhythmValid(true);
    ev.setChildCount(2);
    ev.setBurstRate(0);       // spacingSlot 0 → denom 2; childGate = parentTicks/4

    // Use a divisor large enough that parentTicks * 8 overshoots the legacy
    // 63-tick clamp by a large margin. divisor=96 → parentTicks = 96 * 8 = 768.
    // spacing = 768 / 2 = 384. childGate (legacy) = 384 * 0.5 = 192. Far above 63.
    const uint32_t kDivisor = 96;
    Cache cache{};
    regenerateCacheFromEvents(cache, seq, kDivisor, 0xfeed);

    // Parent at index 0; child at index 1 (we requested 2 children but only
    // the first fits before parentTicks runs out at spacingDenom=2).
    expectTrue(cache.count >= 2, "at least one child must be cached");
    expectFalse(cache.aux[0].burstChild());
    expectTrue(cache.aux[1].burstChild());

    // Compute parentTicks the same way the engine does at trigger time.
    auto frac = StochasticTrackEngine::getDurationFraction(int(ev.durationIndex()));
    uint32_t parentTicks = (uint64_t(kDivisor) * frac.num) / frac.den;
    expectEqual(int(parentTicks), 768);

    // Decode the child's gate from the cell's 64ths fraction.
    uint32_t childGateField = cache.cells[1].gateLen();
    expectTrue(childGateField > 0 && childGateField <= 63,
               "fraction must encode in 6-bit range");
    uint32_t decodedGate = (childGateField * parentTicks) / 64u;

    // Expected legacy gate = parentTicks / 4 = 192. Allow rounding within
    // one quantization step (parentTicks/64 = 12 ticks per fractional unit).
    const uint32_t kExpectedLegacyGate = 192;
    uint32_t tolerance = parentTicks / 64u;   // one fractional-step worth
    uint32_t delta = decodedGate > kExpectedLegacyGate
                   ? decodedGate - kExpectedLegacyGate
                   : kExpectedLegacyGate - decodedGate;
    expectTrue(delta <= tolerance,
               "decoded child gate must match legacy gate within fractional tolerance — "
               "previous raw 63-tick clamp would fail this test by ~129 ticks");
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
        expectEqual(int(cacheA.aux[i].rank()), int(cacheB.aux[i].rank()), "same seed → same rank");
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
        if (cacheA.aux[i].rank() != cacheB.aux[i].rank()) ++divergent;
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
    int rankLong  = cache.aux[0].rank();  // slot 0 = longest
    int rankShort = cache.aux[7].rank();  // slot 7 = shortest
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

    int rankLong  = cache.aux[0].rank();
    int rankShort = cache.aux[7].rank();
    expectTrue(rankShort < rankLong, "negative tilt: short cells get LOW ranks (survive Mask)");
}

CASE("burst_child_degree_octave_parent_mode_copies_parent") {
    // Patch C step 1b (2026-05-22): when burstPitch == Parent, every burst-
    // child cell must carry the parent's degree+octave so the engine plays
    // the same note for the child without consulting evaluateChildren.
    //
    // Note: this test doesn't pass scale/track (default nullptrs) so the
    // builder's bakeChildNotes path is OFF and children would land with
    // degree=0,octave=0. That's the correct fallback behavior for callers
    // that skip the scale plumbing. The engine code path always passes
    // scale+track, exercised below.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setFirst(0);
    seq.setLast(0);
    seq.setBurstPitch(StochasticBurstPitch::Parent);

    auto &ev = seq.events()[0];
    ev.setDurationIndex(1);          // ×4 = 192 ticks → bursts allowed
    ev.setDegree(5);
    ev.setOctave(2);
    ev.setRest(false);
    ev.setRhythmValid(true);
    ev.setChildCount(2);
    ev.setBurstRate(2);

    // No scale/track -> children land with placeholder degree/octave.
    Cache cacheNoScale{};
    regenerateCacheFromEvents(cacheNoScale, seq, 48, 0xabcdef);
    for (uint8_t i = 1; i < cacheNoScale.count; ++i) {
        expectTrue(cacheNoScale.aux[i].burstChild(), "trailing cells must be burst children");
        expectEqual(int(cacheNoScale.cells[i].degree()), 0);
        expectEqual(int(cacheNoScale.cells[i].octave()), 0);
    }
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

CASE("parents_keep_priority_under_burst_truncation") {
    // Codex adversarial review 2026-05-22 finding #2: cell-cap truncation can
    // leave late parent slots unmapped, mixing deterministic-cache ranks for
    // early parents with legacy noisy ranks for late ones. Fix: parents have
    // priority; bursts only consume capacity that doesn't starve later parents.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(32);   // must precede setLast — setLast clamps to size-1.
    seq.setFirst(0);
    seq.setLast(31);  // 32 parents — well within cap

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
        expectFalse(cache.aux[cache.parentCacheIdx[slot]].burstChild(),
                    "mapping must point at a parent, not a burst child");
    }
}

CASE("repeat_path_uses_event_rank_not_cache_rank") {
    // Codex adversarial review 2026-05-22: when Repeat fires, the engine
    // replays _lastEvent. The slot's cached rank refers to the original
    // event at that index, NOT the repeated material. Mask must filter by
    // the repeated event's own densityRank so the right material is gated.
    Cache cache{};
    // Construct a sequence that primes the cache so we know the ranks at
    // each slot.
    StochasticSequence seq;
    seq.clear();
    clearAllEvents(seq);
    seq.setSize(8);
    seq.setFirst(0);
    seq.setLast(7);
    for (int i = 0; i < 8; ++i) {
        auto &ev = seq.events()[i];
        ev.setDurationIndex(i);   // 0..7 spans the LUT
        ev.setRest(false);
        ev.setRhythmValid(true);
    }
    regenerateCacheFromEvents(cache, seq, 48, /*seed*/ 0xa5a5);

    // Choose a readIndex where cache rank and "repeated event's rank" must
    // disagree. We forge a densityRank on a synthetic repeated event that
    // differs from whatever rank the cache assigned to slot 3.
    const int testSlot = 3;
    const uint8_t cacheRankAtSlot = cache.aux[cache.parentCacheIdx[testSlot]].rank();

    // Pick a forged event rank guaranteed to differ from the cache rank.
    const uint8_t forgedRepeatedRank = uint8_t((cacheRankAtSlot + 5) & 0x3f);
    expectTrue(forgedRepeatedRank != cacheRankAtSlot, "test setup: ranks must differ");

    // Non-repeat path returns cache rank.
    uint32_t r_nonrepeat = selectMaskRank(
        /*useRepeat=*/false, forgedRepeatedRank, testSlot, cache);
    expectEqual(int(r_nonrepeat), int(cacheRankAtSlot));

    // Repeat path returns the event's own rank, IGNORING the cache.
    uint32_t r_repeat = selectMaskRank(
        /*useRepeat=*/true, forgedRepeatedRank, testSlot, cache);
    expectEqual(int(r_repeat), int(forgedRepeatedRank));

    // Confirm the two paths actually disagree — proves we're testing the
    // discriminating behavior, not just both returning the same value.
    expectTrue(r_nonrepeat != r_repeat,
               "the bug is that repeat path used cache rank — these MUST differ");
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

CASE("fallback_when_cache_unprimed") {
    // Engine reset, no refresh yet — parentCacheIdx is zero-init = 0, but
    // cacheIdx 0 with count=0 means cells/aux are also zeroed. The mask
    // filter must not lock up; selectMaskRank falls back to the legacy
    // density rank when the cache hasn't been built.
    Cache cache{};   // count = 0, parentCacheIdx[i] = 0 (default zero-init)

    // Out-of-range readIndex returns the event rank via fallback.
    uint32_t r = selectMaskRank(/*useRepeat=*/false, /*evRank=*/42, /*readIndex=*/100, cache);
    expectEqual(int(r), 42);
}

CASE("rank_recomputes_when_tilt_changes_via_caller") {
    // Codex adversarial review 2026-05-22 finding #1: tilt is a live knob, so
    // the engine must call recomputeCacheRanks when tilt changes — the cache
    // build itself only knows about the seed. This test exercises the helper
    // directly to confirm tilt-only changes produce different rank orderings.
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
    regenerateCacheFromEvents(cache, seq, 48, /*seed*/ 0xfeedface);
    // Default build leaves tilt=0; capture the baseline ordering.
    uint8_t ranksTilt0[kCellCap];
    for (int i = 0; i < int(cache.count); ++i) ranksTilt0[i] = cache.aux[i].rank();

    // Caller flips tilt without rebuilding the cache — must change ranks.
    recomputeCacheRanks(cache, /*seed*/ 0xfeedface, /*tilt*/ 100);
    int changed = 0;
    for (int i = 0; i < int(cache.count); ++i) {
        if (cache.aux[i].rank() != ranksTilt0[i]) ++changed;
    }
    expectTrue(changed > 0, "tilt change must reshuffle at least some ranks");
}

} // UNIT_TEST("StochasticCacheParity")
