# Meadow & Kria Porting Guide for Performer

This document outlines the architectural plan for implementing "Meadowphysics" (Meadow Track) and "Kria" (Kria Track) engines within the Westlicht Performer firmware.

## 1. Meadow Track (Rhizomatic Counter Engine)

**Inspiration:** Monome Meadowphysics.
**Goal:** A generative trigger/modulation source where 8 internal counters interact via rules.

### Data Model
**File:** `src/apps/sequencer/model/MeadowSequence.h`

The `MeadowSequence` does not use linear steps. Instead, it holds configuration for 8 "Lanes".

```cpp
struct MeadowSequence {
    static const int NumLanes = 8;
    
    enum RuleType {
        None = 0,      // No rule
        Inc = 1,       // Increment target
        Dec = 2,       // Decrement target
        Max = 3,       // Set target to maximum
        Min = 4,       // Set target to minimum
        Rnd = 5,       // Set target to random value
        Pole = 6,      // Set target to closest pole (boundary)
        Stop = 7       // Stop target lane
    };
    
    struct Lane {
        uint8_t count;                    // Reset target value
        int8_t position;                  // Current counter value (-1 = stopped)
        int8_t speed;                     // Countdown speed (0-255)
        uint8_t tick;                     // Current countdown value
        uint8_t min;                      // Minimum value
        uint8_t max;                      // Maximum value
        uint8_t rule;                     // Rule type (0-7)
        uint8_t ruleDest;                 // Rule destination lane (0-7)
        uint8_t ruleDestTargets;          // Rule destination targets (bit 0=count, bit 1=speed)
        uint8_t triggerMask;              // Bitmask of lanes to trigger
        uint8_t toggleMask;               // Bitmask of lanes to toggle
        uint8_t syncMask;                 // Bitmask of lanes to sync reset
        uint8_t speedMin;                 // Minimum speed value
        uint8_t speedMax;                 // Maximum speed value
    };

    std::array<Lane, NumLanes> lanes;
    uint8_t soundMode;                    // Global sound mode flag

    // Serialization logic...
};
```

### Engine
**File:** `src/apps/sequencer/engine/MeadowTrackEngine.h/cpp`

The engine maintains the runtime state of the 8 counters.

*   **State:**
    *   `int32_t _ticks[8];` (Time remaining until next decrement)
    *   `int32_t _positions[8];` (Current counter value, -1 = stopped)
*   **Update Loop:**
    *   Iterate all 8 lanes.
    *   Decrement `_ticks`. If zero:
        *   Reset `_ticks` to `speed`.
        *   Decrement `_positions`.
        *   If `_positions` reaches 0:
            *   **Trigger Event:** Set internal gate flag for this lane.
            *   **Execute Rule:** Apply the selected rule (Inc/Dec/etc) to the `_positions` or `speed` of targets defined in `ruleDest` and `ruleDestTargets`.
            *   Reset `_positions` to `max`.
            *   **Sync:** If sync is enabled, reset destination lanes to their count values
            *   **Trigger/Toggle:** Trigger or toggle destination lanes as specified
*   **Rule Implementation:**
    *   **None (0):** No action
    *   **Inc (1):** Increment target count or speed
    *   **Dec (2):** Decrement target count or speed
    *   **Max (3):** Set target count or speed to maximum
    *   **Min (4):** Set target count or speed to minimum
    *   **Rnd (5):** Set target count or speed to random value in range
    *   **Pole (6):** Set target count or speed to closest boundary
    *   **Stop (7):** Stop target lane (set position to -1)
*   **Output Generation:**
    *   **Gate:** Configurable. Default: Logical OR of all 8 lanes. Or mapped via Routing Matrix.
    *   **CV:** Could map "Last Triggered Lane Index" to DAC (0V-5V stepped) or "Sum of active gates".

### Rule Execution Logic
```cpp
void executeRule(int laneIndex) {
    switch(_sequence.lanes[laneIndex].rule) {
        case 0: // No rule
            break;
        case 1: // Increment
            if(_sequence.lanes[laneIndex].ruleDestTargets & 1) { // Target count
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count++;
                if(_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count > 
                   _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].max) {
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count = 
                        _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].min;
                }
            }
            if(_sequence.lanes[laneIndex].ruleDestTargets & 2) { // Target speed
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed++;
                if(_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed > 
                   _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMax) {
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed = 
                        _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMin;
                }
            }
            break;
        case 2: // Decrement
            if(_sequence.lanes[laneIndex].ruleDestTargets & 1) { // Target count
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count--;
                if(_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count < 
                   _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].min) {
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count = 
                        _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].max;
                }
            }
            if(_sequence.lanes[laneIndex].ruleDestTargets & 2) { // Target speed
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed--;
                if(_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed < 
                   _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMin) {
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed = 
                        _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMax;
                }
            }
            break;
        case 3: // Max
            if(_sequence.lanes[laneIndex].ruleDestTargets & 1) {
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count = 
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].max;
            }
            if(_sequence.lanes[laneIndex].ruleDestTargets & 2) {
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed = 
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMax;
            }
            break;
        case 4: // Min
            if(_sequence.lanes[laneIndex].ruleDestTargets & 1) {
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count = 
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].min;
            }
            if(_sequence.lanes[laneIndex].ruleDestTargets & 2) {
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed = 
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMin;
            }
            break;
        case 5: // Random
            if(_sequence.lanes[laneIndex].ruleDestTargets & 1) {
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count = 
                    (rand() % (_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].max - 
                               _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].min + 1)) + 
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].min;
            }
            if(_sequence.lanes[laneIndex].ruleDestTargets & 2) {
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed = 
                    (rand() % (_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMax - 
                               _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMin + 1)) + 
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMin;
            }
            break;
        case 6: // Pole
            if(_sequence.lanes[laneIndex].ruleDestTargets & 1) {
                if(abs(_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count - 
                       _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].min) <
                   abs(_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count - 
                       _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].max)) {
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count = 
                        _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].max;
                } else {
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].count = 
                        _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].min;
                }
            }
            if(_sequence.lanes[laneIndex].ruleDestTargets & 2) {
                if(abs(_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed - 
                       _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMin) <
                   abs(_sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed - 
                       _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMax)) {
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed = 
                        _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMax;
                } else {
                    _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speed = 
                        _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].speedMin;
                }
            }
            break;
        case 7: // Stop
            if(_sequence.lanes[laneIndex].ruleDestTargets & 1) {
                _sequence.lanes[_sequence.lanes[laneIndex].ruleDest].position = -1; // Stop the destination
            }
            break;
    }
}
```

### UI
**File:** `src/apps/sequencer/ui/pages/MeadowSequenceEditPage.h/cpp`

*   **Visuals:** A list view is best suited for the 8 lanes.
    *   **Row:** Lane 1-8.
    *   **Cols:** Speed | Range (Min-Max) | Rule | Targets.
*   **UI Interaction Patterns:**
    *   **Mode 0 (Position/Range):** Single press sets position, double press sets range
    *   **Mode 1 (Speed/Connections):** Long press column 1 to enter Mode 2
    *   **Mode 2 (Rules/Destinations):** Rule and destination editing
*   **Controls:**
    *   **Encoders:** Modify parameters of the selected lane.
    *   **Step Buttons (1-8):** Toggle `targetMask` bits for the selected lane (fastest way to set destinations).
    *   **LEDs:** Visualize the countdown of each lane (brightness or blinking).

---

## 2. Kria Track (Polymetric Sequencer)

**Inspiration:** Monome Kria.
**Goal:** A melodic sequencer where musical parameters (Note, Octave, Duration, Trigger, Transpose, Scale) run on independent loops.

### Data Model
**File:** `src/apps/sequencer/model/KriaSequence.h`

The data structure must decouple the parameters. Standard `NoteSequence` uses a struct of arrays (SoA) or array of structs (AoS) where index `i` is the same for all. `KriaSequence` needs independent sequences.

```cpp
struct KriaSequence {
    static const int Length = 16; // Steps per parameter (16 steps)
    static const int NumParams = 7; // Number of parameters

    enum TimeParam {
        tTr,    // Trigger
        tAc,    // Accent  
        tOct,   // Octave
        tDur,   // Duration
        tNote,  // Note
        tTrans, // Transposition
        tScale  // Scale
    };

    struct Lane {
        std::array<uint8_t, Length> data;  // The actual parameter data
        uint8_t loopStart;                 // Loop start position
        uint8_t loopEnd;                   // Loop end position  
        uint8_t loopLength;                // Calculated loop length
        bool loopSwap;                     // Loop swap flag for wrapping
        uint8_t clockDiv;                  // Speed multiplier relative to master
        uint8_t playMode;                  // Currently not used in original, but could be added
    };

    Lane trigger;    // tTr: Trigger states
    Lane accent;     // tAc: Accent states  
    Lane octave;     // tOct: Octave values
    Lane duration;   // tDur: Duration values
    Lane note;       // tNote: Note values
    Lane transpose;  // tTrans: Transposition values
    Lane scale;      // tScale: Scale values
    
    uint8_t dur_mul; // Duration multiplier affecting all durations
    
    // Additional parameters for dual track support
    uint8_t scales[42][7]; // Scale definitions
    uint8_t pscale[7];     // Pattern-specific scale mapping

    // Serialization logic...
};
```

### Engine
**File:** `src/apps/sequencer/engine/KriaTrackEngine.h/cpp`

*   **State:**
    *   `int _playhead[7];` (Independent indices for Tr, Ac, Oct, Dur, Note, Trans, Scale)
    *   `int _tickAccumulator[7];` (For speed handling)
    *   `uint8_t _trans[2];` (Current transposition values for tracks 0 and 1)
    *   `uint8_t _sc[2];` (Current scale values for tracks 0 and 1)
    *   `uint16_t _dur[2];` (Current duration values for tracks 0 and 1)
    *   `uint8_t _oct[2];` (Current octave values for tracks 0 and 1)
    *   `uint8_t _note[2];` (Current note values for tracks 0 and 1)
    *   `uint8_t _ac[2];` (Current accent values for tracks 0 and 1)
    *   `uint8_t _tr[2];` (Current trigger states for tracks 0 and 1)
*   **Update Loop:**
    *   For each parameter type (Tr, Ac, Oct, Dur, Note, Trans, Scale):
        *   Advance `_tickAccumulator`. If threshold met (based on `clockDiv`):
            *   Advance `_playhead` respecting `loopStart`, `loopEnd`, and `loopSwap` for wrapping.
    *   **Synthesis:**
        *   Fetch values: `n = note.data[_playhead[4]]`, `o = octave.data[_playhead[2]]`, etc.
        *   Combine into a single "Note Event".
        *   `OutputPitch = Scale(n) + (o * 12) + (trans & 0xf) + ((trans >> 4)*5)`.
        *   `OutputGate = trig.data[_playhead[0]]`.
        *   `OutputLength = duration.data[_playhead[3]]`.
*   **Parameter Processing:**
    *   **tTrans:** Updates transposition values
    *   **tScale:** Updates scale and recalculates scale mapping if changed
    *   **tDur:** Updates duration values (multiplied by dur_mul)
    *   **tOct:** Updates octave values
    *   **tNote:** Updates note values
    *   **tAc:** Updates accent values
    *   **tTr:** Triggers gate output if value is non-zero

### Parameter Advance Logic
```cpp
bool advanceParameter(uint8_t track, TimeParam param) {
    _pos_mul[track][param]++;
    
    if(_pos_mul[track][param] >= _sequence[track].tmul[param]) {
        if(_pos[track][param] == _sequence[track].lend[param]) {
            _pos[track][param] = _sequence[track].lstart[param];
        } else {
            _pos[track][param]++;
            if(_pos[track][param] > 15) _pos[track][param] = 0;
        }
        _pos_mul[track][param] = 0;
        return true;
    }
    return false;
}
```

### UI
**File:** `src/apps/sequencer/ui/pages/KriaSequenceEditPage.h/cpp`

*   **View Mode:** Since we have 7 independent timelines, we can't show them all in detail at once.
    *   **"Focus" Mode:** User selects a generic "Lane" (Note, Octave, etc.) to edit on the main 16 steps.
    *   **"Overview" Mode:** OLED shows 7 horizontal bars indicating relative playhead positions.
*   **UI Modes:**
    *   **mTr:** Trigger/Accent/Octave editing
    *   **mDur:** Duration editing  
    *   **mNote:** Note editing
    *   **mScale:** Scale editing
    *   **mTrans:** Transposition editing
    *   **mScaleEdit:** Scale definition
    *   **mPattern:** Pattern selection
*   **Modification Modes:**
    *   **modNone:** Normal editing
    *   **modLoop:** Loop editing mode
    *   **modTime:** Time multiplication editing
*   **Controls:**
    *   **Function + Encoder:** Select active Lane (Tr/Ac/Oct/Dur/Note/Trans/Scale).
    *   **Loop Keys:** Holding "Loop" + pressing Step 1 & Step 4 sets the loop points *for the currently active lane only*.
    *   **Step Buttons:** Edit the step values for the active lane.

### 3. Dual Track Support

Both Kria and Meadowphysics implementations support 2 independent tracks:

*   **Kria:** Each track has its own set of parameter sequences and playheads
*   **Meadowphysics:** Each track operates independently with its own counter states

### 4. Scale System

Kria includes a sophisticated scale system:

*   **42 different scales** available
*   **Pattern-specific scale mapping** (pscale array)
*   **Scale calculation** with ET (equal temperament) conversion

### 5. Preset System

Both implementations support preset storage:

*   **8 presets** available
*   **Glyph selection** for preset identification
*   **Flash storage** for persistent preset data

### Integration Steps (General)

1.  **Define Track Mode:** Add `Meadow` and `Kria` to `Track::TrackMode` enum.
2.  **Container:** Update `TrackSequenceContainer` (in `Track.h`) to include `MeadowSequence` and `KriaSequence`.
3.  **Engine Factory:** Update `Engine::createTrackEngine` to instantiate the new engines.
4.  **UI Dispatch:** Update `SequenceEditPage` (or `PageManager`) to route to the correct Edit Page based on track mode.
5.  **Dual Track Support:** Implement dual track state management in engines
6.  **Preset System:** Implement preset storage and recall functionality
7.  **Scale System:** Implement scale calculation and mapping for Kria
