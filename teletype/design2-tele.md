# Teletype Integration: Consolidated Design

## 1. Executive Summary & Core Decisions

This document synthesizes the architectural analysis for porting the Monome Teletype interpreter to the Performer sequencer.

**Primary Constraint:** Performer allocates track memory using a `Container` union sized to the largest track type (`NoteTrack` ≈ 11.2 KB).
**Core Design Decision:** We **MUST** compile the Teletype core without Grid support (`#define NO_GRID`).
- **With Grid:** ~15 KB (Exceeds container, requires complex external memory management).
- **Without Grid:** ~9.2 KB (Fits inside `NoteTrack` footprint).
- **Result:** Teletype becomes a standard, "free" track type in terms of memory allocation, leveraging the existing pre-allocated slot.

## 2. Architecture Overview

### 2.1 Model Layer (`TeletypeTrack`)
Located in `src/apps/sequencer/model/TeletypeTrack.h`.
Holds the serializable state and configuration.

*   **State:** Contains `scene_state_t` (stripped of Grid), script text buffers, and pattern data.
*   **Parity Properties:** Implements standard Performer track properties to ensure consistent behavior:
    *   `Divisor`: Syncs Metro to transport (see Clocking below).
    *   `Octave` / `Transpose`: Applied to `N` op output.
    *   `Root Note` / `Scale`: Applied to quantization ops.
*   **Routing Configuration:** Stores mappings for Input Triggers (which Performer event fires which script).

### 2.2 Engine Layer (`TeletypeTrackEngine`)
Located in `src/apps/sequencer/engine/TeletypeTrackEngine.h`.
Wraps the C-based Teletype VM.

*   **VM Instance:** Owns the `teletype` struct and executes `tele_run_script`.
*   **Update Loop:**
    *   `update(dt)`: Accumulates time for Teletype's 10ms internal tick (delays/slewing).
    *   `tick(tick)`: Handles transport-synced triggers (Metro, Step events).
*   **Hardware Bridge:** Implements a `TeletypeHardwareAdapter` to map `tele_cv`/`tele_tr` calls to:
    1.  The track's physical outputs.
    2.  The global Routing System (as sources).

## 3. Code Structure & API

### 3.1 TeletypeTrack.h (Model)

```cpp
class TeletypeTrack {
public:
    // Input Routing: What triggers a script?
    enum InputSource {
        None,
        PerformerClock,        // Syncs to Divisor
        GateIn1, GateIn2, GateIn3, GateIn4,
        StepButton,            // Manual trigger via UI
        FunctionButton,
        TrackMuteToggle,
        PatternChange,
        ExternalMidi,
    };

    struct ScriptTrigger {
        InputSource source = None;
        uint8_t edgePolarity = 0;  // 0=Rise, 1=Fall, 2=Both
        float threshold = 0.5f;    // For CV-based triggers
    };

    // CV Input Mapping: What feeds 'IN' and 'PARAM'?
    enum CvInputSource {
        CvIn1, CvIn2, CvIn3, CvIn4,
        StepEncoder,
        ProjectScale,
        ProjectTempo,
        TrackProgress,
        ExternalMidiCC,
    };

    // ... Getters/Setters for ScriptTrigger[8], CvInputSource[2] ...
    
    // Output Routing: Where do CV 1-4 / TR 1-4 go?
    enum CvOutputDestination {
        TrackCv1, TrackCv2, TrackCv3, TrackCv4,  // Local track outputs
        GlobalCv1, GlobalCv2, GlobalCv3, GlobalCv4,  // Global CV outputs
        MappedToRouting,  // Exposed as Routing Source only
        Disabled,
    };
    
    // ... Serialization ...
};
```

### 3.2 TeletypeTrackEngine.h (Engine)

```cpp
class TeletypeTrackEngine : public TrackEngine {
public:
    // Lifecycle
    void reset() override;
    void restart() override;
    TickResult tick(uint32_t tick) override;
    void update(float dt) override;

    // Output Interface
    float cvOutput(int index) const override;
    bool gateOutput(int index) const override;

    // Teletype Specific
    void runScript(uint8_t scriptIndex);
    void runCommand(const char *commandText); // For Live mode

    // Adapter Callbacks (called by Teletype C core)
    void setCvOutputInternal(uint8_t channel, float value, bool slew);
    void setGateOutputInternal(uint8_t channel, bool state);
    int16_t getCvInputInternal(uint8_t channel);
};
```

### 3.3 TeletypeHardwareAdapter.h (Bridge)

```cpp
class TeletypeHardwareAdapter {
public:
    static void setEngine(TeletypeTrackEngine *engine) { s_engine = engine; }

    // Map 'CV x y' op to Performer
    static void setCvOutput(uint8_t channel, int16_t value, bool slew);
    
    // Map 'TR x y' op to Performer
    static void setTrOutput(uint8_t channel, bool state);
    
    // Map 'IN' / 'PARAM' ops
    static int16_t getCvInput(uint8_t channel);

    // Map 'TIME' / 'M' ops
    static uint32_t getTicks(); // Returns engine ticks

private:
    static TeletypeTrackEngine *s_engine;
};
```

## 4. Memory & Resource Management

**Target:** < 11.2 KB per track.

| Component | Size (Est) | Notes |
| :--- | :--- | :--- |
| **Variables** | ~0.5 KB | A-Z, internal state |
| **Patterns** | ~0.5 KB | 4 x 64 steps |
| **Scripts** | ~4.5 KB | Text + Tokenized forms (11 scripts) |
| **Delay Queue** | ~3.0 KB | `scene_delay_t` (Queue size may need tuning if tight) |
| **Stack/Ops** | ~0.7 KB | Operational overhead |
| **Total** | **~9.2 KB** | **FITS** (Safe margin of ~2 KB) |

*Action Item:* Verify `scene_delay_t` size. If too large, reduce queue depth from 64 to 32.

## 5. Integration Logic

### 5.1 Clock & Metronome
Teletype's default Metro (`M`) is millisecond-based (free running). Performer is transport-based.

*   **Mode A: Synced (Default):**
    *   `M` variable is ignored/read-only.
    *   `METRO` script triggers based on **Track Divisor** (e.g., every 1/16th note).
*   **Mode B: Realtime:**
    *   `M` variable sets ms interval.
    *   `METRO` script triggers on internal timer accumulator.
    *   *Note:* Use case is generative/ambient patches independent of sequencer tempo.

### 5.2 Scale & Pitch
Teletype has internal quantization (`N`, `QT`).

*   **Override:** The `N` op (and `QT`) will be intercepted to use `model.project().selectedScale()`.
*   **Offset:** `Track::octave()` and `Track::transpose()` are added to the result of `N` before voltage conversion.
*   **Legacy Mode:** Option to disable Performer scaling to use raw Teletype 12TET behavior.

### 5.3 I/O Routing (The "Killer Feature")

**Outputs (Teletype -> Performer):**
*   **CV 1-4 / TR 1-4:** Directly write to the Track's own CV/Gate outputs.
*   **Routing System:** These outputs are registered as **Routing Sources**.
    *   *Example:* Teletype Track 1 CV Output 1 -> Modulate Filter Cutoff on Track 2.

**Inputs (Performer -> Teletype):**
*   **Trigger Inputs (1-8):** Mapped via a "Trigger Map" page.
    *   *Sources:* Clock, Gate In 1-4, Step Buttons, MIDI RX, Transport Start/Stop.
*   **CV Inputs (Param/In):** Mapped via standard Routing Targets.
    *   `Teletype IN` and `Teletype PARAM` are exposed as Routing Targets.
    *   *Example:* LFO -> Teletype IN.

**Variable Modulation:**
*   **Variables A-Z** are exposed as **Routing Targets**.
    *   *Example:* Velocity -> Teletype Var A. Script: `CV 1 V A` (Velocity controls voltage).

## 6. User Interface (256x64 Split Layout)

Leveraging the wide screen to show Context + Code.

### 6.1 Layout Strategy
**Split Pane View:**
*   **Left (Editor):** 6 lines of code. Line numbers. Cursor.
*   **Right (Monitor):** Tabbed views.
    *   *Tab 1 (Live):* State of CV 1-4, TR 1-4, Vars X/Y/Z.
    *   *Tab 2 (Help):* Contextual help for the op under cursor.
    *   *Tab 3 (Patterns):* Mini tracker view of Pattern data.

### 6.2 Input & Interaction
*   **Encoder 1:** Vertical Scroll (Line Select).
*   **Encoder 2:** Horizontal Scroll (Cursor Move).
*   **Encoder 3:** Token/Character Selector.
    *   *Behavior:* Scrolls efficiently through alphabet + common ops (`ADD`, `SUB`, `CV`, `TR`, `IF`).
*   **Step Buttons 1-8:**
    *   *Press:* Jump to Script 1-8.
    *   *Shift+Press:* Manually Trigger Script 1-8 (Great for testing/Live).
*   **F-Keys (Function Buttons):**
    *   **F1 (Context Menu):** Opens "Op Palette" or "Edit Actions" (Copy/Paste Line).
    *   **F2 (View Toggle):** Switches Right Pane (Live Monitor <-> Pattern View).
    *   **F3 (Run):** Execute current line immediately (REPL style).

**T9 / Shortcuts (Alternative Input):**
*   While editing, Step Buttons 1-8 can act as T9-style character entry for rapid typing if Encoder 3 is too slow.
    *   *Btn 1:* A-C, *Btn 2:* D-F, etc.
    *   *Context:* Long press for Ops Menu shortcut.

## 7. Storage & Library

*   **Project Storage:** Script text is saved inside the `.ppr` file for portability.
*   **Library Storage:** Support loading/saving standalone `.tt` files (Teletype format) to SD card (`/SCRIPTS/`).
    *   Allows building a library of algorithmic behaviors shared across projects.
    *   **Format:** Standard Monome Teletype text format (compatible with ecosystem).

## 8. Testing Strategy

### 8.1 Unit Tests (`src/tests/unit/sequencer/TestTeletypeTrack.cpp`)
Focus on logic and arithmetic ops without hardware dependencies.
*   **Case "default_values":** Assert default Metro/Routing state.
*   **Case "op_math":** Run script `A ADD 5 5`, assert `ss.variables[A] == 10`.
*   **Case "op_pattern":** Write to pattern `P 0 0 100`, read back.

### 8.2 Integration Tests (`src/tests/integration/sequencer/TestTeletypeEngine.cpp`)
Focus on Engine <-> Hardware Adapter interaction.
*   **Case "cv_output":**
    *   Run command `CV 1 8192`.
    *   Assert `engine.cvOutput(0) ≈ 0.0V` (Midpoint).
*   **Case "gate_output":**
    *   Run command `TR 1 1`.
    *   Assert `engine.gateOutput(0) == true`.
*   **Case "metro_trigger":**
    *   Set Divisor to 1/16.
    *   Call `tick()` 6 times (advance clock).
    *   Assert `METRO` script ran.

## 9. Implementation Plan (6 Weeks)

**Phase 1: Core VM & Memory Validation (Weeks 1-2)**
1.  Port `teletype` source tree.
2.  Define `NO_GRID`.
3.  Create empty `TeletypeTrack` model.
4.  **Verify compilation & RAM usage.** Ensure it fits in `Container`.
5.  Write Unit Tests for basic Math Ops.

**Phase 2: Basic Engine & I/O (Weeks 3-4)**
1.  Implement `TeletypeTrackEngine`.
2.  Implement `TeletypeHardwareAdapter` (map `tele_cv` to console/debug first).
3.  Connect `tick()` to run Script 1.
4.  Implement `TeletypePerformerBridge` for Scale/Note quantization integration.

**Phase 3: UI & Editor (Week 5)**
1.  Implement Split-Pane Render.
2.  Implement basic text editing (Encoder scrolling).
3.  Add "Live Monitor" view.

**Phase 4: Full Integration (Week 6)**
1.  Connect Output Routing (CV/TR to Track outputs).
2.  Connect Input Routing (Routing Targets -> Variables).
3.  Final Polish: Standalone `.tt` file loading/saving.