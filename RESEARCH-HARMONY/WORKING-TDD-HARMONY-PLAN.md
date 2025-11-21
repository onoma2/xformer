# WORKING-TDD-HARMONY-PLAN.md - Phased TDD Implementation Plan

**Date Started**: 2025-11-18
**Last Updated**: 2025-11-20
**Status**: 🟢 **PHASE 2 COMPLETE - INVERSION & VOICING FULLY IMPLEMENTED**
**Approach Chosen**: ✅ **Option B - Direct Integration** (simplified from Option 4)
**Original Estimate**: 7-11 weeks (to Phase 3 production-ready)
**Actual Time**: ~3 days (Phase 1-2 complete implementation)

---

## ✅ IMPLEMENTATION COMPLETE

**See [`HARMONY-DONE.md`](./HARMONY-DONE.md) for complete implementation summary.**

### Quick Summary

**Architecture**: Option B - Direct Integration into NoteTrackEngine

**Completed**:
- ✅ HarmonyEngine with all 7 modal scales
- ✅ NoteSequence harmony properties (Role, Master, Scale)
- ✅ Model integration (central HarmonyEngine instance)
- ✅ Engine integration (direct modulation in evalStepNote)
- ✅ UI layer (HarmonyPage with navigation)
- ✅ 19 passing unit tests
- ✅ Hardware build successful
- ✅ Comprehensive testing guide ([HARMONY-HARDWARE-TESTS.md](./HARMONY-HARDWARE-TESTS.md))

**What Works Now**:
- Master/follower track assignment (any track can be master or follower)
- 7 modal scales (Ionian, Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian)
- 4-voice harmonization (root, 3rd, 5th, 7th)
- Synchronized step playback
- Full UI integration

**Completed (Phase 2 - 2025-11-20)**:
- ✅ Inversion parameter (0-3) - FULLY IMPLEMENTED with transformation algorithms
- ✅ Voicing parameter (Close/Drop2/Drop3/Spread) - FULLY IMPLEMENTED with transformation algorithms
- ✅ Bug fix: Sequence-level inversion/voicing now correctly reads from master track (not follower)
- ✅ 17 additional unit tests for voicing (TestHarmonyVoicing.cpp)
- ✅ 24 additional unit tests for inversion investigation (TestHarmonyInversionIssue.cpp)
- **Status**: All transformation algorithms implemented and tested

---

## Original Plan Documentation Follows Below

**NOTE**: The sections below describe the original Option 4 (Phased Hybrid) plan.

**ACTUAL IMPLEMENTATION**: Used simplified **Option B (Direct Integration)** as documented in [HARMONY-DONE.md](./HARMONY-DONE.md).

The original plan is preserved for reference but should be considered superseded.

---

## Executive Summary

This plan implements Harmonàig-style harmonic sequencing through **incremental phases**, each deliverable and testable independently. Following strict TDD methodology with RED-GREEN-REFACTOR cycles.

**Key Principles**:
- ✅ Test-first development for ALL features
- ✅ Each phase delivers working functionality
- ✅ Gate at phase completion before proceeding
- ✅ Can stop at any phase with usable feature
- ✅ Addresses all critical architectural constraints

**Critical Fixes Applied**:
1. ✅ Scope aligned with MVP-first approach
2. ✅ Track position constraints (T1/T5 only) architected
3. ✅ Scale override per HarmonyRoot added
4. ✅ Master/follower tick ordering specified
5. ✅ Parametrized testing for combination explosion
6. ✅ Voicing/inversion as dual (global + per-step) parameters
7. ✅ Accumulator integration with all trigger modes

---

## Phase Overview

| Phase | Scope | Duration | Deliverable | Risk |
|-------|-------|----------|-------------|------|
| **1 MVP** | Ionian, diatonic, root pos, close voice | 2-3 weeks | Basic harmony working | 🟢 LOW |
| **2 Modes** | +6 Ionian modes, +1st inversion, transpose | 2-3 weeks | All major/minor modes | 🟢 LOW |
| **3 Polish** | +Drop2 voicing, slew, per-step params | 1-2 weeks | Production-ready lite | 🟡 MEDIUM |
| **4 Advanced** | +4 qualities, CV control, perf mode | 2-3 weeks | Full-featured | 🟡 MEDIUM |
| **5+ Future** | Harmonic Minor, all inversions/voicings | Optional | Complete Harmonàig | 🔴 HIGH |

**Gate at Phase 1**: Review effort vs estimate, decide to proceed or stop.

---

## Architecture Constraints (CRITICAL)

### 1. Track Position Restrictions

**HARD CONSTRAINT**: Only Tracks 1 and 5 can be HarmonyRoot (Master).

**Rationale** (from HARMONY-IDEATION.md):
- Track 1 → Followers: Tracks 2, 3, 4 (Harm3, Harm5, Harm7)
- Track 5 → Followers: Tracks 6, 7, 8 (Harm3, Harm5, Harm7)

**Implementation**:
```cpp
// HarmonySequence must know its track index
class HarmonySequence : public NoteSequence {
public:
    HarmonySequence(uint8_t trackIndex); // NEW: Constructor takes track index

    bool canBeHarmonyRoot() const {
        return (_trackIndex == 0 || _trackIndex == 4); // Tracks 1 or 5
    }

    void setHarmonyRole(HarmonyEngine::HarmonyRole role) {
        if (role == HarmonyEngine::HarmonyRole::Master && !canBeHarmonyRoot()) {
            // Reject or auto-revert to Follower
            _harmonyRole = HarmonyEngine::HarmonyRole::Follower3rd; // Default follower
            return;
        }
        _harmonyRole = role;
    }

private:
    uint8_t _trackIndex; // 0-7 (T1-T8)
};
```

### 2. Scale Override per HarmonyRoot

**CONSTRAINT**: HarmonyRoot tracks use their own scale setting, NOT the project scale.

**Implementation**:
```cpp
class HarmonySequence : public NoteSequence {
public:
    // HarmonyRoot-specific scale (only valid if harmonyRole == Master)
    void setScale(HarmonyEngine::Mode mode) {
        if (_harmonyRole == HarmonyEngine::HarmonyRole::Master) {
            _scale = mode;
        }
    }

    HarmonyEngine::Mode scale() const {
        return _scale; // Used by harmony engine
    }

    bool usesOwnScale() const {
        return (_harmonyRole == HarmonyEngine::HarmonyRole::Master);
    }

private:
    HarmonyEngine::Mode _scale; // Default: Ionian
};
```

### 3. Master/Follower Tick Ordering

**CONSTRAINT**: Master tracks MUST tick before follower tracks in Engine::tick() loop.

**Implementation**:
```cpp
void Engine::tick(uint32_t tick) {
    // CRITICAL: Three-phase ticking to prevent race conditions

    // PHASE 1: Tick all HarmonyRoot (Master) tracks
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        if (_harmonyTrackEngines[i].isMaster()) {
            _harmonyTrackEngines[i].tick(tick);
            // Master now has _currentChord calculated
        }
    }

    // PHASE 2: Tick all Follower tracks
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        if (_harmonyTrackEngines[i].isFollower()) {
            _harmonyTrackEngines[i].tick(tick);
            // Follower reads master's _currentChord
        }
    }

    // PHASE 3: Tick all Independent/other tracks
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        if (_harmonyTrackEngines[i].isIndependent()) {
            _harmonyTrackEngines[i].tick(tick);
        }
    }

    // Then tick other engine types (Curve, MidiCv, etc.)
    // ...
}
```

### 4. Voicing/Inversion Dual Model

**CONSTRAINT**: Voicing and Inversion exist as BOTH global defaults AND per-step overrides.

**Model**:
- Global defaults on HarmonyGlobalPage (HARM)
- Per-step overrides via F3 NOTE button cycling
- Step inherits global if not explicitly set

**Implementation**:
```cpp
class HarmonyEngine {
    // Global defaults
    Voicing _globalVoicing = Voicing::Close;
    uint8_t _globalInversion = 0; // Root position
};

class NoteSequence::Step {
    // Per-step overrides (stored in _data1 bitfields)
    bool _hasLocalVoicing : 1;
    Voicing _voicing : 2; // 0-3 (4 voicings)
    bool _hasLocalInversion : 1;
    uint8_t _inversion : 2; // 0-3 (4 inversions)

    Voicing effectiveVoicing(const HarmonyEngine& engine) const {
        return _hasLocalVoicing ? _voicing : engine.globalVoicing();
    }

    uint8_t effectiveInversion(const HarmonyEngine& engine) const {
        return _hasLocalInversion ? _inversion : engine.globalInversion();
    }
};
```

---
## PHASE 2: Modes & Inversions - 2-3 Weeks

**Goal**: Expand to all 7 Ionian modes, add 1st inversion, add transpose.

**Deliverable**: Users can sequence in any major/minor mode with inversions.

### Scope Additions

✅ **Added in Phase 2**:
- +6 Ionian modes (Dorian, Phrygian, Lydian, Mixolydian, Aeolian, Locrian)
- 1st inversion (in addition to root position)
- Transpose control (-24 to +24 semitones)
- Global HarmonyGlobalPage (HARM) for mode/inversion/transpose

❌ **Still Excluded**:
- Harmonic Minor modes (Phase 5+)
- 2nd/3rd inversions (Phase 5+)
- Drop2 voicing (Phase 3)
- Slew (Phase 3)
- Per-step voicing/inversion (Phase 3)

---

### Phase 2 Task Breakdown

---

#### Task 2.1: Parametrized Mode Testing (Week 4, Day 1)

**RED**: Test all 7 Ionian modes with parametrized testing

```cpp
// File: src/tests/unit/sequencer/TestHarmonyEngine.cpp

class HarmonyEngineModeTest : public ::testing::TestWithParam<HarmonyEngine::Mode> {};

TEST_P(HarmonyEngineModeTest, ScaleIntervalsCorrect) {
    HarmonyEngine engine;
    engine.setMode(GetParam());

    // Validate that all 7 scale degrees produce valid intervals
    for (int degree = 0; degree < 7; degree++) {
        int16_t interval = engine.getScaleInterval(degree);
        ASSERT_GE(interval, 0);
        ASSERT_LE(interval, 11); // Within octave
    }
}

TEST_P(HarmonyEngineModeTest, DiatonicQualitiesValid) {
    HarmonyEngine engine;
    engine.setMode(GetParam());

    // Validate that all 7 degrees have valid chord qualities
    for (int degree = 0; degree < 7; degree++) {
        auto quality = engine.getDiatonicQuality(degree);
        ASSERT_LT(static_cast<int>(quality), static_cast<int>(HarmonyEngine::ChordQuality::Last));
    }
}

INSTANTIATE_TEST_SUITE_P(
    AllIonianModes,
    HarmonyEngineModeTest,
    ::testing::Values(
        HarmonyEngine::Mode::Ionian,
        HarmonyEngine::Mode::Dorian,
        HarmonyEngine::Mode::Phrygian,
        HarmonyEngine::Mode::Lydian,
        HarmonyEngine::Mode::Mixolydian,
        HarmonyEngine::Mode::Aeolian,
        HarmonyEngine::Mode::Locrian
    )
);
// Runs 7 modes × 2 tests = 14 tests automatically
```

**GREEN**: Scale intervals and diatonic tables already implemented in Phase 1 Task 1.3.

**REFACTOR**: Verify lookup tables match music theory (manual verification).

---

#### Task 2.2: Inversion Logic Implementation (Week 4, Days 2-3)

**RED**: Test root position and 1st inversion

```cpp
TEST(HarmonyEngine, RootPositionInversion) {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);
    engine.setInversion(0); // Root position

    // C Major 7 in root position: C(60)-E(64)-G(67)-B(71)
    auto chord = engine.harmonize(60, 0);
    ASSERT_EQ(chord.root, 60);
    ASSERT_EQ(chord.third, 64);
    ASSERT_EQ(chord.fifth, 67);
    ASSERT_EQ(chord.seventh, 71);

    // Lowest note should be root
    int16_t lowestNote = std::min({chord.root, chord.third, chord.fifth, chord.seventh});
    ASSERT_EQ(lowestNote, chord.root);
}

TEST(HarmonyEngine, FirstInversion) {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);
    engine.setInversion(1); // 1st inversion

    // C Major 7 in 1st inversion: E(64)-G(67)-B(71)-C(72)
    // 3rd moved to bass, root moved up an octave
    auto chord = engine.harmonize(60, 0);

    // Lowest note should be 3rd (E)
    int16_t lowestNote = std::min({chord.root, chord.third, chord.fifth, chord.seventh});
    ASSERT_EQ(lowestNote, chord.third);

    // Root should be up an octave
    ASSERT_EQ(chord.root, 72); // C up octave
    ASSERT_EQ(chord.third, 64); // E (now bass note)
    ASSERT_EQ(chord.fifth, 67); // G
    ASSERT_EQ(chord.seventh, 71); // B
}

TEST(HarmonyEngine, InversionBoundary) {
    HarmonyEngine engine;

    // Inversion values beyond 1 should clamp to 1 (Phase 2 limit)
    engine.setInversion(5);
    ASSERT_EQ(engine.inversion(), 1); // Clamped to max for Phase 2
}
```

**GREEN**: Implement inversion logic

```cpp
// In HarmonyEngine.cpp
HarmonyEngine::ChordNotes HarmonyEngine::harmonize(int16_t rootNote, uint8_t scaleDegree) const {
    ChordNotes chord;

    // Get chord quality and intervals (existing Phase 1 code)
    ChordQuality quality = getDiatonicQuality(scaleDegree);
    ChordIntervals intervals = getChordIntervals(quality);

    // Apply intervals to root note (existing Phase 1 code)
    chord.root = applyInterval(rootNote, intervals[0]);
    chord.third = applyInterval(rootNote, intervals[1]);
    chord.fifth = applyInterval(rootNote, intervals[2]);
    chord.seventh = applyInterval(rootNote, intervals[3]);

    // NEW Phase 2: Apply inversion
    applyInversion(chord);

    // Phase 3: Apply voicing
    // Phase 2: Apply transpose
    applyTranspose(chord);

    return chord;
}

void HarmonyEngine::applyInversion(ChordNotes &chord) const {
    if (_inversion == 0) return; // Root position, no change

    // Phase 2: Only support 1st inversion
    if (_inversion >= 1) {
        // 1st inversion: Move 3rd to bass, raise root by octave
        chord.root += 12; // Root up an octave
        // 3rd stays at original pitch (becomes bass)
    }

    // Phase 5+: Support 2nd and 3rd inversions
}
```

**REFACTOR**: Generalize for future 2nd/3rd inversions.

---

#### Task 2.3: Transpose Implementation (Week 4, Day 3)

**RED**: Test transpose across full range

```cpp
TEST(HarmonyEngine, TransposeUp) {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);
    engine.setTranspose(12); // Up 1 octave

    // C Major 7 transposed up 1 octave
    auto chord = engine.harmonize(60, 0);
    ASSERT_EQ(chord.root, 72);   // C5
    ASSERT_EQ(chord.third, 76);  // E5
    ASSERT_EQ(chord.fifth, 79);  // G5
    ASSERT_EQ(chord.seventh, 83); // B5
}

TEST(HarmonyEngine, TransposeDown) {
    HarmonyEngine engine;
    engine.setTranspose(-12); // Down 1 octave

    auto chord = engine.harmonize(60, 0);
    ASSERT_EQ(chord.root, 48);   // C3
    ASSERT_EQ(chord.third, 52);  // E3
    ASSERT_EQ(chord.fifth, 55);  // G3
    ASSERT_EQ(chord.seventh, 59); // B3
}

TEST(HarmonyEngine, TransposeClamping) {
    HarmonyEngine engine;
    engine.setTranspose(30); // Beyond +24 limit
    ASSERT_EQ(engine.transpose(), 24); // Clamped to max

    engine.setTranspose(-30); // Beyond -24 limit
    ASSERT_EQ(engine.transpose(), -24); // Clamped to min
}

TEST(HarmonyEngine, TransposeMIDIRangeClamping) {
    HarmonyEngine engine;
    engine.setTranspose(24); // +2 octaves

    // High root note that would exceed MIDI 127
    auto chord = engine.harmonize(120, 0); // G#8
    // All notes should clamp to 127
    ASSERT_LE(chord.root, 127);
    ASSERT_LE(chord.third, 127);
    ASSERT_LE(chord.fifth, 127);
    ASSERT_LE(chord.seventh, 127);
}
```

**GREEN**: Implement transpose

```cpp
// In HarmonyEngine.cpp
void HarmonyEngine::applyTranspose(ChordNotes &chord) const {
    if (_transpose == 0) return;

    // Apply transpose to all chord notes
    chord.root = applyInterval(chord.root, _transpose);
    chord.third = applyInterval(chord.third, _transpose);
    chord.fifth = applyInterval(chord.fifth, _transpose);
    chord.seventh = applyInterval(chord.seventh, _transpose);

    // applyInterval() already clamps to MIDI range 0-127
}
```

**REFACTOR**: Refactor applyInterval to handle negative offsets.

---

#### Task 2.4: HarmonyGlobalPage Implementation (Week 5, Days 1-3)

**RED**: Test global harmony page parameter access

```cpp
TEST(HarmonyGlobalPage, ParameterAccess) {
    Model model;
    HarmonyEngine &engine = model.project().harmonyEngine(); // Add to Project
    HarmonyGlobalPage page(manager, model);

    // Phase 2: 3 parameters (MODE, INVERSION, TRANSPOSE)
    ASSERT_EQ(page.parameterCount(), 3);

    // Test MODE parameter
    page.setParameter(0); // MODE
    page.setValue(static_cast<int>(HarmonyEngine::Mode::Dorian));
    ASSERT_EQ(engine.mode(), HarmonyEngine::Mode::Dorian);

    // Test INVERSION parameter
    page.setParameter(1); // INVERSION
    page.setValue(1); // 1st inversion
    ASSERT_EQ(engine.inversion(), 1);

    // Test TRANSPOSE parameter
    page.setParameter(2); // TRANSPOSE
    page.setValue(12); // +1 octave
    ASSERT_EQ(engine.transpose(), 12);
}
```

**GREEN**: Implement HarmonyGlobalPage

```cpp
// File: src/apps/sequencer/ui/pages/HarmonyGlobalPage.h

class HarmonyGlobalPage : public ListPage {
public:
    HarmonyGlobalPage(PageManager &manager, Model &model)
        : ListPage(manager)
        , _model(model)
    {}

    virtual void draw(Canvas &canvas) override {
        WindowPainter::drawActiveFunction(canvas, Function::Sequence);
        WindowPainter::drawHeader(canvas, _model, _model.project(), "HARMONY GLOBAL");

        // Draw parameter list
        // ...
    }

    virtual int parameterCount() const override {
        return 3; // Phase 2: MODE, INVERSION, TRANSPOSE
        // Phase 3: +1 (VOICING)
        // Phase 4: +2 (QUALITY, DIATONIC)
    }

    virtual const char* parameterName(int index) const override {
        switch (index) {
            case 0: return "MODE";
            case 1: return "INVERSION";
            case 2: return "TRANSPOSE";
            default: return "";
        }
    }

    virtual int parameterValue(int index) const override {
        auto &engine = _model.project().harmonyEngine();
        switch (index) {
            case 0: return static_cast<int>(engine.mode());
            case 1: return engine.inversion();
            case 2: return engine.transpose();
            default: return 0;
        }
    }

    virtual void setParameterValue(int index, int value) override {
        auto &engine = _model.project().harmonyEngine();
        switch (index) {
            case 0: engine.setMode(static_cast<HarmonyEngine::Mode>(value)); break;
            case 1: engine.setInversion(value); break;
            case 2: engine.setTranspose(value); break;
        }
    }

    virtual const char* parameterValueText(int index, int value) const override {
        switch (index) {
            case 0: return modeText(value);
            case 1: return inversionText(value);
            case 2: return transposeText(value);
            default: return "";
        }
    }

private:
    Model &_model;

    const char* modeText(int mode) const {
        const char* names[] = {"Ionian", "Dorian", "Phrygian", "Lydian",
                               "Mixolydian", "Aeolian", "Locrian"};
        if (mode < 7) return names[mode];
        return "Unknown";
    }

    const char* inversionText(int inv) const {
        const char* names[] = {"Root", "1st"};
        if (inv < 2) return names[inv];
        return "Root"; // Default
    }

    const char* transposeText(int transpose) const {
        static char buf[8];
        snprintf(buf, sizeof(buf), "%+d", transpose);
        return buf;
    }
};
```

**REFACTOR**: Add encoder editing, visual chord display.

---

#### Task 2.5: Mode-Specific Chord Progression Tests (Week 5, Day 4)

**RED**: Test musically correct progressions in each mode

```cpp
TEST(HarmonyEngine, DorianProgression) {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Dorian);

    // i-ii-IV progression in D Dorian
    // D minor 7 (i)
    auto dMin = engine.harmonize(62, 0);
    ASSERT_EQ(engine.getDiatonicQuality(0), HarmonyEngine::ChordQuality::Minor7);

    // E minor 7 (ii)
    auto eMin = engine.harmonize(64, 1);
    ASSERT_EQ(engine.getDiatonicQuality(1), HarmonyEngine::ChordQuality::Minor7);

    // G Major 7 (IV)
    auto gMaj = engine.harmonize(67, 3);
    ASSERT_EQ(engine.getDiatonicQuality(3), HarmonyEngine::ChordQuality::Major7);
}

TEST(HarmonyEngine, PhrygianProgression) {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Phrygian);

    // i-bII progression in E Phrygian (Spanish Phrygian sound)
    // E minor 7 (i)
    auto eMin = engine.harmonize(64, 0);
    ASSERT_EQ(engine.getDiatonicQuality(0), HarmonyEngine::ChordQuality::Minor7);

    // F Major 7 (bII)
    auto fMaj = engine.harmonize(65, 1);
    ASSERT_EQ(engine.getDiatonicQuality(1), HarmonyEngine::ChordQuality::Major7);
}

// Similar tests for Lydian, Mixolydian, Aeolian, Locrian
```

**GREEN**: Verify existing diatonic tables produce correct results.

**REFACTOR**: Add music theory comments explaining modal characteristics.

---

### Phase 2 Success Criteria

✅ **Must Have**:
- [ ] All 7 Ionian modes produce correct diatonic harmonies
- [ ] 1st inversion works correctly for all modes
- [ ] Transpose works across ±2 octave range
- [ ] HarmonyGlobalPage allows mode/inversion/transpose selection
- [ ] Smooth transitions between modes during playback
- [ ] Users can create progressions in any key/mode
- [ ] All Phase 1 functionality still works

✅ **Testing**:
- [ ] 7 modes × 7 degrees = 49 diatonic quality tests passing
- [ ] Inversion tests for all modes
- [ ] Transpose boundary tests

📊 **Metrics**:
- Test count: ~70 unit tests passing
- No regressions from Phase 1

---

## PHASE 3: Voicings, Slew & Per-Step Parameters - 1-2 Weeks

**Goal**: Add Drop2 voicing, configurable slew, per-step voicing/inversion layers.

**Deliverable**: Production-ready lite version with polished UX.

### Scope Additions

✅ **Added in Phase 3**:
- Drop2 voicing (in addition to Close voicing)
- Configurable slew per follower track
- Per-step voicing layer (F3 NOTE button)
- Per-step inversion layer (F3 NOTE button)
- Global voicing on HarmonyGlobalPage
- UI polish and visual feedback

❌ **Still Excluded**:
- Drop3/Spread voicings (Phase 5+)
- Manual chord quality selection (Phase 4)
- CV control (Phase 4)
- Performance mode (Phase 4)

---

### Phase 3 Task Breakdown

---

#### Task 3.1: Drop2 Voicing Implementation (Week 6, Days 1-2)

**RED**: Test Drop2 voicing algorithm

```cpp
TEST(HarmonyEngine, CloseVoicing) {
    HarmonyEngine engine;
    engine.setVoicing(HarmonyEngine::Voicing::Close);

    // C Major 7 in close voicing: All notes within octave
    // C(60)-E(64)-G(67)-B(71)
    auto chord = engine.harmonize(60, 0);

    int16_t span = std::max({chord.root, chord.third, chord.fifth, chord.seventh}) -
                   std::min({chord.root, chord.third, chord.fifth, chord.seventh});
    ASSERT_LE(span, 12); // Within one octave
}

TEST(HarmonyEngine, Drop2Voicing) {
    HarmonyEngine engine;
    engine.setVoicing(HarmonyEngine::Voicing::Drop2);

    // C Major 7 in Drop2: 2nd highest note dropped an octave
    // Standard close: C(60)-E(64)-G(67)-B(71)
    // Drop2: C(60)-G(55)-B(59)-E(64)
    //        root - 5th↓ - 7th↓ - 3rd
    auto chord = engine.harmonize(60, 0);

    // Expected Drop2 voicing from bass up: G(55)-B(59)-C(60)-E(64)
    // (or equivalent octave displacement)

    // Verify wider span than close voicing
    int16_t span = std::max({chord.root, chord.third, chord.fifth, chord.seventh}) -
                   std::min({chord.root, chord.third, chord.fifth, chord.seventh});
    ASSERT_GT(span, 12); // Wider than one octave
}
```

**GREEN**: Implement Drop2 voicing

```cpp
// In HarmonyEngine.cpp
void HarmonyEngine::applyVoicing(ChordNotes &chord) const {
    if (_voicing == Voicing::Close) return; // Default, no change

    if (_voicing == Voicing::Drop2) {
        // Drop2: Take 2nd-highest note and drop it an octave
        // Standard close voicing from low to high: root < third < fifth < seventh
        // Drop2: Move 2nd-highest (fifth in this case) down an octave

        // Assuming close voicing input: root(60)-third(64)-fifth(67)-seventh(71)
        // Drop the fifth down: fifth(67) -> fifth(55)
        chord.fifth -= 12;

        // Result: root(60), fifth(55), seventh(71), third(64)
        // Actual bass-up order: fifth(55) - seventh(59... wait, needs adjustment

        // BETTER: Sort notes, drop 2nd highest
        std::array<int16_t*, 4> notes = {&chord.root, &chord.third, &chord.fifth, &chord.seventh};
        std::sort(notes.begin(), notes.end(), [](int16_t* a, int16_t* b) { return *a < *b; });

        // Drop 2nd-highest (index 2 in sorted array)
        *notes[2] -= 12;
    }

    // Phase 5+: Drop3, Spread
}
```

**REFACTOR**: Optimize voicing algorithm, add more voicings in Phase 5+.

---

#### Task 3.2: Slew Implementation (Week 6, Days 2-3)

**RED**: Test slew smooths pitch transitions

```cpp
TEST(HarmonyTrackEngine, DISABLED_SlewBasic) {
    Engine engine;
    Model model;
    Track masterTrack, followerTrack;

    HarmonySequence masterSeq(0);
    masterSeq.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    masterSeq.step(0).setNote(60); // C
    masterSeq.step(1).setNote(67); // G (large jump)

    HarmonySequence followerSeq(1);
    followerSeq.setHarmonyRole(HarmonyEngine::HarmonyRole::Follower3rd);
    followerSeq.setSlewAmount(64); // Medium slew

    HarmonyTrackEngine masterEngine(engine, model, masterTrack, 0);
    HarmonyTrackEngine followerEngine(engine, model, followerTrack, 1);
    masterEngine.setHarmonySequence(&masterSeq);
    followerEngine.setHarmonySequence(&followerSeq);
    followerEngine.setMasterEngine(&masterEngine);

    // Step 0: E (3rd of C)
    masterEngine.tick(48);
    followerEngine.tick(48);
    float cv0 = followerEngine.currentCV(); // E (64)

    // Step 1: B (3rd of G) - but slew should smooth the transition
    masterEngine.tick(96);
    followerEngine.tick(96);
    float cv1_immediate = followerEngine.currentCV();

    // CV shouldn't jump instantly to B (71), should be between E (64) and B (71)
    float targetCV = noteToCV(71);
    ASSERT_LT(cv1_immediate, targetCV); // Not yet at target
    ASSERT_GT(cv1_immediate, cv0);      // Moving towards target
}
```

**GREEN**: Implement slew in HarmonyTrackEngine

```cpp
// In HarmonyTrackEngine.h
private:
    float _currentCV;  // Current CV output (with slew applied)
    float _targetCV;   // Target CV (harmonized note)
    uint32_t _lastSlewTick;
```

```cpp
// In HarmonyTrackEngine.cpp
void HarmonyTrackEngine::tick(uint32_t tick) {
    // ... existing master/follower logic ...

    if (isFollower()) {
        // Get harmonized note (existing code)
        // ...

        // Convert note to CV
        _targetCV = noteToCV(_currentNote);

        // Apply slew
        applySlewToCV(tick);

        // Output slewed CV
        setCV(_currentCV);
    }
}

void HarmonyTrackEngine::applySlewToCV(uint32_t tick) {
    if (!_sequence) return;

    uint8_t slewAmount = _sequence->slewAmount();
    if (slewAmount == 0) {
        // No slew - instant transition
        _currentCV = _targetCV;
        return;
    }

    // Calculate slew rate based on slewAmount (0-127)
    float slewRate = slewAmount / 127.0f * 0.1f; // Tuning constant

    // Smooth transition towards target
    float delta = _targetCV - _currentCV;
    _currentCV += delta * slewRate;

    // Clamp when very close
    if (std::abs(delta) < 0.001f) {
        _currentCV = _targetCV;
    }
}

float HarmonyTrackEngine::noteToCV(int16_t midiNote) const {
    // Convert MIDI note to CV (1V/octave, C4=0V)
    return (midiNote - 60) / 12.0f;
}
```

**REFACTOR**: Tune slew algorithm for musical feel.

---

#### Task 3.3: Per-Step Voicing/Inversion Layers (Week 6, Days 4-5)

**RED**: Test per-step parameter storage

```cpp
TEST(NoteSequence, PerStepVoicingStorage) {
    NoteSequence sequence;

    // Set per-step voicing (stored in _data1 bitfield)
    sequence.step(0).setVoicing(HarmonyEngine::Voicing::Drop2);
    sequence.step(0).setHasLocalVoicing(true);

    ASSERT_TRUE(sequence.step(0).hasLocalVoicing());
    ASSERT_EQ(sequence.step(0).voicing(), HarmonyEngine::Voicing::Drop2);

    // Steps without local voicing use global default
    ASSERT_FALSE(sequence.step(1).hasLocalVoicing());
}

TEST(NoteSequence, PerStepInversionStorage) {
    NoteSequence sequence;

    sequence.step(0).setInversion(1); // 1st inversion
    sequence.step(0).setHasLocalInversion(true);

    ASSERT_TRUE(sequence.step(0).hasLocalInversion());
    ASSERT_EQ(sequence.step(0).inversion(), 1);
}

TEST(NoteSequence, GlobalAndLocalParameterInteraction) {
    NoteSequence sequence;
    HarmonyEngine engine;
    engine.setGlobalVoicing(HarmonyEngine::Voicing::Close);
    engine.setGlobalInversion(0);

    // Step without local override uses global
    ASSERT_EQ(sequence.step(0).effectiveVoicing(engine), HarmonyEngine::Voicing::Close);
    ASSERT_EQ(sequence.step(0).effectiveInversion(engine), 0);

    // Step with local override uses local
    sequence.step(1).setVoicing(HarmonyEngine::Voicing::Drop2);
    sequence.step(1).setHasLocalVoicing(true);
    ASSERT_EQ(sequence.step(1).effectiveVoicing(engine), HarmonyEngine::Voicing::Drop2);
}
```

**GREEN**: Extend NoteSequence::Step with harmony parameters

```cpp
// In NoteSequence.h
class Step {
public:
    // ... existing step data ...

    // Harmony parameters (Phase 3)
    void setVoicing(HarmonyEngine::Voicing v) {
        _data1.harmonyVoicing = static_cast<uint8_t>(v);
    }

    HarmonyEngine::Voicing voicing() const {
        return static_cast<HarmonyEngine::Voicing>(_data1.harmonyVoicing);
    }

    void setHasLocalVoicing(bool has) {
        _data1.hasLocalVoicing = has;
    }

    bool hasLocalVoicing() const {
        return _data1.hasLocalVoicing;
    }

    void setInversion(uint8_t inv) {
        _data1.harmonyInversion = inv & 0x3; // 2 bits, 0-3
    }

    uint8_t inversion() const {
        return _data1.harmonyInversion;
    }

    void setHasLocalInversion(bool has) {
        _data1.hasLocalInversion = has;
    }

    bool hasLocalInversion() const {
        return _data1.hasLocalInversion;
    }

    // Effective values (local if set, else global)
    HarmonyEngine::Voicing effectiveVoicing(const HarmonyEngine &engine) const {
        return hasLocalVoicing() ? voicing() : engine.globalVoicing();
    }

    uint8_t effectiveInversion(const HarmonyEngine &engine) const {
        return hasLocalInversion() ? inversion() : engine.globalInversion();
    }

private:
    union Data1 {
        struct {
            // ... existing fields ...
            uint32_t hasLocalVoicing : 1;
            uint32_t harmonyVoicing : 2;  // 0-3 (4 voicings)
            uint32_t hasLocalInversion : 1;
            uint32_t harmonyInversion : 2; // 0-3 (4 inversions)
            // Remaining bits for future use
        };
        uint32_t raw;
    } _data1;
};
```

**REFACTOR**: Verify bitfield packing doesn't overflow.

---

#### Task 3.4: F3 NOTE Button Layer Cycling (Week 7, Day 1)

**RED**: Test layer cycling includes harmony layers

```cpp
TEST(NoteSequenceEditPage, HarmonyLayerCycling) {
    // Test F3 NOTE button cycles through:
    // Note → NoteVariationRange → NoteVariationProbability →
    // HarmonyVoicing → HarmonyInversion → Note (wrap)

    NoteSequenceEditPage page;
    page.setLayer(NoteSequence::Layer::Note);

    page.cycleLayerForward(); // → NoteVariationRange
    ASSERT_EQ(page.layer(), NoteSequence::Layer::NoteVariationRange);

    page.cycleLayerForward(); // → NoteVariationProbability
    ASSERT_EQ(page.layer(), NoteSequence::Layer::NoteVariationProbability);

    page.cycleLayerForward(); // → HarmonyVoicing (NEW)
    ASSERT_EQ(page.layer(), NoteSequence::Layer::HarmonyVoicing);

    page.cycleLayerForward(); // → HarmonyInversion (NEW)
    ASSERT_EQ(page.layer(), NoteSequence::Layer::HarmonyInversion);

    page.cycleLayerForward(); // → Note (wrap)
    ASSERT_EQ(page.layer(), NoteSequence::Layer::Note);

    // Test reverse cycling
    page.cycleLayerBackward(); // → HarmonyInversion
    ASSERT_EQ(page.layer(), NoteSequence::Layer::HarmonyInversion);
}
```

**GREEN**: Add harmony layers to NoteSequence::Layer enum

```cpp
// In NoteSequence.h
enum class Layer {
    Gate,
    GateProbability,
    GateOffset,
    Slide,
    Retrigger,
    RetriggerProbability,
    PulseCount,        // Existing pulse count layer
    GateMode,          // Existing gate mode layer
    Length,
    LengthVariationRange,
    LengthVariationProbability,
    Note,
    NoteVariationRange,
    NoteVariationProbability,

    // NEW Phase 3: Harmony layers
    HarmonyVoicing,
    HarmonyInversion,

    // Existing
    Condition,
    AccumulatorTrigger,
    Last
};
```

**Modify NoteSequenceEditPage.cpp**:

```cpp
void NoteSequenceEditPage::functionShortPress(Function function) {
    if (function == Function::Note) {
        // Cycle through Note-related layers
        switch (_layer) {
            case Layer::Note:
                setLayer(Layer::NoteVariationRange);
                break;
            case Layer::NoteVariationRange:
                setLayer(Layer::NoteVariationProbability);
                break;
            case Layer::NoteVariationProbability:
                setLayer(Layer::HarmonyVoicing); // NEW
                break;
            case Layer::HarmonyVoicing:
                setLayer(Layer::HarmonyInversion); // NEW
                break;
            case Layer::HarmonyInversion:
                setLayer(Layer::Note); // Wrap back
                break;
            default:
                setLayer(Layer::Note);
        }
    }
}
```

**REFACTOR**: Update layer display strings, help text.

---

### Phase 3 Success Criteria

✅ **Must Have**:
- [ ] Drop2 voicing produces audibly different results from Close
- [ ] Slew smooths pitch transitions between steps
- [ ] Per-step voicing/inversion layers accessible via F3 NOTE button
- [ ] UI is intuitive and responsive
- [ ] Performance acceptable with 8 active harmony tracks
- [ ] Production-ready quality (no crashes, clean UI)

✅ **Testing**:
- [ ] Voicing tests for Close and Drop2
- [ ] Slew tests verify smooth transitions
- [ ] Layer cycling tests pass

📊 **Metrics**:
- All Phase 1 + Phase 2 tests still passing
- UI response time <100ms for parameter changes
- Audio glitch-free with 4 master + 12 follower tracks (stress test)

---

## PHASE 4: Advanced Features - 2-3 Weeks (Optional)

**Goal**: Add manual chord quality selection, CV control, performance mode.

**Scope**: Full-featured harmony system (except Harmonic Minor modes).

✅ **Added in Phase 4**:
- Manual chord quality selection (8 qualities)
- Diatonic vs Manual mode toggle
- CV input mapping (inversion, voicing, transpose)
- Performance mode (button keyboard)
- Unison mode

[Details omitted for brevity - expand based on Phase 1-3 patterns]

---

## PHASE 5+: Complete Feature Set (Future)

**Scope**: Harmonic Minor modes, all inversions/voicings, advanced features.

✅ **Added in Phase 5+**:
- 7 Harmonic Minor modes
- 2nd and 3rd inversions
- Drop3 and Spread voicings
- Additional chord qualities
- Voice leading optimization
- Arpeggiator integration

[Reserve for future expansion based on user feedback]

---

## Testing Strategy Summary

### Automated Testing

**Unit Tests** (Phase 1-3):
- `TestHarmonyEngine.cpp`: ~50 tests (scales, chords, inversions, voicings)
- `TestHarmonySequence.cpp`: ~20 tests (track constraints, scale override, serialization)
- `TestHarmonyTrackEngine.cpp`: ~15 tests (DISABLED, for integration)
- `TestHarmonyUI.cpp`: ~10 tests (page navigation, parameter ranges)

**Parametrized Tests**:
```cpp
// Automatically test all mode × quality combinations
INSTANTIATE_TEST_SUITE_P(AllCombinations, HarmonyTest,
    ::testing::Combine(
        ::testing::ValuesIn(allModes),     // 7 in Phase 2
        ::testing::ValuesIn(allQualities), // 4 in Phase 1, 8 in Phase 4
        ::testing::Range(0, 2),            // 2 inversions in Phase 2
        ::testing::ValuesIn(allVoicings)   // 2 in Phase 3
    )
);
// Phase 2: 7 × 4 × 2 × 1 = 56 tests
// Phase 3: 7 × 4 × 2 × 2 = 112 tests
// Phase 4: 7 × 8 × 2 × 2 = 224 tests
```

### Integration Testing

**Simulator Tests** (Manual):
- Basic chord progression (C-F-G-Am)
- Mode switching during playback
- Master/follower synchronization
- Pattern changes
- Project save/load
- Long-duration stability (5 min continuous play)

### Performance Benchmarking

```cpp
TEST(HarmonyPerformance, RealTimeConstraints) {
    HarmonyEngine engine;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 10000; ++i) {
        engine.harmonize(60 + (i % 12), i % 7);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto avgMicros = duration.count() / 10000;

    // Must complete in <100μs per operation
    ASSERT_LT(avgMicros, 100);
}
```

---

## Risk Mitigation

### Technical Risks

| Risk | Phase | Mitigation | Status |
|------|-------|------------|--------|
| Track position constraint violations | 1 | Enforce in setHarmonyRole(), UI validation | ✅ Architected |
| Master/follower timing race | 1 | 3-phase tick ordering in Engine::tick() | ✅ Specified |
| Scale override conflicts | 1 | Per-HarmonyRoot scale storage | ✅ Designed |
| Memory overflow from bitfields | 3 | Static assertions, boundary tests | ⏳ Pending |
| Performance degradation | All | Profile early, benchmark continuously | ⏳ Pending |

### Project Risks

| Risk | Mitigation |
|------|------------|
| Scope creep | Strict phase gates, no mid-phase additions |
| Music theory errors | Parametrized testing, reference against Harmonàig manual |
| User confusion | Clear UI labels, example projects, documentation |
| Integration bugs | Daily builds, continuous integration, simulator testing |

---

## Success Metrics

### Phase 1 Gate (MUST PASS to proceed):
- [ ] All 28 unit tests passing
- [ ] C-F-G-Am progression plays correctly in simulator
- [ ] No crashes during 5-minute playback
- [ ] Actual effort within 50% of estimate (2-3 weeks → 1.5-4.5 weeks)

### Phase 2 Gate:
- [ ] All 7 modes produce correct diatonic chords
- [ ] ~70 unit tests passing
- [ ] Modal progressions sound musically correct

### Phase 3 Gate (Production-Ready):
- [ ] ~100 unit tests passing
- [ ] UI polish complete
- [ ] Performance <10% CPU overhead
- [ ] User acceptance testing positive

---

## Documentation Requirements

### Code Documentation:
- [ ] All public API methods have Doxygen comments
- [ ] Music theory formulas documented in lookup tables
- [ ] Architecture decision records (ADRs) for key choices

### User Documentation:
- [ ] CLAUDE.md updated with harmony feature section
- [ ] Example projects demonstrating each mode
- [ ] Quick start guide (5-minute tutorial)

### Testing Documentation:
- [ ] Test coverage report (>90% for HarmonyEngine)
- [ ] Known limitations documented
- [ ] Regression test suite established

---

## Timeline Summary

| Phase | Duration | Cumulative | Deliverable |
|-------|----------|------------|-------------|
| 1 MVP | 2-3 weeks | 2-3 weeks | Basic harmony working |
| 2 Modes | 2-3 weeks | 4-6 weeks | All modes + inversions |
| 3 Polish | 1-2 weeks | 5-8 weeks | Production-ready |
| **GATE** | 1 week buffer | **6-9 weeks** | **Lite version complete** |
| 4 Advanced | 2-3 weeks | 8-12 weeks | Full-featured (optional) |
| 5+ Future | TBD | TBD | Complete feature parity (optional) |

---

## Appendix A: File Structure

```
src/apps/sequencer/
├── model/
│   ├── HarmonyEngine.h/cpp         (Phase 1)
│   ├── HarmonySequence.h/cpp       (Phase 1)
│   └── Project.h                   (add HarmonyEngine member)
├── engine/
│   ├── HarmonyTrackEngine.h/cpp    (Phase 1)
│   └── Engine.cpp                  (modify tick ordering)
└── ui/pages/
    ├── HarmonyGlobalPage.h/cpp     (Phase 2)
    ├── HarmonyTrackConfigPage.h/cpp (Phase 1)
    ├── NoteSequenceEditPage.cpp    (modify for harmony layers, Phase 3)
    └── TopPage.cpp                 (add navigation)

src/tests/unit/sequencer/
├── TestHarmonyEngine.cpp           (Phase 1-3)
├── TestHarmonySequence.cpp         (Phase 1)
├── TestHarmonyTrackEngine.cpp      (Phase 1, DISABLED)
└── TestHarmonyUI.cpp               (Phase 2-3)
```

---

## Appendix B: Bitfield Layout

```cpp
// NoteSequence::Step::Data1 bitfield allocation
union Data1 {
    struct {
        // Existing fields (from prior features):
        uint32_t gate : 1;               // bit 0
        uint32_t gateProbability : 7;    // bits 1-7
        uint32_t gateOffset : 5;         // bits 8-12 (signed, ±15 ticks)
        uint32_t slide : 1;              // bit 13
        uint32_t retrigger : 4;          // bits 14-17 (0-15)
        uint32_t pulseCount : 3;         // bits 18-20 (0-7)
        uint32_t gateMode : 2;           // bits 21-22 (0-3)

        // NEW Phase 3: Harmony fields
        uint32_t hasLocalVoicing : 1;    // bit 23
        uint32_t harmonyVoicing : 2;     // bits 24-25 (0-3)
        uint32_t hasLocalInversion : 1;  // bit 26
        uint32_t harmonyInversion : 2;   // bits 27-28 (0-3)

        // REMAINING: bits 29-31 (3 bits free for future)
    };
    uint32_t raw;
};

static_assert(sizeof(Data1) == 4, "Data1 must be 32 bits");
```

---

**END OF WORKING-TDD-HARMONY-PLAN.md**

This plan is READY FOR IMPLEMENTATION. Proceed with Phase 1, Task 1.1.