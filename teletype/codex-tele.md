# Teletype-Track Integration Plan (Performer)

## Important RAM Implication (Put This First)
- Track models and engines are stored in `Container<...>` with size = max of all
  track types. Adding a large TeletypeTrack inside the container would increase
  RAM for every track.
- Therefore, Teletype state should live outside the per-track container
  (single instance, pointer/handle, or separate pool), or it will blow the
  SRAM budget.

## Goals
- Embed the Teletype interpreter as a Performer track type with maximum compatible ops.
- Omit I2C/II hardware ops (Ansible/Just Friends/etc.) and other impossible hardware.
- Wrap Performer clock, inputs, outputs, scales, and routing into Teletype I/O.
- Replace Teletype keyboard/grid UI with Performer buttons/pages/context menus.

## Teletype Core Summary (from teletype/src)
- Interpreter: `parse()` + `run_script()` in `teletype/src/teletype.c`.
- State: `scene_state_t` in `teletype/src/state.h` (variables, scripts, patterns, delays, stack).
- Ops: modular `teletype/src/ops/*` with `tele_ops[]` in `ops/op.c`.
- Scripts: 8 regular scripts + `METRO` + `INIT` (`teletype/src/script.h`).
- Timing: `tele_tick()` processes WAIT/delays; `M` metronome uses `tele_metro_updated()`.

## Init + Metro Script Behavior (from teletype/module)
**INIT script**
- Runs once after boot/scene load (`initialize_module()` calls `run_script(INIT_SCRIPT)`).
- Runs after preset load (`preset_r_mode.c` calls `run_script(INIT_SCRIPT)`).
- Manual trigger exists in stock UI (F10 / grid button); Performer should provide a UI trigger.
- `INIT` ops in `ops/init.c` reset parts of state and call `tele_metro_updated()`; `INIT.TIME` clears delays and resyncs `EVERY`.

**METRO script**
- Interval set by `M` / `M!` (ms, clamped to minimum). Active toggle via `M.ACT`.
- `tele_metro_updated()` arms/disarms a soft timer and updates icon/state.
- When the timer fires, it runs `METRO_SCRIPT` if the script has content.
- `M.RESET` resets the timer countdown.

## High-Level Architecture in Performer

### 1) New Track Mode: `Teletype`
- Add `Track::TrackMode::Teletype`.
- Create `TeletypeTrack` model object (scripts, patterns, per-track mapping).
- Create `TeletypeTrackEngine` running the Teletype VM with a wrapper.

### 2) Teletype Runtime Wrapper
Add a C++ wrapper that owns a `scene_state_t` and implements `teletype_io.h` functions.

**Suggested layout**
- `src/apps/sequencer/teletype/tele_runtime.h/.cpp`
- `src/apps/sequencer/teletype/tele_io_bridge.cpp` implements `tele_*` functions.
- `src/apps/sequencer/engine/TeletypeTrackEngine.h/.cpp`
- `src/apps/sequencer/model/TeletypeTrack.h/.cpp`

## Op Support Matrix

**Keep (core functionality)**
- `variables.c`, `maths.c`, `controlflow.c`, `patterns.c`, `queue.c`, `stack.c`, `seed.c`, `delay.c`, `metronome.c`, `midi.c`, `turtle.c`, `init.c`.

**Keep with Performer wrappers (hardware ops)**
- `hardware.c` (CV/TR/IN/PARAM/PRINT/MUTE/STATE) remapped to Performer I/O.

**Omit/disable (I2C + external modules)**
- `i2c.c`, `ansible.c`, `crow.c`, `justfriends.c`, `disting.c`, `orca.c`, `earthsea.c`, `matrixarchate.c`, `whitewhale.c`, `meadowphysics.c`, `wslash*.c`, `i2c2midi.c`.

**Omit/replace UI-only ops**
- Grid ops (`grid_ops.c`), Live/Dash/Keyboard-specific ops can be stubbed or redirected to Performer UI widgets.

**Implementation detail**
- Simplest: keep `ops/op.c` but compile a reduced `tele_ops[]` that excludes I2C/grid ops.
- Alternatively: keep op table intact and stub `tele_ii_tx/tele_ii_rx/grid_key_press` to no-op. (Lower risk, but larger binary.)

## I/O Mapping (Wrapper Design)
Teletype expects fixed hardware: 4 CV outs, 4 TR outs, 8 TR ins, 1 IN, 1 PARAM.
Performer has 8 CV outs, 8 gates, 4 CV ins, routing sources, and track layout.

### Outputs (Teletype -> Performer)
- **CV 1..4** -> map to the first four CV outputs assigned to the Teletype track.
  - `TrackEngine::cvOutput(index)` returns `tt.cv[index]` (in volts) for each assigned output.
- **TR 1..4** -> map to the first four gate outputs assigned to the Teletype track.
  - `TrackEngine::gateOutput(index)` returns `tt.tr[index]`.
- **CV slew / CV off** -> implement in Teletype runtime (like `tele_cv_slew`) or translate to Performer’s CV output smoothing (optional).

### Output Expansion / Multi-Output Access (ideas)
Teletype scripts can address multiple hardware outputs at once (CV/TR), and TELEX expanders add more channels.
To avoid boxing Teletype into only 4 outputs, use one of these approaches:

1) **Output Map Page (minimal change)**
   - Add a per-track mapping table: `TT CV1..4` and `TT TR1..4` -> any Performer CV/Gate output.
   - Allow fan-out: one TT output can drive multiple Performer outputs (mirror).

2) **Virtual TELEXo (reuse existing ops)**
   - Implement a local "TELEXo" backend so `ops/telex.c` (`TO.CV`, `TO.TR`, `TO.TR.PULSE`, etc.) write to extra virtual outputs instead of I2C.
   - Map TELEXo channels 1..4, 5..8, ... to Performer CV/Gate outputs via UI.
   - This preserves Teletype syntax for additional outputs without new ops.

3) **Output Banks (quick switching)**
   - Keep CV1..4/TR1..4 ops, but add a per-track "bank" that remaps them to another 4 outputs.
   - Change bank via UI or a dedicated variable/op shim (avoid new op if possible; reuse a variable like `X` as bank selector).

4) **Routing Matrix as Fan-Out**
   - Expose Teletype outputs as routing sources (already done for CV/Gate).
   - Users can route TT outputs to multiple targets without changing scripts.

### Inputs (Performer -> Teletype)
Create a per-track **Tele I/O Mapping** UI that assigns Performer sources to Teletype inputs.

**Digital (TR inputs 1..8)**
- Route sources as inputs (CV in, CV out, Gate out, MIDI CC, etc.).
- Each TR input has:
  - source
  - threshold
  - edge polarity
  - optional debounce
- TR input triggers run script 1..8 (same as Teletype).

**Analog (IN + PARAM)**
- IN/PARAM map to any route source (CV in/out, MIDI CC, or fixed value).
- Use scaling from the route min/max to Teletype 0..16383 domain.

**State ops**
- `STATE x` -> read current mapped digital input state for TRx.
- `TR x` -> read/write Teletype gate outputs.

### MIDI Ops
- Map Teletype MIDI op data to Performer MIDI input buffers.
- `MI.$` and related ops map to scripts triggered by MIDI events.

## Routing Integration (Teletype Track)
**Routable sources (from Teletype track)**
- `CV Out 1..8` and `Gate Out 1..8` as global routing sources.
- These reflect the Teletype track’s outputs after its CV/Gate layout + rotate.

**Routable inputs (into Teletype track)**
- Any `Routing::Source` can be assigned to TT inputs via the Tele I/O Map:
  - CV In 1..4
  - CV Out 1..8
  - Gate Out 1..8
  - MIDI (as configured in route)
- These feed TT `TR 1..8` (digital edge detect) and `IN`/`PARAM` (continuous).

**Routable targets (track parameters)**
- Teletype track should honor existing per-track routing targets:
  - Run, Reset, Mute, Pattern
  - Divisor, ClockMult
  - Scale, RootNote, Octave, Transpose
  - Offset, Rotate, CvOutputRotate, GateOutputRotate
- Other track targets can remain no-ops if they don’t apply to Teletype.

## Clock & Timing Integration

### Core Timing
- Use Performer clock as the master timebase.
- Call `tele_tick()` at a controlled rate derived from Performer ticks, not 10ms.

**Strategy**
- Maintain a `tele_time_ms` accumulator in the Teletype runtime.
- Convert Performer ticks to ms using `Clock::tickDuration()`.
- Call `tele_tick(ss, delta_ms)` in `TeletypeTrackEngine::update()`.

### WAIT / DEL
- Teletype `WAIT` uses `tele_tick()` countdown.
- In Performer, this is now musically aligned (ms derived from current tempo).

### Metronome (M)
- Implement `tele_metro_updated()` to schedule a timer in the runtime.
- Use Performer clock ticks to fire `METRO` script.
- If `M.ACT` is false, disable.
 - Preserve `M.RESET` behavior by resetting the timer accumulator.

### Script triggers
- `TR` inputs run scripts 1..8.
- `M` triggers `METRO` script.
- `INIT` script runs on track init, on preset load, and when manually invoked.

## Track-Parity Behavior (match other Performer tracks)
Teletype track should honor the same musical controls as other track engines.

### Divisor -> Metro
- Use the track's divisor as the primary driver for `METRO` script.
- If divisor changes, update the metro period immediately.
- Optional: allow a "TT Metro Source" toggle (Divisor vs raw `M` value).

### Project Scales + Octave
- When Teletype creates notes (e.g., `N`, `QT`, `SCALE` ops), quantize to `Project::selectedScale()`.
- Apply the track's octave offset after quantize to match other track pitch behavior.

### Route Notes + Transpose
- Apply the track's "route notes" and transpose settings to Teletype pitch output.
- This keeps Teletype track routing consistent with melodic tracks.

### Gated vs Free CV Update
- Expose the existing track CV mode (gated/free) and apply it to TT CV outs:
  - **Gated**: latch/update CV on gate on (or step advance).
  - **Free**: allow continuous CV updates on any script write.

## Scale Integration (Project Scales)
Teletype has its own scale/quantize ops (`QT`, `SCALE`, `N.BX`, etc.).
To use Performer’s project scale system:
- Override quantization ops to use `Project::selectedScale()` and `Project::rootNote()`.
- Provide a small shim so `QT` uses Performer scale for pitch-to-volts conversion.
- Preserve Teletype bitmask ops for scripts that depend on custom scales (optional).

## UI Mapping (Performer Buttons -> Teletype UX)

### Core Pages
- **Tele Live**: REPL input + last output (like TT live mode).
- **Tele Edit**: script editor (1..8, M, I), six lines per script.
- **Tele Patterns**: TT patterns 1..4 (values, length, start/end).
- **Tele Vars**: A..Z + key vars (J/K/I/O/DRUNK) display.
- **Tele IO Map**: assign Performer sources to TR/IN/PARAM outputs.

### Input Controls
- **Step buttons (1–8)**: select script or trigger TR1..TR8.
- **Left/Right**: cursor within line; Shift+Left/Right = word step.
- **Up/Down**: line navigation.
- **Function keys**: copy/paste, insert/delete line, run line, run script.
- **Page/Shift**: switch Tele subpages; Shift toggles edit/command mode.
- **Context menu**: token/op picker (instead of keyboard typing).

### Editing Without Keyboard
- Use an opcode palette + numeric entry.
- Token insertion via context menu (ops list grouped by category).
- Line editor supports insert/delete, clamp to 6 lines.

## Runtime Integration Details (Code Ideas)

### Teletype I/O Bridge (sketch)
```cpp
// teletype_io_bridge.cpp
uint32_t tele_get_ticks() { return Engine::instance().clockTicks(); }

void tele_cv(uint8_t i, int16_t v, uint8_t s) {
    teleRuntime.setCv(i, ttToVolts(v), s != 0);
}

void tele_tr(uint8_t i, int16_t v) { teleRuntime.setGate(i, v > 0); }

void tele_tr_pulse(uint8_t i, int16_t timeMs) {
    teleRuntime.pulseGate(i, timeMs);
}
```

### Track Engine Outputs
```cpp
float TeletypeTrackEngine::cvOutput(int index) const {
    return _runtime.cv(index); // return TT CV in volts
}

bool TeletypeTrackEngine::gateOutput(int index) const {
    return _runtime.gate(index);
}
```

### Tele Input Mapping
```cpp
struct TeleInputMap {
    Routing::Source source;
    float threshold;
    bool rising;
    bool falling;
};

void TeleRuntime::updateInputs(const TeleInputMap& map) {
    // sample route source, update IN/PARAM values and TR edges
}
```

## Test Script Sets (for Performer)

### Set A: Basic I/O (Scripts 1-8)
Script 1:
```tt
TR.PULSE 1
```
Script 2:
```tt
TR.PULSE 2
```
Script 3:
```tt
TR.TOG 1
```
Script 4:
```tt
CV 1 N 60
```
Script 5:
```tt
CV 2 N 67
```
Script 6:
```tt
CV 1 IN
```
Script 7:
```tt
CV 2 PARAM
```
Script 8:
```tt
A + A 1
CV 1 N + 60 A
```

### Set B: Metro + Scale + Delays (Scripts 1-8)
Script 1:
```tt
TR.PULSE 1
CV 1 N.BX O 0
```
Script 2:
```tt
PROB 30: TR.PULSE 2
```
Script 3:
```tt
DEL.X 3 30: TR.PULSE 1
```
Script 4:
```tt
A + A 1
```
Script 5:
```tt
CV 1 N.BX + O A 0
```
Script 6:
```tt
CV 2 N.BX DRUNK 0
```
Script 7:
```tt
TR.TIME 1 40
```
Script 8:
```tt
O 0
A 0
```

Optional (for Set B):
```tt
M:
SCRIPT 1

I:
O.MIN 0
O.MAX 7
O.INC 1
DRUNK.MIN 0
DRUNK.MAX 7
DRUNK.WRAP 1
```

## Serialization
- Store Teletype scripts as text in Performer project (easier to edit); parse into `tele_command_t` on load.
- Alternatively store tokenized `tele_command_t` directly for faster load.
- Patterns/variables stored in TeletypeTrack model fields.

## Script Library (Project-Independent)
Goal: scripts are saved/loaded outside of projects, like a shared library.

**Storage format**
- Use Teletype-style text files (`#1..#8`, `#M`, `#I`) for whole-script sets.
- Directory suggestion: `sd:/teletype/` (sim: `res/teletype/`).
- Each file is one "set" (all scripts); optional per-script files can be added later.

**Model integration**
- Track stores a `scriptSetRef` (filename + optional hash) and a dirty flag.
- On project load: if file exists, load into `scene_state_t`; if missing, keep cached scripts.
- Editing marks dirty; explicit Save writes to library (not project).

**UI flows**
- Load Set: choose file -> confirm -> replace current scripts.
- Save Set: write current scripts to a file (overwrite or new).
- Save Copy: write to new name to preserve library versions.

## UI Considerations (256x64 OLED, 6 lines per script)
Assume 8px font height -> 8 rows visible. Use 6 rows for script lines plus header/status.

**Script Edit Layout**
- Row 0: `S:1  SET:Melodic01  *` (script slot + set name + dirty marker).
- Rows 1–6: line number + text (`1:` .. `6:`).
- Row 7: status or soft actions (`Run`, `Save`, `Load`, errors).
- 256px width @ 8px font => 32 chars; allocate 3 chars for line number + 1 space = 28 chars for script.
- For long lines: horizontal scroll or token wrap with soft indicator.

**Set Browser Layout**
- Row 0: `LIB  12/64` or filter text.
- Rows 1–7: file list; highlight selected.

**Line Editor**
- Show token cursor with inverted block; SHIFT for word-jump and token menu.
- Context menu uses right-side overlay when needed; don’t steal a script row.

## Risks / Constraints
- Interpreter runs on main engine thread: avoid long loops. Keep `WHILE_DEPTH` protection.
- Memory: TT `scene_state_t` is large; remove grid fields to reduce footprint.
- UI complexity: token entry without keyboard needs careful UX.

## Resource Cost Estimate (Script Library + UI)
Approximate incremental cost beyond the core VM:
- **RAM**:
  - Script text buffer (10 scripts * 6 lines * 64 chars): ~3.8 KB if kept in RAM.
  - UI line buffers: ~0.5 KB.
  - Library listing cache (e.g., 64 names * 32 chars): ~2 KB.
  - Total: ~6–8 KB if cached aggressively; can be trimmed by loading on demand.
- **Flash**:
  - Script browser + file I/O glue: small, likely <10 KB.
  - Reusing existing serializer/parsers avoids extra footprint.
- **CPU**:
  - Parsing 60 lines on load is low cost; load is infrequent.
  - Runtime cost unchanged; file I/O only on explicit save/load.

## Implementation Phases
1. **Core VM**: Compile Teletype core + reduced ops list.
2. **I/O bridge**: map CV/TR/IN/PARAM and clock.
3. **Track engine**: new `TeletypeTrackEngine` producing CV/Gate outputs.
4. **UI**: Live/Edit/Patterns/IO mapping pages.
5. **Scale integration**: swap TT quantize ops to Performer scales.
6. **Polish**: script trigger routing, MIDI event scripts, presets.

## Open Decisions
- Keep Teletype’s internal scale system or fully replace with Performer project scales?
- Should TT CV outputs use global CV output layout or track-local mapping only?
- How many Teletype tracks allowed at once (memory)?
