# Accumulator Feature Comparison: Current vs Metropolix

**Date**: 2025-11-19
**Purpose**: Complete audit of Metropolix accumulator features vs current implementation

---

## Feature Matrix

| Feature | Metropolix | PEW\|FORMER Status | Notes |
|---------|------------|-------------------|-------|
| **Mode: Stage** | ✅ Yes | ⚠️ Partial | Model has it, engine doesn't use it |
| **Mode: Track** | ✅ Yes | ✅ Complete | Works correctly |
| **Order: Wrap** | ✅ Yes | ✅ Complete | Wraps at boundaries |
| **Order: Pendulum** | ✅ Yes | ✅ Complete | Bounces at boundaries |
| **Order: Random (Drunk Walk)** | ✅ Yes | ✅ Complete | Random values |
| **Order: Hold** | ✅ Yes | ✅ Complete | Clamps at boundaries |
| **Direction: Up** | ✅ Yes | ✅ Complete | Increments |
| **Direction: Down** | ✅ Yes | ✅ Complete | Decrements |
| **Direction: Freeze** | ✅ Yes | ✅ Complete | No change |
| **Polarity: Unipolar** | ✅ Yes | ❌ **NOT IMPLEMENTED** | Enum exists but no logic |
| **Polarity: Bipolar** | ✅ Yes | ❌ **NOT IMPLEMENTED** | Enum exists but no logic |
| **Reset: Manual** | ✅ Yes | ✅ Complete | STOP button, reset CV |
| **Reset: Auto** | ✅ Yes | ❌ **NOT IMPLEMENTED** | See ACCUMULATOR-RESET-MODE-TDD.md |
| **Trigger Mode: Step** | ✅ Yes | ✅ Complete | Tick once per step |
| **Trigger Mode: Gate** | ✅ Yes | ✅ Complete | Tick per gate pulse |
| **Trigger Mode: Retrigger** | ✅ Yes | ✅ Complete | Tick per retrig |
| **Per-Step Triggers** | ✅ Yes | ✅ Complete | Boolean on/off |
| **Per-Step Tick Count** | ⚠️ Unknown | ❌ **NOT IMPLEMENTED** | See ACCUMULATOR-PER-STEP-TICKS-TDD.md |
| **RatchetTriggerMode** | ⚠️ Unknown | ❌ **NOT IMPLEMENTED** | Enum exists but unused |

---

## Detailed Feature Analysis

### 1. ✅ FULLY IMPLEMENTED

#### Mode: Track
- **Status**: ✅ Complete
- **Location**: `Accumulator.h` line 12, `NoteTrackEngine.cpp` line 378-381
- **Behavior**: Applies accumulator value to ALL steps in sequence
- **Test Status**: Working on hardware

#### Order: Wrap, Pendulum, Random, Hold
- **Status**: ✅ Complete
- **Location**: `Accumulator.cpp` lines 58-118
- **Implementation**:
  - `tickWithWrap()`: Wraps from max → min
  - `tickWithPendulum()`: Bounces between min ↔ max
  - `tickWithRandom()`: Random value in range
  - `tickWithHold()`: Clamps at boundaries
- **Test Status**: All orders working correctly

#### Direction: Up, Down, Freeze
- **Status**: ✅ Complete
- **Location**: `Accumulator.h` line 14
- **Behavior**: Controls increment direction
- **Test Status**: Working

#### Trigger Modes: Step, Gate, Retrigger
- **Status**: ✅ Complete
- **Location**: `Accumulator.h` line 16, `NoteTrackEngine.cpp` lines 353-410
- **Behavior**:
  - STEP: Tick once per step
  - GATE: Tick per gate pulse (respects pulseCount, gateMode)
  - RTRIG: Tick per retrigger subdivision
- **Test Status**: All modes working (see QWEN.md)

#### Reset: Manual
- **Status**: ✅ Complete
- **Implementation**: `Accumulator::reset()`, called on STOP button
- **Behavior**: Resets currentValue to minValue
- **Test Status**: Working

---

### 2. ⚠️ PARTIALLY IMPLEMENTED

#### Mode: Stage
- **Status**: ⚠️ Model has it, engine doesn't use it
- **Problem**: Engine always applies accumulator to ALL steps (Track mode only)
- **Location**: `NoteTrackEngine.cpp` line 378-381
- **Missing Logic**:
  ```cpp
  // Current (WRONG):
  if (sequence.accumulator().enabled()) {
      note += accumulator.currentValue();  // Always applied!
  }

  // Should be:
  if (sequence.accumulator().enabled()) {
      if (accumulator.mode() == Accumulator::Track) {
          note += accumulator.currentValue();  // Track mode
      } else if (step.accumulatorTrigger()) {
          note += accumulator.currentValue();  // Stage mode (only triggered steps)
      }
  }
  ```
- **Fix**: See `ACCUMULATOR-STAGE-MODE-TDD.md`
- **Effort**: 1-2 hours (7 lines of code)

---

### 3. ❌ NOT IMPLEMENTED

#### Polarity: Unipolar vs Bipolar
- **Status**: ❌ Enum exists, getter/setter exist, serialization works, BUT NO LOGIC
- **Evidence**:
  - `Accumulator.h` line 13: `enum Polarity { Unipolar, Bipolar };`
  - `Accumulator.h` line 42: `Polarity polarity() const { ... }`
  - `Accumulator.h` line 47: `void setPolarity(Polarity polarity) { ... }`
  - `Accumulator.cpp` line 122-124: Serialization includes polarity
  - **BUT**: No code in `tick()` methods checks `_polarity`!

- **What Polarity Should Do** (Metropolix behavior):
  - **Unipolar**: Value stays on one side of zero
    - Positive unipolar: 0 → max (e.g., 0 → 10)
    - Negative unipolar: min → 0 (e.g., -10 → 0)
  - **Bipolar**: Value crosses zero, uses full negative-to-positive range
    - Example: -10 → +10

- **Current Behavior**:
  - User can set min=-10, max=10, polarity=Unipolar
  - **BUG**: Accumulator will still go from -10 to +10 (ignores polarity!)

- **Implementation Needed**:
  ```cpp
  void Accumulator::setPolarity(Polarity polarity) {
      _polarity = polarity;

      // Enforce polarity constraints
      if (_polarity == Unipolar) {
          // Unipolar: Ensure min and max are on same side of zero
          if (_minValue < 0 && _maxValue > 0) {
              // If crossing zero, decide which side to use
              if (abs(_maxValue) > abs(_minValue)) {
                  _minValue = 0;  // Use positive range
              } else {
                  _maxValue = 0;  // Use negative range
              }
          }
      }
      // Bipolar: No constraints, allow full range
  }

  void Accumulator::setMinValue(int16_t minValue) {
      _minValue = minValue;
      enforcePolarity();  // Check constraints after value change
  }

  void Accumulator::setMaxValue(int16_t maxValue) {
      _maxValue = maxValue;
      enforcePolarity();  // Check constraints after value change
  }

  void Accumulator::enforcePolarity() {
      if (_polarity == Unipolar) {
          // Clamp range to one side of zero
          if (_minValue < 0 && _maxValue > 0) {
              if (abs(_maxValue) > abs(_minValue)) {
                  _minValue = 0;
              } else {
                  _maxValue = 0;
              }
          }
      }
  }
  ```

- **Alternative Interpretation** (Simpler):
  - **Unipolar**: User sets min=0, max=10 (manually enforce one side)
  - **Bipolar**: User sets min=-10, max=10 (manually use full range)
  - **No automatic enforcement** - just a UI hint/constraint

- **Question for User**: Which interpretation do you want?
  - Option A: Automatic enforcement (polarity constrains min/max)
  - Option B: Manual (polarity is just a label, user sets min/max)

- **Effort**: 2-3 hours (model logic, UI constraints, tests)

#### Reset Mode: Auto
- **Status**: ❌ Not implemented
- **What It Does**: Accumulator resets automatically when track completes a cycle
- **Implementation**: See `ACCUMULATOR-RESET-MODE-TDD.md`
- **Effort**: 1.5-2 days (model, engine, UI, tests)

#### Per-Step Tick Count
- **Status**: ❌ Not implemented (currently boolean on/off)
- **What It Does**: Each step can specify HOW MANY accumulator ticks (0-15)
- **Benefits**: Weighted accumulation patterns
- **Implementation**: See `ACCUMULATOR-PER-STEP-TICKS-TDD.md`
- **Effort**: 3-4 hours (model bitfield, engine loop, UI numeric display)

#### RatchetTriggerMode
- **Status**: ❌ Enum exists but completely unused
- **Evidence**:
  - `Accumulator.h` line 18-24: Enum defined
  - `Accumulator.h` line 64: `_ratchetTriggerMode` bitfield
  - `Accumulator.cpp` line 13, 21, 74: Initialized but NEVER READ
- **What It Might Do** (speculation):
  - Control which retrigger pulses trigger accumulator ticks
  - First: Only first retrig ticks accumulator
  - All: Every retrig ticks accumulator
  - Last: Only last retrig ticks accumulator
  - EveryN: Every Nth retrig ticks accumulator
  - RandomTrigger: Random retrigs tick accumulator
- **Question**: Is this a Metropolix feature? Unknown.
- **Recommendation**: Remove or implement (currently dead code)

---

## Summary Statistics

**Fully Implemented**: 9 features ✅
- Track mode
- All 4 orders (Wrap, Pendulum, Random, Hold)
- All 3 directions (Up, Down, Freeze)
- All 3 trigger modes (Step, Gate, Retrigger)
- Manual reset

**Partially Implemented**: 1 feature ⚠️
- Stage mode (model has it, engine doesn't use it)

**Not Implemented**: 4 features ❌
- Polarity (Unipolar vs Bipolar) - enum exists but no logic
- Auto reset mode
- Per-step tick count (currently boolean)
- RatchetTriggerMode (dead code)

**Total Metropolix Coverage**: ~69% (9 of 13 known features)

---

## Recommended Implementation Order

### Priority 1: Fix Existing Features (Critical)
1. **Stage Mode Fix** - 1-2 hours
   - File: `ACCUMULATOR-STAGE-MODE-TDD.md`
   - Impact: Makes existing UI parameter actually work
   - Risk: LOW (simple fix)

### Priority 2: Core Metropolix Features (High Value)
2. **Polarity Implementation** - 2-3 hours
   - Define polarity behavior (auto-enforce or manual)
   - Add logic to setter methods
   - Update UI to show constraints
   - Add tests

3. **Auto Reset Mode** - 1.5-2 days
   - File: `ACCUMULATOR-RESET-MODE-TDD.md`
   - Impact: Major workflow improvement
   - Risk: MEDIUM (engine integration)

### Priority 3: Enhanced Control (Nice to Have)
4. **Per-Step Tick Count** - 3-4 hours
   - File: `ACCUMULATOR-PER-STEP-TICKS-TDD.md`
   - Impact: More expressive accumulation patterns
   - Risk: LOW (well-understood pattern)

### Priority 4: Investigate (Unknown)
5. **RatchetTriggerMode** - TBD
   - Research: Is this a Metropolix feature?
   - Decide: Implement or remove dead code?

---

## Open Questions

1. **Polarity Behavior**: Should polarity auto-constrain min/max, or is it manual?
2. **RatchetTriggerMode**: Is this a real Metropolix feature? Should we implement it?
3. **Per-Step Ticks × Retrig Interaction**: Should they multiply? (See ACCUMULATOR-PER-STEP-TICKS-TDD.md)

---

## File References

- Current Implementation: `src/apps/sequencer/model/Accumulator.h/cpp`
- Engine Integration: `src/apps/sequencer/engine/NoteTrackEngine.cpp`
- UI Integration: `src/apps/sequencer/ui/model/AccumulatorListModel.h`
- TDD Plans:
  - `ACCUMULATOR-STAGE-MODE-TDD.md` - Fix Stage mode
  - `ACCUMULATOR-RESET-MODE-TDD.md` - Add Auto reset
  - `ACCUMULATOR-PER-STEP-TICKS-TDD.md` - Per-step tick count

---

**Next Steps**: User input needed on:
1. Polarity behavior preference (auto-constrain vs manual)
2. Priority order for implementation
3. RatchetTriggerMode - implement or remove?
