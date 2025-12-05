# Global Phase Offset for Curve Tracks: Analysis and Proposed Solution

## 1. Problem Statement

The initial implementation of a phase offset, as outlined in `splendid-jumping-eagle.md`, applied the phase shift on a per-step basis. This method uses `fmod(_currentStepFraction + phaseOffset, 1.0f)` to "rotate" the curve's shape within the confines of a single step.

This approach results in audible CV jumps at step boundaries. The discontinuity occurs because the phase context is reset for each new step. When the sequencer transitions to a step with a different curve shape or a different min/max voltage range, the starting CV value of the new step does not align with the ending CV value of the previous one.

## 2. Root Cause Analysis

The core of the problem is that the phase calculation is local to each step, not global across the sequence. A true, smooth LFO-style phasing effect requires the phase to progress continuously across all step boundaries, treating the sequence's CV output as a single, unbroken entity.

## 3. Proposed Solution: Global Sequence Phasing

To eliminate CV jumps, the phase offset should be applied to the entire sequence as a whole. This can be achieved by treating the CV shapes of all steps as if they are laid out end-to-end, forming one long "virtual waveform."

The global phase offset parameter will then shift the playback position (the "read head") along this entire waveform. This effectively decouples the CV generation from the gate triggers.

- **Gate Timing:** Gates will continue to fire precisely at the start of their programmed step, locked to the sequencer grid.
- **CV Generation:** The CV value for any given moment will be sampled from a point on the virtual waveform that is determined by the playhead's position plus the global phase offset.

This means that while the sequencer is triggering the gate for **Step 1**, the CV being generated might be sourced from the shape data of **Step 4**, for example. This creates a smooth, continuous CV signal that is musically out-of-phase with the gate sequence.

This behavior can be conceptualized in two equivalent ways:
1.  **Phase-Based Model:** Track a continuous phase that progresses from `0.0` to `N` (where `N` is the sequence length). The offset shifts this phase value before it's used to look up the CV data.
2.  **Tick-Based Model:** Calculate the total number of ticks in the entire sequence. The offset acts as a multiplier to determine a "lookahead" value in ticks, predicting a future point from which to sample the CV.

Both models yield the identical result.

## 4. Implementation Feasibility

This global phasing model is feasible to implement within the existing `CurveTrackEngine` architecture without requiring a major refactor.

- **Data Access:** The `CurveTrackEngine` already has access to the entire `CurveSequence` and can read the data for any step, not just the currently active one.
- **Performance:** The required logic adds only a few floating-point calculations per update cycle, which will have a negligible impact on performance.
- **Isolation:** The necessary code changes can be almost entirely contained within the `CurveTrackEngine`'s CV update logic, with a new parameter added to the `CurveTrack` model.

## 5. Key Considerations for Implementation

- **Gate Logic:** The logic for gate generation must remain untouched and tied to the original, non-offset step index.
- **Live Recording:** The interaction with live CV recording must be handled. A safe approach is to automatically disable the global phase offset on any track that is in record mode to prevent unpredictable feedback loops.
- **UI Display:** The UI should continue to highlight the current "gate step," as this reflects the sequence's rhythmic position. The user must understand that the generated CV will not always visually match the shape of the highlighted step, which is the intended effect of this feature.
