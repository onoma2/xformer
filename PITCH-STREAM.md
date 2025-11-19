# Pitch CV Output Calculation

This document explains how pitch CV output is calculated for note tracks in the PEW|FORMER sequencer, with detailed coverage of harmony roles (Off, Master, FollowerRoot, Follower3rd, Follower5th, Follower7th).

## Overview

The pitch CV output calculation follows a multi-stage pipeline in `NoteTrackEngine::evalStepNote()` (NoteTrackEngine.cpp:72-143):

```
Step Note → Base Calculation → Harmony Modulation → Accumulator → Variation → CV Voltage
```

Each stage modifies the note value, with the final result converted to CV voltage using the 1V/octave standard.

## Complete Calculation Flow

### Stage 1: Base Note Calculation (Line 73)

```cpp
int note = step.note() + evalTransposition(scale, octave, transpose);
```

**Inputs:**
- `step.note()`: Programmed note value (-64 to +63, where 0 = middle C)
- `evalTransposition()`: Combines scale transposition, octave shift, and transpose parameter

**Output:** Base note value before modulation

### Stage 2: Harmony Modulation (Lines 76-127)

**Condition:** Only applied if `harmonyRole != HarmonyOff && harmonyRole != HarmonyMaster`

This is the most complex stage, completely replacing the follower track's note with a harmonized chord tone.

#### Step 2.1: Get Master Track's Note (Lines 79-86)

```cpp
int masterTrackIndex = sequence.masterTrackIndex();  // 0-7 for tracks T1-T8
const auto &masterTrack = model.project().track(masterTrackIndex);
const auto &masterSequence = masterTrack.noteTrack().sequence(0);

// Synchronized step playback - follower reads master's note at same step index
int masterStepIndex = clamp(currentStepIndex, masterSequence.firstStep(), masterSequence.lastStep());
const auto &masterStep = masterSequence.step(masterStepIndex);
int masterNote = masterStep.note();
```

**Key behavior:**
- Follower reads master's step at **same step index** (synchronized playback)
- If follower has more steps than master, indices wrap to master's range
- Master's sequence 0 (first pattern) is always used

#### Step 2.2: Convert to MIDI and Calculate Scale Degree (Lines 89-94)

```cpp
int midiNote = masterNote + 60;  // Convert to MIDI note number (middle C = 60)
int scaleDegree = ((masterNote % 7) + 7) % 7;  // 0-6 for 7-note scales
```

**Why MIDI?**
- HarmonyEngine uses MIDI note numbers (0-127) internally
- Scale degree determines diatonic chord quality (I, ii, iii, IV, V, vi, vii°)

#### Step 2.3: Harmonize Using HarmonyEngine (Lines 97-104)

```cpp
HarmonyEngine::Mode harmonyMode = static_cast<HarmonyEngine::Mode>(sequence.harmonyScale());

HarmonyEngine harmonyEngine;
harmonyEngine.setMode(harmonyMode);                                              // Ionian, Dorian, etc.
harmonyEngine.setInversion(sequence.harmonyInversion());                         // 0-3 (Root, 1st, 2nd, 3rd)
harmonyEngine.setVoicing(static_cast<HarmonyEngine::Voicing>(sequence.harmonyVoicing()));  // Close, Drop2, Drop3, Spread

auto chord = harmonyEngine.harmonize(midiNote, scaleDegree);
```

**HarmonyEngine.harmonize() returns:**
```cpp
struct ChordNotes {
    int16_t root;    // Root note of chord
    int16_t third;   // 3rd (major or minor based on diatonic quality)
    int16_t fifth;   // 5th (perfect or diminished)
    int16_t seventh; // 7th (major, minor, or diminished based on quality)
};
```

**All harmony parameters affect output:**
- **Mode (Scale)**: Determines diatonic chord qualities (Major7, Minor7, Dominant7, HalfDim7)
- **Inversion**: Reorders chord tones (Root pos, 1st inv, 2nd inv, 3rd inv)
- **Voicing**: Spreads notes across octaves (Close, Drop2, Drop3, Spread)

#### Step 2.4: Extract Chord Tone Based on Follower Role (Lines 107-123)

```cpp
int harmonizedMidi = midiNote;
switch (harmonyRole) {
case NoteSequence::HarmonyFollowerRoot:
    harmonizedMidi = chord.root;
    break;
case NoteSequence::HarmonyFollower3rd:
    harmonizedMidi = chord.third;
    break;
case NoteSequence::HarmonyFollower5th:
    harmonizedMidi = chord.fifth;
    break;
case NoteSequence::HarmonyFollower7th:
    harmonizedMidi = chord.seventh;
    break;
default:
    break;
}
```

**Critical insight:** Each follower track plays ONE chord tone, enabling 4-voice harmonization across tracks.

#### Step 2.5: Convert Back and Apply Transposition (Line 126)

```cpp
note = (harmonizedMidi - 60) + evalTransposition(scale, octave, transpose);
```

**Important:**
- Follower's programmed step notes are completely ignored
- Only master's notes determine harmony
- Transposition applies AFTER harmonization (allows independent pitch shifting)

### Stage 3: Accumulator Modulation (Lines 130-133)

```cpp
if (sequence.accumulator().enabled()) {
    int accumulatorValue = sequence.accumulator().currentValue();
    note += accumulatorValue;  // Simple offset addition
}
```

**Modulation order matters:** Harmony is applied first, then accumulator offset.

### Stage 4: Note Variation (Lines 135-142)

```cpp
int probability = clamp(step.noteVariationProbability() + probabilityBias, -1, NoteSequence::NoteVariationProbability::Max);
if (useVariation && int(rng.nextRange(NoteSequence::NoteVariationProbability::Range)) <= probability) {
    int offset = step.noteVariationRange() == 0 ? 0 : rng.nextRange(std::abs(step.noteVariationRange()) + 1);
    if (step.noteVariationRange() < 0) {
        offset = -offset;
    }
    note = NoteSequence::Note::clamp(note + offset);
}
```

Random variation based on step's `noteVariationRange` and `noteVariationProbability`.

### Stage 5: CV Voltage Conversion (Line 143)

```cpp
return scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
```

#### For NoteScale (most scales) - Scale.h:88-92

```cpp
float noteToVolts(int note) const {
    int octave = roundDownDivide(note, _noteCount);
    int index = note - octave * _noteCount;
    return octave + _notes[index] * (1.f / 1536.f);
}
```

**1V/octave standard:**
- Each octave = 1.0 volt
- Fractional voltages based on scale intervals
- `_notes[index]` contains interval data (0-1536 representing 0-1 octave)
- Division by 1536 converts to fractional voltage

**Example:** Middle C (note=0) → octave=0, index=0 → 0V

#### For VoltScale - Scale.h:151-153

```cpp
float noteToVolts(int note) const {
    return note * _interval;
}
```

Simple linear voltage scaling (used for non-musical CV modes).

## Output by Harmony Role

| Role | What Influences Output | Follower's Step Notes Used? |
|------|------------------------|------------------------------|
| **Off** | Step note + Transpose + Octave + Accumulator + Variation | ✅ Yes |
| **Master** | Step note + Transpose + Octave + Accumulator + Variation | ✅ Yes |
| **FollowerRoot** | **Master's note** → harmonized → Root tone + Transpose + Octave + Accumulator + Variation | ❌ No (replaced by harmony) |
| **Follower3rd** | **Master's note** → harmonized → 3rd tone + Transpose + Octave + Accumulator + Variation | ❌ No (replaced by harmony) |
| **Follower5th** | **Master's note** → harmonized → 5th tone + Transpose + Octave + Accumulator + Variation | ❌ No (replaced by harmony) |
| **Follower7th** | **Master's note** → harmonized → 7th tone + Transpose + Octave + Accumulator + Variation | ❌ No (replaced by harmony) |

## Key Insights

### 1. Follower Tracks Completely Ignore Their Own Step Notes

When a track is set to any follower role (Root/3rd/5th/7th), its programmed step notes are discarded. The only note data used is from the master track.

**Practical implication:** You can program random notes in follower tracks - they won't affect output. Only master matters.

### 2. Synchronized Step Playback

Follower reads master's note at the **same step index**:
```cpp
int masterStepIndex = clamp(currentStepIndex, masterSequence.firstStep(), masterSequence.lastStep());
```

**Examples:**
- Follower step 1 → Master step 1
- Follower step 16 → Master step 16 (if master has 16 steps)
- Follower step 16 → Master step 8 (if master only has 8 steps - clamped)

**This means:** Followers with more steps than master will repeat master's last note for out-of-range indices.

### 3. Modulation Order Is Critical

```
Master's Note → Harmony Engine → Harmonized Chord Tone → Transpose → Accumulator → Variation → CV Voltage
```

**Why this matters:**
- Accumulator offsets apply to harmonized result (not master's original note)
- Transposition applies independently to each follower (allows octave spread)
- Variation randomizes the final harmonized pitch

### 4. All Harmony Parameters Affect Output

Even though the code flow seems simple, three parameters deeply influence the harmonized result:

- **Scale (Mode):** Changes chord qualities
  - Ionian: Major7 on I, IV, Dominant7 on V
  - Dorian: Minor7 on i, Major7 on IV
  - Phrygian: Minor7 on i, HalfDim7 on ii

- **Inversion:** Reorders which chord tone is lowest
  - Root: R-3-5-7
  - 1st: 3-5-7-R (3rd is bass)
  - 2nd: 5-7-R-3 (5th is bass)
  - 3rd: 7-R-3-5 (7th is bass)

- **Voicing:** Spreads notes across octaves
  - Close: All within one octave
  - Drop2: Second-highest note dropped an octave
  - Drop3: Third-highest note dropped an octave
  - Spread: Orchestral wide voicing

### 5. Master Track Defines Harmony But Doesn't Modify Its Own Output

A master track outputs exactly what it's programmed to play - no harmonization applied to itself. It only serves as a reference for follower tracks.

**Typical setup:**
- Track 1 (Master): Melody line
- Track 2 (Root): Bass note
- Track 3 (3rd): Middle voice
- Track 4 (5th): Upper voice
- Track 5 (7th): Tension/color voice

### 6. Independent Transposition Per Follower

```cpp
note = (harmonizedMidi - 60) + evalTransposition(scale, octave, transpose);
```

Each follower can have independent octave and transpose settings, allowing:
- Bass follower (Root) at octave -2
- Lead follower (5th) at octave +1
- Middle followers (3rd/7th) at octave 0

## Example: C Major Chord (Ionian Mode)

**Master Track (T1):**
- Step 1: Note = 0 (Middle C)

**Harmony Settings (all followers):**
- Scale: Ionian (0)
- Inversion: Root (0)
- Voicing: Close (0)

**HarmonyEngine calculates:**
- Scale degree 0 → Ionian I chord → Major7 quality
- Chord intervals: R=0, 3rd=4, 5th=7, 7th=11 semitones
- MIDI notes: R=60, 3rd=64, 5th=67, 7th=71

**Follower Track Outputs:**
- T2 (Root): 60 → C
- T3 (3rd): 64 → E
- T4 (5th): 67 → G
- T5 (7th): 71 → B

**Result:** 4-voice C Major 7 chord (C-E-G-B)

## Example: Same Master with Drop2 Voicing

**Harmony Settings:**
- Scale: Ionian (0)
- Inversion: Root (0)
- Voicing: Drop2 (1)

**HarmonyEngine adjusts voicing:**
- Original close: C(60) - E(64) - G(67) - B(71)
- Drop2: Second-highest note (G) dropped one octave
- Result: C(60) - G(55) - E(64) - B(71)

**Follower Track Outputs:**
- T2 (Root): 60 → C (unchanged)
- T3 (3rd): 64 → E (unchanged)
- T4 (5th): 55 → G (dropped octave!)
- T5 (7th): 71 → B (unchanged)

**Result:** Jazz-style Drop2 voicing with wider spread

## Implementation Files

- **NoteTrackEngine.cpp:72-143** - Main evaluation function `evalStepNote()`
- **HarmonyEngine.h/cpp** - Chord calculation and voicing logic
- **Scale.h:88-92** - Voltage conversion `noteToVolts()`
- **NoteSequence.h** - Harmony parameter storage (harmonyRole, masterTrackIndex, harmonyScale, harmonyInversion, harmonyVoicing)
- **HarmonyListModel.h** - UI parameter editing

## Related Features

- **Accumulator:** Can offset harmonized pitches in real-time
- **Note Variation:** Adds randomization to harmonized results
- **Pulse Count:** Works independently of harmony (controls step duration)
- **Gate Mode:** Works independently of harmony (controls gate firing pattern)
- **Retrigger:** Works independently of harmony (creates ratchets)

## Technical Notes

### Thread Safety

The `evalStepNote()` function is called from the Engine task and accesses model data that may be modified by the UI task. Thread safety is ensured by:
- Engine suspension during long operations (file I/O)
- Short-term locking for parameter access
- Const correctness for read-only operations

### Performance

Harmony calculation overhead per step:
- Master track lookup: O(1)
- Scale degree calculation: O(1)
- HarmonyEngine.harmonize(): O(1) lookup table operations
- Chord tone extraction: O(1) switch statement
- Total: Negligible CPU impact (~10-20 CPU cycles)

### Voltage Precision

The 1V/octave standard is implemented with high precision:
- Internal calculations use floating-point
- Scale intervals stored as fixed-point (1/1536 resolution)
- Final DAC output is 16-bit (65536 steps across voltage range)
- Effective resolution: ~0.15 cents per step

## Future Enhancements

Potential improvements not yet implemented:
- Manual chord quality selection (override diatonic chords)
- Additional scales (Harmonic Minor, Melodic Minor, etc.)
- Cross-track accumulator influence on harmony
- CV input tracking for dynamic scale degree
- Per-step harmony override (layer in NoteSequence)
