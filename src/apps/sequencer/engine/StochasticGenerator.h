#pragma once

#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"
#include "model/Scale.h"
#include "core/utils/Random.h"

class StochasticGenerator {
public:
    static void generateRhythm(StochasticSequence &sequence, const StochasticTrack &track, uint32_t seed);
    static void generateMelody(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed);
    // Destructively re-roll one event inside [first..last] for the named
    // domain. Mutate knob is a probability gate; magnitude is not exposed.
    static void mutateRhythmOne(StochasticSequence &sequence, const StochasticTrack &track, Random &rng);
    static void mutateMelodyOne(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng);

    static void generateMaskRanks(StochasticSequence &sequence, int size, uint32_t seed);
    
    struct EvaluatedBurstNote {
        uint32_t tickOffset;
        uint32_t gateTicks;
        int note;
        bool valid;
        bool roll;      // baked Roll/Hold for this burst (playback reads this, not the live mode)
    };

    static void evaluateBurst(EvaluatedBurstNote *bursts, const StochasticSequence &sequence, const StochasticStepContent &event, const StochasticTrack &track, const Scale &scale, int rootNote, int anchorNote, uint32_t durationTicks, Random &rng, uint32_t burstHoldSeed, uint32_t burstHoldCell);

    // Pitch generation state passed through the per-slot picker. Tracks the
    // last absolute degree (for direction/contour) plus the recent class
    // history that drives Step 1's recency penalty. See PITCH-LAW-FINAL.md.
    struct PitchGenState {
        int lastDegree = -1;
        int lastClass = -1;
        int classRunLength = 0;
    };

    static StochasticStepContent generateRhythmEvent(const StochasticSequence &sequence, const StochasticTrack &track, Random &rng);
    static StochasticStepContent generateMelodyEvent(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, PitchGenState &state, Random &rng);
    static int generateDegree(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, PitchGenState &state, Random &rng);
    // Per-cell pickers called from the cache walk. Cache picks per-cell from
    // these so NoteDuration / Variation / Burst* knobs reshape playback on
    // the next refreshStepCache without a full regeneration pass.
    static int pickDurationSlot(const StochasticSequence &sequence, Random &rng);
    static int pickBurstCount(int knob, Random &rng);
    static int pickBurstSpacingSlot(int knob, Random &rng);

};
