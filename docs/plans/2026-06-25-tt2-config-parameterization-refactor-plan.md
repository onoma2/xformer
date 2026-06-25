# TT2 Config Parameterization Refactor — TDD Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Parameterize the Teletype-V2 core on a compile-time config traits struct, instantiated once (`Full`), behavior- and byte-identical to today, so a Mini variant can be added later.

**Spec:** `docs/superpowers/specs/2026-06-25-tt2-config-parameterization-refactor.md` (gate-clean). This plan is its bite-sized TDD execution. Read the spec's work-items + acceptance before starting; this plan does not re-list the exhaustive grep site-lists.

**TDD shape for a behavior-preserving refactor:** the existing `TestTeletypeV2*`/`TestTT2*` suite is the **characterization test** — it must stay green at every commit, that is the red/green signal for the templating phases. New `assert`-based tests are written first only for genuinely new units (config traits, the `tt2OpTable` trait, the Seam-1 accessors, the Seam-2 interface). `sizeof` static_asserts + a grep gate are the structural invariants.

**Build/test (user's local machine only):** unit tests `make -C build sequencer && ctest --test-dir build` (top-level `build/`; `ctest` needs `--test-dir build` or a `cd build` first — bare `ctest` from repo root looks in the source dir); sim `make -C build/sim/debug sequencer`; hardware gate `make -C build/stm32/release sequencer`. Each new test registers in `src/tests/unit/sequencer/CMakeLists.txt` via `register_sequencer_test(...)`.

**Language:** C++11. Function templates + alias templates + explicit specialization only — no variable templates / `if constexpr` / `auto`-return / generic lambdas.

**Safe commit boundaries (feasibility-gated):** each phase below compiles as a whole because the `using TeletypeProgram = …<Full>` aliases keep every consumer seeing the concrete Full type. Never drop an alias before its consumers are converted.

---

## Phase 0 — Config traits

### Task 0a: capture sizeof baselines (do this FIRST, before any templating)
**Why:** the byte-identity gate asserts `sizeof(...) == N`; `N` must be read off the *pre-refactor* tree, and the existing header asserts are `<=` ceilings (`TeletypeProgram.h:175`, `TeletypeRuntime.h:346`, `TT2TrackEngine.h:217`) which do NOT prove identity. `TT2Track` is already `== 9520` (`TT2Track.h:46`).
1. Add a throwaway `static_assert(sizeof(X) == 1, "")` probe for each of `TeletypeProgram`, `TT2Runtime`, `TT2Track`, compiled against the current tree; read the real N from each compiler error. **Record the three POD numbers and replace the `<current sizeof>` placeholders in Task 1 / Task 2 with the literals before executing those tasks** — the plan is not executable until these are filled.
2. **Engine sizes are NOT captured as host `static_assert`s** — `TT2TrackEngine`/`Container` embed pointers/vtables, so `sizeof` differs host (8-byte ptr) vs ARM (4-byte ptr). The engine "no new vptr / unchanged size" check is the **ARM `.map`** gate only (Task 12 step 6), not a host assert. Record the current ARM `.map` engine/container figures as the baseline.
3. No commit (probes are throwaway) — these numbers feed Tasks 1/2/12.

### Task 0: `TT2Config.h`
**Files:** Create `src/apps/sequencer/model/TT2Config.h`; Test `src/tests/unit/sequencer/TestTT2Config.cpp` (register in CMakeLists).

1. **Failing test:** assert `TT2ConfigFull::{ScriptCount==10, DelayDepth==64, TriggerInputCount==8, MetroScript==8, InitScript==9, SceneCount==1}`.
2. Register test + run → fails (no header).
3. Implement `struct TT2ConfigFull` with the six `static constexpr int` members (spec "config model").
4. Run → PASS.
5. **Commit:** `feat(tt2): add TT2ConfigFull traits struct`

---

## Phase 1 — Template `TeletypeProgram` (alias Full)

### Task 1: templatize the program struct
**Files:** `model/TeletypeProgram.h`.

1. **Failing test** (`TestTT2Config.cpp`, extend): `static_assert(sizeof(TeletypeProgramT<TT2ConfigFull>) == 3638, "")` (host baseline from Task 0a). Also `expect(std::is_trivially_copyable<TeletypeProgramT<TT2ConfigFull>>::value)`.
2. Run → fails (type not templated).
3. Implement: `template<typename Cfg> struct TeletypeProgramT`. **Only the two arrays sized by a varying value become `Cfg::`** — `scripts[Cfg::ScriptCount]` and `triggerSource[Cfg::TriggerInputCount]`. Everything else keeps its fixed constant: `patterns[TT2_PATTERN_COUNT]`, `cvInputSource[TT2_CV_INPUT_COUNT]`, `cvOutput*[TT2_CV_OUTPUT_COUNT]`, and `TT2Script`/`TT2Command`/`TT2Pattern` stay **non-templated** (their dims — commands-per-script 6, pattern length 64 — are fixed, not among the six). `init(...)` → free function template (global namespace, ADL). Boot defaults honor `Cfg::InitScript < 0`. Add alias `using TeletypeProgram = TeletypeProgramT<TT2ConfigFull>;` + the existing `sizeof <= 3640` static_assert via the alias. Keep `TT2_SCRIPT_COUNT` etc. as **permanent** `= TT2ConfigFull::X` compat aliases.
4. Run full suite + new asserts → PASS, sizeof identical.
5. **Commit:** `refactor(tt2): template TeletypeProgram on config, alias Full`

---

## Phase 2 — Template `TT2Runtime` (incl. inline helpers)

### Task 2: templatize the runtime struct + its inline helpers
**Files:** `model/TeletypeRuntime.h`.

1. **Failing test:** `static_assert(sizeof(TT2RuntimeT<TT2ConfigFull>) == 5880, "")` (host baseline from Task 0a) + trivially-copyable.
2. Run → fails.
3. Implement: `template<typename Cfg> struct TT2RuntimeT`. **Templated sub-structs (each holds a Cfg-varying array): `TT2VariablesT<Cfg>`** (`j[Cfg::ScriptCount]`, `k[Cfg::ScriptCount]`, `script_pol[Cfg::TriggerInputCount]` — all its other arrays `cv[8]/q[64]/n_scale[16]/tr[8]/dashboard[16]` keep fixed constants), **`TT2DelayQueueT<Cfg>`** (`entries[Cfg::DelayDepth]`), **`TT2EveryStateT<Cfg>`** (`every[Cfg::ScriptCount][TT2_COMMANDS_PER_SCRIPT]`). `TT2RuntimeT<Cfg>` also carries `scriptLastMs[Cfg::ScriptCount]`. **Stay NON-templated (fixed dims):** `TT2Stack` (StackDepth=16), `TT2ExecState` (ExecDepth=8), `TT2Metro`, `TT2Rng`, `TT2Turtle`, `TT2Midi`, `TT2DelayEntry`, `TT2RuntimeCommand`, `TT2EveryCount`. **Helper rule:** a free/inline helper becomes `template<Cfg>` iff its parameter type is now templated — i.e. takes `TT2RuntimeT<Cfg>&`/`TT2VariablesT<Cfg>&`/`TT2DelayQueueT<Cfg>&`/`TT2EveryStateT<Cfg>&` (so `init`, `tt2InitVariables`, `tt2DelayAdd` `:395`, `tt2DelayClear` `:412`, and the `tt2Active*` accessors that take `TT2Runtime&` all template). Helpers taking only a plain sub-struct (`tt2RngNext/Range` on `TT2Rng&`, `tt2Midi*` on `TT2Midi&`, `tt2Every*` on `TT2EveryCount&`) **stay plain**. Alias `using TT2Runtime = TT2RuntimeT<TT2ConfigFull>;` + keep existing `<= 5900` assert.
4. Run full suite → PASS, sizeof identical. (`TestTeletypeV2Delay` exercises the templated delay helpers.)
5. **Commit:** `refactor(tt2): template TT2Runtime + inline helpers on config`

---

## Phase 3 — Template runner + evaluator + the `tt2OpTable` trait

### Task 3: `TT2Runner.h`
**Files:** `engine/TT2Runner.h`. (`runStoredCommand` is NOT here — it lives in `TeletypeNativeOps.cpp`, Task 5.)
1. Templatize the five inline functions on `Cfg`, each taking `const TeletypeProgramT<Cfg>&`, `TT2RuntimeT<Cfg>&`, `TT2OutputState&`: `runScript`, `tt2RunFunction`, `tt2RunFunctionLine`, `tt2AdvanceDelays`, `tt2AdvanceMetro`. (`tt2AdvanceMetro<Cfg>` calls `runScript<Cfg>` — both templated.)
2. Swap **only the varying constants** to `Cfg::`: `TT2_SCRIPT_COUNT`→`Cfg::ScriptCount` (`:16,:63,:93`), `TT2_DELAY_DEPTH`→`Cfg::DelayDepth` (`:126`), `TT2_METRO_SCRIPT`→`Cfg::MetroScript` (`:184,:201`). **Keep fixed constants:** `TT2_COMMANDS_PER_SCRIPT` (`:21,:65`), `TT2_EXEC_DEPTH` (`:25,:66,:96,:144`), `TT2_COMMAND_MAX_LENGTH` (`:157`).
3. **Do NOT reference `tt2OpTable` and do NOT add boot/init logic** — the runner has neither. Keep the `evaluateCommand(cmd, runtime, output, &program)` calls exactly as-is; for Full they bind to today's non-template `evaluateCommand`, and become the templated overload after Task 4.
4. Run suite (`TestTeletypeV2{Metro,Delay,ScriptRunner,Function}` exercise these five) → PASS. **Commit:** `refactor(tt2): template TT2Runner functions on config`

### Task 4: `TT2Evaluator.h` — op-func type + trait decl + Full overload
**Files:** `engine/TT2Evaluator.h` (declarations), `engine/TeletypeNativeOps.cpp` (stub definition).
1. **Failing test** (`TestTT2Config.cpp` or a new `TestTT2OpTable.cpp`): `expect(tt2OpTable<TT2ConfigFull>() == tt2NativeOpTable)` (identity with the legacy global). Won't compile yet.
2. Run → fails.
3. Implement in `TT2Evaluator.h`: `template<typename Cfg> using TT2OpFuncT = void(*)(TT2RuntimeT<Cfg>&, TT2OutputState&, const TeletypeProgramT<Cfg>*, int16_t*, uint8_t&, bool, TT2EvalError&);` + `using TT2OpFunc = TT2OpFuncT<TT2ConfigFull>;` (keeps the existing `extern const TT2OpFunc* tt2NativeOpTable;` decl valid). Add **both** the primary `template<typename Cfg> const TT2OpFuncT<Cfg>* tt2OpTable();` AND the explicit-spec *declaration* `template<> const TT2OpFuncT<TT2ConfigFull>* tt2OpTable<TT2ConfigFull>();` (B1). Templatize on `Cfg` (deducing from the `runtime`/`program` args): the **full** `evaluateSegment` (keeps explicit `const TT2OpFuncT<Cfg>* table, size_t count` params — this is what the fake-ops test injects), the **convenience** `evaluateSegment`/`evaluateModPrefix`/`evaluateModPrefixStack` (now pass `tt2OpTable<Cfg>(), tt2NativeOpCount` instead of the hardcoded global), `tt2EnqueueDelayBody`, `tt2PushStackBody`, and `evaluateCommand`. **`evaluateCommand` gets NO table param** — it calls the convenience wrappers which resolve the trait. `tt2ClampDelayMs` (scalar) stays plain. Count `tt2NativeOpCount` is config-independent (`E_OP__LENGTH`) — keep using the global for the size arg.
4. **MANDATORY stub so this commit links** (blocker — otherwise every `runScript`/`evaluateCommand` caller, incl. `TT2TrackEngine.cpp:76,130,143,360` + ~23 test TUs, hits an undefined `tt2OpTable<TT2ConfigFull>()` and the whole `sequencer` target fails to link). Header (`TT2Evaluator.h`) holds only the primary template + the explicit-spec *declaration* (B1). The **single definition** goes in `TeletypeNativeOps.cpp` (NOT the header — a non-inline explicit specialization in a widely-included header is an ODR multiple-definition): `template<> const TT2OpFuncT<TT2ConfigFull>* tt2OpTable<TT2ConfigFull>() { return tt2NativeOpTable; }`. The legacy global already exists (`TeletypeNativeOps.cpp:4301`) and is valid here since ops aren't templated yet. **Task 5 EDITS this definition's body in place** (`return opTableBuilderFull.table;`) — it must not add a second definition of the same specialization.
5. Run full suite → PASS (trait-identity test green, link holds). **Commit:** `refactor(tt2): template evaluator + tt2OpTable trait (stub → global)`

---

## Phase 4 — Template ops + helper cluster + Full table

### Task 5: templatize every op + non-op helper, build the Full table + trait def
**Files:** `engine/TeletypeNativeOps.{cpp,h}`.
1. Templatize every `op*` AND every `static`/anon-namespace helper whose signature names `TT2Runtime`/`TeletypeProgram`/`TT2Pattern`/`TT2Script` (spec WI3 names them: `tt2PushRandom`, turtle helpers, `tt2InitPatternN`/`tt2ClearScriptN`, `tt2MiIndex`, `runStoredCommand`, `mutablePattern`, the `pattern*` family `:3107–3840`). `TT2_SIMPLE_VAR_OP` emits a template. Body `Cfg::` edits = the WI1 grep hits inside the file (≥10 `TT2_SCRIPT_COUNT` sites + metro/init).
2. `template<typename Cfg> struct OpTableBuilderT`; `CCMRAM_BSS OpTableBuilderT<TT2ConfigFull> opTableBuilderFull;`; **edit the Task-4 stub definition's body in place** to `return opTableBuilderFull.table;` (do NOT add a second definition of `tt2OpTable<TT2ConfigFull>()`); keep legacy `tt2NativeOpTable = opTableBuilderFull.table`.
3. **Helper grep gate:** `grep -n 'static.*\(TT2Runtime\|TeletypeProgram\|TT2Pattern\|TT2Script\)' src/apps/sequencer/engine/TeletypeNativeOps.cpp` → every hit is a `template`.
4. Run full suite (incl. `TestTeletypeV2NativeOps`/`ScriptRunner`/`Function`) + the Task-4 trait-identity test → PASS.
5. **Commit:** `refactor(tt2): template native ops + helpers + Full op table`

### Task 6: serializer + loader (printer is config-independent — SKIP)
**Files:** `engine/TT2SceneSerializer.{h,cpp}`, `engine/TT2ScriptLoader.h`. **`TT2Printer.{h,cpp}` is NOT touched** — `tt2PrintCommand(const TT2Command&)`/`tt2PrintScript(const TT2Script&)` take only fixed non-templated types, use no varying constant.
- `TT2SceneSerializer`: templatize the four functions (`tt2SerializeScene`/`tt2DeserializeScene`/`tt2SerializeScript`/`tt2DeserializeScript`, each takes `TeletypeProgram&`) on `Cfg`. Swap → `Cfg::`: `TT2_SCRIPT_COUNT` (`:53,:227,:240`), `TT2_METRO_SCRIPT`/`TT2_INIT_SCRIPT` (`:55,:56,:129,:131`), `TT2_TRIGGER_INPUT_COUNT` (`:84,:177`). Keep `TT2_PATTERN_COUNT` and the numbered-script `<= '8'` char-range literal as-is (Full-only, deferred). **Out-of-line defs → add explicit `template ...<TT2ConfigFull>;` instantiations at the `.cpp` end** or other TUs won't link.
- `TT2ScriptLoader.h` (inline): templatize `loadScriptText`/`setScriptCommand`/`insertScriptCommand`/`deleteScriptCommand` on `Cfg`; `TT2_SCRIPT_COUNT` (`:18,:78,:91,:106`) → `Cfg::ScriptCount`; `TT2_COMMANDS_PER_SCRIPT` stays fixed.
Run suite (incl. `TestTT2SceneSerializer`) → PASS. **Commit:** `refactor(tt2): template serializer + loader (+explicit Full instantiation)`

---

## Phase 5 — Residual constant cleanup + grep gate

### Task 7: convert non-templated Full call sites + bare literals
**Files:** `model/Project.cpp:37,44` (`TT2_METRO_SCRIPT`/`TT2_INIT_SCRIPT` → `TT2ConfigFull::MetroScript`/`InitScript`); any bare literal flagged in spec WI2 (metro/init markers; the `TT2SceneSerializer.cpp:131` `<= '8'` char-range stays — known-deferred Full-only, exempt). The `TT2TrackEngine.cpp:114-115` `k<4` X/Y/Z/T loop stays (fixed subset).
1. **Grep gate (acceptance #2):** no `TT2_*` varying constant inside templated infra except alias/static_assert definitions; no bare `10/64/9/8/2` as a count/index there. Compat aliases + tests are expected hits, not failures.
2. Run suite → PASS. **Commit:** `refactor(tt2): route Project.cpp seed + residual literals through config`

---

## Phase 6 — Seam 1 (program/count accessors; script + IO pages)

> Pattern page is OUT of scope — stays Full-concrete, untouched.

### Task 8: model-side accessors (TDD)
**Files:** Create `engine/TT2UiAccess.h` (or co-locate); Test `TestTT2UiAccess.cpp` (register).
1. **Failing test:** on a Full `TT2Track`, `tt2ScriptCount(track)==10`, `tt2TriggerInputCount(track)==8`, `tt2ScriptCommand(track,s,l)` returns the same `TT2Command*` as direct `program().scripts[s].commands[l]`, `tt2ScriptLength`/`setScriptLength` round-trip.
2. Run → fails.
3. Implement the free accessors (return shared `TT2Command*`/scalars; branch on `trackMode()` — Full only for now).
4. Run → PASS.
5. **Commit:** `feat(tt2): Seam-1 program/count accessors (Full)`

### Task 9: route script + IO pages through Seam 1
**Files:** `ui/pages/TeletypeScriptViewPage.cpp` (script-nav/edit + HUD trigger loop sites per spec), `ui/pages/TT2IoConfigPage.cpp`.
Replace direct `tt2Track().program()`/`TT2_SCRIPT_COUNT`/`TT2_TRIGGER_INPUT_COUNT` navigation with the accessors. `drawHud` stays concrete (out of seam). CV-in(6)/CV-out(8) keep plain constants. Behavior unchanged — verify with `ui-preview` for any touched layout + manual.
**Residual risk (state honestly):** there is no automated OLED-page behavior harness; a wrong accessor wiring compiles silently (no `-Werror`). The Task-8 test locks the *model-side values* (`tt2ScriptCount==10`, command-pointer identity) so the page only has to call the right accessor, not re-derive a count — that is the only automated coverage; the page wiring itself is manual + `ui-preview`.
**Commit:** `refactor(ui): route script/IO pages through Seam-1 accessors`

---

## Phase 7 — Seam 2 (engine control interface)

### Task 10: base-class virtual verbs + TT2 override (TDD)
**Files:** `engine/TrackEngine.h` (3 non-pure virtuals, default no-op bodies), `engine/TT2TrackEngine.{h,cpp}` (override). Test `TestTT2UiAccess.cpp` (extend).
**No unit test here — this task is not unit-testable, and the plan shouldn't pretend otherwise.** `TrackEngine` has no default ctor; its ctor takes `Engine&/Model&/Track&` and immediately reads `model.project().playState().trackState(track.trackIndex())` then calls `changePattern()` (`TrackEngine.h:32-39`), so even a "dummy" subclass needs the full mock context (confirmed unavailable in the unit harness by `TestTeletypeV2TrackEngineSmoke.cpp:5-9`). Virtual dispatch itself is language-guaranteed; there is nothing meaningful to assert at unit level. Gate = compiles + ARM `.map` size unchanged + sim/manual routing.
1. Implement: `virtual void triggerScript(int){} virtual void toggleScriptMute(int){} virtual void toggleMetroActive(){}` on `TrackEngine` (it already has a vtable — `TrackEngine.h:53`). TT2 already defines these exact signatures (`TT2TrackEngine.h:134-139`) — just add `override`. **No new vptr, no other engine edited.**
2. Verify: full suite still green (compile gate); **engine size via ARM `.map`** (Task 12 step 6), NOT a host `static_assert` (pointer width differs host/ARM); actual routing checked in sim/manual at Task 11.
3. **Commit:** `feat(engine): Seam-2 control verbs on TrackEngine base`

### Task 11: route script-page action sites through Seam 2
**Files:** `ui/pages/TeletypeScriptViewPage.cpp` action sites (`:396,420,909,917,925,968,975`).
Replace `as<TT2TrackEngine>().triggerScript/…` with calls on the `TrackEngine&` base interface. `drawHud`'s `as<TT2TrackEngine>()` (`:233`) stays concrete. Behavior unchanged — manual check.
**Residual risk:** the verb dispatch is compile-proven (the `override` must match the base signature) and manually/sim-checked (Task 10 has no unit test — `TrackEngine` isn't unit-constructible), and *which script index the page passes* to each verb is page-side and only manual-checkable — pay attention to the metro/mute/trigger sites (`:918` vs `:926` etc.) during the mechanical edit.
**Commit:** `refactor(ui): route script-page actions through Seam-2`

---

## Phase 8 — Acceptance gate (the spec's invariants)

### Task 12: full verification pass
1. Full `TestTeletypeV2*`/`TestTT2*` suite green, **unchanged** (no test edited — the compat aliases held).
2. **POD `sizeof` via host `static_assert(== baseline)`:** `TeletypeProgram`, `TT2Runtime`, `TT2Track` (no pointers → host and ARM agree). New `==` asserts against the Task-0a baselines; keep the existing `<=` drift guards too. **Engines are NOT host-asserted** (pointer width differs host/ARM) — checked in step 6.
3. WI1 grep gate + helper grep gate clean.
4. `tt2OpTable<TT2ConfigFull>() == tt2NativeOpTable`.
5. Script/IO page parity (manual + `ui-preview`).
6. **Hardware gate (authoritative for engine sizes):** `make -C build/stm32/release sequencer`; `.map` shows `.data`/`.bss`/`.ccmram_bss` unchanged vs the Task-0a ARM baseline, and `TT2TrackEngine`/engine-`Container` footprint unchanged (the Seam-2 base virtuals added no vptr). This is where "no engine growth" is verified — not a host assert.
7. **Commit (if any cleanup):** `chore(tt2): config parameterization acceptance gate green`

## Notes
- No `TeletypeMini` mode, no dispatch-switch edits, no `FileManager`, no second instantiation — all downstream (Mini-add plan).
- No serialization versioning (dev files break freely).
- After this lands, the Mini-add plan is: `TT2ConfigMini` + Mini instantiations + `TT2MiniTrack` + `TeletypeMini` mode + dispatch cases + Mini engine + per-accessor Mini branch.
