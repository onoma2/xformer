---
id: feat-tt2-envelope-alias-ops
schema: plan
title: "feat: TT2 E.* envelope ops — ADSR-locked aliases over the modulator engine"
type: feat
status: active
date: 2026-06-17
depth: lightweight
---

# TT2 `E.*` Envelope Ops — ADSR-locked modulator aliases — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Revive the legacy `E.*` op family in native TT2 as ergonomic sugar over the existing modulator engine. Writing any `E.*` op claims modulator slot `n` as a triggered envelope — it **forces `Shape::ADSR` + `Mode::Trig`**, then sets the addressed field. The op family *is* the shape selector, so a script never calls `MO.SHAPE`/`MO.MODE` to build an envelope.

**Architecture:** Pure engine layer. The `E.*` op tokens already parse (they live in `op_enum.h` + `match_token.rl` + the name table) but their dispatch-table slots are `nullptr`, so they currently error with `UnsupportedOp` at run. This plan wires those slots to handlers that run a shape-lock prologue then `paramSet` the modulator field — exactly mirroring the shipped `MO.*` ops (`TT2_MO_FIELD_OP`). No new model fields, no RAM growth, no serialization change. The modulator's `Param` dictionary already exposes every field needed (`Shape, Mode, Attack, Decay, Offset, Amplitude`).

**Tech Stack:** C++17, STM32F405. Headless unit tests under `build/` (UnitTest + ctest).

---

## Context & references

- **Modulator model** — 8 global slots (`CONFIG_MODULATOR_COUNT = 8`, `Config.h:60`). `Modulator::Param` (`Modulator.h`) addresses `Shape, Rate, Depth, Mode, Offset, Attack, Decay, Sustain, Release, Amplitude, …` via `paramSet(addr, v)` / `paramGet(addr)`. Field ranges (all clamped in their setters):
  - `Attack` / `Decay`: 0–2000 ms
  - `Amplitude`: 0–127
  - `Offset`: −64…+63
  - `Shape::ADSR`, `Mode::Trig` are the envelope shape/mode.
- **MO.* ops are the pattern to mirror** — `TeletypeNativeOps.cpp` `opMo` (bare read), `opMoTrig`, and the `TT2_MO_FIELD_OP(fn, paramName)` macro (`~:2050-2072`). Each pops a 1-based slot index via `popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)`, then `hostModulator(idx).paramSet(addr, v)` on set or pushes `paramGet` on read. Trigger is `hostModulatorTrigger(idx)`.
- **Dispatch table** — `opTableBuilder` (`TeletypeNativeOps.cpp:~3679`) inits all slots to `nullptr`, then wires op-by-op. `E_OP_E`, `E_OP_E_A`, `E_OP_E_D`, `E_OP_E_O`, `E_OP_E_T` are currently left `nullptr` → `TT2EvalError::UnsupportedOp` at eval (`TT2Evaluator.h:113-114`).
- **Op names already exist** — `TT2OpNames.cpp:216-222` returns `"E.A"/"E.D"/"E.T"/"E.O"/"E.L"/"E.R"/"E.C"`; the bare `E_OP_E` token too. Parser recognizes them (`teletype/src/match_token.rl`).
- **Legacy semantics (this fork)** — `tl-manual.md:163-176`: per-CV envelope. `E n x` = peak level, `E.A` = attack ms, `E.D` = decay ms, `E.O` = offset, `E.T` = trigger. (Legacy had no sustain/release ops — it's an AD-style envelope.)
- **Origin context** — `docs/brainstorms/2026-06-14-tt2-mod-geode-ops-requirements.md` spec'd the `MO.*` modulator-control ops (shipped) and deliberately had *no* native `E.*`. This plan layers `E.*` back as opinionated sugar on that same modulator engine; it does not redefine modulator control.

---

## Key decisions

- **Index `n` = modulator slot 1–8**, via `popOutputIndex(…, CONFIG_MODULATOR_COUNT)` — identical to `MO.*`. The natural default mod[n]→jack[n] preserves the legacy per-CV feel. (Diverges from legacy `E.*` which indexed by CV channel; the modulator architecture makes slot-indexing the clean mapping, and there are 8 of each.)
- **Family lock on every call.** Every `E.*` write first forces `Shape::ADSR` and `Mode::Trig` on the slot, then applies the field. This is the ergonomic win — the op family selects the shape, so no `MO.SHAPE`/`MO.MODE` needed. It is opinionated: sharing a slot with `MO.*` is last-writer-wins. `Mode::Trig` is included because an envelope is a triggered one-shot; an `E.A`/`E.D`/`E.O` write on a slot a user had set to `Mode::Gate` will flip it to `Trig`.
- **Values use native modulator ranges**, not legacy teletype raw 0–16383. Each field clamps in its own setter, so out-of-range script values saturate rather than wrap.
- **Bare `E` mirrors bare `MO`** — `E n x` sets `Amplitude`; `E n` (no value) reads the slot's current modulator output. Set also applies the family lock; read does not.
- **Sustain/Release stay at modulator defaults** — legacy `E.*` exposed no S/R ops, so the ADSR runs its default sustain/release.

---

## Scope Boundaries

**In scope:** wire the five `E.*` dispatch slots (`E`, `E.A`, `E.D`, `E.O`, `E.T`) to handlers with the ADSR+Trig shape-lock prologue; headless tests proving parse→lower→evaluate, field writes, clamping, shape-lock, and out-of-range slot rejection.

**Out of scope / deferred (later plan):**
- **All of `LFO.*`** — rate is the linchpin and its representation (Hz via `RateDomain::Free` vs divisor via `RateDomain::Tempo`) is an open decision. `LFO.A`/`LFO.O` alone can't drive an LFO.
- **`E.L`** (loop k cycles) — modulator has only free/one-shot, no "exactly k".
- **`E.R` / `E.C`** (pulse TR at end of rise / cycle) — modulator has no phase-event→gate hook.
- Any model field, UI page, or serialization change. This is engine-dispatch wiring only.

---

## Implementation Units

### U1. `E.*` envelope-alias op handlers + dispatch wiring

**Goal:** Make `E`, `E.A`, `E.D`, `E.O`, `E.T` run as ADSR-locked modulator aliases instead of erroring with `UnsupportedOp`.

**Requirements:** Delivers the clean `E.*` set from the spec discussion (envelope sugar over the modulator engine).

**Dependencies:** none (builds on shipped `MO.*` infrastructure).

**Files:**
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp` — add handlers + table wiring.
- Test: `src/tests/unit/sequencer/TestTeletypeV2EnvelopeOps.cpp` (new).
- Modify: `src/tests/unit/sequencer/CMakeLists.txt` — register the test.

**Approach:**
- Add a shape-lock prologue helper that, given slot `n`, sets `Shape = ADSR` and `Mode = Trig` on `hostModulator(n)` (via `paramSet` with the `Modulator::Param` addrs, matching `TT2_MO_FIELD_OP`).
- Field setters (`E.A`→`Attack`, `E.D`→`Decay`, `E.O`→`Offset`, bare `E`→`Amplitude`): run the prologue, then `paramSet(addr, v)` on set; on read (`isSet` false) push `paramGet(addr)` — except bare `E` read pushes `hostModulatorOutput(n)` like bare `MO`.
- `E.T`: run the prologue (ensures `Mode::Trig`), then `hostModulatorTrigger(n)`. No value.
- Guard the null-host case exactly as the `MO.*` handlers do (`tt2ActiveHost()` may be null).
- Wire the five slots in `opTableBuilder` next to the `MO.*` block.

**Patterns to follow:** `TT2_MO_FIELD_OP` macro, `opMo`, `opMoTrig`, and the `MO.*` table-wiring block in `TeletypeNativeOps.cpp`. Reuse `popOutputIndex(…, CONFIG_MODULATOR_COUNT)` for the 1-based slot.

**Execution note:** Test-first — write the failing CASEs (op currently returns `UnsupportedOp`) before wiring the table.

**Test scenarios** (mirror the host-spy fixture in `TestTeletypeV2Modulator.cpp`; run real `parse → lowerCommand → evaluateCommand`):
- **Happy path — field writes set the right modulator field:**
  - `E.A 1 500` → modulator 1 `attack == 500`.
  - `E.D 1 300` → modulator 1 `decay == 300`.
  - `E.O 1 -10` → modulator 1 `offset == -10`.
  - `E 1 100` → modulator 1 `amplitude == 100`.
- **Shape lock:** preset modulator 2 to `Shape::Sine` / `Mode::Gate`, then `E.A 2 400` → `shape == ADSR` and `mode == Trig` (and `attack == 400`).
- **Trigger:** `E.T 1` → modulator 1 `mode == Trig` and a trigger is registered (assert via the same mechanism `TestTeletypeV2Modulator` uses for `MO.TRIG`).
- **Bare read vs set:** `E 1 90` sets amplitude 90; bare `E 1` (no value) pushes the slot's current modulator output (not the amplitude field), matching bare `MO`.
- **Edge — clamping:** `E.A 1 9999` → `attack == 2000` (setter clamp); `E.O 1 -999` → `offset == -64`; `E.O 1 999` → `offset == 63`; `E 1 999` → `amplitude == 127`.
- **Error — out-of-range slot:** `E.A 9 500` and `E.A 0 500` → `TT2EvalError::OutOfRange` (1-based, `CONFIG_MODULATOR_COUNT == 8`), no modulator mutated.
- **Integration — pre-wiring guard (RED state):** before the table is wired, `E.A 1 500` evaluates to `UnsupportedOp` (confirms the test exercises the real dispatch path, not a stub).

**Verification:** New test file passes under `ctest -R TeletypeV2EnvelopeOps`; full `TestTeletypeV2*` suite stays green; sim and STM32 release both link clean (no model/RAM change expected). A script with `E.A n / E.D n / E.O n / E.T n` produces a triggered ADSR on modulator slot `n` with no `MO.SHAPE`/`MO.MODE` calls.

---

## Verification

- New `E.*` CASEs pass (field writes, shape-lock, trigger, bare read/set, clamping, out-of-range, pre-wiring `UnsupportedOp`).
- Full `TestTeletypeV2*` suite green (`ctest --test-dir build -R "TeletypeV2|TT2|Modulator"`).
- Sim (`build/sim/debug`) and STM32 release (`build/stm32/release`) link clean; `.data`/`.bss`/`.ccmram_bss` flat (dispatch-only change, no new model state).
- Posture: test-first.

## Rollback

Single unit, single commit; revert restores the `nullptr` dispatch slots (ops fall back to `UnsupportedOp`). The token set is unchanged, so nothing else is affected.
