#include "StochasticGenerator.h"
#include "StochasticTrackEngine.h"   // for getDurationFraction + kMaxBurst
#include "Config.h"                  // CONFIG_PPQN / CONFIG_SEQUENCE_PPQN

#include <cmath>
#include <algorithm>

// Burst count LUT — knob picks weighted-random over {2,3,4,5}.
// 1 child is meaningless; LUT spans {2..8} matching kMaxBurst in StochasticTrackEngine.h.
// burstCount is a MAX intent: actual count clips to whatever the picked
// spacing LUT entry can hold inside the parent duration (Option C).
static const int kBurstCountLut[] = { 2, 3, 4, 5, 6, 7, 8 };
static const int kBurstCountLutSize = 7;

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
    StochasticGenerator::PitchGenState state{};
    int size = sequence.size();

    for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
        if (i < size) {
            auto melody = generateMelodyEvent(sequence, track, scale, rootNote, state, rng);
            sequence.steps()[i].mergeMelodyFrom(melody);
        } else {
            sequence.steps()[i].setMelodyValid(false);
        }
    }

    sequence.setMelodyValid(true);
    sequence.setMelodySeed(seed);
}

// Destructively re-roll the rhythm content of one step inside the active
// window. Mutate is a pure probability gate at the cycle-end hook; magnitude
// is not exposed. Full mask-rank recompute follows because the new event's
// durationIndex changes the sort position.
void StochasticGenerator::mutateRhythmOne(StochasticSequence &sequence, const StochasticTrack &track, Random &rng) {
    int size = sequence.size();
    if (size <= 0) return;
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int i = first + rng.nextRange(last - first + 1);

    auto rhythm = generateRhythmEvent(sequence, track, rng);
    sequence.steps()[i].mergeRhythmFrom(rhythm);
    generateMaskRanks(sequence, size, sequence.rhythmSeed() ^ 0xdeadbeef);
}

// Destructively re-roll the melody content of one step inside the active
// window. Routes through generateDegree so the new pitch uses the 5-step
// pitch law (tickets, region, contour) with a fresh state — no chain context.
void StochasticGenerator::mutateMelodyOne(StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, Random &rng) {
    (void)rootNote;
    int size = sequence.size();
    if (size <= 0) return;
    int first = clamp(int(sequence.first()), 0, size - 1);
    int last  = clamp(int(sequence.last()),  first, size - 1);
    int i = first + rng.nextRange(last - first + 1);

    PitchGenState state{};
    int absDegree = generateDegree(sequence, track, scale, state, rng);
    int activeNotes = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);

    StochasticStepContent melody;
    melody.clear();
    melody.setDegree(absDegree % activeNotes);
    melody.setOctave(absDegree / activeNotes);
    melody.setMelodyValid(true);
    sequence.steps()[i].mergeMelodyFrom(melody);
}

// Three-zone Variation-blended duration picker. See PHASE15-PITCH-MATH-REVIEW
// "Duration / Variation / Rest law" section. Three overlapping triangular
// zone amounts (home / ticket / explore) sum per LUT slot.
//
//   home    = bell around NoteDuration               (Variation=0 dominates)
//   ticket  = max(1, durationTicket[i])              (Variation=50 dominates)
//   explore = floor + bell around mirrored NoteDur   (Variation=100 dominates)
//
// Ticket states (mirror of pitch tickets): -1 excludes the slot entirely,
// 0 is default-flat (weight 1 on the ticket axis), 1..100 is emphasis.
static int pickDuration(const StochasticSequence &sequence, Random &rng) {
    constexpr int entries = 8;
    const int center = clamp(int(sequence.noteDuration()), 0, entries - 1);
    const int V = clamp(int(sequence.variation()), 0, 100);
    const int mirror = (entries - 1) - center;

    const int homeAmount    = std::max(0, 100 - 2 * V);
    const int absVMid       = V > 50 ? V - 50 : 50 - V;
    const int ticketAmount  = std::max(0, 100 - 2 * absVMid);
    const int exploreAmount = std::max(0, 2 * V - 100);

    // Home bell is a narrow spike (halfwidth 1) so Variation=0 locks
    // strictly to NoteDuration. Mirror bell is wider (halfwidth 2) so
    // exploration spreads around the opposite end.
    constexpr int homeHalf = 1;
    constexpr int mirrorHalf = 2;
    constexpr int exploreFloor = 5;

    int weights[entries];
    int total = 0;
    for (int i = 0; i < entries; ++i) {
        const int t = sequence.durationTicket(i);
        if (t < 0) { weights[i] = 0; continue; }

        const int dh = i > center ? i - center : center - i;
        const int homeBell = (homeHalf > dh ? homeHalf - dh : 0) * 10;
        const int ticketW = t > 0 ? t : 1;
        const int dm = i > mirror ? i - mirror : mirror - i;
        const int mirrorBell = (mirrorHalf > dm ? mirrorHalf - dm : 0) * 10;
        const int exploreW = exploreFloor + mirrorBell;

        weights[i] = homeBell * homeAmount
                   + ticketW  * ticketAmount
                   + exploreW * exploreAmount;
        total += weights[i];
    }

    // All-excluded fallback: pick the configured NoteDuration slot even if
    // it's been excluded. Rare; UI should flag.
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
            
            if (burstHoldIsRoll(sequence.burstHold())) {
                StochasticGenerator::PitchGenState burstState{};
                bursts[i].note = generateDegree(sequence, track, scale, burstState, rng);
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

StochasticStepContent StochasticGenerator::generateMelodyEvent(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, int rootNote, PitchGenState &state, Random &rng) {
    StochasticStepContent event;
    event.clear();

    int absoluteDegree = generateDegree(sequence, track, scale, state, rng);
    int activeNotes = scale.notesPerOctave();
    event.setDegree(absoluteDegree % activeNotes);
    event.setOctave(absoluteDegree / activeNotes);
    event.setMelodyValid(true);

    return event;
}

// Run-length recency penalty per PITCH-LAW-FINAL.md Step 1. Returns a
// per-mille multiplier (1000 = full weight). Cuts the favored class's
// weight after each repeat so heavy tickets can't lock the line.
static uint32_t recencyPenaltyMilli(int runLength) {
    if (runLength <= 0) return 1000;
    if (runLength == 1) return 550;
    if (runLength == 2) return 200;
    return 50;
}

// Beta-distribution approximation used by Step 4. Returns 0..1000.
// spread < 500 → collapse toward bias (low spread = concentrated near Bias).
// spread > 500 → Bernoulli-like pull toward edges, Bias picks which edge wins.
// spread = 500 → cloud around Bias (mostly uniform with small bias pull).
static uint32_t betaShapeMilli(uint32_t uMilli, int biasKnob, int spreadKnob) {
    uint32_t bias = uint32_t(clamp(biasKnob, 0, 100)) * 10;       // 0..1000
    uint32_t spread = uint32_t(clamp(spreadKnob, 0, 100)) * 10;   // 0..1000
    if (uMilli > 1000) uMilli = 1000;

    if (spread < 500) {
        // Pull strength = (500 - spread) / 500. At spread=0, fully collapse to bias.
        uint32_t pull = ((500 - spread) * 1000) / 500;            // 0..1000
        return (bias * pull + uMilli * (1000 - pull)) / 1000;
    } else {
        // Bernoulli edge pull. Strength = (spread - 500) / 500.
        uint32_t bernPull = ((spread - 500) * 1000) / 500;        // 0..1000
        uint32_t edgePick = (uMilli < bias) ? 0 : 1000;
        return (uMilli * (1000 - bernPull) + edgePick * bernPull) / 1000;
    }
}

// Five-step sequential pitch generation. See
// .tasks/stochastic-track-port/PITCH-LAW-FINAL.md for the design contract.
//
// Step 1 — Class roll. Weighted random over scale-degree classes using
//          tickets × recency penalty. Blind to motion.
// Step 2 — Class repeat check. Complexity decides whether to keep a repeat
//          or re-roll on Step 1. Bounded retries (3).
// Step 3 — Pitch candidate set. Range builds the absolute-pitch set for
//          the chosen class (single octave, multi-octave field, or single +
//          per-slot octave-jump chance).
// Step 4 — Region roll. Beta-distribution over the sorted set, shaped by
//          Bias + Spread. Octave-blind.
// Step 5 — Direction nudge. Contour applies up/down preference vs the
//          last absolute pitch.
int StochasticGenerator::generateDegree(const StochasticSequence &sequence, const StochasticTrack &track, const Scale &scale, PitchGenState &state, Random &rng) {
    (void)track;
    const int activeNotes = clamp(scale.notesPerOctave(), 1, CONFIG_USER_SCALE_SIZE);
    const int N = activeNotes;
    const int range = clamp(int(sequence.range()), 0, 100);
    const int complexity = clamp(int(sequence.complexity()), 0, 100);
    const int contour = clamp(int(sequence.contour()), -100, 100);
    const int biasKnob = clamp(int(sequence.marblesBias()), 0, 100);
    const int spreadKnob = clamp(int(sequence.marblesSpread()), 0, 100);

    // Build the allowed class set (degrees-in-octave with ticket >= 0).
    int allowedClasses[CONFIG_USER_SCALE_SIZE];
    int classTicket[CONFIG_USER_SCALE_SIZE];
    int allowedCount = 0;
    for (int c = 0; c < N; ++c) {
        const int t = sequence.effectiveDegreeTicket(c, N);
        if (t >= 0) {
            allowedClasses[allowedCount] = c;
            classTicket[allowedCount] = t;
            ++allowedCount;
        }
    }
    if (allowedCount == 0) return 0;

    // ---- Step 1 + 2: class roll with recency, repeat-rejection by Complexity.
    // Ticket states: -1 excludes (filtered above); 0 is default-flat (weight 1);
    // 1..100 is weighted emphasis. The three states have stable meanings
    // independent of context — heavy emphasis stays emphasis, never silently
    // excludes the others.
    int chosenClass = -1;
    for (int retry = 0; retry < 4; ++retry) {
        uint32_t weights[CONFIG_USER_SCALE_SIZE];
        uint32_t total = 0;
        for (int i = 0; i < allowedCount; ++i) {
            uint32_t base = uint32_t(classTicket[i] > 0 ? classTicket[i] : 1);
            uint32_t penalty = (allowedClasses[i] == state.lastClass)
                ? recencyPenaltyMilli(state.classRunLength) : 1000u;
            weights[i] = base * penalty;
            total += weights[i];
        }
        int picked = 0;
        if (total > 0) {
            uint32_t roll = rng.nextRange(total);
            uint32_t sum = 0;
            for (int i = 0; i < allowedCount; ++i) {
                sum += weights[i];
                if (roll < sum) { picked = i; break; }
            }
        }
        chosenClass = allowedClasses[picked];
        // Step 2: if class == lastClass, Complexity-driven reject probability.
        if (chosenClass == state.lastClass && state.lastClass >= 0) {
            const uint32_t reject = uint32_t(complexity);
            if (rng.nextRange(100u) < reject) continue;   // re-roll
        }
        break;
    }
    if (chosenClass < 0) chosenClass = allowedClasses[0];

    // ---- Step 3: pitch candidate set construction.
    // Range > 50 anchors a fixed multi-octave playable field starting at
    // octave 0 — Bias picks within it, Contour can swap both directions.
    // Range = 50 is a single-octave field (Steps 4 + 5 collapse to no-ops).
    // Range < 50 keeps the line on its current octave but rolls a per-slot
    // displacement chance, adding ±1 octave neighbors when the jump fires.
    const int currentOctave = (state.lastDegree >= 0) ? (state.lastDegree / N) : 0;

    int candidates[8];
    int candCount = 0;
    if (range > 50) {
        int width = 1 + ((range - 50) * 3) / 50;   // 1..4
        if (width < 1) width = 1;
        if (width > 4) width = 4;
        for (int k = 0; k < width; ++k) {
            candidates[candCount++] = chosenClass + k * N;
        }
    } else if (range < 50) {
        candidates[candCount++] = chosenClass + currentOctave * N;
        const uint32_t jumpPct = uint32_t((50 - range) * 2);
        if (rng.nextRange(100u) < jumpPct) {
            if (currentOctave - 1 >= 0) candidates[candCount++] = chosenClass + (currentOctave - 1) * N;
            candidates[candCount++] = chosenClass + (currentOctave + 1) * N;
        }
    } else {
        candidates[candCount++] = chosenClass + currentOctave * N;
    }

    // Sort ascending so Steps 4 and 5 can index a flat list.
    for (int i = 1; i < candCount; ++i) {
        int v = candidates[i];
        int j = i;
        while (j > 0 && candidates[j-1] > v) { candidates[j] = candidates[j-1]; --j; }
        candidates[j] = v;
    }

    // ---- Step 4: region roll via beta-distribution over the set.
    int chosenPitch = candidates[0];
    if (candCount > 1) {
        const uint32_t u = rng.nextRange(1001u);
        const uint32_t shaped = betaShapeMilli(u, biasKnob, spreadKnob);
        int idx = int((shaped * uint32_t(candCount - 1) + 500) / 1000);
        if (idx < 0) idx = 0;
        if (idx >= candCount) idx = candCount - 1;
        chosenPitch = candidates[idx];
    }

    // ---- Step 5: direction nudge — Contour swaps to a directionally-correct
    // pitch if the picked one moves against the requested direction.
    if (contour != 0 && state.lastDegree >= 0 && candCount > 1) {
        const int wantSign = contour > 0 ? 1 : -1;
        const int gotSign = (chosenPitch > state.lastDegree) ? 1
                          : (chosenPitch < state.lastDegree ? -1 : 0);
        if (gotSign != wantSign) {
            const uint32_t swapPct = uint32_t(contour > 0 ? contour : -contour);
            if (rng.nextRange(100u) < swapPct) {
                int best = -1;
                int bestDist = 0;
                for (int i = 0; i < candCount; ++i) {
                    int d = candidates[i] - state.lastDegree;
                    int s = d > 0 ? 1 : (d < 0 ? -1 : 0);
                    if (s == wantSign) {
                        int ad = d < 0 ? -d : d;
                        if (best < 0 || ad < bestDist) { best = i; bestDist = ad; }
                    }
                }
                if (best >= 0) chosenPitch = candidates[best];
            }
        }
    }

    // Update state: track lastDegree, lastClass, run length.
    const int chosenClassActual = chosenPitch % N;
    if (chosenClassActual == state.lastClass) {
        state.classRunLength = state.classRunLength + 1;
    } else {
        state.lastClass = chosenClassActual;
        state.classRunLength = 0;
    }
    state.lastDegree = chosenPitch;
    return chosenPitch;
}

