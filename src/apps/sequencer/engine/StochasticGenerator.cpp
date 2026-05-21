#include "StochasticGenerator.h"
#include "StochasticTrackEngine.h"   // for getDurationFraction + kMaxChildren
#include "Config.h"                  // CONFIG_PPQN / CONFIG_SEQUENCE_PPQN

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

// Universal scale-degree weight from position alone. Works for any scale size
// (5, 7, 12, 24, 43, microtonal). Anchors at simple integer fractions of N:
// root (0), half (N/2), thirds (N/3, 2N/3), quarters (N/4, 3N/4). Triangular
// kernel around each anchor falls off across N/6 slots.
static int universalDegreeBoost(int degInOct, int N) {
    if (N <= 1) return 0;
    int halfWidth = N / 6;
    if (halfWidth < 1) halfWidth = 1;
    auto kernel = [&](int target, int weight) {
        int dist = std::abs(degInOct - target);
        // Wrap-around (octave equivalence at N)
        int wrap = N - dist;
        if (wrap < dist) dist = wrap;
        int w = (halfWidth - dist) * weight / halfWidth;
        return w > 0 ? w : 0;
    };
    int boost = 0;
    boost += kernel(0,             30);                  // root anchor
    if (N >= 2) boost += kernel(N / 2,         20);      // half
    if (N >= 3) boost += kernel(N / 3,         10);      // third
    if (N >= 3) boost += kernel((2 * N) / 3,   10);
    if (N >= 4) boost += kernel(N / 4,          5);      // quarter
    if (N >= 4) boost += kernel((3 * N) / 4,    5);
    return boost;
}

void StochasticGenerator::mutateRhythmOne(StochasticSequence &sequence, const StochasticTrack &track, Random &rng, int mutateMagnitude) {
    int size = sequence.size();
    if (size <= 0) return;
    // Phase 12 fix: pick target inside the active playback window [first..last]
    // (rotate doesn't shift which events are touched, just which patternIndex
    // maps to which slot). Mutations outside the window are silent until the
    // window moves; that made the user think mutate was broken.
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int windowSize = last - first + 1;
    int i = first + rng.nextRange(windowSize);

    // Phase 12: Mutate changes this event's durationIndex; tilt-driven ranks
    // depend on duration, so re-rank the whole buffer after the mutate.

    // Run the regular rhythm generator first — gets fresh rolls for density,
    // rest, legato, slide, accent, burst, plus an initial variation-biased
    // duration pick.
    auto rhythm = generateRhythmEvent(sequence, track, rng);

    // Override the duration with mutate-anchored bias: triangular kernel around
    // the current noteDuration slot. bias_strength = 100 - mutateMagnitude:
    //   low magnitude → strong anchor (stays near base)
    //   high magnitude → uniform across the 8 LUT slots
    // Phase 11: variation is symmetric (sign dropped). Spread term widens the
    // kernel symmetrically; direction comes from noteDuration (Bias) alone.
    int baseDur = std::max(0, std::min(7, int(sequence.noteDuration())));
    int biasStrength = 100 - std::max(0, std::min(100, mutateMagnitude));
    int varMag = clamp(int(sequence.variation()), 0, 100);   // abs via getter

    int weights[8]; int total = 0;
    for (int s = 0; s < 8; ++s) {
        int dist = std::abs(s - baseDur);
        int anchorKernel = std::max(0, 7 - dist);
        int spreadBoost = varMag * std::max(0, 4 - dist);    // symmetric spread bonus
        int w = 100 + biasStrength * anchorKernel + spreadBoost;
        weights[s] = w;
        total += w;
    }
    int durIdx = baseDur;
    if (total > 0) {
        int roll = int(rng.nextRange(uint32_t(total)));
        int sum = 0;
        for (int s = 0; s < 8; ++s) {
            sum += weights[s];
            if (roll < sum) { durIdx = s; break; }
        }
    }
    rhythm.setDurationIndex(durIdx);

    sequence.events()[i].mergeRhythmFrom(rhythm);
    // Duration-aware ranks need refresh after this event's durationIndex changed.
    generateMaskRanks(sequence, size, sequence.tilt(), sequence.rhythmSeed() ^ 0xdeadbeef);
}

void StochasticGenerator::mutateMelodyOne(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng, int mutateMagnitude) {
    int size = sequence.size();
    if (size <= 0) return;
    // Phase 12 fix: pick target inside the active playback window [first..last].
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int i = first + rng.nextRange(last - first + 1);

    // Build a weighted candidate list combining: degree tickets (user-curated),
    // universal degree LUT (geometric anchors), and bias_strength inverse to
    // mutate magnitude. Low magnitude = strong anchor pull (tonal); high = uniform.
    int activeNotes = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
    int range = sequence.range();
    int biasStrength = 100 - std::max(0, std::min(100, mutateMagnitude));

    int allowedDegrees[CONFIG_USER_SCALE_SIZE * 4];
    int weights[CONFIG_USER_SCALE_SIZE * 4];
    int allowedCount = 0;
    int totalWeight = 0;

    for (int oct = 0; oct < range; ++oct) {
        for (int idx = 0; idx < activeNotes; ++idx) {
            int tickets = sequence.effectiveDegreeTicket(idx, activeNotes);
            if (tickets < 0) continue;                       // excluded
            int deg = oct * activeNotes + idx;
            // Phase 12: min/max degree clamps dropped — pool gated by range + tickets only.

            int boost = universalDegreeBoost(idx, activeNotes);
            int ticketWeight = (tickets > 0) ? tickets : 1;
            int w = ticketWeight * (100 + (biasStrength * boost) / 100);
            allowedDegrees[allowedCount] = deg;
            weights[allowedCount] = w;
            totalWeight += w;
            ++allowedCount;
        }
    }
    if (allowedCount == 0) return;

    int absDegree = allowedDegrees[0];
    if (totalWeight > 0) {
        int roll = int(rng.nextRange(uint32_t(totalWeight)));
        int sum = 0;
        for (int k = 0; k < allowedCount; ++k) {
            sum += weights[k];
            if (roll < sum) { absDegree = allowedDegrees[k]; break; }
        }
    }

    StochasticSourceEvent melody;
    melody.clear();
    melody.setDegree(absDegree % activeNotes);
    melody.setOctave(absDegree / activeNotes);
    melody.setMelodyValid(true);
    sequence.events()[i].mergeMelodyFrom(melody);
}

// Marbles-style permutation: swap rhythm content between two random positions
// in the active playback window [first..last].
void StochasticGenerator::permuteRhythmOne(StochasticSequence &sequence, Random &rng) {
    int size = sequence.size();
    if (size < 2) return;
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int windowSize = last - first + 1;
    if (windowSize < 2) return;
    int i = first + rng.nextRange(windowSize);
    int j = first + rng.nextRange(windowSize - 1);
    if (j >= i) ++j;  // ensure j != i
    auto &ev = sequence.events();
    StochasticSourceEvent tmp;
    tmp.mergeRhythmFrom(ev[i]);
    ev[i].mergeRhythmFrom(ev[j]);
    ev[j].mergeRhythmFrom(tmp);
}

// Phase 11 unified duration picker. Always combines duration tickets (base weight
// LUT, defaults flat) with noteDuration (kernel center / Bias) and variation
// (kernel width / Spread, symmetric — sign ignored). Returns LUT slot 0..7.
static int pickDuration(const StochasticSequence &sequence, Random &rng) {
    const int slots = 8;
    int center = clamp(int(sequence.noteDuration()), 0, slots - 1);
    int spread = clamp(int(sequence.variation()), 0, 100);  // abs via variation() getter
    int width = 1 + (spread * 4) / 100;                      // 1..5 slots wide

    // Phase 12 musicality fix: the (width-dist) triangle is scaled ×10 so the
    // center keeps a strong lead even when leakage starts ramping in. Old
    // formula had `kernel + (spread/10)` where 1-slot kernel and a leakage of
    // 1 gave a 2:1 center-to-edge ratio → ~22% center which felt random at
    // VAR=10. Scaling the triangle 10× makes the same case 11:1 ≈ 61% center.
    // Also removes the integer-truncation cliff at the leakage threshold.
    int weights[slots];
    int total = 0;
    for (int i = 0; i < slots; ++i) {
        int base = sequence.durationTicket(i);
        if (base <= 0) base = 10;                            // flat default
        int dist = i > center ? i - center : center - i;
        int tri = (width > dist ? width - dist : 0) * 10;
        int kernel = tri + spread / 10;
        weights[i] = base * kernel;
        total += weights[i];
    }
    if (total <= 0) return center;
    int roll = int(rng.nextRange(uint32_t(total)));
    int sum = 0;
    for (int i = 0; i < slots; ++i) {
        sum += weights[i];
        if (roll < sum) return i;
    }
    return center;
}

// Marbles-style permutation: swap melody content between two random positions
// in the active playback window [first..last].
void StochasticGenerator::permuteMelodyOne(StochasticSequence &sequence, Random &rng) {
    int size = sequence.size();
    if (size < 2) return;
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int windowSize = last - first + 1;
    if (windowSize < 2) return;
    int i = first + rng.nextRange(windowSize);
    int j = first + rng.nextRange(windowSize - 1);
    if (j >= i) ++j;
    auto &ev = sequence.events();
    StochasticSourceEvent tmp;
    tmp.mergeMelodyFrom(ev[i]);
    ev[i].mergeMelodyFrom(ev[j]);
    ev[j].mergeMelodyFrom(tmp);
}

// Phase 12: duration-aware rank assignment over the ACTIVE PLAYBACK WINDOW
// [first..last] only. Off-window events are never read by playback, so ranking
// them wastes "top priority" slots on inaudible content and breaks the user
// mental model that Mask thins the audible loop by N%. Tilt is bipolar —
// positive favors long-note survival under Mask cuts; negative favors short
// notes. Tilt=0 → pure noise (random rank).
//
// `size` arg is kept for API stability but only used for buffer bounds; the
// window is derived from sequence.first()/last(). The mask trigger filter must
// compare against windowSize, not size, to match this rank domain.
void StochasticGenerator::generateMaskRanks(StochasticSequence &sequence, int size, int tilt, uint32_t seed) {
    Random rng(seed);
    if (size <= 0) return;
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int windowSize = last - first + 1;
    if (windowSize <= 0) return;

    struct WeightedIndex {
        uint8_t index;
        float weight;
    };
    std::array<WeightedIndex, CONFIG_STEP_COUNT> weightedIndices;

    for (int k = 0; k < windowSize; ++k) {
        int idx = first + k;
        weightedIndices[k].index = uint8_t(idx);
        int durSlot = clamp(int(sequence.events()[idx].durationIndex()), 0, 7);
        float longShortAxis = (durSlot - 3.5f) / 3.5f;          // -1..+1
        float bias = (tilt / 100.f) * longShortAxis;
        weightedIndices[k].weight = bias + (rng.nextRange(1000) / 1000.f) * 0.2f;
    }

    std::sort(weightedIndices.begin(), weightedIndices.begin() + windowSize, [](const WeightedIndex &a, const WeightedIndex &b) {
        return a.weight < b.weight;
    });

    for (int k = 0; k < windowSize; ++k) {
        sequence.events()[weightedIndices[k].index].setDensityRank(k);
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
    // Phase 11: single path — duration tickets always multiply into the
    // noteDuration (Bias) + variation (Spread) kernel.
    int durationIndex = pickDuration(sequence, rng);

    event.setDurationIndex(durationIndex);
    event.setDensityRank(0); // Live events have no mask rank

    // Rest probability: single Bernoulli gate. `sequence.density()` is a reserved
    // model slot (engine-unused — kept for serialization stability and future
    // repurpose, likely as Proteus-style rank-cutoff density).
    bool restGate = (rng.nextRange(100) < sequence.rest());
    event.setRest(restGate);
    event.setLegato(int(rng.nextRange(100)) < sequence.legatoProb());
    event.setSlide(int(rng.nextRange(100)) < sequence.slide());
    event.setAccent(int(rng.nextRange(100)) < sequence.accentProb());
    event.setRhythmValid(true);

    event.setChildCount(0);
    event.setBurstRate(0);
    // Cluster F duration gate: skip burst entirely if the parent is shorter than
    // 1/8 (96 ticks). Phase 12 fix: parent ticks must use the ACTUAL sequence
    // divisor, not the legacy 1/16 reference, otherwise burst is silently
    // suppressed at slower divisors where the parent is plenty long enough.
    auto burstFrac = StochasticTrackEngine::getDurationFraction(durationIndex);
    uint32_t parentTicks = (uint64_t(sequence.divisor())
                            * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN)
                            * burstFrac.num) / burstFrac.den;
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

// Phase 11 unified pitch picker. Single path. All shaping multiplies:
//   1. Build allowed degrees from range × scale tones, gated by ticket≥0
//      (Phase 12: min/max degree clamps dropped — redundant with range + pitch
//      tickets at 0; reserved as model slots only.)
//   2. Steps sieve — universalDegreeBoost rank cutoff. Knob 0..100, 100=open.
//      Top-K most-fundamental degrees survive. Mirrors Mask on rhythm side.
//   3. Per-survivor weight = ticket × complexityKernel × marblesKernel
//      - complexityKernel: triangular around lastDegree, width grows with complexity
//        plus a leakage term so high complexity allows leaps (linearity baked in)
//      - marblesKernel: triangular around bias position, width grows with spread,
//        always running with transparent defaults (bias=50, spread=100 → wide)
//   4. Weighted random pick over survivors
int StochasticGenerator::generateDegree(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int &lastDegree, Random &rng) {
    int activeNotes = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
    int range = sequence.range();
    int N = activeNotes;

    // 1. Build allowed degrees + ticket weights
    constexpr int kMaxSlots = CONFIG_USER_SCALE_SIZE * 4;
    int allowedDegrees[kMaxSlots];
    int ticketWeights[kMaxSlots];
    int allowedCount = 0;

    for (int oct = 0; oct < range; ++oct) {
        for (int i = 0; i < activeNotes; ++i) {
            int tickets = sequence.effectiveDegreeTicket(i, activeNotes);
            if (tickets >= 0) {
                allowedDegrees[allowedCount] = oct * activeNotes + i;
                ticketWeights[allowedCount] = tickets;
                allowedCount++;
            }
        }
    }
    if (allowedCount == 0) return 0;

    // 2. Steps sieve — keep top-K by universalDegreeBoost rank
    int steps = clamp(int(sequence.marblesSteps()), 0, 100);
    int K = (allowedCount * steps + 99) / 100;
    if (K < 1) K = 1;
    if (K > allowedCount) K = allowedCount;

    int sieveRank[kMaxSlots];
    for (int i = 0; i < allowedCount; ++i) {
        int degInOct = ((allowedDegrees[i] % N) + N) % N;
        sieveRank[i] = universalDegreeBoost(degInOct, N);
    }
    bool keep[kMaxSlots] = {};
    if (K >= allowedCount) {
        for (int i = 0; i < allowedCount; ++i) keep[i] = true;
    } else {
        for (int k = 0; k < K; ++k) {
            int bestIdx = -1; int bestRank = -1;
            for (int i = 0; i < allowedCount; ++i) {
                if (keep[i]) continue;
                if (sieveRank[i] > bestRank) { bestRank = sieveRank[i]; bestIdx = i; }
            }
            if (bestIdx >= 0) keep[bestIdx] = true;
            else break;
        }
    }

    // 3. Locate lastDegree's slot in allowedDegrees for complexity kernel
    int lastIdx = -1;
    if (lastDegree != -1) {
        for (int i = 0; i < allowedCount; ++i) {
            if (allowedDegrees[i] == lastDegree) { lastIdx = i; break; }
        }
    }

    int complexity = clamp(int(sequence.complexity()), 0, 100);
    int kernelWidth = 1 + (complexity * N) / 50;     // ~1 .. ~2N
    int kernelLeak = complexity / 10;                // 0..10
    int contour = clamp(int(sequence.contour()), -100, 100); // Drift: directional bias from lastIdx

    int marblesBias = clamp(int(sequence.marblesBias()), 0, 100);     // 0..100
    int marblesSpread = clamp(int(sequence.marblesSpread()), 0, 100); // 0..100

    int biasPos = (allowedCount > 1) ? (marblesBias * (allowedCount - 1) + 50) / 100 : 0;
    int marblesWidth = 1 + (marblesSpread * allowedCount) / 100;
    int marblesLeak = 1 + marblesSpread / 10;

    // Global ticket-active check: if any ticket > 0, zero tickets mean "excluded"
    // (filter). If ALL tickets are 0, use flat=10 fallback for all (no filter).
    bool anyTicket = false;
    for (int i = 0; i < allowedCount; ++i) {
        if (ticketWeights[i] > 0) { anyTicket = true; break; }
    }

    int weights[kMaxSlots];
    int totalWeight = 0;
    for (int i = 0; i < allowedCount; ++i) {
        if (!keep[i]) { weights[i] = 0; continue; }

        int base = anyTicket ? ticketWeights[i] : 10;
        if (base < 0) base = 0;

        int kernel;
        if (lastIdx < 0) {
            kernel = 100;
        } else {
            int d = i > lastIdx ? i - lastIdx : lastIdx - i;
            // Phase 12 musicality fix: the (kernelWidth-dist) triangle is
            // scaled ×10 so the center keeps a strong lead vs the kernelLeak
            // term. Same cliff-removal as the Variation fix in pickDuration.
            int tri = (kernelWidth > d ? kernelWidth - d : 0) * 10;
            // Drift in scaled units too — was contour×signedDist/20, now ×10
            // larger so it operates on the same scale as tri.
            int signedDist = i - lastIdx;
            int drift = (contour * signedDist) / 2;        // was /20
            int maxDrift = (kernelWidth > 2 ? kernelWidth : 2) * 10;
            if (drift > maxDrift) drift = maxDrift;
            else if (drift < -maxDrift) drift = -maxDrift;
            kernel = tri + kernelLeak + drift;
            if (kernel < 0) kernel = 0;
        }

        int dm = i > biasPos ? i - biasPos : biasPos - i;
        // Phase 12 musicality fix: ×10 scaling on the bell triangle so the
        // marblesLeak floor doesn't overwhelm the bell at low spread values.
        int mtri = (marblesWidth > dm ? marblesWidth - dm : 0) * 10;
        int marbles = mtri + marblesLeak;

        weights[i] = base * kernel * marbles;
        totalWeight += weights[i];
    }

    // 4. Weighted random pick
    int degree = allowedDegrees[0];
    if (totalWeight > 0) {
        int roll = int(rng.nextRange(uint32_t(totalWeight)));
        int sum = 0;
        for (int i = 0; i < allowedCount; ++i) {
            sum += weights[i];
            if (roll < sum) { degree = allowedDegrees[i]; break; }
        }
    } else {
        // All weights zero (shouldn't happen with leakage floor); pick any survivor
        for (int i = 0; i < allowedCount; ++i) {
            if (keep[i]) { degree = allowedDegrees[i]; break; }
        }
    }
    lastDegree = degree;
    return degree;
}

int StochasticGenerator::generateJumpOctave(const StochasticSequence &sequence, const StochasticTrack &track, int currentJump, Random &rng) {
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
