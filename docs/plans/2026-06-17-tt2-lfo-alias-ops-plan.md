---
id: feat-tt2-lfo-alias-ops
schema: plan
title: "feat: TT2 LFO.* ops — Sine-locked free/clocked aliases over the modulator engine"
type: feat
status: active
date: 2026-06-17
depth: standard
---

# TT2 `LFO.*` Ops — Sine-locked modulator aliases (free + clocked rate) — Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Add the `LFO.*` op family to native TT2 as ergonomic sugar over the existing modulator engine — the sibling of the shipped `E.*` envelope ops. Writing any `LFO.*` op claims modulator slot `n` as a free-running sine LFO (forces `Shape::Sine` + `Mode::Run`), then sets one field. Two rate ops cover the modulator's two rate domains: `LFO.R` (free, Hz) and `LFO.C` (clocked, note divisor).

**Architecture:** Mostly the same dispatch-wiring pattern as `E.*` (`TeletypeNativeOps.cpp`) — handlers run a shape-lock prologue then write a modulator field via the `Modulator::Param` dictionary. `LFO.R`/`LFO.A`/`LFO.O` reuse op tokens that already parse (their dispatch slots are currently `nullptr` → `UnsupportedOp`). The one new surface is `LFO.C`: its token does not exist, so it requires adding `E_OP_LFO_C` to the vendored teletype parser (`op_enum.h` + `match_token.rl`, regenerated with `ragel`) and the name table, then a dispatch handler. No model fields, no RAM growth, no serialization change.

**Tech Stack:** C++17, STM32F405. Vendored teletype parser is Ragel-generated (`teletype/src/match_token.rl` → committed `teletype/src/match_token.c`); `ragel 6.10` confirmed available locally. Headless unit tests under `build/` (UnitTest + ctest).

---

## Context & references

- **Sibling precedent — the shipped `E.*` plan/code.** `docs/plans/2026-06-17-tt2-envelope-alias-ops-plan.md` and its implementation in `TeletypeNativeOps.cpp` (`tt2ForceEnvelope` prologue + `TT2_E_FIELD_OP` macro + `opEnvT`, wired next to the `MO.*` block). `LFO.*` mirrors this exactly, swapping the shape lock to `Sine`/`Run` and adding the two rate ops. Test fixture to mirror: `src/tests/unit/sequencer/TestTeletypeV2EnvelopeOps.cpp` (the `ModStubHost` + `evalText` harness).
- **Modulator rate domains (`Modulator.h`, `ModulatorEngine.h`).** `RateDomain::Free` — phase advances by real `dt × Hz`; `rate` is **centi-Hz**, clamped 1–1600 (0.01–16 Hz). `RateDomain::Tempo` — phase advances per PPQN tick, one cycle = `rate × 2` ticks (`inc = 65536/(rate×2)`), clamped 6–6144. `setRateDomain()` re-clamps the raw `rate` into the new domain's range, so **domain must be set before rate**.
- **Clock divisor math.** `CONFIG_PPQN = 192` (`Config.h`). A "1/d note per cycle" cycle is `(PPQN×4)/d` ticks; `rate = cycle/2 = PPQN×2/d = 384/d`. So `d=1`→384 (whole note), `d=4`→96 (1/4), `d=8`→48 (1/8), `d=16`→24 (1/16).
- **Field accessors.** Modulator setters/getters: `setShape`/`shape`, `setMode`/`mode`, `setRateDomain`, `setRate`/`rate` (domain-clamped), `setDepth`/`depth` (0–127), `setOffset`/`offset` (−64…+63). All clamp in-setter.
- **Modulator slot indexing.** `popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)` — 1-based input → 0-based, rejects <1 / >8 with `OutOfRange`. Same as `MO.*` and `E.*`.
- **Existing LFO tokens.** `op_enum.h` has `E_OP_LFO_R/W/A/F/O/S`; `match_token.rl:247+` matches `"LFO.R"` etc.; `TT2OpNames.cpp:224-229` names them. All dispatch slots are `nullptr` today → `UnsupportedOp` (`TT2Evaluator.h:113`). There is **no** `E_OP_LFO_C`.
- **Parser regen.** `teletype/module/Makefile` shows the Ragel rule: `ragel -C -G2 src/match_token.rl -o src/match_token.c`. The generated `.c` is committed and compiled by the build.
- **Origin context.** `docs/brainstorms/2026-06-14-tt2-mod-geode-ops-requirements.md` spec'd `MO.*` and deliberately had no native `E.*`/`LFO.*`. This plan layers `LFO.*` back as opinionated sugar on the same modulator engine — consistent with the `E.*` decision already shipped.

---

## Key technical decisions

- **Index `n` = modulator slot 1–8** (mirrors `MO.*`/`E.*`).
- **Family lock on every call:** `Shape::Sine` + `Mode::Run`. The op family selects the shape, so no `MO.SHAPE`/`MO.MODE` needed for a free-running LFO. Opinionated: sharing a slot with `MO.*`/`E.*` is last-writer-wins.
- **Two rate ops, one per domain** — the modulator carries both, and they are musically distinct (free-running vs tempo-synced). Each forces its `RateDomain` (domain set before rate, so the setter clamps correctly):

  | op | domain | value → write | range |
  |---|---|---|---|
  | `LFO.R n x` | `Free` | `rate = x` (centi-Hz) | clamp 1–1600 = 0.01–16 Hz; `100` = 1 Hz |
  | `LFO.C n d` | `Tempo` | `rate = 384/d` (`PPQN×2/d`) | clamp 6–6144; `16` = 1/16, `4` = 1/4, `1` = whole note |
  | `LFO.A n x` | — | `depth = x` | 0–127 |
  | `LFO.O n x` | — | `offset = x` | −64…+63 |

- **Centi-Hz, not milli-Hz**, for `LFO.R`: it matches the modulator's actual 0.01 Hz resolution 1:1 — milli-Hz would accept precision the engine quantizes away.
- **`LFO.C` divisor guard:** clamp `d` to ≥1 before dividing (avoid div-by-zero); the resulting `rate` then clamps to the Tempo range (so very large `d` floors at 6).
- **`LFO.A`/`LFO.O` do not touch rate or domain** — only the common `Sine`/`Run` lock + their field. Rate persists from whatever `LFO.R`/`LFO.C` last set.
- **Reads (no value) push the underlying field** (`rate`/`depth`/`offset`) and do not apply the lock — mirrors the `MO.*`/`E.*` get path. No bare `LFO` op (none exists; not added).
- **Deferred:** `LFO.W` (wave morph), `LFO.F` (wavefold), `LFO.S` (start/stop) — features the modulator lacks or that need a separate decision.

---

## Scope Boundaries

**In scope:** the new `LFO.C` parser token (vendored teletype enum + grammar + regen + name); dispatch handlers + wiring for `LFO.R`/`LFO.C`/`LFO.A`/`LFO.O` with the `Sine`/`Run` family lock and the rate-domain math; headless tests.

**Out of scope / deferred (later):**
- `LFO.W`, `LFO.F`, `LFO.S`.
- Any model field, UI page, or serialization change.
- Multi-bar `LFO.C` cycles (`d<1`) — the note-divisor encoding only reaches whole-note; longer cycles would need a different encoding.

---

## System-wide impact

- **Vendored teletype parser** (`teletype/src/`): adding `E_OP_LFO_C` shifts the op enum and regenerates `match_token.c`. The enum is consumed by the name table and dispatch table sizing (`E_OP__LENGTH`). Regen must be done with the same `ragel -C -G2` invocation the Makefile uses so the committed `.c` stays consistent. Verify the full TT2 suite still parses/prints all ops after regen (enum-ordinal drift is the risk).
- **Dispatch table** (`TeletypeNativeOps.cpp`): five new wired slots (four `LFO.*` + the table already sized by `E_OP__LENGTH`).

---

## Implementation Units

### U1. `LFO.C` parser token (vendored teletype + regen)

**Goal:** Make `LFO.C` a recognized op token so it parses and lowers, ahead of wiring its behavior.

**Requirements:** Enables the clocked-rate op `LFO.C` (Key Decisions rate table).

**Dependencies:** none.

**Files:**
- Modify: `teletype/src/ops/op_enum.h` — add `E_OP_LFO_C` (next to the other `E_OP_LFO_*`).
- Modify: `teletype/src/match_token.rl` — add the `"LFO.C" => { MATCH_OP(E_OP_LFO_C); };` rule alongside the existing `LFO.*` matches.
- Regenerate: `teletype/src/match_token.c` via `ragel -C -G2 teletype/src/match_token.rl -o teletype/src/match_token.c` (the Makefile's invocation).
- Modify: `src/apps/sequencer/engine/TT2OpNames.cpp` — return `"LFO.C"` for `E_OP_LFO_C`.
- Test: `src/tests/unit/sequencer/TestTeletypeV2EnvelopeOps.cpp` (add a parse CASE) — or a small dedicated `TestTeletypeV2LfoOps.cpp` created in U2; keep the U1 parse check wherever U2's fixture will live.

**Approach:** Place `E_OP_LFO_C` adjacent to the existing `E_OP_LFO_*` enumerators to keep the family grouped. Add the grammar rule next to the other `LFO.*` tokens, then regenerate the state machine — do **not** hand-edit `match_token.c`. Confirm the regenerated file differs only by the new token's states (no unrelated churn) before committing.

**Patterns to follow:** the existing `LFO.R`/`E.A` token definitions in `op_enum.h` + `match_token.rl` + `TT2OpNames.cpp` (three spots, same shape).

**Execution note:** Test-first — a parse CASE that fails because `LFO.C` is unknown, then passes after the token exists.

**Test scenarios:**
- **Happy path — token parses + lowers:** `parse("LFO.C 1 16")` returns `E_OK`; `lowerCommand` produces a command whose op is `E_OP_LFO_C`.
- **Pre-dispatch state (RED→carry into U2):** evaluating `LFO.C 1 16` before U2 wires the handler returns `TT2EvalError::UnsupportedOp` (proves the token parses but isn't yet behavior-wired).
- **Regen integrity:** the full `TestTeletypeV2*` suite still parses/prints every existing op after regen (guards against enum-ordinal drift breaking other tokens).

**Verification:** `LFO.C` parses and names correctly; regenerated `match_token.c` compiles; existing TT2 op tests unaffected.

---

### U2. `LFO.*` dispatch handlers + wiring

**Goal:** Make `LFO.R`/`LFO.C`/`LFO.A`/`LFO.O` run as Sine-locked, free-running modulator aliases instead of `UnsupportedOp`.

**Requirements:** Delivers the `LFO.*` clean set (Key Decisions rate table).

**Dependencies:** U1 (for the `LFO.C` token).

**Files:**
- Modify: `src/apps/sequencer/engine/TeletypeNativeOps.cpp` — add handlers + table wiring next to the `E.*` block.
- Test: `src/tests/unit/sequencer/TestTeletypeV2LfoOps.cpp` (new; mirror `TestTeletypeV2EnvelopeOps.cpp`'s `ModStubHost`/`evalText`).
- Modify: `src/tests/unit/sequencer/CMakeLists.txt` — register the test.

**Approach:**
- Add a `tt2ForceLfo(Modulator&)` prologue: `setShape(Sine)`, `setMode(Run)`. Mirror `tt2ForceEnvelope`.
- `LFO.A`/`LFO.O`: reuse the `E.*` field-op shape (prologue + `paramSet`/setter on set; getter push on read) but with the LFO prologue. `LFO.A`→`Depth`, `LFO.O`→`Offset`.
- `LFO.R`: prologue, then `setRateDomain(Free)` **before** `setRate(x)` (so the setter clamps to 1–1600). Read pushes `rate`.
- `LFO.C`: prologue, then `setRateDomain(Tempo)`, then `setRate(384 / max(d,1))` (the setter clamps to 6–6144). Read pushes raw `rate`.
- Null-host guard exactly as the `MO.*`/`E.*` handlers (`tt2ActiveHost()` may be null).
- Wire `E_OP_LFO_R`, `E_OP_LFO_C`, `E_OP_LFO_A`, `E_OP_LFO_O` in `opTableBuilder`.

**Execution note:** Test-first — write the failing CASEs (ops return `UnsupportedOp` for the dispatch-only ones; `LFO.C` already parses from U1) before wiring the table.

**Patterns to follow:** `tt2ForceEnvelope` + `TT2_E_FIELD_OP` + the `E.*` table-wiring block in `TeletypeNativeOps.cpp`; `popOutputIndex(…, CONFIG_MODULATOR_COUNT)`.

**Test scenarios** (mirror `TestTeletypeV2EnvelopeOps.cpp`; run real `parse → lowerCommand → evaluateCommand`):
- **Happy path — free rate:** `LFO.R 1 100` → slot 1 `rateDomain == Free`, `rate == 100`, `shape == Sine`, `mode == Run`.
- **Happy path — clocked rate:** `LFO.C 1 16` → `rateDomain == Tempo`, `rate == 24`; `LFO.C 1 4` → `rate == 96`; `LFO.C 1 1` → `rate == 384`.
- **Happy path — depth/offset:** `LFO.A 1 100` → `depth == 100`; `LFO.O 1 -10` → `offset == -10`.
- **Family lock:** preset slot 2 to `Shape::ADSR`/`Mode::Trig`, then `LFO.A 2 50` → `shape == Sine`, `mode == Run`, `depth == 50`.
- **Domain switch on a slot:** `LFO.R 3 200` then `LFO.C 3 8` → ends `rateDomain == Tempo`, `rate == 48` (later op wins).
- **Edge — clamping:** `LFO.R 1 9999` → `rate == 1600`; `LFO.R 1 0` → `rate == 1`; `LFO.C 1 0` → `d` floored to 1 → `rate == 384`; `LFO.C 1 100` → `384/100 = 3` → clamped to `6` (Tempo min); `LFO.A 1 999` → `depth == 127`; `LFO.O 1 999` → `offset == 63`.
- **Read path:** `LFO.A 1 70` then `LFO.A 1` (no value) pushes `70`; read does not change shape/mode.
- **Error — out-of-range slot:** `LFO.R 0 100` and `LFO.R 9 100` → `TT2EvalError::OutOfRange`, no slot mutated.

**Verification:** New `TestTeletypeV2LfoOps` passes; full `TestTeletypeV2*` suite green; sim + STM32 release link clean; RAM flat (dispatch-only, no model state). A script using `LFO.R`/`LFO.C` + `LFO.A`/`LFO.O` produces a free-running sine LFO on slot `n` (free or tempo-synced) with no `MO.*` calls.

---

## Verification

- `LFO.C` parses, names, and (after U2) runs; regen leaves other ops intact.
- New LFO CASEs pass — free/clocked rate, depth/offset, family lock, domain switch, clamping, read path, out-of-range.
- Full `TestTeletypeV2*` suite green (`ctest --test-dir build -R "TeletypeV2|TT2|Modulator"`).
- Sim (`build/sim/debug`) + STM32 release (`build/stm32/release`) link clean; `.data`/`.bss`/`.ccmram_bss` flat.
- Posture: test-first; U1 parse-first, U2 behavior test-first.

## Rollback

Two units, two commits. U2 revert restores `nullptr` LFO dispatch slots (ops → `UnsupportedOp`). U1 revert removes the `LFO.C` token (re-regen or revert the committed `match_token.c` + enum + name). Revert in reverse; the token set returns to today's state.
