# Grids Track Implementation Plan

This document outlines the plan to implement a new "Grids" track type in the Performer firmware, based on the functionality of the Mutable Instruments Grids module.

## Phase 1: Model Layer - Defining the Data Structure

This phase establishes the data representation for a Grids track.

1.  **Create Pattern Data Header:**
    *   Create a new file: `src/apps/sequencer/model/grids/Patterns.h`.
    *   In this file, define the 25 `node_` arrays from `grids/resources.cc` as `static const uint8_t` arrays. This ensures the 2.4KB of pattern data is stored in flash, not RAM.
    *   Also, define the `5x5` `drum_map` pointer array: `static const prog_uint8_t* drum_map[5][5]`.

2.  **Update Track Type Enum:**
    *   In `src/apps/sequencer/model/Track.h`, add `Grids` to the `TrackType` enum.

3.  **Create `GridsTrack` Model:**
    *   Create `src/apps/sequencer/model/GridsTrack.h` and `.cpp`.
    *   The `GridsTrack` class will store the parameters that define the rhythm:
        *   `_mapX` (uint8_t): 0-255, controls horizontal position on the map.
        *   `_mapY` (uint8_t): 0-255, controls vertical position on the map.
        *   `_density1`, `_density2`, `_density3` (uint8_t): 0-255, sets the density/threshold for each of the three internal instruments (BD, SD, HH).
        *   `_fill` (uint8_t): 0-255, controls the "randomness" or fill amount.
        *   `_output` (uint8_t): 0-2, selects which internal instrument (BD, SD, or HH) this track sends to its main GATE output.
        *   `_cvOutputMode` (uint8_t): 0-3, selects which instrument (Off, BD, SD, HH) is sent to the CV output as a secondary gate.

4.  **Integrate `GridsTrack` into `Track` Model:**
    *   In `Track.h`, add a `GridsTrack _gridsTrack;` member to the `_track` union.
    *   Update the `Track` class to handle initialization, serialization (for saving/loading projects), and clearing of the new `GridsTrack` type.

## Phase 2: Engine Layer - Generating the Rhythms

This phase implements the core logic for generating gates based on the model data.

1.  **Create `GridsEngine`:**
    *   Create `src/apps/sequencer/engine/GridsEngine.h` and `.cpp`.
    *   This class will contain the core logic for interpolation and evaluation.
    *   **Key Methods:**
        *   `readDrumMap(uint8_t x, uint8_t y, uint8_t *levels)`: Performs bilinear interpolation to calculate intensity `levels` for all steps and instruments.
        *   `evaluate(const GridsTrack &track, int step, Random &rng)`: Takes track data and the current step, applies fill logic, compares against density, and returns a 3-bit mask representing the gate states of all three instruments (e.g., `BD | SD | HH`).

2.  **Create `GridsTrackEngine`:**
    *   Create `src/apps/sequencer/engine/GridsTrackEngine.h` and `.cpp` inheriting from `TrackEngine`.
    *   This class will bridge the main `Engine` and the `GridsEngine`.
    *   On each clock tick, it will call `GridsEngine::evaluate()`, and then:
        *   Use the returned 3-bit mask to set the main **GATE output** based on the `_output` parameter.
        *   Use the same mask to set the **CV output** to a high/low voltage based on the `_cvOutputMode` parameter.

3.  **Integrate into `TrackEngineContainer`:**
    *   In `src/apps/sequencer/engine/TrackEngineContainer.cpp`, modify `update()` and `reset()` to call the new `GridsTrackEngine` when a track's type is `Track::TrackType::Grids`.

## Phase 3: UI Layer - User Interaction

This phase allows the user to see and edit the Grids parameters.

1.  **Add `Grids` to Track Type Selection:**
    *   In `TrackSetupPage` (or equivalent), add "Grids" to the list of available track types.

2.  **Create `GridsTrackPage`:**
    *   Create a new page file: `src/apps/sequencer/ui/pages/GridsTrackPage.h` and `.cpp`.
    *   This page will be displayed when editing a track of type `Grids`.
    *   It will register the following parameters for editing:
        *   `Map X` (`_mapX`)
        *   `Map Y` (`_mapY`)
        *   `Density 1` (`_density1`)
        *   `Density 2` (`_density2`)
        *   `Density 3` (`_density3`)
        *   `Fill` (`_fill`)
        *   `Output` (`_output`): Displayed as "BD", "SD", "HH".
        *   `CV Output` (`_cvOutputMode`): Displayed as "Off", "Inst 1", "Inst 2", "Inst 3".

3.  **Integrate into `PageManager`:**
    *   Update the UI logic to navigate to the `GridsTrackPage` when the user selects a Grids track for editing.

## Phase 4: Clocking and Integration

*   **Master Clock Division:** No special implementation is needed. The `GridsTrackEngine` will advance its internal 32-step sequence each time it receives a clock from the main `Engine`. The main `Engine` already handles track-level clock division, so the Grids pattern will naturally run at the correct divided speed.

## Hardware & Performance Considerations (STM32)

*   **CPU Load:** The algorithm is lightweight and will not strain the STM32F4 CPU.
*   **Memory:** Pattern data (2.4KB) will be stored in flash using `const` arrays to conserve RAM.
*   **UI Noise:** The UI will reuse the existing text-based parameter editing framework to avoid introducing audio noise from excessive display updates.
