#pragma once

#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"
#include "model/Scale.h"
#include "core/utils/Random.h"

class StochasticGenerator {
public:
    static void generatePattern(StochasticSequence &sequence, StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed);
    static void mutateOne(StochasticSequence &sequence, StochasticTrack &track, const Scale &scale, int rootNote, Random &rng);
    static void generateDensityRanks(StochasticTrack &track, int size, uint32_t seed);
    static void generateChildren(StochasticTrack &track, int eventIndex, const Scale &scale, int rootNote, Random &rng);

    static StochasticParentEvent generateParentEvent(const StochasticTrack &track, const Scale &scale, int rootNote, int &lastDegree, Random &rng);
    static int generateDegree(const StochasticTrack &track, const Scale &scale, int &lastDegree, Random &rng);
    static int generateJumpOctave(const StochasticTrack &track, int currentJump, Random &rng);

private:
    static float betaDistributionSample(float x, float spread);
};
