# Teletype Mini Scene-Track Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a second Teletype track mode ("TT-Mini": 2 scripts + metro, 8-deep delay) that stores 4 scenes and switches between them via the w-pattern selector (modulo-wrap), coexisting with the full 10-script TT2 track.

**Status of the foundation (DONE, on master):** the config-parameterization refactor is merged. The whole Teletype-V2 core is already `template<typename Cfg>`: `TeletypeProgramT<Cfg>` / `TT2RuntimeT<Cfg>` / `TT2PatternT<Cfg>` (structs), `TT2Runner.h` / `TT2Evaluator.h` / `TeletypeNativeOps.cpp` (~272 ops) / `TT2SceneSerializer` / `TT2ScriptLoader` (all templated), the **`tt2OpTable<Cfg>()` trait** with the `TT2ConfigFull` specialization, and `TT2ConfigFull` carrying **8 varying traits**: `ScriptCount, DelayDepth, TriggerInputCount, MetroScript, InitScript, SceneCount, PatternCount, PatternLength`. UI Seam-1 accessors (`TT2UiAccess.h`) and Seam-2 base virtuals exist. **This plan does NOT re-do any templating** — it instantiates a second config and builds the Mini track/mode/engine/UI on top.

**Architecture:** add `TT2ConfigMini` + its op-table/serializer/printer instantiations, a `TT2MiniTrack` model holding `programs[SceneCount]` + one runtime, a `TeletypeMini` `TrackMode`, the dispatch/routing/container wiring, a `TT2MiniTrackEngine`, and the UI seam's Mini branch. `changePattern()` selects `programs[pattern() % SceneCount]` and re-inits on a real scene change.

**Tech Stack:** C++11. Host tests `make -C build sequencer && ctest --test-dir build`; ARM `make -C build/stm32/release sequencer`; sim `make -C build/sim/debug sequencer`. IDE clang diagnostics are spurious — trust `make`.

**Budgets (verified during the parent refactor):** Mini program ~1.5 KB, Mini runtime ~2.3 KB, `TT2MiniTrack` (4 scenes + 1 runtime) ~8.4 KB ≤ full `TT2Track` 9520 → the `Track` union does NOT grow; the engine `Container` is uncapped and Mini < TT2 engine. Second op table +~1.7 KB CCMRAM (11 KB free), +~42 KB ROM (360 KB free).

---

## Two facts from this session to honor (new since the original plan)

1. **Boot/INIT runs in `update()`, not `tick()`** (fixed in `TT2TrackEngine`: the metro free-runs on wall-clock, so init must apply transport-independently). `TT2MiniTrackEngine` MUST boot the same way — run the scene's boot/init on the first `update()` (when `_firstTick`), never gated on a clock tick. Scene-switch re-arms `_firstTick` so the new scene re-inits.
2. **`X/Y/Z/T` default to `None`** (scratch) in the program init — the Mini program inherits this; scripts can use X/Y as counters.

## Scene-load init — RESOLVED (no separate init slot; `ScriptCount=3`)

`ScriptCount=4` (a dedicated init slot) was considered but **busts the union budget**: `TeletypeProgramT<Mini>` at ScriptCount=4 = 1820 B, so `TT2MiniTrack` = 4×1820 + 2240 (runtime) + `_trackIndex` = **9522 > 9520** → the `Track` union would grow and the `<= 9520` assert fails. It's also unnecessary: boot runs `runScript(bootScriptIndex)` **unconditionally** (`TT2TrackEngine.h:67-69`), and `bootScriptIndex = InitScript >= 0 ? InitScript : 0` (`TeletypeProgram.h:122`) → with `InitScript=-1`, **script0 runs on boot and on every scene-load**. So each scene self-configures (sets `M`, `MO.*`, output shaping) in **script0** — that IS the per-scene init hook. No separate slot needed.

So: **`ScriptCount=3`** — `script0` (runs on scene-load + as trigger 1), `script1` (trigger 2), index 2 = metro. Document in the demo/usage that **script0 is where a scene sets its `M`/outputs** (otherwise `m` defaults to 1000, the metro-bug failure mode).

## Config values

`TT2ConfigMini` (8 fields, matching the 8 in `TT2ConfigFull`):
`ScriptCount=3, DelayDepth=8, TriggerInputCount=2, MetroScript=2, InitScript=-1, SceneCount=4, PatternCount=4, PatternLength=64`. (`CommandsPerScript` stays the fixed shared `6`.)

Pattern dims stay 4×64 (a future "supermini" could shrink them — the dims are now config traits, but Mini keeps Full's values).

---

## Phase 0 — `TT2ConfigMini` + Mini instantiations

### Task 0: add `TT2ConfigMini` traits
**Files:** Modify `src/apps/sequencer/model/TT2Config.h`; Test `TestTT2Config.cpp`.
1. Failing test: assert the 8 values — `ScriptCount==3, DelayDepth==8, TriggerInputCount==2, MetroScript==2, InitScript==-1, SceneCount==4, PatternCount==4, PatternLength==64`.
2. Implement `struct TT2ConfigMini` with all 8 `static constexpr int` members.
3. Run → PASS. **Commit:** `feat(tt2): add TT2ConfigMini traits`

### Task 1: instantiate the Mini op table + trait (decl + def) + serializer
**Files:** `engine/TT2Evaluator.h` (Mini trait *declaration*), `engine/TeletypeNativeOps.cpp` (builder + trait def), `engine/TT2SceneSerializer.cpp` (Mini explicit instantiations). Printer is config-independent — skip. `TT2ScriptLoader.h` is header-inline — auto-instantiates, nothing to add. Test `TestTeletypeMini.cpp`.
1. **Failing test** (expected sizes: `TeletypeProgramT<Mini>`≈1518, `TT2RuntimeT<Mini>`≈2192):
   ```cpp
   expect(sizeof(TeletypeProgramT<TT2ConfigMini>) < 1700, "");
   expect(sizeof(TT2RuntimeT<TT2ConfigMini>)     < 2600, "");
   for (size_t i = 0; i < E_OP__LENGTH; ++i)
       if (tt2NativeOpTableFull[i]) expect(tt2NativeOpTableMini[i] != nullptr, "");   // coverage parity
   ```
2. **`TT2Evaluator.h`:** add the explicit-spec *declaration* next to the Full one (`:75`): `template<> const TT2OpFuncT<TT2ConfigMini>* tt2OpTable<TT2ConfigMini>();` — without it, the Mini engine TU calls the bodyless primary template (link error / ill-formed use-before-decl).
3. **`TeletypeNativeOps.cpp`:** `CCMRAM_BSS OpTableBuilderT<TT2ConfigMini> opTableBuilderMini;`, `const TT2OpFuncT<TT2ConfigMini>* tt2NativeOpTableMini = opTableBuilderMini.table;`, and the def `template<> const TT2OpFuncT<TT2ConfigMini>* tt2OpTable<TT2ConfigMini>() { return tt2NativeOpTableMini; }` (the builder, in the same TU's anon namespace, instantiates all ~272 ops for Mini implicitly).
4. **`TT2SceneSerializer.cpp`:** add `template ...<TT2ConfigMini>;` for the four functions next to the existing Full quartet (`:253-256`).
5. Run → PASS. **Commit:** `feat(tt2): instantiate Mini config (op table, trait decl+def, serializer)`

---

## Phase 1 — `TT2MiniTrack` model + `TeletypeMini` mode + dispatch

### Task 2: `TT2MiniTrack` model
**Files:** Create `src/apps/sequencer/model/TT2MiniTrack.h`; Test `TestTT2MiniTrack.cpp`.
Holds `TeletypeProgramT<TT2ConfigMini> _programs[TT2ConfigMini::SceneCount]` + `TT2RuntimeT<TT2ConfigMini> _runtime`. `program(scene)` → `_programs[scene % SceneCount]`. `clear()` calls `init(p)` per program (free-function-template ADL). `write` loops scenes; `read` loops + `init(_runtime)`. `static_assert(sizeof(TT2MiniTrack) <= 9520)`.
Test: sizeof guard + `&program(0) == &program(SceneCount)` wrap. **Commit:** `feat(tt2): TT2MiniTrack model (4 scenes + runtime)`

### Task 3: register `TeletypeMini` in `Track` — enum placement + ALL `Track.h` switches
**Files:** `src/apps/sequencer/model/Track.h`.
- **Append `TeletypeMini` AFTER `TeletypeV2`, before `Last`** (ordinal = in-memory value, feeds `trackModeSerialize`). `TeletypeV2` is in-memory/serialize **8**, so `TeletypeMini` is **9** and `Last` becomes 10.
- Add `TT2MiniTrack` to the `Container<...>` list **and the `_track` pointer union** (`Track.h:382-392` — add a `TT2MiniTrack *miniTt2;` member alongside `tt2`); add the `tt2MiniTrack()` accessor.
- Add the `TeletypeMini` case to ALL exhaustive switches (no `default:`): `trackModeName`, `trackModeLetter`, **`trackModeSerialize` → `return 9;`** (the switch falls through to `return 0` = Note for unhandled modes, so a missing case silently persists Mini as Note — lose scene data), `initContainer`, `setTrackIndex`.
**Commit:** `feat(model): register TeletypeMini in Track (enum + all Track.h switches)`

### Task 4: `Track.cpp` dispatch switches — silent-noop hazard
**Files:** `src/apps/sequencer/model/Track.cpp`. **No `-Werror`** — a missing case compiles and silently no-ops (`trackModeSerialize` would persist Mini as the fall-through, losing scene data). Add `TeletypeMini` to the six real switch sites: `clearPattern`, `copyPattern`, `gateOutputName`, `cvOutputName`, `write`, `read` (delegate write/read to `tt2MiniTrack().write/read`; mirror TT2 for the rest). **NOT** `clear()` (no switch) or `duplicatePattern()` (delegates to `copyPattern`).
Test: a `write()`/`read()` round-trip on a `TeletypeMini` track (full assert in Task 9). **Commit:** `feat(model): TeletypeMini cases in Track.cpp dispatch`

---

## Phase 2 — Engine: `TT2MiniTrackEngine` + scene switch + routing + container

### Task 5: `TT2MiniTrackEngine` (boot-in-update, ScopedHost, Seam-2 verbs)
**Files:** Create `engine/TT2MiniTrackEngine.{h,cpp}`; Test `TestTT2MiniTrackEngine.cpp`.
Mirror `TT2TrackEngine` **as it is now**:
- Hold `TT2MiniTrack&`. Run scripts via `::runScript<TT2ConfigMini>(_miniTrack.program(_activeScene), _miniTrack.runtime(), _output, idx)` — the op table resolves through `tt2OpTable<TT2ConfigMini>()`. `_prevInputState[TT2ConfigMini::TriggerInputCount]` (2).
- **Boot runs `runScript(bootScriptIndex)` in `update()` on the first refresh (`_firstTick`)**, NOT in `tick()` — exactly as `TT2TrackEngine.h:67-69`. With `InitScript=-1`, `bootScriptIndex` = 0 → **script0 runs on boot/scene-load** (no skip — boot is unconditional). That's the per-scene init hook.
- Wrap every execution entry (`runScript`/`update`/`tt2AdvanceDelays`/`tt2AdvanceMetro`/`updateInputTriggers`) in `ScopedHost`.
- Implement the three Seam-2 verbs (`triggerScript`/`toggleScriptMute`/`toggleMetroActive`) as `override` (bounded to `Cfg::ScriptCount`).
- `static_assert(sizeof(TT2MiniTrackEngine) <= 944)`.
**Test (host, free-function — the engine is NOT host-constructible):** build a `TeletypeProgramT<Mini>` whose script0 sets `M`, run it via `runScript<TT2ConfigMini>(...)`, assert `variables.m` is the script's value (not default 1000); then `tt2AdvanceMetro<TT2ConfigMini>(...)` fires `MetroScript`. (Same free-function style as `TestTeletypeV2Metro.cpp`.) The engine's update()/boot-in-update wiring is sim/manual. **Commit:** `feat(engine): TT2MiniTrackEngine (boot-in-update, ScopedHost)`

### Task 6: idempotent `changePattern()` scene switch
**Files:** `engine/TT2MiniTrackEngine.{h,cpp}`. Net-new (TT2 never overrides `changePattern`).
```cpp
void TT2MiniTrackEngine::changePattern() {
    int scene = pattern() % TT2ConfigMini::SceneCount;
    if (scene == _activeScene) return;   // changePattern fires for ALL tracks on any request/song-advance
    _activeScene = scene;
    init(_miniTrack.runtime());           // scene-load reset
    _firstTick = true;                    // re-run the new scene's boot/init on next update()
}
```
`_activeScene = -1` in ctor; the ctor calls `changePattern()` once to seed (mirrors TT2's ctor `reset()` — base ctor's virtual `changePattern()` doesn't reach the override). The runtime dispatch (`Engine.cpp:1067`) reaches the override at runtime, so seeding is correct.
**Extract the scene math to a pure free helper** `static int tt2SceneIndex(int pattern, int sceneCount) { return pattern % sceneCount; }` and call it from `changePattern()` — so it's host-unit-testable (the engine itself is NOT host-constructible; see below).
**Test (host):** `tt2SceneIndex(5,4)==1`, `tt2SceneIndex(0,4)==0`, wrap. **The idempotency guard + `init`-on-switch + boot-on-next-update is engine-level → sim/manual only** (the engine needs a full Engine/Model/Track; the smoke test `TestTeletypeV2TrackEngineSmoke.cpp:5-10` documents this). **Commit:** `feat(engine): idempotent mini scene switch (modulo wrap)`

### Task 7: engine `Container` + construction + output routing + external dispatch
**Files:** `engine/Engine.h` (add `TT2MiniTrackEngine` to `TrackEngineContainer` — else `create<>()` overflows); `engine/Engine.cpp` (construct case for `TeletypeMini`; **`isTt2[t] = (mode==TeletypeV2 || mode==TeletypeMini)`** so Mini routes through the TT2 jack layer, not the rotation pool); the external `switch(trackMode)` sites that need a case (`TopPage`'s three switches, `TrackPage`, `TeletypePatternViewPage` guards — decide block-vs-allow). Sites with a `default:` (`ClipBoard`, `GeneratorPage`, `LaunchpadController`) need no case.
**Commit:** `feat(engine): register + route + dispatch TeletypeMini`

---

## Phase 3 — Project serialization round-trip

### Task 8: per-scene write/read
**Files:** Test `TestTT2MiniSerialize.cpp` (model delegation done in Task 4).
Failing test: 4 *distinct* scenes → write project blob → read back → each scene preserves its own content (scene N == what was written to scene N; the four stay different), runtime re-inited. Confirm the Task-4 cases fire (silent-noop would surface as lost/blank scenes). **Commit:** `test(tt2): mini 4-scene serialization round-trip`

---

## Phase 4 — UI (v1: edit only; no SD I/O; no live-exec)

> **Scope guard:** mode selection + script/IO editing of the active scene + scene indicator. **No SD scene file save/load** (project blob persists it; `FileManager`/`gTeletypeLoadScratch` stay Full-only). **No live-exec** for Mini. The pattern page stays Full-only (block Mini).

### Task 9: expose `TeletypeMini` in track-mode selection. **Commit:** `feat(ui): expose TeletypeMini in track mode selection`

### Task 10: route script/IO pages to the active mini scene (dual path)
**Files:** `ui/pages/TeletypeScriptViewPage.cpp`, `ui/pages/TT2IoConfigPage.cpp`. Branch on `trackMode()` into a Mini path. Two coupling facts make this more than "add a branch":

- **Scene index must be threaded into Seam-1.** `TT2UiAccess.h` accessors take only `Track&` and hardwire `tt2Track().program()` (a *single* program). For Mini the editable program is `programs[activeScene]`, and the active scene lives on the **engine** (`pattern() % SceneCount` via `_trackState`), not on `Track`. So extend the Seam-1 accessor signatures to take a `scene` index (Full passes 0), and have the page compute `scene = pattern() % SceneCount` from the track state it already holds. Decide this signature change first.
- **Every `!= TeletypeV2` guard and every `as<TT2TrackEngine>()` cast is a wrong-union hazard** (`Container::as` is an unchecked `reinterpret_cast`; `tt2Track()` reads the Full union member). **Enumerate and handle each** in `TeletypeScriptViewPage.cpp` — guards at `:72,:78,:353,:396,:417,:476,:726` and casts at `:109,:234,:735` — plus `TT2IoConfigPage`'s. For each: either redirect to `tt2MiniTrack()` / `as<TT2MiniTrackEngine>()`, or hard-block Mini *before* the deref. **Do NOT just widen a guard to admit Mini** and leave a downstream `tt2Track()`/`as<TT2TrackEngine>()` — that's a silent runtime corruption, not a compile error. `drawHud` (`:234`) is exactly such a site.

`TT2IoConfigPage` shows 2 trigger rows (`Cfg::TriggerInputCount`), 6 CV-in, 8 CV-out. **Exclude live-exec (`runLiveCommand`, `:735`) and SD I/O for Mini.** Render touched layout with `ui-preview/`. Runtime behavior is sim/manual (no OLED harness) — but the wrong-union casts above are correctness, not cosmetics, so enumerate them exhaustively. **Commit:** `feat(ui): script/IO pages edit the mini active scene`

### Task 11: scene indicator — show `pattern() % SceneCount`; `ui-preview/`; minimal chip. **Commit:** `feat(ui): show active mini scene index`

---

## Verification gate
- All `TestTeletypeV2*`/`TestTT2*` green; new Mini tests green; **Full TT2 unchanged** (Mini is additive — adding the mode/instantiation must not alter Full: `sizeof(TeletypeProgramT<Full>)==3638`, `TT2RuntimeT<Full>==5880`, `Track`/engine sizes unchanged on the ARM `.map`).
- `sizeof`: `TeletypeProgramT<Mini>` < 1700, `TT2RuntimeT<Mini>` < 2600, `TT2MiniTrack` ≤ 9520, `TT2MiniTrackEngine` ≤ 944.
- Op-table coverage parity (Mini non-null wherever Full is).
- Scene-switch: `p % SceneCount`; same-pattern `changePattern()` no-op; **INIT runs on boot/scene-load via `update()`** so the metro period is the scene's, not default 1000.
- Output on the TT2 jack layer; serialization round-trip; Mini does NOT persist as the fall-through mode.
- Host + ARM builds clean; sim builds.

## Deferred (propose only, do not build)
- SD scene file save/load + live-exec for mini (`FileManager`/`gTeletypeLoadScratch` overloads).
- Mini pattern editing (the pattern page is Full-only in v1).
- "Supermini" with smaller pattern dims (now possible — `PatternCount`/`PatternLength` are config traits).
