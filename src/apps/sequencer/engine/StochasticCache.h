#pragma once

// Phase 14B engine-side cache for stochastic playback. Bit-packed cell layout
// validated by TestStochasticCacheParity against today's playback semantics.
//
// Patch A scope: data structures + populate-from-events helper + rank
// computation. Not yet wired into StochasticTrackEngine playback — playback
// switch happens in Patch B.

#include <cstdint>
#include <cstddef>

class StochasticSequence;
class StochasticTrack;
class Scale;

namespace stochastic_cache {

// Per-cell duration cap (12-bit field). ~21 quarter-notes at PPQN=192. A
// cell whose natural duration exceeds this is clamped at build time —
// the cycle is the sum of all cells' durations, so unlike the old
// absolute-position scheme this cannot silently corrupt the playhead.
constexpr uint32_t kMaxCellDuration = 4095;

// 6-bit gate length encoded as LUT slot or fractional ticks (0..63).
constexpr uint8_t kMaxGateLen = 63;

// Melody fields. Cells store scale-domain `degree` and `octave` rather than
// resolved voltage. Trigger-time scale + track-offset lookup matches today's
// engine, keeping the cache invariant to scale/rootNote/track.octave/
// track.transpose changes (no rebuild needed when those move).
//
// Phase 16 P2 (2026-05-23): octave narrowed 5 → 4 bits (range 0..15) to free
// one bit for `audible` packed into the cell. Practical stochastic event
// octaves stay well under 15. `audible` was previously in CellAux.flags;
// move into cell saves a byte per aux entry (CellAux shrinks to 1 byte).
constexpr uint8_t kMaxOctave = 15;

// Phase 16 P3 (2026-05-23): bumped 64 → 80 to give burst patterns more
// headroom before the cap truncates. Under the parent/child model this
// reduces burst-truncation in dense patterns. Once Phase 16 P5 lands
// (flat cell model where bursts no longer spawn separate cells), the
// optimal cap may drop back to 64 — revisit after P5.
// Pinned at 64 (Codex H1, 2026-05-23): CellAux::rank is 6 bits (0..63).
// Raising kCellCap past 64 wraps cells 64+ ranks via the 0x3f mask in
// setRank(), corrupting Mask/Tilt ordering. Widen the rank field to 7
// bits (one spare bit in CellAux today) before raising the cap.
constexpr int kCellCap = 64;

// Phase 16 P6 (2026-05-23): cell stores its own duration (per-cell), not
// the absolute cycle position. Removes the silent 4095-tick overflow that
// corrupted long-cycle playback when cells were spaced past the 12-bit
// relTick cap — each cell's duration is independent and naturally bounded
// by what a single cell can hold (kMaxCellDuration = 4095 ticks).
struct CachedCell {
    uint32_t packed;

    uint32_t durationTicks() const { return  packed        & 0xfffu; }
    uint8_t  degree()        const { return (packed >> 12) & 0x7fu; }
    uint8_t  octave()        const { return (packed >> 19) & 0xfu;  }   // 4 bits
    uint8_t  gateLen()       const { return (packed >> 23) & 0x3fu; }
    bool     audible()       const { return (packed >> 29) & 1u; }
    bool     slide()         const { return (packed >> 30) & 1u; }
    bool     legato()        const { return (packed >> 31) & 1u; }

    static CachedCell make(uint32_t durationTicks, uint8_t degree, uint8_t octave,
                           uint8_t gateLen, bool slide, bool legato, bool audible = true) {
        if (octave > kMaxOctave) octave = kMaxOctave;
        if (durationTicks > kMaxCellDuration) durationTicks = kMaxCellDuration;
        uint32_t r =  (durationTicks           & 0xfffu)
                   | ((uint32_t(degree)       & 0x7fu)  << 12)
                   | ((uint32_t(octave)       & 0xfu)   << 19)
                   | ((uint32_t(gateLen)       & 0x3fu) << 23)
                   | ((audible ? 1u : 0u)               << 29)
                   | ((slide   ? 1u : 0u)               << 30)
                   | ((legato  ? 1u : 0u)               << 31);
        return CachedCell{r};
    }
};
static_assert(sizeof(CachedCell) == 4, "CachedCell must be 4 bytes");

// Auxiliary per-cell metadata. Phase 16 P2 (2026-05-23): shrunk to 1 byte —
// audible moved into the cell, burst-child stays here until Phase 16 P5 (when
// flat layout eliminates the concept entirely).
//   bits 0-5: rank (0..63)
//   bit 6:    burst-child
//   bit 7:    reserved
struct CellAux {
    uint8_t packed;

    uint8_t rank()       const { return packed & 0x3fu; }
    bool    burstChild() const { return (packed >> 6) & 1u; }

    void setRank(uint8_t rank) {
        packed = uint8_t((packed & ~0x3fu) | (rank & 0x3fu));
    }

    static CellAux make(bool burstChild, uint8_t rank) {
        return CellAux{
            uint8_t((rank & 0x3fu) | ((burstChild ? 1u : 0u) << 6)),
        };
    }
};
static_assert(sizeof(CellAux) == 1, "CellAux must be 1 byte");

// Cache footprint per active track (post Phase 16 P2):
//   CachedCell[64] = 256 B
//   CellAux[64]    =  64 B   (was 128 B, audible moved into cell)
//   Total          = 320 B   (was 384 B)

// Max event slots in a sequence — matches CONFIG_STEP_COUNT but the header
// avoids pulling that in (Cache is consumed by tests too).
constexpr int kMaxEventSlots = 64;

struct Cache {
    CachedCell cells[kCellCap];
    CellAux    aux[kCellCap];

    // Map from event-slot index (0..kMaxEventSlots-1) to its parent cell index
    // in `cells[]`. Lets the engine look up "for sequence slot K, what's the
    // parent cell's rank?" without scanning the cache at trigger time.
    // 0xff = unmapped (slot outside the active window).
    uint8_t    parentCacheIdx[kMaxEventSlots];

    uint8_t    count;
    uint16_t   cycleTicks;

    // Phase 16 P6 (2026-05-23): Feel scaling moved out of cache state.
    // Engine computes the Q16.16 scale on the fly each trigger from
    // sequence.feel() + cycleTicks + CONFIG_PPQN via computeFeelScaleQ16.
    // This makes routed Feel CV take effect without a cache refresh.
};

// Populate cache by walking an existing StochasticSequence's `_events[]` tape.
// Produces the same playback stream the current engine path would produce, so
// the parity test can match cache content against today's behavior.
//
// `divisor` is the sequence divisor in ticks (engine reads this from track).
// `seed` is the rhythmSeed used for deterministic rank assignment AND for
//        burst-child Generate-mode pitch picking (keyed per cell index).
// `scale` / `track` / `rootNote` are passed for burst-child Generate-mode
//        scale-aware degree picking via StochasticGenerator::generateDegree.
//        Optional — pass nullptr `scale` / `track` to skip burst-child note
//        baking (children land with degree=0 placeholders for callers that
//        only need timing).
// Returns number of cells written.
int regenerateCacheFromEvents(Cache &cache, const StochasticSequence &seq, uint32_t divisor, uint32_t seed,
                              const Scale *scale = nullptr, const StochasticTrack *track = nullptr, int rootNote = 0);

// Recompute ranks from keyed-hash salt + tilt. Cells must already be in
// cache; durationIndex is recovered from gateLen via the duration LUT.
// `tilt` is -100..+100.
void recomputeCacheRanks(Cache &cache, uint32_t seed, int tilt);

// Phase 16 P5 (2026-05-23): compute the Feel scaling factor as Q16.16
// fixed-point. Inputs:
//   feel        — sequence.feel() knob value, 0..100.
//   naturalSum  — sum of natural cell durations in ticks (cache.cycleTicks).
//   beatTicks   — ticks per beat (CONFIG_PPQN typically).
// Detent [45..55] → returns 0x10000 (1.0, no scaling). Outside detent:
// scale = (targetBeats × beatTicks) / naturalSum, clamped to a reasonable
// range. Returns 0x10000 (no-op) if naturalSum is 0 to avoid div-by-zero.
uint32_t computeFeelScaleQ16(int feel, uint32_t naturalSum, uint32_t beatTicks);

// Convenience: multiply a tick value by a Q16.16 scale factor.
inline uint32_t applyFeelScale(uint32_t ticks, uint32_t scaleQ16) {
    return uint32_t((uint64_t(ticks) * scaleQ16) >> 16);
}

// Select the rank a mask filter should use for the event currently being
// played.
//
// Codex adversarial review 2026-05-22: when Repeat fires, the engine replays
// `_lastEvent` instead of the slot's stored event. The cache's per-slot rank
// then refers to the *wrong* material — the current readIndex's event, not
// the repeated event. `_lastEvent` carries its own `densityRank` (legacy
// rank field, copied at capture time), so the right rank source for a
// repeated event is `eval.densityRank()`, not `cache.aux[parentCacheIdx]`.
//
// Inputs:
//   useRepeat        — true if engine is about to play a Repeat replay.
//   eventDensityRank — eval.densityRank() of the (possibly repeated) event.
//   readIndex        — current sequence slot, 0..kMaxEventSlots-1.
//   cache            — engine's per-track cache.
//
// Returns the rank value that should be compared against the mask threshold.
inline uint32_t selectMaskRank(bool useRepeat,
                               uint8_t eventDensityRank,
                               int readIndex,
                               const Cache &cache) {
    if (useRepeat) {
        // The audible material is _lastEvent, whose densityRank travels with
        // it. Slot-keyed cache rank would filter unrelated content.
        return uint32_t(eventDensityRank);
    }
    uint8_t cacheIdx = (readIndex >= 0 && readIndex < int(kMaxEventSlots))
                     ? cache.parentCacheIdx[readIndex]
                     : uint8_t(0xff);
    if (cacheIdx < kCellCap) {
        return uint32_t(cache.aux[cacheIdx].rank());
    }
    // Fallback for unprimed cache (engine reset, no refresh has run yet).
    return uint32_t(eventDensityRank);
}

} // namespace stochastic_cache
