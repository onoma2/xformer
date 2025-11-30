# Feature: Fraction (Phase) Manipulation

## Goal
Enable direct, routable control over the `_currentStepFraction` (micro-timing/phase) within the `CurveTrackEngine`. This allows external CV, LFOs, or other modulation sources to "scrub" through the LFO shape or offset its timing with high precision, effectively turning the Curve Track into a complex waveshaper or phase-modulated oscillator.

## Current Limitation
- `_currentStepFraction` is calculated internally from `relativeTick` (system clock).
- `_globalPhase` is a `float` but is **not Routable**. It can only be set manually via the encoder.
- Existing routable parameters (Offset, Rotate) operate on Voltage or Whole Steps, not micro-phase.

## Implementation Ideas

### 1. Make Global Phase Routable
Convert `_globalPhase` from a raw `float` to a `Routable<int16_t>` or similar fixed-point representation to integrate with the Routing Matrix.

*   **Data Structure:**
    *   Change `float _globalPhase` to `Routable<int16_t> _phase`.
    *   **Range:** 0 to 1000 (representing 0.0 to 1.0).
    *   **Precision:** 0.1% (sufficient for most modulation, though 10000 would be smoother).

*   **Routing Target:**
    *   Add `Routing::Target::Phase` (or similar).

*   **Engine Logic (`CurveTrackEngine::updateOutput`):**
    ```cpp
    // Get routed value (Base + Mod)
    int phaseInt = _curveTrack.phase(); 
    float phaseOffset = phaseInt / 1000.0f; 
    
    // Apply to fraction
    // Scenario A: Offset logic (Standard)
    _currentStepFraction = fmod(_currentStepFraction + phaseOffset, 1.0f);
    ```

### 2. "Manual Phase" Play Mode
Introduce a specific mode where the internal clock is disconnected, and the "Phase" parameter *becomes* the clock.

*   **UI:** Add `PlayMode::Manual` (or similar).
*   **Engine Logic:**
    ```cpp
    if (_curveTrack.playMode() == PlayMode::Manual) {
        // Direct mapping: Routing Value IS the position
        _currentStepFraction = _curveTrack.phase() / 1000.0f;
    } else {
        // Standard Clock
        _currentStepFraction = float(relativeTick % divisor) / divisor;
        // Optional: Add phaseOffset here for Phase Modulation
    }
    ```

### 3. High-Res Floating Point Routing (Advanced)
If 0-1000 integer steps cause audible "zipper" noise for phase modulation (FM), we might need a specialized float routing path, but this requires deeper changes to the `Routing` architecture which primarily handles integers. 
*   *Verdict:* Fixed-point int16 (0-16384) is likely sufficient and fits existing architecture.

## Proposed Workflow
1.  **Add Parameter:** Add `Routable<int16_t> _phaseOffset` to `CurveTrack`.
2.  **UI:** Add "Phase" control to the Curve Track menu (replacing or augmenting the existing non-routable Global Phase).
3.  **Routing:** Allow `CV1` -> `Phase`.
4.  **Result:**
    *   **Static:** Knob sets start point.
    *   **Modulated:** LFO modulates LFO (PM).
    *   **Manual:** CV sweeps through the complex shape (Waveshaping).
