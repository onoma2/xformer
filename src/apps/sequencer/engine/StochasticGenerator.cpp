#include "StochasticGenerator.h"
#include "StochasticTrackEngine.h"   // for getDurationMultiplier + kMaxChildren

#include <cmath>
#include <algorithm>

// Burst count LUT — knob picks weighted-random over {2,3,4,5}.
// 1 child is meaningless; max 5 matches kMaxChildren in StochasticTrackEngine.h.
// burstCount is a MAX intent: actual count clips to whatever the picked
// spacing slot can hold inside the parent duration (Option C).
static const int kBurstCountLut[] = { 2, 3, 4, 5 };
static const int kBurstCountLutSize = 4;

// Burst spacing LUT — knob picks weighted-random denominator from {2,3,4,5,6}.
// Spacing = parentDurationTicks / denominator.
//   /2 = half-parent (echo / slap-back; few children fit)
//   /3 = sextuplet feel
//   /4 = 1/32 fill (3 children fit on 1/8 parent)
//   /5 = quintuplet
//   /6 = 1/48 sextuplet flurry (5 children fit on 1/8 parent)
// At Cluster F gate (parent >= 1/8 = 96 ticks), /6 = 16 ticks ≥ minChildGate+1.
static const int kBurstSpacingLut[] = { 2, 3, 4, 5, 6 };
static const int kBurstSpacingLutSize = 5;

// Minimum parent duration (ticks) for burst to fire at all. 96 ticks = 1/8 at
// PPQN=48. Below this the children either fall under minChildGate or pack into
// inaudible mush. Cluster F gate.
static const uint32_t kMinBurstParentTicks = 96;

// Triangular-kernel weighted pick over a LUT.
// knob = 0   → ~always lut[0]
// knob = 100 → ~always lut[last]
// midpoints  → smooth distribution over neighbors (50-tick tent half-width).
static int pickFromLutTriangular(const int *lut, int lutSize, int knob, Random &rng) {
    int weights[8];  // enough for any reasonable LUT
    int total = 0;
    for (int i = 0; i < lutSize; ++i) {
        int center = (i * 100) / (lutSize - 1);
        int dist = knob > center ? knob - center : center - knob;
        int w = 50 - dist;
        if (w < 0) w = 0;
        weights[i] = w;
        total += w;
    }
    if (total <= 0) {
        return lut[rng.nextRange(uint32_t(lutSize))];
    }
    int roll = int(rng.nextRange(uint32_t(total)));
    int sum = 0;
    for (int i = 0; i < lutSize; ++i) {
        sum += weights[i];
        if (roll < sum) return lut[i];
    }
    return lut[lutSize - 1];
}

static int pickBurstCountFromLut(int knob, Random &rng) {
    return pickFromLutTriangular(kBurstCountLut, kBurstCountLutSize, knob, rng);
}

static int pickBurstSpacingFromLut(int knob, Random &rng) {
    return pickFromLutTriangular(kBurstSpacingLut, kBurstSpacingLutSize, knob, rng);
}

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

// Marbles-style permutation: swap rhythm content between two random positions.
void StochasticGenerator::permuteRhythmOne(StochasticSequence &sequence, Random &rng) {
    int size = sequence.size();
    if (size < 2) return;
    int i = rng.nextRange(size);
    int j = rng.nextRange(size - 1);
    if (j >= i) ++j;  // ensure j != i
    auto &ev = sequence.events();
    StochasticSourceEvent tmp;
    tmp.mergeRhythmFrom(ev[i]);
    ev[i].mergeRhythmFrom(ev[j]);
    ev[j].mergeRhythmFrom(tmp);
}

int StochasticGenerator::applyVariation(int baseIdx, int variation, Random &rng) {
    if (variation == 0) return baseIdx;
    int absVar = variation < 0 ? -variation : variation;
    if (int(rng.nextRange(100)) >= absVar) return baseIdx;

    int sign = variation > 0 ? +1 : -1;
    int maxJump = 1 + (absVar * 3) / 100;     // 1..4 slots
    if (maxJump < 1) maxJump = 1;
    if (maxJump > 4) maxJump = 4;

    int weights[4];
    int total = 0;
    for (int d = 1; d <= maxJump; ++d) {
        int w = maxJump - d + 1;              // triangular: adjacent heaviest
        weights[d - 1] = w;
        total += w;
    }
    int roll = int(rng.nextRange(uint32_t(total)));
    int sum = 0;
    int jump = 1;
    for (int d = 1; d <= maxJump; ++d) {
        sum += weights[d - 1];
        if (roll < sum) { jump = d; break; }
    }

    int target = baseIdx + sign * jump;
    if (target < 0) target = 0;
    if (target > 7) target = 7;
    return target;
}

// Marbles-style permutation: swap melody content between two random positions.
void StochasticGenerator::permuteMelodyOne(StochasticSequence &sequence, Random &rng) {
    int size = sequence.size();
    if (size < 2) return;
    int i = rng.nextRange(size);
    int j = rng.nextRange(size - 1);
    if (j >= i) ++j;
    auto &ev = sequence.events();
    StochasticSourceEvent tmp;
    tmp.mergeMelodyFrom(ev[i]);
    ev[i].mergeMelodyFrom(ev[j]);
    ev[j].mergeMelodyFrom(tmp);
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

    // Cluster F eval-time safety net: regardless of what childCount the event
    // stores (possibly generated at a longer duration), suppress burst if the
    // current effective parent is too short.
    if (durationTicks < kMinBurstParentTicks) {
        count = 0;
    }

    // Spacing comes from the LUT slot baked into event.burstRate(). Option C:
    // the picked denominator determines child spacing INDEPENDENTLY of count;
    // count auto-clips via the break-out logic below when children no longer fit.
    int spacingSlot = int(event.burstRate());
    if (spacingSlot < 0) spacingSlot = 0;
    if (spacingSlot >= kBurstSpacingLutSize) spacingSlot = kBurstSpacingLutSize - 1;
    int spacingDenom = kBurstSpacingLut[spacingSlot];
    float spacing = float(durationTicks) / float(spacingDenom);

    // Minimum audible child gate: 6 ticks ≈ 30ms at 120 BPM (PPQN=192)
    const uint32_t minChildGate = 6;
    spacing = std::max(float(minChildGate + 1), spacing);

    for (int i = 0; i < StochasticTrackEngine::kMaxChildren; ++i) {
        children[i].valid = false;
        if (i < count) {
            uint32_t offset = uint32_t((i + 1) * spacing);
            uint32_t gate = std::max(minChildGate, uint32_t(spacing * 0.5f));

            if (offset + gate >= durationTicks) {
                if (offset + minChildGate < durationTicks) {
                    gate = durationTicks - offset - 1;
                    if (gate < minChildGate) break;
                } else {
                    break;
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

    // V5 Duration: use weighted duration tickets if any are active, else
    // single-slot pick via the noteDuration LUT macro. Variation is applied at
    // generation time so each event gets its own randomized durationIndex
    // baked into the stored pattern — Loop-mode playback then repeats the same
    // varied content until RENEW, and the loop tape display sees the variation.
    int durationIndex;
    if (sequence.durationTicketsActive()) {
        durationIndex = selectDurationTicket(sequence, rng);
    } else {
        int baseDur = std::max(0, std::min(7, int(sequence.noteDuration())));
        durationIndex = applyVariation(baseDur, sequence.variation(), rng);
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

    event.setChildCount(0);
    event.setBurstRate(0);
    // Cluster F duration gate: skip burst entirely if the parent is shorter than
    // 1/8 (96 ticks). Below that, children pack into mush even with the 6-tick
    // gate floor. Combined with the LUT picks below, this guarantees children
    // are always audible.
    uint32_t parentTicks = StochasticTrackEngine::getDurationMultiplier(durationIndex);
    if (parentTicks >= kMinBurstParentTicks && int(rng.nextRange(100)) < sequence.burst()) {
        // burstCount macro biases the LUT pick for max child count (Option C —
        // actual count is auto-shrunk by the eval-time fit loop if the picked
        // spacing can't hold all children).
        event.setChildCount(pickBurstCountFromLut(sequence.burstCount(), rng));
        // burstRate macro biases the spacing LUT pick. The picked denominator
        // is baked into the event (stored in the burstRate field, reinterpreted
        // as the LUT slot index 0..kBurstSpacingLutSize-1) so Loop-mode playback
        // is consistent across iterations.
        int spacingSlot = -1;
        const int picked = pickBurstSpacingFromLut(sequence.burstRate(), rng);
        for (int i = 0; i < kBurstSpacingLutSize; ++i) {
            if (kBurstSpacingLut[i] == picked) { spacingSlot = i; break; }
        }
        if (spacingSlot < 0) spacingSlot = 0;
        event.setBurstRate(uint32_t(spacingSlot));
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
            int tickets = sequence.effectiveDegreeTicket(i, activeNotes);
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

    if (sequence.pitchTicketsActive()) {
        if (totalTickets > 0) {
            int roll = rng.nextRange(totalTickets);
            int sum = 0;
            for (int i = 0; i < allowedCount; ++i) {
                sum += penalizedWeights[i];
                if (roll < sum) { degree = allowedDegrees[i]; break; }
            }
        } else {
            degree = allowedDegrees[rng.nextRange(allowedCount)];
        }
        lastDegree = degree;
        return degree;
    }

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
