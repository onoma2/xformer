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
    // Step-keyed cache: each step owns runtimeSteps[K]. Engine reads
    // cache.runtimeSteps[stepIndex] directly; validity = stepIndex < count.

    const bool bakeChildNotes = (scale != nullptr) && (track != nullptr);

    // Step-keyed walk: iterate the full pattern extent 0..size-1. First is
    // not consulted here — it bounds playback at the engine, not the cache
    // build. Step K's cell is always runtimeSteps[K]; cluster state carries across
    // steps; content depends only on (seed, events, step index).
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
    int clusterIdx = 0;
    int clusterTotal = 0;
    // Sized kMaxBurst + 1 because count from pickBurstCount is the tail count
    // (legacy contract preserved); Fit-mode total cells = anchor + tails.
    uint32_t clusterDurs[StochasticTrackEngine::kMaxBurst + 1] = {};

    // Map BurstRate 0..100 to a log-symmetric curve factor r ∈ [0.4..2.5].
    // r=1.0 at knob=50 (uniform — today's behavior). r<1 = first cell long,
    // tail shrinks (accel into climax). r>1 = first cell short, tail grows
    // (drag flam into long note). See PHASE15/16 trap-flam discussion.
    auto curveFromKnob = [](int knob) -> float {
        const float k = float(std::max(0, std::min(100, knob)));
        // ln(2.5) ≈ 0.9163. exponent ranges -0.9163..+0.9163 across knob 0..100.
        const float expo = 0.9163f * (k - 50.0f) / 50.0f;
        return std::exp(expo);
    };

    // Fit-mode cluster sizing: distribute prev_dur across N cells via curve.
    // Reserves a kMinAudibleGateTicks floor per cell so extreme r still
    // produces audible hits; rest of prev_dur scales by r^k.
    auto buildFitCluster = [&](uint32_t total, int cells, float r) {
        if (cells <= 0) cells = 1;
        if (cells > StochasticTrackEngine::kMaxBurst + 1) cells = StochasticTrackEngine::kMaxBurst + 1;
        // Reserve enough room per cell for a gate-on + audible gap before the
        // next gate-on. Curve distributes the remaining budget.
        const int floorPerCell = stochastic_cache::kMinClusterCellTicks;
        const int reserved = floorPerCell * cells;
        int budget = int(total) - reserved;
        if (budget < 0) budget = 0;
        float weights[StochasticTrackEngine::kMaxBurst + 1];
        float sum = 0.f;
        float rk = 1.f;
        for (int k = 0; k < cells; ++k) {
            weights[k] = rk;
            sum += rk;
            rk *= r;
        }
        if (sum <= 0.f) sum = 1.f;
        for (int k = 0; k < cells; ++k) {
            clusterDurs[k] = uint32_t(floorPerCell) + uint32_t(int(float(budget) * weights[k] / sum));
        }
        clusterTotal = cells;
    };

    // Mode-gated pick paths. Loop = cache picks per cell from keyed RNG
    // (knob movement reshapes deterministically). Live = engine writes
    // fresh content per trigger into the events array; cache reads those
    // values verbatim.
    const bool pickMelodyInCache =
        bakeChildNotes && seq.melodyMode() == StochasticSourceMode::Loop;
    const bool pickRhythmInCache =
        seq.rhythmMode() == StochasticSourceMode::Loop;

    // Pitch chain threads the per-step state (lastDegree + class recency)
    // through every step including cluster tails so Complexity / Contour /
    // recency penalty see continuous motion across the whole cycle.
    StochasticGenerator::PitchGenState pitchState{};
    uint8_t anchorDegree = 0;
    uint8_t anchorOctave = 0;
    bool anchorLegato = false;
    bool anchorSlide  = false;
    bool anchorAudible = true;

    for (int i = 0; i < size; ++i) {
        if (i >= kCellCap) break;  // hard cap; remaining steps dropped silently

        const StochasticStepContent &ev = seq.steps()[i];

        // Per-cell keyed RNG. Step K's cell content is a pure function of
        // (rhythmSeed, K); First / Size do not shift it.
        Random stepRng(keyed_rng::cellSeed(seed, uint32_t(i)));

        // Decide this cell's duration. Cluster overrides the per-cell pick.
        uint32_t cellDur;
        bool isClusterTail = false;   // continuation cell of an in-flight cluster
        const bool burstFit = burstHoldIsFit(seq.burstHold());

        if (clusterRemaining > 0) {
            // Read this cell's duration from the cluster's pre-built array.
            const int idx = std::min(clusterIdx, int(StochasticTrackEngine::kMaxBurst) - 1);
            cellDur = clusterDurs[idx];
            ++clusterIdx;
            --clusterRemaining;
            isClusterTail = true;
        } else if (pickRhythmInCache) {
            // Loop: pick fresh per cell. NoteDuration + Variation knobs
            // reshape the duration; Burst + Count + Rate knobs decide
            // cluster firing — all deterministic in the seed.
            const int picked = StochasticGenerator::pickDurationSlot(seq, stepRng);
            cellDur = lutTicks(uint8_t(picked));

            // Bootstrap prevDur at step 0 so a cluster starting at step 0
            // has something to divide. Subsequent steps see the actual
            // emitted prev.
            if (i == 0) prevDur = cellDur;

            // Burst roll fires per cell. The two timing-mode branches (Fit /
            // Overflow) build different cluster duration arrays.
            if (int(stepRng.nextRange(100)) < int(seq.burst())) {
                // pickBurstCount returns TOTAL cluster cells (anchor + tails)
                // per the BurstCount LUT contract {2..8}.
                int total = StochasticGenerator::pickBurstCount(int(seq.burstCount()), stepRng);
                const int totalMax = StochasticTrackEngine::kMaxBurst + 1;
                if (total > totalMax) total = totalMax;
                if (total < 2) total = 2;

                if (burstFit) {
                    // Fit: auto-reduce cell count if prev_dur can't house the
                    // requested density at the cluster-cell floor. Below 2
                    // fittable cells the cluster doesn't fire and the cell
                    // stays ordinary.
                    int maxFittable = int(prevDur / stochastic_cache::kMinClusterCellTicks);
                    if (maxFittable < 2) {
                        // No room for anchor + tail at the floor — skip cluster.
                    } else {
                        if (total > maxFittable) total = maxFittable;
                        const int tails = total - 1;
                        const float r = curveFromKnob(int(seq.burstRate()));
                        buildFitCluster(prevDur, total, r);
                        cellDur = clusterDurs[0];
                        clusterIdx = 1;
                        clusterRemaining = tails;
                    }
                } else {
                    // Overflow: existing denom-pick uniform clusterDur.
                    const int spacingSlot = StochasticGenerator::pickBurstSpacingSlot(
                        int(seq.burstRate()), stepRng);
                    const int denom = kBurstSpacingLut[spacingSlot];
                    const uint32_t candidate = prevDur / uint32_t(denom);
                    if (candidate >= stochastic_cache::kMinClusterCellTicks) {
                        const int tails = total - 1;
                        for (int k = 0; k < total; ++k) clusterDurs[k] = candidate;
                        clusterTotal = total;
                        clusterIdx = 1;
                        clusterRemaining = tails;
                        cellDur = candidate;
                    }
                }
            }
        } else {
            // Live: engine wrote fresh content into events[i] this trigger.
            cellDur = lutTicks(uint8_t(ev.durationIndex()));
            if (i == 0) prevDur = cellDur;

            if (int(ev.burstTails()) > 0) {
                // burstTails stored = tails count (1..7); total = tails + 1.
                int tails = int(ev.burstTails());
                if (tails > StochasticTrackEngine::kMaxBurst) tails = StochasticTrackEngine::kMaxBurst;
                if (tails < 1) tails = 1;
                int total = tails + 1;

                if (burstFit) {
                    int maxFittable = int(prevDur / stochastic_cache::kMinClusterCellTicks);
                    if (maxFittable < 2) {
                        // No room for cluster — fall back to ordinary cell.
                    } else {
                        if (total > maxFittable) total = maxFittable;
                        const int actualTails = total - 1;
                        const float r = curveFromKnob(int(seq.burstRate()));
                        buildFitCluster(prevDur, total, r);
                        cellDur = clusterDurs[0];
                        clusterIdx = 1;
                        clusterRemaining = actualTails;
                    }
                } else {
                    int spacingSlot = int(ev.burstRate());
                    if (spacingSlot < 0) spacingSlot = 0;
                    if (spacingSlot >= int(sizeof(kBurstSpacingLut)/sizeof(kBurstSpacingLut[0]))) {
                        spacingSlot = int(sizeof(kBurstSpacingLut)/sizeof(kBurstSpacingLut[0])) - 1;
                    }
                    const int denom = kBurstSpacingLut[spacingSlot];
                    const uint32_t candidate = prevDur / uint32_t(denom);
                    if (candidate >= stochastic_cache::kMinClusterCellTicks) {
                        for (int k = 0; k < total; ++k) clusterDurs[k] = candidate;
                        clusterTotal = total;
                        clusterIdx = 1;
                        clusterRemaining = tails;
                        cellDur = candidate;
                    }
                }
            }
        }

        // Loop melody: precompute step K's anchor pitch for EVERY step,
        // advancing chainLastDeg through all of them. Anchor pitch is a
        // pure function of (melodySeed, K); cluster placement does not
        // shift which pitch lands where.
        uint8_t slotDegree = 0;
        uint8_t slotOctave = 0;
        if (pickMelodyInCache) {
            Random anchorRng(keyed_rng::cellSeed(seq.melodySeed(), uint32_t(i)));
            const int degAbs = StochasticGenerator::generateDegree(
                seq, *track, *scale, pitchState, anchorRng);
            const int notes = std::max(1, scale->notesPerOctave());
            slotDegree = uint8_t(degAbs % notes);
            slotOctave = uint8_t(std::min(int(kMaxOctave), degAbs / notes));
        }

        uint8_t cellDegree;
        uint8_t cellOctave;
        if (isClusterTail) {
            if (bakeChildNotes && burstHoldIsRoll(seq.burstHold())) {
                // Cluster-tail Generate pitch lives in the melody domain
                // (melodySeed-keyed) so NewR does not shift cluster-tail
                // pitches. Distinct salt from the anchor RNG so the two
                // streams stay independent.
                Random tailRng(keyed_rng::cellSeed(seq.melodySeed(), uint32_t(i)) ^ 0x7A11C0DEu);
                StochasticGenerator::PitchGenState tailState{};
                const int degAbs = StochasticGenerator::generateDegree(
                    seq, *track, *scale, tailState, tailRng);
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

        // Rest pick — Loop mode rolls fresh per cell from a salted RNG so
        // the Rest knob is scrubbable and the rest stream is independent of
        // the duration stream. Live mode reads the stored event.rest bit
        // (engine writes fresh content per trigger). Cluster tails inherit
        // the anchor's rest decision so a cluster behaves as one gesture.
        bool cellAudible;
        if (isClusterTail) {
            cellAudible = anchorAudible;
        } else if (pickRhythmInCache) {
            const int restKnob = std::max(0, std::min(100, int(seq.rest())));
            Random restRng(keyed_rng::cellSeed(seq.rhythmSeed(), uint32_t(i)) ^ 0x5E57DEADu);
            const bool restHit = (restKnob > 0 && int(restRng.nextRange(100)) < restKnob);
            cellAudible = !restHit;
            anchorAudible = cellAudible;
        } else {
            cellAudible = !ev.rest();
            anchorAudible = cellAudible;
        }

        // Step-keyed write: runtimeSteps[K] for step K.
        cache.runtimeSteps[i] = RuntimeStep::make(
            cellDur,
            cellDegree,
            cellOctave,
            cellGateFrac,
            cellSlide,
            cellLegato,
            cellAudible);

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
