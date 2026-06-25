# Teletype Mini Scene-Track Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a second Teletype track mode ("TT-Mini": 2 scripts + metro, 8-deep delay) that stores 4 scenes and switches between them via the w-pattern selector (modulo-wrap), coexisting with the full 10-script TT2 track.

**Architecture:** Instantiate a second config `TT2ConfigMini` against the already-merged `template<Cfg>` TT2 core, add a `TT2MiniTrack` model (`programs[SceneCount]` + one runtime), a `TeletypeMini` `TrackMode`, dispatch/routing/container wiring, a `TT2MiniTrackEngine`, and the UI seam's Mini branch. `changePattern()` selects `programs[pattern() % SceneCount]` and re-inits on a real scene change.

**Tech Stack:** C++11, STM32F405 + host. Test idiom: `UNIT_TEST("X"){ CASE("..."){ expectEqual(a,b,"msg"); expect(cond,"msg"); } }` (see `src/tests/unit/sequencer/TestTT2Config.cpp`). Register each test with `register_sequencer_test(Name Name.cpp)` in `src/tests/unit/sequencer/CMakeLists.txt`.

**Build/test commands (run from repo root, never `cd`):**
- Host build + one test: `make -C build sequencer && ctest --test-dir build -R <TestName> -V`
- Full host suite: `ctest --test-dir build`
- ARM: `make -C build/stm32/release sequencer`
- Sim: `make -C build/sim/debug sequencer`
- **IDE clang diagnostics are spurious — trust `make`.** Never prefix git/make with `cd`.

---

## Foundation (DONE, on master — do NOT re-template)

The whole TT2 core is `template<typename Cfg>`: `TeletypeProgramT<Cfg>`/`TT2RuntimeT<Cfg>`/`TT2PatternT<Cfg>`, `TT2Runner.h`/`TT2Evaluator.h`/`TeletypeNativeOps.cpp` (~272 ops)/`TT2SceneSerializer`/`TT2ScriptLoader`, the `tt2OpTable<Cfg>()` trait with the `TT2ConfigFull` specialization, UI Seam-1 accessors (`TT2UiAccess.h`), Seam-2 base virtuals (`TrackEngine.h:68-70`). `TT2ConfigFull` has **8 traits**: `ScriptCount, DelayDepth, TriggerInputCount, MetroScript, InitScript, SceneCount, PatternCount, PatternLength`.

**Two facts to honor:**
1. **Boot/INIT runs in `update()` on `_firstTick`, NOT `tick()`** (`TT2TrackEngine.h:67-69`) — the metro free-runs on wall-clock, so init must apply transport-independently. The Mini engine boots the same way; scene-switch re-arms `_firstTick`.
2. **`X/Y/Z/T` default to `None`** (scratch) — the Mini program inherits this; scripts can use X/Y as counters.

## Config decision — `ScriptCount=3`, no init slot (RESOLVED)

The real `Track`-union cap is **NoteTrack = 9544 B**, not TT2Track's 9520 (PROJECT.md:251,293 — the 8-track `Track::_container` is sized by NoteTrack; a model below 9544 B adds no Model RAM). So `ScriptCount=4` (TT2MiniTrack=9522 ≤ 9544) *would* fit for free. We still choose **`ScriptCount=3`**: boot runs `runScript(bootScriptIndex)` unconditionally and `bootScriptIndex = InitScript>=0 ? InitScript : 0` (`TeletypeProgram.h:122`) → with `InitScript=-1`, **script0 runs on boot/scene-load**, so script0 IS the per-scene init hook (set `M`/`MO.*`/output shaping there). **script0 also fires as trigger-1** — triggering input 1 re-runs the scene's setup; that's the accepted trade for the smaller layout.

**`TT2ConfigMini`:** `ScriptCount=3, DelayDepth=8, TriggerInputCount=2, MetroScript=2, InitScript=-1, SceneCount=4, PatternCount=4, PatternLength=64`. Sizes: `TeletypeProgramT<Mini>`≈1518, `TT2RuntimeT<Mini>`≈2192, `TT2MiniTrack`≈8.3 KB (≤9544 NoteTrack cap — ample headroom; the `<=9520` asserts below are a conservative TT2-family cap, not the union cliff). Pattern dims stay 4×64.

**Execution note:** the per-task `commit` steps proceed under the user's standing go-ahead for this plan's execution run (per repo policy, commits otherwise need explicit confirmation).

---

## Phase 0 — `TT2ConfigMini` + Mini instantiations

### Task 0: add `TT2ConfigMini` traits

**Files:** Modify `src/apps/sequencer/model/TT2Config.h`; Test `src/tests/unit/sequencer/TestTT2Config.cpp` (extend existing).

**Step 1 — failing test.** Add a `CASE` to the existing `UNIT_TEST("TT2Config")` block in `TestTT2Config.cpp`:
```cpp
CASE("TT2ConfigMini traits") {
    expectEqual(TT2ConfigMini::ScriptCount, 3, "ScriptCount");
    expectEqual(TT2ConfigMini::DelayDepth, 8, "DelayDepth");
    expectEqual(TT2ConfigMini::TriggerInputCount, 2, "TriggerInputCount");
    expectEqual(TT2ConfigMini::MetroScript, 2, "MetroScript");
    expectEqual(TT2ConfigMini::InitScript, -1, "InitScript");
    expectEqual(TT2ConfigMini::SceneCount, 4, "SceneCount");
    expectEqual(TT2ConfigMini::PatternCount, 4, "PatternCount");
    expectEqual(TT2ConfigMini::PatternLength, 64, "PatternLength");
}
```
**Step 2 — run, expect FAIL** (`'TT2ConfigMini' was not declared`): `make -C build sequencer && ctest --test-dir build -R TestTT2Config -V`
**Step 3 — implement.** In `TT2Config.h`, after `struct TT2ConfigFull { ... };`, add:
```cpp
struct TT2ConfigMini {
    static constexpr int ScriptCount = 3;
    static constexpr int DelayDepth = 8;
    static constexpr int TriggerInputCount = 2;
    static constexpr int MetroScript = 2;
    static constexpr int InitScript = -1;
    static constexpr int SceneCount = 4;
    static constexpr int PatternCount = 4;
    static constexpr int PatternLength = 64;
};
```
**Step 4 — run, expect PASS.**
**Step 5 — commit:** `git add -A && git commit -m "feat(tt2): add TT2ConfigMini traits"`

### Task 1: instantiate Mini op table + trait (decl + def) + serializer

**Files:** Modify `src/apps/sequencer/engine/TT2Evaluator.h`, `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `src/apps/sequencer/engine/TT2SceneSerializer.cpp`; Test create `src/tests/unit/sequencer/TestTeletypeMini.cpp`. (Printer is config-independent — skip. `TT2ScriptLoader.h` is header-inline — auto-instantiates.)

**Step 1 — failing test.** Create `TestTeletypeMini.cpp`:
```cpp
#include "UnitTest.h"
#include "model/TT2Config.h"
#include "model/TeletypeProgram.h"
#include "model/TeletypeRuntime.h"
#include "engine/TT2Evaluator.h"

UNIT_TEST("TeletypeMini") {
CASE("Mini struct sizes fit budget") {
    expect(sizeof(TeletypeProgramT<TT2ConfigMini>) < 1700, "program");
    expect(sizeof(TT2RuntimeT<TT2ConfigMini>) < 2600, "runtime");
}
CASE("Mini op table covers every op Full covers") {
    auto *full = tt2OpTable<TT2ConfigFull>();
    auto *mini = tt2OpTable<TT2ConfigMini>();
    for (int i = 0; i < E_OP__LENGTH; ++i)
        if (full[i]) expect(mini[i] != nullptr, "coverage");
}
}
```
**Step 2 — register** in `CMakeLists.txt` (near the other TT2 lines, ~`:27`): `register_sequencer_test(TestTeletypeMini TestTeletypeMini.cpp)`
**Step 3 — run, expect FAIL** (link error: `tt2OpTable<TT2ConfigMini>` undefined): `make -C build sequencer && ctest --test-dir build -R TestTeletypeMini -V`
**Step 4 — implement (decl).** In `TT2Evaluator.h`, next to the Full spec declaration (~`:75`), add:
```cpp
template<> const TT2OpFuncT<TT2ConfigMini>* tt2OpTable<TT2ConfigMini>();
```
**Step 5 — implement (def).** In `TeletypeNativeOps.cpp`, next to the Full builder/table/def, add:
```cpp
CCMRAM_BSS OpTableBuilderT<TT2ConfigMini> opTableBuilderMini;
const TT2OpFuncT<TT2ConfigMini>* tt2NativeOpTableMini = opTableBuilderMini.table;
template<> const TT2OpFuncT<TT2ConfigMini>* tt2OpTable<TT2ConfigMini>() { return tt2NativeOpTableMini; }
```
(The builder in this TU's anon namespace instantiates all ~272 ops for Mini implicitly.)
**Step 6 — implement (serializer).** In `TT2SceneSerializer.cpp`, after the Full quartet (`:253-256`), add the four `template ...<TT2ConfigMini>;` explicit instantiations (mirror each line, swap `TT2ConfigFull`→`TT2ConfigMini`).
**Step 7 — run, expect PASS.**
**Step 8 — budget the second op table (ARM `.map`).** The engine `≤944` check does NOT cover this — `opTableBuilderMini` is a global in `.ccmram_bss`, and the Mini op bodies are a second `.text` set. `make -C build/stm32/release sequencer`, then: `grep -E 'opTableBuilderMini|tt2NativeOpTableMini' build/stm32/release/*.map` and check the section totals. Expect `.ccmram_bss` +`E_OP__LENGTH`×4 B (~1.7 KB; ~11 KB was free) and `.text` +~42 KB (~360 KB free). Record both deltas in the commit body.
**Step 9 — commit:** `feat(tt2): instantiate Mini config (op table, trait decl+def, serializer)`

---

## Phase 1 — `TT2MiniTrack` model + `TeletypeMini` mode + dispatch

### Task 2: `TT2MiniTrack` model

**Files:** Create `src/apps/sequencer/model/TT2MiniTrack.h` (model `TT2Track.h` exactly, swap `Full`→`Mini`, programs array); Test create `src/tests/unit/sequencer/TestTT2MiniTrack.cpp`.

**Step 1 — failing test.** Create `TestTT2MiniTrack.cpp`:
```cpp
#include "UnitTest.h"
#include "model/TT2MiniTrack.h"

UNIT_TEST("TT2MiniTrack") {
CASE("fits the Track union budget") {
    static_assert(sizeof(TT2MiniTrack) <= 9520, "");
    expect(true, "");
}
CASE("program(scene) wraps modulo SceneCount") {
    TT2MiniTrack t;
    expect(&t.program(0) == &t.program(TT2ConfigMini::SceneCount), "wrap");
    expect(&t.program(1) != &t.program(0), "distinct scenes");
}
}
```
**Step 2 — register + run, expect FAIL** (`TT2MiniTrack.h` not found): add `register_sequencer_test(TestTT2MiniTrack TestTT2MiniTrack.cpp)`; `make -C build sequencer && ctest --test-dir build -R TestTT2MiniTrack -V`
**Step 3 — implement** `TT2MiniTrack.h`: read `TT2Track.h` first; mirror it with `TeletypeProgramT<TT2ConfigMini> _programs[TT2ConfigMini::SceneCount]` + `TT2RuntimeT<TT2ConfigMini> _runtime`. `program(int scene)` returns `_programs[scene % TT2ConfigMini::SceneCount]` (both const/non-const). `clear()` loops `init(_programs[i])` + `init(_runtime)`. `write(VersionedSerializedWriter&)` loops scenes writing each program then ignores runtime (re-init on read); `read(...)` loops + `init(_runtime)`. Add `static_assert(sizeof(TT2MiniTrack) <= 9520, "");`.
**Step 4 — run, expect PASS.**
**Step 5 — commit:** `feat(tt2): TT2MiniTrack model (4 scenes + runtime)`

### Task 3: register `TeletypeMini` in `Track` — enum + all `Track.h` switches

**Files:** `src/apps/sequencer/model/Track.h`. (No standalone test — compile gate; covered by Task 8 round-trip.)

**Step 1 — edit enum:** append `TeletypeMini` AFTER `TeletypeV2`, before `Last` (so `TeletypeMini`=9, `Last`=10).
**Step 2 — container + union:** add `TT2MiniTrack` to the `Container<...>` template list; add `TT2MiniTrack *miniTt2;` to the `_track` pointer union (`Track.h:382-392`, alongside `tt2`); add `const TT2MiniTrack &tt2MiniTrack() const` + non-const accessor (mirror `tt2Track()`).
**Step 3 — exhaustive switches** (no `default:` — add the `TeletypeMini` case to each): `trackModeName` (return `"TT-Mini"`), `trackModeLetter` (e.g. `"m"`), **`trackModeSerialize` → `case TeletypeMini: return 9;`**, `initContainer` (placement-new `TT2MiniTrack`), `setTrackIndex` (forward to `tt2MiniTrack().setTrackIndex` or mirror tt2).
**Step 4 — build:** `make -C build sequencer` (expect clean; a missing switch case is a silent no-op, not an error — eyeball each switch).
**Step 5 — commit:** `feat(model): register TeletypeMini in Track (enum + all Track.h switches)`

### Task 4: `Track.cpp` dispatch switches

**Files:** `src/apps/sequencer/model/Track.cpp`. Add `TeletypeMini` to the **six** real switch sites: `clearPattern`, `copyPattern`, `gateOutputName`, `cvOutputName`, `write`, `read`. Delegate `write`/`read` to `tt2MiniTrack().write/read`; mirror TT2 for the other four. **NOT** `clear()` (no switch) or `duplicatePattern()` (delegates to `copyPattern`).

**Step 1 — implement** each case (read each switch, copy the `TeletypeV2` arm, swap `tt2Track()`→`tt2MiniTrack()`).
**Step 2 — build:** `make -C build sequencer` clean.
**Step 3 — commit:** `feat(model): TeletypeMini cases in Track.cpp dispatch` (round-trip asserted in Task 8).

---

## Phase 2 — Engine: `TT2MiniTrackEngine` + scene switch + routing

### Task 5: `TT2MiniTrackEngine` (boot-in-update, ScopedHost, Seam-2 verbs)

**Files:** Create `src/apps/sequencer/engine/TT2MiniTrackEngine.{h,cpp}` (mirror `TT2TrackEngine.{h,cpp}` **as it is now**); Test create `src/tests/unit/sequencer/TestTT2MiniEngine.cpp` (free-function level — the engine is NOT host-constructible).

**Step 1 — failing test.** Mirror `TestTeletypeV2Metro.cpp` (read it first). Build a `TeletypeProgramT<TT2ConfigMini>` whose script0 sets `M` to a non-default value, `init()` a `TT2RuntimeT<TT2ConfigMini>`, run script0 via the same `runScript`/`runScriptText` helper that file uses (templated on `<TT2ConfigMini>`), assert `runtime.variables.m` == the script's value (not 1000); then call `tt2AdvanceMetro<TT2ConfigMini>(...)` and assert it fires `MetroScript` (index 2). Skeleton:
```cpp
#include "UnitTest.h"
#include "model/TT2Config.h"
#include "model/TeletypeProgram.h"
#include "model/TeletypeRuntime.h"
// + the same engine/runner headers TestTeletypeV2Metro.cpp includes

UNIT_TEST("TT2MiniEngine") {
CASE("script0 boot sets M (scene-load init via script0)") {
    TeletypeProgramT<TT2ConfigMini> p; init(p);
    // load "M 500" into script0 using the same loader TestTeletypeV2Metro uses
    TT2RuntimeT<TT2ConfigMini> rt; init(rt);
    // runScript<TT2ConfigMini>(p, rt, output, 0);
    expectEqual(int(rt.variables.m), 500, "script0 set metro period");
}
}
```
(Use the EXACT helper signatures from `TestTeletypeV2Metro.cpp`, swapping the config type and `MetroScript`/script indices.)
**Step 2 — register + run, expect FAIL.** `register_sequencer_test(TestTT2MiniEngine TestTT2MiniEngine.cpp)`; run.
**Step 3 — implement the engine.** Mirror `TT2TrackEngine.{h,cpp}`:
- Member `TT2MiniTrack &_miniTrack;` + `int _activeScene = -1;` + `bool _firstTick = true;` + `_prevInputState[TT2ConfigMini::TriggerInputCount]`.
- `runScript(idx)` → `::runScript<TT2ConfigMini>(_miniTrack.program(_activeScene), _miniTrack.runtime(), _output, idx)`.
- `update(float dt)`: `ScopedHost host(this);` then **boot block first** (`if (_firstTick){ _firstTick=false; runScript(uint8_t(_miniTrack.program(_activeScene).bootScriptIndex)); }`), then the metro/delay/input body — copy `TT2TrackEngine.h:71-...` verbatim, retemplated.
- `tick()` returns `NoUpdate` (no boot here).
- Implement the three Seam-2 `override`s (`triggerScript`/`toggleScriptMute`/`toggleMetroActive`), bounded to `TT2ConfigMini::ScriptCount`.
- `static_assert(sizeof(TT2MiniTrackEngine) <= 944, "");`
**Step 4 — run, expect PASS** (the free-function boot test). **Engine `update()`/boot wiring is sim/manual — note in commit.**
**Step 5 — commit:** `feat(engine): TT2MiniTrackEngine (boot-in-update, ScopedHost)`

### Task 6: idempotent `changePattern()` scene switch + pure helper

**Files:** `src/apps/sequencer/engine/TT2MiniTrackEngine.{h,cpp}`; Test extend `TestTT2MiniEngine.cpp`.

**Step 1 — failing test** (the pure scene-math helper — the only host-testable part):
```cpp
CASE("tt2SceneIndex wraps modulo") {
    expectEqual(tt2SceneIndex(0, 4), 0, "");
    expectEqual(tt2SceneIndex(5, 4), 1, "");
    expectEqual(tt2SceneIndex(7, 4), 3, "");
}
```
(Include the engine header for the free helper declaration.)
**Step 2 — run, expect FAIL** (`tt2SceneIndex` undefined).
**Step 3 — implement.** Free helper in the engine header: `inline int tt2SceneIndex(int pattern, int sceneCount) { return pattern % sceneCount; }`. Then:
```cpp
void TT2MiniTrackEngine::changePattern() {
    int scene = tt2SceneIndex(pattern(), TT2ConfigMini::SceneCount);
    if (scene == _activeScene) return;
    _activeScene = scene;
    init(_miniTrack.runtime());
    _firstTick = true;   // re-run scene's script0 on next update()
}
```
Ctor: `_activeScene=-1`, then call `changePattern()` once to seed (base ctor's virtual won't reach the override; the runtime dispatch at `Engine.cpp:1067` does).
**Step 4 — run, expect PASS.** (Idempotency guard + init-on-switch is engine-level → sim/manual.)
**Step 5 — commit:** `feat(engine): idempotent mini scene switch (modulo wrap)`

### Task 7: engine `Container` + construction + routing + external dispatch

**Files:** `src/apps/sequencer/engine/Engine.h`, `src/apps/sequencer/engine/Engine.cpp`, plus external page switches.

**Step 1 — container:** add `TT2MiniTrackEngine` to `TrackEngineContainer` (`Engine.h:46`) — **omitting it overruns the placement-new buffer** (silent).
**Step 2 — construct:** add the `case TeletypeMini:` to the construction switch (`Engine.cpp:~588`) → `_trackEngineContainer.create<TT2MiniTrackEngine>(...)` mirroring TT2.
**Step 3 — routing:** change `isTt2[t] = mode==TeletypeV2` (`Engine.cpp:632`) to `(mode==TeletypeV2 || mode==TeletypeMini)` so Mini outputs route through the TT2 jack layer.
**Step 4 — external switches:** add a `TeletypeMini` case to the `switch(trackMode)` sites that lack a `default:` — `TopPage` (3 switches), `TrackPage`, `TeletypePatternViewPage` (block Mini on the pattern page). Sites WITH a `default:` (`ClipBoard`, `GeneratorPage`, `LaunchpadController`) need none. Grep to confirm: `grep -rn "TrackMode::TeletypeV2" src/apps/sequencer/ui src/apps/sequencer/engine`.
**Step 5 — build all three:** `make -C build sequencer && make -C build/sim/debug sequencer && make -C build/stm32/release sequencer` (0 errors).
**Step 6 — commit:** `feat(engine): register + route + dispatch TeletypeMini`

---

## Phase 3 — Project serialization round-trip

### Task 8: round-trip at the `Track` level (mode dispatch, not just the model)

**Files:** Test create `src/tests/unit/sequencer/TestTT2MiniSerialize.cpp`. **Round-trip a `Track`, NOT a bare `TT2MiniTrack`** — the risky code is `Track::trackModeSerialize` (mode→9), `Track::write`, `Track::read` (`Track.cpp`). A direct `TT2MiniTrack` round-trip passes even if the mode serializes as 0 or reads back as the wrong mode.

**Step 1 — failing test** (read an existing `Track` write/read test for the `VersionedSerializedWriter`/`Reader` + buffer idiom; mirror it):
- Construct a `Track`, set mode to `Track::TrackMode::TeletypeMini`, fill 4 *distinct* scenes via `track.tt2MiniTrack().program(n)` (a per-scene marker — e.g. distinct script0 length or a marker command).
- `track.write(writer)` to a buffer, `track.read(reader)` into a *fresh* `Track`.
- **Assert: `fresh.trackMode() == TeletypeMini`** (catches a missing `trackModeSerialize`/`initContainer` case — would come back as Note), AND each of the 4 scenes round-trips to its own content and the four stay distinct.
**Step 2 — register + run, expect FAIL** (Task-3/4 gap surfaces here: wrong mode back, or blank scenes).
**Step 3 — fix** any Task-3/4 gap surfaced (likely a missing `trackModeSerialize`/`write`/`read` case). No new code if 3/4 were complete.
**Step 4 — run, expect PASS.**
**Step 5 — commit:** `test(tt2): mini Track-level mode + 4-scene round-trip`

---

## Phase 4 — UI (v1: edit only; no SD I/O; no live-exec)

> **Scope guard:** mode selection + script/IO editing of the active scene + scene indicator. **No SD scene save/load** (project blob persists it). **No live-exec** for Mini. Pattern page blocks Mini.

### Task 9: expose `TeletypeMini` in track-mode selection

**Files:** the track-mode selection list/page (grep `TrackMode::TeletypeV2` in `ui/pages/TrackPage.cpp` + the mode-name list).
**Step 1 — implement:** ensure `TeletypeMini` appears as a selectable mode (Task 3's `trackModeName` already provides the label; verify the selector iterates to `Last` so the new value shows).
**Step 2 — build sim:** `make -C build/sim/debug sequencer`; manually confirm the mode is selectable.
**Step 3 — commit:** `feat(ui): expose TeletypeMini in track mode selection`

### Task 10: route script/IO pages to the active mini scene (dual path)

**Files:** `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp`, `src/apps/sequencer/ui/pages/TT2IoConfigPage.cpp`, `src/apps/sequencer/engine/TT2UiAccess.h`. **This is the correctness-critical task — wrong-union reads are silent, not compile errors.**

**Step 1 — thread the scene index into Seam-1.** `TT2UiAccess.h` accessors take only `Track&` and hardwire `tt2Track().program()` (single program). Extend the accessor signatures with a `int scene` param (Full passes 0; Mini passes the active scene), and inside, branch `trackMode()`: `TeletypeV2` → `tt2Track().program()`, `TeletypeMini` → `tt2MiniTrack().program(scene)`. The page computes `scene = pattern() % TT2ConfigMini::SceneCount` from the track state it already holds.
**Step 2 — enumerate every hazard** in `TeletypeScriptViewPage.cpp`: `!= TeletypeV2` guards at `:72,:78,:353,:396,:417,:476,:726`, `as<TT2TrackEngine>()` casts at `:109,:234,:735` (incl. `drawHud` `:234`), **and the direct `tt2Track()` derefs in the edit-action paths — commit/duplicate/comment/delete/undo/save/load at `:744,:776,:796,:811,:834`** — plus `TT2IoConfigPage`'s. For EACH: redirect to `tt2MiniTrack()` / `as<TT2MiniTrackEngine>()`, OR hard-block Mini *before* the deref. **Never widen a guard to admit Mini while leaving a downstream `tt2Track()`/`as<TT2TrackEngine>()`.** List each site + its decision in the commit body.
**Step 2b — grep gate (must pass before commit):** after wiring, every remaining `tt2Track()` / `as<TT2TrackEngine>()` in a Mini-admitted code path must be classified (redirected or provably Full-only). Run `grep -nE 'tt2Track\(\)|as<TT2TrackEngine>' src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp src/apps/sequencer/ui/pages/TT2IoConfigPage.cpp` and confirm no unclassified site reachable when `trackMode()==TeletypeMini`.
**Step 3 — IO page:** `TT2IoConfigPage` for Mini shows 2 trigger rows (`Cfg::TriggerInputCount`), 6 CV-in, 8 CV-out. **Exclude live-exec (`runLiveCommand`, `:735`) and SD I/O for Mini.**
**Step 4 — render** any touched layout with `ui-preview/` (per CLAUDE.md OLED rule) and `open` the PNG.
**Step 5 — build sim + host + ARM:** all 0 errors.
**Step 6 — commit:** `feat(ui): script/IO pages edit the mini active scene` (body: per-site guard/cast decisions).

### Task 11: scene indicator

**Files:** `TeletypeScriptViewPage.cpp` (or the relevant Mini view). Show `pattern() % TT2ConfigMini::SceneCount` as a minimal chip.
**Step 1 — implement** the indicator; **render with `ui-preview/`** into `ui-preview/<slug>/` and `open` it before review (OLED rule).
**Step 2 — build sim; commit:** `feat(ui): show active mini scene index`

---

## Verification gate (run before finishing the branch)

- Full host suite green: `ctest --test-dir build` — all `TestTeletypeV2*`/`TestTT2*` + new `TestTeletypeMini`/`TestTT2MiniTrack`/`TestTT2MiniEngine`/`TestTT2MiniSerialize`.
- **Full TT2 unchanged** (additive): `sizeof(TeletypeProgramT<Full>)==3638`, `TT2RuntimeT<Full>==5880` still assert; `Track`/engine sizes unchanged on the ARM `.map`.
- `sizeof`: `TeletypeProgramT<Mini>`<1700, `TT2RuntimeT<Mini>`<2600, `TT2MiniTrack`≤9520 (conservative; real union cap is NoteTrack 9544 per PROJECT.md:293), `TT2MiniTrackEngine`≤944.
- **Second op table budget:** ARM `.map` shows `.ccmram_bss` and `.text` deltas within free space (Task 1 Step 8) — the engine `≤944` check does NOT cover the global op table.
- Op-table coverage parity (Mini non-null wherever Full is).
- Scene-switch: `p % SceneCount`; same-pattern `changePattern()` no-op; script0 runs on boot/scene-load so the metro period is the scene's, not 1000 (sim).
- Output on the TT2 jack layer; serialization round-trip; Mini does NOT persist as the fall-through mode.
- `make -C build/stm32/release sequencer` + `make -C build/sim/debug sequencer` clean.
- Then: **superpowers:finishing-a-development-branch**.

## Deferred (propose only, do not build)
- SD scene save/load + live-exec for Mini (`FileManager`/`gTeletypeLoadScratch` overloads).
- Mini pattern editing (pattern page is Full-only in v1).
- "Supermini" with smaller pattern dims (now possible — dims are config traits).
