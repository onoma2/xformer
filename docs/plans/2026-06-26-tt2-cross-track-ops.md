# TT2 Cross-Track Ops — Implementation Plan (TDD)

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to execute this plan. Work one task at a time, run each task's RED step before the GREEN step, stop at every review checkpoint. Do NOT skip the run-to-fail. Do NOT `git add -A` — stage only the files each task names. Do NOT commit unless a task's commit step says to (the human reviews first if so instructed).

Source spec: `docs/superpowers/specs/2026-06-26-tt2-cross-track-ops.md` (read it before starting).

## Shared-file note (read before Task 2)

This plan touches `engine/TT2Host.h` and the **4 fake-host test files** — `TestTeletypeV2Modulator.cpp`, `TestTeletypeV2Geode.cpp`, `TestTeletypeV2EnvelopeOps.cpp`, `TestTeletypeV2LfoOps.cpp` — which it **shares with the TV-bank plan** (`2026-06-24` modulator/leibniz work). Each fake host is a `struct …Host : TT2Host` that overrides every pure-virtual. Adding pure-virtuals to `TT2Host` forces a stub in each. If both plans are built, run them **sequentially**: whichever runs second adds its host methods + its stubs to the already-modified files. No merge conflict — additive only — but parallel edits to the same lines will collide.

## Goal

Two cross-track Teletype ops so any TT2-family track (full or Mini) reaches another TT2 track's data, mirroring the `WNG`/`WNN` Note-track cross-track ops:

- **`WPN <track> <bank> <idx> [v]`** — get/set cell `idx` of pattern `bank` on `track`'s active program (Mini → its active scene). True cross-track `PN`: identical `normalisePn` (bank) + `normaliseIdx` (idx, negative-counts-back-from-`p.len`) normalisation.
- **`W.SCRIPT <track> <n>`** — trigger script `n` (1-based, `1..Cfg::ScriptCount`) on `track`, run with that track installed as the active host so its own ops resolve against it; out-of-range → `OutOfRange`, recursion past the cross-track cap → `ExecDepthOverflow`, both propagated to the op's `error`.

Op syntax is 1-based; ops subtract 1 so host track args are zero-based (script stays 1-based across the host boundary, `-1` happens inside `triggerScriptFromHost`).

## Architecture

- **`TT2Host`** (`engine/TT2Host.h`) is the cross-track bridge: a pure-virtual interface, overridden separately by `TT2TrackEngine` and `TT2MiniTrackEngine` (no shared base). Ops reach it via `tt2ActiveHost()`.
- New work adds **3 host methods**: `hostTrackPatternVal` (get), `hostSetTrackPatternVal` (set), `hostTriggerTrackScript`. Each engine override forwards to a **shared header helper** so the body is written once, not duplicated across the two `.cpp`s.
- Pattern get/set go through `tt2CrossPatternCell(model, t, bank, idx)` in the new header `engine/TT2HostCrossTrack.h`, returning the cell `int16_t*` after PN-style normalisation, or `nullptr` for a non-TT2 target (op then reads 0 / writes nothing).
- Script trigger needs the **target** engine installed as active host (a bare `runScript` would leave the caller's host active, breaking the target script's `W*`/`BUS`/`MO` ops). Each engine gets a **public** `triggerScriptFromHost` wrapping its own (private) `ScopedHost`. A cross-track recursion cap (`uint8_t _tt2CrossDepth` on `Engine`, RAII-guarded) bounds aggregate C++ stack depth across an N-track cycle.

## Tech stack / build commands

- C++14, STM32 firmware + sim + host unit tests.
- **`make sequencer` builds firmware, NOT tests.** Host unit tests are CMake targets registered by `register_sequencer_test` (`src/tests/unit/sequencer/CMakeLists.txt`). Build + run one:
  `make -C build <TestName> && ctest --test-dir build -R <TestName> -V`
- **Op registration via tooling only** — edit `teletype/src/ops/op_enum.h` + `teletype/src/match_token.rl`, then:
  `( cd teletype/src && ragel -C -G2 match_token.rl -o match_token.c )` then `python3 teletype/utils/tt2_op_names.py`.
  **Never hand-edit `match_token.c` (gitignored) or `TT2OpNames.cpp` (generated).**
- **Trust `make`/`ctest`, not IDE diagnostics.** Never `cd` before git/make (use `make -C build …`); the ragel regen above is the one allowed `( cd … )` subshell.
- **Stage only the files each task lists. Never `git add -A`.**

Path convention: `engine/…` and `model/…` are under `src/apps/sequencer/`; `teletype/…` and `src/tests/…` are repo-root-relative.

---

## Task 1 — Shared cross-track pattern-cell helper (`tt2CrossPatternCell`)

Host-testable directly against a `Model` with a Mini track. PN-exact normalisation, replicated (not refactored from the originals).

**Files:**
- `src/apps/sequencer/engine/TT2HostCrossTrack.h` (new)
- `src/tests/unit/sequencer/TestTT2HostCrossTrack.cpp` (new)
- `src/tests/unit/sequencer/CMakeLists.txt` (register the new test)

### RED — write the failing test

`src/tests/unit/sequencer/TestTT2HostCrossTrack.cpp`:
```cpp
#include "UnitTest.h"

#include "engine/TT2HostCrossTrack.h"
#include "model/Model.h"

UNIT_TEST("TT2HostCrossTrack") {

CASE("mini target: write+read active-scene cell") {
    Model model; model.clear();
    auto &project = model.project();
    project.track(1).setTrackMode(Track::TrackMode::TeletypeMini);
    // active scene = playState pattern % SceneCount; default pattern 0 → scene 0
    int16_t *cell = tt2CrossPatternCell(model, 1, 0, 5);
    expectTrue(cell != nullptr, "mini cell resolves");
    *cell = 42;
    expectEqual(*tt2CrossPatternCell(model, 1, 0, 5), int16_t(42), "read back");
    expectEqual(project.track(1).tt2MiniTrack().program(0).patterns[0].val[5],
                int16_t(42), "landed in scene 0 pattern 0 idx 5");
}

CASE("negative idx counts back from p.len (NOT clamp-to-0)") {
    Model model; model.clear();
    auto &project = model.project();
    project.track(0).setTrackMode(Track::TrackMode::TeletypeV2);
    auto &prog = project.track(0).tt2Track().program();
    // default len = PatternLength (whole pattern); -1 → len-1
    int len = prog.patterns[0].len;
    int16_t *cell = tt2CrossPatternCell(model, 0, 0, -1);
    expectTrue(cell != nullptr, "v2 cell resolves");
    expectEqual(cell, &prog.patterns[0].val[len - 1], "idx -1 == len-1");
}

CASE("bank clamps like normalisePn; non-tt2 track → nullptr") {
    Model model; model.clear();
    auto &project = model.project();
    project.track(0).setTrackMode(Track::TrackMode::TeletypeV2);
    auto &prog = project.track(0).tt2Track().program();
    expectEqual(tt2CrossPatternCell(model, 0, 99, 0),
                &prog.patterns[TT2ConfigFull::PatternCount - 1].val[0], "bank over → last");
    expectEqual(tt2CrossPatternCell(model, 0, -3, 0),
                &prog.patterns[0].val[0], "bank under → 0");
    project.track(2).setTrackMode(Track::TrackMode::Note);
    expectTrue(tt2CrossPatternCell(model, 2, 0, 0) == nullptr, "Note track → nullptr");
    expectTrue(tt2CrossPatternCell(model, 99, 0, 0) == nullptr, "OOB track → nullptr");
}

}
```

Register it in `src/tests/unit/sequencer/CMakeLists.txt` (alongside the other `register_sequencer_test` lines, e.g. after `TestTT2Config`):
```cmake
register_sequencer_test(TestTT2HostCrossTrack TestTT2HostCrossTrack.cpp)
```

### Run-to-fail
```
make -C build TestTT2HostCrossTrack
```
Expected: **compile failure** — `engine/TT2HostCrossTrack.h: No such file or directory` (the helper header doesn't exist yet).

### GREEN — minimal implementation

`src/apps/sequencer/engine/TT2HostCrossTrack.h`:
```cpp
#pragma once

#include "model/Model.h"
#include "model/TeletypeProgram.h"

// Cross-track PN-cell resolver shared by both TT2 engines' host overrides.
// bank/idx are SIGNED and normalised exactly like same-track PN (normalisePn on
// bank, normaliseIdx — negative-counts-from-p.len — on idx). Returns the cell
// pointer, or nullptr if the target isn't a TT2-family track (op no-ops).

template<typename Cfg>
static inline int16_t *tt2CrossPatternCellImpl(TeletypeProgramT<Cfg> &program, int16_t bank, int16_t idx) {
    if (bank < 0) bank = 0;
    else if (bank >= Cfg::PatternCount) bank = Cfg::PatternCount - 1;
    auto &p = program.patterns[bank];
    int16_t len = int16_t(p.len);
    if (idx < 0) {
        if (idx < -len) idx = 0;
        else idx = len + idx;
    }
    if (idx >= Cfg::PatternLength) idx = Cfg::PatternLength - 1;
    if (idx < 0) idx = 0;
    return &p.val[idx];
}

static inline int16_t *tt2CrossPatternCell(Model &model, int t, int16_t bank, int16_t idx) {
    if (t < 0 || t >= CONFIG_TRACK_COUNT) return nullptr;
    Track &tk = model.project().track(t);
    if (tk.trackMode() == Track::TrackMode::TeletypeV2)
        return tt2CrossPatternCellImpl<TT2ConfigFull>(tk.tt2Track().program(), bank, idx);
    if (tk.trackMode() == Track::TrackMode::TeletypeMini) {
        int scene = model.project().playState().trackState(t).pattern() % TT2ConfigMini::SceneCount;
        return tt2CrossPatternCellImpl<TT2ConfigMini>(tk.tt2MiniTrack().program(scene), bank, idx);
    }
    return nullptr;
}
```

### Run-to-pass
```
make -C build TestTT2HostCrossTrack && ctest --test-dir build -R TestTT2HostCrossTrack -V
```
Expected: `TestTT2HostCrossTrack` passes (3 cases).

### Commit (stage only these files)
```
git add src/apps/sequencer/engine/TT2HostCrossTrack.h \
        src/tests/unit/sequencer/TestTT2HostCrossTrack.cpp \
        src/tests/unit/sequencer/CMakeLists.txt
git commit -m "feat(tt2): shared cross-track pattern-cell helper with PN-exact normalisation"
```

---

## Task 2a — `WPN` host methods + 4 fake-host stubs

Add the get/set host methods to `TT2Host` and override in both engines (forwarding to the helper) + stub the 4 fake hosts so the suite still compiles.

**Files:**
- `src/apps/sequencer/engine/TT2Host.h`
- `src/apps/sequencer/engine/TT2TrackEngine.h`, `TT2TrackEngine.cpp`
- `src/apps/sequencer/engine/TT2MiniTrackEngine.h`, `TT2MiniTrackEngine.cpp`
- `src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp`, `TestTeletypeV2Geode.cpp`, `TestTeletypeV2EnvelopeOps.cpp`, `TestTeletypeV2LfoOps.cpp`

### RED — fake-host functional test (write it first, in Modulator's test file)

Add to `src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp` a CASE that drives `opWpn` through the existing `ModStubHost` once the host methods exist. Append inside the `UNIT_TEST("TeletypeV2Modulator")` block:
```cpp
CASE("WPN forwards signed bank/idx to host set/get") {
    struct RecHost : ModStubHost {
        int16_t lastTrack = -1, lastBank = -1, lastIdx = -1, lastVal = -1;
        int16_t cell = 0;
        int16_t hostTrackPatternVal(uint8_t tr, int16_t b, int16_t i) override {
            lastTrack = tr; lastBank = b; lastIdx = i; return cell;
        }
        void hostSetTrackPatternVal(uint8_t tr, int16_t b, int16_t i, int16_t v) override {
            lastTrack = tr; lastBank = b; lastIdx = i; lastVal = v; cell = v;
        }
    } host;
    tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);
    evalText("WPN 2 0 5 42", rt, out);
    expectEqual(int(host.lastTrack), 1, "track 1-based→0-based");
    expectEqual(int(host.lastBank), 0, "bank passed signed");
    expectEqual(int(host.lastIdx), 5, "idx passed signed");
    expectEqual(int(host.lastVal), 42, "value written");
    evalText("WPN 2 0 5", rt, out);   // get path
    expectEqual(int(host.cell), 42, "read back same cell");
}
```

### Run-to-fail
```
make -C build TestTeletypeV2Modulator
```
Expected: **compile failure** — `'hostTrackPatternVal' marked 'override' but does not override` (the methods aren't on `TT2Host` yet), plus `WPN` is an unknown op until Task 2b. (RED is the compile break here; the `evalText("WPN …")` won't tokenize until 2b — that's why 2a + 2b land together before the test goes green.)

> **Note:** Tasks 2a and 2b together make this test pass. Do 2a (host plumbing) then 2b (op + registration) before re-running to green.

### GREEN — host method declarations

`src/apps/sequencer/engine/TT2Host.h`, in the `// per-track pattern` group:
```cpp
    // cross-track PN cell (bank/idx SIGNED — PN-normalised; 0/no-op if not a TT2 track)
    virtual int16_t hostTrackPatternVal(uint8_t track, int16_t bank, int16_t idx) = 0;
    virtual void hostSetTrackPatternVal(uint8_t track, int16_t bank, int16_t idx, int16_t v) = 0;
```

`src/apps/sequencer/engine/TT2TrackEngine.h` and `TT2MiniTrackEngine.h` — add override decls next to the existing `hostNoteGate*` overrides:
```cpp
    int16_t hostTrackPatternVal(uint8_t track, int16_t bank, int16_t idx) override;
    void hostSetTrackPatternVal(uint8_t track, int16_t bank, int16_t idx, int16_t v) override;
```

`src/apps/sequencer/engine/TT2TrackEngine.cpp` (and the identical pair in `TT2MiniTrackEngine.cpp`, changing the class qualifier) — add `#include "TT2HostCrossTrack.h"` at the top, then:
```cpp
int16_t TT2TrackEngine::hostTrackPatternVal(uint8_t track, int16_t bank, int16_t idx) {
    int16_t *cell = tt2CrossPatternCell(_engine.model(), track, bank, idx);
    return cell ? *cell : 0;
}
void TT2TrackEngine::hostSetTrackPatternVal(uint8_t track, int16_t bank, int16_t idx, int16_t v) {
    if (int16_t *cell = tt2CrossPatternCell(_engine.model(), track, bank, idx)) *cell = v;
}
```
(In `TT2MiniTrackEngine.cpp` the method bodies are byte-identical except `TT2MiniTrackEngine::`. Confirm `_engine.model()` is the accessor each engine uses to reach `Model&` — grep the existing `hostNoteGateGet` body in each file and mirror exactly.)

Stub all **4 fake hosts** (each `struct …Host : TT2Host`) — add next to their `hostNoteGateSet` stub:
```cpp
    int16_t hostTrackPatternVal(uint8_t, int16_t, int16_t) override { return 0; }
    void hostSetTrackPatternVal(uint8_t, int16_t, int16_t, int16_t) override {}
```
(In `TestTeletypeV2Modulator.cpp` the `RecHost` in the new CASE overrides both — leave `ModStubHost`'s base stubs in place so the other CASEs still compile.)

### Run-to-pass — deferred to Task 2b
The op `WPN` doesn't exist until 2b. After 2b, run:
```
make -C build TestTeletypeV2Modulator && ctest --test-dir build -R TestTeletypeV2Modulator -V
```

### Commit — combined with Task 2b (one `WPN` commit). Do not commit here.

---

## Task 2b — `WPN` op + enum + token + regen + vtable

**Files:**
- `teletype/src/ops/op_enum.h`
- `teletype/src/match_token.rl`
- `teletype/src/match_token.c` (regenerated — staged but never hand-edited)
- `src/apps/sequencer/engine/TT2OpNames.cpp` (regenerated — staged but never hand-edited)
- `src/apps/sequencer/engine/TeletypeNativeOps.cpp`
- `src/tests/unit/sequencer/TestTeletypeV2ParserContract.cpp`

### RED — parser-contract test

Add to `src/tests/unit/sequencer/TestTeletypeV2ParserContract.cpp`, inside `UNIT_TEST("TeletypeV2ParserContract")`:
```cpp
    CASE("WPN op token") {
        auto r = tryParse("WPN");
        expectEqual(int(r.error), int(E_OK), "WPN parses");
        expectToken(r.cmd, 0, OP, E_OP_WPN, "WPN op");
    }
```

### Run-to-fail
```
make -C build TestTeletypeV2ParserContract
```
Expected: **compile failure** — `'E_OP_WPN' was not declared in this scope` (enum not added yet).

### GREEN — enum, token, op, regen, vtable

1. `teletype/src/ops/op_enum.h` — add to `tele_op_idx_t`, in the `W*` block (after `E_OP_WNN_H`, before `E_OP__LENGTH`):
```c
    E_OP_WPN,
```

2. `teletype/src/match_token.rl` — add in the `W*` token block (e.g. after the `WNN` line). Longest-match is native, so order vs `WP` is irrelevant:
```c
        "WPN"         => { MATCH_OP(E_OP_WPN); };
```

3. Regenerate (the only correct path):
```
( cd teletype/src && ragel -C -G2 match_token.rl -o match_token.c )
python3 teletype/utils/tt2_op_names.py
```

4. `src/apps/sequencer/engine/TeletypeNativeOps.cpp` — add `opWpn` next to `opWng` (mirror it exactly):
```cpp
template<typename Cfg>
static void opWpn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t t = 0, b = 0, i = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, i, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostSetTrackPatternVal(uint8_t(t - 1), b, i, v);   // b/i SIGNED → PN normalisation
    } else {
        pushStack(stack, stackSize, h ? h->hostTrackPatternVal(uint8_t(t - 1), b, i) : 0, error);
    }
}
```
Register it in the op-table block (next to `table[E_OP_WNG] = opWng<Cfg>;`):
```cpp
            table[E_OP_WPN]                = opWpn<Cfg>;
```

### Run-to-pass
```
make -C build TestTeletypeV2ParserContract && ctest --test-dir build -R TestTeletypeV2ParserContract -V
make -C build TestTeletypeV2Modulator && ctest --test-dir build -R TestTeletypeV2Modulator -V
make -C build TestTT2OpNames && ctest --test-dir build -R TestTT2OpNames -V
```
Expected: all three pass (`TestTT2OpNames` covers the regenerated name automatically; `TestTeletypeV2Modulator` now reaches the WPN CASE added in 2a).

### Commit (stage only these files — one `WPN` commit covering 2a + 2b)
```
git add src/apps/sequencer/engine/TT2Host.h \
        src/apps/sequencer/engine/TT2TrackEngine.h \
        src/apps/sequencer/engine/TT2TrackEngine.cpp \
        src/apps/sequencer/engine/TT2MiniTrackEngine.h \
        src/apps/sequencer/engine/TT2MiniTrackEngine.cpp \
        src/apps/sequencer/engine/TeletypeNativeOps.cpp \
        src/apps/sequencer/engine/TT2OpNames.cpp \
        teletype/src/ops/op_enum.h \
        teletype/src/match_token.rl \
        teletype/src/match_token.c \
        src/tests/unit/sequencer/TestTeletypeV2ParserContract.cpp \
        src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp \
        src/tests/unit/sequencer/TestTeletypeV2Geode.cpp \
        src/tests/unit/sequencer/TestTeletypeV2EnvelopeOps.cpp \
        src/tests/unit/sequencer/TestTeletypeV2LfoOps.cpp
git commit -m "feat(tt2): WPN cross-track pattern-cell op (true cross-track PN)"
```

---

## Task 3 — `W.SCRIPT` plumbing (`triggerScriptFromHost` + cross-depth cap + `hostTriggerTrackScript`)

Engine-level: target-host swap, recursion cap, host dispatch. No op yet (Task 4 wires the op + tests).

**Files:**
- `src/apps/sequencer/engine/TT2Host.h`
- `src/apps/sequencer/engine/TT2TrackEngine.h`, `TT2TrackEngine.cpp`
- `src/apps/sequencer/engine/TT2MiniTrackEngine.h`, `TT2MiniTrackEngine.cpp`
- `src/apps/sequencer/engine/Engine.h`
- the 4 fake-host test files (stub the new host method)

### RED — extend the helper test to assert the new host method exists and is reachable

This task is engine-level (real `ScopedHost`/`runScript`), so the focused RED is **the build**: the fake hosts won't compile until they stub the new pure-virtual, and `Engine` needs the new member. Add the host-method stub assertion via the parser path in Task 4; here the failing signal is the compile of the existing suite after adding the pure-virtual.

To get a concrete RED, add the `hostTriggerTrackScript` pure-virtual to `TT2Host.h` FIRST and run:
```
make -C build TestTeletypeV2Modulator
```
Expected: **compile failure** — `ModStubHost` (and the 3 other fake hosts) are abstract: `hostTriggerTrackScript` not overridden.

### GREEN — implementation

1. `src/apps/sequencer/engine/TT2Host.h` — add the include + pure-virtual:
```cpp
#include "engine/TT2Runner.h"   // for TT2EvalError (confirm the actual header that declares TT2EvalError; grep its definition)
```
```cpp
    // trigger script n (1-based) on track; returns None / OutOfRange / ExecDepthOverflow
    virtual TT2EvalError hostTriggerTrackScript(uint8_t track, int16_t script) = 0;
```
(Grep where `TT2EvalError` is declared — likely `TeletypeProgram.h`/`TT2Runner.h`/an eval header — and include that header here, not a guess.)

2. `src/apps/sequencer/engine/Engine.h` — add the cross-depth member + constant in the private members (near `_trackEngines`):
```cpp
    static constexpr uint8_t TT2_CROSS_DEPTH = 4;   // aggregate cross-track script-trigger cap
    uint8_t _tt2CrossDepth = 0;
```

3. `src/apps/sequencer/engine/TT2TrackEngine.h` — public method (and the identical one in `TT2MiniTrackEngine.h`, with `Cfg`/`_tt2Track` vs `_tt2MiniTrack` as that engine uses). Mirror the existing `runScript(uint8_t)` wrapper which already wraps `_tt2Track.program()`:
```cpp
    // public, returns the eval error so the op can report it
    TT2EvalError triggerScriptFromHost(int16_t oneBased) {
        int idx = oneBased - 1;
        if (idx < 0 || idx >= Cfg::ScriptCount) return TT2EvalError::OutOfRange;   // matches SCRIPT range
        ScopedHost host(this);                                                     // run with THIS track active
        return runScript(uint8_t(idx)).error;                                      // ExecDepthOverflow propagates
    }
```
(`runScript(uint8_t)` already exists as a member returning `TT2EvalResult` with `.error`. Confirm `Cfg`/`ScriptCount` is reachable in each engine — Full uses `TT2ConfigFull::ScriptCount`, Mini uses `TT2ConfigMini::ScriptCount`; use the config alias the engine already defines.)

4. `src/apps/sequencer/engine/TT2TrackEngine.cpp` (one copy; identical body in `TT2MiniTrackEngine.cpp` with the class qualifier) — add an RAII guard helper at file scope and the host method. The RAII guard and `hostTriggerTrackScript` body are the same in both engines (both reach `_engine` + `Engine::trackEngine`); put the guard struct in a shared spot (`TT2HostCrossTrack.h` or an anonymous namespace in each `.cpp`):
```cpp
struct TT2CrossGuard {
    uint8_t &d;
    explicit TT2CrossGuard(uint8_t &depth) : d(depth) { ++d; }
    ~TT2CrossGuard() { --d; }
};

TT2EvalError TT2TrackEngine::hostTriggerTrackScript(uint8_t t, int16_t n) {
    if (t >= CONFIG_TRACK_COUNT) return TT2EvalError::None;   // GUARD FIRST — t is uint8_t(arg-1), can be 255
    auto m = _engine.model().project().track(t).trackMode();
    if (m != Track::TrackMode::TeletypeV2 && m != Track::TrackMode::TeletypeMini)
        return TT2EvalError::None;                            // non-TT2 target: silent no-op (BEFORE the cap)
    if (_engine.tt2CrossDepth() >= Engine::TT2_CROSS_DEPTH) return TT2EvalError::ExecDepthOverflow;
    TT2CrossGuard g(_engine.tt2CrossDepthRef());             // RAII: ++ now, -- on every return
    if (m == Track::TrackMode::TeletypeV2)
        return _engine.trackEngine(t).as<TT2TrackEngine>().triggerScriptFromHost(n);
    return _engine.trackEngine(t).as<TT2MiniTrackEngine>().triggerScriptFromHost(n);   // TeletypeMini
}
```
(`_tt2CrossDepth` lives on `Engine` — expose it via `Engine` accessors `uint8_t tt2CrossDepth() const` + `uint8_t &tt2CrossDepthRef()`, OR make `TT2TrackEngine`/`TT2MiniTrackEngine` friends of `Engine`. Pick the accessor route — smaller surface. Add the two inline accessors to `Engine.h` public section. `n` stays 1-based into `triggerScriptFromHost`, which does the `-1` + range check — single source of range truth.)

5. Stub the new pure-virtual in all 4 fake hosts (next to the WPN stubs):
```cpp
    TT2EvalError hostTriggerTrackScript(uint8_t, int16_t) override { return TT2EvalError::None; }
```

### Run-to-pass
```
make -C build TestTeletypeV2Modulator && ctest --test-dir build -R TestTeletypeV2Modulator -V
```
Expected: compiles + passes (the existing CASEs and the WPN CASE; no new W.SCRIPT op yet, so nothing exercises the new engine path — Task 4 adds that).

### Commit — combined with Task 4 (one `W.SCRIPT` commit). Do not commit here.

---

## Task 4 — `W.SCRIPT` op + enum + token + regen + vtable + fake-host tests

**Files:**
- `teletype/src/ops/op_enum.h`
- `teletype/src/match_token.rl`
- `teletype/src/match_token.c` (regenerated)
- `src/apps/sequencer/engine/TT2OpNames.cpp` (regenerated)
- `src/apps/sequencer/engine/TeletypeNativeOps.cpp`
- `src/tests/unit/sequencer/TestTeletypeV2ParserContract.cpp`
- `src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp` (op-level fake-host tests)

### RED — parser-contract + op-propagation tests

Add to `TestTeletypeV2ParserContract.cpp`:
```cpp
    CASE("W.SCRIPT op token") {
        auto r = tryParse("W.SCRIPT");
        expectEqual(int(r.error), int(E_OK), "W.SCRIPT parses");
        expectToken(r.cmd, 0, OP, E_OP_W_SCRIPT, "W.SCRIPT op");
    }
```

Add to `TestTeletypeV2Modulator.cpp` (drives `opWScript` through a fake host that records the call and returns a chosen error):
```cpp
CASE("W.SCRIPT propagates host error; track stays 1-based at boundary") {
    struct ScriptHost : ModStubHost {
        int calls = 0; int16_t lastTrack = -1, lastScript = -1;
        TT2EvalError ret = TT2EvalError::None;
        TT2EvalError hostTriggerTrackScript(uint8_t tr, int16_t n) override {
            ++calls; lastTrack = tr; lastScript = n; return ret;
        }
    } host;
    tt2SetActiveHost(&host);
    TT2Runtime rt = {}; init(rt);
    TT2OutputState out = {}; init(out);

    // valid: host called (track 1 zero-based, script 1 still 1-based), no error
    auto res = evalText("W.SCRIPT 2 1", rt, out);
    expectEqual(int(host.lastTrack), 1, "track 2 → host track 1");
    expectEqual(int(host.lastScript), 1, "script stays 1-based at boundary");
    expectEqual(int(res.error), int(TT2EvalError::None), "no error");

    // invalid track W.SCRIPT 0 1 → t = uint8_t(0-1) = 255; host guards, returns None
    host.calls = 0; host.lastTrack = -1;
    evalText("W.SCRIPT 0 1", rt, out);
    // op still dispatches to host (the guard lives in hostTriggerTrackScript);
    // host receives 255 and its real impl returns None without indexing the engine.
    expectEqual(int(host.lastTrack), 255, "0 → 255 reaches host guard");

    // out-of-range script → host returns OutOfRange → op writes it to error
    host.ret = TT2EvalError::OutOfRange;
    auto r2 = evalText("W.SCRIPT 2 11", rt, out);
    expectEqual(int(r2.error), int(TT2EvalError::OutOfRange), "OutOfRange propagated");

    // cap hit → host returns ExecDepthOverflow → op propagates
    host.ret = TT2EvalError::ExecDepthOverflow;
    auto r3 = evalText("W.SCRIPT 2 1", rt, out);
    expectEqual(int(r3.error), int(TT2EvalError::ExecDepthOverflow), "ExecDepthOverflow propagated");
}
```
(The "never indexes the engine" assertion from the spec is enforced by the **real** `hostTriggerTrackScript` range-guard, exercised at sim/engine level — the fake-host test proves the op→host boundary and error propagation. `evalText` returns `TT2EvalResult` with `.error`; confirm its return type and that `res.error` is reachable.)

### Run-to-fail
```
make -C build TestTeletypeV2ParserContract
```
Expected: **compile failure** — `'E_OP_W_SCRIPT' was not declared in this scope`.

### GREEN — enum, token, op, regen, vtable

1. `teletype/src/ops/op_enum.h` — after `E_OP_WPN`:
```c
    E_OP_W_SCRIPT,
```

2. `teletype/src/match_token.rl` — in the `W*` block:
```c
        "W.SCRIPT"    => { MATCH_OP(E_OP_W_SCRIPT); };
```

3. Regenerate:
```
( cd teletype/src && ragel -C -G2 match_token.rl -o match_token.c )
python3 teletype/utils/tt2_op_names.py
```

4. `src/apps/sequencer/engine/TeletypeNativeOps.cpp` — `opWScript` next to `opWpn`:
```cpp
template<typename Cfg>
static void opWScript(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                      int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t = 0, n = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    if (!popStack(stack, stackSize, n, error)) return;
    if (TT2Host *h = tt2ActiveHost()) error = h->hostTriggerTrackScript(uint8_t(t - 1), n);
}
```
Register in the op-table block:
```cpp
            table[E_OP_W_SCRIPT]           = opWScript<Cfg>;
```

### Run-to-pass
```
make -C build TestTeletypeV2ParserContract && ctest --test-dir build -R TestTeletypeV2ParserContract -V
make -C build TestTeletypeV2Modulator && ctest --test-dir build -R TestTeletypeV2Modulator -V
make -C build TestTT2OpNames && ctest --test-dir build -R TestTT2OpNames -V
```
Expected: all pass.

### Commit (stage only these files — one `W.SCRIPT` commit covering Task 3 + 4)
```
git add src/apps/sequencer/engine/TT2Host.h \
        src/apps/sequencer/engine/TT2TrackEngine.h \
        src/apps/sequencer/engine/TT2TrackEngine.cpp \
        src/apps/sequencer/engine/TT2MiniTrackEngine.h \
        src/apps/sequencer/engine/TT2MiniTrackEngine.cpp \
        src/apps/sequencer/engine/Engine.h \
        src/apps/sequencer/engine/TeletypeNativeOps.cpp \
        src/apps/sequencer/engine/TT2OpNames.cpp \
        teletype/src/ops/op_enum.h \
        teletype/src/match_token.rl \
        teletype/src/match_token.c \
        src/tests/unit/sequencer/TestTeletypeV2ParserContract.cpp \
        src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp \
        src/tests/unit/sequencer/TestTeletypeV2Geode.cpp \
        src/tests/unit/sequencer/TestTeletypeV2EnvelopeOps.cpp \
        src/tests/unit/sequencer/TestTeletypeV2LfoOps.cpp
git commit -m "feat(tt2): W.SCRIPT cross-track script trigger with host-swap + cross-depth cap"
```

---

## Task 5 — Build + verify (host + sim + ARM clean, full suite, RAM check)

No new code. Verify all three build targets compile clean, the full TT2 suite is green, and STM32 RAM didn't blow.

**Files:** none (verification only — nothing staged, no commit).

### Steps

1. Full TT2 unit suite (run every `TestTeletypeV2*`, `TestTT2*`, `TestTT2HostCrossTrack`):
```
make -C build -j && ctest --test-dir build -R 'TT2|TeletypeV2|TeletypeMini' -V
```
Expected: all green, including the new `TestTT2HostCrossTrack`, the WPN/W.SCRIPT parser-contract cases, and the fake-host op tests.

2. Sim build (the user's default sim tree):
```
make -C build/sim/debug sequencer
```
Expected: links clean.

3. ARM firmware build + RAM check:
```
make -C build/stm32/release sequencer
```
Expected: compiles + links clean. Then inspect `build/stm32/release` `.map` (or `build/sequencer.map`) — confirm the op-table additions (2 pointer slots × 2 configs in CCMRAM) and the single `uint8_t _tt2CrossDepth` on `Engine` haven't pushed RAM/CCMRAM over budget. Spec budget: negligible (a few dozen bytes + small `.text`).

### Done
All three targets clean, full suite green, RAM within budget. Stop — the human reviews + commits the verification has no diff to commit.

---

## Deferred (do NOT implement — propose only if asked)
- Cross-track variable peek/poke (`W.A`-style) — same bridge, another host method pair.
- Explicit `WPN.SET` form — the fused `isSet` op already covers set.
