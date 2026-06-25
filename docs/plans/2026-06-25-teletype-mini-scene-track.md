# Teletype Mini Scene-Track Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a second Teletype track mode ("TT-Mini": 2 scripts + metro, 8-deep delay) that stores 4 scenes and switches between them via the existing w-pattern selector (modulo-wrap), coexisting with the full 10-script TT2 track.

**Architecture:** Parameterize the entire Teletype-V2 core on a compile-time config traits struct (`ScriptCount`, `DelayDepth`, `MetroScript`, `InitScript`, `TriggerInputCount`, …). Instantiate twice: `Full` (today's constants, behavior-preserving) and `Mini`. The program/runtime structs, runner, evaluator, ops file, printer, serializer, and script-loader become `template<typename Cfg>`. **The op table is selected by config via a `tt2OpTable<Cfg>()` trait** — every entry point and every re-entrant op resolves its table from `Cfg`, so there is no global the execution path can silently fall back to. `TT2Host`, `TT2OutputState`, command width (16), and CV/TR counts (8) stay concrete.

**Tech Stack:** **C++11** (`CMAKE_CXX_STANDARD 11`, `src/CMakeLists.txt:22`), STM32F405 (1024 KB ROM / 128 KB SRAM / 64 KB CCMRAM), fixed-size POD structs, flat-blob serialization (no migration — dev files break freely), unit tests under `src/tests/unit/sequencer/`.

> **C++11 constraint:** use function templates + alias templates + explicit specialization only. **No C++14+ idioms** — in particular `tt2OpTable<Cfg>()` is a function template, NOT a variable template (variable templates are C++14). No `if constexpr`, no `auto` return deduction, no generic lambdas.
>
> **Test registration:** every new `TestX.cpp` MUST be added to `src/tests/unit/sequencer/CMakeLists.txt` with `register_sequencer_test(TestX TestX.cpp)` — creating the file is not enough; tests are registered manually (`CMakeLists.txt:9`). Each test task below includes this step implicitly.

**Budgets (verified twice against the release map + field-by-field):** Mini program ~1518 B, Mini runtime ~2176 B, `TT2MiniTrack` ~8256 B (≤ full TT2Track 9520, so the `Track` union does NOT grow). Second op table +1728 B CCMRAM (11 KB free). Second ops instantiation +~42 KB ROM (360 KB free). `Container` has no asserted cap and `maxsizeof` is unchanged since Mini < Full. All fit.

---

## Findings folded in from two adversarial passes (read before executing)

The RAM/ROM/CCMRAM premises held. The corrections are all plumbing the early drafts called "mechanical". The most dangerous, in order:

1. **Op-table selection must reach RE-ENTRANT ops, via the `Cfg`.** `opScript` (`TeletypeNativeOps.cpp:2537`), `opF/F1/F2` (`:2561/2571/2582`), `opL/L1/L2/LL*` (`:2592/2603/2615/2624/2634/2645`), `opSAll/opSPop`→`runStoredCommand` (`:3028`) re-enter `runScript`/`evaluateCommand` from inside an op. Threading a table *param* through the top-level call is not enough — the nested calls have no table in scope. Fix: a `template<typename Cfg> const TT2OpFuncT<Cfg>* tt2OpTable()` trait; `runScript<Cfg>`/`evaluateCommand<Cfg>` resolve the table from `Cfg` internally, so `opScript<Mini>` → `runScript<Mini>` → Mini table automatically. (Tasks 4, 6.)

2. **Idempotency guard introduced a ctor bug.** Base `TrackEngine` ctor calls virtual `changePattern()` before the derived vtable is live (`TrackEngine.h:38`) → hits the base no-op, not the override. The runtime dispatch (`Engine.cpp:1065`) only fires on a *change*. A project loaded already at pattern 5 never fires the override → `_activeScene` stays its `-1` init → `program(-1)`. TT2 sidesteps this by calling `reset()` in its own ctor body (`TT2TrackEngine.h:33`). Fix: the derived `TT2MiniTrackEngine` ctor must call `changePattern()` (which, with `_activeScene=-1`, runs once and seeds). (Tasks 10, 11.)

3. **Missing `switch(trackMode)` cases SILENTLY no-op — they do not break the build.** Compiler is `-Wall -Wno-unused-function`, **no `-Werror`** (`src/CMakeLists.txt:28-29`); `OverviewPage.cpp:324` already has an exhaustive switch with no `TeletypeV2` case and compiles today. So an omitted `TeletypeMini` case compiles, and `trackModeSerialize` falling through returns 0 → **Mini persists as Note, scene data lost**. The hazard is silent project corruption, not a compile error. (Tasks 9, 9b.)

4. **Three `Track.h` switches the first revision missed**, one persistence-critical: `trackModeName` (`Track.h:56`), `trackModeLetter` (`:83`), **`trackModeSerialize` (`:99`, must `return 9`)**. Plus enum placement: `TeletypeMini` must be **appended after `TeletypeV2`, before `Last`** — the `TrackMode` ordinal is the in-memory value and feeds `trackModeSerialize`. (Task 9.)

5. **`ScopedHost` lifecycle.** Native host-ops resolve through the global `g_tt2ActiveHost` set by the `ScopedHost` RAII guard (`TT2TrackEngine.h:194`). The mini engine must wrap its own `runScript`/`update`/delays/metro/input-triggers in `ScopedHost` or `W*/BUS/M/G` ops hit a stale host. (Task 10.)

6. **Live-exec hard-casts `as<TT2TrackEngine>()`** (`TeletypeScriptViewPage.cpp:727` guard, `:736` cast → `runLiveCommand`). v1: disable live-exec for Mini (don't reach the cast). (Task 15.)

**Confirmed correct by re-review (kept as-is):** output routing is a single `isTt2[]` point (`Engine.cpp:632`, no sibling `==TeletypeV2` output checks); engine `Container` (`Engine.h:46`) is the only one and uncapped; edit→apply UI path is self-contained and does NOT touch `FileManager`, so "edit but no SD I/O" is coherent; `RouteResolve.h:150` already `default: nullptr` so Mini inherits safe no-routing-params.

**v1 UI/persistence scope (confirm at Phase 8):** edit the active scene via a dual code path; **no SD scene file save/load for mini** (project blob persists it); **no live-exec for mini**. `FileManager`/`gTeletypeLoadScratch` stay Full-only.

**Effort:** 272 op/helper signatures need the type rewrite; ~32 lines need real `Cfg::` body edits; ~12 re-entrant call sites already covered by the trait. Multi-day. Add an op-table coverage assert (a mistyped entry is a silent runtime `UnsupportedOp`, not a compile error).

---

## Config values

`TT2ConfigFull`: `ScriptCount=10, CommandsPerScript=6, PatternCount=4, PatternLength=64, DelayDepth=64, StackDepth=16, ExecDepth=8, QLength=64, MetroScript=8, InitScript=9, TriggerInputCount=8, SceneCount=1`.

`TT2ConfigMini`: `ScriptCount=3 (script0, script1, metro), CommandsPerScript=6, PatternCount=4, PatternLength=64, DelayDepth=8, StackDepth=16, ExecDepth=8, QLength=64, MetroScript=2, InitScript=-1, TriggerInputCount=2, SceneCount=4`.

Kept identical (do NOT templatize): `CommandMaxLength=16` (parser contract), **CV/TR outputs = 8 and CV inputs = 6** (`TT2_CV_INPUT_COUNT=6`, `TT2_CV_OUTPUT_COUNT=8`, hardware — both configs), `TT2OutputState`, `TT2Host`, `TT2Midi`, RNG, turtle. Only `TriggerInputCount` varies: Full 8 → Mini 2 (2 trigger-script inputs). CV input count stays 6 on Mini.

> **OPEN DECISION (flag before Phase 4):** Mini = 2 scripts + metro, **no init script** per spec. For a per-scene init (runs on scene-load), set `ScriptCount=4, InitScript=3` + a `runScript(InitScript)` in Task 11. Default honors "2 scripts + metro".

---

## Phase 0 — Config traits scaffolding

### Task 0: config traits struct
**Files:** Create `src/apps/sequencer/model/TeletypeConfig.h`; Test `TestTeletypeConfig.cpp`; register in `src/tests/unit/sequencer/CMakeLists.txt`.
**Step 1:** failing test asserting both structs' values (above). **Step 2:** add `register_sequencer_test(TestTeletypeConfig TestTeletypeConfig.cpp)` to the tests CMakeLists; run, fails. **Step 3:** implement (all `static constexpr int`). **Step 4:** PASS. **Step 5: commit** — `feat(tt2): add Teletype config traits (Full/Mini)`

---

## Phase 1 — Template the storage structs, alias to Full (behavior-preserving)

**Success = full existing TT2 suite green and `sizeof` unchanged.**

### Task 1: template `TeletypeProgram`
**Files:** `src/apps/sequencer/model/TeletypeProgram.h`.
`template<typename Cfg> struct TeletypeProgramT`; `TT2ScriptT<Cfg>::commands[Cfg::CommandsPerScript]`; `scripts[Cfg::ScriptCount]`; `patterns[Cfg::PatternCount]`; `triggerSource[Cfg::TriggerInputCount]`. Keep `TT2Command`/`TT2Pattern`/`TT2_COMMAND_MAX_LENGTH=16` concrete. `init(...)` becomes a **free function template in the global namespace** (ADL resolves `init(prog)`). Boot: `bootScriptIndex = Cfg::InitScript >= 0 ? Cfg::InitScript : 0`, `bootEnabled = Cfg::InitScript >= 0`.
Aliases at file end: `using TeletypeProgram = TeletypeProgramT<TT2ConfigFull>; using TT2Script = TT2ScriptT<TT2ConfigFull>;` + `static_assert(sizeof(TeletypeProgram) <= 3640)`. Keep `TT2_SCRIPT_COUNT = TT2ConfigFull::ScriptCount` etc. **These aliases keep Phases 1–2 compiling; do NOT remove until Phase 3 done.**
Run full suite (PASS, sizeof holds). **Commit** — `refactor(tt2): template TeletypeProgram on config, alias Full`

### Task 2: template `TT2Runtime`
**Files:** `src/apps/sequencer/model/TeletypeRuntime.h`.
`template<typename Cfg> struct TT2RuntimeT`; templatize `variables.j/k[Cfg::ScriptCount]`, `stack.commands[Cfg::StackDepth]`, `delay.entries[Cfg::DelayDepth]`, `every[Cfg::ScriptCount][Cfg::CommandsPerScript]`, `exec.frames[Cfg::ExecDepth]`, `scriptLastMs[Cfg::ScriptCount]`. Keep `q[Cfg::QLength]`, `n_scale[16]`, `cv[8]`, `tr[8]`, midi/rng/turtle. `init`, `tt2InitVariables`, inline helpers → function templates; bounds → `Cfg::`. Alias `using TT2Runtime = TT2RuntimeT<TT2ConfigFull>;` + `static_assert(<=5900)`.
Run full suite (PASS). **Commit** — `refactor(tt2): template TT2Runtime on config, alias Full`

---

## Phase 2 — Template runner + evaluator + the `tt2OpTable<Cfg>` trait

### Task 3: `TT2Runner.h`
`runScript`, `tt2AdvanceDelays`, `tt2AdvanceMetro`, `tt2RunFunction*`, `runStoredCommand` become `template<typename Cfg>`. They resolve their op table from `tt2OpTable<Cfg>()` internally — **no table param threaded**. Metro fires `Cfg::MetroScript`; delay loops `Cfg::DelayDepth`; skip boot when `Cfg::InitScript < 0`. **Commit** — `refactor(tt2): template TT2Runner, table via Cfg trait`

### Task 4: `TT2Evaluator.h` — op-func type, the trait, Full-defaulted overload
**Files:** `src/apps/sequencer/engine/TT2Evaluator.h`.
```cpp
template<typename Cfg>
using TT2OpFuncT = void (*)(TT2RuntimeT<Cfg>&, TT2OutputState&,
                           const TeletypeProgramT<Cfg>*,
                           int16_t*, uint8_t&, bool, TT2EvalError&);
using TT2OpFunc = TT2OpFuncT<TT2ConfigFull>;                 // keeps TestTeletypeV2Evaluator + extern decl compiling
template<typename Cfg> const TT2OpFuncT<Cfg>* tt2OpTable();  // defined per-config in TeletypeNativeOps.cpp
```
`evaluateSegment`/`evaluateCommand` become `template<typename Cfg>` with the table as a **defaulted param** `const TT2OpFuncT<Cfg>* table = tt2OpTable<Cfg>()` — so re-entrant ops that omit it get the right `Cfg` table, while `TestTeletypeV2Evaluator`'s `fakeOpTable` is still passable explicitly. Keep a non-template Full convenience overload bound to `tt2OpTable<TT2ConfigFull>()` for legacy 2-arg call sites. Bounds use `Cfg::`; keep working stack `[TT2_COMMAND_MAX_LENGTH]` (16).
Run full suite incl. `TestTeletypeV2Evaluator` — PASS. **Commit** — `refactor(tt2): template evaluator + tt2OpTable trait + Full overload`

### Task 5: printer / serializer / loader (incl. `.cpp` explicit instantiation)
**Files:** `TT2Printer.{h,cpp}`, `TT2SceneSerializer.{h,cpp}`, `TT2ScriptLoader.h`.
Templatize on `Cfg`. **`TT2SceneSerializer.cpp` and `TT2Printer.cpp` have out-of-line defs** — add explicit `template ...<TT2ConfigFull>;` at each `.cpp` end (Mini in Phase 4). Run suite. **Commit** — `refactor(tt2): template printer/serializer/loader (+explicit Full instantiation)`

---

## Phase 3 — Template the ops file + Full table + the trait definition

`TeletypeNativeOps.cpp` (4302 lines, 272 op/helper sigs).

### Task 6: templatize ops + builder + Full table + trait specialization
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.{cpp,h}`.
- Every `opXxx`/runtime-touching helper → `template<typename Cfg>` with `TT2RuntimeT<Cfg>&`/`const TeletypeProgramT<Cfg>*`. `TT2_SIMPLE_VAR_OP` macro emits a template. Size bounds → `Cfg::` (~1026, 1743-1805, 2913; script bound `>= Cfg::ScriptCount` at 2287; default `-1 → Cfg::InitScript`, skip if `<0`, at 226). Scalar-op bodies untouched; musical constants untouched.
- **Re-entrant ops** (`opScript:2537`, `opF*:2561/2571/2582`, `opL*:2592…2645`, `opSAll/opSPop`→`runStoredCommand:3028`) call `runScript<Cfg>(...)`/`evaluateCommand<Cfg>(...)` with NO explicit table → they pick up `tt2OpTable<Cfg>()`. Verify each forwards `Cfg`.
- `template<typename Cfg> struct OpTableBuilderT { TT2OpFuncT<Cfg> table[E_OP__LENGTH]; OpTableBuilderT(){ /* nullptr fill + table[E_OP_X]=opX<Cfg>; */ } };`
- Export Full + trait specialization + legacy globals:
```cpp
CCMRAM_BSS OpTableBuilderT<TT2ConfigFull> opTableBuilderFull;
const TT2OpFuncT<TT2ConfigFull>* tt2NativeOpTableFull = opTableBuilderFull.table;
template<> const TT2OpFuncT<TT2ConfigFull>* tt2OpTable<TT2ConfigFull>() { return tt2NativeOpTableFull; }
const TT2OpFunc* tt2NativeOpTable = tt2NativeOpTableFull;   // legacy
const size_t tt2NativeOpCount = E_OP__LENGTH;
```
Run full suite — PASS (behavior-identical for Full). Now legacy `TT2_*` constant aliases may be removed if no non-template consumer remains (grep; if UI still uses them, defer to Phase 8). **Commit** — `refactor(tt2): template native ops + Full table + tt2OpTable<Full>`

---

## Phase 4 — Instantiate Mini

### Task 7: Mini instantiations + size guards + table coverage
**Files:** `TeletypeNativeOps.cpp`, `TT2SceneSerializer.cpp`, `TT2Printer.cpp` (add `<TT2ConfigMini>`); Test `TestTeletypeMini.cpp`.
**Step 1: failing test**
```cpp
expect(sizeof(TeletypeProgramT<TT2ConfigMini>) < 1700);
expect(sizeof(TT2RuntimeT<TT2ConfigMini>)     < 2600);
for (size_t i = 0; i < E_OP__LENGTH; ++i)
    if (tt2NativeOpTableFull[i]) expect(tt2NativeOpTableMini[i] != nullptr);   // coverage parity
```
**Step 3:**
```cpp
CCMRAM_BSS OpTableBuilderT<TT2ConfigMini> opTableBuilderMini;
const TT2OpFuncT<TT2ConfigMini>* tt2NativeOpTableMini = opTableBuilderMini.table;
template<> const TT2OpFuncT<TT2ConfigMini>* tt2OpTable<TT2ConfigMini>() { return tt2NativeOpTableMini; }
```
plus `<TT2ConfigMini>` instantiations in the two `.cpp`s. **Step 4:** PASS. **Commit** — `feat(tt2): instantiate Mini config (structs, ops, table, trait)`

---

## Phase 5 — Mini track model + Track dispatch

### Task 8: `TT2MiniTrack` model
**Files:** Create `src/apps/sequencer/model/TT2MiniTrack.h`; Test `TestTT2MiniTrack.cpp`.
`TeletypeProgramT<Mini> _programs[Cfg::SceneCount]` + `TT2RuntimeT<Mini> _runtime`. `program(scene)` → `_programs[scene % Cfg::SceneCount]`. `write` loops scenes; `read` loops + `init(_runtime)`. `clear()` calls `init(p)` per program (free-template ADL). `static_assert(sizeof(TT2MiniTrack) <= 9520)`.
Test: sizeof + wrap. **Commit** — `feat(tt2): add TT2MiniTrack model (4 scenes + runtime)`

### Task 9: register `TeletypeMini` in `Track` — enum placement + ALL Track.h switches
**Files:** `src/apps/sequencer/model/Track.h`.
- **Append `TeletypeMini` AFTER `TeletypeV2`, before `Last`** (ordinal = in-memory value).
- Add `TT2MiniTrack` to `Container<...>` (`:381`), `tt2MiniTrack()` accessor (mirror `:255-262`).
- Add the `TeletypeMini` case to ALL exhaustive switches (none have `default:`): `trackModeName` (`:56`), `trackModeLetter` (`:83`), **`trackModeSerialize` (`:99` → `return 9`)**, `initContainer` (`:332`), `setTrackIndex` (`:292`).
**Commit** — `feat(model): register TeletypeMini in Track (enum + all Track.h switches)`

### Task 9b: `Track.cpp` dispatch switches — silent-noop hazard
**Files:** `src/apps/sequencer/model/Track.cpp`.
The real `switch(_trackMode)` sites (verified): `clearPattern` (`:16`), `copyPattern` (`:49`), `gateOutputName` (`:90`), `cvOutputName` (`:122`), `write` (`:161`), `read` (`:207`). **`clear()` (`:5`) has no switch and `duplicatePattern()` (`:81`) only delegates to `copyPattern()` — do NOT add cases there.** Add the `TeletypeMini` case to the six real sites: delegate write/read to `tt2MiniTrack().write/read`; mirror TT2 for clearPattern/copyPattern/gateOutputName/cvOutputName.
**No `-Werror` (`-Wall -Wno-unused-function`): a missing case compiles and silently no-ops → `write`/`read` lose scene data and `trackModeSerialize` (Task 9) would persist Mini as Note.**
Test: `write()`/`read()` round-trip on a `TeletypeMini` track (full assert in Task 13). **Commit** — `feat(model): TeletypeMini cases in the six Track.cpp dispatch sites`

---

## Phase 6 — Engine: mini engine, container, scene switch, output routing

### Task 10: `TT2MiniTrackEngine` (+ ScopedHost + ctor seeding)
**Files:** Create `TT2MiniTrackEngine.{h,cpp}`; Test `TestTT2MiniTrackEngine.cpp`.
- Hold `TT2MiniTrack&`. Run scripts via `::runScript<TT2ConfigMini>(_miniTrack.program(_activeScene), _miniTrack.runtime(), _output, idx)` (table via trait). `_prevInputState[Cfg::TriggerInputCount]` (2). Implement `TT2Host` like `TT2TrackEngine`.
- **Expose the engine methods the script page hotkeys call** (mirror `TT2TrackEngine`): `triggerScript(idx)`, `toggleScriptMute(idx)`, `toggleMetroActive()` — bounded to `Cfg::ScriptCount`. The UI (Task 15) calls these via `as<TT2MiniTrackEngine>()`. `runLiveCommand` is NOT added (no live-exec in v1).
- **Wrap every execution entry (`runScript`, `update`, `tt2AdvanceDelays`, `tt2AdvanceMetro`, `updateInputTriggers`) in `ScopedHost`** (mirror `TT2TrackEngine.h:194`); the global host is shared/scoped (last-writer-wins, safe). No live-exec for Mini in v1 (Task 15), so no live-exec wrap.
- **Ctor body calls `changePattern()`** (with `_activeScene = -1` init) to seed the scene + runtime, mirroring TT2's ctor `reset()` — base ctor's virtual `changePattern()` does NOT reach the override.
- `static_assert(sizeof(TT2MiniTrackEngine) <= 944)`.
Test: metro fires `Cfg::MetroScript`; gate/cv emitted. **Commit** — `feat(engine): TT2MiniTrackEngine (+ScopedHost, ctor seed)`

### Task 11: idempotent `changePattern()` scene switch
**Files:** `TT2MiniTrackEngine.{h,cpp}`. Net-new (TT2 never overrides this).
```cpp
void TT2MiniTrackEngine::changePattern() {
    int scene = pattern() % TT2ConfigMini::SceneCount;
    if (scene == _activeScene) return;        // changePattern fires for ALL tracks on any request/song-advance
    _activeScene = scene;
    init(_miniTrack.runtime());               // scene-load reset
    _firstTick = true;
}
```
`_activeScene = -1` in ctor → first call (from Task 10 ctor) seeds scene 0 and inits. If per-scene init slot enabled, also `runScript(Cfg::InitScript)` here.
Test: distinct metro scripts in scene 0/1; `setPattern(5)` → scene 1 runs, runtime cleared; repeated `changePattern()` same pattern is a no-op (variables preserved). **Commit** — `feat(engine): idempotent mini scene switch (modulo wrap)`

### Task 12: construct engine + engine `Container` list
**Files:** `Engine.h:46` add `TT2MiniTrackEngine` to `TrackEngineContainer`; `Engine.cpp:588` add `TeletypeMini` construction case. (Container uncapped; Mini < TT2 engine, no growth.) Confirm virtual `changePattern()` dispatch (`:1065`) reaches it. **Commit** — `feat(engine): register + construct TT2MiniTrackEngine`

### Task 12b: output routing
**Files:** `Engine.cpp:632` — `isTt2[t] = (mode == TeletypeV2 || mode == TeletypeMini);`. Confirmed the single special-case point (all downstream reads `isTt2[]`). **Commit** — `fix(engine): route TeletypeMini through the TT2 jack layer`

### Task 12c: external dispatch sites (corrected list)
**Files / action per site:**
- `TopPage.cpp` — **three** exhaustive switches (`:211, :277, :335`): add the case to each.
- `OverviewPage.cpp:324` — no `default:`, no TT2 case: Mini draws nothing (acceptable for v1; note it, optionally add a glyph).
- `TeletypePatternViewPage.cpp` — **block Mini for v1.** The page derefs `tt2Track()` (Full) at ~20 sites (`:60/:343/:373/…/:726`); extending only the `!= TeletypeV2` guards (`:50/:197/:292/:300`) would route Mini into Full-only accessors. v1: leave the guards as-is so Mini is blocked from this page (its tt-patterns are still readable/writable from scripts). Dual-pathing this page is deferred.
- `TrackPage.cpp:102` — page-routing switch (`Last`-covered): route Mini to the mini script page.
- **No change needed** (already have `default:`): `ClipBoard.cpp`, `GeneratorPage.cpp`, `LaunchpadController.cpp`.
**Commit** — `feat: handle TeletypeMini at the dispatch sites that need it`

---

## Phase 7 — Project serialization round-trip

### Task 13: per-scene write/read
**Files:** Test `TestTT2MiniSerialize.cpp` (model delegation done in 9b).
Failing test: write 4 *distinct* scenes → write project blob → read back → **each scene preserves its own distinct content (scene N reads back exactly what was written to scene N; the four remain different from each other)**, runtime re-inited. Run; confirm 9b cases fire (a silent-noop write/read would surface here as lost or blank scenes). PASS. **Commit** — `test(tt2): mini 4-scene serialization round-trip`

---

## Phase 8 — UI (v1: edit only; no SD I/O; no live-exec)

> **Scope guard:** mode selection + script/IO editing of the active scene + scene indicator. No randomize/init/copy companions. **No SD scene file save/load** and **no live-exec** for mini in v1.

### Task 14: expose `TeletypeMini` in track-mode selection. **Commit** — `feat(ui): expose TeletypeMini in track mode selection`

### Task 15: script/IO pages edit the active mini scene (dual path; no live-exec)
**Files:** `TeletypeScriptViewPage.cpp`, `TT2IoConfigPage.cpp`.
`tt2MiniTrack().program(scene)` and `as<TT2MiniTrackEngine>()` are different static types than the Full ones — branch on `trackMode()` per handler; don't share a `program`/`trackEngine` local. Templatized `TT2ScriptLoader` helpers bind for both.
**Audit EVERY `tt2Track()` / `as<TT2TrackEngine>()` site in the page (not just program-edit + live-exec). The full list, each either branched to Mini or excluded for v1:**
- program read/edit — `:87, :628, :797, :812, :835` → branch to `tt2MiniTrack().program(scene)`.
- `drawHud()` — `:231` (`tt2Track()`), `:233` (`as<TT2TrackEngine>()`) → branch to mini track + engine.
- F-key script triggers — `:396, :420` (`as<TT2TrackEngine>().triggerScript`) → branch to mini engine; map F1–F2 (Mini has 2 scripts).
- script-mute / metro toggles — `:909, :917, :925, :968` (`toggleScriptMute`), `:975` (`toggleMetroActive`) → branch to mini engine.
- **live-exec — `:108, :736` (`runLiveCommand` via `as<TT2TrackEngine>()`): EXCLUDE for Mini v1** (mini engine has no `runLiveCommand`).
- **SD file I/O — `:1189, :1206` (`FileManager::writeTt2Script/readTt2Script`): EXCLUDE for Mini v1.**
`TT2IoConfigPage`: the Mini input grid shows **2 trigger-input rows, 6 CV-input rows, 8 CV-output rows** (only `Cfg::TriggerInputCount` shrinks; CV in stays 6, out stays 8). Render with `ui-preview/` before shipping.
**Commit** — `feat(ui): script/IO pages edit the mini active scene`

### Task 16: scene indicator. Show `pattern() % 4`; render with `ui-preview/`; minimal text chip. **Commit** — `feat(ui): show active mini scene index`

---

## Verification gate
- Full TT2 suite green at every Phase 1–3 commit.
- `sizeof`: `TeletypeProgramT<Mini>` < 1700, `TT2RuntimeT<Mini>` < 2600, `TT2MiniTrack` ≤ 9520, `Track` unchanged, `TT2MiniTrackEngine` ≤ 944.
- Op-table coverage parity (Mini non-null wherever Full is).
- **Re-entrant ops use the Mini table:** a Mini script running `SCRIPT 1` / `$F` / `S.ALL` executes Mini ops (assert via a Mini-only op-count or a script that would mis-evaluate on the Full table).
- Scene-switch: `p%4`; same-pattern `changePattern()` is a no-op; freshly-loaded project at any pattern seeds `_activeScene` (ctor path).
- Output on TT2 jack layer, not rotation pool.
- 4-scene serialization round-trip; Mini does NOT persist as Note (trackModeSerialize==9).
- Unit tests via top-level `build/`. Sim: `make -C build/sim/debug sequencer`.
- **Hardware build is a feature gate (this is RAM/CCMRAM-sensitive):** `make -C build/stm32/release sequencer`, then check the `.map`/size output that `.data` + `.bss` fit 128 KB SRAM and `.ccmram_bss` fits 64 KB after the second op table lands. Confirm the `static_assert`s on `Track`, `TT2MiniTrack` (≤9520), and the engine container hold on the ARM build (sizes can differ from the host build).

## Deferred (propose only)
- Per-scene init script (config open decision).
- SD scene file save/load + live-exec for mini (`FileManager`/`gTeletypeLoadScratch` overloads).
- Shrinking `StackDepth` below 16 (802 B, largest runtime item; risks deep-nest failures).
