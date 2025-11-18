# MEBITEK Fork Research - Retrigger Implementation Analysis

**Repository**: https://github.com/mebitek/performer
**Research Date**: 2025-11-18
**Purpose**: Understand how pulse counts, gate modes, and retriggers are implemented in mebitek's performer fork

---

## Overview

The mebitek fork is a community-maintained version of the original PER|FORMER eurorack sequencer with developer-focused improvements. The repository emphasizes logic operators, launchpad integration, and stochastic features.

**Key Finding**: The mebitek fork **DOES implement pulse count equivalent** via `stageRepeats` + `stageRepeatMode`! In fact, mebitek offers MORE gate pattern options (8 modes) compared to PEW|FORMER's gate mode (4 modes).

---

## Architecture Comparison

### Gate Queue Structure

**mebitek/performer**:
```cpp
// Location: src/apps/sequencer/engine/NoteTrackEngine.h
struct Gate {
    uint32_t tick;    // Timestamp when gate fires
    bool gate;        // Gate value (true=ON, false=OFF)
};

SortedQueue<Gate, 16, GateCompare> _gateQueue;
```

**PEW|FORMER** (current):
```cpp
struct Gate {
    uint32_t tick;
    bool gate;
    // No additional metadata
};
```

**Key Observation**: Both use minimal Gate struct with only tick timestamp and boolean value. No metadata for accumulator ticks, pulse count, or gate mode.

---

## Step-Level Parameters

**mebitek NoteSequence::Step** (from model/NoteSequence.h):

### Gate & Timing
- `gate` - Boolean flag for note triggering
- `gateProbability` - 4-bit unsigned (probability of gate occurrence)
- `gateOffset` - 4-bit signed (timing offset for gate)
- `slide` - Boolean (portamento effect)

### Retrigger/Ratchet
- **`retrigger`** - 3-bit unsigned (retriggering count, 0-7)
- **`retriggerProbability`** - 4-bit unsigned (probability of retriggering)

### Note Parameters
- `note` - 7-bit signed (note pitch value)
- `noteVariationRange` - 7-bit signed (random note variation range)
- `noteVariationProbability` - 4-bit unsigned
- `bypassScale` - Boolean (ignore scale constraints)

### Length
- `length` - 4-bit unsigned (step duration)
- `lengthVariationRange` - 4-bit signed (random length variation)
- `lengthVariationProbability` - 4-bit unsigned

### Sequencing
- `stageRepeats` - 3-bit unsigned (repeat count for step)
- `stageRepeatMode` - 3-bit unsigned (repeat behavior mode)
- `condition` - 7-bit unsigned (conditional execution criteria)

**What's Different Compared to PEW|FORMER**:
- ✅ HAS `stageRepeats` (equivalent to pulseCount)
- ✅ HAS `stageRepeatMode` with 8 options (vs PEW|FORMER's 4 gateMode options)
- ❌ NO accumulator system
- ✅ MORE repeat pattern variety (Odd/Even/Triplets/Middle/Last/Random)
- ❌ NO HOLD mode (continuous gate)

---

## Retrigger Implementation

**Location**: `src/apps/sequencer/engine/NoteTrackEngine.cpp`

### How Retriggers Work (mebitek)

1. **Evaluation Phase**:
   ```cpp
   int stepRetrigger = evalStepRetrigger(step, _noteTrack.retriggerProbabilityBias());
   ```
   - Determines how many times to retrigger based on step's retrigger value
   - Applies probability to randomize retrigger count

2. **Gate Scheduling Loop**:
   ```cpp
   if (stepRetrigger > 1) {
       uint32_t retriggerLength = divisor / stepRetrigger;
       uint32_t retriggerOffset = 0;

       while (stepRetrigger-- > 0 && retriggerOffset <= stepLength) {
           // Schedule GATE ON
           _gateQueue.pushReplace({
               Groove::applySwing(tick + gateOffset + retriggerOffset, swing()),
               true
           });

           // Schedule GATE OFF (halfway through retrigger segment)
           _gateQueue.pushReplace({
               Groove::applySwing(tick + gateOffset + retriggerOffset + retriggerLength / 2, swing()),
               false
           });

           retriggerOffset += retriggerLength;
       }
   }
   ```

3. **Key Characteristics**:
   - Retriggers are **evenly spaced** within the step duration
   - Each retrigger gets: `retriggerLength = divisor / stepRetrigger`
   - Gate OFF happens at **halfway point** of each retrigger segment
   - Uses `Groove::applySwing()` for timing adjustments
   - All gates scheduled with future timestamps (queue-based)

### Example: retrig=3, divisor=48

```
stepRetrigger = 3
retriggerLength = 48 / 3 = 16 ticks

Scheduled Gates:
- Tick 0:  Gate ON
- Tick 8:  Gate OFF (16/2)
- Tick 16: Gate ON
- Tick 24: Gate OFF
- Tick 32: Gate ON
- Tick 40: Gate OFF
```

### Behavior Documented in Manual

From web search results:

> "The Retrigger layer creates ratcheting effects which output a burst of gates when a note is played. Up to 8 ratchets can be sent per step, with configurable probability. All ratchets are sent within the length of the specific step and do not overlap into the next step."

> "Retriggered notes are only output within the current Length of the step, allowing output of a burst of notes only at the beginning of the step."

---

## Stage Repeats Implementation (PULSE COUNT EQUIVALENT!)

**IMPORTANT DISCOVERY**: mebitek **DOES have pulse count** functionality via `stageRepeats` + `stageRepeatMode`!

### How Stage Repeats Work

**Location**: `src/apps/sequencer/engine/NoteTrackEngine.cpp` (tick method, Free playMode)

1. **Step Repetition Control**:
   ```cpp
   // Counter tracks which repetition (1 to N)
   _currentStageRepeat++;

   // Check if this is the last repetition
   bool isLastStageStep = ((int)(step.stageRepeats()+1) - (int)_currentStageRepeat) <= 0;

   // Only advance when all repetitions complete
   if (isLastStageStep) {
       _currentStageRepeat = 1;  // Reset for next step
       _sequenceState.advanceFree(...);  // Advance to next step
   }
   ```

2. **Gate Firing Control** (triggerStep method):
   ```cpp
   // Determine if gate should fire based on current repetition
   switch (step.stageRepeatMode()) {
       case Types::StageRepeatMode::Each:
           // Fire on every repetition (default)
           break;

       case Types::StageRepeatMode::First:
           stepGate = stepGate && (_currentStageRepeat == 1);
           break;

       case Types::StageRepeatMode::Last:
           stepGate = stepGate && (_currentStageRepeat == step.stageRepeats()+1);
           break;

       case Types::StageRepeatMode::Middle:
           stepGate = stepGate && (_currentStageRepeat == (step.stageRepeats()+1)/2);
           break;

       case Types::StageRepeatMode::Odd:
           stepGate = stepGate && (_currentStageRepeat % 2 != 0);
           break;

       case Types::StageRepeatMode::Even:
           stepGate = stepGate && (_currentStageRepeat % 2 == 0);
           break;

       case Types::StageRepeatMode::Triplets:
           stepGate = stepGate && ((_currentStageRepeat - 1) % 3 == 0);
           break;

       case Types::StageRepeatMode::Random:
           // Randomly selects from above modes
           break;
   }
   ```

### Stage Repeat Modes (Types::StageRepeatMode enum)

| Mode | Behavior |
|------|----------|
| **Each** | Gates fire on every repetition (like PEW\|FORMER ALL mode) |
| **First** | Gate fires only on first repetition (like FIRST mode) |
| **Last** | Gate fires only on final repetition |
| **Middle** | Gate fires on middle repetition |
| **Odd** | Gates fire on odd-numbered repetitions (1, 3, 5...) |
| **Even** | Gates fire on even-numbered repetitions (2, 4, 6...) |
| **Triplets** | Gates fire every 3rd repetition (1, 4, 7...) |
| **Random** | Randomly picks one of the above patterns |

### Example: stageRepeats=3, stageRepeatMode=Odd

```
Step configured with 3 repeats (stageRepeats=3)
Mode: Odd

Repetition 1: _currentStageRepeat=1 → Odd (1 % 2 != 0) → GATE FIRES ✓
Repetition 2: _currentStageRepeat=2 → Even (2 % 2 == 0) → NO GATE ✗
Repetition 3: _currentStageRepeat=3 → Odd (3 % 2 != 0) → GATE FIRES ✓
Repetition 4: _currentStageRepeat=4 (last) → Even → NO GATE ✗

Then advance to next step
```

### Comparison: mebitek vs PEW|FORMER

| Feature | mebitek | PEW\|FORMER |
|---------|---------|-------------|
| **Duration Control** | `stageRepeats` (0-7) | `pulseCount` (0-7 = 1-8 pulses) |
| **Counter Variable** | `_currentStageRepeat` | `_pulseCounter` |
| **Gate Pattern** | `stageRepeatMode` (8 modes) | `gateMode` (4 modes) |
| **Fire All** | `Each` | `ALL` |
| **Fire First** | `First` | `FIRST` |
| **Fire First+Last** | ❌ | `FIRSTLAST` |
| **Long Gate** | ❌ | `HOLD` |
| **Fire Last** | `Last` ✅ | ❌ |
| **Fire Middle** | `Middle` ✅ | ❌ |
| **Fire Odd** | `Odd` ✅ | ❌ |
| **Fire Even** | `Even` ✅ | ❌ |
| **Fire Triplets** | `Triplets` ✅ | ❌ |
| **Random** | `Random` ✅ | ❌ |

**Key Differences**:
- **mebitek**: More pattern variety (8 modes), geared toward rhythmic complexity
- **PEW|FORMER**: Simpler (4 modes), includes HOLD for sustained notes
- **Both**: Control step duration and selective gate firing

---

## Comparison with PEW|FORMER

### Similarities

1. **Gate Queue Architecture**:
   - Both use `SortedQueue<Gate, 16>` for timestamp-based event scheduling
   - Both use minimal Gate struct (tick + bool)
   - Both schedule gates with future timestamps

2. **Retrigger Logic**:
   - Both divide step into equal segments
   - Both use `evalStepRetrigger()` for probability
   - Both schedule multiple ON/OFF gate pairs in a loop

3. **Timing**:
   - Both apply swing via `Groove::applySwing()`
   - Both respect step length boundaries

### Differences

| Feature | mebitek/performer | PEW|FORMER |
|---------|-------------------|------------|
| **Pulse Count** | ✅ `stageRepeats` (0-7) | ✅ `pulseCount` (0-7 = 1-8 pulses) |
| **Gate Mode** | ✅ `stageRepeatMode` (8 modes: Each/First/Last/Middle/Odd/Even/Triplets/Random) | ✅ `gateMode` (4 modes: ALL/FIRST/HOLD/FIRSTLAST) |
| **Accumulator** | ❌ Not implemented | ✅ Full accumulator system with STEP/GATE/RTRIG modes |
| **Retrigger** | ✅ Basic (0-7 count) | ✅ Basic + accumulator integration |
| **Gate Struct** | Minimal (tick + bool) | Minimal (tick + bool) |
| **Max Retriggers** | 8 (3-bit) | 8 (3-bit) |
| **Max Stage Repeats** | 8 (3-bit) | 8 (3-bit pulseCount) |

### Architecture Insights

**mebitek's Approach**:
- Simpler, more straightforward implementation
- No extended gate metadata
- No separate tick queue for accumulators
- Focus on other features (logic operators, launchpad, stochastic)

**PEW|FORMER's Approach**:
- More complex feature set (pulse count + gate mode + accumulator)
- Same minimal gate struct (hasn't extended it yet)
- Accumulator ticks happen **immediately** (not queue-based)
- **Proposed experimental feature**: Extend Gate struct with metadata for spread ticks

---

## Relevant Features in mebitek

While mebitek lacks pulse count and gate mode, it has other interesting features:

### Release 0.2.2 (Latest)
- **Per-step gate logic operators** (Logic Track)
- Per-step note logic operators
- Current step CV routing (5ms response)
- Move step forward shortcuts

### Release 0.2.0
- Sequence library with fast switching
- Launchpad Performance Mode
- Multi Curve CV Recording
- **Gate length extended to 4 bits**
- Quick gate accent control

### Release 0.1.47
- Pattern follow functionality
- Pattern chain shortcuts
- Smart note preservation during scale changes
- Launchpad song mode integration

---

## Implementation Lessons

### What mebitek Teaches Us

1. **Simple Gate Queue Works Well**:
   - mebitek successfully implements retriggers with minimal Gate struct
   - No need for complex metadata if timing is all that matters

2. **Queue-Based Scheduling is Standard**:
   - All gate events scheduled with future timestamps
   - Single loop processes queue when `tick >= event.tick`

3. **Retrigger Spacing Formula**:
   - `retriggerLength = divisor / stepRetrigger` ensures even distribution
   - Gate OFF at `retriggerLength / 2` (50% duty cycle)

4. **Probability Integration**:
   - `evalStepRetrigger()` applies probability before scheduling
   - Simpler than per-retrigger probability

### Implications for PEW|FORMER Experimental Feature

**Single-Queue Approach (extends Gate struct)**:
- ✅ Proven architecture (mebitek uses it successfully)
- ✅ Simpler than two-queue approach
- ⚠️  Requires extending Gate struct (memory overhead)
- ⚠️  Need sequence ID (not pointer) for safety

**Two-Queue Approach (separate accumulator queue)**:
- ❌ Not used in mebitek
- ❌ More complex (two queues to manage)
- ✅ Cleaner separation of concerns

**Current PEW|FORMER (immediate ticks)**:
- ✅ Simplest implementation
- ✅ Zero memory overhead
- ❌ Ticks don't spread over time (burst mode)

---

## Code References

### mebitek Repository Files

**Model Layer**:
- `src/apps/sequencer/model/NoteSequence.h` - Step class definition
- `src/apps/sequencer/model/NoteSequence.cpp` - Step implementation

**Engine Layer**:
- `src/apps/sequencer/engine/NoteTrackEngine.h` - Gate struct, queue declaration
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - triggerStep() implementation

**Key Functions**:
- `NoteTrackEngine::triggerStep()` - Main step triggering logic
- `NoteTrackEngine::evalStepRetrigger()` - Probability evaluation
- `Groove::applySwing()` - Timing adjustment

---

## Conclusion

The mebitek fork demonstrates that:

1. **Retriggers work well** with simple queue-based scheduling and minimal Gate struct
2. **Pulse count equivalent (stageRepeats) is a proven feature** with even MORE pattern options than PEW|FORMER
3. **Queue-based event scheduling is the standard approach** for timing-critical features
4. **Extending the Gate struct** for accumulator metadata would be consistent with industry patterns
5. **More gate pattern variety is valuable** - mebitek's 8 modes offer greater rhythmic complexity

**For PEW|FORMER's experimental spread-tick feature**:
- Single-queue approach (extend Gate struct) aligns with mebitek's proven architecture
- Adding `shouldTickAccumulator` and `sequenceId` fields is a reasonable extension
- Using sequence ID instead of pointer avoids the main safety concern
- Memory overhead is acceptable (8-16 bytes per gate)

---

**Document Version**: 1.0
**Last Updated**: November 2025
**Project**: PEW|FORMER Feature Research
