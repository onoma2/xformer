#include "StochasticGenerator.h"

#include <cmath>
#include <algorithm>

void StochasticGenerator::generatePattern(StochasticSequence &sequence, StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed) {
    Random rng(seed);
    int lastDegree = -1;
    int size = sequence.size();
    
    // Clear and generate parents
    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        if (i < size) {
            track.events()[i] = generateParentEvent(track, scale, rootNote, lastDegree, rng);
            generateChildren(track, i, scale, rootNote, rng);
        } else {
            track.events()[i].clear();
        }
    }

    // Generate density ranks using active Size
    generateDensityRanks(track, size, seed ^ 0xdeadbeef);
    
    sequence.setPatternValid(true);
}

void StochasticGenerator::mutateOne(StochasticSequence &sequence, StochasticTrack &track, const Scale &scale, int rootNote, Random &rng) {
    int size = sequence.size();
    if (size <= 0) return;

    int i = rng.nextRange(size);
    int lastDegree = (i > 0) ? int(track.events()[i-1].d0.degree) : -1;
    track.events()[i] = generateParentEvent(track, scale, rootNote, lastDegree, rng);
    generateChildren(track, i, scale, rootNote, rng);
}

void StochasticGenerator::generateDensityRanks(StochasticTrack &track, int size, uint32_t seed) {
    Random rng(seed);
    if (size <= 0) return;
    
    uint8_t indices[CONFIG_STEP_COUNT];
    for (int i = 0; i < size; ++i) indices[i] = i;

    int tilt = track.tilt();
    
    for (int i = 0; i < size; ++i) {
        int target = i + rng.nextRange(size - i);
        if (tilt != 0) {
            for (int retry = 0; retry < 2; ++retry) {
                int pos_score = (target * 200) / size - 100; 
                if ((tilt < 0 && pos_score > -tilt) || (tilt > 0 && pos_score < -tilt)) {
                    target = i + rng.nextRange(size - i);
                }
            }
        }
        std::swap(indices[i], indices[target]);
    }

    // Reset all ranks first
    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) track.events()[i].d1.densityRank = 255;

    // Assign unique ranks within the loop size
    for (int i = 0; i < size; ++i) {
        track.events()[indices[i]].d1.densityRank = i;
    }
}

void StochasticGenerator::generateChildren(StochasticTrack &track, int eventIndex, const Scale &scale, int rootNote, Random &rng) {
    auto &event = track.events()[eventIndex];
    event.d1.childFirst = eventIndex * 4;
    
    int count = event.d1.childCount;
    if (count <= 0) return;

    // Burst Rate defines spacing. 0 = spread across parent, 100 = fast burst at start.
    int burstRate = track.burstRate();
    
    for (int c = 0; c < count; ++c) {
        auto &child = track.children()[event.d1.childFirst + c];
        child.clear();
        
        // Pitch
        if (track.burstPitch() == StochasticBurstPitch::Parent) {
            child.degree = event.d0.degree;
            child.octave = event.d0.octave;
        } else {
            int childLast = event.d0.degree;
            int absoluteDegree = generateDegree(track, scale, childLast, rng);
            int activeNotes = scale.notesPerOctave();
            child.degree = absoluteDegree % activeNotes;
            child.octave = absoluteDegree / activeNotes;
        }

        // Timing
        // burstRate 0: offsets are [1/4, 2/4, 3/4] etc.
        // burstRate 100: offsets are very close [10, 20, 30]
        int spacing = 255 / (count + 1); // evenly spread base
        if (burstRate > 0) {
            int compression = (spacing * burstRate) / 100;
            spacing = std::max(8, spacing - compression);
        }
        
        child.offset = (c + 1) * spacing;
        child.length = 128;
        child.gate = true;
    }
}

StochasticParentEvent StochasticGenerator::generateParentEvent(const StochasticTrack &track, const Scale &scale, int rootNote, int &lastDegree, Random &rng) {
    StochasticParentEvent event;
    event.clear();

    // Pitch
    int absoluteDegree = generateDegree(track, scale, lastDegree, rng);
    int activeNotes = scale.notesPerOctave();
    event.d0.degree = absoluteDegree % activeNotes;
    event.d0.octave = absoluteDegree / activeNotes;
    
    // Rhythm
    event.d0.rate = track.rate();
    event.d0.length = 128; 
    
    // Flags
    event.d1.rest = (int(rng.nextRange(100)) < track.rest());
    event.d1.legato = (int(rng.nextRange(100)) < track.legatoProb());
    event.d1.slide = (int(rng.nextRange(100)) < track.slide());
    event.d1.accent = (int(rng.nextRange(100)) < track.accentProb());
    event.d1.valid = true;

    // Children
    event.d1.childCount = 0;
    if (int(rng.nextRange(100)) < track.burst()) {
        // burstCount maps to 1..4 hits
        event.d1.childCount = 1 + (uint32_t(track.burstCount()) * 3) / 100; 
    }

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
        if (complexity < 33 && lastDegree != -1) {
            if (int(rng.nextRange(100)) < (100 - complexity)) {
                return lastDegree;
            }
        }

        if (track.linearity() > 0 && lastDegree != -1) {
            totalTickets = 0;
            for (int i = 0; i < allowedCount; ++i) {
                int dist = std::abs(allowedDegrees[i] - lastDegree);
                int penalty = (dist * track.linearity()) / 10;
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
    if (track.jump() == 0) return 0;
    if (int(rng.nextRange(100)) < track.jump()) {
        return rng.nextRange(3) - 1; 
    }
    return currentJump;
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
