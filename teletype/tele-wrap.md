# Teletype Operations Wrapping Analysis

This document categorizes Teletype operations based on their integration requirements with the Performer firmware and the `TeletypeBridge`.

## Category 1: Requires Wrapping (Hardware & System Integration)

These operations explicitly call external functions defined in `teletype_io.h`. Without a functional bridge implementation, these operations are effectively no-ops or will not interact with the outside world.

*   **CV Output (`CV.*`, `V`, `VV`)**
    *   **Calls:** `tele_cv`, `tele_cv_slew`, `tele_get_cv`, `tele_cv_off`.
    *   **Status:** Wrapped. `CV.SLEW` is implemented in `TeletypeTrackEngine` (slew target + Slide smoothing).
*   **Gate/Trigger (`TR.*`)**
    *   **Calls:** `tele_tr`, `tele_tr_pulse`, `tele_tr_pulse_clear`, `tele_tr_pulse_time`.
    *   **Status:** Wrapped.
*   **Inputs (`IN`, `PARAM`)**
    *   **Calls:** `tele_update_adc` (to sync physical hardware values into the VM).
    *   **Status:** Wrapped (mapped to Performer CV Inputs).
*   **Input State (`STATE`)**
    *   **Calls:** `tele_get_input_state`.
    *   **Status:** Wrapped (reads mapped trigger input sources).
*   **System Time (`TIME`, `LAST`)**
    *   **Calls:** `tele_get_ticks` (to calculate elapsed time).
    *   **Status:** Wrapped.
*   **Metronome (`M`, `M!`)**
    *   **Calls:** `tele_metro_updated`, `tele_metro_reset`.
    *   **Status:** Hooks are stubbed; metro is driven by `TeletypeTrackEngine::runMetro`.
*   **Storage & Scenes (`SCENE`, `INIT.SCENE`)**
    *   **Calls:** `tele_scene`.
    *   **Status:** Stubbed. Requires integration with Performer project loading/saving.
*   **MIDI Clock (`MI.CLKD`, `MI.CLKR`)**
    *   **Calls:** `reset_midi_counter`.
    *   **Status:** Stubbed.
*   **Expanders / I2C (`II`, `DISTING`, `W/`, `FADER`, `ER301`, `ANS`, `SB`, `JF`)**
    *   **Calls:** `tele_ii_tx`, `tele_ii_rx`.
    *   **Status:** Stubbed. Requires an I2C driver stack.
*   **Grid I/O (`G.*`)**
    *   **Calls:** `grid_key_press` (for `G.KEY`), implicit state sync for others.
    *   **Status:** Stubbed. Requires Monome Grid USB host driver or emulation.
*   **System Commands**
    *   **Calls:** `tele_kill`, `tele_mute`, `tele_save_calibration` (`CAL` ops).
    *   **Status:** Stubbed. (`DEVICE.FLIP` removed from this build.)

## Category 2: May Need Wrapping (UI Feedback & Status)

These operations function correctly within the Teletype VM logic, but they call "hook" functions to notify the host UI that something has changed. Without wrapping, the internal state updates, but the Performer's OLED screens will not reflect the changes until a manual refresh.

*   **Pattern Display (`P.*`, `PN.*`)**
    *   **Calls:** `tele_pattern_updated`.
    *   **Purpose:** Notify Tracker view to redraw pattern data (e.g., after `P.ROT`).
*   **Live Variable Monitoring (`LIVE.*`)**
    *   **Calls:** `tele_vars_updated`, `set_live_submode`.
    *   **Purpose:** Notify Live Dashboard to update variable values.
*   **User Feedback (`PRINT`)**
    *   **Calls:** `print_dashboard_value`.
    *   **Purpose:** Output debug values to the UI.
*   **Execution Status (`DELAY`, `STACK`)**
    *   **Calls:** `tele_has_delays`, `tele_has_stack`.
    *   **Purpose:** Drive UI icons indicating pending or stacked scripts.

## Category 3: Does Not Need Wrapping (Pure Logic & State)

These operations are the "brain" of Teletype. They manipulate internal memory or perform calculations and do not interact with the bridge at all. They work "out of the box".

*   **Math & Logic:** `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `AND`, `OR`, `XOR`, `NOT`, `MIN`, `MAX`, `AVG`, `SGN`, `LIM`, `MAP`.
*   **Quantization & Scaling:** `QT.*`, `SCL.*`, `IN.SCALE`, `PARAM.SCALE` (Uses internal lookup tables).
*   **Control Flow:** `IF`, `ELIF`, `ELSE`, `?`, `L` (Loops), `W` (While), `EVERY`, `SKIP`, `BREAK`, `SCRIPT` (Triggers other scripts internally).
*   **Variables:** `A`-`D`, `X`-`Z`, `T`, `I`, `J`, `K`, `O`.
*   **Randomness:** `R`, `TOSS`, `PROB`, `DRUNK`, `RAND`, `RRAND`.
*   **Data Structures:** `Q.*` (Queue and Stack manipulations).
*   **Turtle Graphics:** `T.*` (Internal movement logic).
*   **MIDI Data Reads (`MI.N`, `MI.V`, `MI.CC`, etc.)**
    *   *Note:* While these ops do not call external functions (they just read `scene_state.midi`), they require the **Engine** to inject MIDI data into that struct to be useful.

## Proposed Next Steps (Wrapping Roadmap)

*   **Decide CV voltage convention**: confirm whether Teletype CV should be 0–10V or bipolar -5..+5V in Performer, and adjust `rawToVolts`/`voltsToRaw` (and related docs/presets) accordingly.
*   **SCENE persistence**: implement `tele_scene` to load/save Teletype scenes within Performer projects (or explicitly disable ops).
*   **MIDI injection**: populate `scene_state.midi` from Performer’s MIDI input so `MI.*` reads are meaningful.
*   **UI hooks**: wire `tele_pattern_updated`, `tele_vars_updated`, `print_dashboard_value`, `tele_has_delays`, and `tele_has_stack` into relevant UI overlays/pages.
*   **Metronome hooks**: optionally plumb `tele_metro_updated`/`tele_metro_reset` if you want engine-driven state to reflect UI or external clocks.
