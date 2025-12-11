# Phase Offset Global Implementation Research

## Issue Summary
The current implementation of phase offset in CurveTrack applies the offset independently to each step, causing CV jumps when transitioning between steps with different shapes. This research explores alternative approaches to implement a global phase offset that maintains continuity across steps.

## Current Problem
- Phase offset is applied per-step using: `float phasedFraction = fmod(_currentStepFraction + phaseOffset, 1.0f)`
- Each step rotates its shape independently
- CV jumps occur at step boundaries when adjacent steps have different shapes/values
- Example: Step ending at value 1.0 transitions to next step starting at value 0.0 creates a jump

## Alternative Approach: Look-Ahead Phase Prediction
Instead of modifying the current step's evaluation point, consider using phase offset as a multiplier to determine a future point in the sequence:

### Concept
- Calculate total sequence duration from first tick of first step to last tick of last step
- Use phase offset as a multiplier to determine how far ahead in the sequence to look
- CV output is based on the value at this "future" point rather than current playhead

### Technical Implementation
1. Calculate total sequence duration: `totalDuration = sum of all step durations`
2. Calculate look-ahead point: `lookAheadPoint = (currentPosition + phaseOffset * totalDuration) % totalDuration`
3. Determine CV value at the look-ahead point through interpolation if needed

### Advantages
- Eliminates per-step CV jumps by treating sequence as continuous timeline
- Predictable phase relationship across entire pattern
- Consistent behavior regardless of individual step shapes
- Smooth phase shifting across entire sequence

### Challenges
- Higher complexity: requires tracking total sequence duration
- Handling variable step lengths (length variation, etc.)
- Interpolation needed when look-ahead point doesn't align with step boundaries
- Less intuitive for users compared to simple phase shift
- Potentially higher computational cost on STM32

### Memory & Computation Impact
- Need to store total sequence duration
- Additional calculations for determining future point
- Possible interpolation logic between steps
- Overall: More computational overhead than global accumulator approach

## Comparison with Global Phase Accumulator Approach

### Global Phase Accumulator
- **Mechanism**: Maintains continuous phase that advances across step boundaries
- **Complexity**: Lower - just tracking cumulative phase
- **Memory**: Minimal - few floats per track
- **Computation**: Low - basic arithmetic per tick
- **Intuitiveness**: More intuitive phase shifting

### Look-Ahead Prediction
- **Mechanism**: Predicts future sequence state based on phase offset
- **Complexity**: Higher - requires total duration calculation and interpolation
- **Memory**: More storage needed for duration tracking
- **Computation**: Higher - more complex calculations
- **Intuitiveness**: Less intuitive concept of looking ahead

## Recommendation
The global phase accumulator approach appears to be the better solution:
1. Simpler to implement and understand
2. Lower resource requirements
3. Maintains phase continuity across steps
4. Preserves gate timing on step boundaries
5. More straightforward mapping to user expectations

The look-ahead approach, while innovative, introduces unnecessary complexity for the benefit it provides. The global phase accumulator achieves the same goal of eliminating CV jumps with less code complexity and resource usage.

## Implementation Considerations
For the global phase accumulator approach:
- Add `_globalPhaseAccumulator` float to CurveTrackEngine
- Update phase calculation in `updateOutput()` method
- Preserve existing gate timing logic
- Ensure reset behavior on pattern changes
- Maintain backward compatibility with existing projects

This research suggests the global phase accumulator approach is the optimal path forward for implementing continuous phase offset in CurveTrack.