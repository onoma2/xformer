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

// Burst spacing denominators — kept in sync by hand with the matching LUT in
// StochasticGenerator.cpp. The shared header refactor was out of scope when
// the cache was added; both call sites must move together if either changes.
static const int kBurstSpacingLut[] = { 2, 3, 4, 5, 6 };
// kBurstSpacingLutSize no longer needed in cache (picker exposed via
// StochasticGenerator::pickBurstSpacingSlot returns a clamped slot index).
// kMinBurstParentTicks was the old generator-side gate that silently killed
// burst at factory defaults — removed in Phase 16 P7. Cache uses the
// playable-cluster-cell floor (kMinChildGate) instead.
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
    (void)activeNotes; // BurstPitch Roll lands in Phase 16 P8 (B5); reroll degree for cluster cells using keyed RNG.

    const int first = std::max(0, std::min(int(seq.first()), int(seq.size()) - 1));
    const int last  = std::max(first, std::min(int(seq.last()),  int(seq.size()) - 1));
    if (last < first) return 0;

    // Phase 16 P7 (2026-05-23): flat-cell generator with shaping in cache.
    //
    // Per-cell duration is picked here via StochasticGenerator::pickDurationSlot
    // — NoteDuration + Variation knobs reshape playback on refreshCache without
    // a full mutate pass. event.durationIndex is no longer read; the generator
    // still writes it for the Repeat replay path (_lastEvent), but the
    // non-Repeat playback path is fully cache-driven.
    //
    // Per-cell burst roll: rolls sequence.burst() probability via keyed RNG.
    // On hit, picks BurstCount and BurstRate from their LUT pickers and starts
    // a cluster of `count + 1` cells with duration prev_dur / denom. Eligibility
    // check is "prev_dur / denom >= kMinChildGate" — judged on the actual
    // previous cell, not the event's would-be LUT slot. This is the law the
    // user expects: short cells can't sustain bursts; long cells can.

    auto lutTicks = [&](uint8_t durationIndex) -> uint32_t {
        auto frac = StochasticTrackEngine::getDurationFraction(int(durationIndex));
        uint32_t t = (uint64_t(divisor) * frac.num) / frac.den;
        return t > 0 ? t : 1;
    };

    uint32_t cycleTicks = 0;
    // Bootstrap prevDur with the first cell's natural pick. Cluster math at
    // slot 0 still has *something* to divide; if the first cell itself starts
    // a cluster, the cell's own LUT pick is used as prevDur. Subsequent cells
    // see the actual emitted prevDur from the cycle.
    uint32_t prevDur = 0;
    int clusterRemaining = 0;
    uint32_t clusterDur = 0;

    for (int i = first; i <= last; ++i) {
        if (cache.count >= kCellCap) break;  // hard cap; remaining slots dropped silently

        const StochasticSourceEvent &ev = seq.events()[i];

        // Per-cell keyed RNG so cache rebuilds are deterministic at fixed
        // seed (Loop replay stable) while keeping each cell's roll independent.
        Random cellRng(keyed_rng::cellSeed(seed, uint32_t(cache.count)));

        // Decide this cell's duration. Cluster overrides the per-cell pick.
        uint32_t cellDur;
        bool isClusterTail = false;   // continuation cell of an in-flight cluster
        if (clusterRemaining > 0) {
            cellDur = clusterDur;
            --clusterRemaining;
            isClusterTail = true;
        } else {
            const int picked = StochasticGenerator::pickDurationSlot(seq, cellRng);
            cellDur = lutTicks(uint8_t(picked));

            // Bootstrap prevDur with this cell's natural pick if we're at slot 0
            // (no prior cell to inherit from). Cluster math will divide it below.
            if (cache.count == 0) prevDur = cellDur;

            // Burst roll: probability gated by sequence.burst(). Eligibility =
            // resulting cluster cell duration ≥ kMinChildGate so we never
            // schedule sub-audible clusters.
            if (int(cellRng.nextRange(100)) < int(seq.burst())) {
                const int spacingSlot = StochasticGenerator::pickBurstSpacingSlot(
                    int(seq.burstRate()), cellRng);
                const int denom = kBurstSpacingLut[spacingSlot];
                const uint32_t candidate = prevDur / uint32_t(denom);
                if (candidate >= kMinChildGate) {
                    clusterDur = candidate;
                    cellDur = clusterDur;
                    // BurstCount picker returns the cluster count beyond the
                    // starter cell, matching the old per-event semantics.
                    int count = StochasticGenerator::pickBurstCount(int(seq.burstCount()), cellRng);
                    if (count > StochasticTrackEngine::kMaxChildren) {
                        count = StochasticTrackEngine::kMaxChildren;
                    }
                    clusterRemaining = count;
                }
            }
        }

        // Pitch resolution per cell. Hold (default) copies event.degree/octave
        // into cluster cells too. Roll (sequence.burstPitch() == Generate)
        // rerolls the cluster continuation cells via generateDegree using
        // the per-cell keyed RNG. The cluster STARTER (clusterRemaining was
        // 0 entering the branch above) keeps its own event pitch — Roll only
        // affects the trailing cells inside a burst.
        uint8_t cellDegree = uint8_t(ev.degree());
        uint8_t cellOctave = uint8_t(ev.octave());
        if (isClusterTail
            && bakeChildNotes
            && seq.burstPitch() == StochasticBurstPitch::Generate) {
            int lastDeg = -1;
            const int degAbs = StochasticGenerator::generateDegree(
                seq, *track, *scale, lastDeg, cellRng);
            const int notes = std::max(1, scale->notesPerOctave());
            cellDegree = uint8_t(degAbs % notes);
            cellOctave = uint8_t(std::min(int(kMaxOctave), degAbs / notes));
        }

        // Gate length per cell. Triangular distribution centered at 50% of
        // cell duration, widened by sequence.gateLength() (the same kernel
        // the engine used to apply at trigger time via pickGateLength). Now
        // baked at cache build so each cell owns its gate fraction.
        // gateFrac stored as 64ths-of-cell-duration → engine multiplies at
        // trigger: gateTicks = (durationTicks * gateFrac) / 64.
        uint8_t cellGateFrac;
        {
            int pct = 50;
            const int spread = std::max(0, std::min(100, int(seq.gateLength())));
            if (spread > 0) {
                int r1 = int(cellRng.nextRange(101));
                int r2 = int(cellRng.nextRange(101));
                int tri = (r1 + r2) / 2;
                pct = 50 + ((tri - 50) * spread) / 100;
            }
            if (pct < 10) pct = 10;
            if (pct > 100) pct = 100;
            int frac = (pct * 64) / 100;
            if (frac > int(kMaxGateLen)) frac = int(kMaxGateLen);
            cellGateFrac = uint8_t(frac);
        }

        // Phase 16 P6 (2026-05-23): store per-cell duration, not the
        // absolute cycle position. CachedCell::make clamps at kMaxCellDuration.
        // Phase 16 P8 (2026-05-23): cellGateFrac stored per cell, picked above.
        cache.cells[cache.count] = CachedCell::make(
            cellDur,
            cellDegree,
            cellOctave,
            cellGateFrac,
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
