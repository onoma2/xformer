# Per-Step Accumulator Increment Implementation Plan

## Overview

Currently, the accumulator has:
- **Global `stepValue`** (1-100): All triggered steps increment by this amount
- **Per-step trigger** (ON/OFF): Each step can enable/disable accumulator trigger

**Proposed Enhancement:**
Each step should be able to override the global `stepValue` with its own per-step increment amount.

## Use Cases

### Example 1: Variable Chord Voicings
```
Step 1: Trigger with +1 → Adds 1 semitone
Step 2: No trigger → Original pitch
Step 3: Trigger with +4 → Adds 4 semitones (major third)
Step 4: Trigger with +7 → Adds 7 semitones (perfect fifth)

Result: C, E, G, C (chord voicing control)
```

### Example 2: Melodic Intervals
```
Step 1: +0 (root)
Step 2: +2 (major second)
Step 3: +4 (major third)
Step 4: +5 (perfect fourth)
Step 5: +7 (perfect fifth)

Result: Custom melodic intervals
```

### Example 3: Asymmetric Patterns
```
Step 1: +1 (small step)
Step 5: +5 (large leap)
Step 9: +2 (medium step)

Result: Irregular rhythmic patterns
```

## Design Options

### Option A: Replace ON/OFF with Increment Value ⭐ RECOMMENDED

**Current:**
- `isAccumulatorTrigger`: 1 bit (ON/OFF)
- 7 bits available in `_data1`

**Proposed:**
- `accumulatorStepValue`: 5 bits (0-31 range)
  - **0 = OFF** (no trigger, backward compatible)
  - **1-31 = increment values**
  - Uses 5 of the 7 available bits
  - 2 bits remain for future features

**UI:**
- Accumulator Steps page shows: `OFF`, `+1`, `+2`, ..., `+31`
- Turn encoder to cycle through values
- Default = 0 (OFF, backward compatible)

**Advantages:**
- Clean design - one field per step
- Backward compatible (0 = OFF)
- Efficient bit packing (5 bits = 32 values)
- Simple UI (one value to edit)

**Disadvantages:**
- Loses independent ON/OFF toggle
- Max increment limited to 31 (not 100)

### Option B: Keep ON/OFF + Add Override Value

**Current:**
- `isAccumulatorTrigger`: 1 bit (ON/OFF)
- 7 bits available

**Proposed:**
- Keep `isAccumulatorTrigger`: 1 bit
- Add `accumulatorStepValueOverride`: 5 bits (0-31)
  - **0 = use global stepValue**
  - **1-31 = override with this value**
- Total: 6 bits used, 1 bit remains

**Behavior:**
```
If isAccumulatorTrigger == OFF:
    No accumulator tick
Else if accumulatorStepValueOverride > 0:
    Tick by override amount
Else:
    Tick by global stepValue
```

**UI:**
- AccumulatorStepsPage shows two columns:
  - Column 1: `TRIGGER` (ON/OFF)
  - Column 2: `STEP` (0, 1-31) where 0 = "use global"

**Advantages:**
- Preserves existing ON/OFF toggle semantics
- Can use global stepValue (most common case)
- More explicit control

**Disadvantages:**
- More complex (two values per step)
- Slightly more confusing UI
- Uses 6 bits instead of 5

### Option C: Full Range (7 bits)

Use all 7 available bits for `accumulatorStepValue`:
- Range: 0-127
- 0 = OFF
- 1-100 = increment values (matches global stepValue range)
- 101-127 = unused (reserved for future)

**Advantages:**
- Matches global stepValue range exactly
- Maximum flexibility

**Disadvantages:**
- Uses all available bits (none left for future)
- Overkill (increments > 31 rarely useful)

## Recommendation: Option A

**Use 5 bits for combined trigger+value:**
- 0 = OFF (no trigger)
- 1-31 = increment values
- Simple, clean, backward compatible
- Leaves 2 bits for future expansion

**Rationale:**
- Most use cases need values 0-12 (chromatic scale)
- Range of 31 is more than sufficient
- Simpler UI (one value to edit)
- Less cognitive load (no "use global" concept)

## Implementation Plan

### Phase 1: Model Layer

**File:** `src/apps/sequencer/model/NoteSequence.h`

**Change 1: Expand bitfield (line 283)**
```cpp
// OLD:
BitField<uint32_t, 16, 1> isAccumulatorTrigger;  // 1 bit
// 7 bits left

// NEW:
BitField<uint32_t, 16, 5> accumulatorStepValue;  // 5 bits (0-31)
// 2 bits left
```

**Change 2: Update type definitions (after line 60)**
```cpp
// Add new type for per-step accumulator value
using AccumulatorStepValue = UnsignedValue<5>;  // 0-31
```

**Change 3: Replace accessor methods (lines 222-225)**
```cpp
// OLD:
bool isAccumulatorTrigger() const { return _data1.isAccumulatorTrigger ? true : false; }
void setAccumulatorTrigger(bool isAccumulatorTrigger) { _data1.isAccumulatorTrigger = isAccumulatorTrigger; }
void toggleAccumulatorTrigger() { setAccumulatorTrigger(!isAccumulatorTrigger()); }

// NEW:
int accumulatorStepValue() const { return _data1.accumulatorStepValue; }
void setAccumulatorStepValue(int value) {
    _data1.accumulatorStepValue = AccumulatorStepValue::clamp(value);
}

// Keep backward-compatible boolean accessor for convenience
bool isAccumulatorTrigger() const { return _data1.accumulatorStepValue > 0; }
void setAccumulatorTrigger(bool trigger) {
    // If turning on, default to 1 (global stepValue behavior)
    // If turning off, set to 0
    _data1.accumulatorStepValue = trigger ? 1 : 0;
}
void toggleAccumulatorTrigger() {
    setAccumulatorTrigger(!isAccumulatorTrigger());
}
```

**Change 4: Update Layer enum (around line 86)**
```cpp
enum class Layer : int {
    Gate,
    GateProbability,
    GateOffset,
    Slide,
    GateMode,
    Retrigger,
    RetriggerProbability,
    PulseCount,
    Length,
    LengthVariationRange,
    LengthVariationProbability,
    Note,
    NoteVariationRange,
    NoteVariationProbability,
    Condition,
    AccumulatorTrigger,      // Keep for backward compatibility
    AccumulatorStepValue,    // NEW: For editing per-step values
    Last
};
```

**Change 5: Update layerValue/setLayerValue (in .cpp file)**

Add case for `AccumulatorStepValue` in both `layerValue()` and `setLayerValue()`.

### Phase 2: Engine Layer

**File:** `src/apps/sequencer/engine/NoteTrackEngine.cpp`

**Location:** Lines 452-458 in `triggerStep()`

**Current code:**
```cpp
// STEP mode: Tick accumulator once per step (first pulse only)
if (step.isAccumulatorTrigger() && _pulseCounter == 1) {
    const auto &targetSequence = useFillSequence ? *_fillSequence : sequence;
    if (targetSequence.accumulator().enabled() &&
        targetSequence.accumulator().triggerMode() == Accumulator::Step) {
        const_cast<Accumulator&>(targetSequence.accumulator()).tick();
    }
}
```

**New code with per-step increment:**
```cpp
// STEP mode: Tick accumulator once per step (first pulse only)
if (step.isAccumulatorTrigger() && _pulseCounter == 1) {
    const auto &targetSequence = useFillSequence ? *_fillSequence : sequence;
    if (targetSequence.accumulator().enabled() &&
        targetSequence.accumulator().triggerMode() == Accumulator::Step) {

        // Get per-step increment value (0 = OFF, 1-31 = increment amount)
        int stepIncrement = step.accumulatorStepValue();

        if (stepIncrement > 0) {
            // Temporarily override accumulator stepValue for this tick
            auto &accumulator = const_cast<Accumulator&>(targetSequence.accumulator());
            uint8_t savedStepValue = accumulator.stepValue();

            accumulator.setStepValue(stepIncrement);
            accumulator.tick();

            // Restore original stepValue
            accumulator.setStepValue(savedStepValue);
        }
    }
}
```

**Alternative (cleaner):** Add `tick(int stepOverride)` method to Accumulator:
```cpp
// In Accumulator.h:
void tick(int stepOverride = -1) const;  // -1 means use _stepValue

// Then in triggerStep():
int stepIncrement = step.accumulatorStepValue();
if (stepIncrement > 0) {
    const_cast<Accumulator&>(targetSequence.accumulator()).tick(stepIncrement);
}
```

**Similar changes needed for:**
- GATE mode tick (around line 495)
- RETRIGGER mode tick (around line 510)

### Phase 3: UI Layer

**File:** `src/apps/sequencer/ui/model/AccumulatorStepsListModel.h`

**Current (lines 47-60):**
```cpp
virtual int indexedCount(int row) const override {
    return 2; // true/false for each step
}

virtual int indexed(int row) const override {
    if (!_sequence || row >= 16) return 0;
    return _sequence->step(row).isAccumulatorTrigger() ? 1 : 0;
}

virtual void setIndexed(int row, int index) override {
    if (!_sequence || row >= 16 || index < 0 || index > 1) return;
    const_cast<NoteSequence::Step&>(_sequence->step(row)).setAccumulatorTrigger(index != 0);
}
```

**New (numeric values 0-31):**
```cpp
virtual int indexedCount(int row) const override {
    return 32; // 0-31 (0=OFF, 1-31=increment values)
}

virtual int indexed(int row) const override {
    if (!_sequence || row >= 16) return 0;
    return _sequence->step(row).accumulatorStepValue();
}

virtual void setIndexed(int row, int index) override {
    if (!_sequence || row >= 16 || index < 0 || index > 31) return;
    const_cast<NoteSequence::Step&>(_sequence->step(row)).setAccumulatorStepValue(index);
}
```

**Update formatValue (lines 67-73):**
```cpp
void formatValue(int stepIndex, StringBuilder &str) const {
    if (_sequence && stepIndex < 16) {
        int value = _sequence->step(stepIndex).accumulatorStepValue();
        if (value == 0) {
            str("OFF");
        } else {
            str("+%d", value);
        }
    } else {
        str("OFF");
    }
}
```

**Update editValue (lines 75-80):**
```cpp
void editValue(int stepIndex, int value, bool shift) {
    if (!_sequence || stepIndex >= 16) return;

    int currentValue = _sequence->step(stepIndex).accumulatorStepValue();
    int newValue = currentValue + value;

    // Wrap around: 0 → 31 → 0
    if (newValue < 0) newValue = 31;
    if (newValue > 31) newValue = 0;

    const_cast<NoteSequence::Step&>(_sequence->step(stepIndex)).setAccumulatorStepValue(newValue);
}
```

### Phase 4: NoteSequenceEditPage Integration

**File:** `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp`

Currently, pressing Note button (F3) cycles through note layers. The `AccumulatorTrigger` layer displays ON/OFF for each step.

**No changes needed** - the layer system uses `step.layerValue()` which will return the new numeric value. The UI will automatically show 0-31.

**Optional enhancement:** Display format could show `OFF` for 0 and `+N` for values 1-31.

### Phase 5: Accumulator Page Enhancement

**File:** `src/apps/sequencer/ui/model/AccumulatorListModel.h`

Consider adding a new item to show **how many steps have non-zero values**:

```cpp
enum Item {
    Enabled,
    Mode,
    Direction,
    Order,
    TriggerMode,
    MinValue,
    MaxValue,
    StepValue,      // Global default (used when step value = 1)
    ActiveSteps,    // NEW: Count of steps with value > 0 (read-only)
    CurrentValue,
    Last
};
```

This gives users quick feedback on how many steps have accumulator triggers.

## Backward Compatibility

### Serialization
The bitfield is serialized as `_data1.raw`, so:
- **Old projects**: `isAccumulatorTrigger=1` will deserialize as `accumulatorStepValue=1`
- This means: trigger enabled with increment of 1 (uses global stepValue behavior)
- **Perfect backward compatibility!** ✅

### Behavior
- Old behavior: All triggered steps increment by global `stepValue`
- New behavior: Steps with value=1 increment by 1, same as before if global stepValue was 1
- If global stepValue was not 1, slight behavior change (steps now increment by 1 instead of global value)

**Migration Strategy:**
- Most users likely use stepValue=1 (default in many cases)
- For others, they can edit per-step values to match their old global stepValue
- Alternative: On project load, check if all triggered steps have value=1, and if so, set them to the global stepValue (migration logic)

### UI Consistency
The `isAccumulatorTrigger()` method is kept for backward compatibility and returns `true` if value > 0.

## Testing Plan

### Test Case 1: Verify Backward Compatibility
1. Load old project with accumulator triggers
2. Verify triggered steps show value=1
3. Verify accumulator still increments
4. Expected: Same behavior as before

### Test Case 2: Different Per-Step Increments
1. Set step 1 = +1
2. Set step 2 = OFF (0)
3. Set step 3 = +5
4. Set step 4 = +10
5. Run sequence with Direction=Up, Order=Wrap, Min=0, Max=20
6. Expected:
   - Step 1: value increases by 1
   - Step 2: no change
   - Step 3: value increases by 5
   - Step 4: value increases by 10

### Test Case 3: Track vs Step Mode
1. Set Mode = Step
2. Set step 1 = +3, step 3 = +7
3. Sequence: C, E, G, C
4. Expected:
   - Step 1: C+3 = D#
   - Step 2: E+0 = E (Step mode: ignores accumulator)
   - Step 3: G+10 = F (accumulator went 0→3→10)
   - Step 4: C+0 = C

5. Change Mode = Track
6. Expected:
   - Step 1: C+3 = D#
   - Step 2: E+3 = G (Track mode: applies accumulator)
   - Step 3: G+10 = F
   - Step 4: C+10 = A

### Test Case 4: All Order Modes
Test Wrap, Pendulum, Random, Hold with different per-step increments:
- Pendulum with asymmetric values (e.g., +1, +5, +2)
- Random with different trigger densities
- Hold reaching max at different rates

### Test Case 5: UI Workflow
1. Navigate to Accumulator Steps page (ACCST)
2. Turn encoder on step 1 → cycles 0 → 1 → 2 → ... → 31 → 0
3. Verify display shows "OFF", "+1", "+2", etc.
4. Navigate to Note page, press Note button to reach AccumulatorStepValue layer
5. Verify step buttons show per-step values
6. Edit values using encoder

## Edge Cases

### Edge Case 1: Global stepValue Interaction
**Question:** What happens to the global stepValue parameter?

**Answer:** Keep it as a **reference/default** value:
- Shows in Accumulator page (ACCUM) as "STEP" parameter
- Informational only (not used in engine anymore)
- Could be used as "set all steps to this value" utility function
- Or: Remove from Accumulator page entirely (breaking change)

**Recommendation:** Keep global stepValue for now as "template" value. Add button function to copy it to all triggered steps.

### Edge Case 2: Step Value = 0 in Middle of Sequence
**Scenario:** Steps 1, 3, 5 have values +1, +0, +1

**Behavior:**
- Step 1: Tick by 1 (accumulator = 1)
- Step 3: Value=0 means OFF, no tick (accumulator stays 1)
- Step 5: Tick by 1 (accumulator = 2)

**Correct:** 0 means OFF (no trigger), not "tick by zero".

### Edge Case 3: Very Large Increments with Small Range
**Scenario:**
- Min=0, Max=5, Order=Wrap
- Step values: +10, +15, +20

**Behavior:**
- Step 1: 0 + 10 = 10, wraps to (10 - 5 - 1) = 4
- Step 2: 4 + 15 = 19, wraps to (19 % 6) = 1
- Step 3: 1 + 20 = 21, wraps to (21 % 6) = 3

**Correct:** Wrap logic in `tickWithWrap()` handles this correctly.

## Alternative UI Approaches

### Approach A: Encoder + Shift for Large Steps ⭐
```
Turn encoder: +1/-1
Hold SHIFT + turn encoder: +5/-5

Quick access to common values:
0 (OFF), 1, 2, 3, 4, 5, 7, 12 (octave)
```

### Approach B: Presets
Add "preset" buttons:
- F1: Set selected steps to OFF (0)
- F2: Set selected steps to +1
- F3: Set selected steps to +5
- F4: Set selected steps to +12

### Approach C: Copy Global
Add function to copy global stepValue to selected steps:
- Select multiple steps (S1-S16)
- Press SHIFT + encoder: copies Accumulator.stepValue to all selected steps

## Summary

### Benefits
✅ **Per-step control:** Each step can have different increment amounts
✅ **Creative flexibility:** Variable intervals, chord voicings, asymmetric patterns
✅ **Backward compatible:** Old projects work identically
✅ **Efficient:** Uses only 5 bits (2 bits remain for future)
✅ **Clean UI:** Single value to edit per step

### Trade-offs
⚠️ **Max increment limited to 31** (down from 100)
- Acceptable: Most musical use cases need 0-12
- Chromatic scale = 12 semitones
- 31 = nearly 3 octaves range per step

⚠️ **Global stepValue becomes less useful**
- Solution: Keep as "template" value or add "copy to all" function

### Effort Estimate
- **Model layer:** 30 minutes (bitfield + accessors)
- **Engine layer:** 1 hour (tick logic for 3 trigger modes)
- **UI layer:** 1 hour (AccumulatorStepsPage + formatting)
- **Testing:** 1.5 hours (backward compat + per-step behavior)
- **Total:** ~4 hours

## Files to Modify

1. **Model:**
   - `src/apps/sequencer/model/NoteSequence.h` - Bitfield + accessors
   - `src/apps/sequencer/model/NoteSequence.cpp` - Layer value methods

2. **Engine:**
   - `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Tick logic (3 places)

3. **UI:**
   - `src/apps/sequencer/ui/model/AccumulatorStepsListModel.h` - Display 0-31
   - `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - Optional display formatting

4. **Tests:**
   - `src/tests/unit/sequencer/TestAccumulatorPerStep.cpp` - New test file
