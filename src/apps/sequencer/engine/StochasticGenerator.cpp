#include "StochasticGenerator.h"
#include "StochasticTrackEngine.h"   // for getDurationFraction + kMaxBurst
#include "Config.h"                  // CONFIG_PPQN / CONFIG_SEQUENCE_PPQN

#include <cmath>
#include <algorithm>

// Burst count LUT — knob picks weighted-random over {2,3,4,5}.
// 1 child is meaningless; max 5 matches kMaxBurst in StochasticTrackEngine.h.
// burstCount is a MAX intent: actual count clips to whatever the picked
// spacing LUT entry can hold inside the parent duration (Option C).
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

// Triangular-kernel weighted pick over a LUT. knob 0 → ~always lut[0],
// knob 100 → ~always lut[last]; midpoints smooth over neighbors with a
// 50-unit tent half-width. Size deduced from the array reference so the
// weights buffer matches the LUT at compile time.
template <int LutSize>
static int pickFromLutTriangular(const int (&lut)[LutSize], int knob, Random &rng) {
    int weights[LutSize];
    int total = 0;
    for (int i = 0; i < LutSize; ++i) {
        int center = (LutSize > 1) ? (i * 100) / (LutSize - 1) : 50;
        int dist = knob > center ? knob - center : center - knob;
        int w = 50 - dist;
        if (w < 0) w = 0;
        weights[i] = w;
        total += w;
    }
    if (total <= 0) {
        return lut[rng.nextRange(uint32_t(LutSize))];
    }
    int roll = int(rng.nextRange(uint32_t(total)));
    int sum = 0;
    for (int i = 0; i < LutSize; ++i) {
        sum += weights[i];
        if (roll < sum) return lut[i];
    }
    return lut[LutSize - 1];
}

// Public wrappers for the cache walk. Cache builds shape decisions per-cell
// using these same triangular LUT pickers; mutate uses the same pickers
// with a different RNG instance — same semantics, different RNG sources
// (mutate uses Loop seed,
// cache uses keyed per-cell seed).
int StochasticGenerator::pickBurstCount(int knob, Random &rng) {
    return pickFromLutTriangular(kBurstCountLut, knob, rng);
}

// Returns LUT entry index 0..kBurstSpacingLutSize-1 (not the denominator).
// Caller looks up the denominator in its own copy of kBurstSpacingLut.
int StochasticGenerator::pickBurstSpacingSlot(int knob, Random &rng) {
    const int picked = pickFromLutTriangular(kBurstSpacingLut, knob, rng);
    for (int i = 0; i < kBurstSpacingLutSize; ++i) {
        if (kBurstSpacingLut[i] == picked) return i;
    }
    return 0;
}

void StochasticGenerator::generateRhythm(StochasticSequence &sequence, const StochasticTrack &track, uint32_t seed) {
    Random rng(seed);
    int size = sequence.size();
    
    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        if (i < size) {
            auto rhythm = generateRhythmEvent(sequence, track, rng);
            sequence.steps()[i].mergeRhythmFrom(rhythm);
        } else {
            // Do not clear the whole event, only rhythm part
            sequence.steps()[i].clearRhythm();
        }
    }

    generateMaskRanks(sequence, size, seed ^ 0xdeadbeef);
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
            sequence.steps()[i].mergeMelodyFrom(melody);
        } else {
            sequence.steps()[i].setMelodyValid(false);
        }
    }

    sequence.setMelodyValid(true);
    sequence.setMelodySeed(seed);
}

void StochasticGenerator::mutateRhythmOne(StochasticSequence &sequence, const StochasticTrack &track, Random &rng, int mutateMagnitude) {
    int size = sequence.size();
    if (size <= 0) return;
    // Pick target inside the active playback window. Mutations outside the
    // window would be silent until the window moved.
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int windowSize = last - first + 1;
    int i = first + rng.nextRange(windowSize);

    // Run the regular generator first for rest / burst / etc. picks, then
    // override the duration with a mutate-anchored triangular kernel: low
    // mutate magnitude → strong anchor pull (stays near the noteDuration
    // setting); high magnitude → uniform across the 8 LUT entrys.
    auto rhythm = generateRhythmEvent(sequence, track, rng);
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

    sequence.steps()[i].mergeRhythmFrom(rhythm);
    // Duration-aware ranks need refresh after this event's durationIndex changed.
    generateMaskRanks(sequence, size, sequence.rhythmSeed() ^ 0xdeadbeef);
}

void StochasticGenerator::mutateMelodyOne(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng, int mutateMagnitude) {
    int size = sequence.size();
    if (size <= 0) return;
    // Pick target inside the active playback window.
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int i = first + rng.nextRange(last - first + 1);

    // Pure ticket-weighted candidate pick. Steps-law centrality boost is
    // gone (mask-melody owns audibility at trigger time). mutateMagnitude
    // is currently a no-op on the pitch axis here; PHASE15 owns the
    // mutate-distance revamp.
    (void)mutateMagnitude;
    int activeNotes = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
    int range = sequence.range();

    int allowedDegrees[CONFIG_USER_SCALE_SIZE * 4];
    int weights[CONFIG_USER_SCALE_SIZE * 4];
    int allowedCount = 0;
    int totalWeight = 0;

    for (int oct = 0; oct < range; ++oct) {
        for (int idx = 0; idx < activeNotes; ++idx) {
            int tickets = sequence.effectiveDegreeTicket(idx, activeNotes);
            if (tickets < 0) continue;
            int deg = oct * activeNotes + idx;
            int w = (tickets > 0) ? tickets : 1;
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

    StochasticStepContent melody;
    melody.clear();
    melody.setDegree(absDegree % activeNotes);
    melody.setOctave(absDegree / activeNotes);
    melody.setMelodyValid(true);
    sequence.steps()[i].mergeMelodyFrom(melody);
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
    auto &ev = sequence.steps();
    StochasticStepContent tmp;
    tmp.mergeRhythmFrom(ev[i]);
    ev[i].mergeRhythmFrom(ev[j]);
    ev[j].mergeRhythmFrom(tmp);
}

// Duration picker. Combines duration tickets (flat-default LUT base weight),
// noteDuration (kernel center), and variation (kernel width, symmetric).
// Returns LUT entry 0..7.
//
// The (width-dist) triangle is scaled ×10 so the center retains a strong
// lead — at low variation a low-leakage center still feels musical instead
// of flattening to near-random.
static int pickDuration(const StochasticSequence &sequence, Random &rng) {
    const int entries = 8;
    int center = clamp(int(sequence.noteDuration()), 0, entries - 1);
    int spread = clamp(int(sequence.variation()), 0, 100);
    int width = 1 + (spread * 4) / 100;                      // 1..5 entries wide
    int weights[entries];
    int total = 0;
    for (int i = 0; i < entries; ++i) {
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
    for (int i = 0; i < entries; ++i) {
        sum += weights[i];
        if (roll < sum) return i;
    }
    return center;
}

// Public entry point for cache walk.
int StochasticGenerator::pickDurationSlot(const StochasticSequence &sequence, Random &rng) {
    return pickDuration(sequence, rng);
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
    auto &ev = sequence.steps();
    StochasticStepContent tmp;
    tmp.mergeMelodyFrom(ev[i]);
    ev[i].mergeMelodyFrom(ev[j]);
    ev[j].mergeMelodyFrom(tmp);
}

// Per-event rank assignment. Score = a normalized long-vs-short axis from
// each event's durationIndex, plus a small random jitter to break ties for
// equal-duration events. Sort sets event.densityRank to the cell's position
// in long-to-short order (rank 0 = longest, rank N-1 = shortest). Tilt does
// NOT participate here — it's applied at trigger time.
//
// Iterates the full pattern extent so each step's rank is stable across
// First/Size window edits. Called from RENEW / Patience / Mutate.
void StochasticGenerator::generateMaskRanks(StochasticSequence &sequence, int size, uint32_t seed) {
    Random rng(seed);
    if (size <= 0) return;
    if (size > CONFIG_STEP_COUNT) size = CONFIG_STEP_COUNT;

    struct WeightedIndex {
        uint8_t index;
        float weight;
    };
    std::array<WeightedIndex, CONFIG_STEP_COUNT> weightedIndices;

    for (int k = 0; k < size; ++k) {
        int durSlot = clamp(int(sequence.steps()[k].durationIndex()), 0, 7);
        // Long-vs-short axis in [-1, +1]: durSlot 0 = longest LUT entry (×8),
        // durSlot 7 = shortest (×1/2). Negate so longest cell -> negative, sorts first.
        float longShortAxis = (durSlot - 3.5f) / 3.5f;
        // Small jitter (range 0..0.2) breaks ties between equal-duration events
        // deterministically per seed without overwhelming the duration ordering.
        float jitter = (rng.nextRange(1000) / 1000.f) * 0.2f;
        weightedIndices[k].index = uint8_t(k);
        weightedIndices[k].weight = longShortAxis + jitter;
    }

    std::sort(weightedIndices.begin(), weightedIndices.begin() + size,
              [](const WeightedIndex &a, const WeightedIndex &b) {
        return a.weight < b.weight;
    });

    for (int k = 0; k < size; ++k) {
        sequence.steps()[weightedIndices[k].index].setDensityRank(k);
    }
}

void StochasticGenerator::evaluateBurst(EvaluatedBurstNote *bursts, const StochasticSequence &sequence, const StochasticStepContent &event, const StochasticTrack &track, const Scale &scale, int rootNote, int anchorNote, uint32_t durationTicks, Random &rng) {
    int count = event.childCount();

    // Cluster F eval-time safety net: regardless of what childCount the event
    // stores (possibly generated at a longer duration), suppress burst if the
    // current effective parent is too short.
    if (durationTicks < kMinBurstParentTicks) {
        count = 0;
    }

    // Spacing comes from the LUT entry baked into event.burstRate(). Option C:
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

    for (int i = 0; i < StochasticTrackEngine::kMaxBurst; ++i) {
        bursts[i].valid = false;
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

            bursts[i].tickOffset = offset;
            bursts[i].gateTicks = gate;
            
            if (sequence.burstHold() == StochasticBurstHold::Roll) {
                int lastDegree = -1;
                bursts[i].note = generateDegree(sequence, track, scale, lastDegree, rng);
            } else {
                bursts[i].note = anchorNote;
            }
            bursts[i].valid = true;
        }
    }
}

StochasticStepContent StochasticGenerator::generateRhythmEvent(const StochasticSequence &sequence, const StochasticTrack &track, Random &rng) {
    StochasticStepContent event;
    event.clear();

    // Duration pick: tickets × kernel(noteDuration center, variation width).
    int durationIndex = pickDuration(sequence, rng);

    event.setDurationIndex(durationIndex);
    event.setDensityRank(0); // Live events have no mask rank

    bool restGate = (rng.nextRange(100) < sequence.rest());
    event.setRest(restGate);
    // Legato + Slide are cache-owned. event.legato / event.slide are kept as
    // dead bits for binary / serialization stability.
    event.setLegato(false);
    event.setSlide(false);
    event.setRhythmValid(true);

    event.setChildCount(0);
    event.setBurstRate(0);
    // Burst storage: the cache decides per-cell eligibility (prev_dur / denom
    // playable). The generator just stores count + spacing so Repeat playback
    // — which replays _lastStepContent through evaluateBurst — has something to
    // evaluate. evaluateBurst keeps its own duration check.
    if (int(rng.nextRange(100)) < sequence.burst()) {
        event.setChildCount(pickFromLutTriangular(kBurstCountLut, sequence.burstCount(), rng));
        int spacingSlot = -1;
        const int picked = pickFromLutTriangular(kBurstSpacingLut, sequence.burstRate(), rng);
        for (int i = 0; i < kBurstSpacingLutSize; ++i) {
            if (kBurstSpacingLut[i] == picked) { spacingSlot = i; break; }
        }
        if (spacingSlot < 0) spacingSlot = 0;
        event.setBurstRate(uint32_t(spacingSlot));
    }

    return event;
}

StochasticStepContent StochasticGenerator::generateMelodyEvent(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, int &lastDegree, Random &rng) {
    StochasticStepContent event;
    event.clear();

    int absoluteDegree = generateDegree(sequence, track, scale, lastDegree, rng);
    int activeNotes = scale.notesPerOctave();
    event.setDegree(absoluteDegree % activeNotes);
    event.setOctave(absoluteDegree / activeNotes);
    event.setMelodyValid(true);

    return event;
}

// Unified pitch picker. All shaping multiplies:
//   1. Build allowed degrees from range × scale tones, gated by ticket≥0.
//   2. Per-slot weight = ticket × complexityKernel × marblesKernel.
//      - complexityKernel: triangular around lastDegree, width grows with
//        complexity plus a leakage term so high complexity allows leaps.
//      - marblesKernel: triangular around bias position, width grows with
//        spread; always running.
//   3. Weighted random pick.
//
// Pitch-centrality (stochasticPitchCentrality) is intentionally NOT a factor
// here — pitch-mask audibility happens at trigger time via maskMelody/tiltMelody.
int StochasticGenerator::generateDegree(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int &lastDegree, Random &rng) {
    int activeNotes = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
    int range = sequence.range();
    int N = activeNotes;

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

    int lastIdx = -1;
    if (lastDegree != -1) {
        for (int i = 0; i < allowedCount; ++i) {
            if (allowedDegrees[i] == lastDegree) { lastIdx = i; break; }
        }
    }

    int complexity = clamp(int(sequence.complexity()), 0, 100);
    int kernelWidth = 1 + (complexity * N) / 50;
    int kernelLeak = complexity / 10;
    int contour = clamp(int(sequence.contour()), -100, 100);

    int marblesBias = clamp(int(sequence.marblesBias()), 0, 100);
    int marblesSpread = clamp(int(sequence.marblesSpread()), 0, 100);

    int biasPos = (allowedCount > 1) ? (marblesBias * (allowedCount - 1) + 50) / 100 : 0;
    int marblesWidth = 1 + (marblesSpread * allowedCount) / 100;
    int marblesLeak = 1 + marblesSpread / 10;

    bool anyTicket = false;
    for (int i = 0; i < allowedCount; ++i) {
        if (ticketWeights[i] > 0) { anyTicket = true; break; }
    }

    int weights[kMaxSlots];
    int totalWeight = 0;
    for (int i = 0; i < allowedCount; ++i) {
        int base = anyTicket ? ticketWeights[i] : 10;
        if (base < 0) base = 0;

        int kernel;
        if (lastIdx < 0) {
            kernel = 100;
        } else {
            int d = i > lastIdx ? i - lastIdx : lastIdx - i;
            int tri = (kernelWidth > d ? kernelWidth - d : 0) * 10;
            int signedDist = i - lastIdx;
            int drift = (contour * signedDist) / 2;
            int maxDrift = (kernelWidth > 2 ? kernelWidth : 2) * 10;
            if (drift > maxDrift) drift = maxDrift;
            else if (drift < -maxDrift) drift = -maxDrift;
            kernel = tri + kernelLeak + drift;
            if (kernel < 0) kernel = 0;
        }

        int dm = i > biasPos ? i - biasPos : biasPos - i;
        int mtri = (marblesWidth > dm ? marblesWidth - dm : 0) * 10;
        int marbles = mtri + marblesLeak;

        weights[i] = base * kernel * marbles;
        totalWeight += weights[i];
    }

    int degree = allowedDegrees[0];
    if (totalWeight > 0) {
        int roll = int(rng.nextRange(uint32_t(totalWeight)));
        int sum = 0;
        for (int i = 0; i < allowedCount; ++i) {
            sum += weights[i];
            if (roll < sum) { degree = allowedDegrees[i]; break; }
        }
    }
    lastDegree = degree;
    return degree;
}

