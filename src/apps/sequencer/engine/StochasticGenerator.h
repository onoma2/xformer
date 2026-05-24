#pragma once

#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"
#include "model/Scale.h"
#include "core/utils/Random.h"

class StochasticGenerator {
public:
    static void generateRhythm(StochasticSequence &sequence, const StochasticTrack &track, uint32_t seed);
    static void generateMelody(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed);
    // Proteus-style: regenerate a single event with a fresh random roll.
    // mutateMagnitude (0..100) drives the bias strength of the replacement pick:
    //   low magnitude → strong anchor bias (close to base / tonal)
    //   high magnitude → uniform random across LUT (chaotic)
    // bias_strength = 100 - magnitude (inverse coupling).
    static void mutateRhythmOne(StochasticSequence &sequence, const StochasticTrack &track, Random &rng, int mutateMagnitude);
    static void mutateMelodyOne(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng, int mutateMagnitude);
    // Marbles-style: swap two existing events (reorders, preserves content).
    static void permuteRhythmOne(StochasticSequence &sequence, Random &rng);
    static void permuteMelodyOne(StochasticSequence &sequence, Random &rng);

    static void generateMaskRanks(StochasticSequence &sequence, int size, uint32_t seed);
    
    struct EvaluatedBurstNote {
        uint32_t tickOffset;
        uint32_t gateTicks;
        int note;
        bool valid;
    };

    static void evaluateBurst(EvaluatedBurstNote *children, const StochasticSequence &sequence, const StochasticSourceEvent &event, const StochasticTrack &track, const Scale &scale, int rootNote, int parentNote, uint32_t durationTicks, Random &rng);

    static StochasticSourceEvent generateRhythmEvent(const StochasticSequence &sequence, const StochasticTrack &track, Random &rng);
    static StochasticSourceEvent generateMelodyEvent(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, int &lastDegree, Random &rng);
    static int generateDegree(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int &lastDegree, Random &rng);
    // Per-cell pickers called from the cache walk. Cache picks per-cell from
    // these so NoteDuration / Variation / Burst* knobs reshape playback on
    // the next refreshCache without a full regeneration pass.
    static int pickDurationSlot(const StochasticSequence &sequence, Random &rng);
    static int pickBurstCount(int knob, Random &rng);
    static int pickBurstSpacingSlot(int knob, Random &rng);

};
