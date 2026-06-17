---
id: feat-tt2-print-dashboard
schema: plan
title: "feat: TT2 PRINT/PRT — 16-slot dashboard store + live-mode display"
type: feat
status: active
date: 2026-06-17
depth: standard
---

# TT2 `PRINT` / `PRT` — Dashboard Value Store + Live-Mode Display — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Bring `PRINT` to native TT2 as a **16-slot dashboard value store** — `PRINT n x` writes slot n, `PRINT n` reads it — matching the TT1-track precedent and original Teletype. Ship the store first (invisible, engine-only), then **display the slots in the live-mode body** (the empty lines on the script page), where the hardware shows its dashboard.

**Architecture:** Two stages, `a → b`.
- **(a)** A 16-slot `int16` array of **ephemeral runtime state** in `TT2Runtime` (not serialized — like the `X`/`A`/`B` vars and the hardware dashboard). Wire the two currently-`nullptr` dispatch slots `E_OP_PRINT` (get/set) and `E_OP_PRT` (alias) to read/write it. Pure engine, same dispatch pattern as the shipped `E.*`/`LFO.*` ops.
- **(b)** Render the 16 slots in `TeletypeScriptViewPage`'s **live-mode body free lines**. Live mode draws 6 rows (`kLineCount=6`): line 3 = last history command, line 4 = `_liveResult`, **lines 0/1/2/5 blank** — that's the dashboard's home, mirroring how hardware shows the dashboard in LIVE mode. `ui-preview`-validated per the project display gate.

**Tech Stack:** C++17, STM32F405. Headless unit tests under `build/`; sim under `build/sim/debug`; `ui-preview/` render for (b).

---

## Context & references

- **TT1-track precedent** — `TeletypeBridge.cpp`: `print_dashboard_value(index, value)` writes `g_dashboardValues[16]` (a `std::array<int16_t,16>`), `get_dashboard_value(index)` reads it. Index is 1-based at the op (`PRINT 1` → slot 0). The TT1 track stored values but **never displayed them**. This plan replicates the store natively and *adds* the display.
- **Original Teletype** — `PRINT n x` / `PRINT n` populate a labeled dashboard shown on the LIVE-mode dashboard screen (`teletype/module/live_mode.c`). `set_live_submode`/sub-screens are out of scope (no Performer analog).
- **Unwired tokens** — `E_OP_PRINT` ("PRINT") and `E_OP_PRT` ("PRT", alias) exist in the enum + parse, but their dispatch slots are `nullptr` → `UnsupportedOp` today (`TT2Evaluator.h:113`). Legacy `op_PRINT` is `MAKE_GET_SET_OP` (get + set); `op_PRT` is an alias.
- **Dispatch pattern to mirror** — the `E.*`/`LFO.*` field ops in `TeletypeNativeOps.cpp` (`popOutputIndex` for the 1-based index, `isSet && stackSize>=1` to branch set vs get, null-host guard, table wiring next to the `MO.*`/`E.*`/`LFO.*` block).
- **Runtime home** — `TT2Runtime` / `TT2Variables` (`TeletypeRuntime.h`): the vars (`a,x,b,y,c,z,d,t`, `mutes`) live in `TT2Variables`. Size asserts at `:332` (`TT2Variables <= 440`), `:342` (`TT2Runtime <= 2296`). A `dashboard[16]` adds 32 B — bump whichever assert it lands under.
- **Live-mode draw** — `TeletypeScriptViewPage::draw` (`:97-162`): the live-mode branch fills line 3 (history) and line 4 (result); lines 0/1/2/5 are left blank. Geometry: rows at `y = kRowStartY(4) + i*kRowStepY(8)`, text at `kTextX(16)`, left of the HUD strip (`kHudX=192`). The HUD (`drawHud`, CV/TR/TI) is unchanged.

---

## Key technical decisions

- **16 slots, 1-based index** (`PRINT 1` → slot 0), matching TT1 and hardware. Out-of-range index → `OutOfRange`, no write.
- **Ephemeral runtime state, not serialized.** Lives in `TT2Runtime`; cleared on `init(runtime)`. Matches the hardware dashboard (runtime-only) and the no-migration policy — no project/program field, no serialization.
- **`PRT` = exact alias of `PRINT`** — same handler, both table slots point at it (mirrors the `MO.*`/`MO.x` alias pairs).
- **Display only in live mode, in the body free lines.** The dashboard is a LIVE-mode readout (hardware parity); edit/script mode is unchanged. Lines 0/1/2/5 host the slots; lines 3/4 (history/result) stay as-is. Exact slot-per-line grid (e.g. 4×4) and label format decided at the `ui-preview` step — the body is ~176 px wide (`kHudX - kTextX`), comfortably fits 4 compact `n:val` cells per line.
- **No HUD-strip change.** The right strip (`kHudX..255`) remains the CV/TR/TI monitor; `PRINT` does not compete with it.

---

## Scope Boundaries

**In scope:** the 16-slot runtime store; `PRINT`/`PRT` dispatch (get/set); live-mode body rendering of the slots; `ui-preview` validation of the layout; headless tests for the store; size-assert update.

**Out of scope:**
- Serialization / persistence of dashboard values (ephemeral by design).
- `LIVE.*` sub-screen switching (`set_live_submode`) — no Performer analog (see `docs/unsupportedops-status.md`).
- Any HUD-strip (CV/TR/TI) change.
- Labels/naming of slots beyond a numeric index (hardware allows named dashboards; not replicated here).

---

## Implementation Units

### U1. `PRINT`/`PRT` 16-slot runtime store + dispatch (stage a)

**Goal:** `PRINT n x` writes slot n, `PRINT n` reads it, against a 16-slot ephemeral runtime array; `PRT` aliases `PRINT`. Replaces the current `UnsupportedOp`.

**Requirements:** Native parity with the TT1-track dashboard value store.

**Dependencies:** none.

**Files:**
- Modify: `src/apps/sequencer/model/TeletypeRuntime.h` — add `int16_t dashboard[16]` (in `TT2Variables` or `TT2Runtime`), clear it in `init`, bump the affected size assert.
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp` — `opPrint` handler + wire `E_OP_PRINT` and `E_OP_PRT`.
- Test: `src/tests/unit/sequencer/TestTeletypeV2Print.cpp` (new); register in `src/tests/unit/sequencer/CMakeLists.txt`.

**Approach:** `opPrint`: pop 1-based index (reject out-of-range → `OutOfRange`); on set (`isSet && stackSize>=1`) pop value, store to `dashboard[idx]`; on get, push `dashboard[idx]`. The dashboard array lives in the runtime (reachable from the evaluator via the `runtime` arg — unlike `MO.*`/`E.*` which go through the host, `PRINT` is pure runtime state, so the handler reads `runtime` directly, no host needed). Clear in `init(runtime)`.

**Execution note:** Test-first — failing CASEs (op returns `UnsupportedOp`) before wiring.

**Patterns to follow:** the `E.*` field-op handler shape in `TeletypeNativeOps.cpp` for index/set/get/clamp; runtime-var access pattern (ops that read `runtime.variables` directly rather than the host).

**Test scenarios** (mirror `TestTeletypeV2EnvelopeOps.cpp`; real `parse → lowerCommand → evaluateCommand`):
- **Happy path:** `PRINT 1 500` then `PRINT 1` → pushes `500`; `PRINT 16 -3` then `PRINT 16` → `-3`.
- **Alias:** `PRT 2 42` then `PRINT 2` → `42` (and `PRT 2` → `42`) — alias hits the same store.
- **Independence:** writing slot 1 doesn't change slot 2.
- **Init clears:** after `init(runtime)`, `PRINT 1` → `0`.
- **Error — out of range:** `PRINT 0 5` and `PRINT 17 5` → `OutOfRange`, no write.
- **Negative/large values:** `int16` round-trips (e.g. `PRINT 3 -12345` → `-12345`); no clamping (raw store).

**Verification:** new test green; full `TestTeletypeV2*` suite green; sim + STM32 release link clean; `TT2Runtime`/`TT2Variables` size asserts pass (32 B added). A script using `PRINT`/`PRT` stores and reads back values; no `UnsupportedOp`.

---

### U2. Live-mode dashboard display (stage b)

**Goal:** Show the 16 `PRINT` slots in the live-mode body free lines (0/1/2/5) of the script page, so written values are visible in live mode — hardware-faithful.

**Requirements:** Surface the U1 store; close the one feature gap vs hardware (dashboard readout).

**Dependencies:** U1.

**Files:**
- Modify: `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` — live-mode branch of `draw` (the `i==3/i==4` block, `:126-162`): render dashboard cells on the blank lines.
- Add/update: `ui-preview/pages_*.py` + `ui-preview/generate.py` wiring; render to `ui-preview/tt2-live/` per the project display gate.

**Approach:** In the live-mode body, draw the 16 slots as a compact grid on lines 0/1/2/5 (lines 3/4 keep history/result). Each cell shows index + value in `tiny5x5` within the ~176 px text region (`kTextX..kHudX`). Exact rows-used, cells-per-line, and label format (`1:500` vs bars vs only-nonzero) are decided from the `ui-preview` render — validate glyph fit and safe area (y 11..52) before finalizing. The HUD strip and edit/prompt line are untouched.

**Execution note:** Display work — render with `ui-preview` and `open` the PNG for the user to approve the layout *before* finalizing (project display-gate rule); not test-first.

**Patterns to follow:** `RoutingPage`/existing `drawHud` cell-drawing for compact value rendering; `ui-preview/UI-VARIANTS.md` geometry contract; the display-gate workflow in `CLAUDE.md`.

**Test scenarios:** `Test expectation: none — pure OLED rendering`. Verification is visual via `ui-preview` + sim audition (the U1 store is already unit-tested).

**Verification:** `ui-preview` render shows the 16 slots legibly in the live-mode body, no glyph collision, within the safe area; history (line 3) and result (line 4) still render; HUD strip unchanged; sim audition — `PRINT` from a script/live command updates the displayed cell. User approves the rendered layout.

---

## System-wide impact

- **Engine:** +32 B in `TT2Runtime` (one size-assert bump). No host-interface change (`PRINT` is pure runtime state).
- **UI:** live-mode draw only; edit/script mode, the HUD strip, and the prompt line are unchanged. Stale-page/track-mode guards already present on the page.
- **Serialization:** none (ephemeral runtime).

---

## Verification

- U1 store CASEs pass (set/get, alias, independence, init-clear, out-of-range, int16 round-trip).
- Full `TestTeletypeV2*` suite green; sim + STM32 release link clean; size asserts hold.
- U2: `ui-preview` layout approved; sim shows live-mode dashboard updating from `PRINT`; history/result/HUD intact.
- Posture: U1 test-first; U2 ui-preview-then-implement.

## Rollback

Two units, two commits. U2 revert restores the blank live-mode lines (store still works, just invisible — the TT1-track state). U1 revert restores `UnsupportedOp` for `PRINT`/`PRT` and removes the runtime field. Revert in reverse.
