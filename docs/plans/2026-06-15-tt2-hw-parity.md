---
id: feat-tt2-hw-parity
schema: plan
title: "feat: TT2 full teletype HW parity (10 scripts + 64-deep delay)"
type: feat
status: active
date: 2026-06-15
depth: standard
---

# TT2 Full Teletype HW Parity Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Bring a single TT2 program to full original-teletype script parity — **10 editable scripts** (8 numbered + Metro + Init), **8 trigger inputs → scripts 1–8**, and a **64-deep delay queue** — verified to fit one track slot (measured: +896 B `.bss`, ~2.1 KB headroom).

**Architecture:** Pure capacity bump of the existing native TT2 model. No new mechanism: raise `TT2_SCRIPT_COUNT` 6→10, `TT2_TRIGGER_INPUT_COUNT` 4→8 (original-Teletype 8 trig → 8 scripts; trigger inputs are configurable sources, not jacks), `TT2_DELAY_DEPTH` 8→64, the Metro/Init indices, and the `static_assert` size gates; extend the editor's script navigation to 10. The script-indexed runtime arrays (`every[][]`, `j`, `k`, `scriptLastMs`) and the delay queue grow with the constants. Delay is a **runtime** mechanism (one copy), so it is a fixed cost, not per-scene. Live + Delay execution already exist (`runLiveCommand`, `tt2AdvanceDelays`).

**Tech Stack:** C++17, STM32F405 firmware. Unit tests under `build/` (UnitTest harness + ctest). Sim under `build/sim/debug`. ARM size probe under `build/stm32/release`.

---

## Context & references

- **Op-level parity is already complete** — see `.scratch/tt2-op-coverage.html` (19 core families wired; only `G.O/G.BAR/G.R/G.B` unsupported-by-design, i2c/scene out of scope) and `docs/teletype_v2.md`.
- **Upstream script model** (`teletype/src/script.h`): `REGULAR_SCRIPT_COUNT = 8`, `METRO_SCRIPT = 8`, `INIT_SCRIPT = 9`, `EDITABLE_SCRIPT_COUNT = 10`; Delay + Live are internal. `SCRIPT_MAX_COMMANDS = 6` lines/script (TT2 already matches). `teletype/src/state.h:24` `DELAY_SIZE = 64` (TT2 currently 8 — the gap), `STACK_SIZE = 16` / `Q_LENGTH = 64` (TT2 already matches).
- **Scene bank is OUT of scope here** — that is the quad reshape in `docs/plans/2026-06-14-004-feat-tt2-supertrack-design.md`. This plan is the *single-program* parity bump that fits one slot.

### Measured sizes — full combined parity (measured 2026-06-17)

Scope of "after parity": 10 scripts + 64-deep delay + **8 trigger inputs** (4→8), on top of the now-committed PRINT `dashboard[16]` and per-output shaping fields. **Numbers are spike-measured** (constants bumped, built host + STM32 release, reverted), not estimated.

| item | now | after parity |
|---|---|---|
| `TT2Script` | 302 | 302 |
| `TeletypeProgram` (scripts + patterns + `triggerSource[8]` + shaping + hdr) | 2426 (6 scripts, 4 trig) | **3638** (10 scripts, 8 trig) |
| `TT2DelayQueue` (entries×60 + count) | 482 (×8) | **3842** (×64) |
| `TT2Variables` (incl `dashboard[16]`, `j[10]`/`k[10]`) | 472 | **~488** |
| `TT2Runtime` (delay + script arrays + `inputLevel[8]`) | 2328 | **5872** |
| **`TT2Track`** (program + runtime + idx) | 4760 | **9512** |
| `Track` union (largest arm) | 9496 (`NoteTrack` gate) | **9536** (TT2Track now largest) |

**Trigger bump (4→8):** `triggerSource[8]` (+4 B program), `inputLevel[8]` (+4 B runtime), engine `_prevInputState[8]` (CCMRAM). `TT2_TRIGGER_INPUTS`/`script_pol[8]` were already 8.

**THE GATE — measured, it fits.** Full parity `TT2Track = 9512` exceeds the host `NoteTrack` gate (9468), so `TT2Track` becomes the largest `Track` arm and the union grows (`Track` 9496→9536) — `Project._tracks[8]` inflates. **But the STM32 release links clean, no region overflow:** `.bss` 113,216 → **114,112 (+896 B)**, `.data` and `.ccmram_bss` flat. `.data+.bss` = **120,684 B (~117.9 KB)**, ~2.1 KB under the 120 KB warning. **No `TT2_DELAY_DEPTH` fallback needed** — 64-deep delay + 8 triggers all fit. The ~2.1 KB remaining is the real headroom budget for anything after this.

---

## Scope Boundaries

**In scope:** `TT2_SCRIPT_COUNT` 6→10, Metro/Init indices, `TT2_DELAY_DEPTH` 8→64, all affected `static_assert` bounds, editor reaches all 10 scripts **on the panel (Option A: page+F bank) and USB keyboard**, SCRIPT-op range verification, demo/doc updates, native + STM32 size verification.

**I/O config + per-output shaping — in scope (decided 2026-06-15).** A TT2 track-config page **modeled on the Routing matrix** (`RoutingPage::drawTabEditor`): WindowPainter header (title + centered view tag + `hline`), name-gutter matrix, 5-cell function footer. **Two views toggled by F1/F2** (`ui-preview/tt2-io/tt2-io-in.png`, `tt2-io-out.png`):
- **INPUTS** — **2-up packed** (two name/source pairs per row, ~5 rows): trigger-in T1–4 → `triggerSource[i]`, CV-in IN/PARAM/X/Y/Z/T → `cvInputSource[i]`; **MIDI source** as a header chip. All already in `TeletypeProgram`.
- **OUTPUTS** — 8 CV rows × 4 cols **range / quantize / offset / root-note** (new fields; mirrors TT1 `PatternSlot` `cvOutput*`, now ×8), scrolls with a scrollbar like the routing matrix.

**Dropped from the page:** **boot** (TT2 always boots Init — `bootScriptIndex` defaults to `TT2_INIT_SCRIPT`); **clock DIV/MUL** (vestigial — the TT2 engine is ms-only and never reads `clockDivisor`/`clockMultiplier`/`timeBase`). Output **dests** stay on Performer's global CV/gate output-track routing.

**Clock-synced time base → future native TT2 op (not this plan).** TT1's Clock time base (metro/delays scaled to tempo via div/mult, `TeletypeTrackEngine.cpp:1134`) is deferred and re-homed as a **script-level native op**, not an I/O-setup field (decided 2026-06-15; user to research). Until then TT2 stays ms-only and the `timeBase`/`clockDivisor`/`clockMultiplier` fields remain unused.

**Panel script navigation — decided (Option A, 2026-06-15).** F1–F4 = S1–S4 (F4 keeps its S4↔Metro re-press cycle = Metro); **page+F1–F4 = S5–S8**; **page+F5 = Init**. `shift+F1–F4` (fire) and F5 (live/pattern) unchanged. Covers all 10 on the panel, no conflicts, no OLED layout change (the top-right `S%d`/`M`/`I` label is the same 2-char width for S5–S8 as S1–S4, so **no `ui-preview` needed**). `page+F` is currently free (the page block only handles left/right/step). Implemented in Task 4.

**Out of scope (separate efforts, referenced):**
- **Scene bank** (multiple programs per w-pattern) → `2026-06-14-004-feat-tt2-supertrack-design.md` (quad) or token packing. A full-parity program fills the slot, so a *bank* of them is firmly quad/packed territory.
- **Human-readable text save/load** of a program → its own plan; printer (`tt2PrintScript`) + parser (`loadScriptText`) exist. TT1's writer (`FileManager.cpp:707`) emits `NAME`/`#IO`/`#S4`/`#M`/`#S1–S3`/`#PATS` (its S1–3-shared vs S4-per-slot split); TT2 has one program so it would emit a uniform `#S1–S8`/`#M`/`#I`/`#PATS`/`#C`. See the roadmap below.
- **I/O routing-assignment editor** → `2026-06-14-001-feat-tt2-editor-repoint-plan.md` deferred bucket.
- **By-design drops** (`E.*`/`LFO.*` → `MO.*`; `G.O/BAR/R/B`; i2c/scene) — not parity targets.

---

## Phase 1 — Program + runtime parity bump

### Task 1: Failing test — 10 scripts, Metro=8, Init=9

**Files:**
- Test: `src/tests/unit/sequencer/TestTeletypeV2ScriptRunner.cpp` (add a CASE)

**Step 1: Write the failing test (constants only — must be safe at the current count of 6)**

Do **not** touch `scripts[7]` here: with the pre-bump count of 6, `scripts[7]` is out of bounds. This CASE only asserts the layout constants, so it compiles and runs against the current code and fails on the values. The script-8 *behavior* is exercised in Task 5 (after the bump).

```cpp
CASE("hw_parity_script_layout") {
    // Full teletype: 8 numbered + Metro + Init = 10 editable scripts.
    expectEqual(TT2_SCRIPT_COUNT, 10, "10 editable scripts");
    expectEqual(TT2_METRO_SCRIPT, 8, "metro at index 8");
    expectEqual(TT2_INIT_SCRIPT, 9, "init at index 9");
}
```

**Step 2: Run to verify it fails**

Run: `make -C build TestTeletypeV2ScriptRunner && ctest --test-dir build -R TeletypeV2ScriptRunner --output-on-failure`
Expected: FAIL (`TT2_SCRIPT_COUNT` is 6, not 10).

**Step 3: Bump the constants**

Modify `src/apps/sequencer/model/TeletypeProgram.h:13,21,22`:
```cpp
static constexpr int TT2_SCRIPT_COUNT        = 10;   // 8 numbered + Metro + Init
// ...
static constexpr int TT2_METRO_SCRIPT     = 8;
static constexpr int TT2_INIT_SCRIPT      = 9;
static constexpr int TT2_TRIGGER_INPUT_COUNT = 8;   // 8 trigger inputs -> scripts 1-8 (was 4)
```
**8 trigger inputs (4→8):** matches original Teletype's 8 trig → 8 scripts. TT2 trigger inputs are configurable *sources* (`triggerSource[i]` = CvIn / GateOut / LogicalGate), not 8 physical jacks, so 8 is feasible on Performer. This grows `triggerSource[8]` (+4 B) and `inputLevel[8]` (+4 B); `TT2_TRIGGER_INPUTS`/`script_pol[8]` were already 8. The `init()` loop and `bootScriptIndex = TT2_INIT_SCRIPT` follow automatically.

**Step 4: Bump the program size assert**

Modify `src/apps/sequencer/model/TeletypeProgram.h:165`:
```cpp
static_assert(sizeof(TeletypeProgram) <= 3640, "TeletypeProgram size drift");
```
(10×302 + 4×138 + header ≈ 3594; 3640 leaves a small margin. If compile still fails, set to the value the error reports.)

**Step 5: Run to verify it passes**

Run: `make -C build TestTeletypeV2ScriptRunner && ctest --test-dir build -R TeletypeV2ScriptRunner --output-on-failure`
Expected: PASS.

**Step 6: Commit**

```bash
git add src/apps/sequencer/model/TeletypeProgram.h src/tests/unit/sequencer/TestTeletypeV2ScriptRunner.cpp
git commit -m "feat(tt2): 10 editable scripts (8 numbered + Metro + Init)"
```

---

### Task 2: Failing test — 64-deep delay queue

**Files:**
- Test: `src/tests/unit/sequencer/TestTeletypeV2Delay.cpp` (add a CASE)

**Step 1: Write the failing test**

```cpp
CASE("hw_parity_delay_depth") {
    expectEqual(TT2_DELAY_DEPTH, 64, "64-deep delay (DELAY_SIZE parity)");
}
```

**Step 2: Run to verify it fails**

Run: `make -C build TestTeletypeV2Delay && ctest --test-dir build -R TeletypeV2Delay --output-on-failure`
Expected: FAIL (`TT2_DELAY_DEPTH` is 8).

**Step 3: Bump the constant + asserts**

Modify `src/apps/sequencer/model/TeletypeRuntime.h:10`:
```cpp
static constexpr int TT2_DELAY_DEPTH      = 64;   // teletype DELAY_SIZE parity
```
Bump the affected size asserts in the same file (`:336`, `:342`, and `:332`/`:337` if the script-array growth pushes them):
```cpp
static_assert(sizeof(TT2DelayQueue) <= 3850, "TT2DelayQueue size drift");  // 64×60+2 = 3842
static_assert(sizeof(TT2EveryState) <= 372, "TT2EveryState size drift");   // every[10][6]
static_assert(sizeof(TT2Variables) <= 496, "TT2Variables size drift");     // 472 base (incl PRINT dashboard) + j[10]/k[10] = 488
static_assert(sizeof(TT2Runtime)   <= 5900, "TT2Runtime size drift");      // ~5864 measured
```
(Use the exact value the compiler reports if a bound still trips — these are `<=` guards. Baselines here include the PRINT `dashboard[16]` (+32 B) added 2026-06-17.)

**Step 4: Run to verify it passes**

Run: `make -C build TestTeletypeV2Delay && ctest --test-dir build -R TeletypeV2Delay --output-on-failure`
Expected: PASS.

**Step 5: Commit**

```bash
git add src/apps/sequencer/model/TeletypeRuntime.h src/tests/unit/sequencer/TestTeletypeV2Delay.cpp
git commit -m "feat(tt2): 64-deep delay queue (DELAY_SIZE parity)"
```

---

### Task 3: Fix the `TT2Track` exact-size assert + measure the gate

**Files:**
- Modify: `src/apps/sequencer/model/TT2Track.h:46`

**Step 1: Measure the new size (native)**

```bash
cat > /tmp/sz.cpp <<'EOF'
#include <cstdio>
#include "model/TT2Track.h"
#include "model/Track.h"
int main(){ printf("TT2Track=%zu Track=%zu NoteTrack=%zu CurveTrack=%zu\n",
  sizeof(TT2Track), sizeof(Track), sizeof(NoteTrack), sizeof(CurveTrack)); }
EOF
c++ -std=c++17 -DPLATFORM_SIM -I src/apps/sequencer -I src/core -I src -I src/platform/sim -I teletype/src -I teletype/libavr32/src /tmp/sz.cpp -o /tmp/sz && /tmp/sz; rm -f /tmp/sz /tmp/sz.cpp
```
Expected (spike-measured 2026-06-17, full combined): host `TT2Track = 9512`. This **exceeds** `NoteTrack (9468)`, so `TT2Track` becomes the largest `Track` arm and the union grows to **9536** — `Project._tracks[8]` inflates. That's fine: the ARM build (Step 3) was measured to fit. The host gate is no longer a "stay under" target; the ARM `.bss` budget is.

**Step 2: Set the exact assert**

Modify `src/apps/sequencer/model/TT2Track.h:46` to the measured value:
```cpp
static_assert(sizeof(TT2Track) == 9512, "TT2Track size drift");  // <-- spike-measured (10 scripts + 64 delay + 8 trig + dashboard + shaping)
```

**Step 3: GATE — STM32 size verification (hard decision point)**

Use the **PROJECT.md** measurement rule (there is no `STM32.md`): build the STM32 release and record `.data`, `.bss`, `.ccmram_bss`, and the `sizeof` of `Model`, `Engine`, `Track`, and each changed type (`TeletypeProgram`, `TT2Runtime`, `TT2Track`).

Run: `make -C build/stm32/release sequencer 2>&1 | grep -iE "region|overflow|\.bss|\.data|\.ccmram"`
Then re-probe the ARM `sizeof(TT2Track)` (size-print TU compiled for the target, or read the `.map`).

**Measured outcome (2026-06-17 spike — accept the RAM):** the union does inflate (`TT2Track` 9512 is now the largest arm), but STM32 release **links clean, no overflow**: `.bss` 113,216 → **114,112 (+896 B)**, `.data`/`.ccmram_bss` flat, `.data+.bss` ~117.9 KB (~2.1 KB under the 120 KB warning). So **proceed with full 64-deep delay + 8 triggers — no `TT2_DELAY_DEPTH` fallback.** Re-confirm these exact numbers at execution (they shift if anything else lands first); the fallback (trim `TT2_DELAY_DEPTH` toward 48) only re-enters if a future addition pushes `.data+.bss` past the warning.

**Step 4: Commit**

```bash
git add src/apps/sequencer/model/TT2Track.h
git commit -m "feat(tt2): update TT2Track size gate for HW-parity layout"
```

---

### Task 4: Editor — reach all 10 scripts (Option A panel bank + keyboard)

**Files:**
- Modify: `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` (the `key.pageModifier()` block in `keyPress`, ~`:344`)

**Decided binding (Option A):** F1–F4 = S1–S4 (F4 keeps S4↔Metro re-press), **page+F1–F4 = S5–S8**, **page+F5 = Init**. `shift+F1–F4` (fire) and F5 (live/pattern) unchanged. `[`/`]` USB keyboard also steps all 10 (end-clamp, `:992`) and the `S%d`/`M`/`I` label auto-adapts — no OLED change.

**Step 1: Add the page+F handler**

In the existing `if (key.pageModifier()) { … }` block in `keyPress` (alongside the left/right/step handlers, before its `return`), add function-key handling:

```cpp
} else if (key.isFunction()) {
    int fn = key.function();
    if (fn >= 0 && fn < 4) {                 // page+F1..F4 -> S5..S8 (indices 4..7)
        setScriptIndex(4 + fn);
        event.consume();
    } else if (fn == 4) {                    // page+F5 -> Init
        setScriptIndex(TT2_INIT_SCRIPT);
        event.consume();
    }
}
```
(Indices 4–7 are the numbered scripts 5–8; valid only once `TT2_SCRIPT_COUNT == 10` from Task 1. Metro stays reachable via F4's existing S4↔Metro re-press; `setScriptIndex` already clamps to `TT2_SCRIPT_COUNT`.)

**Step 2: Build the sim (user tree) + audition**

Run: `make -C build/sim/debug sequencer`
Expected: `Built target sequencer`. Audition on sim: F1–F4 → S1–S4; F4 re-press → Metro; page+F1–F4 → S5–S8; page+F5 → Init; `[`/`]` walks all 10; label shows S1–S8/M/I correctly.

**Step 3: Commit**

```bash
git add src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp
git commit -m "feat(tt2): page+F selects S5-S8/Init (10-script panel nav, Option A)"
```

---

### Task 5: SCRIPT-op reaches all 10 editable scripts (numbered 1–8, Metro=9, Init=10), rejects 11

**Parity rule:** upstream extended `SCRIPT` to call Metro/Init — `SCRIPT 9`/`SCRIPT 10` (`teletype/CHANGELOG.md:88`), and `op_SCRIPT_set` accepts `< EDITABLE_SCRIPT_COUNT` = 10 (`teletype/src/ops/controlflow.c:301`). TT2's `opScript` is 1-based — `SCRIPT N → runScript(N-1)` — and already clamps `zeroBased >= TT2_SCRIPT_COUNT` → `OutOfRange` (`TeletypeNativeOps.cpp:2364-2368`). So once `TT2_SCRIPT_COUNT` is 10, the op *should* accept 1..10 and reject 11 with **no code change** — this task is a guard test that proves it, exercising the real lower → `evaluateCommand` path (not `runScript` directly).

**Files:**
- Test: `src/tests/unit/sequencer/TestTeletypeV2Function.cpp` (add a CASE)
- Verify-only: `src/apps/sequencer/engine/TeletypeNativeOps.cpp:2358` (`opScript`)

**Step 1: Write the test — run real `SCRIPT N` commands**

```cpp
CASE("hw_parity_SCRIPT_reaches_metro_and_init") {
    TeletypeProgram program; init(program);
    TT2Runtime runtime; init(runtime);
    TT2OutputState output; init(output);
    loadScriptText(program, 7, "A 8\n");                 // numbered script 8 (idx 7)
    loadScriptText(program, TT2_METRO_SCRIPT, "B 9\n");  // Metro (idx 8) <- SCRIPT 9
    loadScriptText(program, TT2_INIT_SCRIPT, "X 10\n");  // Init  (idx 9) <- SCRIPT 10
    auto runCmd = [&](const char *text) {
        tele_command_t src{}; char err[TELE_ERROR_MSG_LENGTH];
        parse(text, &src, err);
        TT2Command cmd{}; lowerCommand(src, cmd);
        return evaluateCommand(cmd, runtime, output, &program);
    };
    runCmd("SCRIPT 8");   expectEqual(int(runtime.variables.a), 8,  "SCRIPT 8 -> numbered 8");
    runCmd("SCRIPT 9");   expectEqual(int(runtime.variables.b), 9,  "SCRIPT 9 -> Metro");
    runCmd("SCRIPT 10");  expectEqual(int(runtime.variables.x), 10, "SCRIPT 10 -> Init");
    TT2EvalResult r = runCmd("SCRIPT 11");
    expectTrue(r.error == TT2EvalError::OutOfRange, "SCRIPT 11 rejected (only 10 editable)");
}
```
(Match the file's existing include/fixture style — `loadScriptText`, `parse`, `lowerCommand`, `evaluateCommand` are already used across `TestTeletypeV2*`.)

**Step 2: Run**

Run: `make -C build TestTeletypeV2Function && ctest --test-dir build -R TeletypeV2Function --output-on-failure`
Expected: PASS with no source change. If `SCRIPT 9/10` fail, the clamp is wrong — confirm it is `>= TT2_SCRIPT_COUNT` (not a stale `>= 8` or `> REGULAR_SCRIPT_COUNT`) and fix.

**Step 3: Trigger inputs fire all 8 numbered scripts (4→8)**

With `TT2_TRIGGER_INPUT_COUNT = 8` (Task 1), `updateInputTriggers` (`TT2TrackEngine.cpp`) now samples **8** trigger inputs firing scripts **0..7** — `TI i → script i`, the original-Teletype 8-trig→8-script mapping (was 4 inputs → 0..3). The loop is already bounded by `TT2_TRIGGER_INPUT_COUNT`, so it scales with the constant; verify `_prevInputState`/`inputLevel` are sized `[8]` and `tt2RisingEdges` gets the new count. Metro (8) / Init (9) stay `SCRIPT`/call/MIDI-fired, not trigger-bound. Add a CASE: 8 distinct trigger sources fire scripts 0–7.

**Step 4: Commit**

```bash
git add src/tests/unit/sequencer/TestTeletypeV2Function.cpp src/apps/sequencer/engine/TeletypeNativeOps.cpp
git commit -m "test(tt2): SCRIPT reaches all 10 editable scripts (Metro=9, Init=10), rejects 11"
```

---

### Task 6: Full suite + sim demo sanity

**Step 1: Reconfigure (new test files added) + build all tests**

Run: `cmake -S . -B build && make -C build`
Expected: builds clean (all size asserts satisfied).

**Step 2: Run the whole TT2 suite**

Run: `ctest --test-dir build -R "TeletypeV2|TT2|Geode|Modulator" --output-on-failure`
Expected: 100% pass. (Tests reference `TT2_SCRIPT_COUNT`/`TT2_METRO_SCRIPT` symbolically, so they adapt; watch `TestTeletypeV2Metro`, `TestTeletypeV2NativeOps` turtle-NO_SCRIPT, `TestTeletypeV2ScriptRunner`.)

**Step 3: Build both trees**

Run: `make -C build/sim/debug sequencer && make -C build/stm32/release sequencer`
Expected: both `Built target sequencer`, no region overflow on STM32.

**Step 4: Commit any test fixups**

```bash
git add -A && git commit -m "test(tt2): suite green at 10-script / 64-delay parity"
```

---

## Phase 1B — I/O config page + per-output shaping

Brings the TT1 I/O config back as TT2 track params. The input/clock fields (groups 1–3) already exist in `TeletypeProgram` + are engine-honored — they only need an editor. The per-CV-output shaping is new model + engine + UI. Output **dests** are out (Performer routing owns them).

### Task 7: Per-CV-output shaping fields on `TeletypeProgram`

**Files:**
- Modify: `src/apps/sequencer/model/TeletypeProgram.h`
- Test: `src/tests/unit/sequencer/TestTeletypeV2OutputShaping.cpp` (add a CASE)

**Step 1: Failing test — fields exist with sane defaults**

```cpp
CASE("hw_parity_output_shaping_defaults") {
    TeletypeProgram p; init(p);
    for (int i = 0; i < TT2_OUTPUT_CV_COUNT; ++i) {
        expectTrue(p.cvOutputRange[i] == Types::VoltageRange::Bipolar5V, "default ±5V");
        expectEqual(int(p.cvOutputOffset[i]), 0, "default offset 0");
        expectEqual(int(p.cvOutputQuantizeScale[i]), -1, "default no quantize");
        expectEqual(int(p.cvOutputRootNote[i]), 0, "default root 0");
    }
}
```

**Step 2: Run → fail (fields don't exist).**
Run: `make -C build TestTeletypeV2OutputShaping` → compile error.

**Step 3: Add the arrays (×`TT2_OUTPUT_CV_COUNT` = 8) + `init()` defaults + accessors**, mirroring TT1 `TeletypeTrack` (`:142-145, :360-381`): `cvOutputRange[8]` (`Types::VoltageRange`), `cvOutputOffset[8]` (`int16`), `cvOutputQuantizeScale[8]` (`int8`, −1 = off), `cvOutputRootNote[8]` (`int8`). Cost ≈ 8×5 = 40 B + alignment → bump `TeletypeProgram` assert accordingly (re-measure; ~3640 already has margin).

**Step 4: Run → pass. Commit.**
```bash
git commit -am "feat(tt2): per-CV-output range/quantize/offset/root-note fields"
```

### Task 8: Apply shaping in the engine CV output path

**Files:**
- Modify: `src/apps/sequencer/engine/TT2TrackEngine.h/.cpp` (`cvOutput(index)`)
- Test: `TestTeletypeV2OutputShaping.cpp`

**Step 1: Failing test — range scales, offset shifts, quantize snaps**

```cpp
CASE("hw_parity_output_shaping_applied") {
    // raw mid-scale through Unipolar5V range -> 2.5V (not the bipolar -0V).
    // offset shifts; quantizeScale>=0 snaps to scale+root. (Assert against the
    // shaped cvOutput(), mirroring TT1 TeletypeTrackEngine output shaping.)
}
```
(Fill with concrete raw→volts pairs once the transform is written; mirror TT1's per-output application so values match.)

**Step 2: Run → fail. Step 3: Implement** the transform in `cvOutput(index)` — apply `cvOutputRange`/`Offset`/`QuantizeScale`/`RootNote[index]` to the raw before `rawToVolts`, mirroring TT1 `TeletypeTrackEngine`. **Step 4: pass. Commit.**

### Task 9: TT2 I/O config page — routing-matrix style, INPUTS + OUTPUTS views

Layout decided + `ui-preview`-validated (`render_tt2_io.py` → `tt2-io/tt2-io-in.png`, `tt2-io-out.png`). Modeled on `RoutingPage::drawTabEditor`: WindowPainter header (title + centered view tag + `hline(0,10)`), name-gutter matrix with column headers + right scrollbar on overflow, 5-cell function footer (`WindowPainter::drawFooter`). F1/F2 toggle INPUTS/OUTPUTS.

**Files:**
- Create: `src/apps/sequencer/ui/pages/TT2IoConfigPage.{h,cpp}` — a `BasePage` drawing the matrix directly (mirror `RoutingPage`'s draw/key/encoder + `WindowPainter` header/footer), over `tt2Track().program()`. **Not** a `ListModel` (it's a matrix).
- Modify: `src/apps/sequencer/ui/pages/TrackPage.cpp:196` (empty `TeletypeV2` case) → push/show this page; `Pages.h` for the instance.

**Step 1 — INPUTS view (2-up packed):** two name/source pairs per row (~5 rows): T1–4 → `program().triggerSource[i]`, IN/PARAM/X/Y/Z/T → `program().cvInputSource[i]`. Sources as 2-char codes (I1/G3/L1/O1/R1/C1, `-`=None). **MIDI source** as a top-right header chip. Cursor selects a pair's value; encoder edits the enum. No boot, no div/mult.

**Step 2 — OUTPUTS view:** 8 rows (CV1–8) × 4 cols (RNG/QNT/OFF/ROOT) over the Task 7 fields; routing-style scroll + scrollbar; cursor selects a cell, encoder edits. `QNT` off shown as `-`.

**Step 3:** F1/F2 toggle the view (footer highlights the active one); wire `TrackPage` `TeletypeV2` case to the page. Track-mode + stale-page guard like the other TT2 pages.

**Step 4:** `make -C build/sim/debug sequencer`; audition both views edit + persist into the program. Geometry is `ui-preview`-validated; re-render only if it changes.

**Step 5: Commit.**
```bash
git commit -am "feat(tt2): I/O config page (routing-matrix; INPUTS 2-up + OUTPUTS shaping)"
```

---

## Phase 2+ — Roadmap (separate plans, referenced)

These complete "parity as an instrument" but each needs its own plan/brainstorm; they are **not** bite-sized here because the architecture is undecided or larger:

1. **Human-readable text save/load** (one program ↔ `.txt` scene). Printer/parser exist; needs a TT2 scene-text format + `FileManager` TT2 overloads. The TT1 precedent (`FileManager.cpp:707`) writes `NAME`, `#IO`, `#S4`, `#M`, `#S1`–`#S3`, `#PATS` (note: TT1 splits S1–3 shared vs S4 per-slot — TT2 has one program, so it would emit a uniform `#S1`–`#S8`/`#M`/`#I`/`#PATS`/`#C`). **Recommended next** — self-contained, high value, no quad.
2. **I/O routing-assignment editor** — `2026-06-14-001` deferred bucket; engine already honors `triggerSource`/`cvInputSource`, only the edit UI is missing.
3. **Scene bank** (programs per w-pattern) — `2026-06-14-004` supertrack quad, or token packing. Evaluation (this session): full-parity program fills the slot, so a bank is quad (~9–10 full-parity scenes) or packed (~4–5); the trivial in-slot array gives ~1 at full parity. Storage A/B and reset-on-w-pattern-nudge are still open — needs `/ce-brainstorm`.

---

## Verification

- All new asserts compile (size bounds hold); `TT2Track` exact-size assert matches the measured value.
- Full `TestTeletypeV2*` suite green; new CASEs (10-script layout, 64-delay depth, SCRIPT reaches 1..10 incl Metro=9/Init=10 and rejects 11) pass.
- STM32 release links with no region overflow; `TT2Track` measured ≤ the ARM model gate (or the accepted/trimmed value from Task 3 Step 3).
- Sim: a TT2 track edits + runs S1–S8, M, I; delay queue holds >8 pending commands.
- Execution posture: test-first per task (assert/behavior test before the bump).

## Rollback

Each task is one commit; revert in reverse. The size-gate (Task 3) is the only risk point — if STM32 overflows and the RAM isn't acceptable, trim `TT2_DELAY_DEPTH` and re-run Tasks 2–3 rather than abandoning the script bump (scripts alone keep `TT2Track` well under the gate).
