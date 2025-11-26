# Curve Track Wavefolder Enhancement Session - Status Update

## 1. Critical Issue: Severe Aliasing with Fold-F Feedback
**Problem:** Any amount of Fold-F feedback introduces significant aliasing to the output, particularly noticeable during complex modulation.

**Root Cause:** The sine-based wavefolder combined with feedback creates high-frequency content that exceeds the sampling frequency, causing aliasing artifacts. When Fold-F feeds processed (potentially aliased) output back into the wavefolder, it compounds the problem.

**Current Impact:** Makes Fold-F feedback impractical for many musical applications due to audible distortion.

**Additional Aliasing Source:** The Gain parameter at UI values above 0.4 also contributes to aliasing, though not as severely as Fold-F feedback. This occurs because higher gain increases the amplitude of the signal going into the wavefolder, creating more harmonic content that extends beyond the Nyquist frequency.

## 2. Proposed Solutions for Aliasing Issue

### 2.1 Oversampling Approach
- Process the wavefolder section at higher sample rate (e.g. 2x or 4x)
- Apply low-pass filtering before downsampling
- **Pros:** Effective aliasing reduction
- **Cons:** High computational cost on STM32

### 2.2 Band-Limited Wavefolder
- Use band-limited approximation of the sine wavefolder
- Limit harmonic content to below Nyquist frequency
- **Pros:** More computationally efficient than oversampling
- **Cons:** May affect harmonic character of wavefolder

### 2.3 Feedback Filtering
- Add gentle low-pass filtering in the feedback path
- Reduce high-frequency content before feedback
- **Pros:** Simple to implement, computationally light
- **Cons:** Slightly alters the feedback character

## 3. Implemented Features During This Session

### 3.1 Amplitude Compensation
- Added compensation for amplitude reduction when wavefolder harmonics are attenuated by the filter
- Compensation only applies when filter is active (outside dead zone)
- Also compensates for filtering effects when fold=0

### 3.2 Feedback Control Improvements
- Made Fold-F and Filter-F controls logarithmic for smoother low-end behavior
- Fold-F feedback now only applies when fold > 0 (prevents signal getting "stuck" when fold=0)
- Added resonance feedback limiting when filter control is at extreme settings

### 3.3 Gain Parameter Remapping
- Remapped UI gain from 0.0-2.0 to internal 1.0-5.0 range
- More intuitive mapping: 0.0='standard', 1.0='1 extra', 2.0='2 extreme'
- Maintains all mathematical properties of the wavefolder algorithm

### 3.4 Output Protection
- Added proper ±5V limiting to prevent output overloads
- Added internal feedback state limiting to prevent runaway feedback
- Reduced maximum resonance feedback scaling to prevent instability

### 3.5 Crossfading Functionality (xFade)
- Added xFade parameter to blend between original phased shape and processed signal
- Default value of 1.0 preserves original behavior
- Crossfader operates on final output, feedback path uses processed signal before crossfading

### 3.6 UI Updates
- Added xFade parameter to Wavefolder2 UI (after Filter-F)
- Updated Gain parameter display to show new 0.0-2.0 range properly
- All new parameters have proper serialization support

## 4. Previous Implementation Details

This document summarizes the development and current status of the advanced CV shaping features for the `CurveTrack`, including wavefolding, filtering, and two independent feedback loops.

### 4.1 Overview

The goal of this session was to implement advanced "West Coast" style CV shaping for the `CurveTrack` in a modular fashion. This involved adding:
*   A wavefolder for complex shape generation.
*   A DJ-style LPF/HPF for sculpting the CV output.
*   Two independent feedback loops to create dynamic and potentially chaotic interactions.

### 4.2 Wavefolder Implementation

The wavefolder serves as the primary shape distortion unit.

*   **Parameters:**
    *   `Fold`: Controls the number of folds/complexity of the waveform.
    *   `Gain`: Controls the input drive into the folding stage.
    *   `Symmetry`: Adjusts the bias of the folding, affecting the harmonic character.
*   **Model (`CurveTrack`):** `_wavefolderFold`, `_wavefolderGain`, `_wavefolderSymmetry` (all `float`) were added and integrated into the `CurveTrack` model, including serialization (ProjectVersion 43).
*   **Engine (`CurveTrackEngine`):** An `applyWavefolder` helper function was implemented, operating on the normalized `0.0-1.0` signal.
*   **Fold Control Response:** The `Fold` parameter's linear value from the UI is squared (`fold * fold`) before being used by the wavefolder algorithm. This creates an exponential response, providing more nuanced control at lower fold settings.

### 4.3 DJ Filter Implementation

A versatile DJ-style filter was integrated to provide final sculpting of the CV signal.

*   **Parameters:**
    *   `djFilter`: A bipolar `float` parameter (`-1.0` to `+1.0`).
        *   `0.0`: Filter bypassed.
        *   Positive (`+0.02` to `+1.0`): Engages a High-Pass Filter (HPF).
        *   Negative (`-0.02` to `-1.0`): Engages a Low-Pass Filter (LPF).
    *   **Dead Zone:** A small dead zone (`+/- 0.02`) around the center of the `djFilter` control ensures an explicit bypass state.
*   **Filter Slope:** Implemented as a **6 dB/octave** (first-order IIR) filter.
*   **Model (`CurveTrack`):** `_djFilter` was added to `CurveTrack` and integrated into serialization (ProjectVersion 44).
*   **Engine (`CurveTrackEngine`):**
    *   A `_lpfState` float member was added to `CurveTrackEngine` to maintain the filter's internal state.
    *   An `applyDjFilter` helper function was implemented.
*   **Control Response:** The mapping of the `djFilter` control to the filter's `alpha` coefficient was carefully adjusted to provide an intuitive "stronger effect with more knob turn" behavior for both LPF and HPF modes, addressing initial observations about counter-intuitive amplitude drops.

### 4.4 Feedback Loops Implementation ("West Coast" Style)

Two independent feedback loops were added to create a rich, chaotic, and highly interactive CV shaping instrument.

*   **Parameters:**
    *   `Fold-F` (`_foldF`): Controls feedback from the filter output back to the wavefolder input (`0.0` to `1.0`).
    *   `Filter-F` (`_filterF`): Controls feedback around the filter itself (resonance/Q), from `0.0` to `1.0`.
*   **Model (`CurveTrack`):** `_foldF` and `_filterF` were added to `CurveTrack` and integrated into serialization (ProjectVersion 45).
*   **Engine (`CurveTrackEngine`):**
    *   A `_feedbackState` float member was added to `CurveTrackEngine` to store the last tick's filtered output for `Fold-F` feedback.
    *   The `applyDjFilter` function was modified to accept `_filterF` as a resonance parameter, implementing the filter-to-filter feedback.
    *   The `updateOutput` function was refactored to implement both feedback paths: `_feedbackState` is added to the wavefolder input for `Fold-F`, and the `applyDjFilter` now handles `Filter-F`.
*   **UI (`CurveSequenceEditPage`):**
    *   The `EditMode` state machine was expanded to four states: `Step`, `GlobalPhase`, `Wavefolder1` (for Fold, Gain, Symmetry, Filter), and `Wavefolder2` (for Fold-F, Filter-F).
    *   A second UI page (`Wavefolder2`) was created within `CurveSequenceEditPage` to display and allow editing of the `Fold-F` and `Filter-F` parameters, mimicking the `TuesdayEditPage` style with value bars and F-key selection.

### 4.5 Compilation Issues & Resolutions

Throughout the development process, several compilation errors and warnings were encountered and resolved, including:
*   A typo in an include directive (`LedPainter.hh` instead of `LedPainter.h`).
*   Syntax errors and scope issues in `CurveSequenceEditPage.cpp` due to refactoring the `draw()` and `encoder()` functions.
*   Mismatched enum usage (`EditMode::Wavefolder` after it was renamed to `EditMode::Wavefolder1`/`Wavefolder2`).
*   Warnings regarding the use of bitwise OR (`|`) with boolean operands, which were corrected to logical OR (`||`).

### 4.6 Issue to Investigate: FILTER-F Path

The implementation of `Filter-F` (Filter-to-Filter feedback / Resonance) within the `applyDjFilter` function utilizes a simplified resonance model for a 1-pole filter (`float feedback_input = input - lpfState * feedback;`). While this method introduces a resonant characteristic, it is inherently limited compared to what a multi-pole (e.g., 2-pole State Variable Filter) resonant filter can achieve.

*   **Behavior to Monitor:** The user should observe whether the `Filter-F` parameter provides a sufficiently pronounced and musically useful resonance.
*   **Potential Improvement:** If the resonance effect is deemed too subtle or lacks the desired musical quality, a future investigation into implementing a more advanced filter topology (e.g., a 2-pole resonant low-pass/high-pass filter) within `applyDjFilter` may be warranted. This would increase computational cost and complexity but could yield a more classic resonant filter sound.

## 5. Signal Path Updates
- Original shape → Phase offset → [stored for crossfading] → Fold-F feedback → Wavefolder → Denormalize → Filter + Filter-F → Compensation → Crossfading → Final limiting → CV output
- Feedback path: Processed signal (before xFade) → _feedbackState → Fold-F feedback input

## 6. Performance Considerations
- All changes maintain real-time performance on STM32
- Minimal additional computational overhead (crossfade: 3 mults + 2 adds, limiting: min/max ops)
- Memory usage increased by only a few bytes per CurveTrack engine instance

## 7. Backward Compatibility
- All parameters maintain backward compatibility
- Gain parameter automatically converted from old (1.0-5.0) to new (0.0-2.0) range when loading older projects
- Default xFade value maintains original behavior in existing projects