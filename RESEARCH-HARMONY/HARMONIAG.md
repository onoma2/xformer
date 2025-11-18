# Instruo Harmonàig Integration Plan for PEW|FORMER

**Date**: 2025-11-18
**Feature**: Harmonàig-style Harmonic Quantization & Chord Generation
**Status**: 🔬 Research Complete - Planning Phase
**Priority**: OPTIONAL - Advanced Feature Enhancement

---

## Table of Contents
1. [Executive Summary](#executive-summary)
2. [Instruo Harmonàig Overview](#instruo-harmonàig-overview)
3. [Feature Comparison: Harmonàig vs Performer](#feature-comparison)
4. [Software Architecture](#software-architecture)
5. [Data Model Design](#data-model-design)
6. [Engine Integration](#engine-integration)
7. [UI/UX Design](#uiux-design)
8. [CV/MIDI Integration](#cvmidi-integration)
9. [Implementation Plan](#implementation-plan)
10. [Testing Strategy](#testing-strategy)
11. [Future Enhancements](#future-enhancements)

---

## Executive Summary

### What is Harmonàig?

The **Instruo Harmonàig** is a diatonic, polyphonic, bipolar voltage quantizer that generates four harmonized CV signals (Root, 3rd, 5th, 7th) from a single input CV. It provides sophisticated harmonic quantization within the Ionian modal system and Harmonic Minor scale modes.

**Key Capabilities**:
- 4-voice chord generation from single CV input
- 8 chord quality types (7th chords)
- 7 Ionian modes + 7 Harmonic Minor modes
- 4 inversions per chord
- 4 voicing types (Close, Drop 2, Drop 3, Open/Spread)
- Individual slew per output
- Performance mode vs Quantizer mode
- Unison mode for oscillator tuning
- ±2 octave transpose
- CV control over inversion and voicing

### Integration Goal

Bring Harmonàig-style harmonic intelligence to PEW|FORMER's 8-track sequencer, enabling:
- Automatic chord generation from single-note sequences
- Advanced harmonic progressions without music theory knowledge
- Polyphonic sequencing from monophonic tracks
- Dynamic chord modulation using existing Performer features

---

## Instruo Harmonàig Overview

### Physical Specifications

**Size**: 18HP
**Depth**: 27mm
**Power**: +12V @ 100mA, -12V @ 10mA

### I/O Configuration

**Inputs**:
- 1× CV Input (±10V, 20 octave range) with attenuverter
- CV control for Inversion (0-5V)
- CV control for Voicing (0-5V)
- CV control for Chord Quality (0-5V)

**Outputs**:
- 4× CV Outputs (±10V, 20 octave range):
  - Root (1st degree)
  - 3rd degree
  - 5th degree
  - 7th degree
- 1× Gate/Trigger output (mode-dependent)

### Chord Qualities (8 Types)

All chords are 4-part 7th chords:

| Symbol | Name | Intervals | Example (C root) |
|--------|------|-----------|------------------|
| **-∆7** | Minor Major 7 | 1, ♭3, 5, 7 | C, E♭, G, B |
| **○** | Diminished 7 | 1, ♭3, ♭5, ♭♭7 | C, E♭, G♭, B♭♭ |
| **ø** | Minor 7♭5 (Half-dim) | 1, ♭3, ♭5, ♭7 | C, E♭, G♭, B♭ |
| **-7** | Minor 7 | 1, ♭3, 5, ♭7 | C, E♭, G, B♭ |
| **7** | Dominant 7 | 1, 3, 5, ♭7 | C, E, G, B♭ |
| **∆7** | Major 7 | 1, 3, 5, 7 | C, E, G, B |
| **+∆7** | Augmented Major 7 | 1, 3, ♯5, 7 | C, E, G♯, B |
| **+7** | Augmented 7 | 1, 3, ♯5, ♭7 | C, E, G♯, B♭ |

### Scales & Modes (14 Total)

**Ionian Modes (7)**:
1. **Ionian** (Major): T-T-S-T-T-T-S (W-W-H-W-W-W-H)
2. **Dorian**: T-S-T-T-T-S-T
3. **Phrygian**: S-T-T-T-S-T-T
4. **Lydian**: T-T-T-S-T-T-S
5. **Mixolydian**: T-T-S-T-T-S-T
6. **Aeolian** (Natural Minor): T-S-T-T-S-T-T
7. **Locrian**: S-T-T-S-T-T-T

**Harmonic Minor Modes (7)**:
Each mode of the harmonic minor scale (T-S-T-T-S-T½-S)

### Diatonic Harmony

**Automatic Mode**: Chord quality is determined by the scale degree
- Each mode has specific chord qualities for each degree
- Ensures diatonic harmony within the selected scale

**Manual Mode**: User selects chord quality directly
- Allows non-diatonic/"wrong" notes
- Creates modal interchange and chromatic harmony

### Inversions (4 Types)

Inversions reorder chord notes by moving the lowest note up an octave:

1. **Root Position** (0): Root on bottom (e.g., C-E-G-B)
2. **1st Inversion** (1): 3rd on bottom (e.g., E-G-B-C)
3. **2nd Inversion** (2): 5th on bottom (e.g., G-B-C-E)
4. **3rd Inversion** (3): 7th on bottom (e.g., B-C-E-G)

### Voicings (4 Types)

Voicings displace notes to different octaves for harmonic spread:

1. **Close** (1): All notes within one octave
2. **Drop 2** (2): 2nd highest note dropped an octave
3. **Drop 3** (3): 3rd highest note dropped an octave
4. **Open/Spread** (4): Maximum spread across octaves

### Operational Modes

**Quantizer Mode**:
- Input CV is quantized to nearest scale degree
- Output is harmonized chord based on that degree
- Trigger output fires on pitch changes

**Performance Mode**:
- Button keyboard directly selects root note
- Bypasses quantization
- Gate output reflects button state
- Useful for manual chord progressions

**Unison Mode**:
- All 4 outputs produce identical pitch
- Useful for tuning oscillators
- Activated by special button combination

### Slew (Portamento)

- **Individual slew** per CV output
- Controls glide time between pitch changes
- Also known as Portamento, Slide, or Glide
- Useful for smooth chord transitions

### Transpose

- **±2 octave range** via slider
- Shifts all outputs simultaneously
- Works in both Quantizer and Performance modes

---

## Feature Comparison: Harmonàig vs Performer

### Harmonàig Strengths

✅ **Dedicated harmonic intelligence**
- Purpose-built for chord generation
- Comprehensive scale/mode library
- Sophisticated voicing control

✅ **4-voice polyphony from single input**
- Root, 3rd, 5th, 7th simultaneous outputs
- Polyphonic CV generation

✅ **Extensive music theory integration**
- Diatonic harmony enforcement
- Modal interchange support

### Performer Strengths

✅ **8 independent tracks**
- More voices available (8 vs 4)
- Each track fully programmable

✅ **Advanced sequencing features**
- Pulse count, gate mode, retrigger
- Accumulator modulation
- Fill sequences
- Song mode

✅ **Per-step control**
- Fine-grained parameter automation
- Step conditions
- Probabilistic sequencing

### Integration Synergies

**Combining strengths**:
- Use Performer's sequencing depth for harmonic progressions
- Apply Harmonàig's chord intelligence to Performer tracks
- Leverage Accumulator for dynamic chord modulation
- Use existing CV routing for harmonic control

**Example Use Case**:
- Track 1: Master harmony track (root notes)
- Tracks 2-5: Harmony followers (3rd, 5th, 7th, + doubled root)
- Accumulator: Modulate chord quality over time
- Pulse count + retrigger: Rhythmic chord arpeggiation

---

## Software Architecture

### Overview

The Harmonàig functionality will be integrated as a **track mode** within Performer's existing architecture, similar to how NoteTrack, CurveTrack, and MidiCvTrack coexist.

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                     HarmonyEngine                           │
│  - Scale/Mode lookup tables                                 │
│  - Chord quality calculation                                │
│  - Inversion logic                                          │
│  - Voicing algorithms                                       │
│  - Diatonic harmony enforcement                             │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │
┌───────────────────────────┴─────────────────────────────────┐
│                 HarmonyTrackEngine                          │
│  - Extends TrackEngine                                      │
│  - Manages master/follower relationships                    │
│  - Applies harmonic transformations                         │
│  - Routes CV outputs                                        │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │
┌───────────────────────────┴─────────────────────────────────┐
│                   HarmonySequence                           │
│  - Extends NoteSequence                                     │
│  - Stores harmony-specific parameters                       │
│  - Track role (Master/Follower/Independent)                 │
│  - Follower voice assignment (Root/3rd/5th/7th)             │
└─────────────────────────────────────────────────────────────┘
                            ▲
                            │
┌───────────────────────────┴─────────────────────────────────┐
│                      UI Pages                               │
│  - HarmonyGlobalPage (scale, mode, quality, etc.)          │
│  - HarmonyTrackPage (master/follower config)               │
│  - Integration with TopPage navigation                      │
└─────────────────────────────────────────────────────────────┘
```

### Integration Points

**1. Model Layer** (`src/apps/sequencer/model/`):
- `HarmonyEngine.h/cpp` - Core harmonic logic
- `HarmonySequence.h/cpp` - Extends NoteSequence
- `HarmonySettings.h/cpp` - Global harmony configuration

**2. Engine Layer** (`src/apps/sequencer/engine/`):
- `HarmonyTrackEngine.h/cpp` - New track engine type
- Modifications to `Engine.h` for harmony track container
- Integration with existing `TrackEngineContainer`

**3. UI Layer** (`src/apps/sequencer/ui/pages/`):
- `HarmonyGlobalPage.h/cpp` - Global harmony settings
- `HarmonyTrackConfigPage.h/cpp` - Per-track configuration
- `HarmonyListModel.h` - UI list model for parameters
- Updates to `TopPage.cpp` for navigation

**4. Project Layer** (`src/apps/sequencer/model/`):
- Extensions to `Project.h` for harmony settings
- Serialization support for harmony parameters

---

## Data Model Design

### HarmonyEngine Class

**Location**: `src/apps/sequencer/model/HarmonyEngine.h`

```cpp
class HarmonyEngine {
public:
    // Enums
    enum class Mode {
        // Ionian Modes
        Ionian,         // Major
        Dorian,
        Phrygian,
        Lydian,
        Mixolydian,
        Aeolian,        // Natural Minor
        Locrian,

        // Harmonic Minor Modes
        HarmonicMinor,
        LocrianNat6,
        IonianAug5,
        DorianSharp4,
        PhrygianDominant,
        LydianSharp2,
        SuperLocrianDiminished,

        Last
    };

    enum class ChordQuality {
        MinorMajor7,    // -∆7
        Diminished7,    // ○
        HalfDim7,       // ø (minor 7♭5)
        Minor7,         // -7
        Dominant7,      // 7
        Major7,         // ∆7
        AugMajor7,      // +∆7
        Augmented7,     // +7
        Last
    };

    enum class Voicing {
        Close,          // All notes within octave
        Drop2,          // 2nd highest note dropped
        Drop3,          // 3rd highest note dropped
        Spread,         // Maximum spread
        Last
    };

    enum class HarmonyRole {
        Independent,    // Normal track, no harmony
        Master,         // Provides root for harmony
        FollowerRoot,   // Outputs root
        Follower3rd,    // Outputs 3rd
        Follower5th,    // Outputs 5th
        Follower7th,    // Outputs 7th
        Last
    };

    // Constructor
    HarmonyEngine();

    // Configuration
    void setMode(Mode mode);
    void setChordQuality(ChordQuality quality);
    void setDiatonicMode(bool diatonic);  // Auto vs manual quality
    void setInversion(uint8_t inversion);  // 0-3
    void setVoicing(Voicing voicing);
    void setTranspose(int8_t semitones);   // -24 to +24

    // Getters
    Mode mode() const;
    ChordQuality chordQuality() const;
    bool diatonicMode() const;
    uint8_t inversion() const;
    Voicing voicing() const;
    int8_t transpose() const;

    // Core harmony calculation
    struct ChordNotes {
        int16_t root;   // MIDI note number
        int16_t third;
        int16_t fifth;
        int16_t seventh;
    };

    // Calculate harmonized notes from root note
    ChordNotes calculateChord(int16_t rootNote) const;

    // Calculate chord quality for scale degree (diatonic mode)
    ChordQuality calculateDiatonicQuality(int16_t scaleDegree) const;

    // Quantize to scale
    int16_t quantizeToScale(int16_t inputNote) const;

    // Serialization
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    // Internal state
    Mode _mode;
    ChordQuality _chordQuality;
    bool _diatonicMode;
    uint8_t _inversion;      // 0-3
    Voicing _voicing;
    int8_t _transpose;       // -24 to +24 semitones

    // Lookup tables
    static const uint8_t ScaleIntervals[14][7];  // 14 modes × 7 degrees
    static const uint8_t ChordIntervals[8][4];   // 8 qualities × 4 notes
    static const ChordQuality DiatonicChords[14][7];  // Mode × degree

    // Helper methods
    void applyInversion(ChordNotes &chord) const;
    void applyVoicing(ChordNotes &chord) const;
    int16_t getScaleDegree(int16_t note) const;
};
```

### HarmonySequence Class

**Location**: `src/apps/sequencer/model/HarmonySequence.h`

```cpp
class HarmonySequence : public NoteSequence {
public:
    HarmonySequence();

    // Harmony role configuration
    void setHarmonyRole(HarmonyEngine::HarmonyRole role);
    HarmonyEngine::HarmonyRole harmonyRole() const;

    // Master track reference (for followers)
    void setMasterTrackIndex(int8_t trackIndex);
    int8_t masterTrackIndex() const;

    // Individual slew per harmony voice
    void setSlewAmount(uint8_t amount);  // 0-127
    uint8_t slewAmount() const;

    // Serialization
    void write(VersionedSerializedWriter &writer) const override;
    void read(VersionedSerializedReader &reader) override;

private:
    HarmonyEngine::HarmonyRole _harmonyRole;
    int8_t _masterTrackIndex;  // -1 if not follower
    uint8_t _slewAmount;
};
```

### HarmonySettings Class (Global)

**Location**: `src/apps/sequencer/model/HarmonySettings.h`

```cpp
class HarmonySettings {
public:
    HarmonySettings();

    // Global harmony engine
    HarmonyEngine& harmonyEngine();
    const HarmonyEngine& harmonyEngine() const;

    // Performance mode
    void setPerformanceMode(bool enabled);
    bool performanceMode() const;

    // Unison mode
    void setUnisonMode(bool enabled);
    bool unisonMode() const;

    // Serialization
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    HarmonyEngine _harmonyEngine;
    bool _performanceMode;
    bool _unisonMode;
};
```

---

## Engine Integration

### HarmonyTrackEngine Class

**Location**: `src/apps/sequencer/engine/HarmonyTrackEngine.h`

```cpp
class HarmonyTrackEngine : public TrackEngine {
public:
    HarmonyTrackEngine(Engine &engine, Model &model, Track &track);

    // TrackEngine interface
    virtual void reset() override;
    virtual void restart() override;
    virtual void update(uint32_t tick) override;
    virtual void tick(uint32_t tick) override;
    virtual void changePattern() override;

    // Harmony-specific
    void setHarmonySequence(HarmonySequence *sequence);
    void setFillSequence(HarmonySequence *fillSequence);

    // Master/Follower coordination
    void setMasterEngine(HarmonyTrackEngine *master);
    HarmonyTrackEngine* masterEngine() const;

    // Current harmony state
    int16_t currentRootNote() const;
    HarmonyEngine::ChordNotes currentChord() const;

private:
    HarmonySequence *_sequence;
    HarmonySequence *_fillSequence;
    HarmonyTrackEngine *_masterEngine;  // For follower tracks

    // Current state
    int16_t _currentRootNote;
    HarmonyEngine::ChordNotes _currentChord;

    // Slew state (per follower track)
    float _currentCV;
    float _targetCV;
    uint32_t _slewTick;

    // Helper methods
    void updateHarmony();
    int16_t calculateHarmonizedNote();
    void applySlewToCV(uint32_t tick);
    int16_t noteToCV(int16_t midiNote) const;
};
```

### Integration with Engine.h

**Modifications to** `src/apps/sequencer/engine/Engine.h`:

```cpp
class Engine {
    // ... existing members ...

private:
    // Add harmony track engines to container
    using HarmonyTrackEngineContainer = Container<HarmonyTrackEngine, CONFIG_TRACK_COUNT>;
    HarmonyTrackEngineContainer _harmonyTrackEngines;

    // Harmony coordination
    void updateHarmonyMasterFollowerRelationships();
    HarmonyTrackEngine* findMasterHarmonyEngine();
};
```

### Engine Update Flow

```
┌─────────────────────────────────────────────────────┐
│ Engine::tick(tick)                                  │
│ 1. Update all track engines                        │
│ 2. Identify master harmony track                   │
│ 3. Calculate current chord from master              │
│ 4. Update follower tracks with harmonized notes    │
│ 5. Apply individual slew to each follower          │
│ 6. Output CV/Gate                                  │
└─────────────────────────────────────────────────────┘
```

**Pseudo-code**:

```cpp
void HarmonyTrackEngine::tick(uint32_t tick) {
    // If this is a master track
    if (_sequence->harmonyRole() == HarmonyEngine::HarmonyRole::Master) {
        // Evaluate current step note
        _currentRootNote = evalStepNote(tick);

        // Calculate chord from harmony engine
        const auto &settings = _model.project().harmonySettings();
        _currentChord = settings.harmonyEngine().calculateChord(_currentRootNote);
    }

    // If this is a follower track
    else if (_sequence->harmonyRole() != HarmonyEngine::HarmonyRole::Independent) {
        if (_masterEngine) {
            // Get harmonized note based on role
            int16_t harmonizedNote = calculateHarmonizedNote();

            // Apply slew
            _targetCV = noteToCV(harmonizedNote);
            applySlewToCV(tick);

            // Output CV
            setCV(_currentCV);
        }
    }

    // If independent track, behave like normal NoteTrack
    else {
        NoteTrackEngine::tick(tick);
    }
}

int16_t HarmonyTrackEngine::calculateHarmonizedNote() {
    const auto &chord = _masterEngine->currentChord();

    switch (_sequence->harmonyRole()) {
        case HarmonyEngine::HarmonyRole::FollowerRoot:
            return chord.root;
        case HarmonyEngine::HarmonyRole::Follower3rd:
            return chord.third;
        case HarmonyEngine::HarmonyRole::Follower5th:
            return chord.fifth;
        case HarmonyEngine::HarmonyRole::Follower7th:
            return chord.seventh;
        default:
            return 0;
    }
}
```

---

## UI/UX Design

### Page Structure

Following Performer's existing UI paradigm:

**Navigation Path**:
```
TopPage → Track Key (T1-T8) → Sequence Key → Harmony Mode
                           ↓
                    HarmonyGlobalPage (HARM)
                           ↓
                    HarmonyTrackPage (HTRK)
```

### HarmonyGlobalPage (HARM)

**Layout**: ListPage style (similar to AccumulatorPage)

**Parameters**:
1. **MODE**: Ionian modes + Harmonic Minor modes (14 options)
   - Display: "Ionian", "Dorian", "Phrygian", etc.
   - Encoder: Cycle through modes

2. **QUALITY**: Chord quality (8 options)
   - Display: "-∆7", "○", "ø", "-7", "7", "∆7", "+∆7", "+7"
   - Encoder: Select quality
   - Gray out if DIATONIC=On

3. **DIATONIC**: Auto/Manual chord quality
   - Display: "On" / "Off"
   - Encoder: Toggle
   - On: Auto quality from scale degree
   - Off: Manual quality selection

4. **INVERSION**: Chord inversion (0-3)
   - Display: "Root", "1st", "2nd", "3rd"
   - Encoder: Cycle inversions

5. **VOICING**: Voicing type (4 options)
   - Display: "Close", "Drop2", "Drop3", "Spread"
   - Encoder: Select voicing

6. **TRANSPOSE**: Transpose amount
   - Display: "-24" to "+24" semitones
   - Encoder: Adjust transpose

7. **PERF MODE**: Performance mode toggle
   - Display: "On" / "Off"
   - Encoder: Toggle
   - On: Button keyboard sets root
   - Off: Quantizer mode

8. **UNISON**: Unison mode toggle
   - Display: "On" / "Off"
   - Encoder: Toggle
   - On: All outputs same pitch

**Visual Feedback**:
- Current chord displayed: "C∆7" (root + quality symbol)
- Current notes: "C-E-G-B" (chord spelling)

### HarmonyTrackPage (HTRK)

**Layout**: ListPage style

**Parameters**:
1. **ROLE**: Track harmony role
   - Display: "Independent", "Master", "Root", "3rd", "5th", "7th"
   - Encoder: Select role

2. **MASTER TRK**: Master track reference (for followers)
   - Display: "T1" to "T8"
   - Encoder: Select master track
   - Gray out if ROLE=Independent or Master

3. **SLEW**: Slew amount (portamento)
   - Display: "0" to "127"
   - Encoder: Adjust slew
   - Gray out if ROLE=Independent or Master

**Visual Feedback**:
- If follower: Show current harmonized note
- Connection indicator: "→T1" (following Track 1)

### Integration with TopPage

**Navigation additions**:

```cpp
// In TopPage::functionShortPress(int index)
case Function::Sequence:
    if (track.trackMode() == Track::TrackMode::Harmony) {
        switch (_sequencePage) {
            case SequencePage::HarmonyGlobal:
                _manager.pages().harmonyTrackConfig.show();
                _sequencePage = SequencePage::HarmonyTrack;
                break;
            case SequencePage::HarmonyTrack:
                // Cycle back to HarmonyGlobal
                _manager.pages().harmonyGlobal.show();
                _sequencePage = SequencePage::HarmonyGlobal;
                break;
        }
    }
    break;
```

### Button Keyboard (Performance Mode)

When **PERF MODE = On**:

**Step Buttons (S1-S12)** map to chromatic scale:
- S1: C
- S2: C♯/D♭
- S3: D
- S4: D♯/E♭
- S5: E
- S6: F
- S7: F♯/G♭
- S8: G
- S9: G♯/A♭
- S10: A
- S11: A♯/B♭
- S12: B

**Visual Feedback**:
- Pressed button highlights
- Current chord displayed
- Gate output active while held

---

## CV/MIDI Integration

### CV Input Mapping

**Existing Performer CV inputs** (4 channels):
- **CV1**: Inversion control (0-5V → inversions 0-3)
- **CV2**: Voicing control (0-5V → voicings 1-4)
- **CV3**: Chord quality control (0-5V → qualities 0-7)
- **CV4**: Transpose control (-5V to +5V → ±2 octaves)

**Alternative**: User-configurable CV routing via existing Routing page

### CV Output Mapping

**Track harmony followers** use existing CV outputs:
- Track 1 (Master): CV1 OUT (root note from sequence)
- Track 2 (Root follower): CV2 OUT
- Track 3 (3rd follower): CV3 OUT
- Track 4 (5th follower): CV4 OUT
- Track 5 (7th follower): CV5 OUT
- Tracks 6-8: Available for other uses or doubled voices

### Gate Output

- **Master track**: Gate output follows sequence gates
- **Follower tracks**: Inherit gate from master (OR)
- **Independent tracks**: Normal gate behavior

### MIDI Integration

**MIDI Input**:
- Note messages: Control harmony root (Performance mode)
- CC messages: Map to harmony parameters
  - CC20: Mode
  - CC21: Quality
  - CC22: Inversion
  - CC23: Voicing
  - CC24: Transpose

**MIDI Output**:
- Send chord notes as polyphonic MIDI
- Each harmony voice on separate channel (if desired)

---

## Implementation Plan

### Phase 0: Research & Design (1 week)
**Goal**: Finalize architecture and data structures

**Tasks**:
- [ ] Complete music theory research (scale intervals, chord formulas)
- [ ] Design lookup tables for scales and chords
- [ ] Define exact API for HarmonyEngine
- [ ] Create detailed class diagrams
- [ ] Write implementation specification document

**Deliverables**:
- Completed HARMONIAG.md (this document)
- Scale/Chord lookup table data
- API specification

---

### Phase 1: Core HarmonyEngine (2 weeks)
**Goal**: Implement music theory logic in isolation

**Tasks**:
- [ ] Create `HarmonyEngine.h/cpp` skeleton
- [ ] Define enums (Mode, ChordQuality, Voicing, HarmonyRole)
- [ ] Implement scale interval lookup tables
- [ ] Implement chord interval lookup tables
- [ ] Implement diatonic chord quality tables
- [ ] Write `calculateChord()` method
- [ ] Write `calculateDiatonicQuality()` method
- [ ] Write `quantizeToScale()` method
- [ ] Implement inversion logic
- [ ] Implement voicing algorithms
- [ ] Add serialization support

**Testing**:
- [ ] Unit tests for each scale mode
- [ ] Unit tests for each chord quality
- [ ] Unit tests for inversions
- [ ] Unit tests for voicings
- [ ] Test file: `TestHarmonyEngine.cpp`

**Success Criteria**:
- ✅ All 14 modes produce correct scale intervals
- ✅ All 8 chord qualities produce correct notes
- ✅ Inversions correctly reorder notes
- ✅ Voicings produce expected octave displacements
- ✅ Diatonic mode selects correct qualities per degree

---

### Phase 2: Data Model Extension (1 week)
**Goal**: Extend Performer's data model for harmony

**Tasks**:
- [ ] Create `HarmonySequence.h/cpp`
- [ ] Extend NoteSequence with harmony parameters
- [ ] Create `HarmonySettings.h/cpp` for global config
- [ ] Add to `Project.h` data model
- [ ] Implement serialization for all new classes
- [ ] Add track mode enum value `TrackMode::Harmony`

**Testing**:
- [ ] Unit tests for HarmonySequence
- [ ] Serialization tests
- [ ] Test file: `TestHarmonySequence.cpp`

**Success Criteria**:
- ✅ Harmony parameters save/load correctly
- ✅ Track roles persist across sessions
- ✅ Global harmony settings serialize properly

---

### Phase 3: Engine Integration (2 weeks)
**Goal**: Integrate harmony logic into sequencer engine

**Tasks**:
- [ ] Create `HarmonyTrackEngine.h/cpp`
- [ ] Implement master track logic
- [ ] Implement follower track logic
- [ ] Implement master/follower relationship management
- [ ] Add slew processing per follower
- [ ] Integrate with Engine.cpp tick() loop
- [ ] Handle pattern changes and resets
- [ ] Implement unison mode
- [ ] Implement performance mode vs quantizer mode

**Testing**:
- [ ] Integration tests (disabled by default due to hardware dependencies)
- [ ] Test master track generates chords
- [ ] Test followers output correct harmonized notes
- [ ] Test slew behavior
- [ ] Test file: `TestHarmonyTrackEngine.cpp` (likely disabled like TestNoteTrackEngine)

**Success Criteria**:
- ✅ Master track sequences work normally
- ✅ Follower tracks output harmonized CV
- ✅ Slew smooths pitch transitions
- ✅ Performance mode allows manual chord triggering

---

### Phase 4: UI Implementation (2 weeks)
**Goal**: Create user interface for harmony features

**Tasks**:
- [ ] Create `HarmonyGlobalPage.h/cpp`
- [ ] Create `HarmonyTrackConfigPage.h/cpp`
- [ ] Create `HarmonyListModel.h` for parameter lists
- [ ] Integrate with TopPage navigation
- [ ] Add visual feedback (chord display, note names)
- [ ] Implement button keyboard for Performance mode
- [ ] Add help text and documentation

**Testing**:
- [ ] Manual UI testing on simulator
- [ ] Test all parameter value ranges
- [ ] Test navigation flow
- [ ] Test visual feedback accuracy

**Success Criteria**:
- ✅ All parameters accessible and editable
- ✅ Visual feedback shows correct chord info
- ✅ Navigation smooth and intuitive
- ✅ Performance mode keyboard responsive

---

### Phase 5: CV/MIDI Integration (1 week)
**Goal**: Wire up external CV and MIDI control

**Tasks**:
- [ ] Map CV inputs to harmony parameters
- [ ] Configure CV output routing for follower tracks
- [ ] Implement MIDI input for root note control
- [ ] Implement MIDI CC mapping for parameters
- [ ] Implement polyphonic MIDI output
- [ ] Add to RoutingPage configuration

**Testing**:
- [ ] Test CV modulation of parameters
- [ ] Test MIDI note input
- [ ] Test MIDI CC control
- [ ] Hardware testing with actual CV sources

**Success Criteria**:
- ✅ CV inputs modulate harmony parameters
- ✅ MIDI notes trigger chords in Performance mode
- ✅ MIDI CC controls work correctly
- ✅ Routing configuration persists

---

### Phase 6: Advanced Features (1 week)
**Goal**: Add polish and advanced capabilities

**Tasks**:
- [ ] Integrate with Accumulator feature
  - Accumulator can modulate mode, quality, inversion, voicing
- [ ] Add chord progression templates
- [ ] Implement "strum" timing offset between voices
- [ ] Add harmonic rhythm patterns
- [ ] Performance optimizations

**Testing**:
- [ ] Test accumulator modulation
- [ ] Test progression templates
- [ ] Performance profiling
- [ ] CPU load testing

**Success Criteria**:
- ✅ Accumulator modulates harmony smoothly
- ✅ Templates provide quick chord progressions
- ✅ CPU usage < 10% additional load
- ✅ No audio glitches under heavy use

---

### Phase 7: Testing & Documentation (1 week)
**Goal**: Comprehensive testing and user documentation

**Tasks**:
- [ ] Write comprehensive test suite
- [ ] Hardware stress testing
- [ ] Create user manual section
- [ ] Record demo videos
- [ ] Update CLAUDE.md with harmony documentation
- [ ] Create example projects
- [ ] Performance regression testing

**Testing Scenarios**:
- [ ] All 14 modes × 8 qualities = 112 chord combinations
- [ ] 4 inversions × 4 voicings = 16 variations per chord
- [ ] Master + 4 followers simultaneous playback
- [ ] Pattern changes during playback
- [ ] Project load/save
- [ ] Long-duration stability test (30+ minutes)

**Success Criteria**:
- ✅ All test scenarios pass
- ✅ Documentation complete and accurate
- ✅ Zero crashes or glitches
- ✅ User feedback positive

---

### Total Estimated Time: 10-12 weeks

**Breakdown**:
- Research: 1 week
- Core implementation: 5 weeks (Phases 1-3)
- UI/Integration: 3 weeks (Phases 4-5)
- Polish/Testing: 2 weeks (Phases 6-7)

---

## Testing Strategy

### Unit Tests

**Test Files**:
- `TestHarmonyEngine.cpp` - Core music theory logic
- `TestHarmonySequence.cpp` - Data model
- `TestHarmonyTrackEngine.cpp` - Engine integration (disabled by default)

**Coverage**:
- ✅ All scale modes produce correct intervals
- ✅ All chord qualities generate correct notes
- ✅ Inversions work for all chords
- ✅ Voicings produce expected results
- ✅ Diatonic quality selection accurate
- ✅ Quantization to scale correct
- ✅ Serialization round-trip succeeds

### Integration Tests

**Hardware Testing** (Phase 7):
1. **Basic Chord Generation**:
   - Master track plays C-D-E-F
   - Followers output correct 3rd, 5th, 7th
   - Verify CV voltages with oscilloscope

2. **Mode Changes**:
   - Switch between Ionian, Dorian, Phrygian
   - Verify chord qualities change diatonically

3. **Manual Quality**:
   - Disable diatonic mode
   - Manually select each of 8 qualities
   - Verify correct chord intervals

4. **Inversions**:
   - Cycle through 4 inversions
   - Verify note order changes correctly

5. **Voicings**:
   - Test Close, Drop2, Drop3, Spread
   - Verify octave displacements

6. **Slew**:
   - Set varying slew amounts per follower
   - Verify smooth pitch transitions

7. **Performance Mode**:
   - Use button keyboard
   - Verify immediate chord response

8. **CV Modulation**:
   - Patch LFO to inversion CV
   - Verify smooth inversion changes

9. **Accumulator Integration**:
   - Modulate chord quality with accumulator
   - Verify smooth harmonic evolution

10. **Stress Test**:
    - 8 tracks active
    - Fast tempo (180 BPM)
    - Complex patterns
    - Verify stability

### Music Theory Verification

**Chord Correctness Tests**:

Example test matrix (excerpt):

| Root | Mode | Degree | Expected Quality | Expected Notes |
|------|------|--------|------------------|----------------|
| C | Ionian | I | ∆7 | C-E-G-B |
| C | Ionian | ii | -7 | D-F-A-C |
| C | Ionian | V | 7 | G-B-D-F |
| C | Dorian | i | -7 | C-E♭-G-B♭ |
| C | Phrygian | i | -7 | C-E♭-G-B♭ |

All 14 modes × 7 degrees = 98 combinations must be verified.

---

## Future Enhancements

### Phase 8+ (Optional)

**Advanced Harmony Features**:
- [ ] **Voice Leading**: Smart voice leading between chords
- [ ] **Bass Note Control**: Independent bass note from chord
- [ ] **Tensions**: Add 9th, 11th, 13th extensions
- [ ] **Alterations**: ♯5, ♭9, ♯9, ♯11, ♭13
- [ ] **Poly-chords**: Upper structure triads
- [ ] **Scale Detection**: Analyze incoming CV, suggest scale
- [ ] **Chord Progression AI**: Generate progressions in key
- [ ] **Harmonic Rhythm**: Independent rhythm for chord changes
- [ ] **Borrowed Chords**: Modal interchange suggestions

**Performance Enhancements**:
- [ ] **Chord Memory**: Store favorite chord shapes
- [ ] **Macro Controls**: Single knob morphs multiple parameters
- [ ] **Randomization**: Controlled random harmony generation
- [ ] **Arpeggiator Integration**: Arpeggiate generated chords
- [ ] **Strumming**: Delay between chord voices

**Integration Enhancements**:
- [ ] **Harmony Scenes**: Snapshot and recall harmony settings
- [ ] **Song Mode Integration**: Automate harmony changes
- [ ] **MIDI Learn**: Map MIDI controllers to parameters
- [ ] **Euclidean Harmony**: Euclidean rhythm triggers chord changes

---

## Music Theory Reference

### Ionian Mode Interval Formulas

| Mode | Formula (T=Tone, S=Semitone) | Intervals from Root |
|------|------------------------------|---------------------|
| Ionian (Major) | T-T-S-T-T-T-S | 0-2-4-5-7-9-11 |
| Dorian | T-S-T-T-T-S-T | 0-2-3-5-7-9-10 |
| Phrygian | S-T-T-T-S-T-T | 0-1-3-5-7-8-10 |
| Lydian | T-T-T-S-T-T-S | 0-2-4-6-7-9-11 |
| Mixolydian | T-T-S-T-T-S-T | 0-2-4-5-7-9-10 |
| Aeolian (Minor) | T-S-T-T-S-T-T | 0-2-3-5-7-8-10 |
| Locrian | S-T-T-S-T-T-T | 0-1-3-5-6-8-10 |

### Harmonic Minor Interval Formulas

| Mode | Formula | Intervals from Root |
|------|---------|---------------------|
| Harmonic Minor | T-S-T-T-S-T½-S | 0-2-3-5-7-8-11 |
| Locrian ♮6 | S-T-T-S-T½-S-T | 0-1-3-5-6-9-10 |
| Ionian ♯5 | T-T-S-T½-S-T-S | 0-2-4-5-8-9-11 |
| Dorian ♯4 | T-S-T½-S-T-S-T | 0-2-3-6-7-9-10 |
| Phrygian Dominant | S-T½-S-T-S-T-T | 0-1-4-5-7-8-10 |
| Lydian ♯2 | T½-S-T-S-T-T-S | 0-3-4-6-7-9-11 |
| Super Locrian | S-T-S-T-T-S-T½ | 0-1-3-4-6-8-10 |

### Chord Quality Formulas

| Quality | Symbol | Intervals | Semitones |
|---------|--------|-----------|-----------|
| Minor Major 7 | -∆7 | R, ♭3, 5, 7 | 0-3-7-11 |
| Diminished 7 | ○ | R, ♭3, ♭5, ♭♭7 | 0-3-6-9 |
| Half-Diminished | ø | R, ♭3, ♭5, ♭7 | 0-3-6-10 |
| Minor 7 | -7 | R, ♭3, 5, ♭7 | 0-3-7-10 |
| Dominant 7 | 7 | R, 3, 5, ♭7 | 0-4-7-10 |
| Major 7 | ∆7 | R, 3, 5, 7 | 0-4-7-11 |
| Augmented Major 7 | +∆7 | R, 3, ♯5, 7 | 0-4-8-11 |
| Augmented 7 | +7 | R, 3, ♯5, ♭7 | 0-4-8-10 |

### Diatonic Chord Qualities by Mode

**Ionian (Major Scale)**:
I∆7 - ii-7 - iii-7 - IV∆7 - V7 - vi-7 - viiø

**Dorian**:
i-7 - ii-7 - ♭III∆7 - IV7 - v-7 - viø - ♭VII∆7

**Phrygian**:
i-7 - ♭II∆7 - ♭iii7 - iv-7 - vø - ♭VI∆7 - ♭vii-7

(etc. for all 14 modes)

---

## Implementation Complexity Analysis

### Risk Assessment

**🟢 LOW RISK**:
- Music theory lookup tables (deterministic)
- Data model extensions (follows existing patterns)
- Serialization (proven framework)

**🟡 MEDIUM RISK**:
- HarmonyTrackEngine integration (complex state management)
- Master/Follower coordination (timing sensitive)
- Slew implementation (real-time processing)

**🔴 HIGH RISK**:
- Performance impact (8 tracks × harmony calculations)
- UI responsiveness under load
- Memory footprint (lookup tables)

### Performance Considerations

**CPU Load Estimate**:
- Harmony calculation: ~0.5% per track
- Total with 4 followers: ~2-3% additional load
- Within acceptable range (<10% total)

**Memory Footprint**:
- Scale interval tables: 14 modes × 7 degrees × 1 byte = 98 bytes
- Chord interval tables: 8 qualities × 4 notes × 1 byte = 32 bytes
- Diatonic quality tables: 14 modes × 7 degrees × 1 byte = 98 bytes
- Total static data: ~250 bytes (negligible)

**Real-time Constraints**:
- Harmony calculations must complete within tick period
- Estimated: <100μs per track (well within budget)

---

## Conclusion

The Instruo Harmonàig integration represents a **significant feature enhancement** to PEW|FORMER, bringing sophisticated harmonic intelligence to the sequencer. The implementation leverages Performer's existing architecture while adding powerful new capabilities for chord-based composition and performance.

**Key Benefits**:
- ✅ Polyphonic sequencing from monophonic tracks
- ✅ Advanced harmonic control without deep music theory knowledge
- ✅ Seamless integration with existing Performer features
- ✅ Powerful combination with Accumulator for evolving harmony

**Implementation Status**:
- 📋 Planning complete
- ⏳ Awaiting development resources
- 🎯 Estimated: 10-12 weeks to completion

**Recommendation**: Proceed with Phase 0 (Research & Design) to validate music theory lookup tables and finalize API specifications before committing to full implementation.

---

## References

- **Instruo Harmonàig Manual**: https://www.instruomodular.com/product/harmonaig/
- **CLAUDE.md**: PEW|FORMER project documentation
- **Queue-BasedAccumTicks.md**: Architecture patterns reference
- **TDD-METHOD.md**: Testing methodology
- **Music Theory**: Berklee College of Music harmony texts

---

**END OF DOCUMENT**
