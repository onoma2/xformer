#include "StochasticGenerator.h"

#include <cmath>
#include <algorithm>

void StochasticGenerator::generateRhythm(StochasticSequence &sequence, const StochasticTrack &track, uint32_t seed) {
    Random rng(seed);
    int size = sequence.size();
    
    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        if (i < size) {
            auto rhythm = generateRhythmEvent(sequence, track, rng);
            sequence.events()[i].mergeRhythmFrom(rhythm);
        } else {
            // Do not clear the whole event, only rhythm part
            sequence.events()[i].clearRhythm();
        }
    }

    generateMaskRanks(sequence, size, sequence.tilt(), seed ^ 0xdeadbeef);
    sequence.setRhythmValid(true);
    sequence.setRhythmSeed(seed);
}

void StochasticGenerator::generateMelody(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, uint32_t seed) {
    Random rng(seed);
    int lastDegree = -1;
    int size = sequence.size();
    
    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        if (i < size) {
            auto melody = generateMelodyEvent(sequence, track, scale, rootNote, lastDegree, rng);
            sequence.events()[i].mergeMelodyFrom(melody);
        } else {
            sequence.events()[i].setMelodyValid(false);
        }
    }

    sequence.setMelodyValid(true);
    sequence.setMelodySeed(seed);
}

void StochasticGenerator::mutateRhythmOne(StochasticSequence &sequence, const StochasticTrack &track, Random &rng) {
    int size = sequence.size();
    if (size <= 0) return;
    int i = rng.nextRange(size);
    
    uint8_t oldRank = sequence.events()[i].densityRank();
    auto rhythm = generateRhythmEvent(sequence, track, rng);
    sequence.events()[i].mergeRhythmFrom(rhythm);
    sequence.events()[i].setDensityRank(oldRank);
}

void StochasticGenerator::mutateMelodyOne(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng) {
    int size = sequence.size();
    if (size <= 0) return;
    int i = rng.nextRange(size);

    int lastDegree = (i > 0) ? int(sequence.events()[i-1].degree()) : -1;
    auto melody = generateMelodyEvent(sequence, track, scale, rootNote, lastDegree, rng);
    sequence.events()[i].mergeMelodyFrom(melody);
}

void StochasticGenerator::generateMaskRanks(StochasticSequence &sequence, int size, int tilt, uint32_t seed) {
    Random rng(seed);
    if (size <= 0) return;
    
    struct WeightedIndex {
        uint8_t index;
        float weight;
    };
    std::array<WeightedIndex, CONFIG_STEP_COUNT> weightedIndices;

    for (int i = 0; i < size; ++i) {
        weightedIndices[i].index = i;
        float progress = i / float(size - 1 == 0 ? 1 : size - 1);
        float bias = (tilt / 100.f) * (progress - 0.5f); 
        weightedIndices[i].weight = bias + (rng.nextRange(1000) / 1000.f) * 0.2f; 
    }

    std::sort(weightedIndices.begin(), weightedIndices.begin() + size, [](const WeightedIndex &a, const WeightedIndex &b) {
        return a.weight < b.weight;
    });

    for (int i = 0; i < size; ++i) {
        sequence.events()[weightedIndices[i].index].setDensityRank(i);
    }
}

void StochasticGenerator::evaluateChildren(EvaluatedChild *children, const StochasticSequence &sequence, const StochasticSourceEvent &event, const StochasticTrack &track, const Scale &scale, int rootNote, int parentNote, uint32_t durationTicks, Random &rng) {
    int count = event.childCount();
    uint32_t burstRate = event.burstRate();
    
    // Minimum audible gap (low pulse duration)
    const uint32_t minGap = 2;

    for (int i = 0; i < 4; ++i) {
        children[i].valid = false;
        if (i < count) {
            float spacing = (durationTicks / float(count + 1)) * (burstRate / 100.f);
            spacing = std::max(float(minGap + 1), spacing); 

            uint32_t offset = uint32_t((i + 1) * spacing);
            uint32_t gate = std::max(uint32_t(1), uint32_t(spacing * 0.5f));

            // Bounds check: must fit inside durationTicks
            if (offset + gate >= durationTicks) {
                // If it doesn't fit, try to reduce gate or skip
                if (offset + 1 < durationTicks) {
                    gate = durationTicks - offset - 1;
                } else {
                    break; // Doesn't fit at all
                }
            }

            children[i].tickOffset = offset;
            children[i].gateTicks = gate;
            
            if (sequence.burstPitch() == StochasticBurstPitch::Generate) {
                int lastDegree = -1;
                children[i].note = generateDegree(sequence, track, scale, lastDegree, rng);
            } else {
                children[i].note = parentNote;
            }
            children[i].valid = true;
        }
    }
}

StochasticSourceEvent StochasticGenerator::generateRhythmEvent(const StochasticSequence &sequence, const StochasticTrack &track, Random &rng) {
    StochasticSourceEvent event;
    event.clear();

    // V5 Duration: use duration tickets if enabled, else Rate+Variation
    int durationIndex;
    if (sequence.durationTicketsActive()) {
        durationIndex = selectDurationTicket(sequence, rng);
    } else {
        int r = std::max(1, std::min(400, int(sequence.rate())));
        durationIndex = ((r - 1) * 8) / 400;
    }

    event.setDurationIndex(durationIndex);
    event.setDensityRank(0); // Live events have no mask rank

    // Generator Density: sound/rest amount at generation time
    bool densityGate = (rng.nextRange(100) < sequence.density());
    // Direct Rest probability
    bool restGate = (rng.nextRange(100) < sequence.rest());
    event.setRest(!densityGate || restGate);
    event.setLegato(int(rng.nextRange(100)) < sequence.legatoProb());
    event.setSlide(int(rng.nextRange(100)) < sequence.slide());
    event.setAccent(int(rng.nextRange(100)) < sequence.accentProb());
    event.setRhythmValid(true);
    event.setBurstRate(sequence.burstRate());

    event.setChildCount(0);
    if (int(rng.nextRange(100)) < sequence.burst()) {
        event.setChildCount(1 + (uint32_t(sequence.burstCount()) * 3) / 100);
    }

    return event;
}

StochasticSourceEvent StochasticGenerator::generateMelodyEvent(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, int &lastDegree, Random &rng) {
    StochasticSourceEvent event;
    event.clear();

    int absoluteDegree = generateDegree(sequence, track, scale, lastDegree, rng);
    int activeNotes = scale.notesPerOctave();
    event.setDegree(absoluteDegree % activeNotes);
    event.setOctave(absoluteDegree / activeNotes);
    event.setMelodyValid(true);

    return event;
}

int StochasticGenerator::generateDegree(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int &lastDegree, Random &rng) {
    int activeNotes = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
    int range = sequence.range();
    
    int allowedDegrees[CONFIG_USER_SCALE_SIZE * 4];
    int penalizedWeights[CONFIG_USER_SCALE_SIZE * 4];
    int allowedCount = 0;
    int totalTickets = 0;

    for (int oct = 0; oct < range; ++oct) {
        for (int i = 0; i < activeNotes; ++i) {
            int tickets = sequence.degreeTicket(i);
            if (tickets >= 0) { // -1 is excluded
                int deg = oct * activeNotes + i;
                if (deg >= sequence.minDegree() && deg <= sequence.maxDegree()) {
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

    if (sequence.marblesMode() == MarblesMode::On) {
        float x = rng.nextRange(1000) / 1000.f;
        float shapedX = betaDistributionSample(x, sequence.marblesSpread() / 100.f);
        float bias = sequence.marblesBias() / 100.f;
        float targetIdxFloat = (shapedX - 0.5f) * (allowedCount) + (bias * allowedCount);
        int targetIdx = clamp(int(targetIdxFloat), 0, allowedCount - 1);
        degree = allowedDegrees[targetIdx];
    } else {
        int complexity = sequence.complexity();
        int contour = sequence.contour();
        int linearity = sequence.linearity();

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

        if (contour != 0 && allowedCount > 1) {
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

int StochasticGenerator::generateJumpOctave(const StochasticSequence &sequence, const StochasticTrack &track, int currentJump, Random &rng) {
    return 0;
}

int StochasticGenerator::selectDurationTicket(const StochasticSequence &sequence, Random &rng) {
    int totalWeight = 0;
    for (int i = 0; i < 8; ++i) {
        totalWeight += sequence.durationTicket(i);
    }
    if (totalWeight <= 0) {
        for (int i = 0; i < 8; ++i) totalWeight += 10;
        totalWeight = 80;
    }
    int roll = rng.nextRange(totalWeight);
    int sum = 0;
    for (int i = 0; i < 8; ++i) {
        int w = sequence.durationTicket(i);
        if (w <= 0 && totalWeight <= 0) w = 10;
        sum += w;
        if (roll < sum) return i;
    }
    return 3; // default 1/16
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
