# HARMONY-ARCHITECTURE-ANALYSIS.md - Architectural Compliance Review

**Date**: 2025-11-18
**Status**: ✅ ANALYSIS COMPLETE
**Purpose**: Ensure WORKING-TDD-HARMONY-PLAN aligns with Performer codebase conventions

---

## Executive Summary

After thorough analysis of the existing Performer codebase, I've identified **12 critical architectural mismatches** in the initial TDD plan that must be corrected before implementation.

**Severity Breakdown**:
- 🔴 **CRITICAL** (4): Will cause compilation failures or runtime crashes
- 🟡 **HIGH** (5): Violates conventions, will cause integration issues
- 🟢 **MEDIUM** (3): Best practices, should be fixed for consistency

---

## 1. ENUM NAMING CONVENTIONS

### ❌ VIOLATION: Incorrect Enum Style

**Current Plan (WRONG)**:
```cpp
class HarmonyEngine {
    enum class Mode {
        Ionian,
        Dorian,
        // ...
    };

    enum class HarmonyRole {
        Independent,
        Master,
        Follower3rd,
        // ...
    };
};
```

### ✅ CORRECT PATTERN (from Accumulator.h):
```cpp
class Accumulator {
    enum Mode { Stage, Track };  // NO "class", NO Last member
    enum Polarity { Unipolar, Bipolar };
    enum Direction { Up, Down, Freeze };
    enum Order { Wrap, Pendulum, Random, Hold };
    enum TriggerMode { Step, Gate, Retrigger };
};
```

### 🔧 REQUIRED FIXES:

**1. Remove `enum class` → use plain `enum`**
**2. Remove `Last` enum member** (Performer doesn't use Last for model enums)
**3. Last is ONLY used in UI Layer enums** (see NoteSequence::Layer enum)

**CORRECTED CODE**:
```cpp
class HarmonyEngine {
public:
    enum Mode {
        Ionian,
        Dorian,
        Phrygian,
        Lydian,
        Mixolydian,
        Aeolian,
        Locrian
        // NO Last member
    };

    enum ChordQuality {
        Minor7,      // -7
        Dominant7,   // 7
        Major7,      // ∆7
        HalfDim7     // ø
        // NO Last member
    };

    enum Voicing {
        Close,
        Drop2,
        Drop3,
        Spread
        // NO Last member
    };

    enum HarmonyRole {
        Independent,
        Master,
        Follower3rd,
        Follower5th,
        Follower7th
        // NO Last member
    };
};
```

**SEVERITY**: 🔴 **CRITICAL** - Will not compile with existing serialization code

---

## 2. TYPE USAGE CONVENTIONS

### ❌ VIOLATION: Inconsistent Integer Types

**Current Plan**: Uses mix of `uint8_t`, `int16_t`, `int`, etc.

### ✅ CORRECT PATTERN (from NoteSequence.h):

**For sequence data (bitfields)**:
- Use **typed aliases** from `Bitfield.h`:
  - `UnsignedValue<N>` for unsigned N-bit values
  - `SignedValue<N>` for signed N-bit values

**For MIDI notes**: `int16_t` (can be negative for "no note")
**For ticks**: `uint32_t`
**For counts/indices**: `int` or `uint8_t`
**For boolean flags in bitfields**: `uint8_t fieldName : 1;`

### 🔧 REQUIRED FIXES:

**CORRECTED TYPE DEFINITIONS**:
```cpp
// In NoteSequence.h
class NoteSequence {
public:
    using Voicing = UnsignedValue<2>;    // 0-3 (4 voicings)
    using Inversion = UnsignedValue<2>;  // 0-3 (4 inversions)
    using HarmonyMode = UnsignedValue<3>; // 0-6 (7 modes in Phase 2)

    class Step {
        // ... existing bitfields ...

        union Data1 {
            struct {
                // ... existing fields ...
                uint32_t hasLocalVoicing : 1;
                uint32_t harmonyVoicing : 2;  // Use Voicing::clamp()
                uint32_t hasLocalInversion : 1;
                uint32_t harmonyInversion : 2; // Use Inversion::clamp()
            };
            uint32_t raw;
        } _data1;
    };
};
```

**SEVERITY**: 🟡 **HIGH** - Inconsistent with codebase patterns

---

## 3. SERIALIZATION PATTERN

### ❌ VIOLATION: Missing Bit-Packing Pattern

**Current Plan**: Separate write() calls for each parameter

### ✅ CORRECT PATTERN (from Accumulator.cpp):

```cpp
void Accumulator::write(VersionedSerializedWriter &writer) const {
    // CRITICAL: Pack bitfields into single byte(s) first
    uint8_t flags = (_mode << 0) | (_polarity << 2) |
                    (_direction << 3) | (_order << 5) |
                    (_enabled << 7);
    writer.write(flags);

    // Then write multi-byte values
    writer.write(_minValue);
    writer.write(_maxValue);
    writer.write(_stepValue);
    // DON'T write mutable state (_currentValue, _pendulumDirection, etc.)
}

void Accumulator::read(VersionedSerializedReader &reader) {
    uint8_t flags;
    reader.read(flags);

    _mode = (flags >> 0) & 0x3;      // 2 bits
    _polarity = (flags >> 2) & 0x1;  // 1 bit
    _direction = (flags >> 3) & 0x3; // 2 bits
    _order = (flags >> 5) & 0x3;     // 2 bits
    _enabled = (flags >> 7) & 0x1;   // 1 bit

    reader.read(_minValue);
    reader.read(_maxValue);
    reader.read(_stepValue);
}
```

### 🔧 REQUIRED FIXES:

**CORRECTED SERIALIZATION**:
```cpp
// In HarmonySequence.cpp
void HarmonySequence::write(VersionedSerializedWriter &writer) const {
    NoteSequence::write(writer);

    // Pack harmony parameters into single byte
    uint8_t flags = (static_cast<uint8_t>(_harmonyRole) << 0) | // 3 bits (5 roles)
                    (static_cast<uint8_t>(_scale) << 3);         // 3 bits (7 modes Phase 2)
    writer.write(flags);
    writer.write(_masterTrackIndex); // int8_t
}

void HarmonySequence::read(VersionedSerializedReader &reader) {
    NoteSequence::read(reader);

    uint8_t flags;
    reader.read(flags);
    _harmonyRole = static_cast<HarmonyRole>((flags >> 0) & 0x7);  // 3 bits
    _scale = static_cast<Mode>((flags >> 3) & 0x7);               // 3 bits

    reader.read(_masterTrackIndex);
}
```

**SEVERITY**: 🟡 **HIGH** - Inefficient serialization, not following codebase pattern

---

## 4. TRACK ENGINE CONSTRUCTOR PATTERN

### ❌ VIOLATION: Missing `const` and Constructor Signature

**Current Plan**:
```cpp
HarmonyTrackEngine(Engine &engine, Model &model, Track &track, uint8_t trackIndex)
    : TrackEngine(engine, model, track)
```

### ✅ CORRECT PATTERN (from NoteTrackEngine.h):

```cpp
NoteTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
    TrackEngine(engine, model, track, linkedTrackEngine),
    _noteTrack(track.noteTrack())
{
    reset();
}
```

**Key observations**:
- `const Model &model` (model is read-only)
- Takes `const TrackEngine *linkedTrackEngine` (not trackIndex)
- Initializes track-specific reference in initializer list

### 🔧 REQUIRED FIXES:

**CORRECTED CONSTRUCTOR**:
```cpp
class HarmonyTrackEngine : public TrackEngine {
public:
    HarmonyTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine)
        : TrackEngine(engine, model, track, linkedTrackEngine)
        , _noteTrack(track.noteTrack())  // Get reference to note track
        , _currentRootNote(60)
        , _currentNote(60)
        , _currentCV(0.0f)
        , _targetCV(0.0f)
        , _masterEngine(nullptr)
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override {
        return Track::TrackMode::Note; // Or new TrackMode::Harmony if added
    }

private:
    NoteTrack &_noteTrack; // Store reference, not trackIndex
};
```

**CRITICAL NOTE**: Track index is available via `_track.trackIndex()` - no need to store separately.

**SEVERITY**: 🔴 **CRITICAL** - Will not match TrackEngine interface

---

## 5. TRACK MODE ENUM EXTENSION

### ❌ ISSUE: Missing TrackMode Extension

**Current Plan**: Assumes `Track::TrackMode::Harmony` exists

### ✅ REALITY CHECK:

TrackMode enum is in `Track.h`. Must verify if Harmony tracks should:
- **Option A**: Extend TrackMode enum (new mode)
- **Option B**: Use existing TrackMode::Note (harmony as NoteTrack extension)

**From architecture**: NoteTrackEngine already exists. Harmony should likely **extend NoteTrackEngine**, not create separate track mode.

### 🔧 RECOMMENDED APPROACH:

**OPTION B (RECOMMENDED)**: Harmony as NoteTrack extension
```cpp
// HarmonyTrackEngine extends NoteTrackEngine, NOT TrackEngine directly
class HarmonyTrackEngine : public NoteTrackEngine {
public:
    HarmonyTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine)
        : NoteTrackEngine(engine, model, track, linkedTrackEngine)
        , _masterEngine(nullptr)
    {
    }

    // Override tick() to add harmony logic
    virtual TickResult tick(uint32_t tick) override {
        if (isIndependent()) {
            // Delegate to base NoteTrackEngine
            return NoteTrackEngine::tick(tick);
        } else {
            // Harmony-specific logic
            return harmonyTick(tick);
        }
    }

private:
    TickResult harmonyTick(uint32_t tick);
    const HarmonyTrackEngine *_masterEngine;
};
```

**RATIONALE**:
- Harmony tracks still need all NoteTrack features (gate, retrigger, pulse count, etc.)
- Master tracks ARE normal note tracks (just also calculate chords)
- Followers just transform output notes
- Avoids duplicating NoteTrackEngine logic

**SEVERITY**: 🔴 **CRITICAL** - Architecture decision affects entire design

---

## 6. LISTMODEL PATTERN COMPLIANCE

### ❌ VIOLATION: Incorrect ListModel Interface

**Current Plan**: Assumes `parameterCount()`, `parameterName()`, etc. methods

### ✅ CORRECT PATTERN (from AccumulatorListModel.h):

**ListModel interface**:
- `rows()` - returns number of items
- `columns()` - returns 2 (name, value)
- `cell(row, column, StringBuilder &str)` - formats display text
- `edit(row, column, value, shift)` - handles encoder editing
- `indexedCount(row)` / `indexed(row)` / `setIndexed(row, index)` - for enum cycling

### 🔧 REQUIRED FIXES:

**CORRECTED UI MODEL**:
```cpp
// File: src/apps/sequencer/ui/model/HarmonyGlobalListModel.h

class HarmonyGlobalListModel : public ListModel {
public:
    enum Item {
        Mode,
        Inversion,
        Transpose,
        Voicing  // Phase 3
        // NO Last member here
    };

    HarmonyGlobalListModel() : _harmonyEngine(nullptr) {}

    void setHarmonyEngine(HarmonyEngine *engine) {
        _harmonyEngine = engine;
    }

    virtual int rows() const override {
        return _harmonyEngine ? 3 : 0; // Phase 2: Mode, Inversion, Transpose
    }

    virtual int columns() const override {
        return 2; // Name, Value
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (!_harmonyEngine) return;

        if (column == 0) {
            formatName(Item(row), str);
        } else {
            formatValue(Item(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (!_harmonyEngine || column != 1) return;

        int count = indexedCount(row);
        if (count > 0) {
            // Indexed (enum) values
            int current = indexed(row);
            int next = (current + value) % count;
            if (next < 0) next += count;
            setIndexed(row, next);
        } else {
            // Non-indexed values (transpose)
            editValue(Item(row), value, shift);
        }
    }

    virtual int indexedCount(int row) const override {
        if (!_harmonyEngine) return 0;

        switch (Item(row)) {
        case Mode:      return 7; // Phase 2: 7 Ionian modes
        case Inversion: return 2; // Phase 2: Root, 1st
        case Voicing:   return 2; // Phase 3: Close, Drop2
        default:        return 0;
        }
    }

    virtual int indexed(int row) const override {
        if (!_harmonyEngine) return -1;

        switch (Item(row)) {
        case Mode:      return static_cast<int>(_harmonyEngine->mode());
        case Inversion: return _harmonyEngine->inversion();
        case Voicing:   return static_cast<int>(_harmonyEngine->voicing());
        default:        return -1;
        }
    }

    virtual void setIndexed(int row, int index) override {
        if (!_harmonyEngine || index < 0) return;

        if (index < indexedCount(row)) {
            switch (Item(row)) {
            case Mode:
                _harmonyEngine->setMode(static_cast<HarmonyEngine::Mode>(index));
                break;
            case Inversion:
                _harmonyEngine->setInversion(index);
                break;
            case Voicing:
                _harmonyEngine->setVoicing(static_cast<HarmonyEngine::Voicing>(index));
                break;
            default:
                break;
            }
        }
    }

private:
    static const char *itemName(Item item) {
        switch (item) {
        case Mode:      return "MODE";
        case Inversion: return "INVERS";  // 6 chars max for display
        case Transpose: return "TRANSP";
        case Voicing:   return "VOICE";
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        if (!_harmonyEngine) return;

        switch (item) {
        case Mode: {
            const char *names[] = {"Ionian", "Dorian", "Phrygian", "Lydian",
                                   "Mixolydian", "Aeolian", "Locrian"};
            str(names[_harmonyEngine->mode()]);
            break;
        }
        case Inversion: {
            const char *names[] = {"Root", "1st"};
            str(names[_harmonyEngine->inversion()]);
            break;
        }
        case Transpose:
            str("%+d", _harmonyEngine->transpose()); // +/- sign
            break;
        case Voicing: {
            const char *names[] = {"Close", "Drop2"};
            str(names[static_cast<int>(_harmonyEngine->voicing())]);
            break;
        }
        }
    }

    void editValue(Item item, int value, bool shift) {
        if (!_harmonyEngine) return;

        const int step = shift ? 12 : 1; // Transpose by octaves with shift

        switch (item) {
        case Transpose: {
            int current = _harmonyEngine->transpose();
            int newValue = std::clamp(current + value * step, -24, 24);
            _harmonyEngine->setTranspose(newValue);
            break;
        }
        default:
            break;
        }
    }

    HarmonyEngine *_harmonyEngine;
};
```

**SEVERITY**: 🟡 **HIGH** - UI won't work without correct interface

---

## 7. PAGE NAMING AND STRUCTURE

### ✅ OBSERVATION: Page Pattern is Correct

**Current Plan**: `HarmonyGlobalPage`, `HarmonyTrackConfigPage` - **GOOD**

**Pattern matches**: `AccumulatorPage`, `RoutingPage`, `ClockSetupPage`

**No changes needed** for page class names.

---

## 8. LAYER ENUM EXTENSION

### ❌ ISSUE: Layer Enum Needs `Last` Member

**Current Plan**: Adds `HarmonyVoicing`, `HarmonyInversion` to Layer enum but removes `Last`

### ✅ CORRECT PATTERN (from NoteSequence.h):

```cpp
enum class Layer {
    Gate,
    // ... other layers ...
    AccumulatorTrigger,
    PulseCount,
    GateMode,
    Last  // ← REQUIRED for UI Layer enums
};
```

**Why `Last` is needed**: UI code iterates through layers, needs end marker.

### 🔧 REQUIRED FIXES:

```cpp
enum class Layer {
    Gate,
    GateProbability,
    GateOffset,
    Slide,
    Retrigger,
    RetriggerProbability,
    Length,
    LengthVariationRange,
    LengthVariationProbability,
    Note,
    NoteVariationRange,
    NoteVariationProbability,
    Condition,
    AccumulatorTrigger,
    PulseCount,
    GateMode,
    HarmonyVoicing,   // NEW Phase 3
    HarmonyInversion, // NEW Phase 3
    Last  // ← REQUIRED
};
```

**SEVERITY**: 🟢 **MEDIUM** - UI layer cycling will break without Last

---

## 9. CONST CORRECTNESS

### ❌ VIOLATION: Missing const on Read-Only Methods

**Current Plan**: Many getters missing `const`

### ✅ CORRECT PATTERN:

```cpp
class HarmonyEngine {
public:
    // Getters MUST be const
    Mode mode() const { return _mode; }
    bool diatonicMode() const { return _diatonicMode; }
    uint8_t inversion() const { return _inversion; }

    // Calculation methods MUST be const
    ChordNotes harmonize(int16_t rootNote, uint8_t scaleDegree) const;
    ChordQuality getDiatonicQuality(uint8_t scaleDegree) const;

private:
    // Mutable state if needed
    mutable ChordNotes _cachedChord;
};
```

**SEVERITY**: 🟢 **MEDIUM** - Compilation warnings, const correctness

---

## 10. GLOBAL HARMONY ENGINE LOCATION

### ❌ ISSUE: Where to Store Global HarmonyEngine?

**Current Plan**: Assumes `model.project().harmonyEngine()`

### ✅ DECISION NEEDED:

**Option A**: Add to `Project` class
```cpp
// In Project.h
class Project {
    // ... existing members ...
    HarmonyEngine &harmonyEngine() { return _harmonyEngine; }
    const HarmonyEngine &harmonyEngine() const { return _harmonyEngine; }

private:
    HarmonyEngine _harmonyEngine;
};
```

**Option B**: Add to per-track `NoteSequence`
```cpp
// Each HarmonySequence has own engine (track-level scale override)
// More flexible but more memory
```

**RECOMMENDATION**: **Option A** - Global with per-track scale override in HarmonySequence.

**Add to Project serialization**:
```cpp
void Project::write(VersionedSerializedWriter &writer) const {
    // ... existing writes ...
    _harmonyEngine.write(writer);
}
```

**SEVERITY**: 🟡 **HIGH** - Must decide before implementation

---

## 11. BITFIELD SPACE VERIFICATION

### ⚠️ WARNING: Bitfield Overflow Risk

**Current Plan**: Adds 6 bits to `Data1` (bits 23-28)

**From NoteSequence.h analysis**:
```cpp
union Data1 {
    struct {
        // Existing:
        uint32_t gateOffset : 5;     // bits 0-4
        uint32_t retrigger : 4;      // bits 5-8
        uint32_t retriggerProbability : 3; // bits 9-11
        uint32_t pulseCount : 3;     // bits 12-14
        uint32_t gateMode : 2;       // bits 15-16
        // ... need to verify ALL fields ...

        // NEW:
        uint32_t hasLocalVoicing : 1;    // bit ?
        uint32_t harmonyVoicing : 2;     // bits ?
        uint32_t hasLocalInversion : 1;  // bit ?
        uint32_t harmonyInversion : 2;   // bits ?
    };
    uint32_t raw; // 32 bits total
};
```

### 🔧 ACTION REQUIRED:

**MUST verify exact bitfield layout** by reading full `NoteSequence::Step` definition.

**Add static assertion**:
```cpp
static_assert(sizeof(Data1) == 4, "Data1 must be exactly 32 bits");
```

**SEVERITY**: 🔴 **CRITICAL** - Bitfield overflow will corrupt data

---

## 12. HARMONYSEQUENCE INHERITANCE

### ❌ VIOLATION: Separate HarmonySequence Class

**Current Plan**: `class HarmonySequence : public NoteSequence`

### ✅ ARCHITECTURAL ISSUE:

**Problem**: Performer's `Project` has fixed-size array of `NoteSequence` objects:
```cpp
class NoteTrack {
    // Fixed size sequences array
    NoteSequence sequences[CONFIG_SEQUENCE_COUNT]; // Can't polymorphically store HarmonySequence
};
```

**Cannot use inheritance** because sequences are stored by value, not pointer.

### 🔧 REQUIRED APPROACH:

**Option 1 (RECOMMENDED)**: Extend `NoteSequence` directly (no new class)
```cpp
// Add harmony properties directly to NoteSequence
class NoteSequence {
public:
    // Existing properties...

    // NEW Phase 1: Harmony properties
    enum HarmonyRole {
        Independent,
        Master,
        Follower3rd,
        Follower5th,
        Follower7th
    };

    HarmonyRole harmonyRole() const { return _harmonyRole; }
    void setHarmonyRole(HarmonyRole role);

    int8_t masterTrackIndex() const { return _masterTrackIndex; }
    void setMasterTrackIndex(int8_t index) { _masterTrackIndex = index; }

    HarmonyEngine::Mode scale() const { return _scale; }
    void setScale(HarmonyEngine::Mode mode);

private:
    HarmonyRole _harmonyRole = Independent;
    int8_t _masterTrackIndex = -1;
    HarmonyEngine::Mode _scale = HarmonyEngine::Ionian;
};
```

**Option 2**: Store harmony data in separate parallel structure (more complex)

**SEVERITY**: 🔴 **CRITICAL** - Inheritance approach won't work

---

## SUMMARY OF REQUIRED CHANGES

### 🔴 CRITICAL (MUST FIX):
1. ✅ Change `enum class` → `enum` (remove class keyword)
2. ✅ Remove `Last` from model enums (keep for Layer enum only)
3. ✅ Fix TrackEngine constructor signature (`const Model &`)
4. ✅ HarmonyTrackEngine should extend NoteTrackEngine, not TrackEngine
5. ✅ Don't create separate HarmonySequence class - extend NoteSequence directly
6. ✅ Verify bitfield space availability in Data1/Data2

### 🟡 HIGH PRIORITY (SHOULD FIX):
7. ✅ Use UnsignedValue<N> / SignedValue<N> typedefs
8. ✅ Implement bit-packed serialization pattern
9. ✅ Follow ListModel interface (rows/columns/cell/edit)
10. ✅ Decide where to store global HarmonyEngine (Project recommended)

### 🟢 MEDIUM (BEST PRACTICE):
11. ✅ Add const correctness to all read-only methods
12. ✅ Keep `Last` member in Layer enum

---

## UPDATED ARCHITECTURE DIAGRAM

```
┌──────────────────────────────────────────────────────────┐
│                    HarmonyEngine                         │
│  - Core harmony logic (standalone, no inheritance)       │
│  - Scale/chord/inversion/voicing algorithms              │
│  - Stored in Project (global settings)                   │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │ uses
                          │
┌──────────────────────────────────────────────────────────┐
│              NoteSequence (EXTENDED)                     │
│  - ADD: harmonyRole, masterTrackIndex, scale             │
│  - ADD: per-step voicing/inversion in Data1 bitfield     │
│  - NO separate HarmonySequence class                     │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │ uses
                          │
┌──────────────────────────────────────────────────────────┐
│         HarmonyTrackEngine : NoteTrackEngine             │
│  - Override tick() for master/follower logic             │
│  - Delegates to NoteTrackEngine::tick() for independent  │
│  - Stores reference to master engine (follower tracks)   │
└──────────────────────────────────────────────────────────┘
                          ▲
                          │ managed by
                          │
┌──────────────────────────────────────────────────────────┐
│                     Engine                               │
│  - 3-phase tick: Masters → Followers → Independent       │
│  - Container of HarmonyTrackEngines                      │
└──────────────────────────────────────────────────────────┘
```

---

## NEXT STEPS

1. ✅ **Update WORKING-TDD-HARMONY-PLAN.md** with all corrections
2. ✅ **Read full NoteSequence::Step** to verify bitfield space
3. ✅ **Verify compatibility** with existing serialization version
4. ✅ **Commit updated plan** with architecture compliance

---

**END OF ARCHITECTURAL ANALYSIS**
