#include "StochasticGenerator.h"

#include <cmath>
#include <algorithm>

void StochasticGenerator::generateRhythm(StochasticSequence &sequence, const StochasticTrack &track, uint32_t seed) {
    Random rng(seed);
    int size = sequence.size();
    
    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        if (i < size) {
            auto rhythm = generateRhythmEvent(track, rng);
            sequence.events()[i].d0 = rhythm.d0;
            sequence.events()[i].d1.childCount = rhythm.d1.childCount;
            sequence.events()[i].d1.rest = rhythm.d1.rest;
            sequence.events()[i].d1.legato = rhythm.d1.legato;
            sequence.events()[i].d1.slide = rhythm.d1.slide;
            sequence.events()[i].d1.accent = rhythm.d1.accent;
            sequence.events()[i].d1.rhythmValid = rhythm.d1.rhythmValid;
        }
    }

    generateDensityRanks(sequence, size, seed ^ 0xdeadbeef);
    sequence.setRhythmValid(true);
    sequence.setRhythmSeed(seed);
}

void StochasticGenerator::generateMelody(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed) {
    Random rng(seed);
    int lastDegree = -1;
    int size = sequence.size();
    
    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        if (i < size) {
            auto melody = generateMelodyEvent(track, scale, rootNote, lastDegree, rng);
            sequence.events()[i].d1.degree = melody.d1.degree;
            sequence.events()[i].d1.octave = melody.d1.octave;
            sequence.events()[i].d1.melodyValid = melody.d1.melodyValid;
        }
    }

    sequence.setMelodyValid(true);
    sequence.setMelodySeed(seed);
}

void StochasticGenerator::mutateOne(StochasticSequence &sequence, StochasticTrack &track, const Scale &scale, int rootNote, Random &rng) {
    int size = sequence.size();
    if (size <= 0) return;

    int i = rng.nextRange(size);
    
    if (rng.nextRange(2) == 0) {
        auto rhythm = generateRhythmEvent(track, rng);
        sequence.events()[i].d0 = rhythm.d0;
        sequence.events()[i].d1.childCount = rhythm.d1.childCount;
        sequence.events()[i].d1.rest = rhythm.d1.rest;
        sequence.events()[i].d1.legato = rhythm.d1.legato;
        sequence.events()[i].d1.slide = rhythm.d1.slide;
        sequence.events()[i].d1.accent = rhythm.d1.accent;
        sequence.events()[i].d1.rhythmValid = rhythm.d1.rhythmValid;
    } else {
        int lastDegree = (i > 0) ? int(sequence.events()[i-1].d1.degree) : -1;
        auto melody = generateMelodyEvent(track, scale, rootNote, lastDegree, rng);
        sequence.events()[i].d1.degree = melody.d1.degree;
        sequence.events()[i].d1.octave = melody.d1.octave;
        sequence.events()[i].d1.melodyValid = melody.d1.melodyValid;
    }
}

void StochasticGenerator::generateDensityRanks(StochasticSequence &sequence, int size, uint32_t seed) {
    Random rng(seed);
    if (size <= 0) return;
    
    uint8_t indices[CONFIG_STEP_COUNT];
    for (int i = 0; i < size; ++i) indices[i] = i;

    for (int i = 0; i < size; ++i) {
        int target = i + rng.nextRange(size - i);
        std::swap(indices[i], indices[target]);
    }

    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) sequence.events()[i].d0.densityRank = 255;

    for (int i = 0; i < size; ++i) {
        sequence.events()[indices[i]].d0.densityRank = i;
    }
}

void StochasticGenerator::generateChildren(StochasticSequence &sequence, int eventIndex, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng) {
}

StochasticSourceEvent StochasticGenerator::generateRhythmEvent(const StochasticTrack &track, Random &rng) {
    StochasticSourceEvent event;
    event.clear();

    event.d0.rate = track.rate();
    event.d0.length = 128; 
    
    event.d1.rest = (int(rng.nextRange(100)) < track.rest());
    event.d1.legato = (int(rng.nextRange(100)) < track.legatoProb());
    event.d1.slide = (int(rng.nextRange(100)) < track.slide());
    event.d1.accent = (int(rng.nextRange(100)) < track.accentProb());
    event.d1.rhythmValid = true;

    event.d1.childCount = 0;
    if (int(rng.nextRange(100)) < track.burst()) {
        event.d1.childCount = 1 + (uint32_t(track.burstCount()) * 3) / 100; 
    }

    return event;
}

StochasticSourceEvent StochasticGenerator::generateMelodyEvent(const StochasticTrack &track, const Scale &scale, int rootNote, int &lastDegree, Random &rng) {
    StochasticSourceEvent event;
    event.clear();

    int absoluteDegree = generateDegree(track, scale, lastDegree, rng);
    int activeNotes = scale.notesPerOctave();
    event.d1.degree = absoluteDegree % activeNotes;
    event.d1.octave = absoluteDegree / activeNotes;
    event.d1.melodyValid = true;

    return event;
}

int StochasticGenerator::generateDegree(const StochasticTrack &track, const Scale &scale, int &lastDegree, Random &rng) {
    int activeNotes = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
    int range = track.range();
    
    int allowedDegrees[CONFIG_USER_SCALE_SIZE * 4];
    int penalizedWeights[CONFIG_USER_SCALE_SIZE * 4];
    int allowedCount = 0;
    int totalTickets = 0;

    for (int oct = 0; oct < range; ++oct) {
        for (int i = 0; i < activeNotes; ++i) {
            int tickets = track.degreeTicket(i);
            if (tickets >= 0) {
                int deg = oct * activeNotes + i;
                if (deg >= track.minDegree() && deg <= track.maxDegree()) {
                    allowedDegrees[allowedCount] = deg;
                    penalizedWeights[allowedCount] = tickets;
                    totalTickets += tickets;
                    allowedCount++;
                }
            }
        }
    }

    if (allowedCount == 0) return 0;

    int degree = allowedDegrees[0];

    if (track.marblesMode() == StochasticTrack::MarblesMode::On) {
        float x = rng.nextRange(1000) / 1000.f;
        float shapedX = betaDistributionSample(x, track.marblesSpread() / 100.f);
        float bias = track.marblesBias() / 100.f;
        float targetIdxFloat = (shapedX - 0.5f) * (allowedCount) + (bias * allowedCount);
        int targetIdx = clamp(int(targetIdxFloat), 0, allowedCount - 1);
        degree = allowedDegrees[targetIdx];
    } else {
        int complexity = track.complexity();
        int contour = track.contour();
        int linearity = track.linearity();

        if (complexity < 50 && lastDegree != -1) {
            totalTickets = 0;
            for (int i = 0; i < allowedCount; ++i) {
                int dist = std::abs(allowedDegrees[i] - lastDegree);
                if (dist > (1 + complexity / 10)) {
                    penalizedWeights[i] /= 4;
                }
                totalTickets += penalizedWeights[i];
            }
        }

        if (contour != 0) {
            totalTickets = 0;
            for (int i = 0; i < allowedCount; ++i) {
                float pos = (2.0f * i / (allowedCount - 1)) - 1.0f;
                float bias = 1.0f + (pos * contour / 100.0f);
                penalizedWeights[i] = std::max(0, int(penalizedWeights[i] * bias));
                totalTickets += penalizedWeights[i];
            }
        }

        if (linearity > 0 && lastDegree != -1) {
            totalTickets = 0;
            for (int i = 0; i < allowedCount; ++i) {
                int dist = std::abs(allowedDegrees[i] - lastDegree);
                int penalty = (dist * linearity) / 10;
                penalizedWeights[i] = std::max(0, penalizedWeights[i] - penalty);
                totalTickets += penalizedWeights[i];
            }
        }

        if (totalTickets > 0) {
            int roll = rng.nextRange(totalTickets);
            int sum = 0;
            for (int i = 0; i < allowedCount; ++i) {
                sum += penalizedWeights[i];
                if (roll < sum) {
                    degree = allowedDegrees[i];
                    break;
                }
            }
        } else {
            degree = allowedDegrees[rng.nextRange(allowedCount)];
        }
    }

    lastDegree = degree;
    return degree;
}

int StochasticGenerator::generateJumpOctave(const StochasticTrack &track, int currentJump, Random &rng) {
    return 0;
}

float StochasticGenerator::betaDistributionSample(float x, float spread) {
    float normalizedSpread = clamp(spread, 0.0f, 1.0f);
    if (normalizedSpread == 0.5f) return x;
    
    if (normalizedSpread < 0.5f) {
        float p = 1.0f + (0.5f - normalizedSpread) * 4.0f;
        if (x < 0.5f) {
            return 0.5f * std::pow(2.0f * x, p);
        } else {
            return 1.0f - 0.5f * std::pow(2.0f * (1.0f - x), p);
        }
    } else {
        float p = 1.0f + (normalizedSpread - 0.5f) * 4.0f;
        if (x < 0.5f) {
            return 0.5f * (1.0f - std::pow(1.0f - 2.0f * x, p));
        } else {
            return 0.5f + 0.5f * std::pow(2.0f * x - 1.0f, p);
        }
    }
}
