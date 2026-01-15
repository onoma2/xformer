# Feature: Just Friends "Geode" Engine

This feature implements a virtual control voltage generation engine modeled after the "Geode" concept—inspired by the Mannequins Just Friends module—within the Performer/Teletype environment.

It simulates 6 internal "voices" that generate envelopes/LFOs based on polyrhythmic relationships to the main clock, shaping them with "Physics" (Transient, Sustain, Cycle modes) and mixing them to a single output.

## Functional Description

The Geode Engine operates as a subsystem of the `TeletypeTrackEngine`. It does not consume physical track resources but offers a complex modulation source accessible via Teletype scripts. The output is a control signal (CV) intended to modulate other parameters (pitch, filter, etc.) or be output directly to a CV jack.

### 1. The Geode Sequencer (Time)
*   **Driven by Transport:** The engine is phase-locked to Performer's transport.
*   **6 Voices:** Each voice has a `DIV` (Divisor) and `REP` (Repeats) setting.
*   **Polyrhythms:** Voice 1 might fire every measure (Div 1), while Voice 6 fires 7 times a measure (Div 7).
*   **Phasor Logic:** `VoicePhase = (MasterMeasurePhase * Divisor) % 1.0`. A trigger is generated when the phase wraps.

### 2. The Physics Engine (Dynamics)
Determines the *amplitude/velocity* of each trigger based on the global `MODE` and `RUN` parameter.

*   **Mode 0: Transient (Rhythmic Accent)**
    *   High `RUN` values apply a Euclidean-style mask, silencing steps to create rhythms.
*   **Mode 1: Sustain (Gravity)**
    *   Triggers decay in amplitude over the course of a burst. `RUN` sets the decay rate (gravity).
*   **Mode 2: Cycle (Undulation)**
    *   Amplitude modulates via a sine LFO over the burst. `RUN` sets the LFO speed.

### 3. The Manifold (Shaping)
Generates the actual CV envelope (Attack/Decay) for each voice.

*   **Global Params:**
    *   **TIME:** Base duration of the envelopes.
    *   **INTONE:** Spreads the time across voices. High intone makes high-index voices faster (snappy) and low-index voices slower (swells).
    *   **RAMP:** Skews Attack vs. Decay.
    *   **CURVE:** Bends the slope (Log to Exp).

### 4. Output
*   **Summed Mix:** All 6 active voices are summed to a single normalized output (0.0 - 1.0).
*   **Teletype Access:** Scripts can read this value (`GEO.VAL`) and assign it to physical CV outputs (`CV 1 GEO.VAL`).

---

## Implementation Plan

### Phase 1: Core Engine (C++)

**File:** `src/apps/sequencer/engine/GeodeEngine.h` / `.cpp`

A standalone class handling the DSP logic. It reuses Performer's `Curve` math for shaping.

```cpp
class GeodeEngine {
public:
    void update(float dt, float measurePhase);
    // ... setters for Time, Intone, Ramp, Curve, Run, Mode
    // ... setters for Divisor, Repeats
    float output() const;
};
```

### Phase 2: Host Integration

**File:** `src/apps/sequencer/engine/TeletypeTrackEngine.h` / `.cpp`

*   Add `GeodeEngine _geodeEngine;` member.
*   In `update(dt)`, call `_geodeEngine.update(dt, measureFraction())`.
*   Expose helper methods for the Teletype Bridge to set Geode parameters.

### Phase 3: Bridge & Ops (C Interface)

**File:** `src/apps/sequencer/engine/TeletypeBridge.h` / `.cpp`
*   Add C-compatible exports: `tele_geo_set_time`, `tele_geo_get_val`, etc.

**File:** `teletype/src/ops/geode.c` / `.h`
*   Implement the new Teletype operations.

**New Operations:**

| Op | Description |
| :--- | :--- |
| `GEO.TIME x` | Set base time (0-16383). |
| `GEO.TONE x` | Set intone/spread (-16383 to 16383). |
| `GEO.RAMP x` | Set ramp/skew (0-16383). |
| `GEO.CURV x` | Set shape curve (-16383 to 16383). |
| `GEO.RUN x` | Set physics run param (0-16383). |
| `GEO.MODE x` | Set mode (0=Trans, 1=Sust, 2=Cyc). |
| `GEO.DIV i x` | Set voice `i` divisor to `x`. |
| `GEO.REP i x` | Set voice `i` repeats to `x` (-1=inf). |
| `GEO.VAL` | Get current mixed output value. |
| `GEO.READ i` | Get current value of voice `i`. |

**File:** `teletype/src/table.c`
*   Register the new ops in the dispatch table.

### Phase 4: Integration
*   Build the simulator.
*   Verify functionality with a test script.

## Reused Components
*   **Engine Transport:** `_engine.measureFraction()` for sample-accurate syncing.
*   **Curve Library:** `model/Curve` for envelope shaping.
*   **Bridge Architecture:** Existing `TeletypeBridge` pattern for C/C++ interop.
