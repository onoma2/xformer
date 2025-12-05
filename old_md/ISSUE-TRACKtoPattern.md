# TDD Plan: Moving CurveTrack Parameters to Patterns

**Objective:** Move Chaos and Wavefolder parameters from `CurveTrack` (Global) to `CurveSequence` (Per-Pattern). This allows different patterns to have distinct sound design settings.

**Complexity:** Medium.
**Breaking Change:** Yes. Project file compatibility for Chaos/Wavefolder settings will be broken (settings will reset to default).

## 1. The Data Model Refactor

### Requirements
*   **CurveSequence** must hold:
    *   `ChaosAmount`, `ChaosRate`, `ChaosP1`, `ChaosP2`, `ChaosAlgo`.
    *   `WavefolderFold`, `WavefolderGain`, `DjFilter`, `XFade`.
    *   These must be `Routable<T>` where applicable.
*   **CurveTrack** must lose these variables.
*   **Routing** must target the *Sequence*, not the *Track*.

### Test Plan (`src/tests/unit/sequencer/TestCurveTrackPatterns.cpp`)

#### Test Case 1: Independent Storage
*   **Goal:** Verify that Pattern A and Pattern B can store different Chaos values.
*   **Code:**
    ```cpp
    CurveTrack track;
    auto &seq0 = track.sequence(0);
    auto &seq1 = track.sequence(1);

    seq0.setChaosAmount(10);
    seq1.setChaosAmount(90);

    expectEqual(seq0.chaosAmount(), 10);
    expectEqual(seq1.chaosAmount(), 90);
    ```

#### Test Case 2: Routing Target Redirection
*   **Goal:** Verify that writing to a Routing Target updates the specific Sequence.
*   **Context:** Routing targets like `ChaosAmount` are currently `Track` targets. They need to become `Sequence` targets or handled via the per-pattern loop in `Routing::writeTarget`.

#### Test Case 3: Engine State
*   **Goal:** Verify that the Engine picks up the new settings when the pattern changes.
*   **Code:**
    ```cpp
    Engine engine;
    CurveTrack track;
    CurveTrackEngine trackEngine(engine, ...);
    
    // Setup patterns
    track.sequence(0).setWavefolderFold(0.0f);
    track.sequence(1).setWavefolderFold(1.0f);

    // Play Pattern 0
    trackEngine.setPattern(0);
    trackEngine.tick(0);
    // assert engine is using Fold 0.0 (check internal state or output)

    // Play Pattern 1
    trackEngine.setPattern(1);
    trackEngine.tick(0);
    // assert engine is using Fold 1.0
    ```

## 2. Implementation Steps

1.  **Write the Test:** Create the failing test `TestCurveTrackPatterns.cpp`.
2.  **Model Migration:**
    *   Cut variables/methods from `CurveTrack.h`.
    *   Paste into `CurveSequence.h`.
    *   Update `CurveSequence::clear()`, `write()`, `read()`.
    *   Update `CurveTrack::writeRouted` -> remove moved targets.
    *   Add `CurveSequence::writeRouted` -> add moved targets.
3.  **Routing Update:**
    *   In `Routing.cpp`, ensure `writeTarget` directs these enums to the Sequence loop (just like `FirstStep` or `Divisor`).
4.  **Engine Update:**
    *   In `CurveTrackEngine.cpp`, replace `_curveTrack.chaosAmount()` with `_sequence->chaosAmount()`.
5.  **UI Update:**
    *   Refactor `CurveSequenceEditPage` (and Context Menus) to edit the `selectedCurveSequence()` instead of `selectedTrack().curveTrack()`.
    *   Refactor `CurveTrackListModel` (if it displayed these params) to remove them, or point them to the *current* sequence.

## 3. Future: TuesdayTrack Refactor
*   Moving `TuesdayTrack` params to patterns is a larger "High Complexity" task requiring the creation of a new `TuesdaySequence` class. This is deferred.
