# Gate Mode Feature - TDD Implementation Plan

## Feature Overview

Gate Mode is a per-step parameter that controls how gates are fired during pulse count repetitions. This feature works in conjunction with the pulse count feature to provide fine-grained control over gate timing.

## Gate Mode Types

1. **MULTI** (0): Fires a new gate for every clock pulse
   - Pulse count = 4 → 4 separate gates (on-off-on-off-on-off-on-off)

2. **SINGLE** (1): Single gate on first pulse only
   - Pulse count = 4 → 1 gate on first pulse, silent for remaining 3

3. **HOLD** (2): Gate held high for entire duration
   - Pulse count = 4 → 1 long gate spanning all 4 pulses

4. **FIRST_LAST** (3): Gates on first and last pulse only
   - Pulse count = 4 → gate on pulse 1, silent on pulses 2-3, gate on pulse 4

## TDD Implementation Phases

---

## Phase 1: Model Layer - Storage and Data Structures

### Test 1.1: Basic Storage (RED → GREEN → REFACTOR)

**RED - Write Failing Test:**
```cpp
CASE("step stores and retrieves gate mode") {
    NoteSequence::Step step;

    // Default value should be 0 (MULTI)
    expectEqual(step.gateMode(), 0, "default gate mode should be 0 (MULTI)");

    // Test setting various values
    step.setGateMode(1);
    expectEqual(step.gateMode(), 1, "should store gate mode 1 (SINGLE)");

    step.setGateMode(2);
    expectEqual(step.gateMode(), 2, "should store gate mode 2 (HOLD)");

    step.setGateMode(3);
    expectEqual(step.gateMode(), 3, "should store gate mode 3 (FIRST_LAST)");
}
```

**GREEN - Minimal Implementation:**
- Add to `NoteSequence.h`:
  ```cpp
  using GateMode = UnsignedValue<2>;  // 0-3 representing 4 modes

  enum class GateModeType {
      Multi = 0,
      Single = 1,
      Hold = 2,
      FirstLast = 3,
      Last
  };

  // In Step class:
  int gateMode() const { return _data1.gateMode; }
  void setGateMode(int gateMode) {
      _data1.gateMode = GateMode::clamp(gateMode);
  }

  // In _data1 union (using bits 20-21):
  BitField<uint32_t, 20, GateMode::Bits> gateMode;
  // 10 bits left
  ```

**Run Test:** ✅ Should pass

**REFACTOR:** None needed (clean implementation)

---

### Test 1.2: Value Clamping (GREEN immediately)

**Test:**
```cpp
CASE("gate mode clamps out-of-range values") {
    NoteSequence::Step step;

    // Negative values should clamp to 0
    step.setGateMode(-5);
    expectEqual(step.gateMode(), 0, "negative value should clamp to 0");

    // Values above max should clamp to 3
    step.setGateMode(10);
    expectEqual(step.gateMode(), 3, "10 should clamp to 3");

    step.setGateMode(100);
    expectEqual(step.gateMode(), 3, "100 should clamp to 3");

    // Values within range should be preserved
    for (int i = 0; i <= 3; ++i) {
        step.setGateMode(i);
        expectEqual(step.gateMode(), i, "value within range should be preserved");
    }
}
```

**Expected:** ✅ GREEN (UnsignedValue<2> handles clamping)

---

### Test 1.3: Bitfield Packing (GREEN immediately)

**Test:**
```cpp
CASE("gate mode bitfield does not interfere with other fields") {
    NoteSequence::Step step;

    // Set other fields including pulse count
    step.setPulseCount(5);
    step.setRetrigger(2);
    step.setGateMode(2);

    // Verify independence
    expectEqual(step.pulseCount(), 5, "pulseCount should be unchanged");
    expectEqual(step.retrigger(), 2, "retrigger should be unchanged");
    expectEqual(step.gateMode(), 2, "gateMode should be stored");

    // Test all fields at max
    step.setGateMode(3);
    step.setPulseCount(7);
    step.setRetrigger(3);
    step.setAccumulatorTrigger(true);

    expectEqual(step.gateMode(), 3, "gateMode should be 3");
    expectEqual(step.pulseCount(), 7, "pulseCount should be 7");
    expectEqual(step.retrigger(), 3, "retrigger should be 3");
}
```

**Expected:** ✅ GREEN (using bits 20-21, no overlap)

---

### Test 1.4: Layer Integration (RED → GREEN)

**RED - Write Failing Test:**
```cpp
CASE("gate mode integrates with Layer system") {
    // Test 1: GateMode layer exists
    auto layer = NoteSequence::Layer::GateMode;
    expectTrue(layer < NoteSequence::Layer::Last, "GateMode should be valid layer");

    // Test 2: layerName
    const char* name = NoteSequence::layerName(NoteSequence::Layer::GateMode);
    expectEqual(std::string(name), std::string("GATE MODE"), "layer name");

    // Test 3: layerRange
    auto range = NoteSequence::layerRange(NoteSequence::Layer::GateMode);
    expectEqual(range.min, 0, "min should be 0");
    expectEqual(range.max, 3, "max should be 3");

    // Test 4: layerDefaultValue
    int defaultValue = NoteSequence::layerDefaultValue(NoteSequence::Layer::GateMode);
    expectEqual(defaultValue, 0, "default should be 0 (MULTI)");

    // Test 5: layerValue/setLayerValue
    NoteSequence::Step step;
    step.setLayerValue(NoteSequence::Layer::GateMode, 2);
    expectEqual(step.gateMode(), 2, "setLayerValue should work");
    expectEqual(step.layerValue(NoteSequence::Layer::GateMode), 2, "layerValue should work");
}
```

**GREEN - Implementation:**
- Add `GateMode` to `Layer` enum in `NoteSequence.h`
- Add case to `layerName()` returning "GATE MODE"
- Add case to `layerRange()` in `NoteSequence.cpp`
- Add case to `layerDefaultValue()`
- Add cases to `Step::layerValue()` and `setLayerValue()`

---

### Test 1.5: Serialization (GREEN immediately)

**Test:**
```cpp
CASE("gate mode is included in step data") {
    NoteSequence::Step step1;
    step1.setGateMode(0);

    NoteSequence::Step step2;
    step2.setGateMode(2);

    // Verify different modes are different
    expectTrue(step1.gateMode() != step2.gateMode(), "different modes");

    // Verify copying preserves gate mode
    NoteSequence::Step stepCopy = step2;
    expectEqual(stepCopy.gateMode(), 2, "gate mode preserved when copying");
}
```

**Expected:** ✅ GREEN (automatic via _data1.raw serialization)

---

### Test 1.6: Clear/Reset (RED → GREEN)

**Test:**
```cpp
CASE("gate mode resets to 0 on clear") {
    NoteSequence::Step step;

    step.setGateMode(3);
    expectEqual(step.gateMode(), 3, "gate mode should be 3");

    step.clear();

    expectEqual(step.gateMode(), 0, "gate mode should reset to 0 (MULTI)");
}
```

**Expected:** ✅ GREEN (clear() sets _data1.raw = 1, gateMode bits become 0)

---

## Phase 2: Engine Layer - Gate Generation Logic

### Test 2.1: MULTI Mode Behavior (Conceptual/Manual)

**Expected Behavior:**
- Step with pulseCount=4, gateMode=MULTI (0)
- Should generate 4 separate gate events:
  - Pulse 1: gate on → gate off
  - Pulse 2: gate on → gate off
  - Pulse 3: gate on → gate off
  - Pulse 4: gate on → gate off (then advance to next step)

**Implementation Notes:**
- This is the default behavior (already works with current pulse count)
- Modify `triggerStep()` to check `step.gateMode()`
- For MULTI: Keep current behavior (gate per pulse)

---

### Test 2.2: SINGLE Mode Behavior (Conceptual/Manual)

**Expected Behavior:**
- Step with pulseCount=4, gateMode=SINGLE (1)
- Should generate only 1 gate on first pulse:
  - Pulse 1: gate on → gate off
  - Pulses 2-4: no gates (silent)
  - After pulse 4: advance to next step

**Implementation:**
```cpp
// In triggerStep():
bool shouldFireGate = false;
int gateMode = step.gateMode();

switch (gateMode) {
case 0: // MULTI
    shouldFireGate = true;  // Fire every pulse
    break;
case 1: // SINGLE
    shouldFireGate = (_pulseCounter == 0);  // Only first pulse
    break;
case 2: // HOLD
    // Handle in gate queue logic
    break;
case 3: // FIRST_LAST
    shouldFireGate = (_pulseCounter == 0 || _pulseCounter == step.pulseCount());
    break;
}
```

---

### Test 2.3: HOLD Mode Behavior (Conceptual/Manual)

**Expected Behavior:**
- Step with pulseCount=4, gateMode=HOLD (2)
- Should generate 1 long gate:
  - Pulse 1: gate on (stays high)
  - Pulses 2-4: gate remains high
  - After pulse 4: gate off, advance to next step

**Implementation:**
```cpp
// In triggerStep():
case 2: // HOLD
    if (_pulseCounter == 0) {
        // First pulse: gate on with extended length
        uint32_t holdLength = divisor * (step.pulseCount() + 1);
        _gateQueue.pushReplace({ tick + gateOffset, true });
        _gateQueue.pushReplace({ tick + gateOffset + holdLength, false });
    }
    // Don't queue additional gates on subsequent pulses
    break;
```

---

### Test 2.4: FIRST_LAST Mode Behavior (Conceptual/Manual)

**Expected Behavior:**
- Step with pulseCount=4, gateMode=FIRST_LAST (3)
- Should generate 2 gates:
  - Pulse 1: gate on → gate off
  - Pulses 2-3: no gates
  - Pulse 4: gate on → gate off (then advance)

---

## Phase 3: UI Integration

### UI Changes Required:

1. **Add to button cycling** (add to Gate button cycle):
   - Gate → GateProbability → GateOffset → Slide → **GateMode** → Gate

2. **Visual display:**
   - Show mode as text abbreviation:
     - 0 → "MULT"
     - 1 → "SNGL"
     - 2 → "HOLD"
     - 3 → "F-L"

3. **Detail overlay:**
   - Show full mode name when adjusting

4. **Files to modify:**
   - `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp`
     - Add to `switchLayer()` function
     - Add to `activeFunctionKey()` function
     - Add encoder support in `encoder()` function
     - Add visual display in `draw()` function
     - Add detail overlay in `drawDetail()` function

---

## Phase 4: Integration Testing

### Manual Tests:

1. **MULTI + PulseCount=1**: Should behave like normal (1 gate)
2. **MULTI + PulseCount=4**: Should produce 4 separate gates
3. **SINGLE + PulseCount=4**: Should produce 1 gate, step lasts 4 pulses
4. **HOLD + PulseCount=4**: Should produce 1 long gate (4 pulses long)
5. **FIRST_LAST + PulseCount=4**: Should produce 2 gates (first and last)
6. **Interaction with Retrigger**: Verify retrigger works with all modes
7. **Serialization**: Save/load project, verify gate modes preserved

---

## Implementation Order

1. ✅ **Phase 1: Model Layer** (Tests 1.1-1.6)
   - Add gateMode field to Step
   - Layer integration
   - All unit tests passing

2. ✅ **Phase 2: Engine Layer** (Tests 2.1-2.4)
   - Modify triggerStep() gate generation logic
   - Implement all 4 modes
   - Test in simulator

3. ✅ **Phase 3: UI Integration**
   - Add to Gate button cycling
   - Visual feedback
   - Detail overlay

4. ✅ **Phase 4: Testing & Documentation**
   - Manual testing in simulator
   - Hardware verification
   - Update CHANGELOG.md, CLAUDE.md

---

## Technical Details

### Memory Usage:
- 2 bits in _data1 union (bits 20-21)
- 10 bits remaining for future features

### Compatibility:
- Works with pulse count feature (required)
- Compatible with retrigger
- Compatible with all play modes
- Serialization automatic

### Default Behavior:
- Default gateMode = 0 (MULTI)
- Maintains backward compatibility (existing projects work unchanged)

---

## File Modifications Summary

**Model Layer:**
- `src/apps/sequencer/model/NoteSequence.h` - Add gateMode field, enum, Layer entry
- `src/apps/sequencer/model/NoteSequence.cpp` - Layer integration functions

**Engine Layer:**
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Gate generation logic in triggerStep()

**UI Layer:**
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - Full UI integration

**Tests:**
- `src/tests/unit/sequencer/TestGateMode.cpp` - New test file (6 test cases)
- `src/tests/unit/sequencer/CMakeLists.txt` - Register new test

**Documentation:**
- `CHANGELOG.md` - Add gate mode feature
- `CLAUDE.md` - Document gate mode architecture and usage

---

## Notes

- Gate mode only affects behavior when pulseCount > 0 (multiple pulses)
- When pulseCount = 0 (single pulse), all modes behave identically
- HOLD mode produces the longest gate (entire pulse duration)
- SINGLE mode is useful for melodic patterns with rhythm variation
- FIRST_LAST mode creates interesting syncopated patterns
