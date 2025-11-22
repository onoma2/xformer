# Research: Retrigger Functionality in NoteTrack System

## Overview

This document explains the retrigger functionality in the NoteTrack system of the PEW|FORMER firmware, detailing how it works with respect to the master clock, steps, gate outputs, and other parameters.

## 1. Master Clock and Timing Relationship

The retrigger functionality is deeply integrated with the master clock system in the performer-phazer:

- **Clock Division**: Each sequence has a divisor that determines how often it advances relative to the master clock (PPQN = 192, Sequence PPQN = 48)
- **Step Timing**: When a sequence step is triggered, the retrigger functionality creates multiple gate pulses within the same step duration
- **Pulse Subdivision**: The retrigger count divides the step duration into smaller segments, creating multiple gate events per step

The timing is calculated as:
```
retriggerLength = divisor / stepRetrigger
```

For example, if a step has divisor=48 and retrigger=4, it creates 4 gate pulses each lasting 12 ticks (48/4) instead of one long gate lasting 48 ticks.

## 2. Interaction with Steps and Sequence Timing

The retrigger functionality interacts with steps in the following way:

- **Step Evaluation**: When `triggerStep()` is called, the engine evaluates the step's retrigger parameters:
  - `step.retrigger()`: The base retrigger count (0-3, representing 1-4 pulses)
  - `step.retriggerProbability()`: Probability that retrigger will occur (0-7)
  - `noteTrack.retriggerProbabilityBias()`: Global bias added to probability

- **Pulse Count Integration**: The retrigger system works alongside the pulse count feature:
  - `step.pulseCount()`: Number of pulses to fire before advancing to next step (0-7 representing 1-8 pulses)
  - `step.gateMode()`: Determines which pulses fire gates (All, First, Hold, FirstLast)

The engine maintains a `_pulseCounter` that tracks which pulse is currently being processed within a step.

## 3. Gate Output and Pulse Generation

The retrigger system affects gate outputs through the following mechanisms:

- **Gate Queue System**: Instead of a single gate on/off pair per step, the system creates multiple gate events in a sorted queue:
  ```cpp
  // For retrigger > 1, multiple gates are scheduled
  while (stepRetrigger-- > 0 && retriggerOffset <= stepLength) {
      _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + retriggerOffset, swing()), true });
      _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + retriggerOffset + retriggerLength / 2, swing()), false });
      retriggerOffset += retriggerLength;
  }
  ```

- **Gate Modes**: The `gateMode` parameter determines which pulses fire:
  - **All (0)**: All retrigger pulses fire gates (default)
  - **First (1)**: Only first pulse fires gate
  - **Hold (2)**: Single long gate covering entire duration
  - **FirstLast (3)**: Gates on first and last pulse only

## 4. Retrigger Probability, Pulse Count, and Parameter Relationships

The system uses several interconnected parameters:

- **Retrigger Count**: `step.retrigger()` (0-3) + 1 = actual number of pulses
- **Retrigger Probability**: `clamp(step.retriggerProbability() + probabilityBias, -1, 7)`
- **Actual Pulses**: If probability check passes, uses `step.retrigger() + 1`, otherwise 1 pulse

The probability calculation:
```cpp
int probability = clamp(step.retriggerProbability() + probabilityBias, -1, NoteSequence::RetriggerProbability::Max);
return int(rng.nextRange(NoteSequence::RetriggerProbability::Range)) <= probability ? step.retrigger() + 1 : 1;
```

- **Pulse Count**: Determines how many pulses must occur before advancing to next step
- **Gate Mode**: Determines which of the pulses actually fire gates
- **Accumulator Integration**: When `isAccumulatorTrigger()` is true, the accumulator can be ticked in different modes

## 5. Retrigger vs Regular Gate Events

Key differences between retrigger and regular gate events:

**Regular Gates:**
- Single gate pulse per step
- Duration determined by `step.length()`
- Fires once per step advancement
- No subdivision of step timing

**Retrigger Gates:**
- Multiple gate pulses per step
- Duration is `divisor / retriggerCount`
- Subdivides the step into multiple pulses
- Creates rhythmic patterns within a single step
- Can create "machine gun" or "ratchet" effects

## 6. Timing Precision and Implementation Details

The implementation uses several sophisticated techniques for precise timing:

### Queue-Based Scheduling
- Uses `SortedQueue<Gate, 16, GateCompare>` to schedule gate events
- Events are sorted by tick time for precise timing
- Handles swing and timing variations through `Groove::applySwing()`

### Experimental Spread Mode
The system supports two modes for accumulator interaction:

**Burst Mode (CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS = 0)**:
- Accumulator ticks immediately when retrigger is evaluated
- All N ticks happen at once when step is processed
- Simpler but less synchronized with actual gate firing

**Spread Mode (CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS = 1)**:
- Accumulator ticks when gates actually fire
- Gate struct includes `shouldTickAccumulator` and `sequenceId` metadata
- More precise synchronization but more complex implementation

### Gate Timing Calculation
```cpp
uint32_t retriggerLength = divisor / stepRetrigger;
uint32_t retriggerOffset = 0;
while (stepRetrigger-- > 0 && retriggerOffset <= stepLength) {
    _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + retriggerOffset, swing()), true, shouldTickAccum, seqId });
    _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + retriggerOffset + retriggerLength / 2, swing()), false, false, seqId });
    retriggerOffset += retriggerLength;
}
```

### Accumulator Integration
The system supports three trigger modes:
- **Step**: Ticks once per step (first pulse only)
- **Gate**: Ticks per gate pulse
- **Retrigger**: Ticks per retrigger subdivision

The accumulator has a "delayed first tick" feature to prevent jumps when starting, where the first tick after reset is skipped.

## Summary

This sophisticated retrigger system allows for complex rhythmic patterns, creating multiple gate events within a single step, and provides precise timing control for Eurorack applications. The implementation balances performance with timing accuracy, using queue-based scheduling to handle the complex timing relationships between multiple retriggered gates. The system provides both rhythmic flexibility and precise timing control, making it a powerful feature for creating complex sequences in the PEW|FORMER firmware.