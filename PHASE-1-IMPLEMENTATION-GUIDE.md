# PHASE 1 IMPLEMENTATION GUIDE - Exact Steps

**Goal**: Minimal Viable Harmony (Ionian Mode Only)
**Duration**: 2-3 weeks
**Status**: Ready for implementation after architectural compliance fixes

---

## PREREQUISITES: Architectural Compliance Fixes

**BEFORE starting Phase 1, apply these critical fixes to WORKING-TDD-HARMONY-PLAN.md:**

### Fix 1: Enum Style (CRITICAL)
- ❌ Remove: `enum class Mode`
- ✅ Use: `enum Mode` (no `class` keyword)
- ❌ Remove: `Last` member from model enums
- ✅ Keep: `Last` only in `NoteSequence::Layer` enum

### Fix 2: Don't Create Separate HarmonySequence Class (CRITICAL)
- ❌ Remove: `class HarmonySequence : public NoteSequence`
- ✅ Instead: Extend `NoteSequence` directly with new properties

### Fix 3: Harmony TrackEngine Extends NoteTrackEngine (CRITICAL)
- ❌ Remove: `class HarmonyTrackEngine : public TrackEngine`
- ✅ Use: `class HarmonyTrackEngine : public NoteTrackEngine`

### Fix 4: Constructor Signature (CRITICAL)
- ❌ Remove: `HarmonyTrackEngine(Engine &engine, Model &model, ...)`
- ✅ Use: `HarmonyTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine)`

---

## PHASE 1 EXACT STEPS

### WEEK 1: Core Harmony Engine

---

#### **DAY 1: Setup and Basic HarmonyEngine**

**Step 1.1: Create HarmonyEngine Files**

```bash
cd /home/user/performer-phazer
touch src/apps/sequencer/model/HarmonyEngine.h
touch src/apps/sequencer/model/HarmonyEngine.cpp
touch src/tests/unit/sequencer/TestHarmonyEngine.cpp
```

**Step 1.2: Write First Test (RED)**

File: `src/tests/unit/sequencer/TestHarmonyEngine.cpp`

```cpp
#include "catch.hpp"
#include "model/HarmonyEngine.h"

TEST_CASE("HarmonyEngine Default Construction", "[harmony]") {
    HarmonyEngine engine;

    REQUIRE(engine.mode() == HarmonyEngine::Ionian);
    REQUIRE(engine.diatonicMode() == true);
    REQUIRE(engine.inversion() == 0);
    REQUIRE(engine.voicing() == HarmonyEngine::Close);
    REQUIRE(engine.transpose() == 0);
}
```

**Step 1.3: Implement HarmonyEngine Header (GREEN)**

File: `src/apps/sequencer/model/HarmonyEngine.h`

```cpp
#pragma once

#include "core/math/Math.h"
#include <cstdint>
#include <algorithm>

class VersionedSerializedWriter;
class VersionedSerializedReader;

class HarmonyEngine {
public:
    // FIXED: Plain enum (no "class"), no Last member
    enum Mode {
        Ionian = 0,
        Dorian = 1,
        Phrygian = 2,
        Lydian = 3,
        Mixolydian = 4,
        Aeolian = 5,
        Locrian = 6
    };

    enum ChordQuality {
        Minor7 = 0,      // -7
        Dominant7 = 1,   // 7
        Major7 = 2,      // ∆7
        HalfDim7 = 3     // ø
    };

    enum Voicing {
        Close = 0,
        Drop2 = 1,
        Drop3 = 2,
        Spread = 3
    };

    struct ChordNotes {
        int16_t root;
        int16_t third;
        int16_t fifth;
        int16_t seventh;
    };

    // Constructor
    HarmonyEngine();

    // Getters (const!)
    Mode mode() const { return _mode; }
    bool diatonicMode() const { return _diatonicMode; }
    uint8_t inversion() const { return _inversion; }
    Voicing voicing() const { return _voicing; }
    int8_t transpose() const { return _transpose; }

    // Setters
    void setMode(Mode mode) { _mode = mode; }
    void setDiatonicMode(bool diatonic) { _diatonicMode = diatonic; }
    void setInversion(uint8_t inv) { _inversion = std::min(inv, uint8_t(3)); }
    void setVoicing(Voicing v) { _voicing = v; }
    void setTranspose(int8_t t) { _transpose = clamp(t, int8_t(-24), int8_t(24)); }

    // Core harmony functions (Phase 1)
    int16_t getScaleInterval(uint8_t degree) const;
    ChordQuality getDiatonicQuality(uint8_t scaleDegree) const;
    ChordNotes harmonize(int16_t rootNote, uint8_t scaleDegree) const;

    // Serialization
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    Mode _mode;
    bool _diatonicMode;
    uint8_t _inversion;      // 0-3
    Voicing _voicing;
    int8_t _transpose;       // -24 to +24

    // Lookup tables (Phase 1: Ionian only)
    static const uint8_t ScaleIntervals[7][7];
    static const ChordQuality DiatonicChords[7][7];
    static const uint8_t ChordIntervals[4][4];

    // Helper methods
    int16_t applyInterval(int16_t baseNote, int16_t interval) const;
};
```

**Step 1.4: Implement HarmonyEngine Constructor (GREEN)**

File: `src/apps/sequencer/model/HarmonyEngine.cpp`

```cpp
#include "HarmonyEngine.h"
#include "Serialize.h"

HarmonyEngine::HarmonyEngine()
    : _mode(Ionian)
    , _diatonicMode(true)
    , _inversion(0)
    , _voicing(Close)
    , _transpose(0)
{}
```

**Step 1.5: Run Test**

```bash
cd /home/user/performer-phazer/build/sim/debug
make -j TestHarmonyEngine
./src/tests/unit/sequencer/TestHarmonyEngine
```

**Expected**: Test passes ✅

**Step 1.6: Refactor**

- Add Doxygen comments to public methods
- Verify includes are minimal
- Check const correctness

---

#### **DAY 2: Scale Intervals and Lookup Tables**

**Step 2.1: Write Scale Interval Test (RED)**

Add to `TestHarmonyEngine.cpp`:

```cpp
TEST_CASE("HarmonyEngine Ionian Scale Intervals", "[harmony]") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // Ionian intervals: W-W-H-W-W-W-H = 0-2-4-5-7-9-11
    const int16_t expected[7] = {0, 2, 4, 5, 7, 9, 11};

    for (int degree = 0; degree < 7; degree++) {
        REQUIRE(engine.getScaleInterval(degree) == expected[degree]);
    }
}
```

**Step 2.2: Implement Scale Intervals (GREEN)**

Add to `HarmonyEngine.cpp`:

```cpp
// Scale interval table (Phase 1: Ionian only, others for Phase 2)
const uint8_t HarmonyEngine::ScaleIntervals[7][7] = {
    {0, 2, 4, 5, 7, 9, 11},  // Ionian: W-W-H-W-W-W-H
    {0, 2, 3, 5, 7, 9, 10},  // Dorian (Phase 2)
    {0, 1, 3, 5, 7, 8, 10},  // Phrygian (Phase 2)
    {0, 2, 4, 6, 7, 9, 11},  // Lydian (Phase 2)
    {0, 2, 4, 5, 7, 9, 10},  // Mixolydian (Phase 2)
    {0, 2, 3, 5, 7, 8, 10},  // Aeolian (Phase 2)
    {0, 1, 3, 5, 6, 8, 10}   // Locrian (Phase 2)
};

int16_t HarmonyEngine::getScaleInterval(uint8_t degree) const {
    if (degree >= 7) degree %= 7;
    return ScaleIntervals[_mode][degree];
}
```

**Step 2.3: Run Test** → Should pass ✅

---

#### **DAY 3: Diatonic Chord Quality Lookup**

**Step 3.1: Write Diatonic Quality Test (RED)**

```cpp
TEST_CASE("HarmonyEngine Ionian Diatonic Chord Qualities", "[harmony]") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);
    engine.setDiatonicMode(true);

    // Ionian: I∆7, ii-7, iii-7, IV∆7, V7, vi-7, viiø
    REQUIRE(engine.getDiatonicQuality(0) == HarmonyEngine::Major7);   // I
    REQUIRE(engine.getDiatonicQuality(1) == HarmonyEngine::Minor7);   // ii
    REQUIRE(engine.getDiatonicQuality(2) == HarmonyEngine::Minor7);   // iii
    REQUIRE(engine.getDiatonicQuality(3) == HarmonyEngine::Major7);   // IV
    REQUIRE(engine.getDiatonicQuality(4) == HarmonyEngine::Dominant7); // V
    REQUIRE(engine.getDiatonicQuality(5) == HarmonyEngine::Minor7);   // vi
    REQUIRE(engine.getDiatonicQuality(6) == HarmonyEngine::HalfDim7); // vii
}
```

**Step 3.2: Implement Diatonic Lookup (GREEN)**

Add to `HarmonyEngine.cpp`:

```cpp
const HarmonyEngine::ChordQuality HarmonyEngine::DiatonicChords[7][7] = {
    // Ionian: I∆7, ii-7, iii-7, IV∆7, V7, vi-7, viiø
    {Major7, Minor7, Minor7, Major7, Dominant7, Minor7, HalfDim7},

    // Dorian (Phase 2)
    {Minor7, Minor7, Major7, Dominant7, Minor7, HalfDim7, Major7},

    // Phrygian (Phase 2)
    {Minor7, Major7, Dominant7, Minor7, HalfDim7, Major7, Minor7},

    // Lydian (Phase 2)
    {Major7, Dominant7, Minor7, HalfDim7, Major7, Minor7, Minor7},

    // Mixolydian (Phase 2)
    {Dominant7, Minor7, HalfDim7, Major7, Minor7, Minor7, Major7},

    // Aeolian (Phase 2)
    {Minor7, HalfDim7, Major7, Minor7, Minor7, Major7, Dominant7},

    // Locrian (Phase 2)
    {HalfDim7, Major7, Minor7, Minor7, Major7, Dominant7, Minor7}
};

HarmonyEngine::ChordQuality HarmonyEngine::getDiatonicQuality(uint8_t scaleDegree) const {
    if (scaleDegree >= 7) scaleDegree %= 7;
    return DiatonicChords[_mode][scaleDegree];
}
```

**Step 3.3: Run Test** → Should pass ✅

---

#### **DAY 4: Chord Intervals and Basic Harmonization**

**Step 4.1: Write Chord Interval Test (RED)**

```cpp
TEST_CASE("HarmonyEngine Chord Intervals", "[harmony]") {
    HarmonyEngine engine;

    // Major7: 0-4-7-11
    auto maj7Intervals = engine.getChordIntervals(HarmonyEngine::Major7);
    REQUIRE(maj7Intervals[0] == 0);
    REQUIRE(maj7Intervals[1] == 4);
    REQUIRE(maj7Intervals[2] == 7);
    REQUIRE(maj7Intervals[3] == 11);

    // Minor7: 0-3-7-10
    auto min7Intervals = engine.getChordIntervals(HarmonyEngine::Minor7);
    REQUIRE(min7Intervals[0] == 0);
    REQUIRE(min7Intervals[1] == 3);
    REQUIRE(min7Intervals[2] == 7);
    REQUIRE(min7Intervals[3] == 10);
}
```

**Step 4.2: Implement Chord Intervals (GREEN)**

Add to `HarmonyEngine.h`:

```cpp
struct ChordIntervals {
    uint8_t intervals[4];
    const uint8_t& operator[](int i) const { return intervals[i]; }
};

ChordIntervals getChordIntervals(ChordQuality quality) const;
```

Add to `HarmonyEngine.cpp`:

```cpp
const uint8_t HarmonyEngine::ChordIntervals[4][4] = {
    {0, 3, 7, 10},  // Minor7
    {0, 4, 7, 10},  // Dominant7
    {0, 4, 7, 11},  // Major7
    {0, 3, 6, 10}   // HalfDim7
};

HarmonyEngine::ChordIntervals HarmonyEngine::getChordIntervals(ChordQuality quality) const {
    ChordIntervals result;
    for (int i = 0; i < 4; i++) {
        result.intervals[i] = ChordIntervals[quality][i];
    }
    return result;
}
```

**Step 4.3: Write Basic Harmonization Test (RED)**

```cpp
TEST_CASE("HarmonyEngine Basic Harmonization", "[harmony]") {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Ionian);

    // C Major 7 (I in C): C-E-G-B = 60-64-67-71
    auto cChord = engine.harmonize(60, 0);
    REQUIRE(cChord.root == 60);
    REQUIRE(cChord.third == 64);
    REQUIRE(cChord.fifth == 67);
    REQUIRE(cChord.seventh == 71);

    // D minor 7 (ii in C): D-F-A-C = 62-65-69-72
    auto dChord = engine.harmonize(62, 1);
    REQUIRE(dChord.root == 62);
    REQUIRE(dChord.third == 65);
    REQUIRE(dChord.fifth == 69);
    REQUIRE(dChord.seventh == 72);
}
```

**Step 4.4: Implement Harmonize Method (GREEN)**

Add to `HarmonyEngine.cpp`:

```cpp
HarmonyEngine::ChordNotes HarmonyEngine::harmonize(int16_t rootNote, uint8_t scaleDegree) const {
    ChordNotes chord;

    // Get chord quality for this scale degree (diatonic)
    ChordQuality quality = getDiatonicQuality(scaleDegree);

    // Get chord intervals
    auto intervals = getChordIntervals(quality);

    // Apply intervals to root note
    chord.root = applyInterval(rootNote, intervals[0]);
    chord.third = applyInterval(rootNote, intervals[1]);
    chord.fifth = applyInterval(rootNote, intervals[2]);
    chord.seventh = applyInterval(rootNote, intervals[3]);

    return chord;
}

int16_t HarmonyEngine::applyInterval(int16_t baseNote, int16_t interval) const {
    int16_t result = baseNote + interval;
    return clamp(result, int16_t(0), int16_t(127)); // MIDI range
}
```

**Step 4.5: Run Tests** → All should pass ✅

---

### WEEK 2: Data Model Integration

---

#### **DAY 5-6: Extend NoteSequence with Harmony Properties**

**Step 5.1: Add Harmony Enums to NoteSequence**

File: `src/apps/sequencer/model/NoteSequence.h`

Add after existing enums (around line 40):

```cpp
// Harmony role (Phase 1)
enum HarmonyRole {
    HarmonyIndependent = 0,
    HarmonyMaster = 1,
    HarmonyFollower3rd = 2,
    HarmonyFollower5th = 3,
    HarmonyFollower7th = 4
};
```

**Step 5.2: Add Harmony Properties to NoteSequence**

Add to NoteSequence class (private section, around line 450):

```cpp
private:
    // ... existing members ...

    // Harmony properties (Phase 1)
    HarmonyRole _harmonyRole;
    int8_t _masterTrackIndex;  // -1 if not follower
    uint8_t _harmonyScale;     // HarmonyEngine::Mode for master tracks
```

**Step 5.3: Add Harmony Getters/Setters**

Add to NoteSequence public section:

```cpp
public:
    // Harmony (Phase 1)
    HarmonyRole harmonyRole() const { return _harmonyRole; }
    void setHarmonyRole(HarmonyRole role);

    int8_t masterTrackIndex() const { return _masterTrackIndex; }
    void setMasterTrackIndex(int8_t index) { _masterTrackIndex = index; }

    uint8_t harmonyScale() const { return _harmonyScale; }
    void setHarmonyScale(uint8_t mode) { _harmonyScale = mode; }

    bool canBeHarmonyMaster() const {
        return (_trackIndex == 0 || _trackIndex == 4); // T1 or T5 only
    }
```

**Step 5.4: Implement setHarmonyRole with Track Constraint**

File: `src/apps/sequencer/model/NoteSequence.cpp`

```cpp
void NoteSequence::setHarmonyRole(HarmonyRole role) {
    if (role == HarmonyMaster && !canBeHarmonyMaster()) {
        // Auto-assign appropriate follower role based on track position
        if (_trackIndex == 1) {
            _harmonyRole = HarmonyFollower3rd;
        } else if (_trackIndex == 2) {
            _harmonyRole = HarmonyFollower5th;
        } else if (_trackIndex == 3) {
            _harmonyRole = HarmonyFollower7th;
        } else if (_trackIndex == 5) {
            _harmonyRole = HarmonyFollower3rd;
        } else if (_trackIndex == 6) {
            _harmonyRole = HarmonyFollower5th;
        } else if (_trackIndex == 7) {
            _harmonyRole = HarmonyFollower7th;
        } else {
            _harmonyRole = HarmonyIndependent;
        }
        return;
    }
    _harmonyRole = role;
}
```

**Step 5.5: Update Constructor**

In NoteSequence constructor:

```cpp
NoteSequence::NoteSequence()
    // ... existing initialization ...
    , _harmonyRole(HarmonyIndependent)
    , _masterTrackIndex(-1)
    , _harmonyScale(0) // Ionian
{}
```

**Step 5.6: Update Serialization**

In `NoteSequence::write()`:

```cpp
void NoteSequence::write(VersionedSerializedWriter &writer) const {
    // ... existing writes ...

    // Harmony (Phase 1) - pack into single byte
    uint8_t harmonyFlags = (static_cast<uint8_t>(_harmonyRole) << 0) | // 3 bits
                           (static_cast<uint8_t>(_harmonyScale) << 3);  // 3 bits
    writer.write(harmonyFlags);
    writer.write(_masterTrackIndex);
}
```

In `NoteSequence::read()`:

```cpp
void NoteSequence::read(VersionedSerializedReader &reader) {
    // ... existing reads ...

    uint8_t harmonyFlags;
    reader.read(harmonyFlags);
    _harmonyRole = static_cast<HarmonyRole>((harmonyFlags >> 0) & 0x7);
    _harmonyScale = (harmonyFlags >> 3) & 0x7;
    reader.read(_masterTrackIndex);
}
```

**Step 5.7: Write Tests**

File: `src/tests/unit/sequencer/TestNoteSequence.cpp`

```cpp
TEST_CASE("NoteSequence Harmony Track Position Constraints", "[notesequence]") {
    NoteSequence seq1(0); // Track 1
    REQUIRE(seq1.canBeHarmonyMaster() == true);

    seq1.setHarmonyRole(NoteSequence::HarmonyMaster);
    REQUIRE(seq1.harmonyRole() == NoteSequence::HarmonyMaster);

    NoteSequence seq2(1); // Track 2
    REQUIRE(seq2.canBeHarmonyMaster() == false);

    seq2.setHarmonyRole(NoteSequence::HarmonyMaster);
    REQUIRE(seq2.harmonyRole() != NoteSequence::HarmonyMaster); // Auto-reverted
    REQUIRE(seq2.harmonyRole() == NoteSequence::HarmonyFollower3rd); // Auto-assigned
}
```

---

#### **DAY 7: HarmonyTrackEngine Foundation**

**Step 7.1: Create HarmonyTrackEngine Files**

```bash
touch src/apps/sequencer/engine/HarmonyTrackEngine.h
touch src/apps/sequencer/engine/HarmonyTrackEngine.cpp
```

**Step 7.2: Implement HarmonyTrackEngine Header**

File: `src/apps/sequencer/engine/HarmonyTrackEngine.h`

```cpp
#pragma once

#include "NoteTrackEngine.h"
#include "model/HarmonyEngine.h"

class HarmonyTrackEngine : public NoteTrackEngine {
public:
    HarmonyTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine);

    virtual TickResult tick(uint32_t tick) override;

    // Master/Follower coordination
    void setMasterEngine(const HarmonyTrackEngine *master) {
        _masterEngine = master;
    }

    const HarmonyTrackEngine *masterEngine() const {
        return _masterEngine;
    }

    // Role checks
    bool isMaster() const;
    bool isFollower() const;
    bool isIndependent() const;

    // Current harmony state
    int16_t currentRootNote() const { return _currentRootNote; }
    const HarmonyEngine::ChordNotes& currentChord() const { return _currentChord; }

private:
    TickResult harmonyTick(uint32_t tick);

    const HarmonyTrackEngine *_masterEngine;

    // Current state
    int16_t _currentRootNote;
    HarmonyEngine::ChordNotes _currentChord;
    int16_t _currentNote; // For followers
};
```

**Step 7.3: Implement Role Check Methods**

File: `src/apps/sequencer/engine/HarmonyTrackEngine.cpp`

```cpp
#include "HarmonyTrackEngine.h"

HarmonyTrackEngine::HarmonyTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine)
    : NoteTrackEngine(engine, model, track, linkedTrackEngine)
    , _masterEngine(nullptr)
    , _currentRootNote(60)
    , _currentNote(60)
{
    _currentChord.root = 60;
    _currentChord.third = 64;
    _currentChord.fifth = 67;
    _currentChord.seventh = 71;
}

bool HarmonyTrackEngine::isMaster() const {
    const NoteSequence &seq = sequence();
    return seq.harmonyRole() == NoteSequence::HarmonyMaster;
}

bool HarmonyTrackEngine::isFollower() const {
    const NoteSequence &seq = sequence();
    return seq.harmonyRole() == NoteSequence::HarmonyFollower3rd ||
           seq.harmonyRole() == NoteSequence::HarmonyFollower5th ||
           seq.harmonyRole() == NoteSequence::HarmonyFollower7th;
}

bool HarmonyTrackEngine::isIndependent() const {
    const NoteSequence &seq = sequence();
    return seq.harmonyRole() == NoteSequence::HarmonyIndependent;
}
```

---

### WEEK 3: Engine Tick Logic and UI

#### **DAY 8-10: Implement Tick Logic**

*(Steps for implementing master tick, follower tick, Engine::tick 3-phase ordering)*

#### **DAY 11-14: Basic UI**

*(Steps for track config page, testing, integration)*

---

## SUCCESS CRITERIA CHECKLIST

- [ ] All unit tests passing (HarmonyEngine, NoteSequence)
- [ ] Master track calculates chords correctly
- [ ] Follower tracks output 3rd/5th/7th correctly
- [ ] Track position constraints enforced (T1/T5 only)
- [ ] No crashes during 5-minute playback
- [ ] C-F-G-Am progression plays correctly in simulator
- [ ] Serialization works (save/load)

---

**NEXT**: After completing Phase 1, review with stakeholders before proceeding to Phase 2.

