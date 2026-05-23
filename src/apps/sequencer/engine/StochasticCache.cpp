#include "StochasticCache.h"
#include "StochasticTrackEngine.h"   // for getDurationFraction
#include "StochasticGenerator.h"     // for generateDegree (burst-child notes)
#include "KeyedRng.h"
#include "Config.h"                  // CONFIG_PPQN for Feel beat-ticks

#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"
#include "model/StochasticTypes.h"
#include "model/Scale.h"

#include "core/utils/Random.h"

#include <algorithm>

namespace stochastic_cache {

// Match StochasticGenerator.cpp burst LUTs. Kept in sync by hand for Patch A;
// shared header refactor is out of scope for this patch.
static const int kBurstSpacingLut[] = { 2, 3, 4, 5, 6 };
static const int kBurstSpacingLutSize = 5;
static constexpr uint32_t kMinBurstParentTicks = 96;
static constexpr uint32_t kMinChildGate = 6;

// Encode a duration LUT slot into the 6-bit cell field. Patch A uses the slot
// directly (0..7); higher bits unused. Future patches may pack gateLen as a
// fraction of relTick to a cell-edge encoding.
static uint8_t encodeGateLenAsDurSlot(uint8_t durationIndex) {
    return durationIndex & 0x3fu;
}

uint32_t computeFeelScaleQ16(int feel, uint32_t naturalSum, uint32_t beatTicks) {
    constexpr uint32_t kOneQ16 = 0x10000;
    // Detent: [45..55] → no scaling.
    if (feel >= 45 && feel <= 55) return kOneQ16;
    if (naturalSum == 0) return kOneQ16;
    if (beatTicks == 0)  return kOneQ16;

    // Target beats per cycle: linear from 3 (feel=0) to 5 (feel=100) outside
    // detent. Within detent ([45..55]) this branch isn't taken.
    //   feel < 45:  targetBeats = lerp(4, 3, (45-feel)/45)
    //   feel > 55:  targetBeats = lerp(4, 5, (feel-55)/45)
    // Both branches converge to 4.0 at the detent edges.
    float targetBeats;
    if (feel < 45) {
        targetBeats = 4.f - float(45 - feel) / 45.f;          // → 3 at feel=0
    } else {
        targetBeats = 4.f + float(feel - 55) / 45.f;          // → 5 at feel=100
    }

    float targetTicks = targetBeats * float(beatTicks);
    float scale = targetTicks / float(naturalSum);

    // Clamp to a reasonable range (1/4 .. 4×). Prevents catastrophic
    // compression when naturalSum is wildly off target (e.g. NoteDuration
    // pushed to extremes).
    if (scale < 0.25f) scale = 0.25f;
    if (scale > 4.0f)  scale = 4.0f;

    return uint32_t(scale * float(kOneQ16) + 0.5f);
}

int regenerateCacheFromEvents(Cache &cache, const StochasticSequence &seq, uint32_t divisor, uint32_t seed,
                              const Scale *scale, const StochasticTrack *track, int rootNote) {
    (void)rootNote; // unused — generateDegree consumes scale + track only, root is applied at trigger time
    (void)seed;     // unused under flat — no per-cell keyed RNG needed (clusters use deterministic prev-dur math)
    cache.count = 0;
    cache.cycleTicks = 0;
    for (int i = 0; i < kMaxEventSlots; ++i) cache.parentCacheIdx[i] = 0xff;

    const bool bakeChildNotes = (scale != nullptr) && (track != nullptr);
    const int activeNotes = bakeChildNotes ? std::max(1, scale->notesPerOctave()) : 1;
    (void)activeNotes; // currently unused under flat — Roll mode generated pitch lands here in P7 when wired

    const int first = std::max(0, std::min(int(seq.first()), int(seq.size()) - 1));
    const int last  = std::max(first, std::min(int(seq.last()),  int(seq.size()) - 1));
    if (last < first) return 0;

    // Phase 16 flat-cell generator (2026-05-23).
    //
    // One cache cell per event slot. No parent/child distinction. Bursts
    // become a duration-bias state running across consecutive cells: when
    // an event starts a cluster, the next K cells take a duration derived
    // from the previous cell's duration ÷ a burst-rate denominator.
    //
    // State carried across the walk:
    //   prevDur:           ticks the previous cell occupied (or its LUT pick
    //                      if cluster forced a different value). Used to
    //                      derive cluster duration via the denom math.
    //   clusterRemaining:  cells left in the current cluster (including the
    //                      current one — decremented after each emit).
    //   clusterDur:        the duration cluster cells take while the
    //                      cluster is active.
    //
    // Cycle length = sum of every cell's duration. No "burst inside parent"
    // anymore; clusters consume slot budget.

    auto lutTicks = [&](uint8_t durationIndex) -> uint32_t {
        auto frac = StochasticTrackEngine::getDurationFraction(int(durationIndex));
        uint32_t t = (uint64_t(divisor) * frac.num) / frac.den;
        return t > 0 ? t : 1;
    };

    uint32_t cycleTicks = 0;
    uint32_t prevDur = lutTicks(seq.events()[first].durationIndex());  // bootstrap
    int clusterRemaining = 0;
    uint32_t clusterDur = 0;

    for (int i = first; i <= last; ++i) {
        if (cache.count >= kCellCap) break;  // hard cap; remaining slots dropped silently

        const StochasticSourceEvent &ev = seq.events()[i];

        // Decide this cell's duration. Cluster overrides the natural LUT pick.
        uint32_t cellDur;
        if (clusterRemaining > 0) {
            cellDur = clusterDur;
            --clusterRemaining;
        } else {
            cellDur = lutTicks(ev.durationIndex());

            // If this event starts a cluster, derive cluster duration from
            // the PREVIOUS cell's emitted duration (relative-to-context
            // semantics). The cluster's first cell IS this one.
            int childCount = int(ev.childCount());
            if (childCount > 0) {
                int spacingSlot = int(ev.burstRate());
                if (spacingSlot < 0) spacingSlot = 0;
                if (spacingSlot >= kBurstSpacingLutSize) spacingSlot = kBurstSpacingLutSize - 1;
                int denom = kBurstSpacingLut[spacingSlot];
                clusterDur = prevDur / uint32_t(denom);
                if (clusterDur < kMinChildGate) clusterDur = kMinChildGate;

                cellDur = clusterDur;
                // BurstCount in today's events means "number of children" =
                // additional cells beyond the cluster-starting one. Under
                // flat we count this cell as the cluster start, so K more
                // follow. Total cluster span = childCount + 1 cells.
                clusterRemaining = childCount;
                // childCount semantics: capped at kMaxChildren today; keep
                // the same bound so flat behaves like burst.
                if (clusterRemaining > StochasticTrackEngine::kMaxChildren) {
                    clusterRemaining = StochasticTrackEngine::kMaxChildren;
                }
            }
        }

        // Pitch resolution per cell. For Roll mode in clusters, we'd reroll
        // here using keyed RNG — but for now keep simple: every cell uses
        // its own event.degree()/.octave(). The cluster Hold vs Roll
        // distinction lands when generator gains keyed-RNG melody (next
        // session). For Hold semantics today: events written by mutate keep
        // their degree even inside clusters, which behaves like Hold.
        uint8_t cellDegree = uint8_t(ev.degree());
        uint8_t cellOctave = uint8_t(ev.octave());

        // Gate length: 64ths-of-cell-duration. Default 32 (50%) for now;
        // user gateLength knob modulation lands as a refinement after the
        // flat model is verified.
        constexpr uint8_t kDefaultGateFraction = 32;

        // Phase 16 P6 (2026-05-23): store per-cell duration, not the
        // absolute cycle position. CachedCell::make clamps at kMaxCellDuration.
        cache.cells[cache.count] = CachedCell::make(
            cellDur,
            cellDegree,
            cellOctave,
            kDefaultGateFraction,
            ev.slide(),
            ev.legato(),
            /*audible*/ !ev.rest());

        cache.aux[cache.count] = CellAux::make(
            /*burstChild*/ false,   // no children under flat — flag retained for binary compat
            /*rank*/       0);

        // 1:1 mapping: event slot K maps to cache cell (K - first).
        if (i < kMaxEventSlots) {
            cache.parentCacheIdx[i] = cache.count;
        }

        ++cache.count;
        cycleTicks += cellDur;
        prevDur = cellDur;
    }

    cache.cycleTicks = uint16_t(std::min(cycleTicks, uint32_t(UINT16_MAX)));

    // Phase 16 P6 (2026-05-23): Feel scaling is no longer baked at cache
    // build — engine computes it per trigger from sequence.feel(), so a
    // routed CV change takes effect without a cache refresh.

    recomputeCacheRanks(cache, seed, /*tilt*/ 0);
    return cache.count;
}

// Compute deterministic ranks. Tilt biases cells along a long/short
// duration axis; keyed hash provides a tie-break salt smaller than any
// non-zero tilt step.
//
// Phase 16 P6 (2026-05-23): cell holds its own duration directly, so
// rank weighting reads it without the old relTick-delta dance.
void recomputeCacheRanks(Cache &cache, uint32_t seed, int tilt) {
    if (cache.count == 0) return;

    struct Weighted { uint8_t idx; int32_t weight; };
    Weighted weighted[kCellCap];

    const int tiltClamped = std::max(-100, std::min(100, tilt));

    uint32_t maxDur = 1;
    for (uint8_t i = 0; i < cache.count; ++i) {
        uint32_t d = cache.cells[i].durationTicks();
        if (d > maxDur) maxDur = d;
    }

    for (uint8_t i = 0; i < cache.count; ++i) {
        // longShortAxis: normalize duration to ±7 range centered on midpoint.
        //   shortest cell → -7, longest cell → +7.
        // Tilt > 0 + long cell → high bias → high rank → gets Mask-cut first.
        // Mask cuts HIGH ranks, so for "long survives Mask" we need long → LOW
        // rank. Today's contract: bias = tilt × (longShortAxis). Tilt > 0 and
        // long cell → positive bias → high rank → cut. So semantically
        // tilt > 0 cuts LONG cells first; "survive Mask" = low rank = short
        // cells under +tilt. Reverse for -tilt. Same shape as legacy
        // generateMaskRanks weighting.
        int32_t longShortFP14;
        if (maxDur <= 1) {
            longShortFP14 = 0;
        } else {
            // Map duration ∈ [1, maxDur] → [+7, -7] linear (long → low/
            // negative, short → high/positive). This sign matches the
            // legacy parent/child code path: under tilt > 0, long cells
            // get NEGATIVE bias → LOW rank → SURVIVE Mask (Mask cuts
            // high ranks). Short cells get HIGH rank → cut first.
            longShortFP14 = 7 - int32_t((int64_t(cache.cells[i].durationTicks()) * 14) / maxDur);
        }
        const int32_t biasTerm = int32_t(tiltClamped) * longShortFP14 * 100;

        // Hash salt: top byte of keyed seed × small constant. Always positive,
        // smaller magnitude than any non-zero tilt.
        const uint32_t hash = keyed_rng::cellSeed(seed, i);
        const int32_t saltTerm = int32_t(hash >> 24);   // 0..255

        weighted[i] = Weighted{ i, biasTerm + saltTerm };
    }

    std::sort(weighted, weighted + cache.count, [](const Weighted &a, const Weighted &b) {
        return a.weight < b.weight;
    });

    for (uint8_t k = 0; k < cache.count; ++k) {
        cache.aux[weighted[k].idx].setRank(k);
    }
}

} // namespace stochastic_cache
