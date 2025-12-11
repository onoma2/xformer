# Research: Porting ER-101 Indexed Sequencing to Performer

This document outlines the research and architectural plan for implementing an ER-101 style "Indexed Track" within the Performer sequencer project.

## 1. Core Concept: The "Indexed" Paradigm

The key differentiator of the ER-101 is that it separates **Sequence Data** (the shape of the melody) from **Voltage Data** (the actual pitches/values).

| Feature | Performer (Current `NoteTrack`) | ER-101 (Target `IndexedTrack`) |
| :--- | :--- | :--- |
| **Value Storage** | Stores **Musical Note** (e.g., C#3). | Stores **Index** (0-99). |
| **Value Interpretation** | Calculated via `Scale` class + Root + Transpose. | Looked up in a raw **Voltage Table** (array of 100 values). |
| **Structure** | Fixed Array of Steps. | Linked List (conceptually), but we will likely map this to Performer's fixed array structure for simplicity first. |
| **Logic** | Probabilistic (Chance, Conditions). | Deterministic (Math Transforms). |
| **Timing** | Tick-based (PPQN) with Groove/Swing. | Pulse-based (Clock Pulses) with Multiplier/Divider. |
| **Glide** | "Slide" (Time-based). | "Smoothing" (Linear Interpolation between steps). |

## 2. Data Structures

We need to introduce new data structures to support this paradigm.

### 2.1 Voltage Table (`VoltageTable`)
A raw lookup table replacing the `Scale` class.
```cpp
struct VoltageTable {
    static constexpr int Size = 100;
    uint16_t values[Size]; // Raw DAC values (0-65535) or millivolts (0-10000)

    void setVoltage(int index, float volts);
    float getVoltage(int index) const;
};
```

### 2.2 Indexed Step (`IndexedStep`)
The atomic unit of the sequence.
```cpp
struct IndexedStep {
    uint8_t voltageIndex; // 0-99, Index into VoltageTable
    uint16_t duration;    // Length in ER-Pulses (0-99)
    uint16_t gateLength;  // Gate high time in ER-Pulses (0-99)
    // uint8_t cvBIndex;  // For dual-CV tracks (Future)
    
    // Flags
    bool ratchet;         // Retrigger gate
    bool smooth;          // Glide to next step
};
```

### 2.3 Indexed Track (`IndexedTrack`)
The container for the sequence data.
```cpp
class IndexedTrack {
public:
    VoltageTable table; // The track owns its voltage table
    
    // Timing Configuration
    uint8_t clockDivider;   // 1-99
    uint8_t clockMultiplier; // 1-99
    
    // ... Sequence data (Patterns/Steps) ...
};
```

### 2.4 Integration with Performer Scales
In Performer, Scales are typically global/project-wide settings. In the Indexed Track paradigm, the `VoltageTable` *is* the scale.

To bridge these concepts without compromising the deterministic nature of the Indexed Track:
*   **Playback:** The track operates purely on its internal `VoltageTable`. It ignores the Project Scale setting during playback.
*   **Generation:** We will provide a "Fill Table from Scale" utility in the Table Editor.
    *   This allows the user to initialize a `VoltageTable` using Performer's built-in scales (e.g., "C Minor").
    *   The function will calculate the voltages for the first 100 degrees of the selected scale and write them to the table.
    *   Once written, the values are independent; changing the Project Scale later will not affect the track.

## 3. Engine Logic (`IndexedTrackEngine`)

We need a new engine class inheriting from `TrackEngine`. The critical change is the `tick` function, which must handle the Pulse-based timing mapped to Performer's 192 PPQN system.

### 3.1 Timing Formula
To map ER-101 pulses to Performer ticks:
*   **1 ER-Pulse** = **48 Performer Ticks** (Standard 16th note).

**Accumulator Approach:**
To handle polyrhythmic multipliers (e.g., Mult=7) without drift, we use an error accumulator instead of simple division.

```cpp
void IndexedTrackEngine::tick(uint32_t absoluteTick) {
    // 1. Advance Sub-Tick Accumulator
    // We add 'Multiplier' speed every tick.
    _subTickAccumulator += _track.multiplier();
    
    // 2. Calculate Threshold for 1 ER-Pulse
    // Threshold = BasePulse (48) * Divider
    uint32_t ticksPerPulse = 48 * _track.divider();
    
    // 3. Check for Pulse Completion
    if (_subTickAccumulator >= ticksPerPulse) {
        _subTickAccumulator -= ticksPerPulse; // Keep remainder to preserve phase
        
        // We have completed 1 Real ER-Pulse
        _erPulseCounter++;
        
        // 4. Check Step Duration
        const auto& step = _sequence->step(_currentStepIndex);
        if (_erPulseCounter >= step.duration) {
            advanceStep();
            _erPulseCounter = 0;
            
            // Update CV logic (Smoothing handled here)
        }
        
        // 5. Handle Output (Gate & CV)
        // ER-101 Logic: Smoothing happens AFTER gate goes low.
        
        bool gateActive = _erPulseCounter < step.gateLength;
        _engine.midiOutputEngine().sendGate(_track.trackIndex(), gateActive);
        
        float currentVolts = _track.table.getVoltage(step.voltageIndex);
        
        if (step.smooth && !gateActive) {
            // GLIDE LOGIC:
            // If smoothing is on, hold voltage while Gate is High.
            // When Gate goes Low, glide to the NEXT step's voltage over the remaining duration.
            
            // Calculate interpolation factor (0.0 to 1.0) based on time since Gate Off
            // ... interpolation logic ...
            // _engine.midiOutputEngine().sendCv(_track.trackIndex(), interpolatedVolts);
        } else {
            _engine.midiOutputEngine().sendCv(_track.trackIndex(), currentVolts);
        }
    }
}
```

### 3.2 Math Engine (Transforms)
The ER-101 relies heavily on destructive Math Transforms. The `IndexedSequence` class should support these operations on ranges of steps.

**Core Operations:**
Derived from `er-101/src/sequencer.c` (`do_transform`):

1.  **ARITHMETIC:** `Value += Operand`
2.  **GEOMETRIC:** `Value *= Operand` (or `Value /= -Operand` if Operand < 0)
3.  **SET:** `Value = Operand`
4.  **RANDOM:** `Value = Random(0, Operand)`
5.  **JITTER:** `Value += Random(-Operand, Operand)`
6.  **QUANTIZE:** `Value -= (Value % Operand)` (Snap to grid)

**Target Parameters:**
Each operation can be targeted independently at:
*   **CV Index** (Voltage Table Index)
*   **Duration** (Pulses)
*   **Gate Length** (Pulses)

**Implementation:**
We will implement a `applyTransform` method in `IndexedSequence` that iterates over the selected steps (or entire pattern) and applies the chosen logic to the target fields.

```cpp
// Example Logic
void applyTransform(Layer layer, TransformOp op, int operand) {
    for (auto& step : selectedSteps) {
        int val = step.getLayer(layer);
        // ... switch(op) logic ...
        step.setLayer(layer, newVal);
    }
}
```

## 4. User Interface Requirements

### 4.1 Sequence Editor
*   **Display:** Needs to show `IDX` (Index) instead of Note Names.
*   **Controls:** Encoders should change `Duration` (Pulses) and `Index` (0-99).

### 4.2 Table Editor
*   **New Page:** A dedicated screen to edit the `VoltageTable`.
*   **Layout:** A simple scrolling list of 100 slots.
    *   Left Encoder: Scroll Index (0-99).
    *   Right Encoder: Adjust Voltage (Fine/Coarse).
    *   *Bonus:* Visualization of the curve.

### 4.3 Track Settings
*   Need to expose `Clock Divider` and `Clock Multiplier` in the track setup menu.

## 5. Hardware & Architecture Notes
*   **ER-101 Architecture:** Uses linked lists and memory pools. For Performer, it is safer to stick to fixed arrays first to match the existing memory model, unless dynamic length is strictly required.
*   **Output:** ER-101 writes directly to DACs via DMA. Performer writes to an Event Queue (`MidiOutputEngine`). We will use the latter.
*   **Resolution:** ER-101 is 14-bit (0-16383). Performer's internal resolution handles floats, so we can store voltages as `float` (0.0 - 10.0) or `uint16_t` depending on memory constraints.

### 5.1 Advanced Features (Future)
*   **Parts:** High-level snapshots of loop points and reset positions for all tracks.
*   **Groups:** Arbitrary collections of steps for targeted math operations.
*   **Modulation:** Non-destructive math modifiers driven by external CV (ER-102 feature).

## 6. Implementation Roadmap
1.  **Data Model:** Implement `IndexedStep`, `IndexedTrack`, `VoltageTable`.
2.  **Engine:** Implement `IndexedTrackEngine` with the Accumulator timing logic and **Post-Gate Smoothing**.
3.  **Math:** Implement `IndexedSequence::applyTransform` for bulk editing.
4.  **Integration:** Add `Indexed` to `TrackMode` enum and hook up the engine.
5.  **UI - Engine Test:** Hardcode a table and verify timing/pitch via standard output.
6.  **UI - Table Editor:** Build the UI to edit the voltage table.
7.  **UI - Sequencer:** Build the step editor UI.
