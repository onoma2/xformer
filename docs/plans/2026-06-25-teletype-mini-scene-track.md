# Teletype Mini Scene-Track Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add a second Teletype track mode ("TT-Mini": 2 scripts + metro, 8-deep delay) that stores 4 scenes and switches between them via the w-pattern selector (modulo-wrap), coexisting with the full 10-script TT2 track.

**Architecture:** Instantiate a second config `TT2ConfigMini` against the already-merged `template<Cfg>` TT2 core, add a `TT2MiniTrack` model (`programs[SceneCount]` + one runtime), a `TeletypeMini` `TrackMode`, dispatch/routing/container wiring, a `TT2MiniTrackEngine`, and the UI seam's Mini branch. `changePattern()` just retargets `programs[pattern() % SceneCount]` — switching is **seamless**: the shared runtime carries, no reset, no re-boot (see the Scene-switch contract).

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
1. **Boot/INIT runs in `update()` on `_firstTick`, NOT `tick()`** (`TT2TrackEngine.h:67-69`) — the metro free-runs on wall-clock, so init must apply transport-independently. The Mini engine boots the same way: script0 runs once at **track start** (and on transport `reset()`). **Scene switch does NOT re-arm `_firstTick`** — switching is seamless (see the contract below).
2. **`X/Y/Z/T` default to `None`** (scratch) — the Mini program inherits this; scripts can use X/Y as counters.

## Config decision — `ScriptCount=3`, no init slot (RESOLVED)

The real `Track`-union cap is **NoteTrack = 9544 B**, not TT2Track's 9520 (PROJECT.md:251,293 — the 8-track `Track::_container` is sized by NoteTrack; a model below 9544 B adds no Model RAM). So `ScriptCount=4` (TT2MiniTrack=9522 ≤ 9544) *would* fit for free. We still choose **`ScriptCount=3`**: boot runs `runScript(bootScriptIndex)` unconditionally and `bootScriptIndex = InitScript>=0 ? InitScript : 0` (`TeletypeProgram.h:122`) → with `InitScript=-1`, **script0 runs once at track start** (the boot/init hook — set the *initial* `M`/`MO.*`/output shaping there). **script0 also fires as trigger-1.**

Because scene switching is **seamless** (no re-boot — see the contract), script0 does NOT re-run on switch. So **per-scene config that must change on switch belongs in the metro script** (index 2), which runs every metro tick on the *active* scene: e.g. a scene that needs a different tempo sets `M` in its metro script, applied on the next tick after the switch. script0 is just the one-time track boot.

**`TT2ConfigMini`:** `ScriptCount=3, DelayDepth=8, TriggerInputCount=2, MetroScript=2, InitScript=-1, SceneCount=4, PatternCount=4, PatternLength=64`. Sizes: `TeletypeProgramT<Mini>`≈1518, `TT2RuntimeT<Mini>`≈2192, `TT2MiniTrack`≈8.3 KB (≤9544 NoteTrack cap — ample headroom; the `<=9520` asserts below are a conservative TT2-family cap, not the union cliff). Pattern dims stay 4×64.

**Execution note:** the per-task `commit` steps require the user's explicit go-ahead **at execution time** — this plan text does not itself grant commit permission (repo policy requires confirmation for `git add`/`commit`). The controller obtains that go-ahead before starting the run.

## Scene-switch contract — SEAMLESS (the behavioral spec for `changePattern()`)

A Mini track is one `TT2MiniTrack` = `programs[4]` (per-scene) + **one shared** `runtime`. The w-pattern selector picks `scene = pattern() % 4`. Switching scenes is **seamless**: it changes *only which program is active*; it does **not** reset anything.

| State | Where it lives | On scene switch |
|---|---|---|
| 3 scripts, 4×64 tt-patterns, trigger/CV-in sources, output shaping | per-scene, in `programs[scene]` | **swapped** (the new scene's now active) |
| variables (A/B/X/…), 8-deep delay queue, metro accumulator + period `M`, `M.ACT` | shared, in the single `runtime` | **carry unchanged** (NOT reset) |
| CV/gate outputs (`_output`) | engine | hold last value until a script rewrites them |
| `_firstTick` / boot | engine | **not re-armed** — script0 does NOT re-run on switch |

Consequences (intended): switching scenes is continuous — the metro keeps its current tempo and phase, counters/delays keep running, you're simply now executing the new scene's scripts against the new scene's tt-patterns. A scene's script0 config (`M`/`MO.*`) applies only at **track start** (boot) or when script0 is otherwise fired; to change tempo/config *on* a switch, put it in the **metro script** (runs each tick on the active scene). tt-pattern edits a script makes persist in that scene's program; variables are a shared space across scenes (by design — that's what "seamless" buys).

`changePattern()` is therefore trivial — just retarget the active scene (Task 6).

**Transport `reset()` is partial, mirroring TT2** (`TT2TrackEngine.h:38-44`): it clears `_output`, the delay queue (`tt2DelayClear`), the metro accumulator (`_msAccum`/`_metroAccumMs`), and re-arms boot (`_firstTick=true` → script0 re-runs at next `update()`). It does **NOT** `init(runtime)` — so variables, the script stack, MIDI bindings, RNG, and `scriptLastMs` **persist through a transport stop**, exactly as Full TT2 today. script0 resets whatever it explicitly sets on the re-boot. (If a full runtime wipe on stop is ever wanted, that's a deliberate `init(_miniTrack.runtime())` added to Mini's `reset()` — NOT the default; the default mirrors TT2.)

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
**Step 5 — commit:** stage ONLY this task's files (never `git add -A`): `git add src/apps/sequencer/model/TT2Config.h src/tests/unit/sequencer/TestTT2Config.cpp && git commit -m "feat(tt2): add TT2ConfigMini traits"`.

> **All tasks:** stage only the explicit files you changed; **never `git add -A`** (the working tree holds unrelated `.scratch/`/`docs/`/`ui-preview/` WIP that must not be committed). To build+run a unit test, build its target first — `make sequencer` builds firmware, NOT tests: `make -C build <TestName> && ctest --test-dir build -R <TestName> -V`.

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

### Task 1b: make init-index ops config-safe for `InitScript = -1`

**Files:** `engine/TeletypeNativeOps.cpp`; Test extend `TestTeletypeMini.cpp`. **Why:** several ops use `Cfg::InitScript` as a *value* or *bound*. With Mini's `InitScript=-1` they index `[-1]` or disable everything. These are shared templated ops — the fixes are **equivalent for Full** (Full's `InitScript=9 == ScriptCount-1`), so Full behavior and tests are unchanged. **Audit first:** `grep -nE 'Cfg::InitScript' src/apps/sequencer/engine/TeletypeNativeOps.cpp` — today that's `opLast` (`:238`) and `opMiDollar` (`:2445`); fix every hit and re-grep after.

**Step 1 — failing tests** (in `TestTeletypeMini.cpp`, using a `TT2RuntimeT<Mini>` + the Mini ops):
- `LAST 0` (init script): with no init script, must **return 0**, not read `scriptLastMs[-1]`. Assert `opLast`-via-`runScript` on Mini with arg 0 pushes 0 and doesn't OOB.
- `MI.$ 1 1` (bind MIDI on-event to script 1): on Mini must **bind script 0** (`1-1`), not disable. Assert `runtime.midi.on_script == 0` after, and a script in `0..ScriptCount-1` is accepted.
**Step 2 — run, expect FAIL/UB** (the LAST case is an OOB read; the MI.$ case binds -1).
**Step 3 — fix `opLast`** (`:236-238`): guard the init mapping —
```cpp
int sn = a - 1;
if (sn < -1 || sn >= Cfg::ScriptCount) { pushStack(stack, stackSize, 0, error); return; }
if (sn == -1) {
    if (Cfg::InitScript < 0) { pushStack(stack, stackSize, 0, error); return; }  // no init script
    sn = Cfg::InitScript;
}
```
**Step 4 — fix `opMiDollar`** (`:2445`): bound by script count, not the init index —
```cpp
if (script < 0 || script >= Cfg::ScriptCount) script = -1;   // was: script > Cfg::InitScript
```
(For Full, `ScriptCount-1 == InitScript`, so identical behavior.)
**Step 5 — run Mini + Full TT2 suites, expect PASS** (`ctest --test-dir build -R 'TestTeletypeMini|TestTeletypeV2|TestTT2'`). Re-grep `Cfg::InitScript` — every remaining use must be provably safe for `< 0` (e.g. `opInitTime` loops `0..ScriptCount`, fine).
**Step 6 — commit:** `fix(tt2): init-index ops (LAST 0, MI.\$) safe when InitScript<0`

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

**Files:** `src/apps/sequencer/model/Track.cpp`. Add `TeletypeMini` to the **six** real switch sites: `clearPattern`, `copyPattern`, `gateOutputName`, `cvOutputName`, `write`, `read`. **NOT** `clear()` (no switch) or `duplicatePattern()` (delegates to `copyPattern`).

**Decision — `clearPattern`/`copyPattern` are explicit no-ops for Mini in v1** (mirror TT2's empty `case` at `Track.cpp:~40,~73`). Rationale: the w-pattern selector only *selects* a scene (`pattern() % SceneCount`); the 16 w-pattern slots alias onto 4 scenes, so wiring clear/copy to "the mapped scene" introduces aliasing surprises (slot 0 and slot 4 are the same scene; copy 0→4 is a no-op). Scenes are edited via the script editor (Task 10), not the pattern UI. This is SAFE (no data loss, no crash) and matches the v1 "edit only" scope. **The plan states this explicitly so the executor does not assume pattern clear/copy manages scenes.** (Wiring them to real scene clear/copy is in Deferred.)
- `write`/`read` → delegate to `tt2MiniTrack().write/read`.
- `gateOutputName`/`cvOutputName` → mirror the TT2 arm (output naming is identical).

**Step 1 — implement** each case per the above (clear/copy = empty `break;`; write/read/names mirror/delegate).
**Step 2 — build:** `make -C build sequencer` clean.
**Step 3 — commit:** `feat(model): TeletypeMini cases in Track.cpp dispatch (pattern clear/copy no-op in v1)` (round-trip asserted in Task 8).

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
CASE("script0 boot sets M (track-start init via script0)") {
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
- **Swap hardcoded Full constants** in the mirrored `.cpp`: `TT2_SCRIPT_COUNT` (`TT2TrackEngine.cpp:137` script-index guard, `:358` MIDI fire path) → `TT2ConfigMini::ScriptCount`; any `TT2_METRO_SCRIPT` → `MetroScript`. `InitScript=-1` has no slot — guard any init-index use with `>= 0` first. Grep the mirror: `grep -nE 'TT2_SCRIPT_COUNT|TT2_INIT_SCRIPT|TT2_METRO_SCRIPT' ` your new `.cpp` must return nothing.
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
**Step 3 — implement (SEAMLESS — see the Scene-switch contract).** Free helper in the engine header: `inline int tt2SceneIndex(int pattern, int sceneCount) { return pattern % sceneCount; }`. Then `changePattern()` only retargets the active scene — **no `init(runtime)`, no `_firstTick`**; the shared runtime (variables/delay/metro/`M`) carries across the switch:
```cpp
void TT2MiniTrackEngine::changePattern() {
    _activeScene = tt2SceneIndex(pattern(), TT2ConfigMini::SceneCount);
    // seamless: runtime carries, no re-boot. Metro keeps tempo/phase;
    // a scene changes config via its metro script, not on switch.
}
```
Ctor: `_activeScene=-1`, then call `changePattern()` once to seed the active scene (base ctor's virtual won't reach the override; the runtime dispatch at `Engine.cpp:1067` does). Boot (`_firstTick`) is armed by `reset()`/ctor and runs script0 once at first `update()` — Task 5.
**Step 4 — run, expect PASS** (the pure helper). The seamless-carry behavior (switch mid-run → metro tempo + variables + delays continue; new scene's scripts now active) is engine-level → **sim/manual**.
**Step 5 — commit:** `feat(engine): seamless mini scene switch (modulo wrap, runtime carries)`

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

## Phase 4 — UI (v1: edit + interactive script-firing; no command-line live eval; no SD I/O)

> **Scope guard — one rule for "live":** Mini is a *playable* engine track, so the UI **allows interactive firing of its stored scripts**: trigger/function keys (`:414`), metro toggle, and script mute — all via the Mini engine's Seam-2 verbs (`triggerScript`/`toggleScriptMute`/`toggleMetroActive`, implemented in Task 5). What's **banned for Mini v1** is **command-line live eval** — `runLiveCommand` (`:735`, the ad-hoc typed-command entry/`:903` live-input path) — and **SD scene save/load** (project blob persists scenes). The pattern page blocks Mini. So: stored-script execution = allowed; ad-hoc command eval = deferred.

### Task 9: expose `TeletypeMini` in track-mode selection

**Files:** the track-mode selection list/page (grep `TrackMode::TeletypeV2` in `ui/pages/TrackPage.cpp` + the mode-name list).
**Step 1 — implement:** ensure `TeletypeMini` appears as a selectable mode (Task 3's `trackModeName` already provides the label; verify the selector iterates to `Last` so the new value shows).
**Step 2 — build sim:** `make -C build/sim/debug sequencer`; manually confirm the mode is selectable.
**Step 3 — commit:** `feat(ui): expose TeletypeMini in track mode selection`

### Task 10: route script/IO pages to the active mini scene (dual path)

**Files:** `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp`, `src/apps/sequencer/ui/pages/TT2IoConfigPage.cpp`, `src/apps/sequencer/engine/TT2UiAccess.h`. **This is the correctness-critical task — wrong-union reads are silent, not compile errors.**

**Step 1 — thread the scene index into Seam-1.** `TT2UiAccess.h` accessors take only `Track&` and hardwire `tt2Track().program()` (single program). Extend the accessor signatures with a `int scene` param (Full passes 0; Mini passes the active scene), and inside, branch `trackMode()`: `TeletypeV2` → `tt2Track().program()`, `TeletypeMini` → `tt2MiniTrack().program(scene)`. The page computes `scene = pattern() % TT2ConfigMini::SceneCount` from the track state it already holds.

**Step 1b — config-drive the script-index keys (metro/init/trigger).** The editor hardwires Full's `TT2_METRO_SCRIPT`(8)/`TT2_INIT_SCRIPT`(9) for the script labels (`:96-98`), the metro↔init nav toggle (`:432`), and a function-key map assuming 8 numbered scripts (`:427`). For Mini (`MetroScript=2, InitScript=-1, ScriptCount=3`):
- **Metro key → the active config's `MetroScript`** (script 2 for Mini, 8 for Full) — read it via a Seam-1 accessor, not the `TT2_*` constant.
- **INIT must be unreachable for Mini** (`InitScript < 0`): the metro↔init toggle (`:432`) must NOT select a non-existent init script; the INIT label branch (`:98`) never fires.
- **Function/trigger keys bounded to `ScriptCount`** (F1/F2 → script0/script1 for Mini; F3-F8 inert). `setScriptIndex` (`:637-643`) already clamps to `tt2ScriptCount(...)` — verify the *key handlers* feeding it also respect the bound so no key can land on script ≥ `ScriptCount`.
Drive all three from the config (via Seam-1: `tt2MetroScriptIndex()`/`tt2InitScriptIndex()` returning `Cfg::MetroScript`/`Cfg::InitScript`, or extend existing accessors) — do not branch on hardcoded `TT2_METRO_SCRIPT`/`TT2_INIT_SCRIPT`.
**Step 2 — enumerate every hazard** in `TeletypeScriptViewPage.cpp`: `!= TeletypeV2` guards at `:72,:78,:353,:396,:417,:476,:726`, `as<TT2TrackEngine>()` casts at `:109,:234,:735` (incl. `drawHud` `:234`), **and the direct `tt2Track()` derefs in the edit-action paths — commit/duplicate/comment/delete/undo/save/load at `:744,:776,:796,:811,:834`** — plus `TT2IoConfigPage`'s. For EACH: redirect to `tt2MiniTrack()` / `as<TT2MiniTrackEngine>()`, OR hard-block Mini *before* the deref. **Never widen a guard to admit Mini while leaving a downstream `tt2Track()`/`as<TT2TrackEngine>()`.** List each site + its decision in the commit body.
**Step 2b — grep gate (must pass before commit):** after wiring, every remaining `tt2Track()` / `as<TT2TrackEngine>()` in a Mini-admitted code path must be classified (redirected or provably Full-only). Run `grep -nE 'tt2Track\(\)|as<TT2TrackEngine>' src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp src/apps/sequencer/ui/pages/TT2IoConfigPage.cpp` and confirm no unclassified site reachable when `trackMode()==TeletypeMini`.
**Step 3 — IO page:** `TT2IoConfigPage` for Mini shows 2 trigger rows (`Cfg::TriggerInputCount`), 6 CV-in, 8 CV-out. **Allowed for Mini:** the script trigger/function keys + metro toggle (Seam-2 verbs, Step 1b). **Banned for Mini v1:** `runLiveCommand` (`:735`/`:903`, ad-hoc command-line eval) and SD I/O — hard-block those before their deref.
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
- Scene-switch is **seamless**: `changePattern()` retargets `p % SceneCount` only; the shared runtime (variables/delay/metro tempo+phase) carries across — switching mid-run does NOT reset or reboot (sim). script0 boots once at track start (and on transport `reset()`); per-scene tempo/config lives in the metro script.
- Output on the TT2 jack layer; serialization round-trip; Mini does NOT persist as the fall-through mode.
- `make -C build/stm32/release sequencer` + `make -C build/sim/debug sequencer` clean.
- Then: **superpowers:finishing-a-development-branch**.

## Deferred (propose only, do not build)
- Command-line live eval (`runLiveCommand`) + SD scene save/load for Mini (`FileManager`/`gTeletypeLoadScratch` overloads). (Stored-script firing via trigger keys IS in v1.)
- Mini scene clear/copy via the pattern UI (v1 no-ops `clearPattern`/`copyPattern`; wiring them to "the mapped scene" needs an aliasing-aware design — 16 slots → 4 scenes).
- Mini pattern editing (pattern page is Full-only in v1).
- "Supermini" with smaller pattern dims (now possible — dims are config traits).
