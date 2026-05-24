#pragma once

// Engine-side cache for stochastic playback. Bit-packed cell layout
// validated by TestStochasticCacheParity. The engine reads cells[K]
// directly for each played slot K; the cache is rebuilt only when stored
// material changes (renew, mutate, Size edit, shaping-knob edit via
// notifyStochasticShapingEdit).

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

// Cells store scale-domain `degree` and `octave`, not resolved voltage —
// scale / rootNote / track.octave / track.transpose are layered in at
// trigger time, so those edits don't force a cache rebuild.
//
// Octave packed in 4 bits (0..15); practical stochastic octaves stay
// well below that. `audible` lives in the cell (not CellAux) so the aux
// entry stays 1 byte.
constexpr uint8_t kMaxOctave = 15;

// Hard cap on cells per cache. Matches kMaxEventSlots since the walk is
// slot-keyed (one cell per slot). Pinned at 64 because CellAux::rank is
// a 6-bit field — widening it would let cells 64+ wrap their rank bits.
constexpr int kCellCap = 64;

// Cell stores its own duration in ticks; the engine sums durations for
// the cycle length. 12-bit field clamps at kMaxCellDuration.
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

// Auxiliary per-cell metadata. 1 byte.
//   bits 0-5: rank (0..63) — currently unused; ranks live on events.
//   bit 6:    burst-child  — always 0 under the flat cell model.
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

// Cache footprint per active track:
//   CachedCell[64] = 256 B
//   CellAux[64]    =  64 B
//   Total          = 320 B

// Max event slots in a sequence — matches CONFIG_STEP_COUNT but the header
// avoids pulling that in (Cache is consumed by tests too).
constexpr int kMaxEventSlots = 64;

struct Cache {
    CachedCell cells[kCellCap];
    CellAux    aux[kCellCap];

    // Slot-keyed identity map: parentCacheIdx[K] == K for K < count;
    // 0xff outside that range. Kept for binary compat with earlier
    // parent/child layouts.
    uint8_t    parentCacheIdx[kMaxEventSlots];

    uint8_t    count;
    uint16_t   cycleTicks;
};

// Build the cache from the sequence's events array.
//
// `divisor`  — sequence divisor in ticks.
// `seed`     — rhythmSeed; keys cellRng for duration / burst / gate-length /
//              legato picks.
// `scale` / `track` / `rootNote` — needed only for melody-domain picks
//              (anchor pitch via generateDegree, cluster-tail Generate
//              pitch, slide). Pass nullptr to skip pitch baking.
// Returns number of cells written.
int regenerateCacheFromEvents(Cache &cache, const StochasticSequence &seq, uint32_t divisor, uint32_t seed,
                              const Scale *scale = nullptr, const StochasticTrack *track = nullptr, int rootNote = 0);

// Compute the Feel scaling factor as Q16.16. Detent [45..55] → 1.0 (no
// scaling). Outside detent: scale = (targetBeats × beatTicks) / naturalSum,
// clamped to 1/4 .. 4×. Returns 1.0 when naturalSum is 0.
uint32_t computeFeelScaleQ16(int feel, uint32_t naturalSum, uint32_t beatTicks);

// Convenience: multiply a tick value by a Q16.16 scale factor.
inline uint32_t applyFeelScale(uint32_t ticks, uint32_t scaleQ16) {
    return uint32_t((uint64_t(ticks) * scaleQ16) >> 16);
}

// Mask threshold reads the rank straight off the event. Repeat replays
// the captured `_lastEvent`, which carries its own densityRank from the
// slot it was captured from, so this works for both Repeat and non-Repeat
// trigger paths.
inline uint32_t selectMaskRank(bool /*useRepeat*/,
                               uint8_t eventDensityRank,
                               int /*readIndex*/,
                               const Cache & /*cache*/) {
    return uint32_t(eventDensityRank);
}

} // namespace stochastic_cache
