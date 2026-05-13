# Teletype→Performer Ecosystem — OVERVIEW

## 6-Layer Pipeline Architecture

```
 ┌──────────────────────────────────────────────────────────────────┐
 │                          UI LAYER                               │
 │  TeletypeScriptViewPage  TeletypePatternViewPage  TextInputPage │
 │  (script editing)        (pattern editing)         (names)      │
 └──────────────────────────┬───────────────────────────────────────┘
          │ parse + validate ← typed text                          │
          │ commit tele_command_t                                  │
          ▼                                                         │
 ┌──────────────────────────────────────────────────────────────────┐
 │                       MODEL LAYER                                │
 │  TeletypeTrack                                                   │
 │  ├── PatternSlots[2]  ← persistent (save/load)                 │
 │  │   ├── slotScript + metro (per-slot)                         │
 │  │   ├── patterns[4] (per-slot)                                 │
 │  │   ├── I/O routing + CV config + timing (per-slot)           │
 │  │   └── MIDI source (per-slot)                                │
 │  └── scene_state_t _state  ← live VM state, rebuilt on slot    │
 │        switch                                                   │
 │      ├── scripts[6] (parsed commands)                           │
 │      ├── variables (a,b,c,tr,cv,in,param,...)                  │
 │      ├── patterns[4] (runtime values)                           │
 │      └── delay queue + stack + turtle + RNG                    │
 └──────────────────────────┬───────────────────────────────────────┘
          │ _state ref passed to VM                                │
          ▼                                                         │
 ┌──────────────────────────────────────────────────────────────────┐
 │                  C TELETYPE VM (STATIC)                          │
 │  teletype/src/                                                  │
 │  ├── scanner (Ragel parser)    ── text → tele_command_t         │
 │  ├── process_command           ── stack-based RPN execution     │
 │  ├── run_script/tele_tick      ── script + delay execution      │
 │  ├── tele_ops[] / tele_mods[]  ── FULLY STATIC op tables        │
 │  └── teletype_io.h callbacks   ── extern → bridge               │
 └──────────────────────────┬───────────────────────────────────────┘
          │ tele_*() callbacks (CV, TR, ENV, LFO, etc.)           │
          ▼                                                         │
 ┌──────────────────────────────────────────────────────────────────┐
 │                     BRIDGE LAYER                                 │
 │  TeletypeBridge                                                  │
 │  ├── g_activeEngine (global singleton per execution)            │
 │  ├── tele_cv() → TeletypeTrackEngine::handleCv()               │
 │  ├── tele_tr() → TeletypeTrackEngine::handleTr()               │
 │  ├── tele_tr_pulse() → schedule pulse                           │
 │  ├── ScopedEngine RAII guard                                    │
 │  └── g_dashboardValues[16] (PRINT display)                      │
 └──────────────────────────┬───────────────────────────────────────┘
          │ handleCv → slew/interp → output buffers                │
          ▼                                                         │
 ┌──────────────────────────────────────────────────────────────────┐
 │                     ENGINE LAYER                                 │
 │  TeletypeTrackEngine                                             │
 │  ├── update() tick loop (all sub-systems):                     │
 │  │   1. ADC input read (TI-IN, TI-PARAM, X, Y, Z, T)           │
 │  │   2. CV slew (Slide per channel)                             │
 │  │   3. tele_tick() (VM tick — delays, turtle)                  │
 │  │   4. detect trigger rising edges → run script                │
 │  │   5. metro countdown → run metro script                      │
 │  │   6. pulse timer management                                  │
 │  │   7. envelope state machine (attack→decay→loop)              │
 │  │   8. LFO waveform generation (tri/saw/pulse/wav)            │
 │  │   9. CV interpolation (per-channel phase advance)            │
 │  │  10. Geode engine update (voice allocation, bar)            │
 │  ├── output buffers:                                            │
 │  │   _performerCvOutput[8]   ← final CV volts per track slot   │
 │  │   _performerGateOutput[8] ← final gate bools per track slot │
 │  └── config: range/offset/quantize/root applied before buffer   │
 └──────────────────────────┬───────────────────────────────────────┘
          │ updateTrackOutputs() gathers all tracks                 │
          ▼                                                         │
 ┌──────────────────────────────────────────────────────────────────┐
 │                  OUTPUT COLLECTION + HARDWARE                    │
 │  Engine::updateTrackOutputs()                                    │
 │  ├── gateOutputTracks[8] → route gate per physical output      │
 │  ├── cvOutputTracks[8]   → route CV per physical output        │
 │  ├── _gateOutput.setGate(physCh, bool)                          │
 │  ├── _cvOutput.setChannel(physCh, volts)                        │
 │  └── rotation + CV route lanes applied here                     │
 │                                                                  │
 │  CvOutput::update()  → cal → DAC8568 SPI write                  │
 │  GateOutput::update() → 74HC595 shift register SPI write        │
 └──────────────────────────────────────────────────────────────────┘
```

---

## Who Owns What

| What | Owner | Static/Dynamic |
|------|-------|----------------|
| **Script text** (typed by user) | Model → `scene_state_t.scripts[].c[]` | Dynamic — per line |
| **Opcode tables** (`tele_ops[]`, `tele_mods[]`) | C VM (ROM) | **Static** — compiled in |
| **Variable values** (a, b, cv[4], tr[4], etc.) | Model → `scene_state_t.variables` | Dynamic — per tick |
| **Pattern data** (64 vals × 4 patterns) | Model → `scene_state_t.patterns[].val[]` | Dynamic — per edit |
| **I/O routing config** (TI-TR sources, TO-CV dests) | Model → `PatternSlot` (per-slot) | Dynamic — persisted |
| **CV output config** (range, offset, quantize) | Model → `PatternSlot.cvOutput*` | Dynamic — persisted |
| **Timing config** (timebase, divider, MIDI) | Model → `PatternSlot` | Dynamic — persisted |
| **Engine runtime state** (slew, envelopes, LFOs, pulses) | `TeletypeTrackEngine` (stack) | Dynamic — per tick |
| **ADC input values** (in, param, x, y, z, t) | `TeletypeTrackEngine::updateAdc()` | Dynamic — per update read |
| **CV output buffer** (`_performerCvOutput[8]`) | `TeletypeTrackEngine` | Dynamic — per tick write |
| **Gate output buffer** (`_performerGateOutput[8]`) | `TeletypeTrackEngine` | Dynamic — per script write |
| **Global engine routing** (gateOutputTracks[8], cvOutputTracks[8]) | `Engine` (via Project) | Dynamic — per layout |
| **DAC calibration curves** | `CvOutput` → `Calibration` | Static per unit |
| **DAC hardware values** | `Dac::_channels[]` → SPI | Dynamic — per update |
| **Shift register gates** | `GateOutput::_gates` (uint8_t) | Dynamic — per update |
| **Pattern slot switching** (save/restore slot ↔ VM state) | `TeletypeTrack::onPatternChanged()` | Dynamic — per pattern change |

---

## Data Flow: Script → Voltage (Complete Path)

```
User presses Step 6 twice → cycles "6" → "K"
  → TeletypeScriptViewPage::handleStepKey(step=5)
    → insertChar('K')
    → _editBuffer = "...K..."

User presses encoder to commit
  → commitLine()
    → parse("CV 1 V 60", &cmd, err)     ← scanner (Ragel)
    → validate(&cmd, err)                 ← static analysis
    → run_script(&state, LIVE_SCRIPT)
      → process_command(&state, &es, &cmd)
        → tele_cv(0, voltsToRaw(5V), slew=0)
          → TeletypeBridge::tele_cv()
            → engine->handleCv(0, raw, slew)
              → rawToVolts(raw → 16383 → ±5V)
              → cvOutputDest[0] = CvOut5
              → apply range conversion
              → apply quantize (snap to scale if on)
              → _cvOutputTarget[0] = 5.0V
              → _cvSlewActive[0] = false

Next TeletypeTrackEngine::update(dt):
  → CV interpolation advances
  → _performerCvOutput[trackSlot] = 5.0V

Next Engine::updateTrackOutputs():
  → _cvOutputTracks[physCh] == trackIndex
    → trackIndex == trackSlot
    → _performerCvOutput[trackSlot] = 5.0V
    → _cvOutput.setChannel(physCh, 5.0V)

Next CvOutput::update():
  → calibration.cvOutput(physCh).voltsToValue(5.0V) → DAC code
  → Dac::setValue(physCh, code)
  → Dac::write() → SPI burst to DAC8568
  → Voltage appears on physical jack
```

---

## Critical Architectural Boundaries

1. **Model/Engine split**: `TeletypeTrack` owns persistent data + VM state. `TeletypeTrackEngine` owns runtime state (slew progress, envelope phases, LFO phases). On pattern slot switch, engine state is discarded — only model data persists.

2. **VM has no global state**: `scene_state_t` is passed as pointer everywhere. The bridge uses one-at-a-time singleton `g_activeEngine` per track execution (ScopedEngine RAII). No concurrent VM execution.

3. **I/O routing is two-stage**:
   - Stage 1 (intra-track): CV output index `[0..3]` → `cvOutputDest[i]` selects which physical output (1-8) to target
   - Stage 2 (inter-track): `Engine::updateTrackOutputs()` reads `gateOutputTracks[8]` / `cvOutputTracks[8]` to find which track's slot maps to each physical output

4. **Pattern slot duality**: Slot 0 ↔ Slot 1. Each contains its own I/O config, scripts S3+metro, and pattern data. S0-S2 are shared across both slots. Switching slots saves VM state → old slot, then restores new slot's state → VM. The 2-slot design maps to patterns (pattern 0 = slot 0, pattern 1 = slot 1, pattern 2 = slot 0, etc).

5. **CV output pipeline has 3 processing stages**:
   - Script writes raw value → `handleCv()` applies range + offset + quantize → `_cvOutputTarget[]`
   - `update()` applies slew + interpolation → `_performerCvOutput[]`
   - `Engine::updateTrackOutputs()` routes per physical channel → calibration → DAC

---

## VM Build Config

Single flag in `teletype/src/teletype_config.h` — defaults `TELETYPE_TRACK_LITE=1`, no build system override:

| Config | LITE (default) | Full Teletype |
|--------|----------------|---------------|
| `DELAY_SIZE` | 16 | 64 |
| `REGULAR_SCRIPT_COUNT` | 4 (S0-S3) | 8 (S0-S7) |
| `EDITABLE_SCRIPT_COUNT` | 5 (S0-S3 + M) | 7 (+ INIT) |
| `TOTAL_SCRIPT_COUNT` | 6 (+ DELAY) | 9 (+ DELAY + LIVE) |
| `GRID_BUTTON_COUNT` | 1 | 256 |
| `GRID_FADER_COUNT` | 1 | 64 |
| `CAL_DATA_T` | excluded | included |

LITE mode strips Monome grid/fader support and calibration data to save RAM. Script slots reduce from 8 to 4 user scripts.

### Script slot layout (LITE)
```
S0 ─── script 0 (trigger input TI-TR1)
S1 ─── script 1 (trigger input TI-TR2)
S2 ─── script 2 (trigger input TI-TR3)
S3 ─── script 3 (trigger input TI-TR4, also SlotScriptIndex = pattern slot script)
M  ─── metro script (runs at metro interval)
D  ─── DELAY script (internal, runs delayed commands)
```

---

## Ops: Storage and Access

### Three-layer lookup chain

```
User types "CV 1 V 60"
  │
  ▼
[1] SCANNER (Ragel, match_token.c)
    match_token("CV") → { tag: OP, value: E_OP_CV }
    → value stored in tele_command_t.value[]
  │
  ▼
[2] POINTER TABLE (op.c, compile-time constant)
    const tele_op_t *tele_ops[E_OP__LENGTH] = {
        [E_OP_A]   = &op_A,    // from variables.c
        [E_OP_CV]  = &op_CV,   // from hardware.c
        [E_OP_ADD] = &op_ADD,  // from maths.c
        ...
    };
  │ tele_ops[word_value] → tele_op_t*
  ▼
[3] OP STRUCT + DISPATCH (op.h / teletype.c)
    typedef struct {
        const char *name;
        void (*get)(const void *data, scene_state_t *ss,
                    exec_state_t *es, command_state_t *cs);
        void (*set)(const void *data, scene_state_t *ss,
                    exec_state_t *es, command_state_t *cs);
        uint8_t params;
        bool returns;
        const void *data;
    } tele_op_t;

    process_command():
      if op is leftmost in sub-command → call op->set()  (write)
      else                             → call op->get()  (read)
```

### Construction macros (from `ops/op.h`)

| Macro | Behavior | Example | Generated struct |
|-------|----------|---------|-----------------|
| `MAKE_SIMPLE_VARIABLE_OP(n, m)` | Generic peek/poke via `offsetof(scene_state_t, m)` | `MAKE_SIMPLE_VARIABLE_OP(A, variables.a)` | `{ .get=op_peek_i16, .set=op_poke_i16, .data=(void*)offsetof(variables.a) }` |
| `MAKE_GET_SET_OP(n, g, s, p, r)` | Custom get/set function pointers | `MAKE_GET_SET_OP(CV, op_CV_get, op_CV_set, 0, true)` | Calls through to teletype_io.h → bridge → engine |
| `MAKE_GET_OP(n, g, p, r)` | Read-only (no set) | `MAKE_GET_OP(LAST, op_LAST_get, 1, true)` | `.set = NULL` — writing to this op is an error |

### Performer ↔ VM access — two directions

**Performer writes into VM state** (no bridge needed for simple variables):
```
Engine::updateAdc()
  → ss_set_in(&state, raw)     // direct write to state.variables.in
  → ss_set_param(&state, raw)  // direct write to state.variables.param
  → state.variables.x = raw_x  // direct struct access

Engine::updateInputTriggers()
  → detects rising edge → run_script(i)  // triggers VM execution

TeletypeScriptViewPage::commitLine()
  → parse(text, &cmd) → process_command(&state, &es, &cmd)
    → tele_ops[E_OP_A].set() → op_poke_i16(offsetof(variables.a), ss, ...)
      → *(int16_t*)((char*)ss + offset) = 100  // direct memory write, no bridge
```

**VM calls back into Performer** (hardware ops through bridge):
```
process_command() hits "CV 1 V 60"
  → tele_ops[E_OP_CV].set() in hardware.c
    → op_CV_set(): cs_pop for channel, cs_pop for value
      → tele_cv(ch, raw, slew)    ← extern declared in teletype_io.h
        → TeletypeBridge::tele_cv()  ← extern "C" implemented in TeletypeBridge.cpp
          → g_activeEngine->handleCv(ch, volts, slew)
            → range conversion → quantize → _cvOutputTarget[] → DAC

Same bridge path for:
  tele_tr(), tele_tr_pulse()       → gate outputs
  tele_cv_slew()                   → CV slew active flag
  tele_e_*(), tele_lfo_*()         → envelope/LFO engines
  tele_g_*()                       → Geode engine
  tele_wbpm(), tele_wr()           → tempo/transport
  tele_get_input_state()           → read trigger input pin
  tele_has_delays()                → dashboard flags
```

**Key insight**: Ops that touch real hardware (CV, TR, BUS, IN, PARAM) or drive engine subsystems (E.* envelopes, LFO.*, G.* Geode) go through `teletype_io.h` extern callbacks → `TeletypeBridge` → `TeletypeTrackEngine` method → output buffer → DAC/gate. Ops that only touch state (A, B, +, *, N, V, etc.) use `op_peek_i16`/`op_poke_i16` with `offsetof` for direct struct access — zero indirection, no bridge.

### Enum generation

```mermaid (conceptual)
ops/*.c defines op structs ──► op.c lists pointers in tele_ops[]
                                    │
                                    ▼
                            utils/op_enums.py reads op.c
                                    │
                                    ▼
                            generates op_enum.h (E_OP_* enum)
                                    │
                                    ▼
                            match_token.c uses E_OP_* values
                            in Ragel-generated token matcher
```

The enum must stay in sync with the pointer table order. The comment in `op.c:25` warns: *"If you edit this array, run `utils/op_enums.py` to update `op_enum.h` so the order matches."*

---

## Compiled vs Dead Code — Ops

Of 36 op `.c` files in `teletype/src/ops/`, only **14 are compiled** into the Performer build (listed in `src/apps/sequencer/CMakeLists.txt:159-171`). The remaining **22 files are never linked** — dead code from upstream Teletype.

### Compiled (14 files)

| File | Ops | Category |
|------|-----|----------|
| `variables.c` | A, B, C, D, X, Y, Z, T, DRUNK, FLIP, I, O, TIME, LAST, J, K | Core variables |
| `hardware.c` | CV, CV.OFF, CV.SLEW, CV.GET, CV.CAL | CV output |
| | TR, TR.P, TR.PULSE, TR.TOG, TR.TIME, TR.POL, TR.D, TR.W | Trigger output |
| | IN, IN.SCALE, PARAM, PARAM.SCALE | CV input |
| | E, E.A, E.D, E.T, E.O, E.L, E.R, E.C | Envelopes |
| | LFO.R, LFO.W, LFO.A, LFO.F, LFO.O, LFO.S | LFOs |
| | G.TIME, G.TONE, G.RAMP, G.CURV, G.RUN, G.MODE, G.OUT, G.BAR, G.TUNE, G.VOX, G.VAL, G.* | **Geode engine** |
| | BUS, WBPM, WR, WNG, WNN, RT, PRM, MUTE, STATE, LIVE.*, PRINT | Performer system |
| | II.*, I2C.* | I2C (stubbed — no-op) |
| `maths.c` | ADD, SUB, MUL, DIV, MOD, RAND, AND, OR, comparisons, N, V, HZ, BPM, etc. | Maths |
| `controlflow.c` | IF, ELIF, ELSE, L, W, EVERY, SKIP, PROB, BREAK, SCRIPT, KILL, SCENE, SYNC | Control flow |
| `patterns.c` | P, PN, P.NEXT, P.PREV, P.I, P.L, P.SHUF, P.REV, P.ROT, etc. | Pattern ops |
| `metronome.c` | M, M.ACT, M.RESET | Metro |
| `midi.c` | MI.* | MIDI |
| `delay.c` | DEL, DEL.*, DEL.CLR | Delay mod |
| `init.c` | INIT, INIT.* | Initialization |
| `seed.c` | SEED, RAND.SEED, etc. | RNG seeding |
| `stack.c` | S.ALL, S.POP, S.CLR, S.L | Stack ops |
| `queue.c` | Q ops | Queue |
| `turtle.c` | Turtle graphics | Turtle |
| `op.c` | Pointer tables `tele_ops[]` + `tele_mods[]` | Lookup |

### NOT compiled (22 files — dead code)

| File | Module | Protocol |
|------|--------|----------|
| `justfriends.c` | Mannequins Just Friends | I2C |
| `er301.c` | Orthogonal Devices ER-301 | I2C |
| `crow.c` | Monome Crow | I2C |
| `ansible.c` | Monome Ansible | I2C |
| `earthsea.c` | Monome Earthsea | I2C |
| `meadowphysics.c` | Monome Meadowphysics | I2C |
| `whitewhale.c` | Monome White Whale | I2C |
| `telex.c` | Teletype expansion module | I2C |
| `matrixarchate.c` | Mannequins/Whimsical Raps | I2C |
| `disting.c` | Expert Sleepers Disting | I2C |
| `orca.c` | Orca | I2C |
| `wslash.c` | Whimsical Raps W/ | I2C |
| `wslashdelay.c` | W/ delay | I2C |
| `wslashsynth.c` | W/ synth | I2C |
| `wslashtape.c` | W/ tape | I2C |
| `wslash_shared.c` | W/ shared | I2C |
| `i2c.c` | Generic I2C helpers | I2C |
| `i2c2midi.c` | I2C → MIDI bridge | I2C |
| `grid_ops.c` | Monome Grid | serial |
| `fader.c` | Teletype fader bank | native |

These 22 files exist in the upstream `teletype/` subtree but are excluded by CMakeLists.txt. Their I2C/serial hardware targets (Monome, Mannequins, Expert Sleepers, etc.) don't exist on Performer hardware.

The bridge stubs `tele_ii_tx`/`tele_ii_rx` (in `TeletypeBridge.cpp:599-610`) are no-ops that exist only because the compiled `hardware.c` still has the generic `II.*` and `I2C.*` ops which reference them. Other stubs: `tele_kill()`, `tele_mute()`, `tele_save_calibration()`, `grid_key_press()`, `device_flip()`, `tele_scene()` — all no-ops for linker satisfaction.

---

## `scene_state_t` — Complete Struct Breakdown

One instance per TeletypeTrack, defined in `teletype/src/state.h:277-293`.

```
scene_state_t (one per TeletypeTrack)
├── initializing (bool)                            1 B
├── variables (scene_variables_t)               ~320 B
│   ├─  a, x, b, y, c, z, d, t      8×int16    16 B  ← WARNING: fixed order
│   ├─  j[6], k[6]                  12×int16    24 B  ← per-script loop counters
│   ├─  cv[4], cv_off[4], cv_slew[4]            24 B  ← output raw values
│   ├─  drunk, drunk_max/min/wrap                 8 B
│   ├─  flip, in, m, m_act, mutes[8]            ~26 B
│   ├─  o, o_inc, o_min/max/wrap                10 B
│   ├─  p_n, param                               4 B
│   ├─  q[64], q_n, q_grow          64×int16   132 B  ← circular queue
│   ├─  r_min, r_max, n_scale_bits[16],
│   │   n_scale_root[16]                        68 B  ← note scale data
│   ├─  scene, script_pol[8], time(64bit),
│   │   time_act, tr[4], tr_pol[4], tr_time[4]  44 B
│   ├─  seed, in_range, in_scale,
│   │   param_range, param_scale                ~20 B
│   └─  (fader arrays excluded in LITE)
│
├── patterns[4] (scene_pattern_t)       4×136  = 544 B
│   ├─  idx, len, wrap, start, end     5×int16  10 B
│   └─  val[64]                        64×int16 128 B
│
├── delay (scene_delay_t)          LITE: ~640 B / FULL: ~2.5 KB
│   ├─  commands[DELAY_SIZE]     tele_command_t(20)×16
│   ├─  time[], origin_script[],
│   │   origin_i[], fparam1[], fparam2[]
│   └─  count
│
├── stack_op (scene_stack_op_t)         16×~20  = 320 B
│   └─  commands[16], top
│
├── scripts[6] (scene_script_t)         6×140  = 840 B
│   ├─  l (line count)                         1 B
│   ├─  c[6] (tele_command_t × 6)       6×20  120 B
│   │   ├─  length (1), separator (1)          2 B
│   │   ├─  tag[16], value[16]                32 B
│   │   └─  comment (1)                        1 B
│   ├─  every[6]                              ~12 B
│   └─  last_time (uint32_t)                   4 B
│
├── turtle (scene_turtle_t)                    ~20 B
├── every_last (bool)                           1 B
├── grid (scene_grid_t)
│   LITE: 1× each group/button/fader = ~100 B
│   FULL: ~300 B × 256 buttons = massive (excluded)
├── rand_states (scene_rand_t)              5×~20 = 100 B
│   └─  rand, prob, toss, pattern, drunk
├── cal (cal_data_t)             EXCLUDED in LITE
├── i2c_op_address (int8_t)                    1 B
└── midi (scene_midi_t)                       ~250 B
    ├─  on/off/cc/clk/start/stop/continue_script   7 B
    ├─  last_event_type/channel/note/velocity/cc    6 B
    ├─  on_count + on_channel[10] + note_on[10]
    │   + note_vel[10]                             ~42 B
    ├─  off_count + note_off[10] + off_channel[10] ~22 B
    ├─  cc_count + cc_channel[10] + cn[10]
    │   + cc[10] + clock_div                       ~33 B
    └─  (remaining struct padding)
```

### Field reference

| Field | Type | Purpose |
|-------|------|---------|
| `variables.a..t` | 8×int16 | Core user variables — order-constrained for indexed OP access |
| `variables.j/k[]` | 2×6×int16 | Per-script loop counters (J, K) |
| `variables.cv[]` | 4×int16 | Last raw CV output value written |
| `variables.cv_off[]` | 4×int16 | CV offset per channel |
| `variables.cv_slew[]` | 4×int16 | CV slew time (ms) per channel |
| `variables.tr[]` | 4×int16 | Trigger output state |
| `variables.tr_pol[]` | 4×int16 | Trigger polarity |
| `variables.tr_time[]` | 4×int16 | Trigger pulse width (ms) |
| `variables.in` | int16 | ADC input for `IN` variable |
| `variables.param` | int16 | ADC input for `PARAM` variable |
| `variables.m` | int16 | Metro period (ms) |
| `variables.m_act` | bool | Metro running flag |
| `variables.o` / `o_inc` / etc. | 5×int16 | Oscillator (phasor) state |
| `variables.p_n` | int16 | Pattern selector variable |
| `variables.q[64]` | 64×int16 | Circular queue data |
| `variables.time` | int64 | System time ms (for `TIME`/`LAST`) |
| `variables.n_scale_bits/root[16]` | 16×int16×2 | Note scale bitmasks + roots (for `N.SCALE`/`N.ROOT`) |
| `patterns[].val[64]` | 4×64×int16 | Pattern data values |
| `delay.commands[]` | tele_command_t[16] | DEL queue: commands waiting to fire |
| `scripts[].c[]` | tele_command_t[6]×6 | Parsed script lines (S0-S3 + M + D) |
| `scripts[].every[]` | every_count_t[6] | EVERY/SKIP counters per line |
| `rand_states` | 5×tele_rand_t | Independent PRNGs: rand, prob, toss, pattern, drunk |
| `midi.on/off/cc_script` | int8_t | Script index triggered by MIDI note on/off/CC |
| `midi.on_channel[]` + `note_on[]` + `note_vel[]` | 10×3 | MIDI note-on event buffer |

### Size estimate

**LITE mode per track**: ~3 KB (scripts + delay + patterns dominate)
**8 tracks**: ~24 KB `.bss` — significant portion of the Performer's RAM

The three largest consumers inside `scene_state_t`:
1. `delay.commands[]` + metadata: ~640 B (16 slots × tele_command_t(20) + 16×int16 × 5 fields)
2. `scripts[6].c[6]`: ~840 B (6 scripts × 6 lines × tele_command_t(~20) + every[6] + last_time)
3. `patterns[4].val[64]`: 544 B (4 × 64 × int16)
