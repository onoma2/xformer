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

// 12-bit signed CV over ±5V → ~2.4mV/step. STM32 DAC is 12-bit so we lose
// nothing real. Stored as offset (0..4095) with midpoint 2048 = 0V.
constexpr int16_t kCvMidpoint = 2048;
constexpr int16_t kCvMin = 0;
constexpr int16_t kCvMax = 4095;

// 6-bit gate length encoded as LUT slot or fractional ticks (0..63).
constexpr uint8_t kMaxGateLen = 63;

constexpr int kCellCap = 64;

struct CachedCell {
    uint32_t packed;

    uint32_t relTick()       const { return  packed        & 0xfffu; }
    int      cv()            const { return int((packed >> 12) & 0xfffu) - kCvMidpoint; }
    uint8_t  gateLen()       const { return (packed >> 24) & 0x3fu; }
    bool     slide()         const { return (packed >> 30) & 1u; }
    bool     legato()        const { return (packed >> 31) & 1u; }

    static CachedCell make(uint32_t relTick, int cv, uint8_t gateLen, bool slide, bool legato) {
        uint32_t cvField = uint32_t(cv + kCvMidpoint) & 0xfffu;
        uint32_t r =  (relTick                 & 0xfffu)
                   | ((cvField                 & 0xfffu) << 12)
                   | ((uint32_t(gateLen)       & 0x3fu)  << 24)
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

struct Cache {
    CachedCell cells[kCellCap];
    CellAux    aux[kCellCap];
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

} // namespace stochastic_cache
