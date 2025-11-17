# Metropolix-Style Pulse Count Implementation Plan

**Feature:** Per-step pulse count (1-8) - step repeats for N clock pulses before advancing

**Reference:** Intellijel Metropolix pulse count behavior

**Status:** Ready for implementation

---

## Feature Overview

### What It Does

**Metropolix Pulse Count:**
```
Step 1 (pulseCount=3): Plays for 3 divisor periods before advancing
Step 2 (pulseCount=1): Plays for 1 divisor period (normal)
Step 3 (pulseCount=2): Plays for 2 divisor periods before advancing
```

**Musical Example:**
```
Pattern: A B C D (4 steps, divisor=1/8)
Pulse Counts: 1 3 1 2

Result:
A (1× 1/8 = 1/8)
B (3× 1/8 = 3/8)
C (1× 1/8 = 1/8)
D (2× 1/8 = 2/8)
Total: 7/8 pattern length
```

This creates **variable-length steps** without changing global divisor!

### Difference from Retrigger

**Retrigger (current):** Subdivides step time
```
Step with retrigger=4: ♪♪♪♪ (4 triggers in 1 step time)
Step advances after 1 divisor period
```

**Pulse Count (new):** Extends step duration
```
Step with pulseCount=4: ♩━━━━ (1 trigger, 4 step times)
Step advances after 4 divisor periods
```

**Both features can coexist!**
- Retrigger: Rhythmic subdivision within a step
- Pulse Count: Variable step duration

---

## Architecture Analysis

### Current Timing Flow

```cpp
// NoteTrackEngine::tick() - simplified
uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
uint32_t relativeTick = tick % resetDivisor;

// Step advances every divisor period
if (relativeTick % divisor == 0) {
    _sequenceState.advanceAligned(relativeTick / divisor, ...);
    triggerStep(tick, divisor);
}
```

**Current behavior:** Step advances every time `relativeTick % divisor == 0`

### Required Changes

**New behavior:** Step advances only after N divisor periods (where N = pulseCount)

```cpp
// New: Track pulse counter per step
int _stepPulseCounter = 0;  // Current pulse within step

if (relativeTick % divisor == 0) {
    _stepPulseCounter++;

    int pulseCount = _sequence->step(_sequenceState.step()).pulseCount() + 1;  // 0-7 → 1-8

    if (_stepPulseCounter >= pulseCount) {
        // Advance to next step
        _sequenceState.advance(...);
        _stepPulseCounter = 0;  // Reset counter
        triggerStep(tick, divisor);
    } else {
        // Stay on current step
        // Optionally re-trigger (pulse retrigger)
    }
}
```

---

## Implementation Steps

### Phase 1: Model Layer (1-2 hours)

**File:** `src/apps/sequencer/model/NoteSequence.h`

#### Step 1.1: Add Type Definition
```cpp
// Line 36 - Add with other type definitions
using PulseCount = UnsignedValue<3>;  // 0-7 representing 1-8 pulses
```

#### Step 1.2: Add Layer Enum
```cpp
// Line 54 - Add before Last
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
```

#### Step 1.3: Add Layer Name
```cpp
// Line 73 - Add in layerName() switch
case Layer::PulseCount:         return "PULSE CNT";
```

#### Step 1.4: Add to Step Class
```cpp
// After line 188 - Add pulseCount accessors
// pulseCount

int pulseCount() const { return _data1.pulseCount; }
void setPulseCount(int pulseCount) {
    _data1.pulseCount = PulseCount::clamp(pulseCount);
}
```

#### Step 1.5: Add BitField to _data1
```cpp
// Line 228 - Add to _data1 union
BitField<uint32_t, 16, 1> isAccumulatorTrigger;
BitField<uint32_t, 17, PulseCount::Bits> pulseCount;  // NEW: bits 17-19
// 12 bits left (was 15)
```

#### Step 1.6: Add to layerValue() and setLayerValue()
**File:** `src/apps/sequencer/model/NoteSequence.cpp` (find existing implementation)

```cpp
// In layerValue()
case Layer::PulseCount:
    return step.pulseCount();

// In setLayerValue()
case Layer::PulseCount:
    step.setPulseCount(value);
    break;
```

#### Step 1.7: Add to layerRange() and layerDefaultValue()
```cpp
// In layerRange()
case Layer::PulseCount:
    return { 0, PulseCount::Max };

// In layerDefaultValue()
case Layer::PulseCount:
    return 0;  // Default = 1 pulse (0 stored = 1 actual)
```

---

### Phase 2: Engine Layer (2-3 hours)

**File:** `src/apps/sequencer/engine/NoteTrackEngine.h`

#### Step 2.1: Add Pulse Counter State
```cpp
// Add after line 94 (other state variables)
private:
    int _stepPulseCounter = 0;  // Counts pulses within current step
```

#### Step 2.2: Reset Counter
```cpp
// In reset() method (around line 91)
void NoteTrackEngine::reset() {
    _freeRelativeTick = 0;
    _sequenceState.reset();
    _currentStep = -1;
    _stepPulseCounter = 0;  // NEW: Reset pulse counter
    // ... existing code
}

// In restart() method (around line 108)
void NoteTrackEngine::restart() {
    _freeRelativeTick = 0;
    _sequenceState.reset();
    _currentStep = -1;
    _stepPulseCounter = 0;  // NEW: Reset pulse counter
}
```

**File:** `src/apps/sequencer/engine/NoteTrackEngine.cpp`

#### Step 2.3: Modify tick() Logic
```cpp
// Around line 138-159 - REPLACE the sequence advance logic

// Aligned mode
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
        } else {
            // Stay on current step
            // Optional: Re-trigger on subsequent pulses
            // triggerStep(tick, divisor);  // Uncomment for pulse retriggering
        }
    }
    break;

// Free mode
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

#### Step 2.4: Handle Reset Measure
```cpp
// Around line 133 - When reset occurs
if (relativeTick == 0) {
    reset();  // This already resets _stepPulseCounter
}
```

---

### Phase 3: UI Layer (2-3 hours)

#### Step 3.1: Add Layer Cycling

**File:** `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp`

**Option A: Add to existing button (Length or Retrigger)**
```cpp
// In switchLayer() - add PulseCount to cycling
// Suggested: Add to Length button (F2) cycling

case FunctionKey::Length:
    switch (_layer) {
    case Layer::Length:
        _layer = Layer::LengthVariationRange;
        break;
    case Layer::LengthVariationRange:
        _layer = Layer::LengthVariationProbability;
        break;
    case Layer::LengthVariationProbability:
        _layer = Layer::PulseCount;  // NEW
        break;
    case Layer::PulseCount:  // NEW
        _layer = Layer::Length;
        break;
    default:
        _layer = Layer::Length;
        break;
    }
    break;
```

**Option B: Add to Retrigger button (if more related)**
```cpp
case FunctionKey::Retrigger:  // Or whatever the retrigger button is
    // Add PulseCount to retrigger cycling
    // (similar pattern to above)
```

#### Step 3.2: Add Visual Feedback

**File:** `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp`

```cpp
// In drawLayer() or similar rendering method
// Display step pulse count values (1-8) when layer is active
case Layer::PulseCount:
    for (int stepIndex = 0; stepIndex < viewSteps; ++stepIndex) {
        const auto &step = _sequence.step(stepIndex);
        int value = step.pulseCount() + 1;  // Display as 1-8
        // Draw value on screen at step position
    }
    break;
```

#### Step 3.3: Update activeFunctionKey()
```cpp
// Map PulseCount layer to appropriate function key
case Layer::PulseCount:
    return FunctionKey::Length;  // Or whichever button you chose
```

---

### Phase 4: Testing (2-3 hours)

#### Unit Tests

**File:** `src/tests/unit/sequencer/TestPulseCount.cpp` (new file)

```cpp
#include "catch.hpp"
#include "model/NoteSequence.h"
#include "engine/NoteTrackEngine.h"

TEST_CASE("PulseCount basic functionality") {
    NoteSequence sequence;

    SECTION("Default pulse count is 1") {
        auto step = sequence.step(0);
        REQUIRE(step.pulseCount() == 0);  // Stored as 0, means 1 pulse
    }

    SECTION("Pulse count clamps to 0-7") {
        auto step = sequence.step(0);
        step.setPulseCount(10);  // Try to set out of range
        REQUIRE(step.pulseCount() == 7);  // Should clamp to 7
    }

    SECTION("Step duration extends with pulse count") {
        // Test that step plays for pulseCount × divisor ticks
        // TODO: Implement test with mock engine
    }
}

TEST_CASE("PulseCount serialization") {
    NoteSequence sequence;

    sequence.step(0).setPulseCount(3);
    sequence.step(1).setPulseCount(7);

    // Test save/load preserves pulse count
    // TODO: Implement serialization test
}

TEST_CASE("PulseCount interaction with other features") {
    SECTION("PulseCount works with Retrigger") {
        // Step with pulseCount=2, retrigger=3
        // Should play for 2 divisor periods, each with 3 triggers
    }

    SECTION("PulseCount respects reset measure") {
        // Reset should clear pulse counter
    }
}
```

#### Integration Tests

**Simulator Testing:**
```bash
cd build/sim/debug
./src/apps/sequencer/sequencer
```

**Test Scenarios:**
1. **Basic pulse count:**
   - Set steps with different pulse counts (1, 2, 4, 8)
   - Verify timing with visual/audio feedback

2. **Pattern length calculation:**
   - 4-step pattern: pulseCount = [1, 2, 3, 4]
   - Total pattern = 1+2+3+4 = 10 divisor periods

3. **Interaction with other features:**
   - Pulse count + Retrigger
   - Pulse count + RunMode (Forward, Backward, Pendulum)
   - Pulse count + Reset Measure

4. **Edge cases:**
   - All steps pulseCount=1 (normal behavior)
   - All steps pulseCount=8 (very long)
   - Changing pulse count while playing

#### Hardware Testing

**Critical for accurate timing:**
1. Measure actual step duration with oscilloscope
2. Verify gate timing is correct
3. Test with external MIDI clock sync
4. Check CPU usage (profiler task)

---

## UI Design Considerations

### Display Options

**Option 1: Numeric Display (1-8)**
```
Step:  1    2    3    4    5    6    7    8
Pulse: 1    2    4    1    3    1    2    1
```

**Option 2: Bar Graph**
```
Step:  1    2    3    4    5    6    7    8
Pulse: |    ||   ||||  |   |||   |    ||   |
```

**Option 3: Combined (Recommended)**
- Show number when editing
- Show bar graph for overview

### Button Mapping

**Recommendation:** Add to **Length** button (F2) cycling

**Rationale:**
- Length and PulseCount both control step duration
- Conceptually related (Length = gate duration, PulseCount = step duration)
- Keeps timing-related parameters together

**Cycling:**
```
Length → LengthVariationRange → LengthVariationProbability → PulseCount → Length
```

**Alternative:** Add to **Retrigger** button
- Retrigger and PulseCount are complementary
- Both affect how steps repeat
- Keeps repetition features together

---

## Feature Interactions

### Pulse Count + Retrigger

**Combined behavior:**
```
Step: pulseCount=3, retrigger=2 (means 3 retrigs per pulse)
Result: Step plays for 3 divisor periods, each pulse has 3 triggers

Timeline (divisor = 1/16):
Pulse 1: ♪♪♪ (3 triggers)
Pulse 2: ♪♪♪ (3 triggers)
Pulse 3: ♪♪♪ (3 triggers)
Then advance to next step
```

### Pulse Count + Pattern Length

**Pattern timing calculation:**
```cpp
// Total pattern duration = sum of all step pulse counts × divisor
int totalPatternTicks = 0;
for (int i = firstStep; i <= lastStep; i++) {
    int pulseCount = sequence.step(i).pulseCount() + 1;
    totalPatternTicks += pulseCount * divisor;
}
```

### Pulse Count + Reset Measure

**Interaction:**
- Reset measure still triggers at measure boundaries
- Pulse counter resets when pattern resets
- Step might be cut short if reset occurs mid-pulse

**Handling:**
```cpp
if (relativeTick == 0) {
    reset();  // Resets _stepPulseCounter
    // Step advances regardless of pulse count
}
```

### Pulse Count + RunMode

**Forward/Backward:**
- Pulse count applies to each step regardless of direction
- Step 3 with pulseCount=4 plays for 4 pulses whether advancing or reversing

**Pendulum/PingPong:**
- Pulse count applies on both forward and backward passes
- Same step duration in both directions

**Random:**
- Pulse count still applies to randomly selected step
- Step duration determined by its pulse count value

---

## Optional Features

### Feature 1: Pulse Retrigger

**Concept:** Re-trigger step on each pulse (not just first)

**Implementation:**
```cpp
if (_stepPulseCounter >= pulseCount) {
    // Advance to next step
    _sequenceState.advance(...);
    _stepPulseCounter = 0;
    triggerStep(tick, divisor);  // First pulse of new step
} else {
    // Stay on current step
    if (PULSE_RETRIGGER_ENABLED) {  // Configurable option
        triggerStep(tick, divisor);  // Re-trigger on subsequent pulses
    }
}
```

**Use Case:**
- Step with pulseCount=4 fires 4 separate gate triggers
- Different from Retrigger (which subdivides)
- Creates repeating notes at divisor rate

**UI:** Add as track-level parameter or per-step flag

### Feature 2: Pulse Count Probability

**Concept:** Randomize pulse count per iteration

**Implementation:**
```cpp
// Add to NoteSequence::Step
int pulseCountVariationRange() const;  // -7 to +7
int pulseCountVariationProbability() const;  // 0-7

// In NoteTrackEngine::tick()
int basePulseCount = currentStep.pulseCount() + 1;
int variation = evalPulseCountVariation(currentStep);  // Apply probability
int actualPulseCount = clamp(basePulseCount + variation, 1, 8);
```

**Use Case:**
- Step sometimes plays longer/shorter
- Creates organic timing variations
- Generative rhythms

---

## Performance Considerations

### Memory Impact

**Per-Step Addition:**
- 3 bits per step (PulseCount)
- 64 steps × 3 bits = 192 bits = 24 bytes per sequence
- 8 sequences × 24 bytes = 192 bytes per pattern
- Negligible impact on STM32F4 (192KB RAM total)

**Per-Track Addition:**
- 1 int (4 bytes) for _stepPulseCounter
- 8 tracks × 4 bytes = 32 bytes total
- Negligible impact

### CPU Impact

**Per Tick:**
- 1 additional comparison: `if (_stepPulseCounter >= pulseCount)`
- 1 increment: `_stepPulseCounter++`
- 1 addition: `pulseCount + 1` (convert storage to actual)
- **Total:** ~3 CPU cycles per tick per track

**Estimate:**
- 8 tracks × 3 cycles = 24 cycles per engine tick
- STM32F4 @ 168MHz: 0.00014% CPU usage
- **Impact:** Negligible

### Timing Accuracy

**No change to timing precision:**
- Still uses global tick counter
- Still respects divisor timing
- Pulse count just controls when to advance
- No additional jitter introduced

---

## Migration & Compatibility

### Existing Projects

**Backwards Compatibility:**
- Default pulse count = 0 (stored) = 1 pulse (actual)
- Existing projects load with all steps pulseCount=0
- Behavior identical to current implementation
- No project file migration needed

**Serialization:**
- Pulse count stored in Step _data1 bitfield
- Saved/loaded with existing Step serialization
- Version number can be incremented if desired

### UI Compatibility

**New Layer:**
- Adds Layer::PulseCount to existing layers
- Optional - user can ignore if not interested
- No impact on existing workflows

---

## Documentation Needed

### User Manual Updates

**Section: "Step Programming"**
- Add pulse count explanation
- Diagrams showing pulse count vs retrigger
- Examples of variable step lengths

**Section: "Timing & Sync"**
- How pulse count affects pattern length
- Interaction with reset measure
- Calculating total pattern duration

### Tutorial Video Ideas

1. **"Variable Step Lengths with Pulse Count"**
   - Basic pulse count usage
   - Creating polymetric patterns
   - Pulse count + retrigger combinations

2. **"Metropolix-Style Sequencing"**
   - Emulating Metropolix workflows
   - Complex pattern timing
   - Live performance techniques

---

## Implementation Checklist

### Phase 1: Model (1-2 hours)
- [ ] Add `PulseCount` type definition
- [ ] Add `PulseCount` to `Layer` enum
- [ ] Add layer name in `layerName()`
- [ ] Add `pulseCount()` / `setPulseCount()` to Step class
- [ ] Add BitField to `_data1` union
- [ ] Implement `layerValue()` case
- [ ] Implement `setLayerValue()` case
- [ ] Implement `layerRange()` case
- [ ] Implement `layerDefaultValue()` case
- [ ] Test step data packing (verify no conflicts)

### Phase 2: Engine (2-3 hours)
- [ ] Add `_stepPulseCounter` to NoteTrackEngine
- [ ] Reset counter in `reset()`
- [ ] Reset counter in `restart()`
- [ ] Modify `tick()` Aligned mode logic
- [ ] Modify `tick()` Free mode logic
- [ ] Test timing accuracy in simulator
- [ ] Verify reset measure interaction
- [ ] Test with different RunModes

### Phase 3: UI (2-3 hours)
- [ ] Add PulseCount to button cycling (decide which button)
- [ ] Implement `drawLayer()` rendering
- [ ] Update `activeFunctionKey()` mapping
- [ ] Test UI navigation
- [ ] Verify encoder editing works
- [ ] Test S1-S16 button step editing

### Phase 4: Testing (2-3 hours)
- [ ] Write unit tests (TestPulseCount.cpp)
- [ ] Simulator integration tests
- [ ] Hardware timing verification
- [ ] Test feature interactions (retrigger, reset, etc.)
- [ ] Test backwards compatibility (load old projects)
- [ ] Performance profiling

### Phase 5: Documentation (1-2 hours)
- [ ] Update CLAUDE.md with pulse count info
- [ ] Add to user manual
- [ ] Create example patterns
- [ ] Update FEATURE_EVALUATION.md

---

## Estimated Total Effort

**Development:** 8-11 hours
**Testing:** 2-3 hours
**Documentation:** 1-2 hours

**Total:** 11-16 hours (~2 days)

---

## Risks & Mitigation

### Risk 1: Timing Complexity
**Issue:** Interaction with reset measure, linked tracks
**Mitigation:**
- Test extensively with all RunModes
- Verify pulse counter resets properly
- Document edge cases

### Risk 2: UI Confusion
**Issue:** Users confuse pulse count with retrigger
**Mitigation:**
- Clear naming ("PULSE CNT" vs "RETRIG")
- Documentation with diagrams
- Tutorial videos

### Risk 3: Pattern Length Calculation
**Issue:** Total pattern length becomes unpredictable
**Mitigation:**
- Add UI display of total pattern duration
- Visual feedback showing current position
- Allow reset measure to force boundaries

---

## Recommendation

**GO FOR IT!** This feature:
- ✅ Fits naturally into existing architecture
- ✅ Minimal memory/CPU impact
- ✅ Well-defined behavior (Metropolix reference)
- ✅ Complements existing features (retrigger)
- ✅ Backwards compatible
- ✅ Reasonable implementation effort

**Suggested approach:**
1. Start with Phase 1 (Model) - quickest to implement
2. Test in simulator with Phase 2 (Engine)
3. Add minimal UI in Phase 3 (just numeric display)
4. Validate on hardware
5. Polish UI and add optional features

This is a **high-value feature** that significantly expands the sequencer's rhythmic capabilities!

---

**Document Version:** 1.0
**Author:** Claude (Technical Analysis)
**Status:** Ready for Implementation
**Estimated Completion:** 2 days
