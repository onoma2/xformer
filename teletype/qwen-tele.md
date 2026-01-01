# Teletype Integration for Performer: Qwen Implementation Plan

## Resource Feasibility Assessment

Based on analysis of the Performer's current resource usage patterns, the Teletype implementation faces significant memory constraints:

### Memory Constraints Analysis

**Current Memory Usage:**
- **SRAM (128KB)**: Currently at ~86% utilization (~110KB used, ~18KB free)
- **CCMRAM (64KB)**: Currently at ~70% utilization (~45KB used, ~19KB free)
- **Flash (1024KB)**: Only ~41% utilized (~418KB used), with significant headroom

### Teletype Memory Requirements vs Available Resources

**Estimated Teletype Memory Usage:**
- **Script Storage**: ~6.2KB RAM per Teletype track (11 scripts × 6 lines × 64 chars + parsed structures)
- **Runtime State**: ~10-15KB per track (Teletype scene_state_t is ~10-15KB)
- **Total per track**: ~16-21KB

**Feasibility Assessment:**
- **Single Teletype Track**: Would consume ~16-21KB, which is feasible given the current ~18KB free SRAM
- **Multiple Teletype Tracks**: Would be challenging as 2 tracks would exceed available SRAM
- **Mixed Track Types**: Would require replacing other track types (e.g., Note tracks which use ~11.2KB each) to maintain memory balance

### How Performer Allocates RAM Per Track Type

The Performer uses a **union-based approach** where each track can only be one type at a time:

1. **Single Track Memory Structure**:
   - Each `Track` object contains a union that holds only one track type at a time
   - The `Container` template holds the actual track data
   - Only the memory for the active track type is used per track

2. **Per Track Type Memory Allocation**:
   - **Note Track**: ~11,225 bytes per track (sequence ~10,880 + engine ~345)
   - **Curve Track**: ~10,083 bytes per track (sequence ~9,792 + engine ~291)
   - **Tuesday Track**: ~2,231 bytes per track (sequence ~2,176 + engine ~704)
   - **Discrete Track**: ~2,314 bytes per track (sequence ~1,955 + engine ~359)
   - **Indexed Track**: ~3,341 bytes per track (sequence ~3,298 + engine ~43)
   - **Teletype Track**: Estimated ~16-21KB per track (scripts ~6.2KB + runtime state ~10-15KB)

3. **Total System Memory**:
   - **SRAM**: 128KB total, ~110KB currently used (~18KB free)
   - **CCMRAM**: 64KB total, ~45KB used (~19KB free)
   - **Flash**: 1024KB total, ~418KB used (~606KB free)

### Revised Feasibility Assessment

**Single Teletype Track Implementation**:
- **Memory Impact**: Would replace existing track type memory (e.g., if replacing a Note track: 11,225 bytes → 16-21KB)
- **Net Increase**: ~5-10KB additional memory usage per Teletype track
- **Feasibility**: Possible but would reduce available memory headroom from ~18KB to ~8-13KB

**Multiple Teletype Tracks**:
- **2 Teletype Tracks**: Would likely exceed available SRAM (would need ~32-42KB vs ~18KB available)
- **Feasibility**: Not feasible without removing other track types

**Mixed Track Configuration**:
- **Optimal Strategy**: Replace memory-intensive tracks (Note, Curve) with Teletype
- **Memory Efficiency**: Converting from Note to Teletype would add ~5-10KB per track
- **Maximum Configuration**: Could potentially support 1-2 Teletype tracks if replacing Note tracks

### Resource Considerations

**Memory Management**:
- The union-based design means memory is allocated per active track type only
- Adding Teletype would require careful memory budgeting
- May need to optimize other track types to accommodate Teletype

**CPU Usage**:
- Teletype interpreter execution: ~100-500 μs per script execution
- Engine task currently has 4KB stack (adequate for current load)
- CPU impact should be manageable with proper implementation

**Flash Memory**:
- Teletype core library: ~50-80KB (as estimated in qwen-tele.md)
- Available flash: ~606KB
- **Assessment**: Flash is not a constraint

### Recommendations

1. **Conservative Approach**: Implement single Teletype track as proof of concept
2. **Memory Optimization**: Consider reducing Teletype runtime state size by omitting unused features
3. **Selective Features**: Focus on core Teletype functionality, omit I2C/grid ops
4. **Track Type Management**: Allow users to configure track types to balance memory usage
5. **Dynamic Loading**: Consider loading Teletype runtime only when needed

### Final Feasibility Rating: **MODERATE**

The Teletype implementation is **marginally feasible** but would significantly impact the available memory headroom. The implementation would need to be carefully optimized and likely support only 1-2 Teletype tracks simultaneously. The union-based memory allocation system means each Teletype track would replace the memory of another track type, but the net memory increase would be substantial (5-10KB per track).

## Executive Summary

This plan outlines the integration of Teletype functionality as a track mode in the Performer sequencer firmware. The approach combines the architectural clarity of codex-tele.md with the implementation detail of claude-tele.md to create a comprehensive, practical plan for embedding the Teletype interpreter within the Performer ecosystem.

**Feasibility: MODERATE** - Core logic is portable, adapter layers are well-defined, but implementation faces significant memory constraints as detailed in the resource feasibility assessment above.

## Architecture Overview

### Core Integration Strategy

```
┌─────────────────────────────────────────────────────────────┐
│                    Performer Engine                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ TeletypeTrack (Model Layer)                          │  │
│  │  - Scripts (8 trigger + Metro + Init + Live)       │  │
│  │  - Patterns (4x64 arrays)                           │  │
│  │  - Variables (a,b,c,d,x,y,z,t, etc.)               │  │
│  │  - Input/Output routing configuration               │  │
│  │  - No Grid, No I2C data                            │  │
│  └───────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ TeletypeTrackEngine (Engine Layer)                  │  │
│  │  - tick() → run script based on trigger mapping    │  │
│  │  - update() → metro timer, delays, CV slewing     │  │
│  │  - CV/Gate output via hardware adapter             │  │
│  │  - Input routing and processing                     │  │
│  └───────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Teletype Core (Interpreter - mostly unchanged)      │  │
│  │  - parse() - Ragel scanner                         │  │
│  │  - process_command() - Op dispatcher               │  │
│  │  - tele_tick() - Delay/Wait handling               │  │
│  │  - scene_state_t - Runtime state                   │  │
│  └───────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Hardware Adapter Layer (Bridge to Performer)        │  │
│  │  - CV/Gate: tele_cv() → TrackEngine output         │  │
│  │  - Trigger In: TrackEngine::tick() → run_script() │  │
│  │  - Clock: Performer Clock → Metro/Tick             │  │
│  │  - CV In: Performer ADC → scene_state.in/param     │  │
│  └───────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ TeletypeTrackPage (UI Layer)                        │  │
│  │  - Script editor using step encoder + buttons      │  │
│  │  - Pattern tracker view                            │  │
│  │  - Variable monitor                                │  │
│  │  - Configuration page                              │  │
│  │  - Context menus for ops/commands                  │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Core Interpreter Integration

**Files to port unchanged:**
- `teletype/src/teletype.c/.h` - Main interpreter loop
- `teletype/src/command.c/.h` - Command structure
- `teletype/src/scanner.c` (Ragel-generated) - Tokenizer
- `teletype/src/match_token.c` - Op lookup
- `teletype/src/state.c/.h` - State management
- `teletype/src/script.h` - Script definitions

**Ops to include with modifications:**
- `teletype/src/ops/variables.c/.h` - General purpose registers
- `teletype/src/ops/patterns.c/.h` - Pattern arrays
- `teletype/src/ops/maths.c/.h` - Math operations
- `teletype/src/ops/stack.c/.h` - Stack operations
- `teletype/src/ops/queue.c/.h` - Queue operations
- `teletype/src/ops/controlflow.c/.h` - IF/WHILE/L loops
- `teletype/src/ops/delay.c/.h` - WAIT/DEL commands
- `teletype/src/ops/metronome.c/.h` - Metro timer
- `teletype/src/ops/init.c/.h` - Initialization operations
- `teletype/src/ops/hardware.c/.h` - CV/TR (needs adapter)
- `teletype/src/ops/midi.c/.h` - MIDI ops (route to Performer's MIDI)

**Ops to omit (I2C/grid dependent):**
- All I2C-dependent ops (`ansible.c`, `crow.c`, `disting.c`, etc.)
- `teletype/src/ops/grid_ops.c/.h` - Grid hardware specific
- `teletype/src/ops/i2c.c/.h` - I2C communication

### 2. Init and Metro Scripts Understanding

**Script Structure:**
- Regular Scripts: 8 trigger scripts (0-7) - triggered by external events
- Metro Script: Script 8 (METRO_SCRIPT) - runs at regular intervals
- Init Script: Script 9 (INIT_SCRIPT) - runs on initialization
- Delay Script: Script 10 (DELAY_SCRIPT) - for delayed execution
- Live Script: Script 11 (LIVE_SCRIPT) - for live interaction

**Init Script (Script 9):**
- Runs when the Teletype scene is loaded or when `INIT` command is executed
- Used for initializing variables, patterns, and setting initial states
- Can reset CV outputs, gate states, and other parameters to known values
- Operations like `INIT.CV.ALL`, `INIT.TR.ALL`, `INIT.DATA` can be used to reset specific components

**Metro Script (Script 8):**
- Runs at regular intervals defined by the `M` variable (in milliseconds)
- Can be enabled/disabled with `M.ACT` variable
- Used for continuous processes, LFOs, or periodic updates
- Interval can be set with `M x` where x is the interval in ms (minimum 25ms, or 2ms with M!)

**Hardware Access in Scripts:**
- CV outputs: `CV x y` where x is output (1-4) and y is voltage (0-16383)
- CV slew: `CV.SLEW x y` where y is slew time in ms
- Gate/trigger outputs: `TR x y` where x is output (1-4) and y is state (0/1)
- Gate pulse: `TR.PULSE x` pulses the gate for the duration set by `TR.TIME`
- CV inputs: `IN` returns current value of input
- Gate inputs: `STATE x` returns current state of input x
- Parameters: `PARAM` returns current value of parameter input

## Model Layer Design

### TeletypeTrack.h

```cpp
// src/apps/sequencer/model/TeletypeTrack.h
#pragma once

#include "Track.h"
#include "Serialize.h"

// Forward declare Teletype types
struct scene_state_t;
struct scene_variables_t;
struct scene_pattern_t;
struct scene_script_t;

class TeletypeTrack {
public:
    // Input routing configuration
    enum InputSource {
        None,
        PerformerClock,
        GateIn1, GateIn2, GateIn3, GateIn4,
        StepButton,
        FunctionButton,
        TrackMuteToggle,
        PatternChange,
        ExternalMidi,
    };

    struct ScriptTrigger {
        InputSource source = None;
        uint8_t edgePolarity = 0;  // 0=Rise, 1=Fall, 2=Both
        float threshold = 0.5f;    // For CV-based triggers

        void clear();
        void write(VersionedSerializedWriter &writer) const;
        void read(VersionedSerializedReader &reader);
    };

    enum CvInputSource {
        CvIn1, CvIn2, CvIn3, CvIn4,
        StepEncoder,
        ProjectScale,
        ProjectTempo,
        TrackProgress,
        ExternalMidiCC,
    };

    // Output routing configuration for Teletype CV/TR access
    enum CvOutputDestination {
        TrackCv1, TrackCv2, TrackCv3, TrackCv4,  // Local track outputs
        GlobalCv1, GlobalCv2, GlobalCv3, GlobalCv4,  // Global CV outputs
        GlobalCv5, GlobalCv6, GlobalCv7, GlobalCv8,  // Additional global outputs
        MappedToRouting,  // Mapped through Performer's routing system
        Disabled,  // Output disabled
    };

    enum GateOutputDestination {
        TrackGate1, TrackGate2, TrackGate3, TrackGate4,  // Local track gates
        GlobalGate1, GlobalGate2, GlobalGate3, GlobalGate4,  // Global gate outputs
        GlobalGate5, GlobalGate6, GlobalGate7, GlobalGate8,  // Additional global gates
        MappedToRouting,  // Mapped through Performer's routing system
        Disabled,  // Output disabled
    };

    //----------------------------------------
    // Properties
    //----------------------------------------

    // Script triggers (8 scripts + Metro + Init)
    const ScriptTrigger &scriptTrigger(int index) const {
        return _scriptTriggers[index];
    }
    void setScriptTrigger(int index, const ScriptTrigger &trigger);

    // Metro configuration
    int metroInterval() const { return _metroInterval; }
    void setMetroInterval(int ms) { _metroInterval = clamp(ms, 25, 10000); }

    bool metroEnabled() const { return _metroEnabled; }
    void setMetroEnabled(bool enabled) { _metroEnabled = enabled; }

    // CV Input mapping
    CvInputSource cvInputSource(int index) const { return _cvInputSources[index]; }
    void setCvInputSource(int index, CvInputSource source);

    // CV Input scaling
    float cvInputMin(int index) const { return _cvInputMin[index]; }
    void setCvInputMin(int index, float min) { _cvInputMin[index] = min; }

    float cvInputMax(int index) const { return _cvInputMax[index]; }
    void setCvInputMax(int index, float max) { _cvInputMax[index] = max; }

    // Output routing - allows Teletype scripts to access different Performer outputs
    CvOutputDestination cvOutputDestination(int teletypeChannel) const {
        return _cvOutputDestinations[teletypeChannel];
    }
    void setCvOutputDestination(int teletypeChannel, CvOutputDestination dest) {
        _cvOutputDestinations[teletypeChannel] = dest;
    }

    GateOutputDestination gateOutputDestination(int teletypeChannel) const {
        return _gateOutputDestinations[teletypeChannel];
    }
    void setGateOutputDestination(int teletypeChannel, GateOutputDestination dest) {
        _gateOutputDestinations[teletypeChannel] = dest;
    }

    // Scale integration
    bool usePerformerScale() const { return _usePerformerScale; }
    void setUsePerformerScale(bool use) { _usePerformerScale = use; }

    // Track-specific properties to make Teletype behave like other tracks
    int octave() const { return _octave; }
    void setOctave(int octave) { _octave = clamp(octave, -4, 4); }

    int transpose() const { return _transpose; }
    void setTranspose(int transpose) { _transpose = clamp(transpose, -48, 48); } // In semitones

    int divisor() const { return _divisor; }
    void setDivisor(int divisor) { _divisor = clamp(divisor, 1, 256); }

    int note() const { return _note; }
    void setNote(int note) { _note = clamp(note, 0, 11); } // 0-11 for chromatic scale

    bool quantizeToScale() const { return _quantizeToScale; }
    void setQuantizeToScale(bool quantize) { _quantizeToScale = quantize; }

    bool gatedCvUpdate() const { return _gatedCvUpdate; }
    void setGatedCvUpdate(bool gated) { _gatedCvUpdate = gated; }

    // Direct access to Teletype state
    // (Engine layer will manage the actual scene_state_t)
    // Model layer stores serializable representation

    void clear();
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    ScriptTrigger _scriptTriggers[10]; // 8 scripts + Metro + Init
    CvInputSource _cvInputSources[2];  // IN, PARAM
    float _cvInputMin[2] = {0.0f, 0.0f};
    float _cvInputMax[2] = {10.0f, 10.0f};

    // Output routing configuration
    CvOutputDestination _cvOutputDestinations[4] = {TrackCv1, TrackCv2, TrackCv3, TrackCv4};
    GateOutputDestination _gateOutputDestinations[4] = {TrackGate1, TrackGate2, TrackGate3, TrackGate4};

    int _metroInterval = 500;  // milliseconds
    bool _metroEnabled = true;
    bool _usePerformerScale = false;

    // Track-specific properties to make Teletype behave like other tracks
    int _octave = 0;           // -4 to 4
    int _transpose = 0;        // -48 to 48 semitones
    int _divisor = 1;          // 1 to 256
    int _note = 0;             // 0 to 11 (chromatic scale)
    bool _quantizeToScale = true;
    bool _gatedCvUpdate = true;

    // Scripts stored as text (serializable)
    // Max 6 lines per script, 64 chars per line
    char _scriptText[11][6][64];  // 11 scripts (8 + Metro + Init + Live)

    // Pattern data (directly serializable)
    int16_t _patternData[4][64];
    uint16_t _patternLen[4] = {16, 16, 16, 16};
    uint16_t _patternStart[4] = {0, 0, 0, 0};
    uint16_t _patternEnd[4] = {15, 15, 15, 15};
    uint16_t _patternWrap[4] = {1, 1, 1, 1};
};
```

### Track.h modification

```cpp
// In Track.h, add new track mode:

enum class TrackMode : uint8_t {
    Note,
    Curve,
    MidiCv,
    Tuesday,
    DiscreteMap,
    Indexed,
    Teletype,     // NEW
    Last,
    Default = Note
};

// In Track class:
TeletypeTrack &teletypeTrack() { return _data.teletypeTrack; }
const TeletypeTrack &teletypeTrack() const { return _data.teletypeTrack; }

private:
    union Data {
        NoteTrack noteTrack;
        CurveTrack curveTrack;
        MidiCvTrack midiCvTrack;
        TuesdayTrack tuesdayTrack;
        DiscreteMapTrack discreteMapTrack;
        IndexedTrack indexedTrack;
        TeletypeTrack teletypeTrack;  // NEW
        // ...
    } _data;
```

## Engine Layer Design

### TeletypeTrackEngine.h

```cpp
// src/apps/sequencer/engine/TeletypeTrackEngine.h
#pragma once

#include "TrackEngine.h"

// Include Teletype core
extern "C" {
#include "teletype/src/teletype.h"
#include "teletype/src/state.h"
}

class TeletypeTrackEngine : public TrackEngine {
public:
    TeletypeTrackEngine(Engine &engine, const Model &model, Track &track,
                       const TrackEngine *linkedTrackEngine);
    ~TeletypeTrackEngine();

    // TrackEngine interface
    Track::TrackMode trackMode() const override { return Track::TrackMode::Teletype; }

    void reset() override;
    void restart() override;
    TickResult tick(uint32_t tick) override;
    void update(float dt) override;
    void stop() override;

    bool activity() const override { return _activity; }
    bool gateOutput(int index) const override;
    float cvOutput(int index) const override;

    // Teletype-specific
    void runScript(uint8_t scriptIndex);
    void runCommand(const char *commandText);

    // Called by hardware adapter
    void setCvOutputInternal(uint8_t channel, float value, bool slew);
    void setGateOutputInternal(uint8_t channel, bool state);
    int16_t getCvInputInternal(uint8_t channel);
    bool getGateInputInternal(uint8_t channel);

    // Access to scene state (for UI)
    scene_state_t &sceneState() { return _sceneState; }
    const scene_state_t &sceneState() const { return _sceneState; }

private:
    void initSceneState();
    void loadScriptsFromModel();
    void saveScriptsToModel();
    void updateCvSlewing(float dt);
    void updateMetro(float dt);
    void checkInputTriggers(uint32_t tick);
    void routeCvOutput(uint8_t channel, float value, bool slew);
    void routeGateOutput(uint8_t channel, bool state);
    void updateTrackParameters(uint32_t tick);
    float convertNoteToVoltage(int note, int octave, int transpose) const;

    // Teletype runtime state
    scene_state_t _sceneState;
    exec_state_t _execState;
    command_state_t _commandState;

    // Performer output state
    float _cvOutputs[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float _cvTargets[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    float _cvSlewRates[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    bool _gateOutputs[4] = {false, false, false, false};

    // Timing
    float _systemTickAccumulator = 0.0f;
    float _metroAccumulator = 0.0f;
    uint32_t _lastUpdateTick = 0;
    uint32_t _tickCounter = 0;

    // Input edge detection
    bool _lastGateInputs[8] = {false};

    bool _activity = false;
};
```

### Hardware Adapter Implementation

```cpp
// src/apps/sequencer/engine/TeletypeHardwareAdapter.h
#pragma once

#include <cstdint>

class TeletypeTrackEngine;

class TeletypeHardwareAdapter {
public:
    static void setEngine(TeletypeTrackEngine *engine) { s_engine = engine; }

    // CV outputs (called by teletype ops)
    static void setCvOutput(uint8_t channel, int16_t value, bool slew);

    // Gate outputs
    static void setTrOutput(uint8_t channel, bool state);
    static void pulseTrOutput(uint8_t channel, uint16_t durationMs);

    // CV inputs
    static int16_t getCvInput(uint8_t channel);

    // Gate inputs
    static bool getTrInput(uint8_t channel);

    // Get ticks for timing
    static uint32_t getTicks();

private:
    static TeletypeTrackEngine *s_engine;
};

// src/apps/sequencer/engine/TeletypeHardwareAdapter.cpp
#include "TeletypeHardwareAdapter.h"
#include "TeletypeTrackEngine.h"

TeletypeTrackEngine* TeletypeHardwareAdapter::s_engine = nullptr;

void TeletypeHardwareAdapter::setCvOutput(uint8_t channel, int16_t value, bool slew) {
    if (!s_engine) return;

    // Teletype: 0-16383 (0-10V)
    // Performer: -1.0 to 1.0
    float normalized = (value / 16383.0f) * 2.0f - 1.0f;
    s_engine->setCvOutputInternal(channel, normalized, slew);
}

void TeletypeHardwareAdapter::setTrOutput(uint8_t channel, bool state) {
    if (!s_engine) return;
    s_engine->setGateOutputInternal(channel, state);
}

void TeletypeHardwareAdapter::pulseTrOutput(uint8_t channel, uint16_t durationMs) {
    if (!s_engine) return;
    s_engine->setGateOutputInternal(channel, true);

    // Schedule pulse off after duration
    // This would require a timer system to turn off the gate after the specified duration
    // Implementation would depend on the Performer's timer system
}

int16_t TeletypeHardwareAdapter::getCvInput(uint8_t channel) {
    if (!s_engine) return 0;
    return s_engine->getCvInputInternal(channel);
}

uint32_t TeletypeHardwareAdapter::getTicks() {
    if (!s_engine) return 0;
    return s_engine->engine().clock().ticks();
}
```

### Modifying Teletype ops/hardware.c

```cpp
// teletype/src/ops/hardware.c
// Replace direct hardware access with adapter calls

#include "TeletypeHardwareAdapter.h"

void tele_cv(uint8_t output, int16_t value, uint8_t slew) {
    TeletypeHardwareAdapter::setCvOutput(output, value, slew != 0);
}

void tele_tr(uint8_t output, int16_t value) {
    TeletypeHardwareAdapter::setTrOutput(output, value != 0);
}

void tele_tr_pulse(uint8_t output, int16_t duration) {
    TeletypeHardwareAdapter::pulseTrOutput(output, duration);
}

int16_t tele_get_input_state(uint8_t input) {
    return TeletypeHardwareAdapter::getTrInput(input) ? 1 : 0;
}

uint32_t tele_get_ticks() {
    return TeletypeHardwareAdapter::getTicks();
}
```

### Output Routing Implementation

The Teletype track can route its outputs to different destinations in the Performer system:

```cpp
// In TeletypeTrackEngine.cpp
void TeletypeTrackEngine::routeCvOutput(uint8_t channel, float value, bool slew) {
    if (channel >= 4) return; // Teletype has 4 CV outputs (0-3)

    auto dest = _track.teletypeTrack().cvOutputDestination(channel);

    switch (dest) {
        case TeletypeTrack::CvOutputDestination::TrackCv1:
        case TeletypeTrack::CvOutputDestination::TrackCv2:
        case TeletypeTrack::CvOutputDestination::TrackCv3:
        case TeletypeTrack::CvOutputDestination::TrackCv4: {
            // Route to local track outputs (default behavior)
            int trackChannel = dest - TeletypeTrack::CvOutputDestination::TrackCv1;
            setCvOutputInternal(trackChannel, value, slew);
            break;
        }
        case TeletypeTrack::CvOutputDestination::GlobalCv1:
        case TeletypeTrack::CvOutputDestination::GlobalCv2:
        case TeletypeTrack::CvOutputDestination::GlobalCv3:
        case TeletypeTrack::CvOutputDestination::GlobalCv4:
        case TeletypeTrack::CvOutputDestination::GlobalCv5:
        case TeletypeTrack::CvOutputDestination::GlobalCv6:
        case TeletypeTrack::CvOutputDestination::GlobalCv7:
        case TeletypeTrack::CvOutputDestination::GlobalCv8: {
            // Route to global CV outputs through routing engine
            int globalChannel = dest - TeletypeTrack::CvOutputDestination::GlobalCv1;
            // This would interface with the Performer's routing system
            _engine.routingEngine().setCvOutput(globalChannel, value);
            break;
        }
        case TeletypeTrack::CvOutputDestination::MappedToRouting: {
            // Route through Performer's routing system with custom mapping
            // Implementation would depend on the routing system architecture
            break;
        }
        case TeletypeTrack::CvOutputDestination::Disabled:
            // Output is disabled, do nothing
            break;
    }
}

void TeletypeTrackEngine::routeGateOutput(uint8_t channel, bool state) {
    if (channel >= 4) return; // Teletype has 4 gate outputs (0-3)

    auto dest = _track.teletypeTrack().gateOutputDestination(channel);

    switch (dest) {
        case TeletypeTrack::GateOutputDestination::TrackGate1:
        case TeletypeTrack::GateOutputDestination::TrackGate2:
        case TeletypeTrack::GateOutputDestination::TrackGate3:
        case TeletypeTrack::GateOutputDestination::TrackGate4: {
            // Route to local track gates (default behavior)
            int trackChannel = dest - TeletypeTrack::GateOutputDestination::TrackGate1;
            setGateOutputInternal(trackChannel, state);
            break;
        }
        case TeletypeTrack::GateOutputDestination::GlobalGate1:
        case TeletypeTrack::GateOutputDestination::GlobalGate2:
        case TeletypeTrack::GateOutputDestination::GlobalGate3:
        case TeletypeTrack::GateOutputDestination::GlobalGate4:
        case TeletypeTrack::GateOutputDestination::GlobalGate5:
        case TeletypeTrack::GateOutputDestination::GlobalGate6:
        case TeletypeTrack::GateOutputDestination::GlobalGate7:
        case TeletypeTrack::GateOutputDestination::GlobalGate8: {
            // Route to global gate outputs through routing engine
            int globalChannel = dest - TeletypeTrack::GateOutputDestination::GlobalGate1;
            // This would interface with the Performer's routing system
            _engine.routingEngine().setGateOutput(globalChannel, state);
            break;
        }
        case TeletypeTrack::GateOutputDestination::MappedToRouting: {
            // Route through Performer's routing system with custom mapping
            // Implementation would depend on the routing system architecture
            break;
        }
        case TeletypeTrack::GateOutputDestination::Disabled:
            // Output is disabled, do nothing
            break;
    }
}

// Update the adapter to use routing
void TeletypeHardwareAdapter::setCvOutput(uint8_t channel, int16_t value, bool slew) {
    if (!s_engine) return;

    // If using gated CV update, only update CV when gate is active
    if (s_engine->track().teletypeTrack().gatedCvUpdate()) {
        // In gated mode, CV updates are tied to gate activity
        // The actual CV value would be computed based on track parameters
        float noteVoltage = s_engine->convertNoteToVoltage(
            s_engine->track().teletypeTrack().note(),
            s_engine->track().teletypeTrack().octave(),
            s_engine->track().teletypeTrack().transpose()
        );
        s_engine->routeCvOutput(channel, noteVoltage, slew);
    } else {
        // Teletype: 0-16383 (0-10V)
        // Performer: -1.0 to 1.0
        float normalized = (value / 16383.0f) * 2.0f - 1.0f;
        s_engine->routeCvOutput(channel, normalized, slew);
    }
}

void TeletypeHardwareAdapter::setTrOutput(uint8_t channel, bool state) {
    if (!s_engine) return;
    s_engine->routeGateOutput(channel, state);

    // If using gated CV update, update CV when gate fires
    if (s_engine->track().teletypeTrack().gatedCvUpdate() && state) {
        float noteVoltage = s_engine->convertNoteToVoltage(
            s_engine->track().teletypeTrack().note(),
            s_engine->track().teletypeTrack().octave(),
            s_engine->track().teletypeTrack().transpose()
        );
        // Update CV output when gate fires in gated mode
        s_engine->routeCvOutput(channel, noteVoltage, false); // No slew by default in gated mode
    }
}

// Additional implementation methods for track-like behavior
TickResult TeletypeTrackEngine::tick(uint32_t tick) {
    _activity = false;

    // Update track parameters based on divisor
    updateTrackParameters(tick);

    // Check input triggers and run associated scripts
    checkInputTriggers(tick);

    // Only return update flags if gated CV update is disabled
    // If gated CV update is enabled, we only update when gates fire
    if (!_track.teletypeTrack().gatedCvUpdate()) {
        return _activity ? CvUpdate | GateUpdate : NoUpdate;
    } else {
        // When gated CV update is enabled, only return update when gates change
        return _activity ? GateUpdate : NoUpdate;
    }
}

void TeletypeTrackEngine::updateTrackParameters(uint32_t tick) {
    // Only process on divisor intervals to sync with clock
    if ((tick - _lastUpdateTick) >= static_cast<uint32_t>(_track.teletypeTrack().divisor())) {
        _lastUpdateTick = tick;
        _tickCounter++;

        // If using project scales for notes, apply quantization
        if (_track.teletypeTrack().quantizeToScale()) {
            // Apply scale quantization using project scale
            // This would interface with the Performer's scale system
            // Implementation would depend on how the scale system works in Performer
        }

        // Update metro based on divisor instead of fixed time
        if (_track.teletypeTrack().metroEnabled()) {
            // Run metro script based on divisor intervals
            runScript(METRO_SCRIPT);
        }

        _activity = true;
    }
}

float TeletypeTrackEngine::convertNoteToVoltage(int note, int octave, int transpose) const {
    // Convert note (0-11) + octave (-4 to 4) + transpose (-48 to 48 semitones) to voltage
    // Middle C (C4) = 0V, each semitone = 1/12V

    int totalSemitones = (octave * 12) + note + transpose;
    return static_cast<float>(totalSemitones) / 12.0f;
}
```

## UI Layer Design

### TeletypeTrackPage.h

```
// src/apps/sequencer/ui/pages/TeletypeTrackPage.h
class TeletypeTrackPage : public BasePage {
public:
    enum Mode {
        ScriptEditor,
        VariableMonitor,
        PatternTracker,
        Configuration,
    };

    void enter() override;
    void exit() override;
    void draw(Canvas &canvas) override;
    void updateLeds(Leds &leds) override;

    void keyDown(KeyEvent &event) override;
    void keyUp(KeyEvent &event) override;
    void keyPress(KeyPressEvent &event) override;
    void encoder(EncoderEvent &event) override;
    void midi(MidiEvent &event) override;

private:
    void drawScriptEditor(Canvas &canvas);
    void drawVariableMonitor(Canvas &canvas);
    void drawPatternTracker(Canvas &canvas);
    void drawConfiguration(Canvas &canvas);

    Mode _mode = ScriptEditor;

    // Script editor state
    uint8_t _selectedScript = 0;
    uint8_t _selectedLine = 0;
    bool _editingLine = false;
    char _editBuffer[64];
    uint8_t _cursorPos = 0;

    // Pattern tracker state
    uint8_t _selectedPattern = 0;
    uint8_t _selectedIndex = 0;
};
```

### UI Interaction Patterns

**Page 1: Script Editor**
- Step buttons 1-8: Select script 1-8 (with Shift: Metro, Init)
- Encoder Up/Down: Navigate lines within script
- Function + Encoder: Select context action (Edit, Vars, Pattern, Config)
- Context menu: Op selection via category-based navigation

**Page 2: Variable Monitor**
- Shows live dashboard of variables, CV values, TR states
- Encoder: Enter live command for immediate execution

**Page 3: Pattern Tracker**
- Visualizes pattern data with encoder navigation
- Shift+Encoder: Edit values directly
- Function: Set pattern parameters (length, wrap, etc.)

**Page 4: Configuration**
- Input routing configuration
- CV input mapping
- Metro settings
- Scale integration options
- Output routing configuration (new)

**Page 5: Output Routing** (New page)
- Configure where Teletype CV outputs (1-4) go
- Configure where Teletype gate outputs (1-4) go
- Options: Track outputs, Global outputs, Routing system, Disabled

**Page 6: Track Parameters** (New page)
- Octave (-4 to 4) - for note generation
- Transpose (-48 to 48 semitones)
- Divisor (1 to 256) - for clock division and metro sync
- Note (0-11) - chromatic scale selection
- Quantize to Scale (on/off) - use project scales for note quantization
- Gated CV Update (on/off) - update CV only when gate fires

## Build System Integration

### CMakeLists.txt modifications

```cmake
# src/apps/sequencer/model/CMakeLists.txt
set(SOURCES
    # ... existing sources
    TeletypeTrack.cpp
)

# src/apps/sequencer/engine/CMakeLists.txt
set(SOURCES
    # ... existing sources
    TeletypeTrackEngine.cpp
    TeletypeHardwareAdapter.cpp
)

# Add Teletype core library
add_subdirectory(teletype)

# Link teletype core
target_link_libraries(sequencer-engine
    # ... existing libs
    teletype-core
)

# teletype/CMakeLists.txt (new file)
add_library(teletype-core STATIC
    src/teletype.c
    src/command.c
    src/scanner.c
    src/match_token.c
    src/state.c
    src/ops/op.c
    src/ops/variables.c
    src/ops/patterns.c
    src/ops/maths.c
    src/ops/stack.c
    src/ops/queue.c
    src/ops/controlflow.c
    src/ops/delay.c
    src/ops/metronome.c
    src/ops/hardware.c
    src/ops/midi.c
    # Omit i2c-dependent ops
)

target_include_directories(teletype-core PUBLIC src/)
target_compile_options(teletype-core PRIVATE -Wno-unused-function)
```

## Serialization

### TeletypeTrack serialization

```cpp
void TeletypeTrack::write(VersionedSerializedWriter &writer) const {
    // Write script triggers
    for (int i = 0; i < 10; ++i) {  // 8 scripts + Metro + Init
        _scriptTriggers[i].write(writer);
    }

    // Write CV input sources and scaling
    for (int i = 0; i < 2; ++i) {
        writer.write(static_cast<uint8_t>(_cvInputSources[i]));
        writer.write(_cvInputMin[i]);
        writer.write(_cvInputMax[i]);
    }

    // Write output routing configuration
    for (int i = 0; i < 4; ++i) {
        writer.write(static_cast<uint8_t>(_cvOutputDestinations[i]));
        writer.write(static_cast<uint8_t>(_gateOutputDestinations[i]));
    }

    // Write metro config
    writer.write(_metroInterval);
    writer.write(_metroEnabled);
    writer.write(_usePerformerScale);

    // Write track-specific properties to make Teletype behave like other tracks
    writer.write(_octave);
    writer.write(_transpose);
    writer.write(_divisor);
    writer.write(_note);
    writer.write(_quantizeToScale);
    writer.write(_gatedCvUpdate);

    // Write scripts as text
    for (int s = 0; s < 11; ++s) {  // 8 + Metro + Init + Live
        for (int l = 0; l < 6; ++l) {
            writer.write(_scriptText[s][l], 64);
        }
    }

    // Write patterns
    for (int p = 0; p < 4; ++p) {
        writer.write(_patternData[p], 64);
        writer.write(_patternLen[p]);
        writer.write(_patternStart[p]);
        writer.write(_patternEnd[p]);
        writer.write(_patternWrap[p]);
    }
}

void TeletypeTrack::read(VersionedSerializedReader &reader) {
    // Mirror of write() with appropriate error checking
    // ...
}
```

## Testing Strategy

### Unit Tests

```cpp
// src/tests/unit/sequencer/TestTeletypeTrack.cpp
#include "UnitTest.h"
#include "apps/sequencer/model/TeletypeTrack.h"

UNIT_TEST("TeletypeTrack") {

CASE("default_values") {
    TeletypeTrack tt;
    expectEqual(tt.metroInterval(), 500, "default metro 500ms");
    expectTrue(tt.metroEnabled(), "metro enabled by default");
    expectFalse(tt.usePerformerScale(), "performer scale integration off by default");
}

CASE("script_trigger_mapping") {
    TeletypeTrack tt;
    TeletypeTrack::ScriptTrigger trig;
    trig.source = TeletypeTrack::PerformerClock;
    trig.edgePolarity = 0;
    trig.threshold = 0.7f;

    tt.setScriptTrigger(0, trig);
    expectEqual(static_cast<int>(tt.scriptTrigger(0).source),
                static_cast<int>(TeletypeTrack::PerformerClock),
                "trigger source set");
    expectEqual(tt.scriptTrigger(0).threshold, 0.7f, "trigger threshold set");
}

} // UNIT_TEST
```

### Integration Tests

Test Teletype scripts and verify CV/Gate outputs:

```cpp
// src/tests/integration/sequencer/TestTeletypeEngine.cpp
CASE("cv_output") {
    // Script: CV 1 8192  (5V in Teletype's 0-16383 range)
    TeletypeTrackEngine engine(...);
    engine.runCommand("CV 1 8192");

    float cv = engine.cvOutput(0);
    // 8192/16383 * 2 - 1 ≈ 0.0 (approximately 0V in normalized range)
    expectTrue(std::abs(cv - 0.0f) < 0.01f, "CV output correct");
}

CASE("gate_output") {
    // Script: TR 1 1
    TeletypeTrackEngine engine(...);
    engine.runCommand("TR 1 1");

    bool gate = engine.gateOutput(0);
    expectTrue(gate, "Gate output set to true");
}

CASE("output_routing") {
    // Test that output routing works correctly
    TeletypeTrackEngine engine(...);

    // Set CV output 1 to route to Global CV 1
    engine.track().teletypeTrack().setCvOutputDestination(0, TeletypeTrack::CvOutputDestination::GlobalCv1);

    // Run a script that sets CV 1
    engine.runCommand("CV 1 16383"); // Max voltage

    // Verify that the routing was applied (would check routing engine state)
    // This would depend on how the routing engine interface is implemented
}

CASE("init_script_execution") {
    // Test that init script runs properly
    TeletypeTrackEngine engine(...);

    // Set up an init script that initializes CV outputs
    strcpy(engine.track().teletypeTrack()._scriptText[INIT_SCRIPT][0], "CV 1 8192");
    strcpy(engine.track().teletypeTrack()._scriptText[INIT_SCRIPT][1], "CV 2 4096");

    // Run the init script
    engine.runScript(INIT_SCRIPT);

    // Verify outputs were set correctly
    float cv1 = engine.cvOutput(0);
    float cv2 = engine.cvOutput(1);

    expectTrue(std::abs(cv1 - 0.0f) < 0.01f, "CV 1 set to ~0V");
    expectTrue(std::abs(cv2 - (-0.5f)) < 0.01f, "CV 2 set to ~-0.5V");
}

CASE("metro_script_execution") {
    // Test that metro script runs at intervals
    TeletypeTrackEngine engine(...);

    // Set up a metro script that toggles a gate
    strcpy(engine.track().teletypeTrack()._scriptText[METRO_SCRIPT][0], "TR.TOG 1");

    // Set metro interval to 100ms and enable
    engine.track().teletypeTrack().setMetroInterval(100);
    engine.track().teletypeTrack().setMetroEnabled(true);

    // Simulate time passing to trigger metro
    engine.update(0.1f); // 100ms

    // Verify gate toggled
    bool gate = engine.gateOutput(0);
    expectTrue(gate, "Gate toggled by metro script");
}

CASE("track_parameters") {
    // Test track parameter functionality
    TeletypeTrackEngine engine(...);

    // Set track parameters
    engine.track().teletypeTrack().setOctave(1);
    engine.track().teletypeTrack().setTranspose(12); // One octave up
    engine.track().teletypeTrack().setNote(0); // C note
    engine.track().teletypeTrack().setDivisor(4);
    engine.track().teletypeTrack().setGatedCvUpdate(true);

    // Test note to voltage conversion
    float voltage = engine.convertNoteToVoltage(
        engine.track().teletypeTrack().note(),
        engine.track().teletypeTrack().octave(),
        engine.track().teletypeTrack().transpose()
    );

    // C note (0) + octave 1 (12 semitones) + transpose 12 = 24 semitones = 2.0V
    expectTrue(std::abs(voltage - 2.0f) < 0.01f, "Note to voltage conversion correct");
}

CASE("gated_cv_update") {
    // Test gated CV update functionality
    TeletypeTrackEngine engine(...);

    // Enable gated CV update
    engine.track().teletypeTrack().setGatedCvUpdate(true);
    engine.track().teletypeTrack().setOctave(0);
    engine.track().teletypeTrack().setNote(0); // C note
    engine.track().teletypeTrack().setTranspose(0);

    // Initially, CV should be based on track parameters
    float initialCv = engine.cvOutput(0);

    // Trigger a gate, which should update the CV in gated mode
    engine.routeGateOutput(0, true);

    // Check that CV was updated when gate fired
    float updatedCv = engine.cvOutput(0);

    // In gated mode, CV should update to the note voltage when gate fires
    float expectedVoltage = engine.convertNoteToVoltage(0, 0, 0); // C4 = 0V
    expectTrue(std::abs(updatedCv - expectedVoltage) < 0.01f, "CV updated when gate fired in gated mode");
}

CASE("divisor_sync") {
    // Test that track updates on divisor intervals
    TeletypeTrackEngine engine(...);

    // Set divisor to 4
    engine.track().teletypeTrack().setDivisor(4);

    uint32_t initialTickCount = engine._tickCounter;

    // Call tick multiple times but only update every 4th tick
    for (int i = 0; i < 10; i++) {
        engine.tick(i);
    }

    // Verify that updates happened based on divisor
    uint32_t finalTickCount = engine._tickCounter;

    // Should have updated approximately 10/4 = 2.5 -> 2 or 3 times
    expectTrue(finalTickCount > initialTickCount, "Track parameters updated based on divisor");
}
```

## Performance Considerations

### Memory Footprint
- **Teletype core state**: ~10-15 KB per track (scene_state_t)
- **Mitigation**: Only allocate for tracks using Teletype mode; shared op table

### CPU Usage
- **Script execution**: ~100-500 μs per script (depends on complexity)
- **Safe for Engine::update()**: 4096 stack, priority 4
- **Profiling target**: Ensure `TeletypeTrackEngine::update()` < 1ms on STM32

### Code Size
- **Teletype ops library**: ~50-80 KB of code
- **Mitigation**: Compile with `-Os` for size optimization

## Implementation Phases

### Phase 1: Core Integration (Weeks 1-2)
**Goal**: Get interpreter running in Performer context

1. Port Teletype core files to `src/apps/sequencer/teletype/`
2. Create `TeletypeTrack` model (minimal, no UI)
3. Create `TeletypeTrackEngine` with stub I/O
4. Implement `TeletypeHardwareAdapter`
5. Register in `TrackEngine` factory
6. Write unit tests for model
7. **Milestone**: Can create Teletype track, run hardcoded script

### Phase 2: I/O Integration (Weeks 3-4)
**Goal**: Get CV/Gate outputs working

1. Implement CV output with slewing in `TeletypeTrackEngine::update()`
2. Implement Gate output
3. Wire outputs to Performer's routing system
4. Test with simple scripts: `CV 1 N 60`, `TR.PULSE 1`
5. **Milestone**: Teletype track produces CV/Gate on outputs

### Phase 3: Input Routing (Weeks 5-6)
**Goal**: Trigger scripts from Performer events

1. Implement `checkInputTriggers()` in engine
2. Add input mapping config to `TeletypeTrack` model
3. Test script triggering from clock, gates
4. Implement CV inputs (IN, PARAM) with source mapping
5. **Milestone**: Scripts run on clock/gate events, read CV inputs

### Phase 4: Basic UI (Weeks 7-8)
**Goal**: Minimal script editing

1. Create `TeletypeTrackPage` with script viewer
2. Implement encoder-based line navigation
3. Implement simple character entry (encoder scroll alphabet)
4. Parse and execute on save
5. **Milestone**: Can edit and run scripts from UI

### Phase 5: Advanced UI (Weeks 9-11)
**Goal**: Full editing experience

1. Implement op menu system (categories)
2. Add pattern tracker page
3. Add variable monitor page
4. Add configuration page (input routing)
5. Implement T9/preset button shortcuts
6. **Milestone**: Full-featured script editor

### Phase 6: Polish & Optimization (Weeks 12-13)
**Goal**: Production ready

1. Performance profiling on STM32
2. Optimize hot paths (script execution, slewing)
3. Add help text / op reference
4. Write integration tests
5. Documentation
6. **Milestone**: Ready for release

## Known Limitations

1. **No I2C modules**: All Monome ecosystem ops unavailable
2. **No Grid**: Grid control ops unavailable
3. **No Live Mode REPL**: Replaced with slower encoder-based entry
4. **Limited screen space**: Teletype uses 128x64, Performer's is 256x64 but shared with track info
5. **No keyboard shortcuts**: All editing via encoder/buttons
6. **Timing precision**: 10ms tick vs native timing (should be fine for most use cases)

## Conclusion

This implementation plan combines the architectural clarity of codex-tele.md with the detailed implementation approach of claude-tele.md. The resulting plan provides:

- Clear separation of concerns with well-defined interfaces
- Comprehensive UI design with multiple interaction patterns
- Detailed hardware adapter implementation
- Proper serialization and model layer design
- Comprehensive testing strategy
- Realistic implementation timeline
- Enhanced output routing capabilities allowing Teletype scripts to access different Performer outputs
- Support for Init and Metro scripts with proper initialization and periodic execution
- Track-like parameters (octave, transpose, divisor, note selection) for better integration with other track types
- Scale quantization using project scales for note generation
- Gated vs free CV update modes for different performance styles
- Clock divisor synchronization for metro and track updates

The approach is architecturally sound and feasible, leveraging Performer's existing infrastructure while providing the rich functionality that makes Teletype a powerful tool for algorithmic composition. The output routing system allows Teletype scripts to control not just the local track's outputs but also global CV and gate outputs through the Performer's routing system, significantly expanding the creative possibilities. The added track-like parameters make the Teletype track behave more consistently with other track types in the Performer, providing a more unified user experience.

## Routable Elements in Teletype Track

The Teletype track provides several routable elements that can be directed to different destinations within the Performer system:

### 1. CV Outputs (4 channels)
- **Teletype CV 1-4**: The 4 main CV outputs from Teletype scripts
- **Routing destinations**: Track CV outputs, Global CV outputs (1-8), Routing system, or Disabled
- **Usage in scripts**: `CV x y` where x is 1-4 and y is voltage (0-16383)

### 2. Gate/Trigger Outputs (4 channels)
- **Teletype TR 1-4**: The 4 main gate/trigger outputs from Teletype scripts
- **Routing destinations**: Track gate outputs, Global gate outputs (1-8), Routing system, or Disabled
- **Usage in scripts**: `TR x y` where x is 1-4 and y is state (0/1), or `TR.PULSE x`

### 3. CV Inputs (2 logical inputs)
- **IN and PARAM**: Teletype's main analog inputs
- **Routing sources**: CV inputs 1-4, Step encoder, Project tempo, Track progress, External MIDI CC
- **Usage in scripts**: `IN` and `PARAM` return current values

### 4. Gate Inputs (8 logical inputs)
- **TRIGGER 1-8**: Teletype's trigger inputs that can run scripts
- **Routing sources**: Performer clock, Gate inputs 1-4, Step button, Function button, Track mute toggle, Pattern change, External MIDI
- **Usage in scripts**: When triggered, runs Scripts 1-8 respectively

### 5. Internal Variables and States
- **Pattern data**: Can be routed to other tracks or used for modulation
- **Variables (A-Z, X, Y, Z, etc.)**: Can be accessed by other tracks or modules
- **Slew rates**: CV slew parameters can be modulated by other sources

## Example Teletype Scripts for Performer

Here are sets of Teletype scripts with increasing complexity to test and demonstrate the functionality in the Performer environment:

### Set 1: Basic Sequencing
**Script 1 (TR 1):** `CV 1 N 60 : TR 1 1`
**Script 2 (TR 2):** `CV 1 N 64 : TR 1 1`
**Script 3 (TR 3):** `CV 1 N 67 : TR 1 1`
**Script 4 (TR 4):** `CV 1 N 72 : TR 1 1`
**Script 5 (TR 5):** `CV 2 8192 : TR 2 1`
**Script 6 (TR 6):** `CV 2 4096 : TR 2 1`
**Script 7 (TR 7):** `CV 3 12288 : TR 3 1`
**Script 8 (TR 8):** `CV 3 0 : TR 3 1`

**Metro Script:** `CV.SLEW 1 50`
**Init Script:** `CV 1 0 : CV 2 0 : CV 3 0 : M 500`

### Set 2: Probability and Variation
**Script 1 (TR 1):** `P 1 50 : IF P 1 : CV 1 N 60 : TR 1 1`
**Script 2 (TR 2):** `P 2 75 : IF P 2 : CV 1 N 64 : TR 1 1`
**Script 3 (TR 3):** `RND 12 : CV 1 N 60 ADD RND : TR 1 1`
**Script 4 (TR 4):** `CV 2 8192 : TR.PULSE 2`
**Script 5 (TR 5):** `CV.SLEW 1 100 : CV 1 16383`
**Script 6 (TR 6):** `CV.SLEW 1 10 : CV 1 0`
**Script 7 (TR 7):** `X 0 : X ADD 1 : IF GTE X 8 : X 0`
**Script 8 (TR 8):** `CV 3 N 60 ADD MUL X 2`

**Metro Script:** `MUL R 100 200 : CV 4 R : TR.PULSE 4`
**Init Script:** `X 0 : Y 0 : P 1 50 : P 2 75 : M 750`

### Set 3: Advanced Sequencing with Patterns
**Script 1 (TR 1):** `I 0 : I ADD 1 : IF GTE I 16 : I 0 : CV 1 P 0 I`
**Script 2 (TR 2):** `J 0 : J ADD 1 : IF GTE J 16 : J 0 : CV 2 P 1 J`
**Script 3 (TR 3):** `CV 3 P 2 COUNTER 8`
**Script 4 (TR 4):** `TR.PULSE 1 : COUNTER 1 ADD 1`
**Script 5 (TR 5):** `IF EQ COUNTER 16 : COUNTER 0`
**Script 6 (TR 6):** `CV.SLEW 1 200 : CV.SLEW 2 50`
**Script 7 (TR 7):** `P 0 0 60 : P 0 1 64 : P 0 2 67 : P 0 3 72`
**Script 8 (TR 8):** `P 0 4 76 : P 0 5 79 : P 0 6 84 : P 0 7 60`

**Metro Script:** `P 3 COUNTER 4 100 : CV 4 P 3 COUNTER 4`
**Init Script:** `COUNTER 0 : COUNTER 1 0 : P 2 0 0 : P 2 1 4096 : P 2 2 8192 : P 2 3 12288 : M 250`

### Set 4: Complex Algorithmic Patterns
**Script 1 (TR 1):** `W 1 : N 60 : X 0 : X ADD 1 : IF GTE X 12 : X 0 : N ADD MUL X 7 : N MOD 12 : N ADD 60 : CV 1 N : TR 1 1`
**Script 2 (TR 2):** `W 2 : DRUNK 5 : IF LT DRUNK 0 : DRUNK MUL DRUNK -1 : CV 2 MUL DRUNK 2048 : TR.PULSE 2`
**Script 3 (TR 3):** `EVERY 3 : CV.SLEW 1 500 : CV.SLEW 2 50`
**Script 4 (TR 4):** `EVERY 7 : CV.SLEW 1 50 : CV.SLEW 2 500`
**Script 5 (TR 5):** `L 0 7 : CV 3 P 0 I : TR 3 1`
**Script 6 (TR 6):** `IF EQ COUNTER 32 : COUNTER 0 : P 0 0 R 127 : P 0 1 R 127 : P 0 2 R 127 : P 0 3 R 127`
**Script 7 (TR 7):** `IF EQ COUNTER 16 : COUNTER 0 : CV 4 R 16383`
**Script 8 (TR 8):** `COUNTER ADD 1 : IF GTE COUNTER 64 : COUNTER 0`

**Metro Script:** `X ADD 1 : IF GTE X 8 : X 0 : CV 4 P 1 X : IF EQ X 0 : TR.PULSE 4`
**Init Script:** `X 0 : Y 0 : COUNTER 0 : DRUNK 64 : P 1 0 0 : P 1 1 2048 : P 1 2 4096 : P 1 3 6144 : P 1 4 8192 : P 1 5 10240 : P 1 6 12288 : P 1 7 14336 : M 1000`

### Set 5: Advanced Control and Modulation
**Script 1 (TR 1):** `CV 1 N 60 : CV.SLEW 1 R 500 : TR 1 1 : IF EQ STEP 0 : STEP 1 : IF EQ STEP 1 : CV 1 N 64 : STEP 2 : IF EQ STEP 2 : CV 1 N 67 : STEP 3 : IF EQ STEP 3 : CV 1 N 72 : STEP 0`
**Script 2 (TR 2):** `PARAM : CV 2 MUL PARAM 16 : IF GT PARAM 8000 : TR.PULSE 2`
**Script 3 (TR 3):** `IN : CV 3 MUL IN 16 : IF LT IN 4000 : CV.SLEW 3 1000 : IF GT IN 12000 : CV.SLEW 3 10`
**Script 4 (TR 4):** `L 0 3 : CV 4 P 0 I : TR.PULSE 4 : DEL 250 : TR 4 0`
**Script 5 (TR 5):** `X ADD 1 : IF GTE X 16 : X 0 : CV 1 P 1 X : CV 2 P 2 X`
**Script 6 (TR 6):** `Y ADD 1 : IF GTE Y 8 : Y 0 : CV 3 P 3 Y : CV 4 P 4 Y`
**Script 7 (TR 7):** `IF EQ COUNTER 0 : COUNTER 1 : P 0 0 R 16383 : P 0 1 R 16383 : P 0 2 R 16383 : P 0 3 R 16383`
**Script 8 (TR 8):** `IF EQ COUNTER 0 : COUNTER 1 : CV.SLEW 1 R 1000 : CV.SLEW 2 R 1000 : CV.SLEW 3 R 1000 : CV.SLEW 4 R 1000`

**Metro Script:** `M ADD 10 : IF GTE M 2000 : M 25 : CV 4 MUL COUNTER 65 : COUNTER ADD 1 : IF GTE COUNTER 256 : COUNTER 0`
**Init Script:** `STEP 0 : COUNTER 0 : X 0 : Y 0 : M 500 : P 1 0 0 : P 1 1 2048 : P 1 2 4096 : P 1 3 6144 : P 1 4 8192 : P 1 5 10240 : P 1 6 12288 : P 1 7 14336 : P 2 0 16383 : P 2 1 14336 : P 2 2 12288 : P 2 3 10240 : P 2 4 8192 : P 2 5 6144 : P 2 6 4096 : P 2 7 2048 : P 3 0 0 : P 3 1 4096 : P 3 2 8192 : P 3 3 12288 : P 4 0 16383 : P 4 1 12288 : P 4 2 8192 : P 4 3 4096`

These example scripts demonstrate various levels of complexity and functionality that can be achieved with the Teletype track in the Performer environment, from basic sequencing to complex algorithmic patterns and modulation.

## Independent Script File Management

To allow Teletype scripts to be saved and loaded independently of a project, the following implementation is proposed:

### File Format
- **File Extension**: `.tts` (Teletype Script)
- **Format**: Text-based with metadata header
- **Structure**:
  ```
  #TELETYPE-SCRIPT-v1.0
  #NAME: My Script Set
  #AUTHOR: User
  #DATE: YYYY-MM-DD
  #DESCRIPTION: Brief description of the script set

  #SCRIPT:1
  CV 1 N 60 : TR 1 1
  CV.SLEW 1 50

  #SCRIPT:2
  CV 1 N 64 : TR 1 1

  #METRO
  M 500

  #INIT
  CV 1 0 : CV 2 0
  ```

### File Operations
- **Save Script Set**: Export all 11 scripts (0-8 + Metro + Init + Live) to a single .tts file
- **Load Script Set**: Import and replace all scripts in the current Teletype track
- **Merge Script Set**: Import specific scripts without replacing existing ones
- **Export Single Script**: Save individual scripts as .tts files

## UI Considerations for 256x64 OLED Display

Given the 256x64 OLED display and 6 lines per script, the UI needs to be optimized for readability and navigation:

### Script Editor Page
- **Display Area**: 4 lines visible at a time (256x64 divided into 4 rows of 16 pixels each)
- **Navigation**: Encoder scrolls through script lines, with current line highlighted
- **Line Indicators**: Show line numbers (1-6) on the left side
- **Script Status**: Show current script number and editing mode at top
- **Context Menu**: Access via function button for common operations

### Visual Layout
```
[1] CV 1 N 60 : TR 1 1        [F1:INS] [F2:DEL] [F3:MENU]
[2] CV.SLEW 1 50              CURSOR: Line 2, Col 12
[3] >IF P 1 : CV 1 N 64       [UP/DOWN:NAV] [LEFT/RIGHT:EDIT]
[4]  TR 1 1                   [PAGE:EXIT] [SHIFT:SEL]
```

### Multi-Script View
- **Thumbnail View**: Show first line of each script in a compact format
- **Navigation**: Use encoder to select script, press to edit
- **Status Indicators**: Show which scripts have content vs are empty

### Character Entry
- **T9-style Input**: Use step buttons 1-8 for character selection
- **Alphabet Scrolling**: Encoder scrolls through character sets
- **Quick Commands**: Function buttons for common Teletype commands
- **Context-Sensitive Menu**: For Teletype-specific operations

### File Browser UI
- **Directory Navigation**: Show .tts files in current directory
- **File Preview**: Show first few lines of script set
- **File Operations**: Load, Save, Delete, Rename operations
- **Path Display**: Show current directory path at top

## Resource Cost Evaluation

### Memory Requirements
- **Script Storage**:
  - 11 scripts × 6 lines × 64 characters = 4,224 bytes for text storage
  - Additional memory for parsed command structures: ~2KB
  - Total: ~6.2KB RAM per Teletype track

- **File System Cache**:
  - Buffer for file operations: 2KB
  - Directory listing cache: 1KB
  - Total: ~3KB additional RAM when browsing files

### Storage Requirements
- **Script File Size**:
  - Average .tts file: ~1-2KB
  - Metadata overhead: ~200 bytes per file
  - Efficient compression possible for repetitive scripts

### CPU Usage
- **Parsing Overhead**:
  - Initial parsing when loading: ~50ms
  - Runtime command execution: Minimal (same as embedded scripts)
  - File I/O operations: ~100-200ms for read/write

- **Display Updates**:
  - OLED refresh: Minimal impact (handled by dedicated driver)
  - Text rendering: ~1-2ms per frame update during editing

### Performance Impact
- **Engine Thread**: No significant impact during normal operation
- **UI Thread**: Slight increase during script editing (acceptable)
- **File Operations**: Performed on File Task thread, no impact on real-time performance

### Flash Memory
- **Additional Code**:
  - File I/O functions: ~5KB
  - Script management UI: ~8KB
  - File format parsing: ~3KB
  - Total: ~16KB additional flash usage

The resource costs are reasonable for the STM32F4 platform, with memory usage well within available limits and performance impact minimal during normal operation.