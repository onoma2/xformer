# Gate Mode Engine Design

## Current Gate Generation Flow (Step 2.1 Analysis)

### Overview
The NoteTrackEngine generates gates through a two-stage process:

1. **tick() method** (lines 140-189):
   - Called every tick for timing
   - Increments `_pulseCounter` on each divisor match
   - Only advances step when `_pulseCounter > step.pulseCount()`
   - Calls `triggerStep()` **on EVERY pulse** (not just first pulse)

2. **triggerStep() method** (lines 329-381):
   - Gets current step and evaluates gate condition (lines 354-357)
   - If `stepGate` is true, queues gate ON/OFF events via `_gateQueue.pushReplace()`
   - Gate queue format: `{tick + gateOffset, true}` for ON, `{tick + gateOffset + length, false}` for OFF
   - Handles retrigger subdivision (lines 362-369)
   - Normal gate firing (lines 371-372)

### Key Insight
**`triggerStep()` is called on EVERY pulse**, so currently it fires gates on every pulse if stepGate is true. We need to add gate mode logic to control WHEN gates fire within the pulse count duration.

### Pulse Counter Tracking
- `_pulseCounter` starts at 0, increments before step trigger
- After increment, `_pulseCounter` ranges from 1 to (pulseCount + 1)
- When `_pulseCounter > pulseCount`, step advances and counter resets to 0
- **Important:** First pulse has `_pulseCounter == 1`, last pulse has `_pulseCounter == (pulseCount + 1)`

---

## Gate Mode Logic Design (Step 2.2)

### Implementation Strategy
Add gate mode check in `triggerStep()` BEFORE queuing gates (between lines 358-359).

### Pseudocode

```cpp
void NoteTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    // ... existing code lines 330-358 ...

    if (stepGate) {
        // NEW: Check gate mode to determine if we should fire gate on this pulse
        bool shouldFireGate = false;
        int gateMode = step.gateMode();
        int pulseCount = step.pulseCount();

        switch (gateMode) {
        case 0: // ALL - Fire gates on every pulse (default, backward compatible)
            shouldFireGate = true;
            break;

        case 1: // FIRST - Fire gate only on first pulse
            shouldFireGate = (_pulseCounter == 1);
            break;

        case 2: // HOLD - Fire ONE long gate on first pulse
            // On first pulse: queue gate ON and extended gate OFF
            // On other pulses: do nothing (gate remains high)
            shouldFireGate = (_pulseCounter == 1);
            break;

        case 3: // FIRSTLAST - Fire gates on first and last pulse
            shouldFireGate = (_pulseCounter == 1) || (_pulseCounter == (pulseCount + 1));
            break;
        }

        if (shouldFireGate) {
            uint32_t stepLength = (divisor * evalStepLength(step, _noteTrack.lengthBias())) / NoteSequence::Length::Range;

            // HOLD mode: extend gate length to cover all pulses
            if (gateMode == 2) { // HOLD
                // Calculate total duration: pulseCount + 1 pulses worth of time
                stepLength = divisor * (pulseCount + 1);
            }

            int stepRetrigger = evalStepRetrigger(step, _noteTrack.retriggerProbabilityBias());

            // ... existing retrigger and gate queue logic ...
        }
    }

    // ... rest of existing code ...
}
```

---

## Edge Cases and Considerations

### 1. pulseCount = 0 (Single Pulse - Default)
- `_pulseCounter` goes: 0 → 1 → advance
- First pulse: `_pulseCounter == 1`
- Last pulse: `_pulseCounter == 1` (same as first)
- **ALL**: Fires on pulse 1 ✓
- **FIRST**: Fires on pulse 1 ✓
- **HOLD**: Fires extended gate on pulse 1 (length = 1 * divisor) ✓
- **FIRSTLAST**: Fires on pulse 1 (first AND last) ✓

### 2. pulseCount = 3 (Four Pulses)
- `_pulseCounter` goes: 0 → 1 → 2 → 3 → 4 → advance
- First pulse: `_pulseCounter == 1`
- Last pulse: `_pulseCounter == 4`
- **ALL**: Fires on pulses 1, 2, 3, 4 ✓
- **FIRST**: Fires only on pulse 1 ✓
- **HOLD**: Fires extended gate on pulse 1 (length = 4 * divisor) ✓
- **FIRSTLAST**: Fires on pulses 1 and 4 ✓

### 3. Interaction with Gate Offset
- Gate offset is added to tick before queuing (line 352, 366, 367, 371, 372)
- Gate mode logic doesn't affect offset
- Offset applies to all gate modes ✓

### 4. Interaction with Gate Length
- For ALL, FIRST, FIRSTLAST: Use normal stepLength calculation
- For HOLD: Override stepLength to `divisor * (pulseCount + 1)`
- Length variation still applies via `evalStepLength()` for non-HOLD modes ✓

### 5. Interaction with Retrigger
- Retrigger subdivides gates WITHIN a single pulse
- Gate mode controls WHETHER gate fires on this pulse
- If gate mode says don't fire (shouldFireGate = false), skip entire retrigger logic
- If gate fires, retrigger works normally ✓

### 6. Backward Compatibility
- Default gateMode = 0 (ALL)
- ALL mode behaves exactly like current implementation
- Existing projects load with gateMode = 0 (guaranteed by clear() reset)
- No change in behavior for existing sequences ✓

---

## Implementation Plan (Step 2.3)

1. **Add member variable access** (already exists):
   - `step.gateMode()` ✓
   - `step.pulseCount()` ✓
   - `_pulseCounter` ✓

2. **Modify triggerStep()** around line 359:
   - Add shouldFireGate logic with switch statement
   - Calculate extended gate length for HOLD mode
   - Wrap existing gate queue code in `if (shouldFireGate)`

3. **No changes needed**:
   - tick() method (pulse counting already works)
   - Gate queue mechanism
   - CV queue mechanism
   - Retrigger logic

---

## Testing Plan (Step 2.4)

Test with pulseCount = 4 (5 pulses total):

### ALL Mode (gateMode = 0)
- **Expected**: 5 separate gates on pulses 1, 2, 3, 4, 5
- **Verify**: Hear 5 distinct gates

### FIRST Mode (gateMode = 1)
- **Expected**: 1 gate on pulse 1 only, silence on pulses 2-5
- **Verify**: Hear 1 gate, step lasts 5 pulses

### HOLD Mode (gateMode = 2)
- **Expected**: 1 long gate from pulse 1 through pulse 5
- **Verify**: Hear 1 continuous gate (5 * divisor length)

### FIRSTLAST Mode (gateMode = 3)
- **Expected**: Gate on pulse 1, silence on pulses 2-4, gate on pulse 5
- **Verify**: Hear 2 gates (first and last)

---

## Files to Modify

- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - triggerStep() method
- `src/apps/sequencer/engine/NoteTrackEngine.h` - No changes needed (_pulseCounter already exists)

---

## Summary

Gate mode controls WHEN gates fire during pulse count repetitions:
- Works by adding a switch statement BEFORE gate queue operations
- Leverages existing `_pulseCounter` state tracking
- Maintains full backward compatibility (default mode = ALL)
- Simple, clean implementation with clear behavior for all edge cases
