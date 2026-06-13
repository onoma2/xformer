# teletype-v2-dialect

## Goal

Define Teletype v2 as a Performer-native Teletype++ dialect: preserve hardware-independent pasted Teletype scripts, keep Performer-added ops, extend CV/TR to 8 outputs, and drop Monome hardware-only legacy as a design driver.

## Key files

- `docs/teletype_v2.md` — dialect contract, keep/drop/redesign inventory, compatibility rule, verification gates.
- `docs/teletype_v2_phase1_plan.md` — Phase 1 execution plan for native program/runtime/output state.
- `src/apps/sequencer/model/TeletypeProgram.h` — v2 native program storage (Step 1, approved).
- `src/apps/sequencer/model/TeletypeRuntime.h` — v2 native runtime state (Step 2, approved).
- `teletype/src/match_token.rl` — current Ragel token source and source-compatibility frontend.
- `teletype/src/ops/op_enum.h` — current runtime op enum.
- `teletype/src/ops/op.c` — current compiled runtime op table.
- `src/apps/sequencer/CMakeLists.txt:156` — current compiled Teletype source list.

## Decisions log

- 2026-05-28: Narrow native `SCRIPT n` support implemented in TT2 only. `runScript()` now pushes/pops `TT2ExecFrame` entries and enforces `TT2_EXEC_DEPTH` with `ExecDepthOverflow`. `SCRIPT` is an op-table handler that receives the current `TeletypeProgram` as transient evaluator context, avoiding any `TT2Runtime`/`TT2Track` size growth. Parent `I` is preserved across child calls, child `I` is isolated, child errors propagate and stop the parent. Explicit exclusions remain: `$`, `$F`, `$L`, function return values, fparams, `DEL`, `DEL.CLR`, `BREAK`, and `KILL`.
- 2026-05-28: Accidental old-engine edits were rejected and removed. TT2 v2 work must not patch `teletype/src/ops/controlflow.c` or `teletype/src/state.c` for native dialect semantics unless explicitly requested as a separate v1/upstream fix.
- 2026-05-28: EVERY/SKIP and `I` ownership fixed. `I` moved to `TT2ExecFrame.i` through `tt2ActiveI()` with a depth assert. EVERY/SKIP use upstream modulo semantics: normalize mod (`0 -> 1`, negative -> positive), `count++`, `count %= mod`, EVERY fires on `count == 0`, SKIP fires on `count != 0`. Counter state is per `(script,line)`.
- 2026-05-27: 6 post-Step-5 findings resolved in one session. Include path fixed after `TeletypeInterpreter.h` + `TeletypeNativeOps.cpp` moved from `model/` to `engine/`. Runtime defaults aligned with upstream `ss_variables_init()` (A=1, B=2, C=3, D=4, M=1000, M.ACT=1, TR.TIME=100, TR.POL=1, etc.). CV write clamped with `normalise_value(0, 16383, 0, val)`. TR write stores boolean (`level != 0`). Pattern init sets `len = 0` (was 64) to match upstream `ss_pattern_init()`. Stack push is bounds-checked. Set-vs-get semantics corrected: leftmost token only sets when `isSetPosition && stackSize >= 1`; otherwise it gets. This fixes `A`, `CV 1`, `M` alone from StackUnderflow, and preserves `B + A 5` where `A` is a getter despite values on the stack.
- 2026-05-27: Step 3 separator/mod execution. `evaluateCommand()` splits at SUB_SEP into segments (left-to-right, fresh stack per segment). PRE_SEP splits segment into prefix+body; IF mod evaluates prefix then conditionally executes body. Unsupported mods rejected before prefix evaluation. IF arity enforced (prefix must produce exactly 1 value). `evaluateSegment()` refactored with explicit op-table parameter to prevent test drift.
- 2026-05-27: Step 2 semantic blockers fixed as one bounded patch. M/M.ACT ownership moved from `metro` struct to `variables.m`/`variables.m_act` to match upstream `ss->variables.m` usage; clamp minimum 2ms. Scale and turtle defaults initialized to match upstream `ss_init()`/`turtle_init()`. `runScript()` clears exec frame with `memset` before use.
- 2026-05-27: `TT2TrackEngine` static_assert fixed from `== 344` to `<= 344` after ARM `sizeof` probe returned 340. Container gate refreshed: `Engine::TrackEngineContainer = 916` (was remembered as 912).
- 2026-05-26: Phase 0 is considered complete after source sanity checks: every parser-visible op/mod spelling is classified, `W.ACT`/`WR.ACT` aliasing is documented, and external hardware families are confirmed not to drive the native dialect.
- 2026-05-26: V2 keeps Ragel/`match_token.rl` as the tokenizer/parser frontend for pasted-script compatibility; the replacement boundary is after parsing, where parsed tokens lower into native Performer command/runtime state.
- 2026-05-26: Compatibility rule is script-level, not project-file-level: hardware-independent pasted Teletype scripts should run as Teletype++, but old Performer project migration and Monome hardware emulation are out of scope.
- 2026-05-26: Phase 1 starts with state ownership only: native program storage, native runtime state, and native output state; no interpreter execution before RAM/ownership are measurable.
- 2026-05-26: V2 program and runtime live in Performer model ownership (`src/apps/sequencer/model/`), not in `teletype/src/`. The C parser may feed them, but must not own v2 state.
- 2026-05-26: V2 follows the current model/engine split. TT2Track (model, SRAM) owns TeletypeProgram + TT2Runtime. TT2TrackEngine (engine, CCMRAM) owns TT2OutputState + timing/plumbing. Engine holds a pointer to TT2Track.
- 2026-05-26: V2 runtime uses full `TT2_COMMAND_MAX_LENGTH` (16) words in stack/delay, not a reduced width. Stack depth 16, delay depth 8 — matching kept language semantics. Runtime command mirrors `TT2Command` layout exactly.
- 2026-05-26: TT2Runtime includes turtle state (32 B) because v2 spec keeps `@*` ops in Core Keep.
- 2026-05-26: Phase 1 wiring shells complete. TT2Track = 4,504 B (under NoteTrack 9,544 B). TT2TrackEngine = 344 B (under TeletypeTrackEngine 912 B). No container wiring touched. No behavior change. STM32 build clean.

## Open questions

- [ ] Exact packed command record width for native program storage — resolved: `TT2_COMMAND_MAX_LENGTH = 16`.
- [ ] Exact pattern storage shape — resolved: 4×64, same as current.
- [ ] `P.CYC` / `PN.CYC` dead implementations — decided: delete; NOT yet executed (still in `teletype/src/ops/patterns.c`). Gated by the "don't patch `teletype/src` for native work without an explicit request" rule, so deferred to a dedicated upstream-cleanup pass.

## Project end state / scope guard

Final state:

- One active Performer Teletype implementation: Teletype++.
- TT2 model owns `TeletypeProgram` + `TT2Runtime`.
- TT2 engine owns `TT2OutputState` + scheduling/output plumbing.
- Existing Ragel parser remains the source-compatibility frontend unless proven blocking.
- Hardware-independent pasted Teletype scripts run through native TT2.
- Performer-added ops and 8-output CV/TR semantics stay.

Replacement boundary:

- Remove active Performer dependence on `scene_state_t`, `TeletypeBridge`, `tele_*` output callbacks, `g_activeEngine`, and old bridge-style `TeletypeTrackEngine` execution after native coverage is sufficient.
- Keep old Teletype source only as parser/input reference and semantic comparison material.
- Do not patch `teletype/src` to make TT2 work unless explicitly doing a separate old-runtime fix.

Scope guard:

- No full upstream Teletype emulator.
- No old project migration layer during dev-stage work.
- No Monome hardware-only compatibility target.
- No new dialect surface just because old Teletype is being deleted.
- Deletion happens only after native TT2 covers the kept language slice, init/metro/trigger/delay scheduling, and a bounded golden-script smoke set.

## Completed steps

- [x] Reframed v2 as a breaking Performer-native dialect, not a compatibility wrapper.
- [x] Added hard compatibility rule for hardware-independent pasted Teletype scripts.
- [x] Classified current op surface into Core Keep, Performer-Added Keep, Redesign/Extend, and Drop/Unsupported.
- [x] Verified all 435 parser op/mod spellings from `match_token.rl` are covered by the spec categories.
- [x] Verified all 434 op enum constants have parser spellings.
- [x] Added `docs/teletype_v2_phase1_plan.md`.
- [x] Step 1: TeletypeProgram — native program storage, 2,374 B, approved.
- [x] Step 2: TT2Runtime — native runtime state, 2,128 B, approved.
- [x] Step 3: TT2OutputState — native output state, 336 B, approved.
- [x] Phase 2 Step 1: Parser contract tests — 23 cases covering numbers, symbols, aliases, mods, CV/TR/M families, W.ACT/WR.ACT aliasing, separators, unsupported hardware rejection, Performer-added keep tokens, MIDI/pattern/turtle/init/control-flow/queue/seed smoke. All green on sim debug and STM32 release.
- [x] Phase 2 Step 2: Lowering — `lowerCommand()` inline in `TeletypeProgram.h` copies `length`/`tag[]`/`value[]` directly, drops `separator`/`comment`, zeros trailing slots, rejects `length > TT2_COMMAND_MAX_LENGTH`. 11 test cases: plain, symbol, PRE_SEP, SUB_SEP, combined, W.ACT/WR.ACT alias, trailing zeroing, length boundary reject, mods+numbers, hex/binary/reversed. All green; STM32 release clean.
- [x] Phase 2 Step 3: Native evaluator skeleton — `TeletypeInterpreter.h` with `evaluateCommand()` stack machine. Right-to-left evaluation, NUMBER/XNUMBER/BNUMBER/RNUMBER push, OP dispatch through caller-provided table. Rejects MOD/PRE_SEP/SUB_SEP. Fake op table in tests: Add, Push42, VarSet, VarGet. 14 test cases: number pushes (decimal/hex/binary/reversed), fake add order, fake push, var set/get, mod/pre-sep/sub-sep rejection, stack underflow, unknown op, empty command, multiple pushes. All green; STM32 release clean.
- [x] Phase 2 Step 4: Minimal native ops — `TeletypeNativeOps.cpp` with real variable/math/output/metro op handlers wired into `tt2NativeOpTable`. `A`/`B`/`X` get/set, `ADD`/`+`, `CV`/`TR` 1..8 with dirty bits and out-of-range errors, `M`/`M.ACT` get/set. Evaluator updated to `isSetPosition` semantics (set at idx==0, get elsewhere). `TT2EvalError` extended with `UnsupportedOp`/`OutOfRange`. 17 test cases: variable set/get, math, CV/TR set/get/dirty/out-of-range, metro, unsupported-op. Skeleton tests (14 cases) still green. STM32 release build clean.
- [x] Phase 2 Step 5: One-script runner — `runScript()` in `TeletypeInterpreter.h` executes lines `0..script.length-1` through `evaluateCommand()`. Validates script index and length bounds, sets `TT2ExecState` context (script_number/line_number/depth), skips zero-length lines, stops on first error and reports line via execution frame. 10 test cases: empty script, single-line CV, multi-line variable+math+CV, context tracking, invalid index, unsupported-op stop with line report, blank-line skip, length-overflow reject, boundary index, eval-error stop. All prior tests still green. STM32 release build clean.
- [x] Post-Step-5 findings fix batch — 6 issues resolved: (1) broken include path after ownership move (`model/` → `engine/`); (2) runtime defaults aligned with upstream; (3) CV clamp to 0..16383; (4) TR boolean normalization; (5) pattern init `len = 0`; (6) stack overflow guard. Set-vs-get semantics refined to `isSetPosition && stackSize >= 1`. All 75 TT2 tests green. STM32 release clean. RAM gate proven.
- [x] Step 2 semantic blockers — M/M.ACT ownership fixed: `opM`/`opMACT` now read/write `variables.m`/`variables.m_act` instead of `metro` struct; `opM` clamps to minimum 2ms. Scale bits initialized to `0x0AB5` and roots to 0 for all 16 entries. Turtle initialized to upstream `turtle_init()` defaults (fence `{0,0,3,63}`, mode `Bump`, heading 180, speed 100, `NO_SCRIPT`). `runScript()` clears full exec frame with `memset` before setting context, preventing stale `if_else_condition`/`breaking`/`fparam*` from prior runs. 81 TT2 tests green.
- [x] Step 3 separator/mod execution — `evaluateCommand()` restructured with `evaluateSegment()` helper. SUB_SEP splits into segments executed left-to-right with isolated stacks. PRE_SEP splits segment into prefix (mod condition) and body. IF mod supported: prefix evaluated, body executes if top-of-stack != 0. Unsupported mods rejected before prefix evaluation. IF arity enforced (`prefix.stackSize == 1`, else `InvalidModArity`). `evaluateSegment()` takes explicit op-table parameter; fake evaluator tests call real helper. 93 TT2 tests green.
- [x] Step 4: ELSE / ELIF — conditional chain state tracks across SUB_SEP segments. IF starts a fresh chain; ELIF/ELSE continue it. First true branch runs, later branches skipped. Empty bodies (`: ;`) supported. Error cases: `OrphanElse`, `OrphanElif`, `DuplicateElse`. 8 new test cases. 108 TT2 tests green. STM32 release clean.
- [x] Step 5: PROB mod — `PROB n: body` executes body with probability n%. Uses per-slot LCG on `TT2RngSlot::Prob` (state initialized to distinct constants in `init()`). `n <= 0` never runs, `n >= 100` always runs. Arity enforced before body side effects. PROB participates in conditional chain state: skipped if prior branch Taken/ElseSeen, sets Taken when body executes, sets Pending when skipped (so ELSE can run). 11 test cases: boundaries, negative, seeded deterministic mid-value, side-effect isolation, arity errors, chain interaction with ELSE and IF. `tt2RngRange()` fixed from division (could return `range`) to modulo `[0, range)` with `range==0` guard. Existing unsupported-mod tests retargeted to EVERY. 119 TT2 tests green. STM32 release clean.
- [x] Step 6: TT2TrackEngine smoke wiring — `runScript()`, `cvOutput()`, `gateOutput()` added to `TT2TrackEngine`. `cvOutput()` converts raw 0..16383 to Performer float volts (-5V..+5V). `gateOutput()` returns boolean from `trLevel`. Test file `TestTeletypeV2TrackEngineSmoke.cpp` with 6 cases: single CV, single TR high/low, multi-CV, no-track noop, voltage bounds. All 99 TT2 tests green. STM32 release build clean; firmware binary unchanged (TT2TrackEngine.h not yet linked into main build).
- [x] Cleanup: `TeletypeInterpreter.h` renamed to `TT2Evaluator.h` + `TT2Runner.h`. Evaluator (segment/command/stack/errors) separated from runner (script iteration, frame context). Comments updated to "native v2 evaluator". No compatibility shim. 108 TT2 tests green. STM32 release clean.
- [x] Step 6-adjacent: Narrow L loop — `E_MOD_L` in `evaluateCommand()` builds body command from remaining tokens and calls `evaluateCommand()` per iteration. `I` variable added to `TT2Variables` with `opI` handler. Reversed bounds (descending), equal bounds (single iter), negative bounds, and nested L (re-entrant `evaluateCommand`) supported. Mod prefix evaluation uses `forceGet` to prevent leftmost-op setter from consuming stack values. 12 test cases: L 1 3, L 0 2 counter, L 3 1 reversed, L 1 arity fail, L 1 3 5 arity fail, nested L, body failure stop, I restore, equal bounds, negative-to-positive, variable bounds. All 131 TT2 tests green. STM32 release clean. `sizeof(TT2Variables)` = 402 → `.data+bss` +2 B; `sizeof(TT2Runtime)` = 2130 → +2 B vs baseline. Size guards relaxed to `<=` for host/ARM portability.
- [x] Step 7: EVERY/SKIP + exec-frame I ownership — `I` moved out of variables into `TT2ExecFrame.i` and is accessed through `tt2ActiveI()`. EVERY/EV/SKIP implemented with upstream modulo semantics, per-script/per-line counters, normalized mod values, side-effect isolation, and bounds checks. 13 EVERY/SKIP test cases cover every-1, every-2 multi-call, every-3, per-script isolation, per-line persistence, init reset, skip-2, opposite phase with EVERY, EV alias, side-effect isolation, arity, negative mod, and zero mod.
- [x] Step 8: Narrow SCRIPT calls — `SCRIPT n` executes child scripts by pushing a new TT2 exec frame through `runScript()`. Parent frame is restored on return, child frame starts with independent `I = 0`, child errors propagate, parent continuation works after successful child calls, and recursive calls stop at `TT2_EXEC_DEPTH` with `ExecDepthOverflow`. Program reference is transient evaluator context, not stored in `TT2Runtime`, so `TT2Track` size remains flat. `$`, `$F`, `$L`, fparams, return values, delay, break, and kill remain out of scope.

## Phase 1 RAM budget

| Component | Size | Location | Gate |
|-----------|------|----------|------|
| Component | Size (live `static_assert`) | Location | Gate |
| TeletypeProgram | ≤ 2,376 B | model (SRAM) | |
| TT2Runtime | ≤ 2,132 B (2,130 measured, post-`I`) | model (SRAM) | |
| **TT2Track** | **== 4,504 B** | model (SRAM) | NoteTrack = 9,544 B |
| TT2OutputState | == 26 B | engine (CCMRAM) | |
| **TT2TrackEngine** | **≤ 944 B** (real subclass since wiring) | engine (CCMRAM) | TeletypeTrackEngine = 944 B |

Numbers reconciled 2026-06-13 to the header `static_assert`s. (Drift fixed: the
step-3 log's "336 B" for TT2OutputState was a mis-measurement — the live struct
is 26 B; TT2Runtime is 2,130 B since the `I` field moved into the exec frame.)

Phase 1 complete. No container wiring touched. No behavior change.

Hard guards when structs are wired:
- `static_assert(sizeof(TT2Track) <= sizeof(NoteTrack))`
- `static_assert(sizeof(TT2TrackEngine) <= sizeof(TeletypeTrackEngine))`

## Next action

Phase 2 sandbox/smoke complete through narrow native SCRIPT calls. Cleanup done.

Next semantic step is narrow delay scheduling.

## Audit (2026-06-13) — faithfulness bugs FIXED

Independent 3-agent audit. Architecture verdict: faithful (no bridge/scene_state/heap; parse→lower→run). Kept-op behaviour bugs all now fixed:

- **DEL.X arg order (count/interval swap), DEL.X/.R/.B operand `<1→1` clamp, smoke-test build regression** — `6b9261e5`.
- **IF/ELIF/ELSE chain → exec frame + standalone PROB** — `2290512b`. Chain flag now on `tt2ActiveIfElse` (exec frame), so standalone `ELSE:`/`ELIF:` lines see the prior IF (multi-line idiom restored). IF resets + sets-on-true; ELIF always evaluates prefix, body gated; ELSE runs if not-taken; no orphan/duplicate errors; only IF resets the flag. PROB decoupled (standalone roll). Mirrors `controlflow.c` exactly; tests updated to upstream behaviour + multi-line cases added.

Remaining audit notes (lower severity): `bootEnabled` ignored (boot script always runs on first tick — debatable, upstream INIT always runs on load); delay depth 8 matches the on-module Teletype build (NOT a bug); `loadScriptText` mis-splits >127-char lines (unreachable with ≤16-token commands). **`reset()` now flushes the delay queue + accumulators (`20a6d919`).**

**Governing contract (reaffirmed 2026-06-13): TT2 = the Teletype language, BEHAVIOUR-IDENTICAL.** Only the architecture changes (native runtime, no bridge, 8 outputs, hardware ops dropped). Every kept op's semantics must match upstream exactly — no native shortcuts. (Already in `docs/teletype_v2.md`: "preserve core Teletype syntax and behavior", "don't rename core ops".)

**Engine wiring — DONE (`44289ed5`, 2026-06-13).** `TrackMode::TeletypeV2` live: `TT2Track` in the Track variant, `TT2TrackEngine` is a real `TrackEngine` subclass (boot-script-on-start + metro-script-every-`clockDivisor` minimal tick), Engine dispatch wired, serialize tag 9 (no version bump). `loadScriptText()` (Ragel→lower→store) + `TestTeletypeV2RealScript` run real script text end-to-end. sim + STM32 release green; 0 new test failures. Full trigger-input + true metro scheduling still pending (this was a minimal vertical slice).

Phase 2b candidates:
- **Delay family — `DEL` / `DEL.CLR` (`4977d8af`) + `DEL.X` / `DEL.R` / `DEL.B` (`59404b30`) DONE.** Faithful: parallel-slot queue, `<1→1` clamp, origin snapshot, ms drain from `update(dt)`, issue-80 busy-slot guard; X=x copies at delay_time·k, R=n copies first-at-1ms then +x, B=i·base per set bit; operand `<1→1` clamp on all three (audit fix). `TestTeletypeV2Delay` 9 cases. **Remaining: `DEL.G` (4-arg geometric)** — needs (a) a stack-exposing mod-prefix helper (`TT2EvalResult` only exposes top+2nd), (b) the exact 4-arg pop order + `x = x·m/d` formula verified against `teletype/src/ops/delay.c` before claiming identical.
- **Delay scheduling (`DEL`, then `DEL.CLR`) — original plan note, faithful ms port.** Upstream model (grounded `teletype/src/ops/delay.c` + `teletype.c:362-419`): parallel-array queue (`TT2DelayEntry`/`TT2DelayQueue` already match — `command` + `time`(ms) + origin script/`I`/fparam), `time==0` = empty slot, delay `<1→1` clamp, drain decrements each slot's ms and fires due bodies via a fresh exec frame with origin restored. **`DEL` delay unit = MILLISECONDS** (identical Teletype). Drive the drain from `TT2TrackEngine::update(dt)` — `dt*1000` is real elapsed ms (the `update(0.f)` recompose call is guarded out), mirroring v1 `advanceTime`. Narrow slice = `DEL n: body` one-shot + `DEL.CLR`; `.X/.R/.G/.B` repeat variants follow.
- `$` / function calls and return values
- **`BREAK` / `BRK` / `KILL` — DONE (`bb1ef2fc`)**: BREAK flags the exec frame, L loop stops + resets; KILL clears stack/metro/delays. Faithful to `controlflow.c`.
- **Metro — DONE (`20a6d919`)**: ms-based, fires METRO script every `M` ms when `M.ACT`, driven from `update(dt)` (`tt2AdvanceMetro`). **Remaining: trigger-input scheduling** — read gate/CV inputs, fire trigger scripts 1..8 on rising edges (needs engine input wiring).
- Add `TT2Track` to `Track` container with `TrackMode::TeletypeV2` or mode-switching strategy
- **LAST:** `E`/`LFO`/`G` → Modulator engine port (deferred to the final stage — see section below)

## Native modules (E / LFO / G) → Modulator engine (DEFERRED — last stage)

**Sequencing: this is the LAST stage of the dialect.** Port the `E`/`LFO`/`G` op
families to the Modulator engine only after the core dialect, scheduling
(DEL/metro/trigger), engine wiring (`TT2TrackEngine` into the Track container),
and old-bridge deletion are all done. Design is captured here; do not start the
port before everything else lands.

The v1 engine self-generates envelope/LFO/Geode curves (`updateEnvelopes`/`updateLfos`/`updateGeode` → `handleCv`). Redesign: these ops **target the existing Modulator engine** instead of producing their own curves. Geode already proves the pattern — its 6 voices render in `GeodeEngine` and land in modulator slots M3-M8 via `setVoiceOutput`.

Decided (2026-06-13 design pass):
- **One shared global modulator pool** (8 slots, `Project`-level), not per-track — dodges the CCMRAM engine-container gate. `CONFIG_MODULATOR_COUNT = 8` ↔ 8 tracks = natural 1:1 (track N → modulator N), cross-addressable by number.
- **Address a modulator by number; the op name predefines the shape.** `E n` = "modulator n shaped envelope" (the op *is* `setShape`); `LFO n` = "shaped LFO". No separate set-shape op. Sub-ops (`E.A`, `LFO.R`, …) write that slot's fields, shape-scoped for free. Keeps upstream `E`/`LFO` spelling (pasted-script compat). Shape is last-writer-wins.
- **Geode is the bank-takeover exception, not a per-slot shape** — a pool-mode where modulator-number = voice (M1 clock, M2 run, M3-8 voices), mutually exclusive like JustF.
- **Delete** the v1 self-generating machines + their `_env*`/`_lfo*`/`_geode*` state + `handleCv` curve/slew + the `tele_env_*`/`tele_lfo_*`/`tele_g_*` bridge callbacks.
- **Depends on engine-wiring** (the Phase 2b item above): the wiring must land already holding a `ModulatorEngine` pointer + this addressing model, so the output topology bakes in correctly.

Open questions (parked):
1. **Authority** — TT vs panel/routing writing the same modulator slot. The 8:8 convention softens contention, doesn't kill it. Tag TT-controlled? Last-writer-wins?
2. **Output topology** — modulator renders to its jack on its own (Geode's model) with TT's `CV` op separate, vs TT *samples* `MOD.VAL n` into `TT2OutputState`. The first needs the engine to hand back `E.R`/`E.C` stage-end events + `MOD.VAL` readback (not emitted today).
3. **Resolution** — modulator output is 0..127; TT ops + `TT2OutputState` speak 0..16383 / ±5V. Pick the reconciliation point.
4. **`LFO.W`** — upstream morph knob vs the modulator's discrete shapes (Sine/Tri/SawUp/SawDown/Square).
5. **Chaos / Spring** — TT-addressable, or panel/routing-only (leaning: out of the dialect).
6. **`E` envelope topology** — TT `E` is **AD + loop (`E.L`) + stage-end gates (`E.R`/`E.C`)**; the modulator's envelope is a gated one-shot **ADSR** (sustain/release, no loop, no events). Add an AD/loop/event envelope shape to the engine, or map `E` lossy onto ADSR? Overlaps #2.

## Phase 2 outline

Exit gate:

- A TT2 script can be parsed, lowered, stored, and executed through a native straight-line interpreter.
- No `scene_state_t`.
- No `tele_*`.
- No `TeletypeBridge`, `ScopedEngine`, or `g_activeEngine`.
- Parser contract tests protect kept source syntax before behavior work.

Step 1: Parser contract tests.

- Feed representative lines through the existing Ragel parser.
- Assert accepted/rejected status.
- Assert token sequence: `length`, `tag[]`, `value[]`, and separator position where relevant.
- Cover numbers, symbols, aliases, mods, `CV`, `TR`, `M`, `W.ACT`, and unsupported hardware tokens.
- No execution.

Step 2: Lowering.

- Convert `tele_command_t` to `TT2Command`.
- Preserve full 16-word command length.
- Strip storage-only fields: `separator`, `comment`.
- Reject commands that cannot fit.
- Add tests for commands with and without separators.
- No execution.

Step 3: Native command evaluator skeleton.

- Implement stack-machine evaluation for one already-lowered `TT2Command`.
- Support `NUMBER` and `OP` dispatch only.
- Use fake op table entries first.
- No mods yet.
- No scripts, delay, loops, or control flow yet.

Step 4: Minimal native ops.

- Variables: start with `A`, `B`, maybe `X`.
- Math: `ADD` or `+`.
- Output: `CV`, `TR`.
- Metro state setters only if trivial: `M`, `M.ACT`.
- Each op writes `TT2Runtime` / `TT2OutputState` directly.

Step 5: One-script runner.

- Execute lines `0..script.length-1`.
- Track active script and line in `TT2ExecState`.
- No nested script calls yet.
- No mods yet.
- No delay yet.

Step 6: TT2TrackEngine smoke wiring.

- Optional only after tests pass.
- Trigger one script manually or through a controlled test hook.
- Verify `CV 1 5000` changes `TT2OutputState`.
- Do not wire full trigger inputs or metro scheduling until the core runner is stable.

Phase 2b, not first slice:

- `IF` / `ELSE`;
- `L` loops;
- delay scheduling;
- nested script calls;
- full trigger input and metro scheduling.

## Notes

- Do not reintroduce slots, `scene_state_t`, `tele_*` callback chains, `TeletypeBridge`, or `g_activeEngine` as v2 architecture.
- `CV` and `TR` remain canonical names; extend them to 1-8 and add `.ALL` forms rather than creating an `OUT.*` namespace.
- STM32 release RAM numbers are the acceptance gate once structs exist.
- TT2Runtime is volatile runtime state, not persisted. Reset deterministically on track init/load.
- Do not move TT2Runtime into TT2TrackEngine. The engine container is the dangerous CCMRAM gate.
- Envelope, LFO, and Geode are not pre-allocated in TT2TrackEngine — decided: they drive the shared Modulator pool, not their own curves. See "Native modules (E / LFO / G) → Modulator engine".
- TT2OutputState is ms-based (float) for pulse/slew timing. CV targets are raw int16_t; performer outputs are float volts.
- I lives in `TT2ExecFrame.i`, not `TT2Variables`; use `tt2ActiveI()` only when an exec frame is active.
