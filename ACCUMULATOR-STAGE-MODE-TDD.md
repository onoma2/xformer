# TDD Plan: Accumulator STAGE Mode Implementation

**Date**: 2025-11-19
**Priority**: HIGH (Prerequisite for Reset Mode feature)
**Status**: Planning Phase

---

## Problem Statement

The current accumulator implementation ONLY supports **TRACK mode** (applies accumulation to all steps in the sequence). **STAGE mode** is not implemented.

### Current Behavior (TRACK Mode Only)
```cpp
// In NoteTrackEngine::evalStepNote()
if (sequence.accumulator().enabled()) {
    int accumulatorValue = sequence.accumulator().currentValue();
    note += accumulatorValue;  // Applied to ALL steps!
}
```

This applies the accumulator value to EVERY step that plays, regardless of which steps have accumulator triggers enabled.

### Missing Behavior (STAGE Mode)

**STAGE mode should**:
- Only apply accumulator value to steps where accumulator trigger is ENABLED
- Steps without triggers play their original note values
- Example: If only step 1 has a trigger, only step 1's pitch evolves over cycles

---

## Feature Specification

### Mode Parameter (Already Exists)

From `Accumulator.h`:
```cpp
enum Mode {
    Stage = 0,  // ✅ Enum exists
    Track = 1,  // ✅ Enum exists
    Last
};

Mode mode() const { return _mode; }
void setMode(Mode mode) { _mode = mode; }
```

**Model layer already has the parameter!** We just need to USE it in the engine.

---

## Architecture: Model-Engine-UI Separation

### Model Layer (`Accumulator.h/cpp`)
**Current Status**: ✅ Already complete
- Has `_mode` parameter (Stage vs Track)
- Getter/setter methods work
- Serialization works
- No changes needed!

### Engine Layer (`NoteTrackEngine.cpp`)
**Current Status**: ❌ Only implements Track mode
- Always applies accumulator to all steps
- Ignores `mode` parameter
- **Needs fix**: Check mode and step trigger before applying

### UI Layer (`AccumulatorListModel.h`)
**Current Status**: ✅ Already complete
- MODE parameter visible in UI
- Can toggle between Stage/Track
- No changes needed!

---

## Current vs Correct Implementation

### Current Implementation (WRONG)
```cpp
// In NoteTrackEngine::evalStepNote()
// Called for EVERY step that plays
if (sequence.accumulator().enabled()) {
    int accumulatorValue = sequence.accumulator().currentValue();
    note += accumulatorValue;  // ALWAYS APPLIED (Track mode only!)
}
```

### Correct Implementation (STAGE + TRACK modes)
```cpp
// In NoteTrackEngine::evalStepNote()
if (sequence.accumulator().enabled()) {
    int accumulatorValue = sequence.accumulator().currentValue();

    // Check mode
    if (sequence.accumulator().mode() == Accumulator::Track) {
        // TRACK mode: Apply to ALL steps
        note += accumulatorValue;
    } else {
        // STAGE mode: Only apply to steps with triggers
        if (step.accumulatorTrigger()) {  // Check trigger on THIS step
            note += accumulatorValue;
        }
    }
}
```

---

## TDD Implementation Plan

### Phase 1: Model Layer - Already Complete ✅

No work needed! Model already has:
- ✅ `Mode` enum (Stage, Track)
- ✅ `mode()` getter
- ✅ `setMode()` setter
- ✅ Serialization
- ✅ UI integration

### Phase 2: Engine Layer - Implement Mode Logic

#### Test 1: Track Mode (Current Behavior)
**File**: `src/tests/integration/` or manual hardware test

**Setup**:
- Enable accumulator with mode=TRACK
- Set stepSize=1, direction=UP, min=0, max=10
- Create 4-step sequence: C, E, G, C
- Enable accumulator triggers on steps 1 and 3 only

**Expected** (Track mode):
- Cycle 1: C, E, G, C (accumulator ticks at steps 1 and 3, value becomes +2)
- Cycle 2: D, F#, A, D (ALL steps +2 semitones)
- Cycle 3: F, G#, B, F (ALL steps +4 semitones)

#### Test 2: Stage Mode (New Behavior)
**File**: `src/tests/integration/` or manual hardware test

**Setup**:
- Enable accumulator with mode=STAGE
- Set stepSize=1, direction=UP, min=0, max=10
- Create 4-step sequence: C, E, G, C
- Enable accumulator triggers on steps 1 and 3 only

**Expected** (Stage mode):
- Cycle 1:
  - Step 1: C + 0 = C (trigger fires, accumulator becomes +1)
  - Step 2: E + 0 = E (NO trigger, accumulator not applied)
  - Step 3: G + 1 = G# (trigger fires, accumulator becomes +2)
  - Step 4: C + 0 = C (NO trigger, accumulator not applied)
- Cycle 2:
  - Step 1: C + 2 = D (trigger fires, accumulator becomes +3)
  - Step 2: E + 0 = E (NO trigger)
  - Step 3: G + 3 = A# (trigger fires, accumulator becomes +4)
  - Step 4: C + 0 = C (NO trigger)
- Cycle 3:
  - Step 1: C + 4 = E (trigger fires, accumulator becomes +5)
  - Step 2: E + 0 = E (NO trigger)
  - Step 3: G + 5 = C (trigger fires, accumulator becomes +6)
  - Step 4: C + 0 = C (NO trigger)

**Key Difference**:
- Track mode: ALL steps shift together
- Stage mode: ONLY triggered steps accumulate

#### Implementation: Modify NoteTrackEngine.cpp

**File**: `src/apps/sequencer/engine/NoteTrackEngine.cpp`
**Location**: Line ~378 (in `evalStepNote()` function)

**Current Code** (lines 378-381):
```cpp
// Apply accumulator modulation (AFTER harmony)
if (sequence.accumulator().enabled()) {
    int accumulatorValue = sequence.accumulator().currentValue();
    note += accumulatorValue;
}
```

**New Code**:
```cpp
// Apply accumulator modulation (AFTER harmony)
if (sequence.accumulator().enabled()) {
    int accumulatorValue = sequence.accumulator().currentValue();

    // Check accumulator mode
    if (sequence.accumulator().mode() == Accumulator::Track) {
        // TRACK mode: Apply to ALL steps
        note += accumulatorValue;
    } else {
        // STAGE mode: Only apply to steps with triggers enabled
        if (step.accumulatorTrigger()) {
            note += accumulatorValue;
        }
    }
}
```

**Lines Changed**: 4 lines modified, 7 lines total
**Risk**: LOW (simple conditional, well-tested pattern)

---

### Phase 3: Validation

#### Unit Tests
No unit tests needed - this is purely engine integration logic. The model layer already has complete tests.

#### Hardware Tests

**Test A: Track Mode (Regression Test)**
1. Set MODE=TRACK, enable accumulator
2. Set 4-step sequence with triggers on all 4 steps
3. **Expected**: Entire track transposes together
4. **Purpose**: Ensure Track mode still works

**Test B: Stage Mode (New Feature)**
1. Set MODE=STAGE, enable accumulator
2. Set 4-step sequence with triggers on steps 1 and 3 only
3. **Expected**: Only steps 1 and 3 accumulate, steps 2 and 4 stay constant
4. **Purpose**: Verify Stage mode works correctly

**Test C: Stage Mode with Single Trigger**
1. Set MODE=STAGE, enable accumulator
2. Set 4-step sequence with trigger on step 1 only
3. **Expected**: Only step 1 accumulates, all other steps constant
4. **Purpose**: Verify single-step accumulation (classic Metropolix behavior)

**Test D: Mode Switching**
1. Set MODE=TRACK, play for a few cycles
2. Switch to MODE=STAGE mid-playback
3. **Expected**: Behavior changes immediately, no glitches
4. **Purpose**: Verify mode can be changed in real-time

---

## Impact Analysis

### Code Changes
- **Files Modified**: 1 file (`NoteTrackEngine.cpp`)
- **Lines Changed**: ~7 lines
- **Complexity**: LOW (simple if/else)

### Memory Impact
- **Flash**: +~50 bytes (conditional logic)
- **RAM**: 0 bytes (no new state)
- **CPU**: Negligible (one extra condition check per note evaluation)

### Compatibility
- ✅ **Backward Compatible**: Default mode is Track (existing behavior)
- ✅ **No Breaking Changes**: Users can switch modes via UI
- ✅ **Model Already Supports It**: Just engine implementation missing

---

## File Manifest

### Files to Modify
1. `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Add mode check in evalStepNote()

### Files Already Complete (No Changes)
- ✅ `src/apps/sequencer/model/Accumulator.h` - Mode enum exists
- ✅ `src/apps/sequencer/model/Accumulator.cpp` - Serialization works
- ✅ `src/apps/sequencer/ui/model/AccumulatorListModel.h` - UI already shows MODE
- ✅ `src/tests/unit/sequencer/TestAccumulator.cpp` - Model tests already pass

---

## Timeline Estimate

**Total Time**: 1-2 hours

- **30 minutes**: Modify NoteTrackEngine.cpp (7 lines)
- **30 minutes**: Hardware testing (Track + Stage modes)
- **30 minutes**: Edge case testing, validation
- **30 minutes**: Documentation updates (CLAUDE.md, QWEN.md)

---

## Success Criteria

✅ Track mode continues to work (regression test)
✅ Stage mode applies accumulator only to triggered steps
✅ Mode can be switched in real-time
✅ No crashes, no glitches
✅ Hardware tests pass
✅ Documentation updated

---

## Dependencies

**Blocks**:
- ACCUMULATOR-RESET-MODE-TDD.md (Reset Mode feature needs Stage mode to work correctly)

**Depends On**:
- Nothing! All prerequisites already implemented.

---

## Notes

### Why Was This Missed?

Looking at the implementation history, it seems:
1. Model layer was implemented correctly (has mode parameter)
2. UI layer was implemented correctly (can edit mode)
3. Engine layer was partially implemented (only Track mode logic)

This is a classic "forgot to wire it up" bug.

### Testing Strategy

Since there are no model-layer changes, we don't need TDD for this feature. We just need:
1. Modify engine layer (7 lines)
2. Manual hardware testing
3. Validation

The existing unit tests already verify that the model's `mode()` getter/setter work correctly.

---

## Implementation Checklist

- [ ] Modify NoteTrackEngine.cpp::evalStepNote() (add mode check)
- [ ] Hardware test: Track mode (regression)
- [ ] Hardware test: Stage mode (new feature)
- [ ] Hardware test: Single-step trigger
- [ ] Hardware test: Mode switching
- [ ] Update CLAUDE.md (clarify Stage vs Track behavior)
- [ ] Update QWEN.md (clarify Stage vs Track behavior)
- [ ] Commit and push

---

**Status**: Ready for implementation
**Next Step**: Modify NoteTrackEngine.cpp (7 lines)
**Risk**: LOW (simple fix, well-understood)
