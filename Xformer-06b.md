# Xformer-06b Release Notes

## 1. Changes to Pre-Existing Tracks

### Curve Track
*   **Chaos Mixing:**
    *   Implemented crossfade mixing for Chaos.
    *   Replaces direct signal addition with a blend: `Shape * (1-Amount) + NormalizedChaos * Amount`.
    *   Prevents DC offset drift and keeps the signal strictly within the 0-1 range before wavefolding.
*   **Frequency Modulation (FM):**
    *   **Free Play Mode:** Now uses a **Phase Accumulator** engine.
    *   **Curve Rate:** Controls the frequency of the phase accumulator, allowing for smooth, click-free FM.
    *   **Routing:** The `XFade` routing target was temporarily repurposed but has been **restored** as a manual parameter. FM is controlled via the `Curve Rate` parameter.
*   **Initialization:**
    *   Defaults to **Free** play mode.
    *   Sequences default to **Divisor 192**.

### Algo (Tuesday) Track
*   **3-Slot Clipboard (Quick Edit):**
    *   Implemented a dedicated clipboard system for Algo tracks.
    *   **Shortcuts:** Accessed via `Shift` + `Page` (Steps 9-14).
        *   **Steps 9-11:** Copy to Slot 1-3.
        *   **Steps 12-14:** Paste from Slot 1-3.
    *   **Content:** Clipboard stores Algorithm, Flow, Ornament, Power, Loop, Rotate, Glide, Skew, Gate params, **plus** Octave, Transpose, Root Note, Divisor, and Mask.
*   **Context Menu:**
    *   Added `INIT`, `RESEED`, `RAND` (Randomize), `COPY`, `PASTE`.
*   **Shortcuts:**
    *   `Shift` + `F5`: **Reseed** (Generate new random values for Flow/Ornament).
    *   `Shift` + `Step 16`: **Reset** (Hard reset of the track engine).
    *   `Step 15` (Quick Edit): **Randomize** parameters.
*   **Status Box:**
    *   New visual feedback element showing Current Note, Gate Status, CV Voltage, and Current Step/Loop position.

### Note Track
*   **New Play Modes:**
    *   **Ikra:** Inspired by the Flux sequencer. Features a separate **Note Loop** (`Note First Step`, `Note Last Step`) that runs independently of the main rhythm/gate sequence. The Note Loop has its own divisor (`Divisor Y`), allowing for complex phasing effects between rhythm and pitch.
    *   **Re:Rene:** A Cartesian sequencer mode (X/Y) with **XY Seek Mode**. Steps are accessed via X and Y coordinates with independent divisors (`Divisor` for X, `Divisor Y` for Y) and constrained step ranges. The "Seek" logic ensures the playhead stays within the defined `First Step` and `Last Step` bounds by skipping invalid coordinates.
*   **Pulse Count & Gate Modes:**
    *   **Pulse Count:** Per-step repetition count (1-8 pulses).
    *   **Gate Modes:**
        *   **A (All):** Fires gates on every pulse.
        *   **1 (First):** Fires gate only on the first pulse.
        *   **H (Hold):** Fires one long gate covering all pulses.
        *   **1L (FirstLast):** Fires gates on the first and last pulse.
    *   **Burst/Spread:** Retriggers now support Burst (all at once) or Spread (distributed) logic, with Accumulator integration.

### Discrete Map Track
*   **Pluck Effect:**
    *   Added a **Pluck** parameter that introduces a decay envelope to the CV output when a gate fires.
    *   Controls pitch depth and direction.
    *   Includes random **Jitter** for organic variation in depth and decay time.
*   **Linear Slew:**
    *   Implemented constant-rate slew limiting for predictable glides between voltages.
*   **Monitor Mode:** Added monitor mode support for both layers.

### Indexed Track
*   **Step Count:** Increased maximum step count to **48 steps**.
*   **Monitor Mode:** Added monitor mode support for the Note layer.
*   **Initialization:**
    *   Defaults to **Free** play mode.

### Accumulator Enhancements
*   **Visual Indicators:**
    *   **Counter Display:** Shows current accumulator value (e.g., "+5") in the header.
    *   **Trigger Dots:** Small indicators on steps where the accumulator trigger is enabled.
*   **Trigger Modes:**
    *   **STEP:** Increment once per step.
    *   **GATE:** Increment per gate pulse (respects pulse count).
    *   **RTRIG:** Increment N times for N retriggers (Burst mode).
*   **Logic:** Accumulator evaluations now respect per-step conditions.

---

## 2. New Functionality

### Routing System
*   **CV Router Engine:** Comprehensive updates to the CV routing backend.
*   **VCA Next Shaper (VC):** Amplitude modulation using the next route's raw source value.
*   **Per-Track Reset Target:** Hard reset of track engine on the rising edge of a routed signal.
*   **Bus Route Shapers:** Added routing shapers for internal buses.

---

## 3. T9type (Teletype) Track

The **T9type** track is a new track mode integrating a subset of the Monome Teletype scripting language, tailored for the Performer environment.

### Core Architecture
*   **Scripts:**
    *   **4 Trigger Scripts (1-4):** Triggered by input events or manually.
    *   **1 Metro Script (M):** Runs periodically based on a timer or clock division.
    *   **Init Script (I):** Runs on project load/boot.
    *   **Script Scope:** Scripts 1-3 are **Global** (shared across patterns), while Script 4 and Metro are **Slot Owned** (stored per pattern slot).
*   **Variables:**
    *   Standard Teletype variables: `X`, `Y`, `Z`, `T` (delta time).
    *   `M` / `M.ACT`: Metronome interval and active state.
    *   `TR.TIME`: Pulse widths for trigger outputs.
*   **State:** Binary Teletype state is now stored within the project file.

### I/O System
Flexible I/O mapping system bridging physical/logical Performer signals with the virtual Teletype environment.

#### Inputs (to Teletype)
*   **TI-TR1...TI-TR4 (Trigger Inputs):**
    *   Mapped to physical **CV Inputs** (Threshold > 1V), **Gate Outputs** (Loopback), or **Logical Track Gates**.
*   **TI-IN / TI-PARAM (CV Inputs):**
    *   Mapped to `IN` and `PARAM` variables.
    *   Sources: Physical CV Inputs, Physical CV Outputs (Loopback), or Logical Track CVs.
*   **TI-X / TI-Y / TI-Z:**
    *   Direct mapping of sources to `X`, `Y`, `Z` variables.

#### Outputs (from Teletype)
*   **TO-TRA...TO-TRD (Trigger Outputs):** Mapped to physical **Gate Outputs**.
*   **TO-CV1...TO-CV4 (CV Outputs):** Mapped to physical **CV Outputs** with Range, Offset, Quantize, and Root Note processing.

### New & Enhanced Ops
*   **Hardware Control:**
    *   `WBPM val`: Write system BPM (1-1000).
    *   `WBPM.S`: Write BPM Sync mode.
    *   `WP / WP.SET`: Pattern selection and control.
    *   `WR / WR.ACT`: Record state and control.
    *   `RT`: Read Route source value.
    *   `M.A / M.ACT.A / M.RESET.A`: Global Metronome controls.
    *   `PANIC`: Stop all triggers and reset state.
*   **Timing & Delays:**
    *   `WMS val`: Wait (sleep) for milliseconds.
    *   `WTU val`: Wait until absolute time.
    *   `BAR`: Barrier op for synchronization.
*   **Pattern Ops:** `P.PA`, `P.PS`, `P.PM`, `P.PD`, `P.PMOD`, `P.SCALE`, `P.SUM`, `P.AVG`, `P.MINV`, `P.MAXV`, `P.FND`, `RND.P`, `RND.PN`.

### User Interface & Shortcuts
*   **Keyboard MVP:** Implemented QWERTY keyboard support for script editing.
*   **Script View:**
    *   `Shift` + `Encoder`: Line rotation/scrolling.
    *   `Step 16`: Commit line changes.
    *   Display showing I/O grid and CV bars.
*   **Pattern View:**
    *   `Enter` (Encoder Press) / `Backspace`: Navigation shortcuts.
*   **Layout:** Consistent Teletype I/O and layout management.
