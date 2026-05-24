#pragma once

// Engine-side cache for stochastic playback. Bit-packed cell layout
// validated by TestStochasticCacheParity. The engine reads runtimeSteps[K]
// directly for each played step K; the cache is rebuilt only when stored
// material changes (renew, mutate, Size edit, shaping-knob edit via
// notifyStochasticShapingEdit).

#include <cstdint>
#include <cstddef>

class StochasticSequence;
class StochasticTrack;
class Scale;

namespace stochastic_cache {

// Audible-trigger floor in ticks (~16 ms @ 192 PPQN, 120 BPM). Minimum
// gate-on duration for triggered events.
constexpr uint32_t kMinAudibleGateTicks = 6;

// Minimum cluster cell duration in ticks (~32 ms @ 192 PPQN, 120 BPM). Sized
// above kMinAudibleGateTicks so the gate-length clamp inside a cluster cell
// leaves a discrete gate-off gap before the next cell's gate-on. Below this
// floor a Fit cluster auto-reduces its cell count, and an Overflow cluster
// refuses to fire — preventing the "machine-gun continuous gate" symptom
// where consecutive cluster gate-ons land same-tick as the previous gate-off
// and the envelope can't retrigger.
constexpr uint32_t kMinClusterCellTicks = 12;

// Per-cell duration cap (12-bit field). ~21 quarter-notes at PPQN=192. A
// cell whose natural duration exceeds this is clamped at build time —
// the cycle is the sum of all cells' durations, so unlike the old
// absolute-position scheme this cannot silently corrupt the playhead.
constexpr uint32_t kMaxCellDuration = 4095;

// 6-bit gate length encoded as LUT entry or fractional ticks (0..63).
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
// step-keyed (one cell per step). Pinned at 64 because CellAux::rank is
// a 6-bit field — widening it would let cells 64+ wrap their rank bits.
constexpr int kCellCap = 64;

// Cell stores its own duration in ticks; the engine sums durations for
// the cycle length. 12-bit field clamps at kMaxCellDuration.
struct RuntimeStep {
    uint32_t packed;

    uint32_t durationTicks() const { return  packed        & 0xfffu; }
    uint8_t  degree()        const { return (packed >> 12) & 0x7fu; }
    uint8_t  octave()        const { return (packed >> 19) & 0xfu;  }   // 4 bits
    uint8_t  gateLen()       const { return (packed >> 23) & 0x3fu; }
    bool     audible()       const { return (packed >> 29) & 1u; }
    bool     slide()         const { return (packed >> 30) & 1u; }
    bool     legato()        const { return (packed >> 31) & 1u; }

    static RuntimeStep make(uint32_t durationTicks, uint8_t degree, uint8_t octave,
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
        return RuntimeStep{r};
    }
};
static_assert(sizeof(RuntimeStep) == 4, "RuntimeStep must be 4 bytes");

// Auxiliary per-cell metadata. 1 byte; all bits currently reserved. Ranks
// live on events (event.densityRank), burst-child has no meaning under the
// flat cell model. The byte stays so a future per-cell flag has somewhere
// cheap to land.
struct CellAux {
    uint8_t packed;

    static CellAux make() { return CellAux{0}; }
};
static_assert(sizeof(CellAux) == 1, "CellAux must be 1 byte");

// Cache footprint per active track:
//   RuntimeStep[64] = 256 B
//   CellAux[64]    =  64 B
//   Total          = 320 B

// Max stored steps in a sequence — matches CONFIG_STEP_COUNT but the header
// avoids pulling that in (Cache is consumed by tests too).
constexpr int kMaxEventSlots = 64;

struct StepCache {
    RuntimeStep runtimeSteps[kCellCap];
    CellAux     aux[kCellCap];

    uint8_t     count;
    // Sum of per-cell durations. 32-bit so dense long-duration patterns
    // (worst case 64 × kMaxCellDuration = 262 080) reach Feel scaling
    // without truncation.
    uint32_t    cycleTicks;
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
int rebuildStepCache(StepCache &cache, const StochasticSequence &seq, uint32_t divisor, uint32_t seed,
                              const Scale *scale = nullptr, const StochasticTrack *track = nullptr, int rootNote = 0);

// Compute the Feel scaling factor as Q16.16. Detent [45..55] → 1.0 (no
// scaling). Outside detent: scale = (targetBeats × beatTicks) / naturalSum,
// clamped to 1/4 .. 4×. Returns 1.0 when naturalSum is 0.
uint32_t computeFeelScaleQ16(int feel, uint32_t naturalSum, uint32_t beatTicks);

// Convenience: multiply a tick value by a Q16.16 scale factor.
inline uint32_t applyFeelScale(uint32_t ticks, uint32_t scaleQ16) {
    return uint32_t((uint64_t(ticks) * scaleQ16) >> 16);
}

} // namespace stochastic_cache
