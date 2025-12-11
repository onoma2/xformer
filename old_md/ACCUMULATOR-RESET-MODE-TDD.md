# TDD Plan: Accumulator Reset Mode (Manual vs Auto)

**Date**: 2025-11-19
**Feature**: Add Metropolix-style reset mode to accumulator
**Status**: Planning Phase

---

## Prerequisites

⚠️ **IMPORTANT**: Before implementing Reset Mode, STAGE mode must be fixed!

**See**: `ACCUMULATOR-STAGE-MODE-TDD.md` for details.

**Current Issue**: The accumulator currently only implements TRACK mode (applies to all steps). STAGE mode (applies only to triggered steps) is not working in the engine, even though the UI and model support it.

**Action Required**: Implement STAGE mode first (~1-2 hours), then proceed with Reset Mode.

---

## Feature Comparison: Current vs Metropolix

### ✅ Already Implemented
- **Mode**: Stage vs Track ⚠️ (Model has it, but engine only implements Track mode - **needs fix first!**)
- **Order**: Wrap, Pendulum, Random, Hold ✅ (We have this)
- **Polarity**: Unipolar vs Bipolar ✅ (We have this)
- **Direction**: Up, Down, Freeze ✅ (We have this)
- **Range**: Min/Max limits ✅ (We have this)
- **Manual Reset**: Via STOP button, reset CV ✅ (We have this)

### ❌ Missing Feature
- **Reset Mode**: Manual vs Auto ❌ (THIS IS NEW!)
  - **Manual**: Accumulator only resets via explicit triggers (STOP, reset jack, etc.)
  - **Auto**: Accumulator resets automatically when track completes a cycle

---

## Feature Specification

### Reset Mode Parameter

**Name**: `resetMode`
**Type**: `enum ResetMode { Manual = 0, Auto = 1 }`
**Default**: `Manual` (backward compatible)
**Range**: 0-1 (2 options)

### Behavior

**Manual Mode (0):**
- Accumulator value persists across track cycles
- Only resets when:
  - User presses STOP button
  - External reset CV received
  - User calls `accumulator.reset()` manually

**Auto Mode (1):**
- Accumulator value resets automatically when track completes a full cycle
- Reset happens BEFORE first step of new cycle plays
- Respects delayed first tick (first tick after reset is skipped)
- Still responds to manual reset triggers

### Use Cases

**Manual Mode:**
- Create evolving sequences that build over multiple cycles
- Long-form generative patterns
- Cross-cycle accumulation effects

**Auto Mode:**
- Repeating patterns with consistent accumulation per cycle
- Predictable, looping sequences
- Pattern variations that reset each cycle

---

## Architecture: Model-Engine-UI Separation

This feature follows the standard PEW|FORMER architecture pattern with clear separation of concerns:

### Model Layer (`src/apps/sequencer/model/Accumulator.h/cpp`)
**Responsibilities:**
- Store `resetMode` parameter (Manual vs Auto)
- Provide `handleTrackReset()` method that implements auto-reset logic
- Maintain accumulator state (currentValue, min, max, etc.)
- Serialize/deserialize resetMode to/from projects
- **No timing logic** - model is passive, waits to be called

**Key Methods:**
```cpp
// Data storage
ResetMode resetMode() const;
void setResetMode(ResetMode mode);

// Reset handling (called by engine)
void handleTrackReset();  // Checks resetMode, calls reset() if Auto

// Existing methods
void reset();  // Manual reset (always works)
void tick();   // Increment/decrement value
```

**Thread Safety:**
- Mutable state accessed via `WriteLock`/`ConfigLock` at higher levels
- No internal locking needed (engine controls access)

---

### Engine Layer (`src/apps/sequencer/engine/NoteTrackEngine.cpp`)
**Responsibilities:**
- Detect track cycle boundary (when sequence wraps from last step to first step)
- Call model's `handleTrackReset()` at the appropriate time
- Continue to call `tick()` when accumulator triggers fire
- **No knowledge of resetMode** - delegates to model layer

**Integration Point:**
```cpp
void NoteTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    // Detect cycle boundary
    int prevStep = _sequenceState.step();

    // ... advance step logic ...

    int nextStep = _sequenceState.step();

    // If wrapping from last to first step
    if (nextStep == sequence.firstStep() &&
        prevStep == sequence.lastStep()) {
        // Notify model of track cycle
        if (sequence.accumulator().enabled()) {
            sequence.accumulator().handleTrackReset();
        }
    }

    // ... rest of triggerStep ...
}
```

**Why Engine, Not Model?**
- Only engine knows about step advancement and timing
- Model has no concept of "cycles" or "steps"
- Clean separation: Engine = timing, Model = data

---

### UI Layer (`src/apps/sequencer/ui/model/AccumulatorListModel.h`)
**Responsibilities:**
- Display RESET parameter in AccumulatorPage
- Allow user to toggle between MANUAL and AUTO
- Provide indexed value support (0=MANUAL, 1=AUTO)
- **No business logic** - just displays model state

**Integration:**
```cpp
enum Item {
    // ... existing items ...
    ResetMode,  // NEW
    // ...
};

virtual void cell(int row, int column, StringBuilder &str) const override {
    case ResetMode:
        if (column == 0) {
            str("RESET");
        } else {
            str(resetModeNames[_sequence.accumulator().resetMode()]);
        }
        break;
}

virtual void edit(int row, int column, int value, bool shift) override {
    case ResetMode:
        _sequence.accumulator().setResetMode(
            ModelUtils::adjustedEnum(
                _sequence.accumulator().resetMode(), value));
        break;
}
```

**UI Updates:**
- AccumulatorPage automatically refreshes via ListPage base class
- No special rendering logic needed
- Follows existing pattern (Enable, Mode, Direction, etc.)

---

## Data Flow

**User Interaction (UI → Model):**
1. User rotates encoder on AccumulatorPage
2. AccumulatorListModel.edit() called
3. `_sequence.accumulator().setResetMode(Auto)` updates model
4. Model stores new resetMode value
5. UI re-renders showing "AUTO"

**Track Cycle (Engine → Model):**
1. Engine detects step wrap (last → first)
2. Engine calls `accumulator.handleTrackReset()`
3. Model checks `if (_resetMode == Auto)`
4. If Auto, model calls `reset()` to clear value
5. Next step uses reset value (0 or min)

**Manual Reset (UI/Engine → Model):**
1. User presses STOP or external reset received
2. Engine or UI calls `accumulator.reset()` directly
3. Model resets regardless of resetMode
4. Works in both Manual and Auto modes

---

## Responsibility Matrix

| Concern | Model Layer | Engine Layer | UI Layer |
|---------|------------|--------------|----------|
| **Store resetMode** | ✅ `_resetMode` member | ❌ | ❌ |
| **Validate resetMode** | ✅ `setResetMode()` clamps | ❌ | ❌ |
| **Detect cycle boundary** | ❌ | ✅ Detects step wrap | ❌ |
| **Decide when to reset** | ✅ `handleTrackReset()` checks mode | ❌ | ❌ |
| **Perform reset** | ✅ `reset()` clears value | ❌ | ❌ |
| **Display resetMode** | ❌ | ❌ | ✅ AccumulatorListModel |
| **Edit resetMode** | ❌ | ❌ | ✅ AccumulatorListModel.edit() |
| **Serialize resetMode** | ✅ `write()`/`read()` | ❌ | ❌ |

**Key Principle**: Each layer has a single, well-defined responsibility:
- **Model**: Data + business logic (what to do)
- **Engine**: Timing + coordination (when to do it)
- **UI**: Presentation + interaction (how to show it)

---

## TDD Implementation Plan

### Phase 1: Model Layer (Day 1)

#### Test 1: Default Reset Mode
**File**: `src/tests/unit/sequencer/TestAccumulator.cpp`

**RED**: Write test expecting default resetMode
```cpp
CASE("default_reset_mode_is_manual") {
    Accumulator acc;
    expectEqual(static_cast<int>(acc.resetMode()),
                static_cast<int>(Accumulator::Manual),
                "default reset mode should be Manual");
}
```

**GREEN**: Add to `Accumulator.h`
```cpp
enum ResetMode {
    Manual = 0,
    Auto = 1,
    Last
};

ResetMode resetMode() const { return _resetMode; }
void setResetMode(ResetMode mode) {
    _resetMode = clamp(static_cast<int>(mode), 0, 1);
}

private:
    ResetMode _resetMode = Manual;
```

#### Test 2: Reset Mode Setter/Getter
**RED**: Write test for setter/getter
```cpp
CASE("reset_mode_setter_getter") {
    Accumulator acc;
    acc.setResetMode(Accumulator::Auto);
    expectEqual(static_cast<int>(acc.resetMode()),
                static_cast<int>(Accumulator::Auto),
                "should get Auto mode");
}
```

**GREEN**: Implementation already added above

#### Test 3: Reset Mode Clamping
**RED**: Write test for invalid values
```cpp
CASE("reset_mode_clamping") {
    Accumulator acc;
    acc.setResetMode(static_cast<Accumulator::ResetMode>(99));
    expectEqual(static_cast<int>(acc.resetMode()),
                static_cast<int>(Accumulator::Auto),
                "should clamp to Auto (1)");
}
```

**GREEN**: Clamping already in setResetMode implementation

#### Test 4: Manual Mode Behavior
**RED**: Write test verifying manual mode persists across cycles
```cpp
CASE("manual_mode_persists_across_cycles") {
    Accumulator acc;
    acc.setEnabled(true);
    acc.setResetMode(Accumulator::Manual);
    acc.setDirection(Accumulator::Up);
    acc.setOrder(Accumulator::Wrap);
    acc.setMin(0);
    acc.setMax(10);
    acc.setStepSize(1);

    // First cycle: 5 ticks
    for (int i = 0; i < 5; i++) {
        acc.tick();
    }
    expectEqual(acc.currentValue(), 5, "should be at 5 after 5 ticks");

    // Simulate track reset (does NOT reset accumulator in manual mode)
    // No acc.reset() call here

    // Second cycle: 3 more ticks
    for (int i = 0; i < 3; i++) {
        acc.tick();
    }
    expectEqual(acc.currentValue(), 8,
                "manual mode should continue from 5 to 8");
}
```

**GREEN**: Implement manual mode (no changes needed - current behavior IS manual mode)

#### Test 5: Auto Mode Behavior
**RED**: Write test verifying auto mode resets each cycle
```cpp
CASE("auto_mode_resets_each_cycle") {
    Accumulator acc;
    acc.setEnabled(true);
    acc.setResetMode(Accumulator::Auto);
    acc.setDirection(Accumulator::Up);
    acc.setOrder(Accumulator::Wrap);
    acc.setMin(0);
    acc.setMax(10);
    acc.setStepSize(1);

    // First cycle: 5 ticks
    for (int i = 0; i < 5; i++) {
        acc.tick();
    }
    expectEqual(acc.currentValue(), 5, "should be at 5 after 5 ticks");

    // Simulate track reset (SHOULD reset accumulator in auto mode)
    acc.handleTrackReset();

    // Value should reset to min
    expectEqual(acc.currentValue(), 0,
                "auto mode should reset to min (0)");

    // Second cycle: 3 ticks
    for (int i = 0; i < 3; i++) {
        acc.tick();
    }
    expectEqual(acc.currentValue(), 3,
                "auto mode should start fresh at 3");
}
```

**GREEN**: Implement `handleTrackReset()` method
```cpp
// In Accumulator.h
void handleTrackReset() {
    if (_resetMode == Auto) {
        reset();
    }
}
```

#### Test 6: Serialization
**RED**: Write test for serialization
```cpp
CASE("reset_mode_serialization") {
    Accumulator acc1;
    acc1.setResetMode(Accumulator::Auto);

    // Serialize
    VersionedSerializedWriter writer;
    acc1.write(writer);

    // Deserialize
    Accumulator acc2;
    VersionedSerializedReader reader;
    acc2.read(reader);

    expectEqual(static_cast<int>(acc2.resetMode()),
                static_cast<int>(Accumulator::Auto),
                "resetMode should persist after serialization");
}
```

**GREEN**: Add serialization to `Accumulator.cpp`
```cpp
void Accumulator::write(VersionedSerializedWriter &writer) const {
    // ... existing writes ...

    // Pack resetMode with other flags (1 bit needed)
    uint8_t flags2 = (static_cast<uint8_t>(_resetMode) << 0);
    writer.write(flags2);
}

void Accumulator::read(VersionedSerializedReader &reader) {
    // ... existing reads ...

    uint8_t flags2;
    reader.read(flags2);
    _resetMode = static_cast<ResetMode>((flags2 >> 0) & 0x1);
}
```

---

### Phase 2: Engine Integration (Day 1-2)

#### Test 7: Engine Calls handleTrackReset
**File**: `src/tests/integration/` (or manual hardware test)

**Goal**: Verify NoteTrackEngine calls `handleTrackReset()` when track cycles

**Implementation Location**: `src/apps/sequencer/engine/NoteTrackEngine.cpp`

**Where to Add**:
```cpp
void NoteTrackEngine::reset() {
    // ... existing reset logic ...

    // Notify accumulator of track reset
    if (_noteTrack.sequence().accumulator().enabled()) {
        _noteTrack.sequence().accumulator().handleTrackReset();
    }
}
```

**Alternative Location**: In `triggerStep()` when step index wraps from last to first:
```cpp
void NoteTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    // Detect cycle boundary
    int prevStep = _sequenceState.step();
    int nextStep = evalNextStep(tick, divisor);

    // If wrapping from last step to first step
    if (prevStep > nextStep && nextStep == sequence.firstStep()) {
        // Track has cycled
        if (sequence.accumulator().enabled()) {
            sequence.accumulator().handleTrackReset();
        }
    }

    // ... rest of triggerStep ...
}
```

**Manual Test**:
1. Set accumulator to Auto mode
2. Enable accumulator with stepSize=1, direction=Up
3. Set 4-step sequence with accumulator triggers on all steps
4. Play sequence
5. **Expected**: Values should be 0,1,2,3 → 0,1,2,3 → 0,1,2,3 (repeating)
6. **Not**: 0,1,2,3 → 4,5,6,7 → 8,9,10,11 (accumulating across cycles)

---

### Phase 3: UI Integration (Day 2)

#### Update AccumulatorListModel
**File**: `src/apps/sequencer/ui/model/AccumulatorListModel.h`

**Add to Item enum**:
```cpp
enum Item {
    Enable,
    Mode,
    Direction,
    Order,
    ResetMode,  // NEW
    Polarity,
    Min,
    Max,
    StepSize,
    Value,
    TriggerMode,
    Last
};
```

**Add to row() method**:
```cpp
virtual void cell(int row, int column, StringBuilder &str) const override {
    // ... existing cases ...
    case ResetMode:
        if (column == 0) {
            str("RESET");
        } else if (column == 1) {
            str(resetModeNames[_sequence.accumulator().resetMode()]);
        }
        break;
```

**Add resetModeNames**:
```cpp
static const char *resetModeNames[] = { "MANUAL", "AUTO" };
```

**Add to edit() method**:
```cpp
virtual void edit(int row, int column, int value, bool shift) override {
    // ... existing cases ...
    case ResetMode:
        _sequence.accumulator().setResetMode(
            ModelUtils::adjustedEnum(
                _sequence.accumulator().resetMode(), value));
        break;
```

#### Update AccumulatorPage
**File**: `src/apps/sequencer/ui/pages/AccumulatorPage.cpp`

No changes needed - ListPage will automatically show new parameter.

---

### Phase 4: Testing & Validation (Day 2-3)

#### Unit Tests
**Run**: `cd src/tests/unit && make test`
**Expected**: All 6 new test cases pass

#### Integration Tests (Manual on Hardware)

**Test A: Manual Mode (Default)**
1. Navigate to ACCUM page
2. Set: ENABLE=ON, MODE=STAGE, DIR=UP, ORDER=WRAP, RESET=MANUAL
3. Set: MIN=0, MAX=10, STEP=1, TRIG=STEP
4. Navigate to ACCST page, enable triggers on steps 1-4
5. Set 4-step sequence
6. Press PLAY
7. **Expected**:
   - Cycle 1: Steps play 0, 1, 2, 3
   - Cycle 2: Steps play 4, 5, 6, 7
   - Cycle 3: Steps play 8, 9, 10, 10 (hold at max)

**Test B: Auto Mode**
1. Same setup as Test A, but set RESET=AUTO
2. Press PLAY
3. **Expected**:
   - Cycle 1: Steps play 0, 1, 2, 3
   - Cycle 2: Steps play 0, 1, 2, 3 (RESET!)
   - Cycle 3: Steps play 0, 1, 2, 3 (RESET!)
   - Pattern repeats exactly

**Test C: Auto Mode with Track Mode**
1. Navigate to ACCUM page
2. Set: ENABLE=ON, MODE=TRACK, DIR=UP, ORDER=WRAP, RESET=AUTO
3. Set: MIN=-5, MAX=5, STEP=2, TRIG=STEP
4. Enable trigger on step 4 only
5. Set 4-step melody: C, E, G, C
6. Press PLAY
7. **Expected**:
   - Cycle 1: C, E, G, C (tick on step 4, accum becomes +2)
   - Cycle 2: D, F#, A, D (whole track +2, tick on step 4, accum becomes +4, THEN RESET to 0)
   - Cycle 3: C, E, G, C (reset!, tick on step 4, accum becomes +2)
   - Pattern repeats

**Test D: Manual Reset Still Works**
1. Set RESET=AUTO
2. Play sequence for several cycles
3. Press STOP button
4. **Expected**: Accumulator resets (manual reset still works)

**Test E: Serialization**
1. Set RESET=AUTO
2. Save project to SD card
3. Load project
4. **Expected**: RESET still shows AUTO

---

## Impact Analysis

### Memory Impact
- **Flash**: +~300 bytes (UI strings, handleTrackReset logic)
- **RAM**: +1 byte per sequence (resetMode parameter)
- **Total RAM**: 8 tracks × 1 byte = 8 bytes

### Performance Impact
- **CPU**: Negligible (single condition check per track reset)
- **Timing**: No impact on critical paths

### Compatibility
- ✅ **Backward Compatible**: Default is Manual (existing behavior)
- ✅ **Serialization**: New projects save resetMode, old projects default to Manual
- ✅ **No Breaking Changes**: All existing features unchanged

---

## File Manifest

### Files to Modify
1. `src/apps/sequencer/model/Accumulator.h` - Add resetMode parameter
2. `src/apps/sequencer/model/Accumulator.cpp` - Implement handleTrackReset, serialization
3. `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Call handleTrackReset on cycle
4. `src/apps/sequencer/ui/model/AccumulatorListModel.h` - Add RESET parameter to UI
5. `src/tests/unit/sequencer/TestAccumulator.cpp` - Add 6 new test cases
6. `src/apps/sequencer/CMakeLists.txt` - (No changes needed)

### Files to Create
- None (feature integrates into existing files)

---

## Timeline Estimate

**Total Time**: 1.5-2 days

- **Day 1 Morning**: Phase 1 - Model layer tests (6 tests) + implementation
- **Day 1 Afternoon**: Phase 2 - Engine integration + testing
- **Day 2 Morning**: Phase 3 - UI integration
- **Day 2 Afternoon**: Phase 4 - Hardware testing, validation, documentation
- **Optional Day 3**: Buffer for edge cases, documentation updates

---

## Success Criteria

✅ All 6 unit tests pass
✅ Manual mode preserves existing behavior
✅ Auto mode resets on track cycle
✅ Serialization works correctly
✅ UI displays RESET parameter
✅ Hardware tests confirm expected behavior
✅ No regressions in existing accumulator features
✅ Documentation updated (CLAUDE.md, QWEN.md)

---

## Notes

### Design Decisions

**Why `handleTrackReset()` instead of automatic detection?**
- Explicit reset call is clearer and more maintainable
- Allows engine to control when reset happens (before vs after step trigger)
- No state coupling between accumulator and engine

**Why reset BEFORE first step of new cycle?**
- Ensures first step of cycle uses reset value (0 or min)
- Consistent with delayed first tick behavior
- Matches Metropolix behavior

**Where to call `handleTrackReset()`?**
- Option A: In `NoteTrackEngine::reset()` - called on STOP/PLAY
- Option B: In `triggerStep()` when wrapping from last to first step
- **Recommendation**: Option B (more accurate - detects actual cycle boundary)

### Future Enhancements
- Reset on pattern change
- Reset on song section change
- External CV reset input routing
- Per-step reset triggers

---

## Implementation Checklist

- [ ] Phase 1: Model layer tests (RED)
- [ ] Phase 1: Model layer implementation (GREEN)
- [ ] Phase 1: Serialization tests
- [ ] Phase 2: Engine integration
- [ ] Phase 2: Manual engine testing
- [ ] Phase 3: UI integration
- [ ] Phase 3: UI manual testing
- [ ] Phase 4: Hardware validation
- [ ] Phase 4: Documentation updates
- [ ] Commit and push

---

**Status**: Ready for implementation
**Next Step**: Begin Phase 1 - Write first test case
