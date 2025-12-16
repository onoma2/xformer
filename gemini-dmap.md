# Discrete Map Implementation Plan (Final Revision)

This plan integrates the original hardware design (NS Instruments Discrete Map) with the Performer's existing infrastructure, maximizing code reuse and minimizing redundancy.

## 1. Data Model & Serialization (`DiscreteMapSequence.h`)

We will extend the existing `DiscreteMapSequence` model to support the missing hardware features (Input Range, Gate Length) while reusing Performer's standard types.

### 1.1 New Parameters
*   **Voltage Range:** Reuse `Types::VoltageRange` (existing enum: `-5V..+5V`, `0V..+5V`, etc.).
    *   *Mapping:* Corresponds to the hardware's "BELOW" and "ABOVE" patching concept.
*   **Gate Length:** `uint8_t gateLength` (0-100%).
    *   *Behavior:* Controls the duration of the trigger pulse relative to the step length or clock.
*   **Reset Mode:** `Types::RunMode` (existing enum: `Free`, `OneShot`, `Loop`).
    *   *Reuse:* Matches Performer's standard sequencer transport logic.

### 1.2 Structure Update
```cpp
class DiscreteMapSequence {
public:
    // ... existing ...

    // Reuse Types::VoltageRange for input scaling
    Types::VoltageRange inputRange() const;
    void setInputRange(Types::VoltageRange range);

    // Gate Length (0-100%)
    int gateLength() const;
    void setGateLength(int length);

    // Reuse Types::RunMode (OneShot = Once, Loop = Loop)
    Types::RunMode runMode() const;
    void setRunMode(Types::RunMode mode);

    // ... serialization updates ...
};
```

## 2. Engine Logic (`DiscreteMapTrackEngine.cpp`)

### 2.1 Input Processing
*   **Voltage Scaling:**
    *   Use `Types::voltageRangeMin(range)` and `Types::voltageRangeMax(range)` to get the bounds.
    *   Scale the incoming signal `getRoutedInput()` from this range to the normalized internal 0.0-1.0 float range used for threshold comparison.
*   **Clock Source:**
    *   *Internal:* Use `ClockSetup`'s system tick and the Sequence's `divisor` to drive the internal ramp.
    *   *External:* Use the routed CV input directly.

### 2.2 Crossing Detection (High-Res)
To handle audio rates (VCO usage) and fast modulation:
*   **Trajectory Check:** Instead of just checking `Current > Threshold`, check if the *segment* `[Previous, Current]` intersects the `Threshold`.
*   **Directionality:** Respect the `Rise/Fall` setting by checking the sign of `(Current - Previous)`.

### 2.3 Gate Output
*   **Logic:**
    *   On Stage Change (Crossing): Set `_gateDuration = gateLength * ticksPerStep`. Set Gate Output HIGH.
    *   On Tick: Decrement `_gateDuration`. If 0, Set Gate Output LOW.
    *   **Retrigger:** If a new stage activates while Gate is HIGH, reset the timer (retrigger behavior).

## 3. Routing System (`Routing.h`)

Reuse the powerful `Routing` system to emulate the hardware "Expander" CV inputs.

### 3.1 New Targets
*   **Target::DiscreteMapInput:** Modulate the main Input Voltage (X).
*   **Target::DiscreteMapThreshold:** Global bias for all thresholds (emulates Expander's "Group" modulation).

## 4. User Interface

### 4.1 Sequence Page (`DiscreteMapSequencePage`)
*   **Visualization:**
    *   **Bar Graph:** Input Voltage (vertical cursor) vs Thresholds (vertical lines).
    *   **Range Indication:** Show min/max voltage labels based on `inputRange`.
*   **Footer Controls:**
    *   **F1 (Clock):** Toggle Internal/External.
    *   **F2 (Mode):** Toggle Position/Length.
    *   **F3 (Range):** Cycle `VoltageRange` (-5..5, 0..5, 0..10).
    *   **F4 (Gate):** Hold + Encoder -> Adjust `GateLength`.

### 4.2 Edit Page (`DiscreteMapStagesPage`)
*   **Reuse:** Similar to `TuesdayEditPage` or `CurveTrackPage`.
*   **Layout:** List view for precise numerical editing of:
    *   Threshold Values (0-255).
    *   Note Values (C3, D#4...).
    *   Directions (Rise/Fall/Off).
    *   Gate Length.

## 5. Implementation Steps

1.  **Model:** Update `DiscreteMapSequence.h` with `VoltageRange`, `GateLength`, `RunMode`. Update serialization.
2.  **Engine:** Implement scaling logic and gate timer in `DiscreteMapTrackEngine.cpp`.
3.  **UI:** Update `DiscreteMapSequencePage.cpp` footer and drawing. Implement `DiscreteMapStagesPage.cpp` (if not fully featured yet).
4.  **Routing:** Register new targets in `Routing.h/cpp` (if needed, otherwise reuse existing).
