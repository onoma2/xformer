#include "IndexedSequence.h"
#include "ProjectVersion.h"

#include "core/utils/Random.h"
#include <cmath>

static Random rng;

// Helper: adjust durations so their sum is a multiple of quantum
static void adjustDurationsToQuantum(uint16_t *durations, int count, uint16_t minDur, uint16_t maxDur, int quantum) {
    if (count <= 0) {
        return;
    }
    int sum = 0;
    int slackDown = 0;
    int slackUp = 0;
    for (int i = 0; i < count; ++i) {
        sum += int(durations[i]);
        slackDown += int(durations[i]) - int(minDur);
        slackUp += int(maxDur) - int(durations[i]);
    }
    int remainder = sum % quantum;
    if (remainder == 0) {
        return;
    }
    int down = remainder;
    int up = quantum - remainder;
    bool canDown = slackDown >= down;
    bool canUp = slackUp >= up;
    int delta = 0;
    if (canDown && canUp) {
        delta = (down <= up) ? -down : up;
    } else if (canDown) {
        delta = -down;
    } else if (canUp) {
        delta = up;
    }

    int remaining = std::abs(delta);
    if (remaining == 0) {
        return;
    }
    if (delta > 0) {
        while (remaining > 0) {
            bool changed = false;
            for (int i = 0; i < count && remaining > 0; ++i) {
                if (durations[i] < maxDur) {
                    durations[i]++;
                    remaining--;
                    changed = true;
                }
            }
            if (!changed) {
                break;
            }
        }
    } else {
        while (remaining > 0) {
            bool changed = false;
            for (int i = 0; i < count && remaining > 0; ++i) {
                if (durations[i] > minDur) {
                    durations[i]--;
                    remaining--;
                    changed = true;
                }
            }
            if (!changed) {
                break;
            }
        }
    }
}

// ============================================================================
// Rhythm Macros
// ============================================================================

void IndexedSequence::populateWithMacroRhythm(int firstStep, int lastStep, const int *groups, int groupCount) {
    if (groupCount <= 0) {
        return;
    }
    int totalSteps = 0;
    for (int i = 0; i < groupCount; ++i) {
        totalSteps += groups[i];
    }
    if (totalSteps <= 0) {
        return;
    }

    // If operating on full sequence, resize to match pattern
    if (firstStep == 0 && lastStep + 1 >= _activeLength) {
        setActiveLength(totalSteps);
        lastStep = totalSteps - 1;
    } else if (lastStep + 1 > _activeLength) {
        setActiveLength(lastStep + 1);
    }

    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    const int totalTicks = 768;
    const int baseGroupTicks = totalTicks / groupCount;
    const int groupRemainder = totalTicks % groupCount;

    uint16_t pattern[MaxSteps];
    int patternCount = 0;
    for (int g = 0; g < groupCount; ++g) {
        int groupSteps = groups[g];
        if (groupSteps <= 0) {
            continue;
        }
        int groupTicks = baseGroupTicks + (g < groupRemainder ? 1 : 0);
        int baseStepTicks = groupTicks / groupSteps;
        int stepRemainder = groupTicks % groupSteps;

        for (int i = 0; i < groupSteps; ++i) {
            int dur = baseStepTicks + (i < stepRemainder ? 1 : 0);
            pattern[patternCount++] = uint16_t(dur);
        }
    }

    if (patternCount == 0) {
        return;
    }

    for (int i = 0; i < stepCount; ++i) {
        int idx = i % patternCount;
        _steps[firstStep + i].setDuration(pattern[idx]);
    }
}

// ============================================================================
// Waveform Macros
// ============================================================================

void IndexedSequence::populateWithMacroTriangle(int firstStep, int lastStep, int cycles) {
    if (cycles < 1) {
        cycles = 1;
    }
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    const uint16_t minDur = 8;
    const uint16_t maxDur = 192;
    const int quantum = 96;

    uint16_t durations[MaxSteps];
    for (int i = 0; i < stepCount; ++i) {
        float t = (stepCount > 1) ? float(i) / float(stepCount - 1) : 0.f;
        float phase = t * float(cycles);
        float frac = phase - std::floor(phase);
        float tri = 1.f - std::abs(2.f * frac - 1.f);
        float normalized = 1.f - tri;
        uint16_t dur = uint16_t(std::round(minDur + normalized * (maxDur - minDur)));
        durations[i] = clamp<uint16_t>(dur, minDur, maxDur);
    }
    adjustDurationsToQuantum(durations, stepCount, minDur, maxDur, quantum);
    for (int i = 0; i < stepCount; ++i) {
        _steps[firstStep + i].setDuration(durations[i]);
    }
}

void IndexedSequence::populateWithMacroSawtooth(int firstStep, int lastStep, int cycles) {
    if (cycles < 1) {
        cycles = 1;
    }
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    const uint16_t minDur = 8;
    const uint16_t maxDur = 192;
    const int quantum = 96;

    uint16_t durations[MaxSteps];
    for (int i = 0; i < stepCount; ++i) {
        float t = (stepCount > 1) ? float(i) / float(stepCount - 1) : 0.f;
        float phase = t * float(cycles);
        float frac = phase - std::floor(phase);
        float normalized = frac;
        uint16_t dur = uint16_t(std::round(minDur + normalized * (maxDur - minDur)));
        durations[i] = clamp<uint16_t>(dur, minDur, maxDur);
    }
    adjustDurationsToQuantum(durations, stepCount, minDur, maxDur, quantum);
    for (int i = 0; i < stepCount; ++i) {
        _steps[firstStep + i].setDuration(durations[i]);
    }
}

// ============================================================================
// Melodic Macros
// ============================================================================

void IndexedSequence::populateWithMacroScale(int firstStep, int lastStep, bool ascending) {
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    static const int8_t scaleIntervals[] = {0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21, 23, 24};
    static const int scaleLength = sizeof(scaleIntervals) / sizeof(scaleIntervals[0]);

    for (int i = 0; i < stepCount; ++i) {
        int idx = ascending ? i : (stepCount - 1 - i);
        int8_t note = scaleIntervals[idx % scaleLength];
        if (idx >= scaleLength) {
            note += 12 * (idx / scaleLength);
        }
        _steps[firstStep + i].setNoteIndex(note);
    }
}

void IndexedSequence::populateWithMacroArpeggio(int firstStep, int lastStep, int patternIndex) {
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    static const int8_t patterns[][8] = {
        {0, 4, 7, 12, 16, 19, 24, 28},
        {24, 19, 16, 12, 7, 4, 0, -5},
        {0, 4, 7, 12, 7, 4, 0, -5},
        {0, 4, 7, 0, 4, 7, 12, 7},
    };
    patternIndex = clamp(patternIndex, 0, 3);

    const int8_t *pattern = patterns[patternIndex];
    for (int i = 0; i < stepCount; ++i) {
        _steps[firstStep + i].setNoteIndex(pattern[i % 8]);
    }
}

void IndexedSequence::populateWithMacroChord(int firstStep, int lastStep, int progressionIndex) {
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    static const int8_t progressions[][4] = {
        {0, 5, 7, 0},
        {0, 7, 9, 5},
        {2, 7, 0, 2},
    };
    static const int8_t triad[] = {0, 4, 7};
    progressionIndex = clamp(progressionIndex, 0, 2);

    const int8_t *roots = progressions[progressionIndex];

    for (int i = 0; i < stepCount; ++i) {
        int chordIdx = (i / 3) % 4;
        int noteIdx = i % 3;
        int8_t note = roots[chordIdx] + triad[noteIdx];
        _steps[firstStep + i].setNoteIndex(note);
    }
}

void IndexedSequence::populateWithMacroModal(int firstStep, int lastStep, int modeIndex) {
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    static const int8_t modes[][7] = {
        {0, 2, 3, 5, 7, 9, 10},   // Dorian
        {0, 1, 3, 5, 7, 8, 10},   // Phrygian
        {0, 2, 4, 6, 7, 9, 11},   // Lydian
        {0, 2, 4, 5, 7, 9, 10},   // Mixolydian
    };
    modeIndex = clamp(modeIndex, 0, 3);

    const int8_t *intervals = modes[modeIndex];

    for (int i = 0; i < stepCount; ++i) {
        int octave = i / 7;
        int degree = i % 7;
        int8_t note = intervals[degree] + (octave * 12);
        _steps[firstStep + i].setNoteIndex(note);
    }
}

void IndexedSequence::populateWithMacroRandomMelody(int firstStep, int lastStep) {
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    static const int8_t pentatonic[] = {0, 2, 4, 7, 9, 12, 14, 16, 19, 21};
    static const int pentaLength = sizeof(pentatonic) / sizeof(pentatonic[0]);

    for (int i = 0; i < stepCount; ++i) {
        int idx = rng.nextRange(pentaLength);
        int8_t note = pentatonic[idx];
        int octaveShift = (rng.nextRange(3) - 1) * 12;
        note += octaveShift;
        _steps[firstStep + i].setNoteIndex(note);
    }
}

// ============================================================================
// Duration Transform Macros
// ============================================================================

void IndexedSequence::transformDurationLog(int firstStep, int lastStep) {
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    uint16_t minDur = 48;
    uint16_t maxDur = 768;
    for (int i = 0; i < stepCount; ++i) {
        float t = float(i) / float(stepCount - 1);
        float curved = std::log(1.0f + t * 9.0f) / std::log(10.0f);
        uint16_t dur = uint16_t(minDur + curved * (maxDur - minDur));
        _steps[firstStep + i].setDuration(dur);
    }
}

void IndexedSequence::transformDurationExp(int firstStep, int lastStep) {
    int stepCount = lastStep - firstStep + 1;
    if (stepCount <= 0) {
        return;
    }

    uint16_t minDur = 48;
    uint16_t maxDur = 768;
    for (int i = 0; i < stepCount; ++i) {
        float t = float(i) / float(stepCount - 1);
        float curved = (std::exp(t * 2.0f) - 1.0f) / (std::exp(2.0f) - 1.0f);
        uint16_t dur = uint16_t(minDur + curved * (maxDur - minDur));
        _steps[firstStep + i].setDuration(dur);
    }
}

void IndexedSequence::transformQuantizeMeasure(int firstStep, int lastStep) {
    uint32_t totalDuration = 0;
    for (int i = firstStep; i <= lastStep; ++i) {
        totalDuration += _steps[i].duration();
    }
    if (totalDuration == 0) {
        return;
    }

    const uint32_t barTicks = 768;
    uint32_t targetDuration = ((totalDuration + barTicks / 2) / barTicks) * barTicks;
    if (targetDuration == 0) {
        targetDuration = barTicks;
    }

    float scalingFactor = float(targetDuration) / float(totalDuration);

    uint32_t scaledTotal = 0;
    for (int i = firstStep; i <= lastStep; ++i) {
        uint16_t originalDur = _steps[i].duration();
        uint16_t scaledDur = uint16_t(std::round(originalDur * scalingFactor));
        scaledDur = clamp<uint16_t>(scaledDur, 1, MaxDuration);
        _steps[i].setDuration(scaledDur);
        scaledTotal += scaledDur;
    }

    if (scaledTotal != targetDuration) {
        int diff = int(targetDuration) - int(scaledTotal);
        uint16_t lastDur = _steps[lastStep].duration();
        uint16_t adjustedDur = clamp<uint16_t>(int(lastDur) + diff, 1, MaxDuration);
        _steps[lastStep].setDuration(adjustedDur);
    }
}

void IndexedSequence::transformReverse(int firstStep, int lastStep) {
    int stepCount = lastStep - firstStep + 1;
    for (int i = 0; i < stepCount / 2; ++i) {
        _steps[firstStep + i].swap(_steps[lastStep - i]);
    }
}

void IndexedSequence::transformMirror(int firstStep, int lastStep) {
    int stepCount = lastStep - firstStep + 1;
    int midpoint = firstStep + stepCount / 2;
    for (int i = midpoint; i <= lastStep; ++i) {
        int mirror = firstStep + (midpoint - i);
        if (mirror >= firstStep && mirror < midpoint) {
            _steps[i].setNoteIndex(_steps[mirror].noteIndex());
            _steps[i].setDuration(_steps[mirror].duration());
            _steps[i].setGateLength(_steps[mirror].gateLength());
            _steps[i].setSlide(_steps[mirror].slide());
            _steps[i].setGroupMask(_steps[mirror].groupMask());
        }
    }
}
