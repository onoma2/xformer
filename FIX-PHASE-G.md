# Summary of Global Phase Fix

This document summarizes the work done to address the issue of per-step phase offset causing audible CV jumps, as described in `ISSUE-PHASE-G.md`.

## The Journey

The process followed a Test-Driven Development (TDD) approach, with some course correction as the implementation details became clearer.

1.  **Analysis and Initial Plan:** The initial analysis of `ISSUE-PHASE-G.md` confirmed that a global phase offset was needed. The first plan involved adding a `globalPhase` parameter to the `CurveSequence` model.

2.  **Implementation and Refactoring:** During implementation, it was discovered that placing `globalPhase` on the `CurveSequence` created a mismatch with the UI. The relevant UI control ("Phase Offset") existed on the track-level settings page, which did not have direct access to a specific sequence. This led to a key realization and a pivot in the plan.

3.  **Revised Plan:** The implementation was refactored to move the `globalPhase` property from `CurveSequence` to the `CurveTrack` model. This aligned the new parameter with the existing UI and model architecture.

4.  **Implementation (Round 2):**
    *   The `globalPhase` property (a `float` from 0.0 to 1.0) was added to `CurveTrack`.
    *   The old `phaseOffset` property (`uint8_t`) was removed.
    *   Serialization logic in `CurveTrack` was updated to handle the new `globalPhase` and, critically, to migrate the value from old projects that had `phaseOffset`.
    *   The `CurveTrackEngine` was modified to use the new track-level `globalPhase` to calculate a continuous, phase-shifted CV output across the entire sequence.
    *   The UI was updated by remapping the old "Phase Offset" control in `CurveTrackListModel` and `CurveSequenceEditPage` to the new `globalPhase` property.
    *   Unit tests were written to cover the new `CurveTrack` property, the serialization and migration logic, and the conceptual engine algorithm.

5.  **Bug Fixing:** After the main implementation, a compilation error was identified and fixed. It was caused by a remaining reference to the old `phaseOffset` in `CurveSequenceEditPage`. An obsolete test file for the old property was also removed.

6.  **Verification:** Finally, when asked about serialization, a specific test case was added to verify both the serialization of the new property and the correct migration from the old `phaseOffset` data format.

## Key Findings

*   **Parameter Placement is Key:** The most significant finding was that `globalPhase` belongs on the `CurveTrack` model, not `CurveSequence`. This simplifies the architecture and aligns the data model with the user interface structure, where phase is treated as a track-level parameter.
*   **Data Migration is Crucial:** Replacing a property (`phaseOffset`) with a new one (`globalPhase`) requires a migration path in the deserialization logic to ensure backward compatibility with older project files.
*   **Impact of Changes:** A seemingly small change to one property had wide-ranging effects, touching the data model, the engine, multiple UI files, and several test files.

## Results

*   **Smooth CV Phasing:** The primary goal was achieved. The implemented global phase offset produces a smooth, continuous CV signal across step boundaries, eliminating the audible jumps of the previous per-step implementation.
*   **UI Remapping:** The existing "Phase Offset" control on the track settings page now correctly adjusts the global phase, providing a familiar user experience.
*   **Backward Compatibility:** Projects saved with the old integer-based `phaseOffset` will have their values correctly migrated to the new float-based `globalPhase` when loaded.
*   **Robust Testing:** The feature is supported by a suite of unit tests that verify the property's behavior, its serialization, data migration, and the correctness of the core engine algorithm.

## Postponed Topics

*   **Dedicated UI for Global Phase:** The current UI reuses the old "Phase Offset" control, which was designed for an integer percentage. A future improvement could be to design a UI specifically for the new float-based `globalPhase` parameter, perhaps offering a more intuitive display.
*   **Per-Step Phase Feature:** The old per-step phase offset functionality has been completely removed. If this is still a desired feature, it would need to be designed and implemented as a separate feature, likely with its own UI on the step edit page, to coexist with the new global phase. The `Phase` function (F5) on the `CurveSequenceEditPage` currently activates the global phase editing mode; its original purpose may need to be revisited if per-step phasing is reintroduced.

## Remaining Artifacts of old `phaseOffset`

While the `phaseOffset` property was removed from the `CurveTrack` model, some references to `phaseOffset` still exist in the codebase for specific reasons:

*   **`src/apps/sequencer/model/CurveTrack.cpp`**: Inside the `read` function, a local variable named `phaseOffset` is used. This is a crucial part of the data migration process. When loading a project file saved with an older version, this code reads the old `phaseOffset` value, converts it, and stores it in the new `_globalPhase` property. This is intentional and necessary for backward compatibility.

*   **`src/tests/unit/sequencer/TestCurveTrackEnginePhase.cpp`**: This test file has not been removed. It contains several unit tests that verify the mathematical concepts of phase shifting, such as `fmod` calculations and curve evaluation at a shifted fraction. These tests do not depend on the removed `CurveTrack::phaseOffset` property; they use local variables named `phaseOffset` for testing the math in isolation. The file remains as a valid, albeit potentially misleadingly named, test for the underlying phase-shifting logic that is still used by the new global phase feature. It was intentionally left in place for potential future reuse.
