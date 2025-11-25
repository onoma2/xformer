# Curve Track Wavefolder Feature Design

This document outlines the design for a wavefolder feature to be added to the `CurveTrack`.

## 1. Overview & Goal

The goal is to add a wavefolder-like effect to the `CurveTrack`'s CV output. As the `CurveTrack` is primarily used as a sequenced LFO, the purpose of this effect is not to add audio-rate harmonics, but to create more complex and interesting modulation shapes.

The effect will be implemented as a "post-processing" stage within the `CurveTrackEngine`, acting on the normalized (0.0 to 1.0) CV signal after the basic shape has been generated from the sequence.

## 2. Controllable Parameters

The wavefolder effect will be controlled by three main parameters:

1.  **Fold Amount** (or "Fold")
    *   **Description:** The primary control for the effect's intensity. It determines how many times the LFO shape is folded back on itself.
    *   **Effect:** Transforms a simple shape (like a ramp) into a more complex one with multiple peaks and troughs ("ripples") within a single LFO cycle.

2.  **Input Gain** (or "Gain")
    *   **Description:** Controls the amplitude of the LFO signal *before* it enters the folding algorithm.
    *   **Effect:** Acts as a "drive" or "intensity" control. Higher gain will push more of the LFO's dynamic range past the folding threshold, resulting in a more pronounced effect.

3.  **Symmetry**
    *   **Description:** Adjusts the character of the folding by applying it asymmetrically to the waveform.
    *   **Effect:** This has a strong impact on the rhythmic feel of the modulation. For example, it can create an LFO shape that rises in sharp, angular steps but falls with smooth, rounded curves. At a setting of 0, the folding is perfectly symmetrical.

## 3. Proposed UI/UX

The user interface for this feature will be integrated into the `CurveSequenceEditPage` and will leverage existing UI patterns from the `TuesdayTrack` interface for consistency and ease of use.

*   **Page Toggling with `PHASE` (F4) Key:**
    The `PHASE` key (F4) will act as a two-state toggle, cycling through different editing modes:
    1.  **Mode 1 (1st press):** "Global Phase" edit mode (the current behavior).
    2.  **Mode 2 (2nd press):** A new "Wavefolder" parameter edit page.
    A subsequent press would cycle back to Mode 1. Pressing a different function key (F1-F3) would exit to the normal step editing mode for that layer.

*   **Wavefolder Edit Page:**
    *   This page will present a simple list of the three wavefolder parameters: `Fold`, `Gain`, and `Symmetry`.
    *   The UI will be functionally identical to other parameter list pages (like the main Track page or the Tuesday Algo page), where the user can scroll through the list, select a parameter, and turn the encoder to adjust its value.
    *   This design is highly feasible and can be implemented cleanly by creating a new, dedicated `WavefolderListModel` and navigating to a "sub-page" that uses it.

## 4. Technical Implementation Plan

The implementation is broken down into three main areas: the Model, the UI, and the Engine.

#### 4.1. Model (`CurveTrack`)

The new parameters must be added to `CurveTrack.h` and `CurveTrack.cpp`.
*   **New Members:** Add three private `float` members: `_wavefolderFold`, `_wavefolderGain`, `_wavefolderSymmetry`.
*   **Functions:** Implement public `getter`, `setter`, and `edit` functions for each parameter.
*   **Serialization:** Update the `clear()`, `write()`, and `read()` functions to initialize and persist these parameters. This will require a new `ProjectVersion`.

#### 4.2. Engine (`CurveTrackEngine`)

The core logic is placed in `CurveTrackEngine::updateOutput`.
*   **Placement:** The wavefolder logic should be applied *after* the normalized shape value is generated (from `evalStepShape`) and *before* it is denormalized into a final voltage.
*   **Conceptual Flow in `updateOutput`:**
    ```cpp
    // 1. Phased time is calculated and a normalized value is generated
    float shapeValue = evalStepShape(step, ..., phasedFraction);

    // 2. Get wavefolder parameters
    float fold = _curveTrack.wavefolderFold();

    // 3. NEW: Apply wavefolder if active
    if (fold > 0.f) {
        float gain = _curveTrack.wavefolderGain();
        float symmetry = _curveTrack.wavefolderSymmetry();
        shapeValue = applyWavefolder(shapeValue, fold, gain, symmetry);
    }

    // 4. Convert final value to voltage
    _cvOutputTarget = range.denormalize(shapeValue);
    ```
*   **`applyWavefolder` Helper Function:** A new static helper function inside `CurveTrackEngine.cpp` will contain the algorithm. It will take the normalized (0.0 to 1.0) `shapeValue` and the three parameters, and return a new folded, normalized value.

#### 4.3. UI (New `WavefolderListModel`)

To support the proposed UI, a new `WavefolderListModel.h` would be created. This file would be very similar to `CurveTrackListModel.h` but would be dedicated solely to listing, formatting, and editing the three new wavefolder parameters.

## 5. Interaction with `globalPhase`

The wavefolder and `globalPhase` features work together in series and do not conflict.

1.  `globalPhase` acts first, in the **time-domain**. It determines *which point in the sequence* to sample a value from.
2.  The wavefolder acts second, in the **value-domain**. It takes the value that was just sampled and mathematically distorts its *amplitude and shape*.

This is analogous to a hardware patch: `globalPhase` is like turning the "Phase" knob on an LFO module. The wavefolder is like taking the output of that LFO and patching it into a separate wavefolder module. The wavefolder simply processes whatever signal it is given, whether it's phase-shifted or not.
