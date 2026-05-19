#pragma once

#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"
#include "model/Scale.h"
#include "core/utils/Random.h"

class StochasticGenerator {
public:
    static void generateRhythm(StochasticSequence &sequence, const StochasticTrack &track, uint32_t seed);
    static void generateMelody(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed);
    static void mutateRhythmOne(StochasticSequence &sequence, const StochasticTrack &track, Random &rng);
    static void mutateMelodyOne(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng);
    static void generateDensityRanks(StochasticSequence &sequence, int size, int tilt, uint32_t seed);
    
    struct EvaluatedChild {
        uint32_t tickOffset;
        uint32_t gateTicks;
        int note;
        bool valid;
    };

    static void evaluateChildren(EvaluatedChild *children, const StochasticSequence &sequence, const StochasticSourceEvent &event, const StochasticTrack &track, const Scale &scale, int rootNote, int parentNote, uint32_t durationTicks, Random &rng);

    static StochasticSourceEvent generateRhythmEvent(const StochasticSequence &sequence, const StochasticTrack &track, Random &rng);
    static StochasticSourceEvent generateMelodyEvent(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, int &lastDegree, Random &rng);
    static int generateDegree(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int &lastDegree, Random &rng);
    static int generateJumpOctave(const StochasticSequence &sequence, const StochasticTrack &track, int currentJump, Random &rng);

private:
    static float betaDistributionSample(float x, float spread);
};
