# Gemini Research: Retrigger and Accumulator

This document details the research into the retrigger functionality of the PEW|FORMER firmware and how it could be integrated with the proposed accumulator feature to create pitched retrigger effects.

## Retrigger Implementation Analysis

The retrigger functionality is implemented in `src/apps/sequencer/engine/NoteTrackEngine.cpp`. When a step with retriggers is processed, the engine performs the following actions:

1.  **Determine Retrigger Count:** The `evalStepRetrigger` function is called to determine the number of times a step should be retriggered. This function takes the base retrigger value from the step and applies a probability to it. If a step has `retrig=3`, this function will return `3`.

2.  **Gate Generation Loop:** If the retrigger count is greater than 1, the code enters a `while` loop that iterates for the number of retriggers. Inside this loop:
    *   **Gate Timing:** The total duration of the step is divided by the number of retriggers to calculate the length of each individual retriggered gate.
    *   **Gate Events:** For each iteration, the engine pushes two events into the gate queue:
        1.  A "gate on" (`true`) event at the start of the retriggered segment.
        2.  A "gate off" (`false`) event halfway through the retriggered segment's duration.

This process is repeated for each retrigger, creating a series of distinct, shorter gates within the time of a single step. This is why you hear three separate gates when you set `retrig=3`.

**Crucially, this existing logic only affects the gate output. It does not modify the pitch or any other parameter of the note for each retrigger.**

## Comparison with Accumulator and Implementation of Pitched Retriggers

The "Accumulator" is a proposed feature (as described in `GEMINI.md`) that is **not yet implemented**. The concept is to have a value that can be incremented or decremented and applied to various step parameters over time.

To implement the desired behavior of having each retrigger play at a different pitch, the accumulator logic would need to be integrated into the retrigger loop in `NoteTrackEngine.cpp`. Here is a conceptual outline of how that could work:

1.  **Accumulator State:** An accumulator would be associated with the sequence, configured with a target parameter (e.g., pitch) and an increment/decrement value.

2.  **Engine Integration:** The `NoteTrackEngine` would be modified to be aware of the accumulator.

3.  **Modified Retrigger Loop:** The `while` loop that generates the retrigger gates would be updated. In each iteration of the loop (for each retriggered gate), the following would happen:
    *   The current value of the accumulator would be retrieved.
    *   This value would be added to the base pitch of the current step.
    *   The resulting pitch value would be used to generate the note for that specific retriggered gate.
    *   The accumulator's value would then be incremented (or decremented) according to its settings.

This would result in three distinct note events, each with its own gate and a progressively changing pitch, achieving the desired "pitched retrigger" effect.
