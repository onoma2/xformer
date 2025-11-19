# TDD Plan: Per-Step Accumulator Tick Count

**Date**: 2025-11-19
**Feature**: Replace boolean accumulator trigger with integer tick count per step
**Status**: Planning Phase
**Priority**: Enhancement (improves upon existing feature)

---

## Problem Statement

**Current System** (Boolean Toggle):
- Each step has `accumulatorTrigger()` returning `bool` (on/off)
- When enabled, step ticks accumulator exactly ONCE
- No control over accumulation weight per step

**Limitations**:
- All triggered steps contribute equally (+1 tick each)
- Cannot create weighted accumulation patterns
- Less flexible than Metropolix-style per-step control

---

## Proposed Enhancement

**New System** (Integer Tick Count):
- Each step has `accumulatorTicks()` returning `int` (0-15 ticks)
- When step fires, accumulator ticks N times
- 0 = no accumulation (same as "off")
- 1 = single tick (backward compatible)
- 2-15 = multiple ticks (new capability)

**Benefits**:
- Finer control over accumulation weight per step
- Create accelerating/decelerating patterns
- More expressive, Metropolix-style workflow
- Backward compatible (0 = off, 1 = on)

---

## Use Cases

### Use Case 1: Weighted Accumulation
**Setup**:
- 4-step sequence
- Step 1: ticks=1
- Step 2: ticks=0 (skip)
- Step 3: ticks=2 (double weight)
- Step 4: ticks=1

**Result**: Pattern accumulates as +1, +0, +2, +1 = +4 per cycle

### Use Case 2: Accelerating Pattern
**Setup**:
- 8-step sequence
- Steps 1-4: ticks=1
- Steps 5-6: ticks=2
- Steps 7-8: ticks=3

**Result**: Accumulation accelerates as pattern progresses

### Use Case 3: Accent on Downbeat
**Setup**:
- 16-step sequence
- Step 1: ticks=4 (strong downbeat)
- Steps 5, 9, 13: ticks=2 (quarter notes)
- Other steps: ticks=1 (eighth notes)

**Result**: Downbeat contributes 4x as much accumulation

---

## Architecture: Model-Engine-UI Separation

### Model Layer (`NoteSequence.h/cpp`)

**Current Implementation**:
```cpp
// Step class
bool accumulatorTrigger() const { return _data1.accumulatorTrigger; }
void setAccumulatorTrigger(bool trigger) {
    _data1.accumulatorTrigger = trigger;
}

// In _data1 union
BitField<uint32_t, 22, 1> accumulatorTrigger;  // 1 bit (bool)
```

**New Implementation**:
```cpp
// Step class
int accumulatorTicks() const { return _data1.accumulatorTicks; }
void setAccumulatorTicks(int ticks) {
    _data1.accumulatorTicks = AccumulatorTicks::clamp(ticks);
}

// Type definition
using AccumulatorTicks = UnsignedValue<4>;  // 0-15 range

// In _data1 union
BitField<uint32_t, 22, 4> accumulatorTicks;  // 4 bits (0-15)
```

**Bitfield Analysis**:
- **Current**: 1 bit used (bits 22)
- **New**: 4 bits used (bits 22-25)
- **Cost**: +3 bits per step
- **Available**: 10 bits free → 7 bits free after change
- **Verdict**: ✅ Plenty of space

**Backward Compatibility**:
```cpp
// Migration: Old projects with bool trigger
void read(VersionedSerializedReader &reader) {
    // ... existing reads ...

    if (reader.dataVersion() < NEW_VERSION) {
        // Old format: 1-bit boolean
        bool oldTrigger = _data1.raw & (1 << 22);
        _data1.accumulatorTicks = oldTrigger ? 1 : 0;
    } else {
        // New format: 4-bit count (already read from _data1.raw)
    }
}
```

---

### Engine Layer (`NoteTrackEngine.cpp`)

**Current Implementation**:
```cpp
// In triggerStep() - STEP mode
if (step.accumulatorTrigger()) {
    sequence.accumulator().tick();  // Tick once
}

// In triggerStep() - GATE mode
if (step.accumulatorTrigger() && shouldFireGate) {
    sequence.accumulator().tick();  // Tick once per gate
}

// In triggerStep() - RTRIG mode
if (step.accumulatorTrigger()) {
    int retrigCount = step.retrigger() + 1;
    for (int i = 0; i < retrigCount; i++) {
        sequence.accumulator().tick();  // Tick N times
    }
}
```

**New Implementation**:
```cpp
// In triggerStep() - STEP mode
int tickCount = step.accumulatorTicks();
if (tickCount > 0) {
    for (int i = 0; i < tickCount; i++) {
        sequence.accumulator().tick();
    }
}

// In triggerStep() - GATE mode
int tickCount = step.accumulatorTicks();
if (tickCount > 0 && shouldFireGate) {
    for (int i = 0; i < tickCount; i++) {
        sequence.accumulator().tick();
    }
}

// In triggerStep() - RTRIG mode
int tickCount = step.accumulatorTicks();
if (tickCount > 0) {
    int retrigCount = step.retrigger() + 1;
    int totalTicks = tickCount * retrigCount;
    for (int i = 0; i < totalTicks; i++) {
        sequence.accumulator().tick();
    }
}
```

**Changes**:
- Replace boolean check with integer count
- Loop N times instead of once
- RTRIG mode: Multiply ticks by retrig count (interesting interaction!)

---

### UI Layer (`NoteSequenceEditPage.cpp`)

**Current Implementation** (AccumulatorTrigger layer):
```cpp
case Layer::AccumulatorTrigger:
    // Boolean toggle display
    drawValue(step.accumulatorTrigger() ? "ON" : "OFF");

    // Button toggles on/off
    void keyPress(KeyPressEvent &event) {
        if (event.key() >= Key::Step1 && event.key() <= Key::Step16) {
            step.setAccumulatorTrigger(!step.accumulatorTrigger());
        }
    }
```

**New Implementation** (AccumulatorTicks layer):
```cpp
case Layer::AccumulatorTicks:
    // Numeric value display
    if (step.accumulatorTicks() > 0) {
        drawValue(step.accumulatorTicks());
    } else {
        drawValue("-");  // or blank
    }

    // Button + encoder adjusts value
    void keyPress(KeyPressEvent &event) {
        if (event.key() >= Key::Step1 && event.key() <= Key::Step16) {
            // Toggle between 0 and 1 (quick on/off)
            step.setAccumulatorTicks(step.accumulatorTicks() > 0 ? 0 : 1);
        }
    }

    void encoder(EncoderEvent &event) {
        if (_selectedSteps.any()) {
            for (int i = 0; i < 16; i++) {
                if (_selectedSteps[i]) {
                    int newTicks = step(i).accumulatorTicks() + event.value();
                    step(i).setAccumulatorTicks(newTicks);  // Clamped 0-15
                }
            }
        }
    }
```

**UI Workflow**:
1. Select AccumulatorTicks layer (press Note button repeatedly)
2. Press step button → toggles 0/1 (quick on/off)
3. Select step, turn encoder → adjust 0-15

**Display**:
- Show numeric value above each step (0-15)
- "-" or blank for 0 (off)
- Similar to existing numeric layers (retrigger, pulse count, etc.)

---

## Layer Enum Update

**File**: `NoteSequence.h`

**Current**:
```cpp
enum class Layer : uint8_t {
    Gate,
    // ... other layers ...
    AccumulatorTrigger,  // OLD: Boolean
    Last
};
```

**New**:
```cpp
enum class Layer : uint8_t {
    Gate,
    // ... other layers ...
    AccumulatorTicks,  // NEW: Integer (0-15)
    Last
};
```

**Note**: This is a breaking change for the enum name, but the underlying functionality is backward compatible (0=off, 1=on).

---

## TDD Implementation Plan

### Phase 1: Model Layer Tests

#### Test 1: Default Ticks Value
**File**: `src/tests/unit/sequencer/TestNoteSequence.cpp`

**RED**:
```cpp
CASE("accumulator_ticks_default_is_zero") {
    NoteSequence seq;
    expectEqual(seq.step(0).accumulatorTicks(), 0,
                "default accumulator ticks should be 0");
}
```

**GREEN**: Modify `NoteSequence.h` to change bitfield from 1-bit to 4-bit

#### Test 2: Ticks Setter/Getter
**RED**:
```cpp
CASE("accumulator_ticks_setter_getter") {
    NoteSequence seq;
    seq.step(0).setAccumulatorTicks(5);
    expectEqual(seq.step(0).accumulatorTicks(), 5,
                "should store tick count");
}
```

**GREEN**: Implementation already done in Test 1

#### Test 3: Ticks Clamping
**RED**:
```cpp
CASE("accumulator_ticks_clamping") {
    NoteSequence seq;

    // Test upper bound
    seq.step(0).setAccumulatorTicks(100);
    expectEqual(seq.step(0).accumulatorTicks(), 15,
                "should clamp to 15");

    // Test lower bound
    seq.step(1).setAccumulatorTicks(-5);
    expectEqual(seq.step(1).accumulatorTicks(), 0,
                "should clamp to 0");
}
```

**GREEN**: `UnsignedValue<4>::clamp()` already handles this

#### Test 4: Serialization (Backward Compatibility)
**RED**:
```cpp
CASE("accumulator_ticks_serialization") {
    NoteSequence seq1;
    seq1.step(0).setAccumulatorTicks(7);
    seq1.step(1).setAccumulatorTicks(0);
    seq1.step(2).setAccumulatorTicks(15);

    // Serialize
    VersionedSerializedWriter writer;
    seq1.write(writer);

    // Deserialize
    NoteSequence seq2;
    VersionedSerializedReader reader(writer.data(), writer.dataSize());
    seq2.read(reader);

    expectEqual(seq2.step(0).accumulatorTicks(), 7, "should persist");
    expectEqual(seq2.step(1).accumulatorTicks(), 0, "should persist");
    expectEqual(seq2.step(2).accumulatorTicks(), 15, "should persist");
}
```

**GREEN**: Bitfield serialization via `_data1.raw` handles this automatically

---

### Phase 2: Engine Integration Tests

#### Test 5: Engine Ticks Accumulator N Times
**Manual Hardware Test**

**Setup**:
- Enable accumulator: MODE=STAGE, TRIG=STEP, DIR=UP, ORDER=WRAP
- Set MIN=0, MAX=20, STEP=1
- Create 4-step sequence
- Set accumulator ticks:
  - Step 1: ticks=1
  - Step 2: ticks=0
  - Step 3: ticks=3
  - Step 4: ticks=2

**Expected**:
- Cycle 1:
  - Step 1: value=0, tick once → value=1
  - Step 2: value=1, no ticks → value=1
  - Step 3: value=1, tick 3x → value=4
  - Step 4: value=4, tick 2x → value=6
- Cycle 2:
  - Step 1: value=6, tick once → value=7
  - Step 2: value=7, no ticks → value=7
  - Step 3: value=7, tick 3x → value=10
  - Step 4: value=10, tick 2x → value=12

**Total per cycle**: 1+0+3+2 = 6 ticks

#### Test 6: RTRIG Mode Interaction
**Manual Hardware Test**

**Setup**:
- Same as Test 5, but with retrigger on step 3
- Step 3: retrig=3, ticks=2

**Expected**:
- Step 3 fires 4 times (retrig+1)
- Each fire ticks accumulator 2x
- Total: 4 fires × 2 ticks = 8 ticks from step 3
- **Question for user**: Should RTRIG multiply ticks? Or tick once per retrig?

---

### Phase 3: UI Integration

#### Update NoteSequenceEditPage
**File**: `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp`

**Changes**:
1. Rename `Layer::AccumulatorTrigger` to `Layer::AccumulatorTicks`
2. Update `drawLayer()` to show numeric value (0-15)
3. Update `keyPress()` to toggle 0/1
4. Add `encoder()` support to adjust value
5. Update detail overlay to show "TICKS: X"

**Pattern**: Follow existing numeric layers (Retrigger, PulseCount)

---

## Bitfield Space Analysis

**Current _data1 Layout**:
```cpp
// NoteSequence::Step::_data1 (32 bits total)
BitField<uint32_t, 0, 3> retrigger;              // bits 0-2   (3 bits)
BitField<uint32_t, 3, 3> retriggerProbability;   // bits 3-5   (3 bits)
BitField<uint32_t, 6, 3> length;                 // bits 6-8   (3 bits)
BitField<uint32_t, 9, 3> lengthVariationRange;   // bits 9-11  (3 bits)
BitField<uint32_t, 12, 3> lengthVariationProbability; // bits 12-14 (3 bits)
BitField<uint32_t, 15, 4> condition;             // bits 15-18 (4 bits)
BitField<uint32_t, 19, 3> pulseCount;            // bits 19-21 (3 bits)
BitField<uint32_t, 22, 1> accumulatorTrigger;    // bit  22    (1 bit)  <-- CURRENT
// 10 bits free (bits 23-31)
```

**New _data1 Layout**:
```cpp
// NoteSequence::Step::_data1 (32 bits total)
// ... same as above ...
BitField<uint32_t, 22, 4> accumulatorTicks;      // bits 22-25 (4 bits) <-- NEW
// 7 bits free (bits 26-31)
```

**Cost**: +3 bits per step
**Impact**: 16 steps × 3 bits = 48 bits = 6 bytes per sequence
**Total RAM**: 8 tracks × 6 bytes = 48 bytes (negligible)

---

## Backward Compatibility Strategy

### Option A: Automatic Migration (Recommended)
When loading old projects, automatically convert:
- Old boolean `false` → New integer `0` (off)
- Old boolean `true` → New integer `1` (on)

**Implementation**:
```cpp
void NoteSequence::Step::read(VersionedSerializedReader &reader) {
    _data1.raw = reader.read<uint32_t>();

    // Check data version
    if (reader.dataVersion() < VERSION_ACCUMULATOR_TICKS) {
        // Old format: bit 22 was boolean
        bool oldTrigger = (_data1.raw >> 22) & 0x1;

        // Clear old bit
        _data1.raw &= ~(0x1 << 22);

        // Set new 4-bit field
        _data1.raw |= ((oldTrigger ? 1 : 0) << 22);
    }
    // New format reads directly from _data1.raw
}
```

### Option B: No Migration (Simpler)
Since the new field overlaps with the old bit (bit 22), old projects will automatically have:
- Old `false` → reads as `0` (off) ✅
- Old `true` → reads as `1` (on) ✅

**No migration code needed!** The overlap is perfectly aligned.

**Verdict**: ✅ Use Option B (zero migration cost)

---

## RTRIG Mode Interaction Design Decision

**Question**: How should accumulatorTicks interact with retrigger mode?

### Option A: Multiply (ticks × retrigs)
```cpp
int tickCount = step.accumulatorTicks();
int retrigCount = step.retrigger() + 1;
int totalTicks = tickCount * retrigCount;

for (int i = 0; i < totalTicks; i++) {
    accumulator.tick();
}
```

**Example**: ticks=2, retrig=3 → 2×4=8 ticks

**Pros**: More expressive, can create "bursts" of accumulation
**Cons**: Might be unexpected, can accumulate very fast

### Option B: Independent (ticks once, regardless of retrigs)
```cpp
int tickCount = step.accumulatorTicks();

if (tickCount > 0) {
    for (int i = 0; i < tickCount; i++) {
        accumulator.tick();
    }
}
```

**Example**: ticks=2, retrig=3 → 2 ticks (retrig ignored)

**Pros**: Simpler, more predictable
**Cons**: Less expressive

### Option C: Tick per retrig (current RTRIG behavior)
```cpp
int tickCount = step.accumulatorTicks();
int retrigCount = step.retrigger() + 1;

for (int i = 0; i < retrigCount; i++) {
    for (int j = 0; j < tickCount; j++) {
        accumulator.tick();
    }
}
```

**Example**: ticks=2, retrig=3 → 4 retrigs × 2 ticks = 8 ticks

**Pros**: Consistent with current RTRIG mode implementation
**Cons**: Same as Option A

**Recommendation**: Ask user to choose. I lean toward **Option A/C** (multiply) for maximum expressiveness, but Option B is safer.

---

## Impact Analysis

### Memory
- **Flash**: +~500 bytes (UI layer changes, loop logic)
- **RAM**: +48 bytes (3 bits × 16 steps × 8 tracks)
- **Total Impact**: Negligible

### Performance
- **CPU**: Loop overhead (ticks N times instead of once)
  - Worst case: 15 ticks × 50Hz = 750 ticks/sec
  - Accumulator::tick() is ~20 cycles
  - Total: 750×20 = 15K cycles/sec = 0.009% CPU @ 168MHz
  - **Verdict**: ✅ Negligible

### Compatibility
- ✅ **Backward Compatible**: Old projects read as 0/1
- ✅ **No Migration**: Bitfield overlap is perfect
- ✅ **No Breaking Changes**: Default behavior unchanged

---

## File Manifest

### Files to Modify
1. `src/apps/sequencer/model/NoteSequence.h` - Change bitfield from 1-bit to 4-bit
2. `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Loop N times instead of once
3. `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` - Numeric display, encoder support
4. `src/tests/unit/sequencer/TestNoteSequence.cpp` - Add 4 new test cases

### Files to Create
- None (all changes integrate into existing files)

---

## Timeline Estimate

**Total Time**: 3-4 hours

- **1 hour**: Model layer (bitfield change, tests)
- **1 hour**: Engine integration (loop logic, RTRIG interaction)
- **1 hour**: UI layer (numeric display, encoder)
- **1 hour**: Hardware testing, validation, documentation

---

## Success Criteria

✅ All 4 unit tests pass
✅ Old projects load correctly (0/1 values)
✅ New projects can use 0-15 range
✅ UI displays numeric values
✅ Encoder adjusts tick count
✅ Engine ticks accumulator N times
✅ RTRIG interaction works as designed
✅ No regressions in existing accumulator features
✅ Documentation updated

---

## Implementation Checklist

- [ ] Phase 1: Model layer tests (RED)
- [ ] Phase 1: Change bitfield to 4-bit (GREEN)
- [ ] Phase 1: Verify serialization
- [ ] Phase 2: Update engine tick logic
- [ ] Phase 2: Decide RTRIG interaction (user input needed)
- [ ] Phase 2: Hardware test weighted accumulation
- [ ] Phase 3: Update UI layer display
- [ ] Phase 3: Add encoder support
- [ ] Phase 3: Hardware test UI workflow
- [ ] Documentation updates (CLAUDE.md, QWEN.md)
- [ ] Commit and push

---

## Open Questions for User

1. **RTRIG Interaction**: Should `accumulatorTicks` multiply with retrigger count?
   - Option A: Yes, multiply (ticks × retrigs) - more expressive
   - Option B: No, independent (ticks only, ignore retrigs) - simpler
   - Option C: Tick once per retrig (current behavior) - consistent

2. **UI Toggle Behavior**: When user presses step button, should it:
   - Option A: Toggle 0 ↔ 1 (simple on/off)
   - Option B: Cycle 0 → 1 → 2 → ... → 15 → 0 (full range)
   - Recommendation: Option A (encoder for full range)

3. **Default Value**: Should new steps default to:
   - Option A: 0 (off, backward compatible)
   - Option B: 1 (on by default)
   - Recommendation: Option A

---

**Status**: Ready for implementation (pending user input on RTRIG interaction)
**Next Step**: Get user decision on open questions, then begin Phase 1
