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

// Burst spacing denominators — must stay in sync with the matching LUT in
// StochasticGenerator.cpp.
static const int kBurstSpacingLut[] = { 2, 3, 4, 5, 6 };

// kMinAudibleGateTicks (see StochasticCache.h) is the cluster-cell floor.

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

int rebuildStepCache(StepCache &cache, const StochasticSequence &seq, uint32_t divisor, uint32_t seed,
                              const Scale *scale, const StochasticTrack *track, int rootNote) {
    (void)rootNote; // unused — generateDegree consumes scale + track only, root is applied at trigger time
    cache.count = 0;
    cache.cycleTicks = 0;
    // Slot-keyed cache: each step owns runtimeSteps[K]. Engine reads
    // cache.runtimeSteps[stepIndex] directly; validity = stepIndex < count.

    const bool bakeChildNotes = (scale != nullptr) && (track != nullptr);

    // Slot-keyed walk: iterate the full pattern extent 0..size-1. First is
    // not consulted here — it bounds playback at the engine, not the cache
    // build. Slot K's cell is always runtimeSteps[K]; cluster state carries across
    // slots; content depends only on (seed, events, slot index).
    const int size = std::max(0, std::min(int(seq.size()), int(kMaxEventSlots)));
    if (size <= 0) return 0;

    auto lutTicks = [&](uint8_t durationIndex) -> uint32_t {
        auto frac = StochasticTrackEngine::getDurationFraction(int(durationIndex));
        uint32_t t = (uint64_t(divisor) * frac.num) / frac.den;
        return t > 0 ? t : 1;
    };

    uint32_t cycleTicks = 0;
    // prevDur carries the last emitted cell's duration so a burst cluster
    // can derive its own cell duration as prev / denom. Bootstrapped from
    // the first cell's natural LUT pick the first time it's needed.
    uint32_t prevDur = 0;
    int clusterRemaining = 0;
    uint32_t clusterDur = 0;

    // Mode-gated pick paths. Loop = cache picks per cell from keyed RNG
    // (knob movement reshapes deterministically). Live = engine writes
    // fresh content per trigger into the events array; cache reads those
    // values verbatim.
    const bool pickMelodyInCache =
        bakeChildNotes && seq.melodyMode() == StochasticSourceMode::Loop;
    const bool pickRhythmInCache =
        seq.rhythmMode() == StochasticSourceMode::Loop;

    // Pitch chain threads lastDegree through every slot (including cluster
    // tails) so Complexity / Contour kernels see continuous motion that
    // does not depend on cluster placement.
    int chainLastDeg = -1;
    uint8_t anchorDegree = 0;
    uint8_t anchorOctave = 0;
    bool anchorLegato = false;
    bool anchorSlide  = false;

    for (int i = 0; i < size; ++i) {
        if (i >= kCellCap) break;  // hard cap; remaining slots dropped silently

        const StochasticStepContent &ev = seq.steps()[i];

        // Per-cell keyed RNG. Slot K's cell content is a pure function of
        // (rhythmSeed, K); First / Size do not shift it.
        Random stepRng(keyed_rng::cellSeed(seed, uint32_t(i)));

        // Decide this cell's duration. Cluster overrides the per-cell pick.
        uint32_t cellDur;
        bool isClusterTail = false;   // continuation cell of an in-flight cluster
        if (clusterRemaining > 0) {
            cellDur = clusterDur;
            --clusterRemaining;
            isClusterTail = true;
        } else if (pickRhythmInCache) {
            // Loop: pick fresh per cell. NoteDuration + Variation knobs
            // reshape the duration; Burst + Count + Rate knobs decide
            // cluster firing — all deterministic in the seed.
            const int picked = StochasticGenerator::pickDurationSlot(seq, stepRng);
            cellDur = lutTicks(uint8_t(picked));

            // Bootstrap prevDur at slot 0 so a cluster starting at slot 0
            // has something to divide. Subsequent slots see the actual
            // emitted prev.
            if (i == 0) prevDur = cellDur;

            // Burst roll. Reject if the resulting cluster cell would fall
            // below the audible-cell floor.
            if (int(stepRng.nextRange(100)) < int(seq.burst())) {
                const int spacingSlot = StochasticGenerator::pickBurstSpacingSlot(
                    int(seq.burstRate()), stepRng);
                const int denom = kBurstSpacingLut[spacingSlot];
                const uint32_t candidate = prevDur / uint32_t(denom);
                if (candidate >= kMinAudibleGateTicks) {
                    clusterDur = candidate;
                    cellDur = clusterDur;
                    int count = StochasticGenerator::pickBurstCount(int(seq.burstCount()), stepRng);
                    if (count > StochasticTrackEngine::kMaxBurst) {
                        count = StochasticTrackEngine::kMaxBurst;
                    }
                    clusterRemaining = count;
                }
            }
        } else {
            // Live: engine wrote fresh content into events[i] this trigger.
            // Read its durationIndex / childCount / burstRate so every event
            // is genuinely fresh (not seed-locked to the previous cycle).
            cellDur = lutTicks(uint8_t(ev.durationIndex()));
            if (i == 0) prevDur = cellDur;

            if (int(ev.childCount()) > 0) {
                int spacingSlot = int(ev.burstRate());
                if (spacingSlot < 0) spacingSlot = 0;
                if (spacingSlot >= int(sizeof(kBurstSpacingLut)/sizeof(kBurstSpacingLut[0]))) {
                    spacingSlot = int(sizeof(kBurstSpacingLut)/sizeof(kBurstSpacingLut[0])) - 1;
                }
                const int denom = kBurstSpacingLut[spacingSlot];
                const uint32_t candidate = prevDur / uint32_t(denom);
                if (candidate >= kMinAudibleGateTicks) {
                    clusterDur = candidate;
                    cellDur = clusterDur;
                    int count = int(ev.childCount());
                    if (count > StochasticTrackEngine::kMaxBurst) {
                        count = StochasticTrackEngine::kMaxBurst;
                    }
                    clusterRemaining = count;
                }
            }
        }

        // Loop melody: precompute slot K's anchor pitch for EVERY slot,
        // advancing chainLastDeg through all of them. Anchor pitch is a
        // pure function of (melodySeed, K); cluster placement does not
        // shift which pitch lands where.
        uint8_t slotDegree = 0;
        uint8_t slotOctave = 0;
        if (pickMelodyInCache) {
            Random anchorRng(keyed_rng::cellSeed(seq.melodySeed(), uint32_t(i)));
            const int degAbs = StochasticGenerator::generateDegree(
                seq, *track, *scale, chainLastDeg, anchorRng);
            const int notes = std::max(1, scale->notesPerOctave());
            slotDegree = uint8_t(degAbs % notes);
            slotOctave = uint8_t(std::min(int(kMaxOctave), degAbs / notes));
        }

        uint8_t cellDegree;
        uint8_t cellOctave;
        if (isClusterTail) {
            if (bakeChildNotes && seq.burstHold() == StochasticBurstHold::Roll) {
                // Cluster-tail Generate pitch lives in the melody domain
                // (melodySeed-keyed) so NewR does not shift cluster-tail
                // pitches. Distinct salt from the anchor RNG so the two
                // streams stay independent.
                Random tailRng(keyed_rng::cellSeed(seq.melodySeed(), uint32_t(i)) ^ 0x7A11C0DEu);
                int lastDeg = -1;
                const int degAbs = StochasticGenerator::generateDegree(
                    seq, *track, *scale, lastDeg, tailRng);
                const int notes = std::max(1, scale->notesPerOctave());
                cellDegree = uint8_t(degAbs % notes);
                cellOctave = uint8_t(std::min(int(kMaxOctave), degAbs / notes));
            } else {
                cellDegree = anchorDegree;
                cellOctave = anchorOctave;
            }
        } else if (pickMelodyInCache) {
            cellDegree = slotDegree;
            cellOctave = slotOctave;
            anchorDegree = cellDegree;
            anchorOctave = cellOctave;
        } else {
            cellDegree = uint8_t(ev.degree());
            cellOctave = uint8_t(ev.octave());
            anchorDegree = cellDegree;
            anchorOctave = cellOctave;
        }

        // Gate length per cell — triangular distribution around 50% of
        // cell duration, widened by Gate Length. Stored as 64ths so the
        // engine derives gateTicks = (durationTicks * gateFrac) / 64.
        uint8_t cellGateFrac;
        {
            int pct = 50;
            const int spread = std::max(0, std::min(100, int(seq.gateLength())));
            if (spread > 0) {
                int r1 = int(stepRng.nextRange(101));
                int r2 = int(stepRng.nextRange(101));
                int tri = (r1 + r2) / 2;
                pct = 50 + ((tri - 50) * spread) / 100;
            }
            if (pct < 10) pct = 10;
            if (pct > 100) pct = 100;
            int frac = (pct * 64) / 100;
            if (frac > int(kMaxGateLen)) frac = int(kMaxGateLen);
            cellGateFrac = uint8_t(frac);
        }

        // Legato is rhythm-domain (stepRng / rhythmSeed) — it's a gate /
        // tie behavior. Slide is melody-domain (slideRng / melodySeed)
        // since it's a pitch-glide gesture; keeping it under rhythmSeed
        // would mean NewR shifts which notes glide. Both pick per cell
        // Bernoulli against their knobs; cluster tails inherit the
        // anchor's decision so a cluster behaves as one gesture.
        bool cellLegato;
        bool cellSlide;
        if (isClusterTail) {
            cellLegato = anchorLegato;
            cellSlide  = anchorSlide;
        } else {
            const int legSpread = std::max(0, std::min(100, int(seq.legatoProb())));
            cellLegato = (legSpread > 0 && int(stepRng.nextRange(100)) < legSpread);
            const int slideSpread = std::max(0, std::min(100, int(seq.slide())));
            Random slideRng(keyed_rng::cellSeed(seq.melodySeed(), uint32_t(i)) ^ 0x511DE51Du);
            cellSlide  = (slideSpread > 0 && int(slideRng.nextRange(100)) < slideSpread);
            anchorLegato = cellLegato;
            anchorSlide  = cellSlide;
        }

        // Slot-keyed write: runtimeSteps[K] for step K.
        cache.runtimeSteps[i] = RuntimeStep::make(
            cellDur,
            cellDegree,
            cellOctave,
            cellGateFrac,
            cellSlide,
            cellLegato,
            /*audible*/ !ev.rest());

        cache.aux[i] = CellAux::make();

        ++cache.count;
        cycleTicks += cellDur;
        prevDur = cellDur;
    }

    cache.cycleTicks = cycleTicks;

    // Feel scaling is computed per trigger from sequence.feel(), not baked
    // here. Ranks live on event.densityRank, assigned by the generator at
    // RENEW / Patience / mutateRhythmOne.
    return cache.count;
}

} // namespace stochastic_cache
