# Teletype Track Integration for Performer: Architecture & Implementation
## Hardware constraints


Hardware Constraints (STM32F405RGTx)

  Available Resources:
  - Flash: 1024 KB
  - RAM: 192 KB total (128 KB main + 64 KB CCM)
  - CPU: 168 MHz ARM Cortex-M4

  Current Task Stack Usage:
  Driver Task:   1024 bytes  (priority 5)
  Engine Task:   4096 bytes  (priority 4) ← Teletype runs here
  USB Host:      2048 bytes  (priority 3)
  UI Task:       4096 bytes  (priority 2)
  File Task:     2048 bytes  (priority 1)
  Profiler:      2048 bytes  (priority 0)
  ---
  Total stacks:  15360 bytes (15 KB)

  Teletype Resource Requirements (Per Track)

  RAM Breakdown:

  Core scene_state_t (from teletype/src/state.h):
  - Variables: ~500 bytes
  - Patterns (4×64): ~520 bytes
  - Delay queue (64 slots): ~3-4 KB
  - Stack operations: ~800 bytes
  - Scripts (11×6 lines): ~3-4 KB
  - Turtle state: ~100 bytes
  - Grid structures: ~8-10 KB ⚠️ MUST BE REMOVED
  - MIDI state: ~200 bytes
  - Random states: ~100 bytes

  Without Grid: ~5-7 KB per track ✅
  With Grid: ~13-15 KB per track ❌

  Additional Overhead:
  - TeletypeTrack model: ~1-2 KB (Performer config, routing tables)
  - File I/O buffer (temporary): 512 bytes
  - Scene list cache (UI only): ~2 KB

  Total per active Teletype track: ~6-9 KB

  Flash (Code Size):

  - Teletype ops library: ~50-80 KB
  - Parser/interpreter: ~10-15 KB
  - UI pages (script editor, file browser): ~15-20 KB
  - Total: ~75-115 KB

  CPU Performance:

  From claude-tele.md:
  - Script execution: 100-500 μs per script
  - Target: Engine::update() < 1ms
  - File I/O: 50-100ms (async on FileTask)

  Feasibility Analysis

  ✅ FEASIBLE with Constraints:

  1. RAM Budget (Tight but Workable):
  Available RAM:        192 KB
  Current stacks:        -15 KB
  Current heap/static:   ~? KB (unknown current usage)
  ---
  Teletype (1 track):    -9 KB
  Teletype (2 tracks):   -18 KB
  File system overhead:  -6 KB (scene cache + buffers)

  Verdict:
  - 1 Teletype track: Low risk, plenty of headroom
  - 2 Teletype tracks: Moderate risk, depends on current heap usage
  - 3+ Teletype tracks: High risk, likely insufficient RAM

  CRITICAL: Must remove grid structures from scene_state_t (saves 8-10 KB per track)

  2. Flash Budget (Comfortable):
  Available:   1024 KB
  Teletype:    ~75-115 KB
  Remaining:   ~910-950 KB

  Verdict: Low risk. Teletype code ~7-11% of flash.

  3. CPU Budget (Acceptable):

  From Config.h, Engine task has 4096-byte stack and priority 4. Teletype script execution (100-500 μs) is well within the 1ms update target. File I/O runs async on FileTask (priority 1), no audio impact.

  Concern: Complex scripts with nested loops could spike CPU. Mitigation: WHILE_DEPTH limit (10000 iterations) prevents runaway loops.

  4. File System (Well-Designed):

  Text format (.tele files):
  - Size: 3-6 KB per scene
  - Load time: 50-100ms (acceptable, async)
  - Storage: Negligible on 16GB SD

  Critical Issues & Mitigations

  ⚠️ Issue 1: Grid Structure Bloat

  Problem: scene_grid_t is ~8-10 KB of unused memory (256 buttons, 64 faders, etc.)

  Solution: Compile teletype with #define NO_GRID or manually strip grid structures from state.h

  Impact: Reduces per-track RAM from 13-15 KB → 5-7 KB

  ⚠️ Issue 2: Unknown Current RAM Usage

  Problem: I don't have visibility into how much RAM the current Project/Model uses

  Solution: Profile current RAM usage before implementing Teletype:
  // Add to main():
  extern char _ebss;  // End of BSS (from linker)
  extern char _stack; // Stack pointer
  uint32_t heapUsed = (uint32_t)&_ebss;
  uint32_t stackUsed = (uint32_t)&_stack - heapUsed;
  // Log these values

  Risk: If current usage is >140 KB, 2 Teletype tracks may not fit.

  ⚠️ Issue 3: Routing Target Explosion

  Problem: Adding 29 Teletype routing targets increases routing system complexity

  Mitigation: Routing infrastructure is already in place. Adding targets is mostly enum expansion, minimal RAM impact.

  ⚠️ Issue 4: Stack Safety

  Problem: Engine task stack is 4096 bytes. Teletype interpreter adds call depth.

  Solution:
  - Profile stack usage with Teletype active
  - Consider increasing ENGINE_TASK_STACK_SIZE to 5120 or 6144 if needed
  - Add stack overflow detection in debug builds

  Recommendations

  Phase 0: Pre-Implementation Profiling

  BEFORE starting Teletype integration:

  1. Measure current RAM usage:
    - Heap usage at startup
    - Peak heap usage during operation
    - Stack high-water marks for all tasks
  2. Establish baseline:
    - CPU utilization in Engine task
    - Worst-case Engine::update() timing
    - Flash usage
  3. Document headroom:
    - How much RAM is available for Teletype?
    - Can we fit 2 tracks? 3 tracks?

  Phase 1: Minimal Teletype Port

  MVP with lowest resource cost:

  1. Remove grid code:
  // state.h
  #ifdef PERFORMER_TELETYPE
  #define NO_GRID
  #endif
  2. Limit to 1 track initially:
    - Proves concept
    - Measures actual resource usage
    - Validates CPU timing
  3. Text-only file format:
    - Skip binary format initially
    - 50-100ms load time is acceptable
  4. Defer routable variables:
    - Start with basic routing (Octave, Transpose, Divisor)
    - Add 26 variable targets in Phase 2 if RAM permits

  Resource Targets

  | Resource         | Target   | Risk Level                      |
  |------------------|----------|---------------------------------|
  | RAM (1 track)    | < 10 KB  | ✅ Low                          |
  | RAM (2 tracks)   | < 20 KB  | ⚠️ Medium (depends on baseline) |
  | Flash            | < 100 KB | ✅ Low                          |
  | Engine::update() | < 1ms    | ✅ Low (with WHILE_DEPTH limit) |
  | File load        | < 100ms  | ✅ Low (async)                  |

  Final Verdict

  Overall Feasibility: ✅ FEASIBLE with careful implementation

  Confidence Level: 75-80%

  Success Depends On:
  1. Removing grid structures (mandatory)
  2. Current RAM usage < 140 KB (unknown, needs profiling)
  3. Limiting to 1-2 tracks (not 8)
  4. Conservative stack usage (may need to increase Engine stack)

  Biggest Unknowns:
  - Current Performer RAM footprint (Model + Project + Engines)
  - Real-world stack depth with Teletype scripts
  - Worst-case combined CPU load (2 Teletype tracks + 6 other tracks)

  Recommendation:
  Start with 1 Teletype track MVP after measuring baseline resource usage. Expand to 2 tracks only if profiling shows ≥30 KB free RAM headroom. The design is solid, but hardware constraints are tight enough that runtime validation is essential before committing to multi-track support.

## Executive Summary

Integrating Teletype as a track mode in Performer is architecturally feasible by treating it as a **script-driven sequencer track** that executes Teletype code in response to Performer's transport. The core Teletype interpreter and ops system can be ported largely intact, while hardware I/O and UI require comprehensive adapter layers to map Teletype's paradigm onto Performer's infrastructure.

**Feasibility: HIGH** - Core logic is portable, adapter layers are well-defined.

---

## Architecture Overview

### Core Integration Strategy

```
┌─────────────────────────────────────────────────────────────┐
│                    Performer Engine                         │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ TeletypeTrack (Model Layer)                          │  │
│  │  - Scripts (8 trigger + Metro + Init + Live)        │  │
│  │  - Patterns (4x64 arrays)                            │  │
│  │  - Variables (a,b,c,d,x,y,z,t, etc.)                │  │
│  │  - Output Routing (CV 1-20, TR 1-20 mapping)        │  │
│  │  - No Grid, No I2C data                             │  │
│  └───────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ TeletypeTrackEngine (Engine Layer)                   │  │
│  │  - tick() → run script based on trigger mapping     │  │
│  │  - update() → metro timer, delays                   │  │
│  │  - CV/Gate output via routing to ANY track          │  │
│  └───────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Teletype Core (Interpreter - mostly unchanged)       │  │
│  │  - parse() - Ragel scanner                          │  │
│  │  - process_command() - Op dispatcher                │  │
│  │  - tele_tick() - Delay/Wait handling                │  │
│  │  - scene_state_t - Runtime state (11 scripts)       │  │
│  └───────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ I/O Adapter Layer (Bridge to Performer)              │  │
│  │  - CV/Gate: tele_cv() → Multi-track routing         │  │
│  │  - Trigger In: TrackEngine::tick() → run_script()   │  │
│  │  - Clock: Performer Clock → Metro/Tick              │  │
│  │  - CV In: Performer Routing → scene_state.in/param  │  │
│  └───────────────────────────────────────────────────────┘  │
│                          ↓                                   │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ TeletypeTrackPage (UI Layer)                         │  │
│  │  - Script editor using step encoder + buttons       │  │
│  │  - Pattern tracker view                             │  │
│  │  - Variable monitor                                 │  │
│  │  - Output routing config                            │  │
│  │  - Context menus for ops/commands                   │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

---

## Native Performer Track Integration

### Making Teletype Feel Like a Performer Track

To ensure Teletype track behaves consistently with other Performer track types, we integrate standard track-level features that users expect:

#### 1. Track-Level Properties

```cpp
// TeletypeTrack.h - Add Performer-native properties
class TeletypeTrack {
public:
    //----------------------------------------
    // Standard Track Properties (like NoteTrack, IndexedTrack, etc.)
    //----------------------------------------

    // Octave shift (-10 to +10)
    int octave() const { return _octave.get(isRouted(Routing::Target::Octave)); }
    void setOctave(int octave, bool routed = false) {
        _octave.set(clamp(octave, -10, 10), routed);
    }
    void editOctave(int value, bool shift) {
        if (!isRouted(Routing::Target::Octave)) {
            setOctave(octave() + value);
        }
    }
    void printOctave(StringBuilder &str) const {
        printRouted(str, Routing::Target::Octave);
        str("%+d", octave());
    }

    // Transpose (-100 to +100 semitones)
    int transpose() const { return _transpose.get(isRouted(Routing::Target::Transpose)); }
    void setTranspose(int transpose, bool routed = false) {
        _transpose.set(clamp(transpose, -100, 100), routed);
    }
    void editTranspose(int value, bool shift) {
        if (!isRouted(Routing::Target::Transpose)) {
            setTranspose(this->transpose() + value);
        }
    }
    void printTranspose(StringBuilder &str) const {
        printRouted(str, Routing::Target::Transpose);
        str("%+d", transpose());
    }

    // CV Update Mode (Gate-triggered vs Always)
    enum class CvUpdateMode : uint8_t {
        Gate,    // Update CV only when gate is high (classic envelope behavior)
        Always,  // Update CV continuously (free-running)
        Last
    };

    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode mode) {
        _cvUpdateMode = ModelUtils::clampedEnum(mode);
    }
    void editCvUpdateMode(int value, bool shift) {
        setCvUpdateMode(ModelUtils::adjustedEnum(cvUpdateMode(), value));
    }
    static const char *cvUpdateModeName(CvUpdateMode mode) {
        switch (mode) {
        case CvUpdateMode::Gate:   return "Gate";
        case CvUpdateMode::Always: return "Always";
        case CvUpdateMode::Last:   break;
        }
        return nullptr;
    }

    // Divisor (for metro timing - sync to transport)
    int divisor() const { return _divisor.get(isRouted(Routing::Target::Divisor)); }
    void setDivisor(int divisor, bool routed = false) {
        _divisor.set(clamp(divisor, 1, 192), routed);
    }
    void editDivisor(int value, bool shift) {
        if (!isRouted(Routing::Target::Divisor)) {
            setDivisor(ModelUtils::adjustedByPowerOfTwo(divisor(), value, shift));
        }
    }
    void printDivisor(StringBuilder &str) const {
        printRouted(str, Routing::Target::Divisor);
        ModelUtils::printDivisor(str, divisor());
    }

    // Use Project Scale (vs Teletype's internal scale system)
    bool useProjectScale() const { return _useProjectScale; }
    void setUseProjectScale(bool use) { _useProjectScale = use; }
    void editUseProjectScale(int value, bool shift) {
        setUseProjectScale(value > 0);
    }
    void printUseProjectScale(StringBuilder &str) const {
        ModelUtils::printYesNo(str, useProjectScale());
    }

    // Metro Mode (realtime ms vs transport-synced divisor)
    enum class MetroMode : uint8_t {
        Realtime,   // Use M variable (milliseconds, independent of transport)
        Synced,     // Use divisor (tempo-synced)
        Last
    };

    MetroMode metroMode() const { return _metroMode; }
    void setMetroMode(MetroMode mode) {
        _metroMode = ModelUtils::clampedEnum(mode);
    }
    static const char *metroModeName(MetroMode mode) {
        switch (mode) {
        case MetroMode::Realtime: return "Realtime";
        case MetroMode::Synced:   return "Synced";
        case MetroMode::Last:     break;
        }
        return nullptr;
    }

private:
    Routable<int8_t> _octave;         // Default: 0
    Routable<int8_t> _transpose;      // Default: 0
    Routable<uint16_t> _divisor;      // Default: 12 (1/16th note)
    CvUpdateMode _cvUpdateMode = CvUpdateMode::Gate;
    MetroMode _metroMode = MetroMode::Realtime;
    bool _useProjectScale = true;     // Default: use Performer scales

    // ... existing Teletype state
};
```

#### 2. Divisor-Driven Metro

**Problem**: Teletype's metro uses milliseconds (independent of tempo). This breaks when tempo changes.

**Solution**: Add `MetroMode::Synced` that uses transport divisor instead.

```cpp
// TeletypeTrackEngine.cpp
void TeletypeTrackEngine::tick(uint32_t tick) {
    // Metro handling
    if (_track.teletypeTrack().metroMode() == TeletypeTrack::MetroMode::Synced) {
        // Transport-synced metro (respects tempo changes)
        uint32_t divisor = _track.teletypeTrack().divisor();
        if (tick % divisor == 0) {
            runScript(METRO_SCRIPT);
        }
    }
    // Realtime metro handled in update() below

    // ... rest of tick logic
}

void TeletypeTrackEngine::update(float dt) {
    // Realtime metro (original Teletype behavior)
    if (_track.teletypeTrack().metroMode() == TeletypeTrack::MetroMode::Realtime) {
        if (_track.teletypeTrack().metroEnabled()) {
            _metroAccumulator += dt;
            float intervalSec = _sceneState.variables.m / 1000.0f;
            if (_metroAccumulator >= intervalSec) {
                _metroAccumulator -= intervalSec;
                runScript(METRO_SCRIPT);
            }
        }
    }

    // ... system tick for delays, CV slewing, etc.
}
```

**UI Configuration:**
```
┌─────────────────────────────────────────────────────────────┐
│ Teletype Track - Settings                                  │
├─────────────────────────────────────────────────────────────┤
│ Metro Mode:    [Synced]  ← Realtime / Synced               │
│ Metro Divisor: [1/16]    ← Only shown in Synced mode       │
│ Metro Time:    [500ms]   ← Only shown in Realtime mode     │
│ Metro Active:  [Yes]                                        │
└─────────────────────────────────────────────────────────────┘
```

#### 3. Project Scale Integration

**Teletype's native ops:**
- `N x` - Convert note number to voltage (uses internal scale)
- `VV x` - Volts per volt (unscaled)
- `V x` - Direct voltage (scaled by 12TET)

**Performer integration:**

```cpp
// Override Teletype's N op to use Project scale
// teletype/src/ops/hardware.c (modified)

// BEFORE (original Teletype):
static void op_N_get(const void *NOTUSED(data), scene_state_t *ss,
                     exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t note = cs_pop(cs);
    // Fixed 12TET: 1V/oct, C = 0V
    cs_push(cs, (note * 1366) / 12);  // 16383 / 10V * (1V/12 semitones)
}

// AFTER (Performer-integrated):
#include "TeletypePerformerBridge.h"

static void op_N_get(const void *NOTUSED(data), scene_state_t *ss,
                     exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t note = cs_pop(cs);

    // Check if we should use Performer's project scale
    if (TeletypePerformerBridge::shouldUseProjectScale()) {
        // Use Performer's scale system
        int16_t scaledNote = TeletypePerformerBridge::quantizeNote(note);
        cs_push(cs, TeletypePerformerBridge::noteToTeletypeVoltage(scaledNote));
    } else {
        // Original Teletype behavior (12TET)
        cs_push(cs, (note * 1366) / 12);
    }
}
```

**Bridge implementation:**
```cpp
// TeletypePerformerBridge.h
class TeletypePerformerBridge {
public:
    static void setEngine(TeletypeTrackEngine *engine);

    static bool shouldUseProjectScale() {
        return s_engine && s_engine->track().teletypeTrack().useProjectScale();
    }

    static int16_t quantizeNote(int16_t note) {
        if (!s_engine) return note;

        // Apply octave and transpose
        const auto &tt = s_engine->track().teletypeTrack();
        note += (tt.octave() * 12) + tt.transpose();

        // Quantize to Performer's project scale
        const Scale &scale = s_engine->model().project().scale();
        int rootNote = s_engine->model().project().rootNote();

        return scale.quantize(note, rootNote);
    }

    static int16_t noteToTeletypeVoltage(int16_t note) {
        // Convert MIDI note (0-127) to Teletype voltage (0-16383)
        // Teletype: 0-16383 = 0-10V
        // MIDI note: 60 = C4 = 0V in V/oct
        return ((note - 60) * 1366) / 12;  // 1V/oct, 12TET spacing
    }

private:
    static TeletypeTrackEngine *s_engine;
};
```

**Example script with scale integration:**
```
# Script 1: Arpeggio using project scale
CV 1 N 0     # Root note (quantized to project scale)
TR.P 1

DEL 100: CV 1 N 4   # Fourth (quantized)
DEL 200: CV 1 N 7   # Fifth (quantized)
DEL 300: CV 1 N 12  # Octave (quantized)
```

With `UseProjectScale = Yes`, if project is in C Minor, this produces C-Eb-G-C instead of C-E-G-C.

#### 4. Gate-Triggered vs Always CV Update

**Like NoteTrack's CV Update Mode:**

```cpp
// TeletypeTrackEngine.cpp
void TeletypeTrackEngine::setCvOutputInternal(uint8_t channel, float value, bool slew) {
    if (channel >= 4) return;

    // Check CV update mode
    auto mode = _track.teletypeTrack().cvUpdateMode();

    if (mode == TeletypeTrack::CvUpdateMode::Gate) {
        // Only update CV when gate is active
        if (!_gateOutputs[channel]) {
            // Gate is low, don't update CV (hold current value)
            return;
        }
    }
    // CvUpdateMode::Always - update CV regardless of gate state

    _cvTargets[channel] = value;

    if (slew) {
        int16_t slewMs = _sceneState.variables.cv_slew[channel];
        _cvSlewRates[channel] = slewMs > 0 ? (1.0f / (slewMs / 1000.0f)) : 0.0f;
    } else {
        _cvOutputs[channel] = value;
        _cvSlewRates[channel] = 0.0f;
    }
}
```

**Use Case:**
- **Gate mode**: Classic envelope behavior - CV changes only when note is triggered
- **Always mode**: LFO/modulation - CV runs continuously regardless of gates

**Example:**
```
# Script 1: Gate-triggered melody (CV Update Mode = Gate)
CV 1 N RRAND 0 12   # Random note
TR.PULSE 1           # Trigger gate
# CV changes only when gate triggers (classic behavior)

# Script M: Free-running LFO (CV Update Mode = Always)
X ADD X 1
CV 2 * X 100         # Sawtooth LFO
# CV updates every metro tick, gate state doesn't matter
```

#### 5. Routable Parameters - Complete System

Performer's routing system allows CV inputs, MIDI, and other sources to modulate track parameters in real-time. TeletypeTrack supports both standard track parameters and Teletype-specific parameters.

##### 5.1. Standard Track-Level Routing Targets

These use existing `Routing::Target` enum values (shared with other track types):

```cpp
// Already defined in Routing.h
enum class Target : uint8_t {
    // ... existing targets ...

    // Track targets (shared across all track types)
    Octave,           // -10 to +10
    Transpose,        // -100 to +100
    SlideTime,        // 0-100%

    // Sequence targets
    Divisor,          // 1-192 (PPQN divisions)

    // ... other targets ...
};
```

**Implementation in TeletypeTrack:**

```cpp
// TeletypeTrack.h
class TeletypeTrack {
public:
    // Octave (uses existing Routing::Target::Octave)
    int octave() const {
        return _octave.get(Routing::isRouted(Routing::Target::Octave, _trackIndex));
    }
    void setOctave(int octave, bool routed = false) {
        _octave.set(clamp(octave, -10, 10), routed);
    }

    // Transpose (uses existing Routing::Target::Transpose)
    int transpose() const {
        return _transpose.get(Routing::isRouted(Routing::Target::Transpose, _trackIndex));
    }
    void setTranspose(int transpose, bool routed = false) {
        _transpose.set(clamp(transpose, -100, 100), routed);
    }

    // Divisor (uses existing Routing::Target::Divisor)
    int divisor() const {
        return _divisor.get(Routing::isRouted(Routing::Target::Divisor, _trackIndex));
    }
    void setDivisor(int divisor, bool routed = false) {
        _divisor.set(clamp(divisor, 1, 192), routed);
    }

private:
    Routable<int8_t> _octave;         // Default: 0
    Routable<int8_t> _transpose;      // Default: 0
    Routable<uint16_t> _divisor;      // Default: 12 (1/16th note)
    int8_t _trackIndex;               // For routing lookups
};
```

##### 5.2. Teletype-Specific Routing Targets

**NEW targets added to Routing.h:**

```cpp
// Routing.h - Add Teletype routing targets
enum class Target : uint8_t {
    // ... existing targets ...

    // Teletype-specific targets
    TeletypeFirst,
    TeletypeMetroPeriod = TeletypeFirst,  // Metro period (1-10000ms, Realtime mode only)
    TeletypeVarA,                          // Variable A (0-16383)
    TeletypeVarB,                          // Variable B (0-16383)
    TeletypeVarC,                          // Variable C (0-16383)
    TeletypeVarD,                          // Variable D (0-16383)
    // ... Variables E-Z would follow (26 total)
    TeletypeVarZ,                          // Variable Z (0-16383)
    TeletypePatternIndex,                  // Active pattern (0-3)
    TeletypeLast = TeletypePatternIndex,

    Last,
};

static bool isTeletypeTarget(Target target) {
    return target >= Target::TeletypeFirst && target <= Target::TeletypeLast;
}
```

**Target naming:**

```cpp
static const char *targetName(Target target) {
    switch (target) {
    // ... existing cases ...

    case Target::TeletypeMetroPeriod:  return "TT Metro";
    case Target::TeletypeVarA:         return "TT Var A";
    case Target::TeletypeVarB:         return "TT Var B";
    // ... etc for C-Z
    case Target::TeletypePatternIndex: return "TT Pattern";
    // ... rest
    }
}
```

**Serialization mapping:**

```cpp
static uint8_t targetSerialize(Target target) {
    switch (target) {
    // ... existing targets 0-55 ...

    // Teletype targets (56-84)
    case Target::TeletypeMetroPeriod:  return 56;
    case Target::TeletypeVarA:         return 57;
    case Target::TeletypeVarB:         return 58;
    // ... etc through Z
    case Target::TeletypeVarZ:         return 82;  // 57 + 25
    case Target::TeletypePatternIndex: return 83;

    case Target::Last: break;
    }
    return 0;
}
```

##### 5.3. Variables as Routable Parameters (CRITICAL FEATURE)

**Why This Is Important:**

Routing CV sources to Teletype variables creates **externally modulated scripting**:

```
# Example: Route CV In 1 → Variable A (configured in UI)
# Script reads A dynamically:

CV 1 N + 60 A      # Pitch = C4 + CV input
TR.P 1

# A now ranges 0-16383 based on CV In 1 voltage (0-10V)
# User patches external LFO → CV In 1 → modulates pitch
```

**Implementation:**

```cpp
// TeletypeTrack.h
class TeletypeTrack {
public:
    // Routable variables (A-Z = 26 variables)
    static constexpr int VAR_COUNT = 26;

    int variable(int index) const {
        if (index < 0 || index >= VAR_COUNT) return 0;

        // Check if this variable is routed
        Routing::Target target = static_cast<Routing::Target>(
            static_cast<int>(Routing::Target::TeletypeVarA) + index
        );

        return _variables[index].get(Routing::isRouted(target, _trackIndex));
    }

    void setVariable(int index, int value, bool routed = false) {
        if (index < 0 || index >= VAR_COUNT) return;
        _variables[index].set(clamp(value, 0, 16383), routed);
    }

    // Helper to get variable by letter
    int variableByName(char letter) const {
        int index = toupper(letter) - 'A';
        return variable(index);
    }

private:
    std::array<Routable<int16_t>, VAR_COUNT> _variables;  // A-Z
};
```

**Engine integration:**

```cpp
// TeletypeTrackEngine.cpp
void TeletypeTrackEngine::updateRoutedVariables() {
    // Called from update() - sync routed variables to scene_state
    for (int i = 0; i < TeletypeTrack::VAR_COUNT; ++i) {
        // If variable is routed, copy routed value to scene_state
        Routing::Target target = static_cast<Routing::Target>(
            static_cast<int>(Routing::Target::TeletypeVarA) + i
        );

        if (Routing::isRouted(target, _track.trackIndex())) {
            // Variable is controlled by routing - update scene_state
            int routedValue = _track.teletypeTrack().variable(i);
            _sceneState.variables.a + i = routedValue;  // a, b, c, ... z
        }
        // If not routed, scene_state value is controlled by scripts
    }
}
```

##### 5.4. Metro Period Routing

**Use Case:** Tempo modulation via CV

```cpp
// TeletypeTrack.h
class TeletypeTrack {
public:
    // Metro period (ms, only used in Realtime mode)
    int metroPeriod() const {
        return _metroPeriod.get(
            Routing::isRouted(Routing::Target::TeletypeMetroPeriod, _trackIndex)
        );
    }
    void setMetroPeriod(int ms, bool routed = false) {
        _metroPeriod.set(clamp(ms, 1, 10000), routed);
    }

private:
    Routable<uint16_t> _metroPeriod;  // Default: 1000ms
};
```

**Engine behavior:**

```cpp
// TeletypeTrackEngine.cpp
void TeletypeTrackEngine::update(float dt) {
    if (_track.teletypeTrack().metroMode() == TeletypeTrack::MetroMode::Realtime) {
        // Use routed metro period if available
        int periodMs = _track.teletypeTrack().metroPeriod();

        _metroAccumulator += dt;
        float intervalSec = periodMs / 1000.0f;

        if (_metroAccumulator >= intervalSec) {
            _metroAccumulator -= intervalSec;
            runScript(METRO_SCRIPT);
        }
    }
    // Synced mode uses divisor (already routable)
}
```

**Routing example:**
```
Route: CV In 2 → TT Metro Period (range: 100ms to 2000ms)
Result: External CV controls metro speed
```

##### 5.5. Pattern Index Routing

**Use Case:** Pattern switching via CV

```cpp
// TeletypeTrack.h
class TeletypeTrack {
public:
    int activePatternIndex() const {
        return _activePatternIndex.get(
            Routing::isRouted(Routing::Target::TeletypePatternIndex, _trackIndex)
        );
    }
    void setActivePatternIndex(int index, bool routed = false) {
        _activePatternIndex.set(clamp(index, 0, 3), routed);
    }

private:
    Routable<uint8_t> _activePatternIndex;  // Default: 0
};
```

**Script integration:**

```cpp
// When script reads pattern (e.g., P.N 0), check active index
int activePattern = _track.teletypeTrack().activePatternIndex();
int value = _sceneState.patterns[activePattern].data[position];
```

**Routing example:**
```
Route: CV In 3 → TT Pattern Index (range: 0-3)
Result: External CV selects which pattern is active
```

##### 5.6. Routing Configuration UI

**Routing Page:**

```
┌─────────────────────────────────────────────────────────────┐
│ Routing  [Route 1]                                          │
├─────────────────────────────────────────────────────────────┤
│ Source:    [CV In 1      ]                                  │
│ Target:    [TT Var A     ]                                  │
│ Tracks:    [----X---]  ← Track 5 (Teletype track)          │
│ Min:       [0           ]                                   │
│ Max:       [16383       ]                                   │
│ Shaper:    [None        ]                                   │
└─────────────────────────────────────────────────────────────┘

Available Teletype Targets:
  - TT Metro         (Metro period, 1-10000ms)
  - TT Var A..Z      (Variables A-Z, 0-16383)
  - TT Pattern       (Pattern index, 0-3)
  - Octave           (Standard track target, -10 to +10)
  - Transpose        (Standard track target, -100 to +100)
  - Divisor          (Standard target, 1-192)
```

##### 5.7. Complete Routing Examples

**Example 1: External LFO Controls Pitch via Variable**

```
Routing Config:
  Route 1: CV In 1 → TT Var A (range: -2400 to +2400)

Teletype Script 1 (triggered by gate):
  CV 1 N + 60 A      # C4 + variable A (CV-controlled detune)
  TR.P 1

Result: External LFO on CV In 1 modulates pitch ±2 octaves
```

**Example 2: MIDI CC Controls Metro Speed**

```
Routing Config:
  Route 2: MIDI CC 1 → TT Metro (range: 100ms to 2000ms)

Teletype Metro Script:
  X RRAND 0 12       # Random note
  CV 1 N X
  TR.P 1

Result: MIDI controller modulates random arpeggio speed
```

**Example 3: CV Switches Patterns**

```
Routing Config:
  Route 3: CV In 2 → TT Pattern (range: 0-3)

Teletype Script 1:
  # Reads active pattern (selected by CV In 2)
  CV 1 N P.NEXT 0    # Step through active pattern
  TR.P 1

Result: External CV selects which of 4 patterns is played
```

**Example 4: Envelope Controls Transpose**

```
Routing Config:
  Route 4: Gate Out 3 → TT Transpose (range: -12 to +12)
  Shaper: Envelope (AR)

Teletype Script 1:
  CV 1 N 60          # C4 with routed transpose
  TR.P 1

Result: Another track's gate creates pitch sweep via envelope shaping
```

**Example 5: Multi-Parameter Modulation**

```
Routing Config:
  Route 1: CV In 1 → TT Var A (range: 0-16383)
  Route 2: CV In 2 → TT Var B (range: 0-100)
  Route 3: CV In 3 → Octave (range: -2 to +2)

Teletype Script M (metro):
  # A controls pitch offset, B controls gate probability
  IF GT RRAND 0 100 B: CV 1 N + 60 A; TR.P 1

Result: Three CVs control different aspects of generative melody
```

##### 5.8. Routing Engine Integration

**RoutingEngine needs to handle Teletype targets:**

```cpp
// RoutingEngine.cpp
void RoutingEngine::writeTarget(const Route &route, float normalizedValue) {
    if (Routing::isTeletypeTarget(route.target())) {
        // Handle Teletype-specific targets
        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            if (!(route.tracks() & (1 << trackIndex))) continue;

            Track &track = _model.project().track(trackIndex);
            if (track.trackMode() != Track::TrackMode::Teletype) continue;

            TeletypeTrack &tt = track.teletypeTrack();

            // Denormalize value to target range
            float denorm = Routing::denormalizeTargetValue(route.target(), normalizedValue);

            switch (route.target()) {
            case Routing::Target::TeletypeMetroPeriod:
                tt.setMetroPeriod(int(denorm), true /* routed */);
                break;

            case Routing::Target::TeletypeVarA:
            case Routing::Target::TeletypeVarB:
            // ... through TeletypeVarZ:
                int varIndex = static_cast<int>(route.target()) -
                               static_cast<int>(Routing::Target::TeletypeVarA);
                tt.setVariable(varIndex, int(denorm), true /* routed */);
                break;

            case Routing::Target::TeletypePatternIndex:
                tt.setActivePatternIndex(int(denorm), true /* routed */);
                break;

            default:
                break;
            }
        }
    }
    // ... handle other target types
}
```

**Value range helpers:**

```cpp
// Routing.cpp
std::pair<float, float> Routing::targetValueRange(Target target) {
    switch (target) {
    // ... existing targets ...

    case Target::TeletypeMetroPeriod:  return { 1.f, 10000.f };
    case Target::TeletypeVarA:
    case Target::TeletypeVarB:
    // ... through TeletypeVarZ:
        return { 0.f, 16383.f };
    case Target::TeletypePatternIndex: return { 0.f, 3.f };

    // ... rest
    }
}
```

##### 5.9. Summary: Routable Parameter Categories

| Category | Parameters | Range | Use Case |
|----------|-----------|-------|----------|
| **Track-level** | Octave | -10 to +10 | Pitch shifting via CV |
| | Transpose | -100 to +100 | MIDI CC transpose control |
| | Divisor | 1-192 | Tempo modulation |
| **Teletype Metro** | Metro Period | 1-10000ms | CV-controlled tempo (Realtime mode) |
| **Teletype Variables** | A-Z (26 vars) | 0-16383 | External CV → script parameters |
| **Teletype Patterns** | Pattern Index | 0-3 | CV pattern selection |

**Total Teletype Routing Targets:** 29
- 3 shared track targets (Octave, Transpose, Divisor)
- 1 metro target (Metro Period)
- 26 variable targets (A-Z)
- 1 pattern target (Pattern Index)

---

## Teletype Script System Deep Dive

### Script Types and Indices

Teletype has **11 script slots** with specific triggering mechanisms:

```cpp
// teletype/src/script.h
#define REGULAR_SCRIPT_COUNT 8      // Scripts 0-7
#define METRO_SCRIPT 8               // Script 8
#define INIT_SCRIPT 9                // Script 9
#define DELAY_SCRIPT 10              // Internal (for delayed commands)
#define LIVE_SCRIPT 11               // Live mode REPL
```

| Script Index | Name | Trigger | Purpose |
|--------------|------|---------|---------|
| 0-7 | Trigger Scripts 1-8 | TR inputs with configurable edge detection | User-assignable event handlers |
| 8 | METRO (M) | Timer callback at interval `M` (ms) | Periodic execution (like a clock divider) |
| 9 | INIT (I) | Scene load, reset, manual `INIT.SCRIPT` op | Initialization code |
| 10 | DELAY | Internal | Holds delayed commands from `DEL` op |
| 11 | LIVE | Manual | REPL-entered commands |

### Script Trigger Polarity

Each trigger script (0-7) has a **polarity bitmask** in `scene_state.variables.script_pol[i]`:

```cpp
// From teletype/module/main.c handler_Trigger()
bool tr_state = gpio_get_pin_value(input);
if (tr_state) {
    if (script_pol[input] & 1) {  // Bit 0: Rising edge
        run_script(&scene_state, input);
    }
}
else {
    if (script_pol[input] & 2) {  // Bit 1: Falling edge
        run_script(&scene_state, input);
    }
}
```

**Polarity values:**
- `0`: Disabled (no trigger)
- `1`: Rising edge only
- `2`: Falling edge only
- `3`: Both edges

This allows fine-grained control over when scripts execute.

### INIT Script Behavior

INIT script runs in these scenarios:
1. **Scene load** (`flash_read()` → `deserialize_scene()` → `run_script(INIT_SCRIPT)`)
2. **Manual trigger** via `INIT.SCRIPT` op
3. **Project reset** (user action)

**Typical INIT script uses:**
```
# Script I (INIT)
M 500            # Set metro to 500ms
CV.SLEW 1 100    # Set CV 1 slew to 100ms
A 0              # Initialize variable A
SCRIPT.POL 1 1   # Set script 1 to rising edge
```

### METRO Script Timing

METRO script uses a software timer with interval controlled by variable `M`:

```cpp
// teletype/module/main.c
void tele_metro_updated() {
    int16_t metro_time = scene_state.variables.m;
    if (metro_time < METRO_MIN_MS) metro_time = METRO_MIN_MS;

    if (scene_state.variables.m_act) {
        timer_add(&metroTimer, metro_time, &metroTimer_callback, NULL);
    } else {
        timer_remove(&metroTimer);
    }
}

void metroTimer_callback(void* o) {
    if (ss_get_script_len(&scene_state, METRO_SCRIPT)) {
        run_script(&scene_state, METRO_SCRIPT);
    }
}
```

**Key ops:**
- `M`: Get/set metro interval (milliseconds)
- `M.ACT`: Enable/disable metro (0=off, 1=on)
- `M.RESET`: Reset metro timer to start of period

**Typical METRO script:**
```
# Script M (METRO)
X ADD X 1         # Increment counter
CV 1 RRAND 0 16383  # Random CV on output 1
IF EQ X 8: X 0    # Wrap counter at 8
```

---

## What Can Be Ported Directly

### 1. Core Interpreter (`teletype/src/`)

**Files to port unchanged:**
- `teletype.c/.h` - Main interpreter loop
- `command.c/.h` - Command structure
- `scanner.c` (Ragel-generated) - Tokenizer
- `match_token.c` - Op lookup

**Rationale**: These are pure logic with no hardware dependencies.

### 2. Ops System (`teletype/src/ops/`)

**Port with minor modifications:**
- `variables.c/.h` - General purpose registers
- `patterns.c/.h` - Pattern arrays
- `maths.c/.h` - Math operations
- `stack.c/.h` - Stack operations
- `queue.c/.h` - Queue operations
- `controlflow.c/.h` - IF/WHILE/L loops
- `delay.c/.h` - WAIT/DEL commands
- `metronome.c/.h` - Metro timer
- `hardware.c/.h` - CV/TR (needs I/O wrapper)
- `midi.c/.h` - MIDI ops (route to Performer's MIDI)

**Omit entirely (I2C-dependent):**
- `ansible.c/.h`
- `crow.c/.h`
- `disting.c/.h`
- `earthsea.c/.h`
- `er301.c/.h`
- `justfriends.c/.h`
- `meadowphysics.c/.h`
- `telex.c/.h`
- `whitewhale.c/.h`
- `wslash*.c/.h`
- `i2c.c/.h`
- `grid_ops.c/.h` - Grid hardware specific

### 3. State Management

**Port `state.c/.h` with reductions:**
```cpp
// TeletypeTrack.h (Model Layer)
class TeletypeTrack {
    // KEEP:
    scene_variables_t _variables;      // a,b,c,d,x,y,z,t, cv[], tr[], etc.
    scene_pattern_t _patterns[4];      // 4 patterns
    scene_delay_t _delays;             // WAIT queue
    scene_script_t _scripts[11];       // 8 trigger + Metro + Init + (optional Live)
    scene_rand_t _randStates;          // RNG states
    scene_midi_t _midi;                // MIDI state

    // OMIT:
    // scene_grid_t - No Grid support
    // i2c_op_address - No I2C
};
```

---

## What Needs Wrappers/Adapters

### 1. Hardware I/O Abstraction

Teletype expects direct hardware calls (`tele_cv()`, `tele_tr()`). We need an adapter:

```cpp
// TeletypeHardwareAdapter.h
class TeletypeHardwareAdapter {
public:
    // Called by teletype ops (hardware.c)
    static void setCvOutput(uint8_t channel, int16_t value, bool slew);
    static void setTrOutput(uint8_t channel, bool state);
    static void pulseTrOutput(uint8_t channel, uint16_t duration_ms);

    // Called by teletype ops to read inputs
    static int16_t getCvInput(uint8_t channel);
    static bool getTrInput(uint8_t channel);

    // Injected by TeletypeTrackEngine
    static void setEngine(TeletypeTrackEngine* engine);

private:
    static TeletypeTrackEngine* s_engine;
};

// Implementation maps to Performer's routing
void TeletypeHardwareAdapter::setCvOutput(uint8_t channel, int16_t value, bool slew) {
    if (!s_engine) return;

    // Teletype CV range: 0-16383 (14-bit, 0-10V)
    // Performer CV range: -1.0 to 1.0 (normalized)
    float normalized = (value / 16383.0f) * 2.0f - 1.0f;

    s_engine->setCvOutputInternal(channel, normalized, slew);
}
```

**Modified Teletype files:**
```cpp
// teletype/src/ops/hardware.c
// Replace direct hardware calls:

// OLD:
void tele_cv(uint8_t output, int16_t value, uint8_t slew) {
    aout[output].target = value;
    aout[output].slew = slew;
}

// NEW:
void tele_cv(uint8_t output, int16_t value, uint8_t slew) {
    TeletypeHardwareAdapter::setCvOutput(output, value, slew);
}
```

### 2. Extended CV/TR Output Routing (CRITICAL FEATURE)

**Teletype's Expandable I/O via I2C:**

In the original Teletype hardware, scripts can address up to **20 CV outputs** and **20 TR outputs**:
- **CV 1-4 / TR 1-4**: Local Teletype hardware
- **CV 5-20 / TR 5-20**: Via I2C to Ansible expansion modules (4 Ansibles × 4 outputs each)

```cpp
// From teletype/src/ops/hardware.c op_CV_set()
int16_t a = cs_pop(cs);  // CV channel (1-based)
a--;  // Convert to 0-based

if (a < 4) {
    // Local hardware
    ss->variables.cv[a] = value;
    tele_cv(a, value, 1);
}
else if (a < 20) {
    // I2C to Ansible (addresses 5-20 → Ansible modules)
    uint8_t d[] = { II_ANSIBLE_CV, a & 0x3, value >> 8, value & 0xff };
    uint8_t addr = II_ANSIBLE_ADDR + (((a - 4) >> 2) << 1);
    tele_ii_tx(addr, d, 4);
}
```

**Why This Matters for Performer:**

We can replicate this expandability by **routing Teletype CV/TR outputs to ANY Performer track's outputs**, not just the Teletype track's own outputs!

#### Proposed Routing Architecture

```cpp
// TeletypeTrack.h - Output Routing Configuration
class TeletypeTrack {
public:
    struct OutputRoute {
        uint8_t targetTrack;   // 0-7 (which Performer track)
        uint8_t targetChannel; // 0-3 (which CV/Gate on that track)
        bool enabled;

        void clear() { targetTrack = 0; targetChannel = 0; enabled = false; }
        void write(VersionedSerializedWriter &writer) const;
        void read(VersionedSerializedReader &reader);
    };

    // CV output routing (1-20)
    const OutputRoute &cvOutputRoute(int index) const {
        return _cvOutputRoutes[index];
    }
    void setCvOutputRoute(int index, const OutputRoute &route);

    // TR output routing (1-20)
    const OutputRoute &trOutputRoute(int index) const {
        return _trOutputRoutes[index];
    }
    void setTrOutputRoute(int index, const OutputRoute &route);

private:
    OutputRoute _cvOutputRoutes[20];  // CV 1-20
    OutputRoute _trOutputRoutes[20];  // TR 1-20
};
```

#### Example Routing Configuration

```
Teletype Track (Track 1):
  CV 1 → Track 1, CV Out 0 (self)
  CV 2 → Track 1, CV Out 1 (self)
  CV 3 → Track 2, CV Out 0 (control another track!)
  CV 4 → Track 2, CV Out 1 (control another track!)
  CV 5 → Track 3, CV Out 0
  CV 6 → Track 3, CV Out 1
  CV 7-20 → Disabled (or map to other tracks)

  TR 1 → Track 1, Gate 0
  TR 2 → Track 4, Gate 0 (trigger a different track's gate!)
  TR 3-20 → ...
```

**Use Case: Master Sequencer**

```
# Teletype track controls multiple tracks
CV 1 V 0      # Track 1 CV 1 → C note
TR.PULSE 1    # Track 1 Gate  → trigger

CV 3 V 7      # Track 2 CV 1 → G note (harmony)
TR.PULSE 2    # Track 2 Gate  → trigger

CV 5 RRAND 0 16383  # Track 3 CV 1 → random modulation
```

A single Teletype script becomes a **conductor** that orchestrates the entire Performer rig!

#### Hardware Adapter Implementation

```cpp
// TeletypeHardwareAdapter.cpp
void TeletypeHardwareAdapter::setCvOutput(uint8_t channel, int16_t value, bool slew) {
    if (!s_engine || channel >= 20) return;

    const auto &route = s_engine->track().teletypeTrack().cvOutputRoute(channel);
    if (!route.enabled) return;

    // Teletype: 0-16383 (0-10V)
    // Performer: -1.0 to 1.0
    float normalized = (value / 16383.0f) * 2.0f - 1.0f;

    // Route to target track's output
    s_engine->setCvOutputToTrack(route.targetTrack, route.targetChannel,
                                  normalized, slew);
}
```

#### Engine Cross-Track Access

```cpp
// TeletypeTrackEngine.h
class TeletypeTrackEngine : public TrackEngine {
public:
    // Route CV output to another track
    void setCvOutputToTrack(uint8_t trackIndex, uint8_t channel,
                            float value, bool slew);

    // Route Gate output to another track
    void setGateOutputToTrack(uint8_t trackIndex, uint8_t channel, bool state);

    // Implementation via Engine reference
private:
    void setCvOutputToTrack(uint8_t trackIndex, uint8_t channel,
                            float value, bool slew) {
        // Access Performer's routing engine
        // This requires adding a method to Engine or RoutingEngine

        // Option 1: Direct routing table access (if available)
        // _engine.routing().setCvSource(trackIndex, channel, value);

        // Option 2: Via track state (cleaner)
        // Store in a shared buffer that routing engine reads

        // For now, store in local buffer indexed by [trackIndex][channel]
        _crossTrackCvOutputs[trackIndex][channel] = value;
        _crossTrackCvSlew[trackIndex][channel] = slew;
    }

    float _crossTrackCvOutputs[8][8];  // 8 tracks × 8 CV outs
    bool _crossTrackGateOutputs[8][8];
};
```

#### UI for Output Routing

Add a **new page** or **submenu** in TeletypeTrackPage:

```
┌─────────────────────────────────────────────────────────────┐
│ Teletype - Output Routing                                  │
├─────────────────────────────────────────────────────────────┤
│ CV Outputs:                                                 │
│  CV  1 → Track 1 / CV 0  ✓                                 │
│  CV  2 → Track 1 / CV 1  ✓                                 │
│  CV  3 → Track 2 / CV 0  ✓  ← Cross-track!                │
│  CV  4 → Track 2 / CV 1  ✓  ← Cross-track!                │
│  CV  5 → [Disabled]                                        │
│  ...                                                        │
│                                                             │
│ TR Outputs:                                                │
│  TR  1 → Track 1 / Gate 0  ✓                               │
│  TR  2 → Track 4 / Gate 0  ✓  ← Trigger another track!    │
│  TR  3 → [Disabled]                                        │
│                                                             │
│ [Encoder: Select] [Function: Toggle] [Edit: Track/Chan]   │
└─────────────────────────────────────────────────────────────┘
```

**Navigation:**
- **Encoder**: Scroll through CV 1-20, TR 1-20
- **Function**: Enable/disable selected route
- **Left/Right**: Select target track (0-7)
- **Shift+Left/Right**: Select target channel (0-3 for CV, 0-3 for Gate)

#### Integration with Performer Routing

**Option A: Bypass Routing (Direct Injection)**

Teletype track directly writes to other tracks' output buffers, bypassing normal routing.

**Pros:** Simple, fast
**Cons:** Conflicts with tracks' own outputs, no standard routing features

**Option B: Virtual Sources**

Register Teletype outputs as routing sources (e.g., `TeletypeCV1`, `TeletypeCV2`, ...)

```cpp
// Register with routing engine
_engine.routing().registerSource("Teletype CV 1", ...);
_engine.routing().registerSource("Teletype CV 2", ...);
// ... up to CV 20

// Then normal routing handles it
```

**Pros:** Full integration with routing system, clean architecture
**Cons:** More complex, requires routing engine changes

**Recommended: Option B** - Register as sources, use routing engine.

---

### 3. Input Mapping Strategy

Performer has different inputs than Teletype's 8 trigger inputs. We need flexible mapping:

```cpp
// TeletypeInputRouter.h
class TeletypeInputRouter {
public:
    enum InputSource {
        // Performer-specific sources
        PerformerClock,        // Map to Script 1 (like TR 1)
        PerformerGateIn1,      // Map to Script 2
        PerformerGateIn2,      // Map to Script 3
        PerformerGateIn3,      // Map to Script 4
        PerformerGateIn4,      // Map to Script 5
        StepButton,            // Map to Script 6 (manual trigger)
        FunctionButton,        // Map to Script 7
        ShiftButton,           // Map to Script 8
        // Additional sources
        TrackMuteToggle,       // Could trigger Init
        PatternChange,         // Could trigger specific script
    };

    // Configuration per script
    struct ScriptTriggerConfig {
        InputSource source;
        uint8_t edgePolarity;  // 0=Rising, 1=Falling, 2=Both
        bool enabled;
    };

    ScriptTriggerConfig scriptTriggers[8];

    // Called by TeletypeTrackEngine when events occur
    void routeInputEvent(InputSource source, bool risingEdge);
};
```

### 3. CV Input Mapping

Teletype has 4 CV inputs. Performer has 4 CV inputs + encoders. Map creatively:

```cpp
// TeletypeTrack.h
class TeletypeTrack {
public:
    enum CvInputMapping {
        CvIn1,           // Physical CV In 1
        CvIn2,           // Physical CV In 2
        CvIn3,           // Physical CV In 3
        CvIn4,           // Physical CV In 4
        StepEncoder,     // Encoder as "PARAM"
        ProjectScale,    // Current project scale as pitched CV
        ProjectTempo,    // BPM mapped to CV range
        TrackProgress,   // Sequence progress (0-100%)
    };

    CvInputMapping cvInputMap[4];  // What IN, PARAM map to

    // Scale/range for each input
    int16_t cvInputMin[4];
    int16_t cvInputMax[4];
};
```

**Adapter implementation:**
```cpp
int16_t TeletypeHardwareAdapter::getCvInput(uint8_t channel) {
    switch (s_engine->track().cvInputMap[channel]) {
    case CvIn1:
        return s_engine->model().cvInput(0);  // Read from routing engine
    case StepEncoder:
        return s_engine->getEncoderValue() * 100;  // Map to 0-16383
    case ProjectTempo:
        return s_engine->engine().clock().bpm() * 100;  // BPM → CV
    // ...
    }
}
```

### 4. Clock/Timing Integration

Teletype has 3 timing sources. Map to Performer's clock:

```cpp
// TeletypeTrackEngine.cpp

void TeletypeTrackEngine::tick(uint32_t tick) {
    // Option 1: Trigger Metro script at divisor intervals
    if (tick % _metroDiv == 0) {
        run_script(&_sceneState, METRO_SCRIPT);  // Script 9
    }

    // Option 2: Trigger scripts based on input routing
    // (See InputRouter above)
}

void TeletypeTrackEngine::update(float dt) {
    // Teletype system tick (for delays)
    // Convert dt to Teletype's 10ms ticks
    static float accumulator = 0.0f;
    accumulator += dt;

    while (accumulator >= 0.010f) {  // 10ms tick
        tele_tick(&_sceneState, 10);
        accumulator -= 0.010f;
    }

    // CV slewing
    updateCvSlewing(dt);
}
```

---

## What's Impossible / Omitted

### 1. I2C Communication
**Reason**: Performer has no I2C bus for Monome ecosystem.

**Omitted ops:**
- All Trilogy module ops (Ansible, Meadowphysics, etc.)
- Grid ops (G.*)
- ER-301 ops
- Crow ops
- JustFriends ops
- W/ ops (Whitewhale)

**Mitigation**: Document which ops are unsupported. Return error if used.

```cpp
// In op.c, replace i2c ops with stubs:
void op_stub_unsupported(const void *data, scene_state_t *ss,
                        exec_state_t *es, command_state_t *cs) {
    // Silently fail or log warning
    // Could set a flag to display "I2C op not supported" in UI
}
```

### 2. Grid Integration
**Reason**: No USB host for Grid hardware.

**Omitted**: All `scene_grid_t` state and grid_ops.

### 3. Keyboard-based Live Mode
**Reason**: No keyboard.

**Solution**: Replace with encoder-based command entry (see UI section below).

### 4. Text-based Help Screen
**Reason**: Limited screen space.

**Solution**: Context-sensitive help via menus, or omit entirely. Use external reference.

---

## UI Layer Design

### Challenge
Teletype uses a keyboard and 128x64 screen. Performer uses:
- **Input**: 16 step buttons, Left/Right, Function, Page, Shift, Step Encoder
- **Display**: 256x64 OLED (larger, but no keyboard)

### Solution: Modal UI with Contextual Menus

```
┌─────────────────────────────────────────────────────────────┐
│                  Teletype Track - Page 1                    │
├─────────────────────────────────────────────────────────────┤
│ Script: [1] ▸  (F+Step1-8 to select script 1-8)           │
│                                                             │
│ Line 1: CV 1 N 60                                          │
│ Line 2: TR.PULSE 1                                         │
│ Line 3: X ADD X 1                                          │
│ Line 4: IF GT X 8: X 0                                     │
│ Line 5: DEL 500: TR 1 0                                    │
│ Line 6: ─                                                  │
│                                                             │
│ [EDIT] [VARS] [PATT] [CFG]  ← Context menu (encoder sel.) │
└─────────────────────────────────────────────────────────────┘
```

#### Page 1: Script Editor

**Navigation:**
- **Step Buttons 1-8**: Select script 1-8 (with Shift: Metro, Init)
- **Encoder Up/Down**: Navigate lines within script
- **Function + Encoder**: Select context action (Edit, Vars, Pattern, Config)

**Editing Mode:**

When "EDIT" selected:
```
┌─────────────────────────────────────────────────────────────┐
│ Edit Line 3: X ADD X 1                                      │
├─────────────────────────────────────────────────────────────┤
│ Cursor: X▐ADD X 1                                           │
│                                                             │
│ [A-Z] [0-9] [OPS] [DEL] [OK]  ← Encoder scrolls menu       │
│                                                             │
│ Step Buttons:                                              │
│ [1-8]: Insert preset ops/common commands                   │
│ Function: Show ops category menu                           │
│ Shift+Encoder: Character picker A-Z, 0-9                   │
│ Left/Right: Move cursor                                    │
└─────────────────────────────────────────────────────────────┘
```

**Character Entry Method 1 - Op Menu:**
```
Function → Category Menu:
[VAR] [MATH] [CTRL] [TIME] [MIDI] [CV] [TR] [PTRN]

Select MATH → Sub-menu:
[ADD] [SUB] [MUL] [DIV] [MOD] [RAND] [EQ] [GT] [LT] ...
```

**Character Entry Method 2 - T9-style:**
```
Step Buttons mapped like T9 phone:
Button 1: . , 1
Button 2: A B C 2
Button 3: D E F 3
Button 4: G H I 4
Button 5: J K L 5
Button 6: M N O 6
Button 7: P Q R S 7
Button 8: T U V 8
Shift+1: W X Y Z 9
Shift+2: 0

Encoder: Space, Backspace, Enter
```

**Character Entry Method 3 - Encoder Scroll (Simplest):**
```
Encoder scroll through alphabet/ops:
A → B → C → ... → Z → 0 → 1 → ... → 9 → ADD → SUB → ...

Left/Right: Move cursor position
Function: Insert at cursor
Page: Delete at cursor
```

#### Page 2: Variable Monitor (Live Dashboard)

```
┌─────────────────────────────────────────────────────────────┐
│ Teletype - Variables                                        │
├─────────────────────────────────────────────────────────────┤
│ A: 42      X: 127    CV.1: 5000   TR.1: ■                  │
│ B: 0       Y: -32    CV.2: 0       TR.2: □                  │
│ C: 1000    Z: 64     CV.3: 8192    TR.3: ■                  │
│ D: 5       T: 3      CV.4: 16383   TR.4: □                  │
│                                                             │
│ M: 500ms  DRUNK: 42  O: 7  Q.N: 4                          │
│                                                             │
│ [Run Cmd]  ← Encoder: Enter live command                   │
└─────────────────────────────────────────────────────────────┘
```

**Live Command Entry:**
When "Run Cmd" selected, use same editor as script editing, but execute immediately.

#### Page 3: Pattern Tracker

```
┌─────────────────────────────────────────────────────────────┐
│ Patterns  (Encoder: scroll, Shift+Enc: edit value)         │
├───────┬───────┬───────┬───────────────────────────────────┤
│  P.0  │  P.1  │  P.2  │  P.3                              │
├───────┼───────┼───────┼───────────────────────────────────┤
│ ▸ 0   │  42   │  100  │  -50                              │
│   1   │  37   │  95   │  -45                              │
│   2   │  50   │  88   │  -40                              │
│   3   │  63   │  77   │  -30                              │
│   4   │  70   │  64   │  -20                              │
│   5   │  63   │  50   │  -10                              │
│  ...  │  ...  │  ...  │  ...                              │
│                                                             │
│ Len: 16  Start: 0  End: 15  Wrap: 1                       │
└─────────────────────────────────────────────────────────────┘
```

**Navigation:**
- **Left/Right**: Select pattern column
- **Encoder**: Scroll through indices
- **Shift+Encoder**: Edit value at cursor
- **Function**: Set pattern parameters (length, wrap, etc.)

#### Page 4: Configuration

```
┌─────────────────────────────────────────────────────────────┐
│ Teletype - Configuration                                    │
├─────────────────────────────────────────────────────────────┤
│ Input Routing:                                             │
│  Script 1 ← [Performer Clock]                              │
│  Script 2 ← [Gate In 1]                                    │
│  Script 3 ← [Gate In 2]                                    │
│  Script 4 ← [Step Button]                                  │
│  Metro    ← [Internal: 500ms]                              │
│                                                             │
│ CV Input Mapping:                                          │
│  IN   ← [CV In 1]                                          │
│  PARAM ← [Step Encoder]                                    │
│                                                             │
│ [SAVE] [LOAD] [INIT]                                       │
└─────────────────────────────────────────────────────────────┘
```

### UI File Structure

```cpp
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

---

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
    };

    struct ScriptTrigger {
        InputSource source = None;
        uint8_t edgePolarity = 0;  // 0=Rise, 1=Fall, 2=Both

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
    };

    //----------------------------------------
    // Properties
    //----------------------------------------

    // Script triggers (8 scripts)
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

    // Direct access to Teletype state
    // (Engine layer will manage the actual scene_state_t)
    // Model layer stores serializable representation

    void clear();
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    ScriptTrigger _scriptTriggers[8];
    CvInputSource _cvInputSources[2];  // IN, PARAM

    int _metroInterval = 500;  // milliseconds
    bool _metroEnabled = true;

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

---

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

    // Input edge detection
    bool _lastGateInputs[8] = {false};

    bool _activity = false;
};
```

### TeletypeTrackEngine.cpp (key methods)

```cpp
// src/apps/sequencer/engine/TeletypeTrackEngine.cpp

#include "TeletypeTrackEngine.h"
#include "TeletypeHardwareAdapter.h"
#include "Engine.h"

TeletypeTrackEngine::TeletypeTrackEngine(Engine &engine, const Model &model,
                                         Track &track, const TrackEngine *linkedTrackEngine)
    : TrackEngine(engine, model, track, linkedTrackEngine)
{
    // Initialize Teletype state
    ss_init(&_sceneState);
    es_init(&_execState);
    cs_init(&_commandState);

    // Set hardware adapter to use this engine
    TeletypeHardwareAdapter::setEngine(this);

    // Load scripts from model
    loadScriptsFromModel();

    // Run Init script
    runScript(INIT_SCRIPT);
}

void TeletypeTrackEngine::tick(uint32_t tick) {
    _activity = false;

    // Check input triggers and run associated scripts
    checkInputTriggers(tick);

    return _activity ? CvUpdate | GateUpdate : NoUpdate;
}

void TeletypeTrackEngine::update(float dt) {
    // System tick (10ms for delays)
    _systemTickAccumulator += dt;
    while (_systemTickAccumulator >= 0.010f) {
        tele_tick(&_sceneState, 10);
        _systemTickAccumulator -= 0.010f;
    }

    // Metro timer
    if (_track.teletypeTrack().metroEnabled()) {
        updateMetro(dt);
    }

    // CV slewing
    updateCvSlewing(dt);
}

void TeletypeTrackEngine::checkInputTriggers(uint32_t tick) {
    const auto &tt = _track.teletypeTrack();

    for (int i = 0; i < 8; ++i) {
        const auto &trigger = tt.scriptTrigger(i);
        if (trigger.source == TeletypeTrack::None) continue;

        bool currentState = false;

        switch (trigger.source) {
        case TeletypeTrack::PerformerClock:
            // Trigger on every tick
            currentState = true;
            break;

        case TeletypeTrack::GateIn1:
        case TeletypeTrack::GateIn2:
        case TeletypeTrack::GateIn3:
        case TeletypeTrack::GateIn4: {
            int gateIndex = trigger.source - TeletypeTrack::GateIn1;
            currentState = getGateInputInternal(gateIndex);
            break;
        }

        // ... other sources
        }

        // Edge detection
        bool rising = currentState && !_lastGateInputs[i];
        bool falling = !currentState && _lastGateInputs[i];
        _lastGateInputs[i] = currentState;

        bool shouldTrigger = false;
        if (trigger.edgePolarity == 0 && rising) shouldTrigger = true;
        if (trigger.edgePolarity == 1 && falling) shouldTrigger = true;
        if (trigger.edgePolarity == 2 && (rising || falling)) shouldTrigger = true;

        if (shouldTrigger) {
            runScript(i);  // Run script 0-7
        }
    }
}

void TeletypeTrackEngine::runScript(uint8_t scriptIndex) {
    if (scriptIndex >= TOTAL_SCRIPT_COUNT) return;

    process_result_t result = run_script(&_sceneState, scriptIndex);

    if (result.has_value) {
        // Script returned a value - could be used for something
    }

    _activity = true;
}

void TeletypeTrackEngine::updateMetro(float dt) {
    _metroAccumulator += dt;
    float intervalSec = _track.teletypeTrack().metroInterval() / 1000.0f;

    if (_metroAccumulator >= intervalSec) {
        _metroAccumulator -= intervalSec;
        runScript(METRO_SCRIPT);
    }
}

void TeletypeTrackEngine::updateCvSlewing(float dt) {
    for (int i = 0; i < 4; ++i) {
        if (_cvSlewRates[i] > 0.0f) {
            float delta = (_cvTargets[i] - _cvOutputs[i]);
            float step = _cvSlewRates[i] * dt;

            if (std::abs(delta) < step) {
                _cvOutputs[i] = _cvTargets[i];
            } else {
                _cvOutputs[i] += (delta > 0 ? step : -step);
            }
        } else {
            _cvOutputs[i] = _cvTargets[i];
        }
    }
}

// Hardware adapter callbacks
void TeletypeTrackEngine::setCvOutputInternal(uint8_t channel, float value, bool slew) {
    if (channel >= 4) return;

    _cvTargets[channel] = value;

    if (slew) {
        // Use scene_state CV slew rate
        int16_t slewMs = _sceneState.variables.cv_slew[channel];
        _cvSlewRates[channel] = slewMs > 0 ? (1.0f / (slewMs / 1000.0f)) : 0.0f;
    } else {
        _cvOutputs[channel] = value;
        _cvSlewRates[channel] = 0.0f;
    }
}

void TeletypeTrackEngine::setGateOutputInternal(uint8_t channel, bool state) {
    if (channel >= 4) return;
    _gateOutputs[channel] = state;
}

int16_t TeletypeTrackEngine::getCvInputInternal(uint8_t channel) {
    if (channel >= 2) return 0;

    auto source = _track.teletypeTrack().cvInputSource(channel);

    switch (source) {
    case TeletypeTrack::CvIn1:
    case TeletypeTrack::CvIn2:
    case TeletypeTrack::CvIn3:
    case TeletypeTrack::CvIn4: {
        int cvIndex = source - TeletypeTrack::CvIn1;
        // Read from Performer's routing engine
        float cv = 0.0f;  // TODO: Get from routing
        // Convert -1..1 to Teletype's 0..16383
        return static_cast<int16_t>((cv + 1.0f) * 0.5f * 16383.0f);
    }

    case TeletypeTrack::ProjectTempo: {
        float bpm = _engine.clock().bpm();
        // Map 60-240 BPM to 0-16383
        return static_cast<int16_t>(clamp((bpm - 60.0f) / 180.0f, 0.0f, 1.0f) * 16383.0f);
    }

    // ... other sources
    }

    return 0;
}

bool TeletypeTrackEngine::gateOutput(int index) const {
    return index < 4 ? _gateOutputs[index] : false;
}

float TeletypeTrackEngine::cvOutput(int index) const {
    return index < 4 ? _cvOutputs[index] : 0.0f;
}
```

---

## Hardware Adapter Implementation

### TeletypeHardwareAdapter.h/cpp

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

int16_t TeletypeHardwareAdapter::getCvInput(uint8_t channel) {
    if (!s_engine) return 0;
    return s_engine->getCvInputInternal(channel);
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

int16_t tele_get_input_state(uint8_t input) {
    return TeletypeHardwareAdapter::getTrInput(input) ? 1 : 0;
}
```

---

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

---

## Serialization

### TeletypeTrack serialization

```cpp
void TeletypeTrack::write(VersionedSerializedWriter &writer) const {
    // Write script triggers
    for (int i = 0; i < 8; ++i) {
        _scriptTriggers[i].write(writer);
    }

    // Write CV input sources
    for (int i = 0; i < 2; ++i) {
        writer.write(static_cast<uint8_t>(_cvInputSources[i]));
    }

    // Write metro config
    writer.write(_metroInterval);
    writer.write(_metroEnabled);

    // Write scripts as text
    for (int s = 0; s < 11; ++s) {
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
    // Mirror of write()
    // ...
}
```

---

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
}

CASE("script_trigger_mapping") {
    TeletypeTrack tt;
    TeletypeTrack::ScriptTrigger trig;
    trig.source = TeletypeTrack::PerformerClock;
    trig.edgePolarity = 0;

    tt.setScriptTrigger(0, trig);
    expectEqual(static_cast<int>(tt.scriptTrigger(0).source),
                static_cast<int>(TeletypeTrack::PerformerClock),
                "trigger source set");
}

} // UNIT_TEST
```

### Integration Tests

Create Teletype scripts and verify outputs:

```cpp
// TestTeletypeEngine.cpp
CASE("cv_output") {
    // Script 1: CV 1 5000
    TeletypeTrackEngine engine(...);
    engine.runScript(0);

    float cv = engine.cvOutput(0);
    // 5000/16383 * 2 - 1 = -0.389
    expectTrue(std::abs(cv - (-0.389f)) < 0.01f, "CV output correct");
}
```

---

## Performance Considerations

### Memory Footprint

**Teletype core state:**
- `scene_state_t`: ~10-15 KB (vars, patterns, scripts, delays)
- Per-track overhead: ~15 KB

**Mitigation**:
- Only allocate for tracks using Teletype mode
- Shared op table (global, not per-track)

### CPU Usage

**Interpreter overhead:**
- Script execution: ~100-500 μs per script (depends on complexity)
- Safe for `Engine::update()` (4096 stack, priority 4)

**Profiling targets:**
- Ensure `TeletypeTrackEngine::update()` < 1ms on STM32
- Test complex scripts with loops

### Code Size

**Teletype ops library:** ~50-80 KB of code

**Mitigation**:
- Compile with `-Os` for size
- Strip unused ops at link time if possible

---

## Independent Script File System

### Design Goals

1. **Project Independence**: Teletype scenes stored separately from Performer projects
2. **Shareability**: Copy script files between devices via SD card
3. **Version Control**: Human-readable text format for git/diff
4. **UI Constraints**: Text entry without keyboard on 256x64 OLED
5. **Performance**: Fast load/save without blocking audio engine

### File Format

#### File Type: `.tele` (Teletype Scene)

**Text-based format** for maximum portability:

```
# Teletype Scene File v1.0
# Name: MyScene
# Created: 2025-12-31
# Description: Generative arpeggio with probability

[SCRIPTS]
# Script 1 (6 lines max)
CV 1 N RRAND 0 12
TR.P 1
DEL 100: CV 1 N 0
DEL 200: TR 1 0

# Script 2
IF GT RRAND 0 100 50: $1

# Script 3-8
<empty>

# Metro Script
X ADD X 1
IF GT X 64: X 0
CV 2 * X 100

# Init Script
M 500
M.ACT 1
X 0

[PATTERNS]
# Pattern 0 (64 values)
P0: 0 2 4 7 9 11 12 14 ...
# Pattern 1-3
P1: <empty>

[VARIABLES]
# Variables A-Z (if non-zero)
A=0 B=0 C=0 D=0 E=0 ...

[CONFIG]
MetroMode=Realtime
MetroPeriod=500
Octave=0
Transpose=0
Divisor=12
UseProjectScale=true
CvUpdateMode=Gate

[OUTPUT_ROUTES]
# CV 1-20, TR 1-20 routing
# Format: CV<n>=Track<t>:Channel<c>
CV5=1:0  # CV 5 routes to Track 1, Channel 0
TR5=1:0  # TR 5 routes to Track 1, Gate 0
```

**Binary alternative** (for faster load, smaller size):

```cpp
struct TeletypeSceneFile {
    FileHeader header;  // 8-byte name, type, version

    // Scripts (11 scripts x 6 lines x 64 chars = 4224 bytes)
    char scripts[11][6][64];

    // Patterns (4 patterns x 64 values x 2 bytes = 512 bytes)
    int16_t patterns[4][64];

    // Variables A-Z (26 x 2 bytes = 52 bytes)
    int16_t variables[26];

    // Config (20 bytes)
    uint8_t metroMode;
    uint16_t metroPeriod;
    int8_t octave;
    int8_t transpose;
    uint16_t divisor;
    uint8_t useProjectScale;
    uint8_t cvUpdateMode;
    uint8_t reserved[10];

    // Output routes (40 routes x 3 bytes = 120 bytes)
    struct OutputRoute {
        uint8_t targetTrack;
        uint8_t targetChannel;
        uint8_t enabled;
    } cvRoutes[20], trRoutes[20];

    // Total: ~4928 bytes per scene
} __attribute__((packed));
```

**Recommendation**: **Text format** for MVP
- Easier debugging
- Git-friendly
- Human editable
- Binary can be added later for performance

#### File Organization

```
/TELE/           (SD card root)
  scenes/
    default.tele
    arp01.tele
    bass.tele
    drums.tele
    ...
  templates/
    empty.tele
    4step.tele
    8step.tele
```

### UI Design (256x64 OLED)

#### File Browser Page

**Layout:**

```
┌────────────────────────────────────────────────────────┐  64px
│ TELE SCENES  [Load] [Save] [New]                    ↑ │ ← 12px header
├────────────────────────────────────────────────────────┤
│ > arp01.tele          2.1KB  Dec 31              [*]  │ ← 10px row
│   bass.tele           1.8KB  Dec 30                   │ ← 10px row
│   default.tele        2.4KB  Dec 28              [*]  │ ← 10px row
│   drums.tele          1.5KB  Dec 27                   │ ← 10px row
├────────────────────────────────────────────────────────┤
│ F1:Load  F2:Save  F3:New  F4:Del     1/4            ↓ │ ← 12px footer
└────────────────────────────────────────────────────────┘

Legend:
  > = selected file
  [*] = file has unsaved changes (dirty bit)
  ↑↓ = scrollbar (more files above/below)
  F1-F4 = function button actions
```

**Display Constraints:**
- Header: 12px (title + context buttons)
- Rows: 4 rows x 10px = 40px (ListPage standard)
- Footer: 12px (function labels + page indicator)
- Total: 64px

**Per-row content:**
- Filename: 16 chars max (`arp01.tele`)
- File size: 6 chars (`2.1KB`)
- Date: 6 chars (`Dec 31`)
- Dirty indicator: `[*]` or blank

#### Load Scene Dialog

```
┌────────────────────────────────────────────────────────┐
│ LOAD SCENE: arp01.tele                                 │
├────────────────────────────────────────────────────────┤
│ Current scene has unsaved changes!                     │
│                                                         │
│   [Cancel]    [Discard & Load]    [Save & Load]        │
│                                                         │
├────────────────────────────────────────────────────────┤
│ Encoder: Select   Shift+Encoder: Cancel                │
└────────────────────────────────────────────────────────┘
```

#### Save Scene Dialog

**New file:**

```
┌────────────────────────────────────────────────────────┐
│ SAVE SCENE AS:                                         │
├────────────────────────────────────────────────────────┤
│ Filename: [arp01_______]     (.tele)                   │
│                                                         │
│ Entry Mode: [T9]  (Step buttons 1-8)                   │
│   1:     2:ABC  3:DEF  4:GHI  5:JKL  6:MNO  7:PQRS     │
│   8:TUV  Fn:Space  Shift:Backspace                     │
│                                                         │
├────────────────────────────────────────────────────────┤
│ F1:Save  F2:Cancel  F3:Mode(T9/Enc)  Enc:Cursor        │
└────────────────────────────────────────────────────────┘
```

**Overwrite existing:**

```
┌────────────────────────────────────────────────────────┐
│ OVERWRITE: arp01.tele?                                 │
├────────────────────────────────────────────────────────┤
│ File already exists (2.1KB, modified Dec 31)           │
│                                                         │
│   [Cancel]         [Overwrite]                         │
│                                                         │
├────────────────────────────────────────────────────────┤
│ Encoder: Select   Shift+Encoder: Cancel                │
└────────────────────────────────────────────────────────┘
```

#### Text Entry Methods

**Method 1: T9 (Recommended for short names)**

```
Step Buttons = Phone Keypad Layout:
  [1]      [2]ABC   [3]DEF   [4]GHI
  [5]JKL   [6]MNO   [7]PQRS  [8]TUV

Press multiple times to cycle:
  2 → A → B → C → 2
  7 → P → Q → R → S → 7

Special:
  Function: Space
  Shift: Backspace
  Encoder: Move cursor left/right
```

**Method 2: Encoder Scroll (Alternative)**

```
Encoder: Scroll through alphabet + numbers + symbols
  A B C D E F ... Z 0 1 2 ... 9 _ - <space> <backspace>

Step buttons: Insert character at cursor, advance
Shift+Encoder: Fast scroll (jump 5 chars)
```

**Method 3: Quick Templates (Fastest for common names)**

```
┌────────────────────────────────────────────────────────┐
│ SAVE SCENE - QUICK NAME                                │
├────────────────────────────────────────────────────────┤
│ Select template:                                       │
│   > scene01  scene02  scene03  scene04                 │
│     seq01    seq02    arp01    bass01                  │
│     lead01   pad01    fx01     drum01                  │
│                                                         │
│ Or: [Custom Name]                                      │
├────────────────────────────────────────────────────────┤
│ Encoder: Select   Fn: Custom   Shift: Cancel           │
└────────────────────────────────────────────────────────┘
```

Auto-increment: `arp01` → `arp02` → `arp03`

### File Operations Integration

#### FileManager Extension

```cpp
// FileDefs.h
enum class FileType : uint8_t {
    Project     = 0,
    UserScale   = 1,
    TeletypeScene = 2,  // NEW
    Settings    = 255
};

// FileManager.h
class FileManager {
public:
    // NEW methods
    static fs::Error writeTeletypeScene(const TeletypeScene &scene, const char *filename);
    static fs::Error readTeletypeScene(TeletypeScene &scene, const char *filename);

    static fs::Error writeTeletypeScene(const TeletypeScene &scene, int slot);
    static fs::Error readTeletypeScene(TeletypeScene &scene, int slot);

    // List available scene files
    struct SceneInfo {
        char filename[32];
        uint32_t size;
        uint32_t modifiedTime;
        bool dirty;  // Has unsaved changes
    };
    static int listTeletypeScenes(SceneInfo *infos, int maxCount);
};
```

#### TeletypeScene Model

```cpp
// TeletypeScene.h
class TeletypeScene {
public:
    static constexpr int MAX_FILENAME_LEN = 16;

    // Metadata
    char filename[MAX_FILENAME_LEN];
    bool dirty = false;  // Has unsaved changes since last load/save

    // Scene data (wraps scene_state_t)
    scene_state_t state;

    // Performer-specific config
    TeletypeTrack::MetroMode metroMode;
    uint16_t metroPeriod;
    int8_t octave;
    int8_t transpose;
    uint16_t divisor;
    bool useProjectScale;
    TeletypeTrack::CvUpdateMode cvUpdateMode;

    // Output routing
    OutputRoute cvRoutes[20];
    OutputRoute trRoutes[20];

    // Serialization
    fs::Error writeToFile(const char *path) const;
    fs::Error readFromFile(const char *path);

    void clear();
    void markDirty() { dirty = true; }
    void markClean() { dirty = false; }
};
```

#### UI Page Implementation

```cpp
// TeletypeSceneBrowserPage.h
class TeletypeSceneBrowserPage : public ListPage {
public:
    TeletypeSceneBrowserPage(PageManager &manager, PageContext &context);

    virtual void enter() override;
    virtual void exit() override;

    virtual void draw(Canvas &canvas) override;
    virtual void keyPress(KeyPressEvent &event) override;

private:
    void loadScene();
    void saveScene();
    void newScene();
    void deleteScene();

    void loadSceneFromFile(const char *filename);
    void saveSceneToFile(const char *filename);

    void showSaveDialog();
    void showLoadConfirm();
    void showOverwriteConfirm();

    TeletypeSceneListModel _listModel;
    TeletypeScene *_currentScene;  // Points to track's active scene

    enum DialogState {
        None,
        SaveDialog,
        LoadConfirm,
        OverwriteConfirm,
    };
    DialogState _dialogState = None;

    // Text entry state
    char _filenameBuffer[TeletypeScene::MAX_FILENAME_LEN];
    int _cursorPos = 0;
    enum TextEntryMode {
        T9,
        EncoderScroll,
        QuickTemplate
    };
    TextEntryMode _textEntryMode = T9;
    int _t9PressCount = 0;  // For multi-tap
    uint32_t _t9LastPressTicks = 0;
};
```

### Resource Cost Analysis

#### Storage (SD Card)

**Per-scene file size:**

| Format | Size | Notes |
|--------|------|-------|
| Text (.tele) | ~3-6 KB | Human-readable, compressible |
| Binary (.tele) | ~5 KB | Fixed size, faster parsing |

**Typical usage:**
- 20 scenes = 60-120 KB (text) or 100 KB (binary)
- SD card capacity: 16GB → can store 160,000+ scenes
- **Impact**: Negligible

#### Memory (RAM)

**Runtime overhead:**

| Component | Size | Lifecycle |
|-----------|------|-----------|
| `TeletypeScene` struct | ~5-6 KB | Persistent (per track) |
| File buffer (load/save) | 512 bytes | Temporary (file I/O only) |
| Scene list cache | ~2 KB | UI only (20 SceneInfo entries) |
| Text entry buffer | 32 bytes | Dialog only |

**Total active overhead**: ~5-6 KB per Teletype track
- **Impact**: Moderate (scene_state_t already ~10-15 KB)

**Mitigation**:
- Limit to 1-2 Teletype tracks per project (already planned)
- Reuse file buffer across operations
- Clear scene list cache when not in browser

#### Performance (CPU Time)

**File I/O timing estimates (text format):**

| Operation | Time (STM32) | Notes |
|-----------|-------------|-------|
| Parse text file | 50-100ms | Per 5KB file |
| Write text file | 30-60ms | Sequential write |
| SD card seek | 10-50ms | Variable (file fragmentation) |
| Directory listing | 20-100ms | Per 20 files |

**Binary format** (if needed):
- Load: 10-20ms (direct memcpy)
- Save: 10-20ms (direct write)

**Impact on audio**:
- File operations run on **FileTask** (priority 1, separate thread)
- Engine continues on **EngineTask** (priority 4)
- **No audio dropouts** if properly suspended

**Critical**: Must use `Engine::suspend()` during file I/O:

```cpp
void TeletypeSceneBrowserPage::loadSceneFromFile(const char *filename) {
    // Suspend engine to prevent conflicts with scene_state_t
    _model.engine().suspend();

    FileManager::task(
        [filename, this]() -> fs::Error {
            return _currentScene->readFromFile(filename);
        },
        [this](fs::Error result) {
            _model.engine().resume();
            if (result == fs::OK) {
                _currentScene->markClean();
                showMessage("Loaded OK");
            } else {
                showError("Load failed");
            }
        }
    );
}
```

#### OLED Drawing Cost

**File browser page:**

| Element | Pixels | Complexity | Time (est.) |
|---------|--------|------------|-------------|
| Header text | 256x12 | Low | 1-2ms |
| 4 file rows | 256x40 | Low (text) | 2-4ms |
| Footer text | 256x12 | Low | 1-2ms |
| Scrollbar | 4x40 | Minimal | <1ms |

**Total redraw**: ~5-10ms per frame

**Optimization**:
- Use dirty flags (only redraw on change)
- Limit refresh rate to 10-20 FPS in file browser (vs 50 FPS in sequencer)
- **Impact**: Minimal (UI task priority 2, below engine)

**Noise reduction consideration**:
- File browser is OLED-heavy (lots of text)
- Triggers screensaver faster (per noise reduction settings)
- Use dimmed brightness in browser (user configurable)

#### Worst-Case Scenario

**Simultaneous heavy load:**
- User in file browser (OLED active)
- Loading large scene (5KB text parse)
- 2 Teletype tracks running complex scripts

**Resource usage peak:**
- **RAM**: 10KB (scene) + 512B (file buf) + 2KB (list) + 15KB (2nd track) = ~28KB
- **CPU**: FileTask parsing (100ms burst) + UI redraw (10ms) + Engine update (1ms/tick)
- **OLED**: High pixel count (text-heavy browser)

**Mitigation**:
- Engine suspended during file load (no script execution)
- File I/O is async (doesn't block UI)
- OLED noise reduction kicks in (dimming + screensaver)

**Conclusion**: Within STM32F4 capabilities (192KB RAM, 168MHz CPU)

### Auto-Save & Dirty Tracking

**Dirty bit logic:**

```cpp
// TeletypeTrackEngine.cpp
void TeletypeTrackEngine::runScript(int scriptIndex) {
    // ... execute script ...

    // If script modified patterns, variables, or other state
    _track.teletypeTrack().scene().markDirty();
}

// UI indicator
void TeletypeSceneBrowserPage::draw(Canvas &canvas) {
    for (int i = 0; i < visibleRows; ++i) {
        // ... draw filename, size, date ...

        if (sceneInfo.dirty) {
            canvas.drawText(240, y, "[*]");  // Unsaved changes indicator
        }
    }
}
```

**Auto-save options** (future enhancement):

```
Settings → Teletype → Auto-Save:
  - Off
  - On scene change (when switching tracks)
  - On project save
  - Periodic (every 5 minutes)
```

### File Format Version Migration

**Version header:**

```
# Teletype Scene File v1.0
```

**Forward compatibility:**

```cpp
fs::Error TeletypeScene::readFromFile(const char *path) {
    // Parse version line
    int majorVersion, minorVersion;
    parseVersion(line, majorVersion, minorVersion);

    if (majorVersion > 1) {
        return fs::UNSUPPORTED_VERSION;
    }

    // Handle minor version differences gracefully
    if (minorVersion > 0) {
        // Load new fields if present, use defaults if missing
    }

    // Parse rest of file...
}
```

### Summary: Resource Budget

| Resource | Per Scene | Per Track | System Impact |
|----------|-----------|-----------|---------------|
| **Storage** | 3-6 KB | N/A | Negligible (SD card >> GB) |
| **RAM** | 5-6 KB | 15-20 KB total | Moderate (limit 1-2 tracks) |
| **Load time** | 50-100ms | N/A | Low (async, engine suspended) |
| **Save time** | 30-60ms | N/A | Low (async, no audio impact) |
| **OLED cost** | 5-10ms/frame | N/A | Low (browser only, <50 FPS) |

**Verdict**: Feasible within STM32F4 constraints. Text format acceptable for MVP; binary format can be added if load times become an issue.

---

### Phase 1: Core Integration (Foundation)
**Goal**: Get interpreter running in Performer context

1. Port Teletype core files to `src/apps/sequencer/engine/teletype/`
2. Create `TeletypeTrack` model (minimal, no UI)
3. Create `TeletypeTrackEngine` with stub I/O
4. Implement `TeletypeHardwareAdapter`
5. Register in `TrackEngine` factory
6. Write unit tests for model
7. **Milestone**: Can create Teletype track, run hardcoded script

### Phase 2: I/O Integration (Basic)
**Goal**: Get CV/Gate outputs working for local track

1. Implement CV output with slewing in `TeletypeTrackEngine::update()`
2. Implement Gate output
3. Wire CV 1-4 / TR 1-4 to Teletype track's own outputs (direct)
4. Test with simple scripts: `CV 1 N 60`, `TR.PULSE 1`
5. **Milestone**: Teletype track produces CV/Gate on its own outputs

### Phase 2b: Extended Output Routing (Advanced)
**Goal**: Enable cross-track CV/TR control (CV 5-20, TR 5-20)

1. Implement `OutputRoute` configuration in `TeletypeTrack`
2. Extend adapter to route outputs based on routing table
3. Decide: Direct injection vs Virtual Sources approach
4. Implement cross-track CV/TR write methods
5. Add Output Routing UI page
6. Test: Teletype script controlling multiple tracks
7. **Milestone**: Single Teletype track orchestrates all 8 tracks

### Phase 3: Input Routing
**Goal**: Trigger scripts from Performer events

1. Implement `checkInputTriggers()` in engine
2. Add input mapping config to `TeletypeTrack` model
3. Test script triggering from clock, gates
4. Implement CV inputs (IN, PARAM) with source mapping
5. **Milestone**: Scripts run on clock/gate events, read CV inputs

### Phase 4: Basic UI
**Goal**: Minimal script editing

1. Create `TeletypeTrackPage` with script viewer
2. Implement encoder-based line navigation
3. Implement simple character entry (encoder scroll alphabet)
4. Parse and execute on save
5. **Milestone**: Can edit and run scripts from UI

### Phase 5: Advanced UI
**Goal**: Full editing experience

1. Implement op menu system (categories)
2. Add pattern tracker page
3. Add variable monitor page
4. Add configuration page (input routing)
5. Implement T9/preset button shortcuts
6. **Milestone**: Full-featured script editor

### Phase 6: Polish & Optimization
**Goal**: Production ready

1. Performance profiling on STM32
2. Optimize hot paths (script execution, slewing)
3. Add help text / op reference
4. Write integration tests
5. Documentation
6. **Milestone**: Ready for release

---

## Known Limitations

1. **No I2C modules**: All Monome ecosystem ops unavailable
2. **No Grid**: Grid control ops unavailable
3. **No Live Mode REPL**: Replaced with slower encoder-based entry
4. **Limited screen space**: Teletype uses 128x64, Performer's is 256x64 but shared with track info
5. **No keyboard shortcuts**: All editing via encoder/buttons
6. **Timing precision**: 10ms tick vs native timing (should be fine for most use cases)

---

## Research Summary: INIT/METRO Scripts and Extended I/O

### Key Findings from Teletype Source Analysis

#### 1. Script System Architecture

**11 Total Scripts:**
- Scripts 0-7: User-triggerable (TR inputs with edge polarity)
- Script 8 (METRO): Timer-driven periodic execution
- Script 9 (INIT): Runs on load/reset/manual trigger
- Script 10 (DELAY): Internal storage for `DEL` commands
- Script 11 (LIVE): REPL-entered commands

**Edge Detection Polarity:**
```cpp
script_pol[i] = 0  // Disabled
script_pol[i] = 1  // Rising edge (0→1)
script_pol[i] = 2  // Falling edge (1→0)
script_pol[i] = 3  // Both edges
```

This fine-grained control allows scripts to respond to specific trigger events.

#### 2. INIT Script Execution Points

**Runs automatically:**
1. Scene load from flash (project load)
2. Scene reset (user action)

**Runs manually:**
- `INIT.SCRIPT` op

**Typical uses:**
- Initialize variables
- Set metro interval
- Configure CV slew rates
- Set script polarities
- One-time setup code

**Implementation note:** In Performer, INIT should run:
- Track creation
- Track mode change to Teletype
- Pattern/project load
- User manual trigger (UI button or script op)

#### 3. METRO Script Timer Mechanics

**Implementation:**
- Software timer callback (`metroTimer`)
- Interval: `M` variable (milliseconds, min 25ms)
- Enable/disable: `M.ACT` variable (bool)
- Reset: `M.RESET` op

**Typical uses:**
- Clock divider for periodic events
- LFO-style modulation
- Step sequencer advance
- Periodic randomization

**Performer integration:**
```cpp
// Option A: Real-time timer (independent of transport)
void TeletypeTrackEngine::update(float dt) {
    if (_track.teletypeTrack().metroEnabled()) {
        _metroAccumulator += dt;
        float intervalSec = _sceneState.variables.m / 1000.0f;
        if (_metroAccumulator >= intervalSec) {
            _metroAccumulator -= intervalSec;
            runScript(METRO_SCRIPT);
        }
    }
}

// Option B: Transport-synced (tempo-dependent)
void TeletypeTrackEngine::tick(uint32_t tick) {
    uint32_t metroDivisor = msToTicks(_sceneState.variables.m);
    if (tick % metroDivisor == 0) {
        runScript(METRO_SCRIPT);
    }
}
```

**Recommendation:** Offer both modes as user-configurable per track.

#### 4. Extended Output Addressing (THE GAME CHANGER)

**Original Teletype Hardware:**
- CV 1-4: Local DAC outputs
- CV 5-20: I2C to 4× Ansible modules (16 additional outputs)
- TR 1-4: Local gate outputs
- TR 5-20: I2C to Ansible (16 additional gates)

**Total addressable:** 20 CV + 20 TR outputs from a single script!

**Code evidence:**
```cpp
// teletype/src/ops/hardware.c op_CV_set()
if (a < 4) {
    // Local hardware
    tele_cv(a, value, 1);
}
else if (a < 20) {
    // I2C to Ansible expansion modules
    tele_ii_tx(ansible_addr, data, 4);
}
```

**Performer Opportunity:**

Since Performer has 8 tracks with 8 CV outs each (64 total), we can:

1. **Map CV 1-20 to ANY track's outputs:**
   - CV 1 → Track 1, Output 0
   - CV 2 → Track 1, Output 1
   - CV 3 → Track 2, Output 0  (cross-track!)
   - CV 4 → Track 3, Output 2  (cross-track!)
   - ...

2. **Create a Master Conductor Track:**
   ```
   # Script 1: Orchestrate entire rig
   CV 1 V 0     # Track 1 plays C
   TR.P 1       # Trigger Track 1

   CV 3 V 7     # Track 2 plays G
   TR.P 2       # Trigger Track 2

   CV 5 RRAND 0 16383  # Track 3 modulation
   CV 6 LFO value      # Track 4 modulation
   ```

3. **Routing via Performer's System:**
   - **Option A:** Direct injection into track output buffers
   - **Option B:** Register as virtual routing sources (cleaner)
   - **Recommended:** Option B for consistency with Performer architecture

**UI Requirements:**
- Output Routing Configuration Page
- Map CV/TR 1-20 to Track × Channel
- Enable/disable individual routes
- Visual indication of cross-track routing

#### 5. Implementation Priorities

**Phase 1-2: Core + Basic I/O**
- Get scripts running
- CV/TR 1-4 working on local track
- INIT and METRO scripts functional

**Phase 2b: Extended Routing (CRITICAL)**
- Implement OutputRoute configuration
- Wire CV 5-20 / TR 5-20 to other tracks
- Add routing UI
- **This is what makes Teletype track uniquely powerful in Performer**

**Phase 3+: Polish**
- Input routing
- UI enhancements
- Performance optimization

### Architectural Implications

**Cross-track communication:**

Teletype track breaks the normal track isolation model. It can:
- Read any CV/Gate (via routing sources)
- Write to any track's outputs (via extended routing)

This requires:
1. Engine-level cross-track access methods
2. Routing system integration
3. Careful handling of output conflicts (what if Track 2 AND Teletype both set Track 3's CV?)

**Conflict resolution strategy:**
- **Additive:** Sum Teletype output + track output (requires float buffers)
- **Override:** Teletype takes precedence (simple but destructive)
- **Multiplexed:** User chooses per-output (complex UI)

**Recommended:** Override mode with visual warning in UI when conflict detected.

---

## Summary: Native Performer Integration Benefits

### What Makes Teletype Track Feel Native

The integration goes beyond simple porting - Teletype becomes a **first-class Performer citizen**:

**1. Standard Track Properties:**
- **Octave** (-10 to +10): Shift all note ops globally
- **Transpose** (-100 to +100): Semitone transposition with routing support
- **Divisor** (1-192): Sync metro to transport instead of realtime ms
- **CV Update Mode** (Gate/Always): Control when CV updates (classic vs free-running)
- **Routable**: All params support Performer's routing system

**2. Project Scale Integration:**
- Teletype's `N` op uses Performer's project scale
- Scripts automatically quantize to current scale/mode
- Octave + Transpose applied before quantization
- Toggle between Project Scale and Teletype's 12TET

**3. Metro Modes:**
- **Realtime**: Original Teletype behavior (milliseconds, independent)
- **Synced**: Transport-locked (divisor-based, tempo-aware)
- Switch between modes without changing scripts

**4. Cross-Track Orchestration:**
- CV 1-20 / TR 1-20 routing to ANY track
- Single Teletype track controls all 8 tracks
- Conductor/orchestrator role

**5. Consistent UX:**
- Same routing paradigm as other tracks
- Same UI patterns (encoder, buttons, context menus)
- Same serialization (project files, patterns)

### Practical Examples

**Example 1: Generative Melody with Project Scale**
```
# Setup: Project scale = C Minor, Teletype Octave = +1

# Script 1: Random melody
CV 1 N RRAND 0 7    # Random note 0-7, quantized to C Minor
TR.P 1               # Trigger
X RRAND 0 100        # Random delay
DEL X: CV 1 N RRAND 0 7
```

Result: Plays in C Minor (Cm, D, Eb, F, G, Ab, Bb) at octave +1.

**Example 2: Tempo-Synced LFO**
```
# Setup: Metro Mode = Synced, Divisor = 48 (1/4 note)

# Script M: Triangle LFO
X ADD X 1
IF GT X 127: X SUB 256 X  # Triangle wave -127 to +127
CV 2 SCALE X -127 127 0 16383
```

Result: Triangle wave synced to quarter notes, adapts to tempo changes.

**Example 3: Multi-Track Conductor**
```
# Setup: CV 1-2 → Track 1, CV 3-4 → Track 2, CV 5-6 → Track 3

# Script 1: Orchestrate 3 tracks
CV 1 N 0     # Track 1: Root
CV 3 N 4     # Track 2: Third
CV 5 N 7     # Track 3: Fifth
TR.P 1; TR.P 2; TR.P 3  # Trigger all

# Chord changes on Script 2
A ADD A 1    # Step through progression
CV 1 N * A 3 # Track 1: I, iii, V...
```

Result: 3-voice harmony with chord progression.

---

## Conclusion (Updated)

Integrating Teletype into Performer as a track type is **architecturally sound** and **highly feasible**, with research revealing **multiple killer features**:

### 1. Extended Output Routing
**CV 1-20 / TR 1-20 allows a single Teletype track to function as a master conductor**, orchestrating all 8 Performer tracks simultaneously. This transforms Performer from an 8-track sequencer into an 8-track sequencer with a programmable brain.

### 2. Native Performer Integration
**Teletype behaves like any other Performer track**, with:
- Octave, Transpose, Divisor (routable)
- Project scale integration (automatic quantization)
- Transport-synced metro (tempo-aware)
- Gate vs Always CV update modes
- Consistent UI/UX with other track types

### 3. Best of Both Worlds
- **Teletype's power**: Scripting, variables, patterns, control flow, delays
- **Performer's ecosystem**: Scales, routing, multi-track, transport, clock

The key architectural separations:
- **Core logic** (interpreter, ops): Port unchanged, extend for Performer
- **I/O layer**: Abstracted via adapter + bridge patterns
- **UI layer**: Reimplemented for button/encoder paradigm
- **Model layer**: Serialize scripts as text with Performer-native properties

The resulting implementation provides **~80% of Teletype's functionality** (minus I2C/Grid) while enabling:
- Unprecedented cross-track control
- Scripted sequencing with project-wide consistency
- Musical integration (scales, tempo-sync)
- Native Performer workflow

**Estimated effort (revised):**
- Phase 1: Core Integration (2-3 weeks)
- Phase 2: Basic I/O (1 week)
- Phase 2b: Extended Routing (1-2 weeks) ← **Conductor feature!**
- Phase 2c: Performer Integration (1 week) ← **Octave/Transpose/Scale!**
- Phase 3: Input Routing (1 week)
- Phase 4-5: UI (3-4 weeks)
- Phase 6: Polish (1-2 weeks)

**Total**: ~8-12 weeks for complete implementation including conductor capability and native Performer integration.

**Priority recommendation**: Implement Phase 2c (Performer Integration) early, as it defines the user experience and makes Teletype feel like it belongs in Performer rather than being a foreign module.

---

## Test Script Sets

### Test Set 1: Basic I/O and Core Operations

**Purpose**: Verify core Teletype functionality, CV/Gate output, basic ops

**Setup:**
- Output Routing: CV 1-4 → Track 1, TR 1-4 → Track 1
- Metro Mode: Realtime, 500ms
- Use Project Scale: No (12TET)
- CV Update Mode: Always

**INIT Script (I):**
```
# Initialize variables
M 500          # Set metro to 500ms
M.ACT 1        # Enable metro
CV.SLEW 1 100  # CV 1 slew 100ms
CV.SLEW 2 0    # CV 2 no slew
A 0            # Counter starts at 0
B 60           # Base note (C4)
```

**Script 1: Simple Note + Gate**
```
CV 1 N 60      # C4
TR.PULSE 1     # Trigger gate 1
```
**Expected**: Single C note (0V) with gate pulse on TR 1

**Script 2: Random Notes**
```
X RRAND 0 12   # Random 0-12 semitones
CV 1 N + B X   # Base note + random
TR.PULSE 1
```
**Expected**: Random notes C4-C5

**Script 3: Voltage Sweep**
```
A ADD A 100    # Increment by 100
CV 2 A         # Raw voltage output
IF GT A 16383: A 0
```
**Expected**: Sawtooth ramp on CV 2 (0-10V)

**Script 4: Multiple Gates**
```
TR.PULSE 1     # Gate 1
DEL 50: TR.PULSE 2   # Gate 2 after 50ms
DEL 100: TR.PULSE 3  # Gate 3 after 100ms
DEL 150: TR.PULSE 4  # Gate 4 after 150ms
```
**Expected**: Cascading gate pulses

**Script 5: Gate Toggle**
```
TR.TOG 1       # Toggle gate 1 state
```
**Expected**: Gate 1 alternates high/low each trigger

**Script 6: Slew Test**
```
CV 1 RRAND 0 16383  # Random voltage with slew
CV 2 RRAND 0 16383  # Random voltage no slew
```
**Expected**: CV 1 slews smoothly, CV 2 jumps instantly

**Script 7: Math Operations**
```
A RRAND 0 100
B RRAND 0 100
C + A B        # Addition
D * A B        # Multiplication
CV 1 C
CV 2 D
```
**Expected**: Two CVs based on random math

**Script 8: Gate Time Control**
```
TR.TIME 1 RRAND 10 500  # Random gate duration
TR.PULSE 1
```
**Expected**: Variable-length gate pulses

**METRO Script (M):**
```
A ADD A 1      # Increment counter
CV 3 * A 100   # Counter to CV
IF GT A 163: A 0  # Wrap at ~10V
```
**Expected**: Staircase pattern on CV 3, updates every 500ms

**Test Checklist:**
- [ ] CV outputs produce correct voltages
- [ ] Gate pulses trigger
- [ ] Slew works (CV 1 smooth, CV 2 instant)
- [ ] DEL command creates timed delays
- [ ] Metro runs at 500ms intervals
- [ ] Variables (A, B, C, D) work
- [ ] Math ops (ADD, MUL, RRAND) work
- [ ] Conditionals (IF GT) work

---

### Test Set 2: Variables, Patterns, and Control Flow

**Purpose**: Test advanced scripting features, patterns, loops, conditionals

**Setup:**
- Output Routing: CV 1-4 → Track 1, TR 1-4 → Track 1
- Metro Mode: Synced, Divisor = 12 (1/16 note)
- Use Project Scale: No
- CV Update Mode: Gate

**INIT Script (I):**
```
# Initialize patterns
P.N 0
P.L 0 8
P 0 0 0
P 0 1 4
P 0 2 7
P 0 3 12
P 0 4 7
P 0 5 4
P 0 6 0
P 0 7 -5

P.N 1
P.L 1 4
P 1 0 100
P 1 1 80
P 1 2 60
P 1 3 40

M.ACT 1
```

**Script 1: Pattern Sequencer**
```
P.N 0
CV 1 N + 60 P.NEXT 0  # Step through pattern 0
TR.PULSE 1
```
**Expected**: Arpeggiated sequence: C, E, G, C, G, E, C, F

**Script 2: Pattern with Counter**
```
P.N 0
A P.I 0           # Get current index
CV 1 N + 60 P 0 A
TR.PULSE 1
P.NEXT 0          # Advance pattern
```
**Expected**: Same sequence, demonstrates pattern indexing

**Script 3: Conditional Pattern Direction**
```
P.N 0
X RRAND 0 100
IF GT X 50: P.NEXT 0   # Forward if X > 50
IF LT X 50: P.PREV 0   # Backward if X < 50
CV 1 N + 60 P.HERE 0
TR.PULSE 1
```
**Expected**: Random walk through pattern

**Script 4: Nested Pattern (Velocity)**
```
P.N 0
P.N 1
CV 1 N + 60 P.NEXT 0    # Note from pattern 0
CV 2 SCALE P.NEXT 1 0 100 0 16383  # Velocity from pattern 1
TR.PULSE 1
```
**Expected**: Melody with varying velocity/brightness

**Script 5: L (Loop) Command**
```
A 0
L 1 4: A ADD A P.NEXT 0  # Sum first 4 pattern values
CV 1 N + 60 A
TR.PULSE 1
P.N 0
P.START 0  # Reset pattern
```
**Expected**: Plays sum of first 4 notes (0+4+7+12=23)

**Script 6: EVERY Divider**
```
EVERY 4: TR.PULSE 1     # Trigger every 4th time
EVERY 3: TR.PULSE 2     # Trigger every 3rd time
EVERY 2: TR.PULSE 3     # Trigger every 2nd time
TR.PULSE 4              # Trigger every time
```
**Expected**: Polyrhythmic gate pattern (4:3:2:1)

**Script 7: Drunk Walk**
```
DRUNK.MIN 0
DRUNK.MAX 12
DRUNK.WRAP 1
CV 1 N + 60 DRUNK
TR.PULSE 1
```
**Expected**: Random walk melody (0-12 semitones)

**Script 8: Stack Operations**
```
S.CLR              # Clear stack
S.PUSH 60          # Push C4
S.PUSH 64          # Push E4
S.PUSH 67          # Push G4
CV 1 N S.POP       # Play G4
TR.P 1
DEL 100: CV 1 N S.POP  # Play E4
DEL 200: CV 1 N S.POP  # Play C4
```
**Expected**: Descending triad C-E-G played in reverse

**METRO Script (M):**
```
# Euclidean rhythm generator
A ADD A 1
IF EQ % A 3 0: TR.P 1   # Every 3rd
IF EQ % A 5 0: TR.P 2   # Every 5th
IF EQ % A 8 0: TR.P 3   # Every 8th
IF GT A 16: A 0
```
**Expected**: Complex polyrhythm every 1/16 note

**Test Checklist:**
- [ ] Patterns advance/reverse correctly
- [ ] P.NEXT, P.PREV, P.HERE work
- [ ] L (loop) command executes N times
- [ ] EVERY divider triggers correctly
- [ ] DRUNK random walk stays in range
- [ ] Stack push/pop LIFO behavior
- [ ] Metro syncs to transport (tempo changes)
- [ ] CV Update Mode: Gate (CV only updates on gate)

---

### Test Set 3: Cross-Track Orchestration

**Purpose**: Test extended output routing (CV 5-20, TR 5-20), multi-track control

**Setup:**
- Output Routing:
  - CV 1-2, TR 1-2 → Track 1 (bass)
  - CV 3-4, TR 3-4 → Track 2 (melody)
  - CV 5-6, TR 5-6 → Track 3 (harmony)
  - CV 7-8, TR 7-8 → Track 4 (drums/percussion)
- Metro Mode: Synced, Divisor = 12 (1/16)
- Use Project Scale: No

**INIT Script (I):**
```
# Initialize chord progression
P.N 0
P.L 0 4
P 0 0 0    # I (C)
P 0 1 5    # IV (F)
P 0 2 7    # V (G)
P 0 3 0    # I (C)

# Initialize melody pattern
P.N 1
P.L 1 8
P 1 0 0
P 1 1 2
P 1 2 4
P 1 3 5
P 1 4 7
P 1 5 5
P 1 6 4
P 1 7 2

M.ACT 1
A 0    # Beat counter
B 0    # Chord index
```

**Script 1: Chord Change (every 4 beats)**
```
P.N 0
IF EQ % A 4 0: B P.NEXT 0  # Advance chord every 4 beats
```
**Expected**: Chord root changes every 4 triggers

**Script 2: Bass Line (Track 1)**
```
CV 1 N + 36 B      # Root note, octave -2
TR.PULSE 1
DEL 250: TR 1 0    # Gate off after 250ms
```
**Expected**: Bass plays root of current chord on Track 1

**Script 3: Melody (Track 2)**
```
P.N 1
X P.NEXT 1         # Get melody note
CV 3 N + + 60 B X  # Transpose to chord
TR.PULSE 3
```
**Expected**: Melody on Track 2, harmonized with chord

**Script 4: Harmony (Track 3)**
```
# Play triad (root, third, fifth)
CV 5 N + 60 B          # Root
CV 6 N + + 60 B 4      # Third
TR.PULSE 5
TR.PULSE 6
```
**Expected**: Two-voice harmony on Track 3

**Script 5: Kick Drum (Track 4)**
```
IF EQ % A 4 0: TR.PULSE 7    # Kick on downbeat
```
**Expected**: Kick every 4 beats on Track 4

**Script 6: Hi-Hat (Track 4)**
```
TR.PULSE 8         # Hi-hat every 16th
```
**Expected**: Constant 16th notes on Track 4, Gate 2

**Script 7: Snare (Track 4)**
```
IF EQ % A 4 2: TR.PULSE 7    # Snare on beat 3
```
**Expected**: Snare on offbeat (beats 3 and 7)

**Script 8: Fill (Track 4)**
```
X RRAND 0 100
IF GT X 90: L 1 4: TR.P 8    # Random drum fill
```
**Expected**: Occasional rapid hi-hat fills (10% chance)

**METRO Script (M):**
```
A ADD A 1          # Beat counter
IF GT A 15: A 0    # Wrap at 16

# Visual feedback on Track 1 CV 2
CV 2 * A 1024      # Step voltage for beat visualization
```
**Expected**: 4-track orchestration with bass, melody, harmony, drums

**Test Checklist:**
- [ ] CV 1-2 control Track 1 (bass)
- [ ] CV 3-4 control Track 2 (melody)
- [ ] CV 5-6 control Track 3 (harmony)
- [ ] TR 7-8 control Track 4 (drums)
- [ ] Chord progression transposes melody/harmony
- [ ] All tracks sync to same transport clock
- [ ] Cross-track routing doesn't conflict

---

### Test Set 4: Advanced Integration (Scales, Routing, Performer Features)

**Purpose**: Test Performer-native features - project scales, octave/transpose, routable params

**Setup:**
- Output Routing: CV 1-8 → Tracks 1-4 (2 CVs each)
- Metro Mode: Synced, Divisor = 12
- **Use Project Scale: Yes** ← KEY!
- **Project Scale: C Minor** (C D Eb F G Ab Bb)
- **Octave: +1**
- **Transpose: +2** (D)
- CV Update Mode: Gate

**INIT Script (I):**
```
# Initialize scale degrees pattern
P.N 0
P.L 0 8
P 0 0 0    # Root
P 0 1 1    # 2nd
P 0 2 2    # 3rd (Eb in C Minor)
P 0 3 3    # 4th
P 0 4 4    # 5th
P 0 5 5    # 6th (Ab in C Minor)
P 0 6 6    # 7th (Bb in C Minor)
P 0 7 7    # Octave

M.ACT 1
A 0
```

**Script 1: Quantized Melody**
```
P.N 0
CV 1 N P.NEXT 0    # Will be quantized to D Minor (Octave +1, Transpose +2)
TR.P 1
```
**Expected**:
- Notes: D E F G A Bb C D (D Minor scale)
- Octave: One octave higher than written
- All notes quantized to D Minor (project scale transposed)

**Script 2: Random Quantized Notes**
```
X RRAND 0 7        # Random scale degree
CV 2 N X           # Quantized to project scale
TR.P 2
```
**Expected**: Random walk within D Minor scale, octave shifted

**Script 3: Chromatic to Diatonic**
```
X RRAND 0 12       # Random chromatic note
CV 3 N X           # Will snap to nearest scale tone
TR.P 3
```
**Expected**:
- Input: 0,1,2,3,4,5,6,7,8,9,10,11,12
- Output (D Minor): D,D,E,F,F,G,G,A,Bb,Bb,C,C,D

**Script 4: Arpeggio with Octaves**
```
P.N 0
X P.NEXT 0
A ADD A 1
CV 4 N + X * / A 8 12  # Add octave every 8 notes
TR.P 4
```
**Expected**: Arpeggios climbing through octaves in D Minor

**Script 5: Unquantized Control (VV op)**
```
X RRAND 0 16383
CV 5 VV X          # Bypass quantization (raw voltage)
```
**Expected**: CV 5 gets unquantized voltage (bypass project scale)

**Script 6: CV Update Mode Test**
```
# CV Update Mode = Gate
CV 6 N RRAND 0 12  # Set CV
# Don't pulse gate - CV should NOT update
```
**Expected**: CV 6 does not change (Gate mode, no gate pulse)

**Script 7: CV Update Mode Test 2**
```
CV 7 N RRAND 0 12  # Set CV
TR.P 7              # Pulse gate
# CV SHOULD update now
```
**Expected**: CV 7 updates to new note (gate triggered)

**Script 8: Transpose Modulation**
```
# (Assumes Transpose is routed to CV In or similar)
# This script just outputs notes - external routing modulates transpose
A ADD A 1
CV 8 N % A 8       # Cycle through scale degrees
TR.P 8
```
**Expected**: Notes shift in pitch as Transpose param is modulated

**METRO Script (M):**
```
# Chord stabs using project scale
X RRAND 0 7
CV 1 N X           # Root
CV 2 N + X 2       # Third
CV 3 N + X 4       # Fifth
CV 4 N + X 7       # Seventh
TR.P 1
TR.P 2
TR.P 3
TR.P 4
```
**Expected**: Random 4-note chords, all quantized to D Minor

**Test Checklist:**
- [ ] Project Scale = C Minor applied to all N ops
- [ ] Octave +1 shifts all notes up one octave
- [ ] Transpose +2 shifts scale root to D
- [ ] Result: All notes in D Minor scale
- [ ] VV op bypasses quantization
- [ ] CV Update Mode: Gate prevents updates without gates
- [ ] CV Update Mode: Gate allows updates with gates
- [ ] Chromatic inputs snap to nearest diatonic note
- [ ] Metro syncs to divisor (tempo changes reflected)

---

### Test Set 5: Stress Test & Complex Logic

**Purpose**: Push interpreter limits, test complex scripts, edge cases

**Setup:**
- Output Routing: All 8 tracks mapped
- Metro Mode: Synced, Divisor = 6 (1/32 note)
- Use Project Scale: Yes
- CV Update Mode: Always

**INIT Script (I):**
```
# Initialize complex state
L 1 64: P 0 I 0        # Fill pattern 0 with indices
P.L 0 64
A 0
B 0
C 0
D 0
X 0
Y 0
Z 0
T 0
M.ACT 1
```

**Script 1: Fibonacci Sequence**
```
A B              # A = prev
B + A B          # B = A + B
C B              # Store result
IF GT B 1000: B 1  # Reset if overflow
CV 1 SCALE B 0 1000 0 16383
TR.P 1
```
**Expected**: CV follows Fibonacci sequence (1,1,2,3,5,8,13,21...)

**Script 2: Cellular Automaton (Rule 30)**
```
# Simplified 8-bit cellular automaton
X << X 1           # Shift left
IF GT RRAND 0 100 50: X + X 1  # Random bit injection
CV 2 * X 2048
TR.P 2
```
**Expected**: Chaotic CV pattern

**Script 3: Nested Loops**
```
A 0
L 1 4: L 1 4: A ADD A 1  # Nested loop (4x4 = 16 iterations)
CV 3 * A 1024
TR.P 3
```
**Expected**: CV jumps to 16 * 1024 each trigger

**Script 4: Recursive Pattern Access**
```
P.N 0
A P.I 0            # Get current index
B P 0 A            # Get value at index
CV 4 N % + B A 12  # Use index and value
P.NEXT 0
TR.P 4
```
**Expected**: Complex interaction between index and value

**Script 5: Queue Operations**
```
Q.N RRAND 0 64
Q RRAND 0 127      # Push random value
Q RRAND 0 127
Q RRAND 0 127
A Q.AVG            # Average of queue
CV 5 SCALE A 0 127 0 16383
TR.P 5
```
**Expected**: Smoothed random CV (queue average)

**Script 6: Multiple DELays Chain**
```
CV 6 N 60
TR.P 6
DEL 50: CV 6 N 64
DEL 100: CV 6 N 67
DEL 150: CV 6 N 72
DEL 200: CV 6 N 67
DEL 250: CV 6 N 64
DEL 300: CV 6 N 60
DEL 350: TR 6 0
```
**Expected**: Arpeggio: C-E-G-C-G-E-C with precise timing

**Script 7: State Machine**
```
# 4-state sequencer
IF EQ A 0: CV 7 N 60
IF EQ A 1: CV 7 N 64
IF EQ A 2: CV 7 N 67
IF EQ A 3: CV 7 N 72
TR.P 7
A + A 1
IF GT A 3: A 0
```
**Expected**: 4-step sequence C-E-G-C

**Script 8: All Variables Test**
```
A ADD A 1
B * A 2
C / A 3
D % A 5
X - 100 A
Y + X D
Z * C D
T / + Y Z 2
CV 8 SCALE T 0 500 0 16383
TR.P 8
```
**Expected**: Complex math using all variables (A,B,C,D,X,Y,Z,T)

**METRO Script (M) - High Frequency:**
```
# At 1/32 note, this runs very fast
A ADD A 1
CV 1 * A 100      # Fast ramp
CV 2 * RRAND 0 163 100  # Random CV
TR.P 8            # Clock pulse
IF GT A 163: A 0
```
**Expected**: Fast modulation, stress test interpreter speed

**Test Checklist:**
- [ ] Nested loops execute correctly
- [ ] Multiple DELays execute in order
- [ ] Queue operations (Q.AVG) work
- [ ] All 8 variables (A,B,C,D,X,Y,Z,T) work
- [ ] Complex math expressions parse correctly
- [ ] State machine tracks state across calls
- [ ] High-frequency metro (1/32) doesn't crash
- [ ] Fibonacci/recursion doesn't overflow
- [ ] Interpreter handles complex logic without errors

---

### Test Set 6: Edge Cases & Error Handling

**Purpose**: Test boundary conditions, error handling, recovery

**INIT Script (I):**
```
M.ACT 1
A 0
```

**Script 1: Division by Zero**
```
A RRAND 0 10
B / 100 A          # Could be /0 if A=0
CV 1 B
```
**Expected**: Should not crash; /0 should return 0 or max value

**Script 2: Array Out of Bounds**
```
X 100              # Out of pattern range
P.N 0
CV 2 P 0 X         # Access pattern[100]
```
**Expected**: Should clamp to valid range or return 0

**Script 3: Stack Overflow**
```
S.CLR
L 1 20: S.PUSH I   # Push 20 values (stack is 16 deep)
CV 3 S.POP
```
**Expected**: Stack should handle overflow gracefully

**Script 4: Extreme Values**
```
CV 4 32767         # Max int16
TR.TIME 1 32767    # Max time
```
**Expected**: Should clamp to valid ranges

**Script 5: Negative Values**
```
CV 5 -10000        # Negative voltage
```
**Expected**: Should clamp to 0 (Teletype CV is 0-10V)

**Script 6: Empty Pattern**
```
P.N 2              # Uninitialized pattern
CV 6 P.NEXT 2
```
**Expected**: Should return 0 or default value

**Script 7: WAIT Overflow**
```
WAIT 10000         # Very long wait
```
**Expected**: Should queue delay without blocking other scripts

**Script 8: Invalid Commands** (for parser testing)
```
# This would be entered in UI, should show error:
# INVALID.OP 123
# N
# CV 99 100
```
**Expected**: Parser should reject and show error message

**Test Checklist:**
- [ ] Division by zero doesn't crash
- [ ] Out of bounds array access handled
- [ ] Stack overflow handled gracefully
- [ ] Extreme values clamped
- [ ] Negative voltages clamped to 0
- [ ] Empty/uninitialized patterns return defaults
- [ ] Long WAITs don't block execution
- [ ] Invalid ops show parser errors (not crash)

---

## Test Execution Plan

### Phase 1: Sanity Check (Test Set 1)
1. Load Test Set 1
2. Verify basic CV/Gate output
3. Confirm interpreter running
4. Check Metro timing

### Phase 2: Feature Validation (Test Sets 2-4)
1. Test Set 2: Verify advanced ops (patterns, loops, conditionals)
2. Test Set 3: Verify cross-track routing
3. Test Set 4: Verify Performer integration (scales, octave, transpose)

### Phase 3: Stress & Edge Cases (Test Sets 5-6)
1. Test Set 5: Performance under complex load
2. Test Set 6: Error handling and recovery

### Success Criteria
- All test checklists pass
- No crashes or hangs
- Timing accurate (metro, delays)
- Cross-track routing works
- Project scale integration correct
- Error handling graceful

### Known Acceptable Failures (MVP)
- I2C ops (expected, omitted)
- Grid ops (expected, omitted)
- Some LIVE mode features (keyboard-specific)

---

## Conclusion

This integration plan provides a comprehensive roadmap for bringing Monome Teletype's powerful scripting capabilities into the Performer sequencer as a native track type. The design balances three critical goals:

### 1. Teletype Authenticity
- Preserves the core Teletype experience: script execution model, op library, patterns, variables
- Maintains the 11-script system (8 trigger + METRO + INIT + DELAY + LIVE)
- Keeps familiar op syntax and behavior where possible
- Respects the original timing model while offering Performer-native alternatives

### 2. Performer Integration
- Behaves like a native track: divisor-driven metro, project scale integration, octave/transpose
- Leverages Performer's routing system for flexible I/O mapping
- Enables cross-track orchestration via CV 1-20 / TR 1-20 routing (conductor capability)
- Supports routable parameters for dynamic modulation
- Provides both realtime (wallclock) and synced (musical) timing modes

### 3. Practical UI Design
- Replaces keyboard input with three text entry methods: T9, Op Menu, Encoder Scroll
- Uses Performer's button layout: Step 1-8 for script selection, Function buttons for editing
- Provides dedicated pages: Live REPL, Script Editor, Patterns, Variables, I/O Mapping
- Context-aware menus for op selection grouped by category

### 4. Independent Script File System
- **Project independence**: Scenes stored as `.tele` files separate from Performer projects
- **Shareability**: Copy script files between devices via SD card
- **Human-readable format**: Text-based for git/diff/version control
- **Resource-efficient**: 3-6 KB per scene, 50-100ms load time, 5-6 KB RAM overhead
- **Three text entry methods**: T9 phone keypad, encoder scroll, quick templates
- **Dirty tracking**: `[*]` indicator for unsaved changes in file browser

### Key Innovation: Cross-Track Orchestration
The extended output routing (CV 1-20, TR 1-20) transforms a single Teletype track into a "conductor" that can control all 8 Performer tracks simultaneously. This enables:
- Polyrhythmic orchestration (different divisors per track)
- Harmonic arrangements (bass, melody, harmony, drums scripts)
- Conditional routing based on global state
- Generative multi-track composition

### Implementation Timeline
**8-12 weeks total** across 6 phases:
- Phase 1 (2-3 weeks): Core Integration - Teletype VM running hardcoded scripts
- Phase 2a (1 week): I/O Integration - CV/Gate output functional
- Phase 2b (1 week): Input Routing - Script triggers working
- Phase 2c (1 week): Performer Integration - Divisor, scales, octave, transpose
- Phase 3 (2 weeks): Basic UI - Script editing without keyboard
- Phase 4 (2-3 weeks): Advanced UI - Full editor with T9/Op Menu/Encoder entry
- Phase 5 (1-2 weeks): Polish - Patterns, Variables, I/O Map pages, serialization

### Testing Strategy
Six test sets with increasing complexity provide a structured validation path:
1. Basic I/O - Verify core functionality
2. Variables & Control Flow - Advanced scripting
3. Cross-Track Orchestration - Multi-track conductor capability
4. Performer Integration - Native track features
5. Stress Tests - Performance limits
6. Edge Cases - Error handling

### Success Criteria
The integration succeeds when:
- All 6 test sets pass their checklists
- Teletype scripts control multiple Performer tracks simultaneously
- Project scale quantization works correctly
- Text entry methods feel natural without keyboard
- No audio dropouts or timing glitches
- Graceful error handling (no crashes)

### Risk Mitigation
- **Memory**: scene_state_t is large (~10-15 KB) - limit to 1-2 Teletype tracks per project
- **Performance**: Profile script execution (<1ms update target) - implement WHILE_DEPTH limits
- **UI Complexity**: Prototype T9 entry early - validate with user testing
- **Scope Creep**: MVP omits I2C/Grid/Crow - focus on core Teletype ops first

### What Makes This Integration Special
Unlike typical ports that either strip features for simplicity or add complexity through abstraction, this design:
- **Amplifies capabilities**: Extended routing gives Teletype more outputs than the original hardware
- **Maintains identity**: The scripting experience feels authentically Teletype
- **Enables new workflows**: Conductor capability creates entirely new sequencing paradigms
- **Respects both systems**: Teletype's scripting + Performer's routing = synergistic whole

The result is not just "Teletype in Performer" but rather a new hybrid instrument that combines the best of both: Teletype's live-codable flexibility with Performer's multi-track routing power.

---

## Appendix: Quick Reference

### Op Coverage by Category
✅ **Included**: Variables, Math, Control Flow, Patterns, Queue, Stack, Delays, Metro, Hardware (wrapped), Seed, MIDI, Turtle, Init
❌ **Omitted**: I2C, Ansible, Crow, Just Friends, Grid, Earthsea, Meadowphysics, White Whale, Disting, W/

### Hardware Op Mapping
| Teletype Op | Performer Implementation |
|-------------|--------------------------|
| `CV 1-4 value` | Track's own CV outputs 1-4 |
| `CV 5-20 value` | Routed to other tracks (via OutputRoute config) |
| `TR 1-4 value` | Track's own Gate outputs 1-4 |
| `TR 5-20 value` | Routed to other tracks (via OutputRoute config) |
| `TR.P n` | Pulse gate n (default 100ms) |
| `IN` | Mapped CV input (configurable source) |
| `PARAM` | Mapped CV input (configurable source) |
| `STATE n` | Read trigger input n state |

### Metro Modes
| Mode | Behavior | Use Case |
|------|----------|----------|
| Realtime | `M 500` = 500ms (tempo-independent) | Original Teletype behavior, precise timing |
| Synced | `M` driven by track divisor | Musical timing, tempo-aware sequences |

### File System Reference

**File Format:** `.tele` (text-based, human-readable)
**Storage Location:** `/TELE/scenes/` on SD card
**File Size:** 3-6 KB per scene (text), ~5 KB (binary alternative)
**Load Time:** 50-100ms (text), 10-20ms (binary)
**RAM Overhead:** 5-6 KB per active scene

**Text Entry Methods:**
1. **T9 Phone Keypad** - Step buttons 1-8 (multi-tap: 2→A→B→C)
2. **Encoder Scroll** - Scroll through alphabet/numbers
3. **Quick Templates** - Pre-configured names (scene01, arp01, bass01, etc.)

**File Browser UI:**
- 256x64 OLED, 4 rows per page (10px each)
- Shows: filename, size, date, dirty indicator `[*]`
- Functions: F1=Load, F2=Save, F3=New, F4=Delete

**Resource Impact:**
- Storage: Negligible (160,000+ scenes on 16GB SD)
- RAM: 5-6 KB per track (scene_state_t already 10-15 KB)
- CPU: Async file I/O on FileTask (no audio dropouts)
- OLED: 5-10ms/frame (browser only, reduced FPS)

### Routable Parameters Reference

TeletypeTrack supports **29 routing targets** (3 shared + 26 Teletype-specific):

| Target | Display Name | Range | Type | Description |
|--------|-------------|-------|------|-------------|
| **Track-Level Targets (Shared)** |||||
| `Routing::Target::Octave` | Octave | -10 to +10 | Track | Pitch shift applied before quantization |
| `Routing::Target::Transpose` | Transpose | -100 to +100 | Track | Semitone shift applied before quantization |
| `Routing::Target::Divisor` | Divisor | 1-192 | Sequence | Metro period in Synced mode |
| **Teletype-Specific Targets (NEW)** |||||
| `TeletypeMetroPeriod` | TT Metro | 1-10000ms | Teletype | Metro period in Realtime mode |
| `TeletypeVarA` | TT Var A | 0-16383 | Teletype | Route CV to variable A |
| `TeletypeVarB` | TT Var B | 0-16383 | Teletype | Route CV to variable B |
| `TeletypeVarC` | TT Var C | 0-16383 | Teletype | Route CV to variable C |
| `TeletypeVarD` | TT Var D | 0-16383 | Teletype | Route CV to variable D |
| `TeletypeVarE` | TT Var E | 0-16383 | Teletype | Route CV to variable E |
| ... | ... | ... | ... | *Variables F-Y (21 more)* |
| `TeletypeVarZ` | TT Var Z | 0-16383 | Teletype | Route CV to variable Z |
| `TeletypePatternIndex` | TT Pattern | 0-3 | Teletype | Pattern selection (P.I) |

**Variable Routing - Key Use Cases:**
1. **External Modulation**: Route LFO → Variable A, script reads A for pitch/timbre/etc.
2. **CV Control**: Route CV In → Variable B, use B as threshold/probability
3. **MIDI Integration**: Route MIDI CC → Variable C, C controls script behavior
4. **Multi-Source**: Route different sources to A-Z for complex modulation matrices

**Routing Example:**
```
Route 1: CV In 1 → TT Var A (0-16383)
Route 2: CV In 2 → TT Var B (0-100)
Route 3: MIDI CC 1 → TT Metro (100-2000ms)
Route 4: Gate Out 3 → Octave (-2 to +2) with Envelope shaper

Teletype Script M:
  IF GT RRAND 0 100 B: CV 1 N + 60 A; TR.P 1
  # B controls gate probability, A controls pitch offset
```

### Project Integration Points
| Performer Feature | Teletype Integration |
|-------------------|---------------------|
| Project Scale | Overrides `N` op quantization |
| Root Note | Applied during quantization |
| Octave (-10 to +10) | Added before quantization (routable) |
| Transpose (-100 to +100) | Added before quantization (routable) |
| Divisor (1-192) | Drives METRO in Synced mode (routable) |
| Routing Sources | Used for IN/PARAM/TR trigger mapping |
| Routing Targets | 29 targets (3 shared + 26 Teletype-specific) |

---

**Document Version**: 1.2
**Last Updated**: 2025-12-31
**Status**: Design Complete, Ready for Implementation

**Changelog:**
- **v1.2**: Added independent script file system (.tele files) with UI design (file browser, T9 text entry, dialogs), resource cost analysis (RAM: 5-6KB, Load: 50-100ms, OLED: 5-10ms), and FileManager integration
- **v1.1**: Added comprehensive routable parameters system (29 routing targets: Octave, Transpose, Divisor, Metro Period, Variables A-Z, Pattern Index)
- **v1.0**: Initial design document
