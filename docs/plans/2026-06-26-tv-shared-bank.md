# TV Shared Variable Bank Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `TV i` / `TV i v` — a bank of 16 `int16` variables shared across all TT-family tracks (full TT2 + Mini), so scripts on different TT tracks pass integers to each other; like `BUS` but ints-not-CV, TT-only, last-writer-wins, 0-based, out-of-range = silent no-op/0.

**Architecture:** One `int16_t _tv[16]` array on `Engine` (session-global, zero-init at construction, never serialized, never re-seeded per frame). Reached from scripts via two new `TT2Host` pure virtuals (`hostTvGet`/`hostTvSet`) that both real engines forward to `Engine::tvGet`/`tvSet`. One templated native op `opTv` registered as `E_OP_TV`, popping the raw 0-based index (no `popOutputIndex`), host-bounds-checked.

**Tech Stack:** C++ (STM32 firmware + host/sim builds), CMake/CTest unit tests, Ragel-generated token tables, the project's `tt2_op_names.py` codegen.

## Global Constraints

- **Never `cd` before git or make.** Use repo-root-relative paths and `-C` only for build dirs already shown in commands below.
- **Stage only the files each task names** in its commit. NEVER `git add -A` — the working tree has unrelated WIP.
- **`make sequencer` builds firmware, NOT tests.** To run a unit test, build its target then ctest: `make -C build <TestName> && ctest --test-dir build -R <TestName> -V`.
- **Op registration goes through the tooling.** Edit `op_enum.h` + `match_token.rl`, then regenerate. NEVER hand-edit `match_token.c` (gitignored) or `TT2OpNames.cpp` (generated).
- **Trust `make`/`ctest`, not IDE clang diagnostics.** An IDE may flag a class abstract mid-task; the build is the source of truth.
- **Path convention:** `engine/…`, `model/…` are under `src/apps/sequencer/`; `teletype/…` are repo-root-relative.

> **Shared-file note (read before starting):** This plan modifies `engine/TT2Host.h` and the 4 fake-host test files (`TestTeletypeV2Modulator.cpp`, `TestTeletypeV2Geode.cpp`, `TestTeletypeV2EnvelopeOps.cpp`, `TestTeletypeV2LfoOps.cpp`) — the same files the cross-track-ops plan touches. If both plans are executed, run them **sequentially**, not in parallel. Whichever runs second simply adds its own host methods + stubs to the already-modified files (additive, no conflict).

> **Compile-order rationale (why the tasks are ordered this way):** Adding the 2 pure virtuals to `TT2Host` makes BOTH real engines AND the 4 fake-host test files abstract until each implements them. The real engine overrides forward to `Engine::tvGet`/`tvSet`, which must exist first. So: Task 1 adds the `Engine` store; Task 2 adds the host bridge + all overrides/stubs in one compile-gated step; Task 3 registers the op; Task 4 adds the functional test; Task 5 is the RAM gate.

---

## Task 1: `Engine` store (`_tv[16]` + `tvGet`/`tvSet`)

**Files:**
- Modify: `src/apps/sequencer/engine/Engine.h` (add members + inline methods next to the `BusCvCount` block, ~`:135`; add the `_tv` array near `_busCv` at `:367`)

**Interfaces:**
- Produces: `static constexpr int Engine::TvCount = 16;`, `int16_t Engine::tvGet(int i) const`, `void Engine::tvSet(int i, int16_t v)`, member `int16_t Engine::_tv[16]`.

- [ ] **Step 1: Add the public methods + constant.**

In `src/apps/sequencer/engine/Engine.h`, immediately after the `setBusCv(...)` method's closing brace (the bus block ending ~`:170`), add:

```cpp
    static constexpr int TvCount = 16;
    int16_t tvGet(int i) const { return (i >= 0 && i < TvCount) ? _tv[i] : 0; }
    void tvSet(int i, int16_t v) { if (i >= 0 && i < TvCount) _tv[i] = v; }
```

- [ ] **Step 2: Add the backing array.**

In the private members region, next to the bus arrays (`std::array<float, BusCvCount> _busCv{};` at `:367`), add:

```cpp
    int16_t _tv[TvCount] = {};
```

- [ ] **Step 3: Build firmware target to compile-gate (no behavior test yet).**

Run: `make -C build sequencer`
Expected: clean build, no errors. (Plain members + bounds-checked accessors; nothing calls them yet.)

- [ ] **Step 4: Commit.**

```bash
git add src/apps/sequencer/engine/Engine.h
git commit -m "feat(tt): add Engine _tv[16] shared store (tvGet/tvSet)"
```

---

## Task 2: Host bridge (`hostTvGet`/`hostTvSet`) + all overrides and fake-host stubs

This task is a single compile-gate: the moment the 2 pure virtuals land in `TT2Host.h`, both real engines and the 4 fake-host test files go abstract. They are all fixed here so the build + existing suite stay green at task end.

**Files:**
- Modify: `src/apps/sequencer/engine/TT2Host.h` (add 2 pure virtuals after `hostSetBusCv` at `:40`)
- Modify: `src/apps/sequencer/engine/TT2TrackEngine.h:182` (override decls), `src/apps/sequencer/engine/TT2TrackEngine.cpp:261` (override defs)
- Modify: `src/apps/sequencer/engine/TT2MiniTrackEngine.h:146` (override decls), `src/apps/sequencer/engine/TT2MiniTrackEngine.cpp:233` (override defs)
- Modify: `src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp:43`, `TestTeletypeV2Geode.cpp:44`, `TestTeletypeV2EnvelopeOps.cpp`, `TestTeletypeV2LfoOps.cpp` (stub the 2 methods in each fake host)

**Interfaces:**
- Consumes: `Engine::tvGet`/`tvSet` (Task 1).
- Produces: `virtual int16_t TT2Host::hostTvGet(uint8_t slot) = 0;`, `virtual void TT2Host::hostTvSet(uint8_t slot, int16_t v) = 0;` — the indexed-host bridge `opTv` (Task 3) calls.

- [ ] **Step 1: Add the 2 pure virtuals to `TT2Host`.**

In `src/apps/sequencer/engine/TT2Host.h`, after the bus line `virtual void hostSetBusCv(uint8_t index, int16_t raw) = 0;` (`:40`), add:

```cpp
    virtual int16_t hostTvGet(uint8_t slot) = 0;          // shared TV bank, 0-based
    virtual void hostTvSet(uint8_t slot, int16_t v) = 0;
```

- [ ] **Step 2: Declare overrides in both real engine headers.**

In `src/apps/sequencer/engine/TT2TrackEngine.h`, after `void hostSetBusCv(uint8_t index, int16_t raw) override;` (`:182`):

```cpp
    int16_t hostTvGet(uint8_t slot) override;
    void hostTvSet(uint8_t slot, int16_t v) override;
```

In `src/apps/sequencer/engine/TT2MiniTrackEngine.h`, after `void hostSetBusCv(uint8_t index, int16_t raw) override;` (`:146`):

```cpp
    int16_t hostTvGet(uint8_t slot) override;
    void hostTvSet(uint8_t slot, int16_t v) override;
```

- [ ] **Step 3: Define overrides in both real engine .cpp files (forward to `_engine`).**

In `src/apps/sequencer/engine/TT2TrackEngine.cpp`, after the `hostSetBusCv` body (ending `:263`):

```cpp
int16_t TT2TrackEngine::hostTvGet(uint8_t slot) {
    return _engine.tvGet(slot);
}
void TT2TrackEngine::hostTvSet(uint8_t slot, int16_t v) {
    _engine.tvSet(slot, v);
}
```

In `src/apps/sequencer/engine/TT2MiniTrackEngine.cpp`, after the `hostSetBusCv` body (ending `:235`):

```cpp
int16_t TT2MiniTrackEngine::hostTvGet(uint8_t slot) {
    return _engine.tvGet(slot);
}
void TT2MiniTrackEngine::hostTvSet(uint8_t slot, int16_t v) {
    _engine.tvSet(slot, v);
}
```

- [ ] **Step 4: Stub the 2 methods in all 4 fake hosts.**

Each fake host stubs `hostBusCv`/`hostSetBusCv` already — add the TV pair right after that line. In `src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp` (after `:43`), `TestTeletypeV2Geode.cpp` (after `:44`), `TestTeletypeV2EnvelopeOps.cpp`, and `TestTeletypeV2LfoOps.cpp` (find the `void hostSetBusCv(uint8_t, int16_t) override {}` line in each), add:

```cpp
    int16_t hostTvGet(uint8_t) override { return 0; }
    void hostTvSet(uint8_t, int16_t) override {}
```

- [ ] **Step 5: Build firmware to compile-gate the real engines.**

Run: `make -C build sequencer`
Expected: clean build (both engines now concrete again).

- [ ] **Step 6: Build + run the 4 fake-host tests to confirm they're concrete and still pass.**

Run:
```bash
make -C build TestTeletypeV2Modulator TestTeletypeV2Geode TestTeletypeV2EnvelopeOps TestTeletypeV2LfoOps \
  && ctest --test-dir build -R "TestTeletypeV2Modulator|TestTeletypeV2Geode|TestTeletypeV2EnvelopeOps|TestTeletypeV2LfoOps" -V
```
Expected: all 4 build and PASS.

- [ ] **Step 7: Commit.**

```bash
git add src/apps/sequencer/engine/TT2Host.h \
        src/apps/sequencer/engine/TT2TrackEngine.h src/apps/sequencer/engine/TT2TrackEngine.cpp \
        src/apps/sequencer/engine/TT2MiniTrackEngine.h src/apps/sequencer/engine/TT2MiniTrackEngine.cpp \
        src/tests/unit/sequencer/TestTeletypeV2Modulator.cpp \
        src/tests/unit/sequencer/TestTeletypeV2Geode.cpp \
        src/tests/unit/sequencer/TestTeletypeV2EnvelopeOps.cpp \
        src/tests/unit/sequencer/TestTeletypeV2LfoOps.cpp
git commit -m "feat(tt): host bridge hostTvGet/hostTvSet (both engines + fake-host stubs)"
```

---

## Task 3: `opTv` op + registration (`E_OP_TV`, `TV` token)

**Files:**
- Modify: `teletype/src/ops/op_enum.h:173` (add `E_OP_TV` before `E_OP__LENGTH` at `:441`)
- Modify: `teletype/src/match_token.rl:200` (add the `"TV"` rule near `"BUS"`)
- Regenerate: `teletype/src/match_token.c` (via Ragel) + `src/apps/sequencer/engine/TT2OpNames.cpp` (via `tt2_op_names.py`)
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp` (add `opTv` template ~near `opBus` at `:2070`; register `table[E_OP_TV]` next to `table[E_OP_BUS]` at `:4483`)
- Test: `src/tests/unit/sequencer/TestTeletypeV2ParserContract.cpp` (add a `TV` token case)

**Interfaces:**
- Consumes: `TT2Host::hostTvGet`/`hostTvSet` (Task 2), `tt2ActiveHost()`, `popStack`/`pushStack`.
- Produces: enum constant `E_OP_TV`, token `"TV"`, vtable handler `opTv<Cfg>`.

- [ ] **Step 1: Write the failing test (parser contract token round-trip).**

In `src/tests/unit/sequencer/TestTeletypeV2ParserContract.cpp`, add a new `CASE` inside the `UNIT_TEST("TeletypeV2ParserContract")` block (e.g. after the `seed_family_smoke` case):

```cpp
    CASE("tv_token_smoke") {
        auto r = tryParse("TV 0");
        expectEqual(int(r.error), int(E_OK), "TV parses");
        expectToken(r.cmd, 0, OP, E_OP_TV, "TV op");
    }
```

- [ ] **Step 2: Run test to verify it fails.**

Run: `make -C build TestTeletypeV2ParserContract 2>&1 | tail -20`
Expected: COMPILE FAIL — `E_OP_TV` is undeclared (enum not added yet). This confirms the test references the new symbol.

- [ ] **Step 3: Add the enum constant.**

In `teletype/src/ops/op_enum.h`, add `E_OP_TV,` to `tele_op_idx_t` near `E_OP_BUS` (`:173`), before `E_OP__LENGTH` (`:441`):

```c
    E_OP_TV,
```

- [ ] **Step 4: Add the token rule.**

In `teletype/src/match_token.rl`, next to the `"BUS"` rule (`:200`), add:

```
        "TV"          => { MATCH_OP(E_OP_TV); };
```

- [ ] **Step 5: Regenerate the token table and op-names (NEVER hand-edit the outputs).**

Run:
```bash
( cd teletype/src && ragel -C -G2 match_token.rl -o match_token.c )
python3 teletype/utils/tt2_op_names.py
```
Expected: `teletype/src/match_token.c` regenerated (gitignored); `src/apps/sequencer/engine/TT2OpNames.cpp` updated to include the `TV` name.

- [ ] **Step 6: Add the `opTv` op body.**

In `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, after the `opBus` template (ends `:2082`), add:

```cpp
template<typename Cfg>
static void opTv(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t i = 0;
    if (!popStack(stack, stackSize, i, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSetPosition && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostTvSet(uint8_t(i), v);
    } else {
        pushStack(stack, stackSize, h ? h->hostTvGet(uint8_t(i)) : 0, error);
    }
}
```

- [ ] **Step 7: Register the op in the vtable.**

In `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, immediately after `table[E_OP_BUS] = opBus<Cfg>;` (`:4483`):

```cpp
            table[E_OP_TV]                 = opTv<Cfg>;
```

- [ ] **Step 8: Run the parser-contract test to verify it passes.**

Run: `make -C build TestTeletypeV2ParserContract && ctest --test-dir build -R TestTeletypeV2ParserContract -V`
Expected: PASS (`TV` tokenizes to `E_OP_TV`).

- [ ] **Step 9: Confirm op-names test still passes (auto-covers the regenerated name).**

Run: `make -C build TestTT2OpNames && ctest --test-dir build -R TestTT2OpNames -V`
Expected: PASS.

- [ ] **Step 10: Commit.**

```bash
git add teletype/src/ops/op_enum.h teletype/src/match_token.rl \
        src/apps/sequencer/engine/TT2OpNames.cpp \
        src/apps/sequencer/engine/TeletypeNativeOps.cpp \
        src/tests/unit/sequencer/TestTeletypeV2ParserContract.cpp
git commit -m "feat(tt): add TV op (E_OP_TV) + token + vtable registration"
```

---

## Task 4: Functional test (`TestTtSharedTv.cpp`)

End-to-end: parse → lower → `evaluateCommand` against a fake `TT2Host` backed by `int16_t[16]`. Covers set/get, sharing, last-writer-wins, out-of-range (`TV 99`, `TV -1`) → 0 with `error == None`, and null-host → 0. Modeled on `TestTeletypeV2Geode.cpp`'s `evalText` harness.

**Files:**
- Create: `src/tests/unit/sequencer/TestTtSharedTv.cpp`
- Modify: `src/tests/unit/sequencer/CMakeLists.txt` (register the new test next to `:135`)

**Interfaces:**
- Consumes: `E_OP_TV` / `TV` token (Task 3), `TT2Host::hostTvGet`/`hostTvSet` (Task 2), `evaluateCommand`, `tt2SetActiveHost`.

- [ ] **Step 1: Write the failing test file.**

Create `src/tests/unit/sequencer/TestTtSharedTv.cpp`:

```cpp
#include "UnitTest.h"

#include "engine/TeletypeOutputState.h"
#include "engine/TT2Evaluator.h"
#include "engine/TT2Host.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
#include "ops/op_enum.h"
}

namespace {

// Fake host: TV bank backed by a single int16_t[16], inert everywhere else.
struct TvStubHost : TT2Host {
    int16_t tv[16] = {};

    int16_t hostTempo() override { return 0; }
    void hostSetTempo(int16_t) override {}
    int16_t hostTransportRunning() override { return 0; }
    void hostSetTransportRunning(int16_t) override {}
    int16_t hostBarFraction(uint8_t) override { return 0; }
    int16_t hostWms(uint8_t) override { return 0; }
    int16_t hostWtu(uint8_t, uint8_t) override { return 0; }
    int16_t hostTrackPattern(uint8_t) override { return 0; }
    void hostSetTrackPattern(uint8_t, uint8_t) override {}
    int16_t hostNoteGateGet(uint8_t, uint8_t) override { return 0; }
    void hostNoteGateSet(uint8_t, uint8_t, int16_t) override {}
    int16_t hostNoteNoteGet(uint8_t, uint8_t) override { return 0; }
    void hostNoteNoteSet(uint8_t, uint8_t, int16_t) override {}
    int16_t hostNoteGateHere(uint8_t) override { return 0; }
    int16_t hostNoteNoteHere(uint8_t) override { return 0; }
    int16_t hostRoutingSource(uint8_t) override { return 0; }
    int16_t hostBusCv(uint8_t) override { return 0; }
    void hostSetBusCv(uint8_t, int16_t) override {}

    Modulator &hostModulator(uint8_t) override { static Modulator m; return m; }
    int16_t hostModulatorOutput(uint8_t) override { return 0; }
    void hostModulatorTrigger(uint8_t) override {}

    GeodeConfig &hostGeodeConfig() override { static GeodeConfig g; return g; }
    int16_t hostGeodeMix() override { return 0; }
    void hostGeodeTriggerVoice(uint8_t, int16_t, int16_t) override {}
    void hostGeodeTriggerAll(int16_t, int16_t) override {}
    int16_t hostGeodeRun() override { return 0; }
    void hostGeodeSetRun(int16_t) override {}

    // TV bank: bounds-checked, 0-based.
    int16_t hostTvGet(uint8_t slot) override { return slot < 16 ? tv[slot] : 0; }
    void hostTvSet(uint8_t slot, int16_t v) override { if (slot < 16) tv[slot] = v; }
};

TT2EvalResult evalText(const char *text, TT2Runtime &rt, TT2OutputState &out) {
    tele_command_t src = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    parse(text, &src, err);
    TT2Command cmd = {};
    lowerCommand(src, cmd);
    return evaluateCommand(cmd, rt, out, nullptr);
}

} // namespace

UNIT_TEST("TtSharedTv") {

    CASE("set_then_get_roundtrip") {
        TvStubHost host; tt2SetActiveHost(&host);
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        evalText("TV 0 42", rt, out);
        expectEqual(int(host.tv[0]), 42, "TV 0 42 stores 42");
        auto r = evalText("TV 0", rt, out);
        expectEqual(int(r.error), int(TT2EvalError::None), "TV 0 get ok");
        expectEqual(int(r.value), 42, "TV 0 reads 42 back");
        tt2SetActiveHost(nullptr);
    }

    CASE("shared_across_invocations") {
        TvStubHost host; tt2SetActiveHost(&host);
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        evalText("TV 3 17", rt, out);
        auto r = evalText("TV 3", rt, out);
        expectEqual(int(r.value), 17, "second invocation sees first's write");
        tt2SetActiveHost(nullptr);
    }

    CASE("last_writer_wins") {
        TvStubHost host; tt2SetActiveHost(&host);
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        evalText("TV 0 5", rt, out);
        evalText("TV 0 9", rt, out);
        auto r = evalText("TV 0", rt, out);
        expectEqual(int(r.value), 9, "last write stands (no accumulation)");
        tt2SetActiveHost(nullptr);
    }

    CASE("out_of_range_high_is_silent_zero") {
        TvStubHost host; tt2SetActiveHost(&host);
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        evalText("TV 99 7", rt, out);
        auto r = evalText("TV 99", rt, out);
        expectEqual(int(r.error), int(TT2EvalError::None), "TV 99 leaves error None");
        expectEqual(int(r.value), 0, "TV 99 reads 0");
        tt2SetActiveHost(nullptr);
    }

    CASE("out_of_range_negative_is_silent_zero") {
        TvStubHost host; tt2SetActiveHost(&host);
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        auto r = evalText("TV -1", rt, out);
        expectEqual(int(r.error), int(TT2EvalError::None), "TV -1 leaves error None");
        expectEqual(int(r.value), 0, "TV -1 reads 0");
        tt2SetActiveHost(nullptr);
    }

    CASE("null_host_reads_zero") {
        tt2SetActiveHost(nullptr);
        TT2Runtime rt = {}; init(rt);
        TT2OutputState out = {}; init(out);
        auto r = evalText("TV 0", rt, out);
        expectEqual(int(r.error), int(TT2EvalError::None), "null host get error None");
        expectEqual(int(r.value), 0, "null host get pushes 0");
    }
}
```

- [ ] **Step 2: Register the test in CMake.**

In `src/tests/unit/sequencer/CMakeLists.txt`, after `register_sequencer_test(TestTeletypeV2Geode TestTeletypeV2Geode.cpp)` (`:134`), add:

```cmake
register_sequencer_test(TestTtSharedTv TestTtSharedTv.cpp)
```

- [ ] **Step 3: Run the test to verify it passes.**

Run: `make -C build TestTtSharedTv && ctest --test-dir build -R TestTtSharedTv -V`
Expected: PASS — all 6 cases (set/get, sharing, last-writer-wins, out-of-range high/negative, null-host).

- [ ] **Step 4: Commit.**

```bash
git add src/tests/unit/sequencer/TestTtSharedTv.cpp src/tests/unit/sequencer/CMakeLists.txt
git commit -m "test(tt): functional TV shared-bank coverage (sharing, LWW, out-of-range)"
```

---

## Task 5: Build + RAM acceptance gate

No code change — this is the verification gate from the spec's budget section (PROJECT.md:307: direct RAM still matters). Confirm host + sim + ARM builds are clean and the STM32 release deltas match the spec (`.bss`/`.data` +~32 B for `_tv`, `.ccmram_bss` op-table noise only, `sizeof(Engine)` grows only by ~32 B, no region overflow).

**Files:** none (verification only).

- [ ] **Step 1: Host unit-test build + full TT suite green.**

Run: `make -C build && ctest --test-dir build -R "TeletypeV2|TT2|TtSharedTv" -V 2>&1 | tail -30`
Expected: all TT tests PASS, including `TestTtSharedTv`, `TestTeletypeV2ParserContract`, `TestTT2OpNames`.

- [ ] **Step 2: Sim build clean.**

Run: `make -C build/sim/debug sequencer`
Expected: clean build.

- [ ] **Step 3: STM32 release build + RAM gate.**

Run: `make -C build/stm32/release sequencer 2>&1 | tail -30`
Expected: clean link, no region (`FLASH`/`RAM`/`CCMRAM`) overflow.

- [ ] **Step 4: Confirm the size deltas vs PROJECT.md baseline.**

Inspect the section sizes from the release build output (or `arm-none-eabi-size` on the `.elf`). Expected vs PROJECT.md:294 baseline (`.text=889,236`, `.data=6,416`, `.bss=167,996`, `.ccmram_bss=55,084`):
- `.bss`/`.data`: **+~32 B** (the `int16_t _tv[16]` on `Engine`).
- `.ccmram_bss`: op-table noise only (1 slot × 2 configs).
- `sizeof(Engine)` / `TrackEngineContainer`: unchanged beyond the ~32 B; container slot ceiling (912 B) not exceeded.

If `.bss`/`.data` grew by far more than 32 B, or any region overflowed, STOP and investigate before considering the feature done.

- [ ] **Step 5: No commit.**

This task verifies only — there is nothing to stage.
