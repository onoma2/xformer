# G_ACCUM_PLAN.md

This document outlines the plan for implementing the Accumulator feature in the PEW|FORMER firmware, with per-ratchet triggering as a core component.

**Prerequisite:** This plan assumes that the core Ratcheting/Pulse Count feature is being implemented concurrently or is already in place.

## Phase 1: Core Model & Engine Implementation

This phase focuses on building the backend logic for the accumulator, including its interaction with ratchets from the start.

### 1.1. Model (`src/apps/sequencer/model/`)

- **Create `Accumulator.h` and `Accumulator.cpp`:**
    - Define the `Accumulator` class. It will include parameters for ratchet interaction from the beginning.
    ```cpp
    class Accumulator {
    public:
        // General accumulator settings
        enum Mode { Stage, Track };
        enum Polarity { Unipolar, Bipolar };
        enum Direction { Up, Down, Freeze };
        enum Order { Wrap, Pendulum, Random, Hold };

        // Ratchet-specific trigger settings
        enum RatchetTriggerMode {
            First, // Trigger on the first ratchet of the step
            All,   // Trigger on every ratchet
            Last,  // Trigger on the last ratchet
            EveryN,// Trigger on every Nth ratchet
            Random // Trigger on random ratchets based on a probability
        };

        // Use bitfields to save space
        uint8_t _mode : 2;
        uint8_t _polarity : 1;
        uint8_t _direction : 2;
        uint8_t _order : 2;
        uint8_t _enabled : 1;
        uint8_t _ratchetTriggerMode : 3;

        int16_t _currentValue;
        int16_t _minValue;
        int16_t _maxValue;
        uint8_t _stepValue;
        uint8_t _ratchetTriggerParam; // For N in EveryN or probability in Random
    };
    ```

- **Modify `NoteSequence.h`:**
    - Add an `Accumulator` instance to the `NoteSequence` class.
    - Add a per-step flag to the `NoteSequence::Step` struct to designate it as a potential trigger:
    ```cpp
    struct Step {
        // ... existing fields
        uint8_t note;
        uint8_t gate;
        uint8_t pulseCount : 4; // Assumed from ratcheting feature
        // ...
        bool isAccumulatorTrigger : 1;
    };
    ```

### 1.2. Engine (`src/apps/sequencer/engine/`)

- **Modify `NoteTrackEngine.cpp`:**
    - The engine's core sequence processing logic needs to be updated.
    - When processing a step, first check if `step.isAccumulatorTrigger` is true.
    - **If the step has no ratchets (`pulseCount <= 1`):**
        - Trigger the accumulator once by calling `accumulator.tick()`.
    - **If the step has ratchets (`pulseCount > 1`):**
        - For each generated ratchet pulse, evaluate the accumulator's `RatchetTriggerMode`.
        - A helper function like `shouldTriggerAccumulator(ratchetIndex, pulseCount)` can be used to contain the logic for `First`, `All`, `Last`, `EveryN`, and `Random` modes.
        - Call `accumulator.tick()` only on the ratchets that meet the criteria.
    - The returned value from `tick()` will be used for pitch transposition, as in the original plan.

## Phase 2: UI Implementation

This phase focuses on creating a comprehensive user interface for all accumulator parameters, including ratchet settings.

### 2.1. Create Accumulator Pages

- **Create `AccumulatorPage.h` and `AccumulatorPage.cpp` ("ACCUM"):**
    - This page will be the main interface for editing the accumulator.
    - It will display and allow editing of all `Accumulator` parameters:
        - Basic settings: Enable, Mode, Direction, Min, Max, Step.
        - Ratchet settings: "Ratchet Trig" (`RatchetTriggerMode`) and "Ratchet Param" (`_ratchetTriggerParam`).
    - The "Ratchet Param" item should only be visible and editable when a relevant "Ratchet Trig" mode (`EveryN` or `Random`) is selected. The label should update dynamically (e.g., to "Every N" or "Prob %").

- **Create `AccumulatorStepsPage.h` and `AccumulatorStepsPage.cpp` ("ACCST"):**
    - This page will be used to toggle the `isAccumulatorTrigger` flag for each step in the sequence.
    - It will provide a visual grid of the steps, similar to the `NoteSequencePage`, where the user can press a step button to toggle its trigger status.
    - The LED for the step should provide clear visual feedback.

### 2.2. Register Pages and Implement Cycling

- This part of the plan remains the same as before:
    - Register `AccumulatorPage` and `AccumulatorStepsPage` in `Pages.h`.
    - Implement the page cycling logic in `TopPage.h` and `TopPage.cpp` to cycle through `noteSequence`, `accumulatorPage`, and `accumulatorStepsPage` when the `NOTE` button is pressed repeatedly.

## Phase 3: Persistence

This phase ensures that all accumulator settings are saved with the project.

### 3.1. Serialization (`src/apps/sequencer/model/`)

- **Update `NoteSequence.cpp` and `Accumulator.cpp`:**
    - The `write` and `read` methods for `NoteSequence` must now serialize the `isAccumulatorTrigger` flag for each step.
    - The `write` and `read` methods for `Accumulator` must serialize all its parameters, including `_ratchetTriggerMode` and `_ratchetTriggerParam`.
    - All new serialization must be guarded by a project version check to maintain backward compatibility.

- **Update Project Version:**
    - Increment the project version number to reflect the new data structure.

## Phase 4: Advanced Features (Future Work)

Once the core functionality with per-ratchet triggering is implemented, the following features can be added:

- **More targets:** Allow the accumulator to modulate other parameters like gate length, probability, and CV output.
- **Advanced modes:** Implement `Pendulum`, `Random`, and `Hold` orders for the accumulator's direction.
- **Performance controls:** Add real-time controls like `Freeze` (to pause the accumulator) and `Nudge` (to manually offset the value).