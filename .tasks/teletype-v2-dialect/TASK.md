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
- [ ] Whether `P.CYC` / `PN.CYC` dead implementations should be deleted — resolved: delete.

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

## Phase 1 RAM budget

| Component | Size | Location | Gate |
|-----------|------|----------|------|
| TeletypeProgram | 2,374 B | model (SRAM) | |
| TT2Runtime | 2,128 B | model (SRAM) | |
| **TT2Track** | **4,504 B** | model (SRAM) | NoteTrack = 9,544 B |
| TT2OutputState | 336 B | engine (CCMRAM) | |
| **TT2TrackEngine** | **344 B** | engine (CCMRAM) | TeletypeTrackEngine = 912 B |

Phase 1 complete. No container wiring touched. No behavior change.

Hard guards when structs are wired:
- `static_assert(sizeof(TT2Track) <= sizeof(NoteTrack))`
- `static_assert(sizeof(TT2TrackEngine) <= sizeof(TeletypeTrackEngine))`

## Next action

Phase 2 Step 6: TT2TrackEngine smoke wiring — trigger one script manually or through a controlled test hook. Verify `CV 1 5000` changes `TT2OutputState`. Do not wire full trigger inputs or metro scheduling until the core runner is stable.

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
- Envelope, LFO, and Geode are not pre-allocated in TT2TrackEngine. They may become control ops over the Modulator/engine layer.
- TT2OutputState is ms-based (float) for pulse/slew timing. CV targets are raw int16_t; performer outputs are float volts.
