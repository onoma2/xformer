# Teletype v2: Performer-Native Dialect

## Current Verdict

Teletype v2 is a breaking Performer-native dialect, not a compatibility port.

Old project compatibility is explicitly out of scope. Legacy Teletype file compatibility is explicitly out of scope. `scene_state_t` is not long-term truth. The Monome hardware model is not being emulated. The native dialect is now the live, broad implementation (see Current State); the bridge-heavy `scene_state_t` engine still coexists as `TrackMode::Teletype` and is the deletion target, not the target architecture.

The goal is a native Performer scripting language derived from Teletype:

- keep the musical core;
- keep existing Performer-added ops such as `W.ACT`, `WR.ACT`, and the broader `W*` family;
- keep intentional stripped-down Performer behavior;
- run pasted Teletype scripts when they only use the hardware-independent language core;
- add direct 8-output Performer semantics;
- drop hardware-only legacy;
- delete wrapper paths once the native dialect covers the kept language.

## Current State (2026-06-14)

`TrackMode::TeletypeV2` runs as a native `TT2TrackEngine`: parse (Ragel) → lower → native ops → `TT2OutputState`. No `scene_state_t`, no `tele_*` bridge, no C VM in the runtime. The legacy bridge engine (`TrackMode::Teletype`) still coexists and is the eventual deletion target (Action Plan Phase 5), gated on the native path reaching editor + scheduler parity.

### Done — native, tested, STM32-green
- **Control/timing language (MODs):** IF / ELIF / ELSE, PROB, EVERY / SKIP, L, BREAK / BRK, SCRIPT, KILL, S (stack push), the full DEL family; ms-based metro.
- **Op families:** math (ADD/SUB/MUL/DIV/MOD/MIN/MAX/ABS/SGN + symbols), comparison (EQ/NE/LT/GT/LTE/GTE + symbols), logic (AND/OR/AND3/OR3/AND4/OR4/NZ/EZ), range (LIM/WRAP/WRP/AVG/INR/OUTR/INRI/OUTRI), queue (`Q.*`), stack (`S.*`), pitch (`N`, `V`), and the **full pattern family** (`P`/`PN` — value/bounds/nav/insert-remove/reorder/whole-window arith/query/random).
- **Runtime/model:** variables store, scale tables, RNG slots, delay queue, 8 CV + 8 TR output state, serialized `TeletypeProgram` (6 scripts + 4 patterns).
- **Trigger-input firing:** `TT2TrackEngine` samples 4 trigger inputs each update and fires scripts 0-3 on rising edges; per-input source (CvIn/GateOut/LogicalGate, mirroring the legacy dispatch) defaults to CvIn1-4 — the editing UI for it rides with the deferred editor I/O grid.
- **Output shaping:** native linear ms CV slew (raw<<8 fixed-point, exact interpolation, no sub-LSB stall) + offset; one-shot TR pulse with rest polarity. Per-ms shaping pass in `update(dt)`; `cvOutput()` emits the ramped value. Ops `CV.SLEW`/`CV.OFF`/`TR.POL`/`TR.TIME`/`TR.PULSE`/`TR.P`/`TR.TOG`.
- **Build:** release switched to `-Os` (reclaimed ~352 KB flash; ~350 KB headroom); op breadth no longer flash-bound.

### Remaining — engine mechanics (highest value; survey 2026-06-14)
These change what TT2 *is* — a reactive Eurorack voice, not a self-contained calculator. Engine-level, not op-table. (Trigger-input firing and output shaping are now done; see above.)
- **IN / PARAM inputs (PARTIAL)** — `IN`/`PARAM`/`IN.SCALE`/`PARAM.SCALE` ops + a sampling write from a Performer CV source. State (`in`/`param`/min/max) exists.

### Remaining — language tail (PARTIAL — wire ops onto existing state)
- Variable ops: `O` / `DRUNK` / `FLIP` / `TIME` / `LAST` (read-side mutation/tick source) + plain `A`–`Z`/`J`/`K` getters-setters. `LAST` also needs a per-script last-run timestamp array.
- Function calls: `$F`/`$L`/`$S`, `FR`, `I.1`/`I.2` — exec frames already carry `fparam`/`fresult`.
- Pitch tail: `VN`/`VV`/`HZ`, `SCALE`/`QT` (+ `P.SCALE`, deferred from the pattern batch — needs a scale-quantize helper).
- Randomness ops: `RAND`/`RRAND`/`TOSS` + seeds (rng slots exist).
- Bitwise/shift: `|` `&` `~` `^` `XOR`, `BSET`/`BGET`/`BCLR`/`BTOG`/`BREV`, `RSH`/`LSH`/`RROT`/`LROT`.

### Remaining — editor (plan written)
Repoint the editing UI to TeletypeV2, reusing the working pages. `tele2_ops[]` native op registry + `TT2Command→text` printer (no `tele_ops[]`/`print_command` round-trip), per-line edit, rebind the script + pattern pages TrackMode-aware (live mode → native runner), TopPage routing, a behavioral parity harness (orig C VM vs native runner). Plan: `docs/plans/2026-06-14-001-feat-tt2-editor-repoint-plan.md`. Defers the I/O grid (needs the trigger-input / I/O-routing model above) + file save/load.

### PORT — grid-independent
- **Turtle (`@` ops)** — walks *pattern memory*, not the monome grid; the `TT2Turtle` struct already exists in the runtime. Portable to gridless Performer.

### SKIP — confirmed out (no Performer hardware)
Scenes/`SCENE`, i2c/Telex, grid/Arc, Ansible/WW/Meadow/Earthsea/ORCA, Just Friends/ER-301/Disting/`W/`, Crow, MIDI-query families.

### Recommended order
**Engine mechanics first** (trigger-in ✅ → output-shaping ✅ → IN/PARAM next), *then* editor, *then* the language tail + turtle as breadth, *then* bridge deletion (Phase 5). Rationale: a script you can edit but that can't fire on a gate or slew a CV isn't auditionable as an instrument — the engine mechanics gate the hardware audition more than the editor does.

## Non-Goals

- No full upstream Teletype compatibility.
- No old project migration.
- No `scene_state_t` preservation for save/load compatibility.
- No `tele_*` callback bridge as a permanent architecture.
- No fake Teletype hardware panel hidden behind Performer routing.
- No heap-backed script strings.
- No project version ceremony during dev-stage work unless release prep starts.

## Project End State

The final project state has one active Performer Teletype implementation: Teletype++.

Old Teletype remains useful only as source material for token spelling, parser compatibility, and semantic reference tests. It is not the runtime architecture.

Target ownership:

```text
Performer Teletype++ track
  model:  TeletypeProgram + TT2Runtime
  engine: TT2OutputState + scheduling/output plumbing
  parser: existing Ragel frontend -> native lowering
```

Replacement boundary:

- replace active Performer execution paths based on `scene_state_t`, `TeletypeBridge`, `tele_*` output callbacks, `g_activeEngine`, and old `TeletypeTrackEngine` bridge execution;
- keep Ragel tokenization/parsing as the source-compatibility frontend unless it becomes a proven blocker;
- keep token/op spelling tables only as compatibility input, not as a reason to preserve old runtime shape;
- keep hardware-independent pasted-script compatibility as the user-facing rule;
- do not preserve Monome hardware-only behavior or old Performer project files during dev-stage work.

Deletion is allowed only after the native path covers the kept language slice, has scheduler coverage for init/metro/trigger/delay, and passes a bounded golden-script smoke set. Deletion is not a license to add new dialect surface.

## Scope Guard

Do not expand this work into a second Teletype product, a full upstream emulator, or an old-project migration layer.

The path is:

```text
native TT2 covers kept language
  -> wire TT2 as the Performer Teletype track
  -> run golden pasted-script smoke set
  -> remove old bridge/runtime from active track path
```

Anything outside that path needs a separate task.

## Hard Compatibility Rule

Scripts pasted from elsewhere should run as Teletype++ when they do not require Monome Teletype hardware.

This means the dialect should accept ordinary hardware-independent Teletype script idioms for variables, math, conditionals, script calls, metro, delays, patterns, random, CV, and trigger behavior. It does not mean preserving old Performer project files, Teletype scene files, I2C devices, Grid, Arc, Crow, Ansible, ER-301, or any other absent hardware surface.

If a script uses only the core language, it should work. If it addresses unavailable hardware, it should fail clearly or map only through an explicitly designed Performer-native replacement.

Do not rename core ops just to make the architecture feel cleaner. Preserve core Teletype syntax and behavior, then extend it where Performer has more surface.

## What Must Stay

Core language concepts:

- scripts;
- variables;
- math;
- conditionals;
- script calls;
- metro and timing;
- delays;
- patterns;
- trigger output;
- CV output;
- pulse behavior;
- random state where musically relevant.

Performer-specific behavior:

- current Performer-added ops, including `W*` transport/time/pattern/track-note ops;
- current stripped op set where Teletype hardware features were intentionally removed;
- Performer routing and output model;
- direct interaction with Performer tracks where already added.

## What Must Change

### State

Replace `scene_state_t` as the model/runtime truth.

Persistent state should be a compact native program:

- script command storage;
- pattern data;
- dialect version;
- boot script;
- saved timing/config parameters;
- saved output/routing intent.

Runtime state should be a compact native runtime:

- variables;
- stack;
- delay queue;
- metro state;
- random state;
- active script context;
- 8 CV output state;
- 8 trigger output state;
- native module state for envelopes, LFOs, Geode, and future Performer-native blocks.

### Execution

Use a native op table for the Performer dialect.

`CV 1 5000` should reach output state directly. It must not travel through `tele_cv()`, `TeletypeBridge`, `g_activeEngine`, and helper layers before touching the actual track output.

Ragel / `match_token.rl` may remain the tokenizer/parser frontend for v2. It already encodes Teletype spelling, aliases, number formats, symbols, mods, and pasted-script syntax. Rewriting tokenization is not a requirement.

The boundary is after parsing. Parsed Teletype tokens must be lowered into a native Performer command representation and executed by native runtime code.

The execution path should look like:

```text
source text -> Ragel tokens / parsed command -> native command IR -> native op -> TeletypeRuntime / output state
```

Not:

```text
parsed command -> C VM op -> tele_* callback -> bridge -> active engine -> helper -> output
```

`tele_command_t` may be used as a temporary parsed input format if that keeps pasted-script compatibility cheap. `tele_ops[]`, `scene_state_t`, `tele_*` callbacks, and `TeletypeBridge` are not v2 architecture.

### Outputs

The dialect is Performer-native and can address all 8 CV outputs and all 8 trigger outputs.

Existing hardware-independent Teletype output scripts must keep their meaning:

```text
CV 1 5000
CV 4 5000
CV.SLEW 1 100
CV.OFF 2 1000
TR 1 1
TR.P 1
TR.TOG 2
TR.TIME 1 50
TR.POL 1 1
```

Performer extends the same surface to outputs 5-8:

```text
CV 5 5000
CV 8 5000
CV.SLEW 8 100
CV.OFF 8 1000
TR 5 1
TR.P 8
TR.TOG 8
TR.TIME 8 50
TR.POL 8 1
```

Performer also adds all-output forms:

```text
CV.ALL 0
CV.SLEW.ALL 100
CV.OFF.ALL 0
TR.ALL 0
TR.ALL.P
TR.ALL.TOG
TR.TIME.ALL 50
TR.POL.ALL 1
```

No separate `OUT.CV` or `OUT.TR` namespace. `CV` and `TR` remain canonical.

Hardware-specific families are not part of core compatibility:

```text
II / I2C
GRID
ARC
CROW
ANSIBLE
TELEX
ER-301
upstream scene/file ops that imply Monome hardware or scene files
```

These should fail clearly or be redesigned later as explicit Performer-native replacements.

## Conversion Inventory

Source references:

- Parser-visible tokens: `teletype/src/match_token.rl`.
- Runtime op enum: `teletype/src/ops/op_enum.h`.
- Runtime op table: `teletype/src/ops/op.c`.
- Individual op implementations: `teletype/src/ops/*.c`.

`match_token.rl` is allowed to remain as the lexical/source-compatibility frontend. The conversion target is the op execution path, not token spelling.

There is no current `W.RUN` token or op in source. Current Performer run-control style ops are `W.ACT` / `WR.ACT`, with `WR` readback.

### Core Keep

Keep these as native Teletype++ ops because hardware-independent pasted Teletype scripts depend on them.

- Variables/state: `A`, `B`, `C`, `D`, `X`, `Y`, `Z`, `I`, `J`, `K`, `O`, `O.INC`, `O.MAX`, `O.MIN`, `O.WRAP`, `T`, `TIME`, `TIME.ACT`, `LAST`, `DRUNK`, `DRUNK.MAX`, `DRUNK.MIN`, `DRUNK.WRAP`, `FLIP`.
- Init: `INIT`, `INIT.SCRIPT`, `INIT.SCRIPT.ALL`, `INIT.P`, `INIT.P.ALL`, `INIT.CV`, `INIT.CV.ALL`, `INIT.TR`, `INIT.TR.ALL`, `INIT.DATA`, `INIT.TIME`.
- Turtle: `@`, `@X`, `@Y`, `@MOVE`, `@F`, `@FX1`, `@FY1`, `@FX2`, `@FY2`, `@SPEED`, `@DIR`, `@STEP`, `@BUMP`, `@WRAP`, `@BOUNCE`, `@SCRIPT`, `@SHOW`.
- Metro: `M`, `M!`, `M.ACT`, `M.A`, `M.ACT.A`, `M.RESET`, `M.RESET.A`.
- Patterns: `P`, `PN`, `P.N`, `P.L`, `PN.L`, `P.WRAP`, `PN.WRAP`, `P.START`, `PN.START`, `P.END`, `PN.END`, `P.I`, `PN.I`, `P.HERE`, `PN.HERE`, `P.NEXT`, `PN.NEXT`, `P.PREV`, `PN.PREV`, `P.INS`, `PN.INS`, `P.RM`, `PN.RM`, `P.PUSH`, `PN.PUSH`, `P.POP`, `PN.POP`, `P.MIN`, `PN.MIN`, `P.MAX`, `PN.MAX`, `P.SHUF`, `PN.SHUF`, `P.REV`, `PN.REV`, `P.ROT`, `PN.ROT`, `P.RND`, `PN.RND`, `P.+`, `PN.+`, `P.-`, `PN.-`, `P.+W`, `PN.+W`, `P.-W`, `PN.-W`, `P.PA`, `PN.PA`, `P.PS`, `PN.PS`, `P.PM`, `PN.PM`, `P.PD`, `PN.PD`, `P.PMOD`, `PN.PMOD`, `P.SCALE`, `PN.SCALE`, `P.SUM`, `PN.SUM`, `P.AVG`, `PN.AVG`, `P.MINV`, `PN.MINV`, `P.MAXV`, `PN.MAXV`, `P.FND`, `PN.FND`, `RND.P`, `RND.PN`.
- Queue: `Q`, `Q.AVG`, `Q.N`, `Q.CLR`, `Q.GRW`, `Q.SUM`, `Q.MIN`, `Q.MAX`, `Q.RND`, `Q.SRT`, `Q.REV`, `Q.SH`, `Q.ADD`, `Q.SUB`, `Q.MUL`, `Q.DIV`, `Q.MOD`, `Q.I`, `Q.2P`, `Q.P2`.
- Math/random/pitch/logic: `ADD`, `SUB`, `MUL`, `DIV`, `MOD`, `RAND`, `RND`, `RRAND`, `RRND`, `R`, `R.MIN`, `R.MAX`, `TOSS`, `MIN`, `MAX`, `LIM`, `WRAP`, `WRP`, `QT`, `QT.S`, `QT.CS`, `QT.B`, `QT.BX`, `AVG`, `EQ`, `NE`, `LT`, `GT`, `LTE`, `GTE`, `INR`, `OUTR`, `INRI`, `OUTRI`, `NZ`, `EZ`, `RSH`, `LSH`, `RROT`, `LROT`, `EXP`, `ABS`, `SGN`, `AND`, `OR`, `AND3`, `OR3`, `AND4`, `OR4`, `JI`, `SCALE`, `SCL`, `SCALE0`, `SCL0`, `N`, `VN`, `HZ`, `N.S`, `N.C`, `N.CS`, `N.B`, `N.BX`, `V`, `VV`, `ER`, `NR`, `DR.T`, `DR.P`, `DR.V`, `BPM`, `|`, `&`, `~`, `^`, `BSET`, `BGET`, `BCLR`, `BTOG`, `BREV`, `XOR`, `CHAOS`, `CHAOS.R`, `CHAOS.ALG`, `?`, `+`, `-`, `*`, `/`, `%`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `><`, `<>`, `>=<`, `<=>`, `!`, `<<`, `>>`, `<<<`, `>>>`, `&&`, `||`, `&&&`, `|||`, `&&&&`, `||||`.
- Stack/control/delay/seed: `S.ALL`, `S.POP`, `S.CLR`, `S.L`, `SCRIPT`, `$`, `SCRIPT.POL`, `$.POL`, `KILL`, `BREAK`, `BRK`, `SYNC`, `$F`, `$F1`, `$F2`, `$L`, `$L1`, `$L2`, `$S`, `$S1`, `$S2`, `I1`, `I2`, `FR`, `DEL.CLR`, `SEED`, `RAND.SEED`, `RAND.SD`, `R.SD`, `TOSS.SEED`, `TOSS.SD`, `PROB.SEED`, `PROB.SD`, `DRUNK.SEED`, `DRUNK.SD`, `P.SEED`, `P.SD`.
- Mods: `IF`, `ELIF`, `ELSE`, `L`, `W`, `EVERY`, `EV`, `SKIP`, `OTHER`, `PROB`, `DEL`, `DEL.X`, `DEL.R`, `DEL.G`, `DEL.B`, `P.MAP`, `PN.MAP`, `S`.

### Performer-Added Keep

Keep these, but implement them as direct native calls into Performer state or native runtime modules.

- `W*` transport/time/pattern/track-note ops: `WBPM`, `WBPM.S`, `WMS`, `WTU`, `BAR`, `WP`, `WP.SET`, `W.ACT`, `WR.ACT`, `WNG.H`, `WNN.H`, `WNG`, `WNN`, `WR`, `RT`.
- Performer routing/readback: `BUS`, `PRM`, `MUTE`, `STATE`, `LIVE.OFF`, `LIVE.O`, `LIVE.DASH`, `LIVE.D`, `LIVE.VARS`, `LIVE.V`, `PRINT`, `PRT`.
- Native modules: `E`, `E.A`, `E.D`, `E.T`, `E.O`, `E.L`, `E.R`, `E.C`; `LFO.R`, `LFO.W`, `LFO.A`, `LFO.F`, `LFO.O`, `LFO.S`; `G.TIME`, `G.TONE`, `G.RAMP`, `G.CURV`, `G.RUN`, `G.MODE`, `G.O`, `G.BAR`, `G.TUNE`, `G.V`, `G.VAL`, `G.R`, `G.T`, `G.I`, `G.RA`, `G.C`, `G.N`, `G.M`, `G.B`, `G.L`, `G.S`.
- MIDI query state: `MI.$`, `MI.LE`, `MI.LN`, `MI.LNV`, `MI.LV`, `MI.LVV`, `MI.LO`, `MI.LC`, `MI.LCC`, `MI.LCCV`, `MI.NL`, `MI.N`, `MI.NV`, `MI.V`, `MI.VV`, `MI.OL`, `MI.O`, `MI.CL`, `MI.C`, `MI.CC`, `MI.CCV`, `MI.LCH`, `MI.NCH`, `MI.OCH`, `MI.CCH`, `MI.CLKD`, `MI.CLKR`.

### Redesign / Extend

Keep these names and preserve 1-4 behavior. Extend them to 1-8 and remove bridge callbacks.

- CV family: `CV`, `CV.OFF`, `CV.SLEW`, `CV.GET`, `CV.SET`, `CV.CAL`, `CV.CAL.RESET`.
- TR family: `TR`, `TR.D`, `TR.W`, `TR.POL`, `TR.TIME`, `TR.TOG`, `TR.PULSE`, `TR.P`.
- Input family: `IN`, `IN.SCALE`, `IN.CAL.MIN`, `IN.CAL.MAX`, `IN.CAL.RESET`, `PARAM`, `PARAM.SCALE`, `PARAM.CAL.MIN`, `PARAM.CAL.MAX`, `PARAM.CAL.RESET`.
- New all-output forms: `CV.ALL`, `CV.SLEW.ALL`, `CV.OFF.ALL`, `TR.ALL`, `TR.ALL.P`, `TR.ALL.TOG`, `TR.TIME.ALL`, `TR.POL.ALL`.

### Drop / Explicit Unsupported

These are not part of the hardware-independent pasted-script contract. Drop them from v2 unless they are later redesigned as explicit Performer-native features.

- Parser-visible scene/hardware surface: `INIT.SCENE`, `SCENE`, `SCENE.G`, `SCENE.P`, `LIVE.GRID`, `LIVE.G`.
- Raw ii/I2C family: `IIA`, `IIS*`, `IIQ*`, `IIB*`.
- External device families present in source: Ansible/Kria/Meadow/Levels/Cycles/Arp, White Whale, Meadowphysics, Earthsea, Just Friends, Telex `TO.*` / `TI.*`, Crow, ER-301 `SC.*`, Disting EX, ORCA, I2C2MIDI, Grid, Fader, Matrixarchate, W/ audio module families (`W/`, `W/S`, `W/D`, `W/T`).

## Migration And Test Strategy

TeletypeScript is a language. Exhaustive per-op migration proof is not practical. Do not try to write 1000 tests before cutting over.

V2 should use a contract test pyramid:

1. **Parser contract tests.**
   Feed script lines through the current Ragel parser and v2 lowering. Assert the same accepted/rejected status and the same parsed token sequence for kept syntax. This protects spelling, aliases, symbols, numbers, and modifiers.

2. **Core golden behavior tests.**
   Write representative behavior tests for language classes, not every op:
   - variables and assignment;
   - math and symbol aliases;
   - comparisons and conditionals;
   - loop/modifier behavior: `L`, `W`, `EVERY`, `PROB`;
   - script calls: `SCRIPT`, `$`, `BREAK`;
   - delay: `DEL`, `DEL.CLR`;
   - metro: `M`, `M.ACT`, `M.RESET`;
   - patterns and basic mutation;
   - queue and stack smoke paths;
   - deterministic random/seed paths;
   - CV/TR compatibility on 1-4;
   - CV/TR extension on 5-8 and `.ALL`.

3. **Script corpus tests.**
   Build a small corpus of real scripts:
   - current default scripts;
   - pasted hardware-independent Teletype examples;
   - user scripts worth protecting;
   - edge-case scripts from docs.

   Run corpus scripts through the old reference path and the v2 native runtime where both should apply. Compare output traces: CV writes, trigger writes, variable changes, pattern state, metro/delay events, and errors.

4. **Unsupported hardware tests.**
   For dropped hardware families, assert clear unsupported errors. Examples: `IIA`, `GRID`, `CROW`, `TO.CV`, `SC.CV`.

Leap-of-faith boundary:

- Guarantee parser compatibility for kept hardware-independent syntax.
- Guarantee representative golden behavior for core language classes.
- Guarantee corpus behavior for scripts we actually care about.
- Do not guarantee every obscure upstream op until it has a test or a user report.

The cutover gate is confidence from contracts and corpus traces, not exhaustive proof.

## Keep From Prior Reviews

Useful suggestions that still apply:

1. **Inventory the op set first.**
   Classify every current op as:
   - core keep;
   - Performer-added keep;
   - hardware legacy drop;
   - redesign for 8-output Performer semantics.

2. **Use fixed-size storage.**
   No `std::string` script storage, no heap-backed command buffers, no dynamic per-op allocation.

3. **Measure RAM on STM32 release builds.**
   Native state must respect the existing model/engine container gates. Any runtime moved into the engine can multiply across 8 engine slots.

4. **Keep the old implementation only as a temporary reference while replacing it.**
   It may help compare behavior for kept core ops, but it is not a compatibility layer and not a permanent fallback.

5. **Extract native modules where they clarify ownership.**
   Geode, envelopes, LFOs, and pulse handling should become native modules owned by the new runtime, not bridge side effects.

6. **Verify on hardware.**
   Build success and simulator checks are not enough for this track. Hardware boot, timing, CV output, trigger output, and audible behavior are the gate.

## Action Plan

**Status (2026-06-14) — see Current State for the live roadmap.** Phase 0 (dialect definition) ✅, Phase 1 (native data model) ✅, Phase 2 (native core interpreter) ✅ **and well beyond** (the full op surface + control-flow MODs + patterns are native and tested). Phase 3 (native output semantics) ✅ — instant `CV`/`TR` plus slew / pulse / offset / polarity (the "output shaping" engine) are native and tested. Phase 4 (native modules: envelopes/LFO/Geode) **not started** (lower priority). Phase 5 (delete bridge) **pending**, gated on native editor + scheduler (trigger/input) parity. The phase definitions below stay as the architectural frame; the prioritized remaining work lives in Current State.

### Phase 0: Dialect Definition

- Inventory existing Teletype ops and Performer-added ops.
- Mark each op as core keep, Performer-added keep, hardware legacy drop, or Performer-native redesign.
- Preserve core Teletype `CV`/`TR` names and behavior for outputs 1-4.
- Extend `CV`/`TR` names to outputs 5-8.
- Define `.ALL` forms for CV/TR family ops.
- Define which hardware-only concepts are gone.

### Phase 1: Native Data Model

Execution plan: `docs/teletype_v2_phase1_plan.md`.

- Add native fixed-size script/command storage.
- Add native pattern storage.
- Add native runtime state.
- Add 8 CV + 8 trigger output state.
- Add size probes for model/runtime structs.

### Phase 2: Native Core Interpreter

- Implement native execution for the smallest useful core:
  - Ragel/parsed-token lowering into native commands;
  - literals;
  - variables;
  - math;
  - conditionals;
  - script calls;
  - `CV`;
  - `TR`;
  - `M`;
  - `DEL`;
  - kept Performer ops such as `W.ACT`, `WR.ACT`, and the broader `W*` family.

### Phase 3: Native Output Semantics

- Implement direct 8-output CV/TR ops.
- Define output ordering and interaction with Performer routing.
- Preserve pasted script behavior for hardware-independent `CV 1..4` and `TR 1..4` idioms.
- Remove fake Teletype hardware assumptions from the execution path.

### Phase 4: Native Modules

- Move envelope behavior into native runtime ownership.
- Move LFO behavior into native runtime ownership.
- Move Geode into a native module.
- Keep module APIs direct and local.

### Phase 5: Delete Bridge Architecture

- Remove `g_activeEngine`.
- Remove `ScopedEngine`.
- Remove permanent `tele_*` forwarding.
- Remove remaining C VM dependency for the kept dialect subset.

## Verification Gates

- STM32 release build passes: `cd build/stm32/release && make sequencer`.
- `.data + .bss` and `.ccmram_bss` measured after each phase.
- Native core op tests pass.
- Parser contract tests pass for kept syntax.
- Script corpus trace tests pass for protected scripts.
- Unsupported hardware ops fail clearly.
- 8-output CV/TR scripts work on hardware.
- Metro and delay behavior verified on hardware.
- Existing Performer-added ops such as `W.ACT`, `WR.ACT`, and the broader `W*` family verified.
- No heap added to the runtime path.

## Design Rule

If an implementation choice exists only to preserve old Teletype projects or upstream Monome hardware compatibility, reject it.

If an implementation choice makes the dialect more direct, more Performer-native, and still keeps the musical core, prefer it.
