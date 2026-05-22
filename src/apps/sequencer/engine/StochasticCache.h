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

namespace stochastic_cache {

// ~5 bars at PPQN=192 (4096 ticks). Reasonable upper bound for one cycle.
constexpr uint32_t kMaxRelTick = 4095;

// 6-bit gate length encoded as LUT slot or fractional ticks (0..63).
constexpr uint8_t kMaxGateLen = 63;

// Melody fields (Phase 14B Patch C step 1, 2026-05-22):
// Cells store scale-domain `degree` and `octave` rather than resolved voltage.
// Trigger-time scale + track-offset lookup matches today's engine, keeping
// the cache invariant to scale/rootNote/track.octave/track.transpose changes
// (no rebuild needed when those move). Range matches StochasticSourceEvent:
//   degree: 0..127  (7-bit unsigned)
//   octave: 0..31   (5-bit unsigned — engine adds _jumpRegister + track.octave()
//                   at trigger time for the signed offset case).
constexpr uint8_t kMaxOctave = 31;

constexpr int kCellCap = 64;

struct CachedCell {
    uint32_t packed;

    uint32_t relTick()       const { return  packed        & 0xfffu; }
    uint8_t  degree()        const { return (packed >> 12) & 0x7fu; }
    uint8_t  octave()        const { return (packed >> 19) & 0x1fu; }
    uint8_t  gateLen()       const { return (packed >> 24) & 0x3fu; }
    bool     slide()         const { return (packed >> 30) & 1u; }
    bool     legato()        const { return (packed >> 31) & 1u; }

    static CachedCell make(uint32_t relTick, uint8_t degree, uint8_t octave,
                           uint8_t gateLen, bool slide, bool legato) {
        if (octave > kMaxOctave) octave = kMaxOctave;
        uint32_t r =  (relTick                 & 0xfffu)
                   | ((uint32_t(degree)       & 0x7fu)  << 12)
                   | ((uint32_t(octave)       & 0x1fu)  << 19)
                   | ((uint32_t(gateLen)       & 0x3fu) << 24)
                   | ((slide  ? 1u : 0u)               << 30)
                   | ((legato ? 1u : 0u)               << 31);
        return CachedCell{r};
    }
};
static_assert(sizeof(CachedCell) == 4, "CachedCell must be 4 bytes");

// Auxiliary per-cell metadata. Kept in a parallel array because the cell
// itself has no spare bits. burst-child + audible live here so cell stays 4 B.
struct CellAux {
    uint8_t flags;  // bit0 audible, bit1 burst-child, bit2..7 reserved
    uint8_t rank;   // 0..kCellCap-1, sort index from recomputeCacheRanks

    bool audible()    const { return (flags >> 0) & 1u; }
    bool burstChild() const { return (flags >> 1) & 1u; }

    static CellAux make(bool audible, bool burstChild, uint8_t rank) {
        return CellAux{
            uint8_t((audible ? 1u : 0u) | (burstChild ? 2u : 0u)),
            rank,
        };
    }
};
static_assert(sizeof(CellAux) == 2, "CellAux must be 2 bytes");

// Cache footprint per active track:
//   CachedCell[64] = 256 B
//   CellAux[64]    = 128 B
//   Total          = 384 B
//
// (CellAux grew to 2 B because we store rank alongside flags. Aux remains
//  outside the cell to keep the cell at exactly 4 B for future ABI work.)

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
};

// Populate cache by walking an existing StochasticSequence's `_events[]` tape.
// Produces the same playback stream the current engine path would produce, so
// the parity test can match cache content against today's behavior.
//
// `divisor` is the sequence divisor in ticks (engine reads this from track).
// `seed` is the rhythmSeed used for deterministic rank assignment.
// Returns number of cells written.
int regenerateCacheFromEvents(Cache &cache, const StochasticSequence &seq, uint32_t divisor, uint32_t seed);

// Recompute ranks from keyed-hash salt + tilt. Cells must already be in
// cache; durationIndex is recovered from gateLen via the duration LUT.
// `tilt` is -100..+100.
void recomputeCacheRanks(Cache &cache, uint32_t seed, int tilt);

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
        return uint32_t(cache.aux[cacheIdx].rank);
    }
    // Fallback for unprimed cache (engine reset, no refresh has run yet).
    return uint32_t(eventDensityRank);
}

} // namespace stochastic_cache
