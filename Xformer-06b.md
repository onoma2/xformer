# Xformer-06b Release Notes

## 1. Changes to Pre-Existing Tracks

### Curve Track
*   **Chaos Mixing:**
    *   Implemented crossfade mixing for Chaos.
    *   Replaces direct signal addition with a blend: `Shape * (1-Amount) + NormalizedChaos * Amount`.
    *   Prevents DC offset drift and keeps the signal strictly within the 0-1 range before wavefolding.
*   **Frequency Modulation (FM):**
    *   **Free Play Mode:** Now uses a **Phase Accumulator** engine for smooth, click-free FM.
    *   **Curve Rate:** Repurposed from the manual `XFade` parameter to control the frequency of the phase accumulator.
*   **Initialization:** Defaults to **Free** play mode with **Divisor 192**.

### Algo (Tuesday) Track
*   **3-Slot Clipboard (Quick Edit):** Dedicated clipboard system accessed via `Shift` + `Page` (Steps 9-14) for Slot 1-3 Copy/Paste.
*   **Enhanced Content:** Clipboard now includes Octave, Transpose, Root Note, Divisor, and Mask in addition to core algorithm parameters.
*   **Context Menu:** Added `INIT`, `RESEED`, `RAND`, `COPY`, `PASTE`.
*   **Shortcuts:**
    *   `Shift` + `F5`: **Reseed** Flow/Ornament RNG.
    *   `Shift` + `Step 16`: **Hard Reset** of track engine.
*   **Status Box:** Real-time feedback for Note, Gate, CV Voltage, and Step position.

### Note Track
*   **New Play Modes:**
    *   **Ikra:** Features a separate **Note Loop** (`Note First/Last Step`) running independently of the rhythm sequence with its own divisor (`Divisor Y`).
    *   **Re:Rene:** Cartesian sequencer (X/Y) with independent divisors and **XY Seek Mode** to keep the playhead within defined step bounds.
*   **Pulse Count & Gate Modes:**
    *   Per-step repetition (1-8 pulses) with modes: **All**, **First**, **Hold**, **FirstLast**.
    *   Support for **Burst** (immediate) or **Spread** (distributed) retrigger logic with Accumulator integration.

### Discrete Map Track
*   **Pluck Effect:** Decay envelope applied to CV output on gate fire, featuring adjustable depth, direction, and random **Jitter**.
*   **Linear Slew:** Constant-rate slew limiting for smooth voltage transitions.
*   **Monitor Mode:** Added to both layers for easier stage/pitch auditing.

### Indexed Track
*   **Step Count:** Increased to **48 steps**.
*   **Monitor Mode:** Added to the Note layer.
*   **Initialization:** Defaults to **Free** play mode.

### Accumulator Enhancements
*   **UI Indicators:** Header counter display and per-step trigger dots.
*   **Trigger Modes:** **STEP**, **GATE**, and **RTRIG** (Retrigger) modes.
*   **Conditional Logic:** Accumulator evaluations now respect per-step conditions.

---

## 2. New Functionality

### CV Router Engine
*   **4x4 Matrix Routing:** Independently maps 4 inputs to 4 outputs at a 1000Hz control rate.
*   **Scanning & Routing:**
    *   **Scan:** Crossfades between the 4 selected input sources (CV In, Bus, or Off).
    *   **Route:** Distributes the scanned signal across 4 selected destinations (CV Out, Bus, or None) using weighted distribution.
*   **Integration:** Controllable via `CVR Scan` and `CVR Route` targets in the Routing system.

### Routing System
*   **VCA Next Shaper (VC):** Center-referenced amplitude modulation using the next route's raw source value.
*   **Per-Track Reset:** Hard reset of track engines via routed signals.
*   **Bus Route Shapers:** Full shaper support (Crease, Envelope, Follower, etc.) for internal Bus targets.

---

## 3. T9type (Teletype) Track

Integration of the Monome Teletype scripting language into the Performer environment.

### Core Architecture
*   **Scripts:** 4 Trigger scripts, 1 Metro script, and 1 Init script.
*   **Scope:** Scripts 1-3 are **Global**; Script 4 and Metro are **Slot Owned** (per pattern).
*   **I/O Mapping:** Flexible virtual-to-physical mapping for Triggers (TI-TR / TO-TR) and CV (TI-IN-PARAM / TO-CV).
*   **Persistence:** Binary Teletype state and plain-text scripts are saved within the project file.

### New & Enhanced Ops
*   **WBPM / WBPM.S:** System BPM control.
*   **WMS / WTU / BAR:** Timing, sleep, and barrier synchronization.
*   **WP / WP.SET / WR / WR.ACT:** Pattern and Record state management.
*   **RT:** Read Route source values directly into scripts.
*   **M.A / M.ACT.A / M.RESET.A:** Global metronome control.
*   **PANIC:** Emergency reset of all triggers and state.

### User Interface
*   **Keyboard Support:** QWERTY keyboard integration for script editing.
*   **Script View:** Features line rotation (`Shift` + `Enc`), I/O grid visualization, and CV bars.
*   **Pattern View:** Optimized navigation with `Enter` and `Backspace` shortcuts.