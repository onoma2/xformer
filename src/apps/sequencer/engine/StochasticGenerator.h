#pragma once

#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"
#include "model/Scale.h"
#include "core/utils/Random.h"

class StochasticGenerator {
public:
    static void generateRhythm(StochasticSequence &sequence, const StochasticTrack &track, uint32_t seed);
    static void generateMelody(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed);
    static void mutateOne(StochasticSequence &sequence, StochasticTrack &track, const Scale &scale, int rootNote, Random &rng);
    static void generateDensityRanks(StochasticSequence &sequence, int size, uint32_t seed);
    static void generateChildren(StochasticSequence &sequence, int eventIndex, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng);

    static StochasticSourceEvent generateRhythmEvent(const StochasticTrack &track, Random &rng);
    static StochasticSourceEvent generateMelodyEvent(const StochasticTrack &track, const Scale &scale, int rootNote, int &lastDegree, Random &rng);
    static int generateDegree(const StochasticTrack &track, const Scale &scale, int &lastDegree, Random &rng);
    static int generateJumpOctave(const StochasticTrack &track, int currentJump, Random &rng);

private:
    static float betaDistributionSample(float x, float spread);
};
