# Pulse Count Feature - TDD Implementation Plan

**Feature:** Metropolix-style per-step pulse count (1-8 pulses)

**Approach:** Test-Driven Development (TDD)

**Cycle:** Red → Green → Refactor

---

## Table of Contents
1. [Test Infrastructure Setup](#test-infrastructure-setup)
2. [Phase 1: Model Layer Tests](#phase-1-model-layer-tests)
3. [Phase 2: Engine Layer Tests](#phase-2-engine-layer-tests)
4. [Phase 3: Integration Tests](#phase-3-integration-tests)
5. [Phase 4: UI Tests](#phase-4-ui-tests)
6. [Phase 5: Acceptance Tests](#phase-5-acceptance-tests)
7. [Implementation Checklist](#implementation-checklist)

---

## TDD Principles for This Feature

### The TDD Cycle

**Red:** Write a failing test
```cpp
TEST_CASE("Step stores pulse count") {
    NoteSequence::Step step;
    step.setPulseCount(3);
    REQUIRE(step.pulseCount() == 3);  // ❌ FAILS - not implemented yet
}
```

**Green:** Write minimal code to pass
```cpp
// In NoteSequence.h
int pulseCount() const { return _data1.pulseCount; }
void setPulseCount(int pulseCount) {
    _data1.pulseCount = PulseCount::clamp(pulseCount);
}
```

**Refactor:** Clean up while keeping tests green
```cpp
// Extract validation, improve naming, etc.
```

### Test Categories

1. **Unit Tests:** Individual components in isolation
2. **Integration Tests:** Components working together
3. **Property Tests:** Invariants that must always hold
4. **Regression Tests:** Ensure existing features still work

---

## Test Infrastructure Setup

### Step 0.1: Create Test File
**File:** `src/tests/unit/sequencer/TestPulseCount.cpp`

**Before Implementation:**
```bash
cd src/tests/unit/sequencer
touch TestPulseCount.cpp
```

**Initial Structure:**
```cpp
#include "catch.hpp"
#include "model/NoteSequence.h"

TEST_CASE("PulseCount placeholder", "[pulse-count]") {
    // Placeholder - will be replaced with real tests
    REQUIRE(true);
}
```

**Build & Run:**
```bash
cd build/sim/debug
make -j
./src/tests/unit/unit_tests "[pulse-count]"
```

**Expected:** ✅ Placeholder test passes

---

### Step 0.2: Add to CMakeLists.txt
**File:** `src/tests/unit/CMakeLists.txt`

**Add:**
```cmake
sequencer/TestPulseCount.cpp
```

**Verify:** Test file compiles and runs

---

## Phase 1: Model Layer Tests

### Test 1.1: Basic Storage and Retrieval

**RED - Write Failing Test:**
```cpp
TEST_CASE("Step stores and retrieves pulse count", "[pulse-count][model]") {
    NoteSequence::Step step;

    SECTION("Default pulse count is 0") {
        REQUIRE(step.pulseCount() == 0);
    }

    SECTION("Can set and get pulse count") {
        step.setPulseCount(3);
        REQUIRE(step.pulseCount() == 3);
    }

    SECTION("Can set minimum value") {
        step.setPulseCount(0);
        REQUIRE(step.pulseCount() == 0);
    }

    SECTION("Can set maximum value") {
        step.setPulseCount(7);
        REQUIRE(step.pulseCount() == 7);
    }
}
```

**Expected:** ❌ Compilation error - `pulseCount()` doesn't exist

**GREEN - Minimal Implementation:**

**File:** `src/apps/sequencer/model/NoteSequence.h`

```cpp
// Add type definition (line 36)
using PulseCount = UnsignedValue<3>;  // 0-7 representing 1-8 pulses

// Add to Step class (after line 188)
int pulseCount() const { return _data1.pulseCount; }
void setPulseCount(int pulseCount) {
    _data1.pulseCount = PulseCount::clamp(pulseCount);
}

// Add bitfield (line 229)
BitField<uint32_t, 17, PulseCount::Bits> pulseCount;  // bits 17-19
```

**Run Test:**
```bash
./src/tests/unit/unit_tests "[pulse-count][model]"
```

**Expected:** ✅ All tests pass

**REFACTOR:** None needed yet - code is minimal

---

### Test 1.2: Value Clamping

**RED - Write Failing Test:**
```cpp
TEST_CASE("PulseCount clamps out-of-range values", "[pulse-count][model]") {
    NoteSequence::Step step;

    SECTION("Negative values clamp to 0") {
        step.setPulseCount(-5);
        REQUIRE(step.pulseCount() == 0);
    }

    SECTION("Values above max clamp to 7") {
        step.setPulseCount(10);
        REQUIRE(step.pulseCount() == 7);

        step.setPulseCount(100);
        REQUIRE(step.pulseCount() == 7);
    }

    SECTION("Values within range are preserved") {
        for (int i = 0; i <= 7; ++i) {
            step.setPulseCount(i);
            REQUIRE(step.pulseCount() == i);
        }
    }
}
```

**Expected:** ✅ Should pass with existing implementation (PulseCount::clamp)

**GREEN:** No code changes needed - clamping works via UnsignedValue

**REFACTOR:** None needed

---

### Test 1.3: Bitfield Packing

**RED - Write Failing Test:**
```cpp
TEST_CASE("PulseCount bitfield doesn't interfere with other fields", "[pulse-count][model]") {
    NoteSequence::Step step;

    SECTION("Setting pulse count preserves other fields") {
        // Set various other fields
        step.setRetrigger(2);
        step.setRetriggerProbability(5);
        step.setCondition(Types::Condition::Fill);
        step.setAccumulatorTrigger(true);

        // Set pulse count
        step.setPulseCount(4);

        // Verify other fields unchanged
        REQUIRE(step.retrigger() == 2);
        REQUIRE(step.retriggerProbability() == 5);
        REQUIRE(step.condition() == Types::Condition::Fill);
        REQUIRE(step.isAccumulatorTrigger() == true);
        REQUIRE(step.pulseCount() == 4);
    }

    SECTION("Setting other fields preserves pulse count") {
        step.setPulseCount(6);

        step.setRetrigger(3);
        step.setRetriggerProbability(4);

        REQUIRE(step.pulseCount() == 6);
    }

    SECTION("Bitfield independence test") {
        // Set all bits to verify no overlap
        step.setPulseCount(7);           // bits 17-19 = 111
        step.setRetrigger(3);             // bits 0-1   = 11
        step.setRetriggerProbability(7);  // bits 2-4   = 111
        step.setAccumulatorTrigger(true); // bit 16     = 1

        REQUIRE(step.pulseCount() == 7);
        REQUIRE(step.retrigger() == 3);
        REQUIRE(step.retriggerProbability() == 7);
        REQUIRE(step.isAccumulatorTrigger() == true);
    }
}
```

**Expected:** ✅ Should pass - bitfields are independent

**GREEN:** No code changes needed

**REFACTOR:** None needed

---

### Test 1.4: Layer Integration

**RED - Write Failing Test:**
```cpp
TEST_CASE("PulseCount integrates with Layer system", "[pulse-count][model]") {
    NoteSequence sequence;

    SECTION("Layer enum includes PulseCount") {
        // This is a compile-time check
        auto layer = NoteSequence::Layer::PulseCount;
        REQUIRE(layer != NoteSequence::Layer::Last);
    }

    SECTION("layerName returns correct name") {
        const char* name = NoteSequence::layerName(NoteSequence::Layer::PulseCount);
        REQUIRE(name != nullptr);
        REQUIRE(std::string(name) == "PULSE CNT");
    }

    SECTION("layerRange returns correct range") {
        auto range = NoteSequence::layerRange(NoteSequence::Layer::PulseCount);
        REQUIRE(range.min == 0);
        REQUIRE(range.max == 7);
    }

    SECTION("layerDefaultValue returns 0") {
        int defaultVal = NoteSequence::layerDefaultValue(NoteSequence::Layer::PulseCount);
        REQUIRE(defaultVal == 0);
    }
}
```

**Expected:** ❌ Fails - Layer::PulseCount doesn't exist

**GREEN - Implementation:**

**File:** `src/apps/sequencer/model/NoteSequence.h`

```cpp
// Add to Layer enum (line 54)
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
    PulseCount,  // NEW
    Last
};

// Add to layerName() (line 73)
case Layer::PulseCount:         return "PULSE CNT";
```

**File:** `src/apps/sequencer/model/NoteSequence.cpp`

```cpp
// In layerRange() function
case Layer::PulseCount:
    return { 0, PulseCount::Max };

// In layerDefaultValue() function
case Layer::PulseCount:
    return 0;
```

**Run Test:** ✅ All tests pass

**REFACTOR:** None needed

---

### Test 1.5: Layer Value Access

**RED - Write Failing Test:**
```cpp
TEST_CASE("PulseCount accessible via layerValue/setLayerValue", "[pulse-count][model]") {
    NoteSequence sequence;
    auto& step = sequence.step(0);

    SECTION("layerValue returns pulse count") {
        step.setPulseCount(5);
        int value = step.layerValue(NoteSequence::Layer::PulseCount);
        REQUIRE(value == 5);
    }

    SECTION("setLayerValue sets pulse count") {
        step.setLayerValue(NoteSequence::Layer::PulseCount, 3);
        REQUIRE(step.pulseCount() == 3);
    }

    SECTION("setLayerValue clamps values") {
        step.setLayerValue(NoteSequence::Layer::PulseCount, 10);
        REQUIRE(step.pulseCount() == 7);

        step.setLayerValue(NoteSequence::Layer::PulseCount, -1);
        REQUIRE(step.pulseCount() == 0);
    }
}
```

**Expected:** ❌ Fails - case not handled in layerValue/setLayerValue

**GREEN - Implementation:**

**File:** `src/apps/sequencer/model/NoteSequence.cpp`

```cpp
// In Step::layerValue()
case Layer::PulseCount:
    return pulseCount();

// In Step::setLayerValue()
case Layer::PulseCount:
    setPulseCount(value);
    break;
```

**Run Test:** ✅ All tests pass

**REFACTOR:** None needed

---

### Test 1.6: Serialization

**RED - Write Failing Test:**
```cpp
TEST_CASE("PulseCount persists through serialization", "[pulse-count][model][serialization]") {
    NoteSequence sequence1;
    NoteSequence sequence2;

    SECTION("Single step pulse count") {
        sequence1.step(0).setPulseCount(4);

        // Serialize
        VersionedSerializedWriter writer;
        sequence1.write(writer);

        // Deserialize
        VersionedSerializedReader reader(writer.data(), writer.dataSize());
        sequence2.read(reader);

        REQUIRE(sequence2.step(0).pulseCount() == 4);
    }

    SECTION("Multiple steps with different pulse counts") {
        for (int i = 0; i < 16; ++i) {
            sequence1.step(i).setPulseCount(i % 8);
        }

        VersionedSerializedWriter writer;
        sequence1.write(writer);

        VersionedSerializedReader reader(writer.data(), writer.dataSize());
        sequence2.read(reader);

        for (int i = 0; i < 16; ++i) {
            REQUIRE(sequence2.step(i).pulseCount() == (i % 8));
        }
    }

    SECTION("Pulse count combined with other step data") {
        auto& step = sequence1.step(0);
        step.setPulseCount(5);
        step.setGate(true);
        step.setNote(12);
        step.setRetrigger(2);

        VersionedSerializedWriter writer;
        sequence1.write(writer);

        VersionedSerializedReader reader(writer.data(), writer.dataSize());
        sequence2.read(reader);

        auto& step2 = sequence2.step(0);
        REQUIRE(step2.pulseCount() == 5);
        REQUIRE(step2.gate() == true);
        REQUIRE(step2.note() == 12);
        REQUIRE(step2.retrigger() == 2);
    }
}
```

**Expected:** ✅ Should pass - bitfield automatically serialized

**GREEN:** No code changes needed - Step serialization handles all bitfields

**REFACTOR:** None needed

---

### Test 1.7: Clear/Reset Behavior

**RED - Write Failing Test:**
```cpp
TEST_CASE("PulseCount resets on step clear", "[pulse-count][model]") {
    NoteSequence::Step step;

    step.setPulseCount(6);
    step.setGate(true);
    step.setNote(24);

    step.clear();

    SECTION("Pulse count resets to default") {
        REQUIRE(step.pulseCount() == 0);
    }

    SECTION("Other fields also reset") {
        REQUIRE(step.gate() == false);
        REQUIRE(step.note() == NoteSequence::Note::Min);
    }
}
```

**Expected:** ✅ Should pass - clear() resets all bitfields

**GREEN:** No code changes needed

**REFACTOR:** None needed

---

## Phase 2: Engine Layer Tests

### Test 2.1: Pulse Counter State

**RED - Write Failing Test:**
```cpp
TEST_CASE("NoteTrackEngine has pulse counter state", "[pulse-count][engine]") {
    // Note: This requires creating a test fixture with Engine
    // Simplified version:

    SECTION("Pulse counter initializes to 0") {
        // Will verify via integration test
        REQUIRE(true);  // Placeholder
    }
}
```

**Expected:** Test structure in place (may need mock engine)

**GREEN - Implementation:**

**File:** `src/apps/sequencer/engine/NoteTrackEngine.h`

```cpp
// Add member variable (around line 94)
private:
    int _stepPulseCounter = 0;
```

**File:** `src/apps/sequencer/engine/NoteTrackEngine.cpp`

```cpp
// In reset() method
void NoteTrackEngine::reset() {
    _freeRelativeTick = 0;
    _sequenceState.reset();
    _currentStep = -1;
    _prevCondition = false;
    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;
    _cvOutputTarget = 0.f;
    _slideActive = false;
    _stepPulseCounter = 0;  // NEW
    _gateQueue.clear();
    _cvQueue.clear();
    _recordHistory.clear();

    changePattern();
}

// In restart() method
void NoteTrackEngine::restart() {
    _freeRelativeTick = 0;
    _sequenceState.reset();
    _currentStep = -1;
    _stepPulseCounter = 0;  // NEW
}
```

**REFACTOR:** None needed

---

### Test 2.2: Single Step Behavior

**Property:** Step with pulseCount=1 behaves like current implementation

**RED - Write Failing Test:**
```cpp
TEST_CASE("Step with pulseCount=0 (1 actual) advances normally", "[pulse-count][engine]") {
    // This is an integration test - will implement in Phase 3
    // Property: pulseCount=0 should not change current behavior
}
```

**Expected:** Will verify in integration tests

**GREEN:** Implementation in next section

---

### Test 2.3: Multi-Pulse Step Behavior

**Property:** Step advances only after pulseCount divisor periods

**RED - Write Test Specification:**
```cpp
TEST_CASE("Step advances after pulseCount divisor periods", "[pulse-count][engine]") {
    /*
    Test Scenario:
    - Step 0: pulseCount=3 (stays for 3 divisor periods)
    - Step 1: pulseCount=1 (advances after 1 period)

    Expected tick sequence:
    Tick 0:   Step 0, pulse 1/3
    Tick 48:  Step 0, pulse 2/3
    Tick 96:  Step 0, pulse 3/3
    Tick 144: Step 1, pulse 1/1
    Tick 192: Step 2
    */
}
```

**Expected:** Will implement as integration test

**GREEN - Implementation:**

**File:** `src/apps/sequencer/engine/NoteTrackEngine.cpp`

```cpp
// In tick() method - Aligned mode section (around line 138-159)

case Types::PlayMode::Aligned:
    if (relativeTick % divisor == 0) {
        // NEW: Pulse count logic
        const auto &currentStep = sequence.step(_sequenceState.step());
        int pulseCount = currentStep.pulseCount() + 1;  // 0-7 stored → 1-8 actual

        _stepPulseCounter++;

        if (_stepPulseCounter >= pulseCount) {
            // Advance to next step
            _sequenceState.advanceAligned(relativeTick / divisor, sequence.runMode(),
                                          sequence.firstStep(), sequence.lastStep(), rng);
            _stepPulseCounter = 0;  // Reset for next step

            recordStep(tick, divisor);
            triggerStep(tick, divisor);
        }
        // else: Stay on current step (don't trigger)
    }
    break;
```

**Run Integration Test:** (Phase 3)

**REFACTOR:** Extract pulse count logic to helper method?

---

### Test 2.4: Free Mode Support

**Property:** Free mode also respects pulse count

**RED - Write Test:**
```cpp
TEST_CASE("Free mode respects pulse count", "[pulse-count][engine]") {
    /*
    Free mode advances based on internal counter, not global tick
    Should still count pulses before advancing
    */
}
```

**GREEN - Implementation:**

```cpp
// In tick() method - Free mode section

case Types::PlayMode::Free:
    relativeTick = _freeRelativeTick;
    if (++_freeRelativeTick >= divisor) {
        _freeRelativeTick = 0;
    }
    if (relativeTick == 0) {
        // NEW: Pulse count logic
        const auto &currentStep = sequence.step(_sequenceState.step());
        int pulseCount = currentStep.pulseCount() + 1;

        _stepPulseCounter++;

        if (_stepPulseCounter >= pulseCount) {
            _sequenceState.advanceFree(sequence.runMode(), sequence.firstStep(),
                                      sequence.lastStep(), rng);
            _stepPulseCounter = 0;

            recordStep(tick, divisor);
            triggerStep(tick, divisor);
        }
    }
    break;
```

**REFACTOR:** Consider extracting common pulse logic

---

### Test 2.5: Reset Measure Interaction

**Property:** Reset measure clears pulse counter

**RED - Write Failing Test:**
```cpp
TEST_CASE("Reset measure clears pulse counter", "[pulse-count][engine]") {
    /*
    Scenario:
    - Step with pulseCount=5 (long step)
    - Reset measure triggers during pulse 3
    - Counter should reset, sequence restarts
    */
}
```

**Expected:** ✅ Should already work (reset() clears counter)

**GREEN:** No changes needed

**REFACTOR:** None needed

---

## Phase 3: Integration Tests

### Test 3.1: Basic Pattern Timing

**RED - Write Integration Test:**
```cpp
TEST_CASE("Pattern timing with pulse counts", "[pulse-count][integration]") {
    // Create a test harness with mock engine
    // This will be a more complex test requiring full engine setup

    SECTION("4-step pattern with varying pulse counts") {
        /*
        Pattern: A B C D
        Pulse counts: 1 2 3 1
        Divisor: 48 (1/16 note)

        Expected timing:
        Tick 0:   Step A starts (pulse 1/1)
        Tick 48:  Step B starts (pulse 1/2)
        Tick 96:  Step B continues (pulse 2/2)
        Tick 144: Step C starts (pulse 1/3)
        Tick 192: Step C continues (pulse 2/3)
        Tick 240: Step C continues (pulse 3/3)
        Tick 288: Step D starts (pulse 1/1)
        Tick 336: Back to Step A

        Total pattern: (1+2+3+1) × 48 = 336 ticks
        */

        // Setup
        NoteSequence sequence;
        sequence.step(0).setPulseCount(0);  // 1 pulse
        sequence.step(1).setPulseCount(1);  // 2 pulses
        sequence.step(2).setPulseCount(2);  // 3 pulses
        sequence.step(3).setPulseCount(0);  // 1 pulse
        sequence.setDivisor(48);

        // Run engine ticks and verify step positions
        // (Requires integration test harness)
    }
}
```

**Expected:** Integration test framework needed

**GREEN:** Will implement with full engine mock

---

### Test 3.2: Pulse Count + Retrigger

**Property:** Both features work together

**RED - Write Test:**
```cpp
TEST_CASE("Pulse count works with retrigger", "[pulse-count][integration]") {
    SECTION("Step plays for N pulses, each with M retrigs") {
        /*
        Step: pulseCount=2, retrigger=2 (3 triggers per pulse)
        Expected: Step plays for 2 divisor periods
                  Each period has 3 triggers (retrigger=2 means 3 total)
        */

        NoteSequence sequence;
        sequence.step(0).setPulseCount(1);  // 2 pulses
        sequence.step(0).setRetrigger(2);   // 3 triggers

        // Verify triggers occur at correct times
    }
}
```

**Expected:** Should work without conflicts

**GREEN:** Verify retrigger logic unchanged

---

### Test 3.3: All RunModes

**Property:** Pulse count works with all sequence directions

**RED - Write Tests:**
```cpp
TEST_CASE("Pulse count with all RunModes", "[pulse-count][integration]") {
    SECTION("Forward mode") {
        // Steps advance forward, each respecting pulse count
    }

    SECTION("Backward mode") {
        // Steps advance backward, each respecting pulse count
    }

    SECTION("Pendulum mode") {
        // Direction reverses, pulse count applies in both directions
    }

    SECTION("PingPong mode") {
        // Endpoints repeat, pulse count applies
    }

    SECTION("Random mode") {
        // Random step selection, selected step respects pulse count
    }

    SECTION("RandomWalk mode") {
        // Walk ±1 step, pulse count applies
    }
}
```

**Expected:** All modes work correctly

**GREEN:** Test and verify

---

### Test 3.4: Track Linking

**Property:** Linked tracks share pulse counter state

**RED - Write Test:**
```cpp
TEST_CASE("Pulse count with linked tracks", "[pulse-count][integration]") {
    SECTION("Linked track follows master pulse count") {
        // Track 1: Master with various pulse counts
        // Track 2: Linked to Track 1
        // Expected: Track 2 advances when Track 1 advances
    }
}
```

**Expected:** May need special handling for linked tracks

**GREEN:** Implement link data sharing if needed

---

## Phase 4: UI Tests

### Test 4.1: Layer Cycling

**Manual Test:** (Automated UI testing is complex)

**Test Steps:**
1. Build simulator
2. Create note sequence
3. Press Length button (F2) repeatedly
4. Verify cycling: Length → LengthVariationRange → LengthVariationProbability → PulseCount → Length

**Acceptance Criteria:**
- ✅ PulseCount layer appears in cycle
- ✅ Layer name displays as "PULSE CNT"
- ✅ Function key lights up correctly

---

### Test 4.2: Step Value Display

**Manual Test:**

**Test Steps:**
1. Navigate to PulseCount layer
2. Set steps to different pulse counts (0-7)
3. Verify display shows values 1-8 (not 0-7)

**Acceptance Criteria:**
- ✅ Display shows 1-8 (user-facing values)
- ✅ Internal storage is 0-7 (matches UnsignedValue<3>)
- ✅ Visual feedback clear and readable

---

### Test 4.3: Encoder Editing

**Manual Test:**

**Test Steps:**
1. Navigate to PulseCount layer
2. Select a step
3. Turn encoder left/right
4. Verify value changes correctly
5. Test boundary conditions (0→7 wraps, 7→0 wraps)

**Acceptance Criteria:**
- ✅ Encoder changes values
- ✅ Values clamp at boundaries
- ✅ Visual updates in real-time

---

### Test 4.4: Button Editing

**Manual Test:**

**Test Steps:**
1. Navigate to PulseCount layer
2. Press S1-S16 buttons
3. Verify each button controls corresponding step
4. Hold button + encoder to edit value

**Acceptance Criteria:**
- ✅ Buttons select correct steps
- ✅ Step values edit independently
- ✅ No interference with other layers

---

## Phase 5: Acceptance Tests

### Acceptance Test 1: Variable Step Lengths

**Scenario:** Create a pattern with varying step durations

**Setup:**
```
Pattern: 4 steps
Pulse counts: [1, 2, 3, 4]
Divisor: 1/8 note
```

**Expected Result:**
```
Step 1: 1 × 1/8 = 1/8 duration
Step 2: 2 × 1/8 = 2/8 duration
Step 3: 3 × 1/8 = 3/8 duration
Step 4: 4 × 1/8 = 4/8 duration
Total: 10/8 pattern length
```

**Test:**
1. Program pattern as specified
2. Start playback
3. Use oscilloscope to measure gate timing
4. Verify each step duration matches expected

**Acceptance Criteria:**
- ✅ Step 1 plays for 1 divisor period
- ✅ Step 2 plays for 2 divisor periods
- ✅ Step 3 plays for 3 divisor periods
- ✅ Step 4 plays for 4 divisor periods
- ✅ Pattern loops correctly

---

### Acceptance Test 2: Pulse Count + Retrigger

**Scenario:** Combine pulse count with retrigger

**Setup:**
```
Step: pulseCount=3, retrigger=2 (3 triggers)
Divisor: 1/16 note
```

**Expected Result:**
```
Pulse 1: Trigger, wait, trigger, wait, trigger (3 total)
Pulse 2: Trigger, wait, trigger, wait, trigger (3 total)
Pulse 3: Trigger, wait, trigger, wait, trigger (3 total)
Then advance to next step
```

**Test:**
1. Program step as specified
2. Monitor gate output
3. Count triggers per pulse
4. Verify step duration

**Acceptance Criteria:**
- ✅ Each pulse has 3 triggers
- ✅ Step plays for 3 pulses total
- ✅ 9 triggers total before advancing
- ✅ Timing is accurate

---

### Acceptance Test 3: Backwards Compatibility

**Scenario:** Load old project without pulse count

**Setup:**
1. Create project in current firmware
2. Save project
3. Load project in new firmware with pulse count

**Expected Result:**
- All steps have pulseCount=0 (default)
- Sequence behavior unchanged
- No corruption or errors

**Test:**
1. Load old project file
2. Verify all step data intact
3. Play sequence
4. Confirm timing identical to original

**Acceptance Criteria:**
- ✅ Project loads without errors
- ✅ All step data preserved
- ✅ Default pulse count = 0
- ✅ Playback identical to before

---

### Acceptance Test 4: All RunModes

**Scenario:** Test pulse count with each RunMode

**Setup:**
```
Pattern: 4 steps
Pulse counts: [2, 1, 3, 1]
```

**Tests:**

**Forward:**
- Steps play in order: 0→1→2→3→0
- Each respects pulse count

**Backward:**
- Steps play in reverse: 3→2→1→0→3
- Each respects pulse count

**Pendulum:**
- Forward then backward: 0→1→2→3→2→1→0
- Pulse count applies both directions

**PingPong:**
- Repeats endpoints: 0→1→2→3→3→2→1→0→0
- Pulse count applies to repeated steps

**Random:**
- Steps selected randomly
- Selected step respects its pulse count

**RandomWalk:**
- Steps advance ±1
- Each step respects pulse count

**Acceptance Criteria:**
- ✅ All modes function correctly
- ✅ Pulse count respected in all modes
- ✅ Direction changes work properly

---

### Acceptance Test 5: Reset Measure

**Scenario:** Reset measure interrupts pulse count

**Setup:**
```
Step: pulseCount=5 (long step)
Reset measure: 1 bar
Divisor: 1/8 note
```

**Expected:**
- Step starts at measure beginning
- If reset occurs during pulse 3, sequence resets
- Pulse counter clears
- Step restarts from beginning

**Test:**
1. Program long step (pulseCount=5)
2. Set reset measure
3. Observe behavior when reset triggers

**Acceptance Criteria:**
- ✅ Reset clears pulse counter
- ✅ Sequence restarts correctly
- ✅ No glitches or stuck states

---

### Acceptance Test 6: CPU Performance

**Scenario:** Verify no performance degradation

**Setup:**
- 8 tracks, all with varying pulse counts
- Complex patterns with retriggers
- Monitor profiler task

**Test:**
1. Run all 8 tracks simultaneously
2. Check CPU usage via profiler
3. Listen for audio glitches
4. Monitor OLED noise (display updates)

**Acceptance Criteria:**
- ✅ CPU usage within normal range
- ✅ No audio glitches
- ✅ OLED noise unchanged
- ✅ Engine timing stable

---

### Acceptance Test 7: Hardware Timing Verification

**Scenario:** Measure actual timing on hardware

**Equipment:**
- Oscilloscope
- PEW|FORMER hardware
- MIDI clock source

**Test:**
1. Program pattern with known pulse counts
2. Sync to MIDI clock
3. Measure gate output timing
4. Compare to expected values

**Example:**
```
MIDI clock: 120 BPM
Divisor: 1/16 note
Expected duration: 125ms per pulse
Step pulseCount=3: Should be 375ms
```

**Acceptance Criteria:**
- ✅ Timing matches calculations
- ✅ No drift over time
- ✅ Jitter within acceptable range (<1ms)
- ✅ Consistent across all tracks

---

## Implementation Checklist

### Pre-Implementation
- [ ] Set up test file structure
- [ ] Verify test framework builds
- [ ] Review existing test patterns
- [ ] Plan test data fixtures

### Phase 1: Model Layer (TDD)
- [ ] **Test 1.1:** Basic storage/retrieval ✓
  - [ ] Write test (RED)
  - [ ] Implement pulseCount() (GREEN)
  - [ ] Refactor if needed
- [ ] **Test 1.2:** Value clamping ✓
  - [ ] Write test (RED)
  - [ ] Verify clamping works (GREEN)
- [ ] **Test 1.3:** Bitfield packing ✓
  - [ ] Write test (RED)
  - [ ] Verify no interference (GREEN)
- [ ] **Test 1.4:** Layer integration ✓
  - [ ] Write test (RED)
  - [ ] Add Layer enum (GREEN)
  - [ ] Add layerName (GREEN)
- [ ] **Test 1.5:** Layer value access ✓
  - [ ] Write test (RED)
  - [ ] Implement layerValue case (GREEN)
  - [ ] Implement setLayerValue case (GREEN)
- [ ] **Test 1.6:** Serialization ✓
  - [ ] Write test (RED)
  - [ ] Verify auto-serialization (GREEN)
- [ ] **Test 1.7:** Clear behavior ✓
  - [ ] Write test (RED)
  - [ ] Verify clear() works (GREEN)

### Phase 2: Engine Layer (TDD)
- [ ] **Test 2.1:** Pulse counter state ✓
  - [ ] Write test (RED)
  - [ ] Add _stepPulseCounter (GREEN)
  - [ ] Update reset() and restart() (GREEN)
- [ ] **Test 2.2:** Single pulse behavior ✓
  - [ ] Write property test
  - [ ] Verify compatibility (GREEN)
- [ ] **Test 2.3:** Multi-pulse behavior ✓
  - [ ] Write test specification (RED)
  - [ ] Implement Aligned mode logic (GREEN)
  - [ ] Refactor if needed
- [ ] **Test 2.4:** Free mode support ✓
  - [ ] Write test (RED)
  - [ ] Implement Free mode logic (GREEN)
  - [ ] Refactor common code
- [ ] **Test 2.5:** Reset measure ✓
  - [ ] Write test (RED)
  - [ ] Verify reset behavior (GREEN)

### Phase 3: Integration Tests
- [ ] **Test 3.1:** Pattern timing ✓
  - [ ] Create test harness
  - [ ] Write timing test (RED)
  - [ ] Verify correct timing (GREEN)
- [ ] **Test 3.2:** Pulse + Retrigger ✓
  - [ ] Write combined test (RED)
  - [ ] Verify compatibility (GREEN)
- [ ] **Test 3.3:** All RunModes ✓
  - [ ] Test each mode (RED)
  - [ ] Verify all work (GREEN)
- [ ] **Test 3.4:** Track linking ✓
  - [ ] Write link test (RED)
  - [ ] Implement if needed (GREEN)

### Phase 4: UI Implementation
- [ ] Add PulseCount to button cycling
- [ ] Implement layer rendering
- [ ] Test encoder editing (manual)
- [ ] Test button editing (manual)
- [ ] Verify visual feedback

### Phase 5: Acceptance Testing
- [ ] **AT1:** Variable step lengths
- [ ] **AT2:** Pulse + Retrigger
- [ ] **AT3:** Backwards compatibility
- [ ] **AT4:** All RunModes
- [ ] **AT5:** Reset measure
- [ ] **AT6:** CPU performance
- [ ] **AT7:** Hardware timing

### Documentation
- [ ] Update CLAUDE.md with pulse count
- [ ] Update user manual
- [ ] Create tutorial examples
- [ ] Document edge cases

### Final Verification
- [ ] All unit tests pass
- [ ] All integration tests pass
- [ ] All acceptance tests pass
- [ ] Code review complete
- [ ] Performance verified
- [ ] Hardware tested
- [ ] Documentation complete

---

## Test Execution Commands

### Build Tests
```bash
cd build/sim/debug
make -j
```

### Run All Pulse Count Tests
```bash
./src/tests/unit/unit_tests "[pulse-count]"
```

### Run Specific Test Categories
```bash
# Model layer only
./src/tests/unit/unit_tests "[pulse-count][model]"

# Engine layer only
./src/tests/unit/unit_tests "[pulse-count][engine]"

# Integration tests only
./src/tests/unit/unit_tests "[pulse-count][integration]"
```

### Run Specific Test Case
```bash
./src/tests/unit/unit_tests "Step stores pulse count"
```

### Run with Verbose Output
```bash
./src/tests/unit/unit_tests "[pulse-count]" -s
```

---

## Success Criteria

### Code Quality
- [ ] All tests pass (100% pass rate)
- [ ] No compiler warnings
- [ ] No memory leaks (valgrind clean)
- [ ] Code coverage >90% for new code

### Functionality
- [ ] Feature works as specified
- [ ] No regression in existing features
- [ ] Backwards compatible with old projects
- [ ] Performance impact negligible

### User Experience
- [ ] UI intuitive and consistent
- [ ] Documentation clear and complete
- [ ] Examples provided
- [ ] No confusion with existing features

---

## Notes on TDD Process

### Benefits of TDD for This Feature

1. **Confidence:** Tests prove each component works before integration
2. **Design:** Tests force clear API design
3. **Regression:** Tests catch breaking changes
4. **Documentation:** Tests show how to use the feature

### Tips for Success

1. **Write smallest possible test first**
   - Don't try to test everything at once
   - One assertion per test when possible

2. **Red-Green-Refactor discipline**
   - Must see test fail before making it pass
   - Proves test is actually testing something
   - Refactor only when tests are green

3. **Test behavior, not implementation**
   - Test what the code does, not how
   - Allows refactoring without breaking tests

4. **Integration tests last**
   - Unit test components individually first
   - Integration tests verify components work together
   - Acceptance tests verify user-facing behavior

---

**Document Version:** 1.0
**Author:** Test-Driven Implementation Plan
**Status:** Ready to Execute
**Estimated Time:** 2-3 days with TDD approach
