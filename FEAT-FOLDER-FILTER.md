# Curve Track DJ-Style Filter Feature Design

This document outlines the design for adding a DJ-style LPF/HPF to the `CurveTrack`.

## 1. Overview & Goal

The goal is to introduce a final shaping tool for the `CurveTrack`'s CV output by adding a simple but effective DJ-style filter. This filter will be applied *after* the wavefolder effect, allowing the user to further sculpt the signal.

*   The **Low-Pass Filter (LPF)** functionality will smooth out the sharp edges of a complex wavefolded shape, making the modulation more rounded and less aggressive.
*   The **High-Pass Filter (HPF)** functionality will remove the slow-moving components of the signal, transforming shapes into more percussive "plucks" or "bounces," ideal for rhythmic modulation.

## 2. Core Concept: A 6 dB/octave "DJ-Style" Filter

The feature will be implemented as a single, bipolar control parameter.

*   **Control:** A single parameter ranging from -1.0 to +1.0.
    *   **Center (0.0):** The filter is off (bypassed), and the signal is unaffected.
    *   **Positive values (> 0.0):** Engage the **Low-Pass Filter**. The further from center, the lower the cutoff frequency and the stronger the smoothing effect.
    *   **Negative values (< 0.0):** Engage the **High-Pass Filter**. The further from center, the higher the cutoff frequency and the more low-frequency content is removed.

*   **Filter Slope:** A **6 dB/octave** slope will be used. This first-order filter is ideal for this application because:
    *   It is computationally very efficient.
    *   For LFO modulation, its gentle slope is perfect for musically *shaping* a contour rather than aggressively transforming it, offering a wide and controllable "sweet spot." It can smooth without completely destroying detail (LPF) and create useful percussive shapes without thinning the signal into uselessness (HPF).

## 3. Technical Implementation Plan

The implementation will touch the Model, Engine, and UI layers.

#### 3.1. Signal Chain Placement

The filter will be placed at the very end of the `CurveTrackEngine::updateOutput` function. This means it operates on the final target voltage for a given tick.

**Signal Flow:**
`Shape Generation` -> `Wavefolder` -> `Denormalize to Voltage` -> **`DJ Filter`** -> `Set _cvOutputTarget`
This ensures the filter processes the full, wavefolded voltage signal before it is sent to the final `Slide` (slew) stage in the `update()` function.

#### 3.2. Model (`CurveTrack`)

A new parameter needs to be added to `CurveTrack.h` and `CurveTrack.cpp`.

*   **New Parameter:** Add a private `float _djFilter;` member.
*   **Interface:** Create the full public interface: `djFilter()`, `setDjFilter()`, `editDjFilter()`, and `printDjFilter()`.
*   **Serialization:**
    *   Initialize `_djFilter = 0.f` in the `clear()` function.
    *   Update the `write()` and `read()` functions to save and load the parameter. This will require a `ProjectVersion` bump to `Version44`.

#### 3.3. Engine (`CurveTrackEngine`)

The engine needs to hold the filter's state and implement the filtering logic.

*   **Files:** `CurveTrackEngine.h` and `CurveTrackEngine.cpp`.
*   **Add State:** Add a private `float _lpfState;` member to `CurveTrackEngine.h` to store the internal state of the LPF. This must be initialized to `0.f` in the `reset()` and `restart()` methods.
*   **`applyDjFilter` Function:** Create a new static helper function in `CurveTrackEngine.cpp`. It will contain the core filter algorithm, taking the input voltage, a reference to `_lpfState`, and the `_djFilter` control value. Its internal logic will derive both LPF and HPF outputs from a single first-order IIR filter core.

#### 3.4. UI (`CurveSequenceEditPage`)

The control for the new filter will be added to the "Wavefolder" page we previously designed.

*   **File:** `CurveSequenceEditPage.cpp`.
*   **Actions:**
    1.  **Footer:** Update the `wavefolderFunctionNames` array to include "FILTER" as the fourth item, mapped to the F4 key: `{"FOLD", "GAIN", "SYM", "FILTER", "NEXT"}`.
    2.  **Drawing:** Modify the `draw` function's `Wavefolder` mode to display four parameters instead of three. The fourth slot will show the DJ Filter's value and a bipolar bar.
    3.  **Input:** Update the `keyPress` and `encoder` functions to handle the fourth parameter (`_wavefolderRow = 3`) for selection and editing, calling the new `editDjFilter` function.
