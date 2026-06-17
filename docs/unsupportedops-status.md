# TT2 Unsupported Ops — Status & Proposed Functionality

Op tokens that **parse and lower** but whose native dispatch slot in
`tt2NativeOpTable` is `nullptr`, so evaluating one returns `TT2EvalError::UnsupportedOp`
at runtime (`TT2Evaluator.h`). They are inert, not parse errors.

Recomputed 2026-06-17 as (op enum) − (wired dispatch entries): **30 unsupported**
(down from 38 — 8 shipped, see below). The fork's `op_enum.h` carries no
i2c / TELEXo / Just Friends / Disting ops, so this is the *complete* set — all
Performer-relevant.

Source of truth: `teletype/src/ops/op.c` (`tele_ops[]` → generates `op_enum.h`);
dispatch wiring in `src/apps/sequencer/engine/TeletypeNativeOps.cpp`.

---

## Shipped (no longer unsupported)

Wired this session — moved out of the buckets below:

| op(s) | what it does now |
|---|---|
| `<<` / `>>` | aliases of `LSH` / `RSH` (bit shift) |
| `PRM` | alias of `PARAM` (read the PARAM knob) |
| `Q.2P` / `Q.P2` | copy queue ↔ pattern (0-based index, current = `p_n`) |
| `Q.RND` | random queue element / fill-random |
| `PRINT` / `PRT` | 16-slot dashboard value store in `TT2Runtime` (get/set) — runs in live mode via the existing history + result lines |

Everything below is **deferred or out-of-scope** (decided 2026-06-17): no further
op-gap wiring planned for now.

---

## Deferred — dispatch-only, one decision each

Implementable as `E.*`/`LFO.*`-style aliases (no model/engine change), each needs
one semantic call. Deferred.

| op | legacy meaning | proposed |
|---|---|---|
| `LFO.W n x` | wave morph 0–100 | map `x` → shape-select across the 5 periodic shapes (Sine/Tri/SawUp/SawDown/Square) |
| `LFO.S n x` | start/stop | `x≤0` stops via depth 0 (or Mode juggle); a true gate wants a modulator enable flag |
| `E.L n k` | loop k cycles | `k≤0` → `Mode::Run` (free loop), else `Mode::Trig`; exact-k count needs a new modulator field |

---

## Deferred — need a new `ModulatorEngine` feature

The modulator engine lacks the capability; can't be pure aliases. Deferred.

| op | legacy meaning | blocker |
|---|---|---|
| `E.R n tr` | pulse TR at end of envelope rise | no phase-event → gate hook |
| `E.C n tr` | pulse TR at end of cycle | same phase-event hook |
| `LFO.F n x` | wavefold threshold | no wavefolder / fold stage on the modulator output |

---

## Deferred — scene bank / quad

TT2 is single-program; multi-scene load/save is the supertrack quad effort
(`docs/plans/2026-06-14-004-feat-tt2-supertrack-design.md`).

| op | legacy meaning |
|---|---|
| `SCENE x` | load scene x |
| `SCENE.G` | load scene without scripts |
| `SCENE.P` | preload scene |
| `INIT.SCENE` | re-init current scene |

---

## Out of scope by design

### Geode routing
No live `GeodeConfig` field backs these; Geode output routing is owned by Performer.

| op | meaning |
|---|---|
| `G.O` / `G.BAR` / `G.R` / `G.B` | Geode output / bar / mix-route / B routing |

### Live-terminal sub-screens
Teletype "live mode" is a USB-terminal surface with sub-screens Performer doesn't have.

| op | meaning |
|---|---|
| `LIVE.OFF` / `LIVE.O` | suppress live output |
| `LIVE.DASH` / `LIVE.D` | live dashboard toggle |
| `LIVE.GRID` / `LIVE.G` | live grid view |
| `LIVE.VARS` / `LIVE.V` | print vars to live |

### Calibration
Input/output calibration is owned by Performer's own calibration system/UI, not scripts.

| op | meaning |
|---|---|
| `CV.CAL` / `CV.CAL.RESET` | CV output calibration |
| `IN.CAL.MIN` / `IN.CAL.MAX` / `IN.CAL.RESET` | CV-in calibration |
| `PARAM.CAL.MIN` / `PARAM.CAL.MAX` / `PARAM.CAL.RESET` | PARAM-knob calibration |

---

## Summary

| bucket | count | status |
|---|---|---|
| Shipped (`<< >> PRM`, `Q.2P/P2/RND`, `PRINT/PRT`) | 8 | done |
| Modulator alias, dispatch-only (`LFO.W/S`, `E.L`) | 3 | deferred |
| Modulator alias, new engine feature (`E.R/E.C`, `LFO.F`) | 3 | deferred |
| Scene (`SCENE*`, `INIT.SCENE`) | 4 | deferred (scene bank) |
| Geode routing (`G.O/BAR/R/B`) | 4 | by-design out |
| Live sub-screens (`LIVE.*`) | 8 | out of scope (no surface) |
| Calibration (`CV/IN/PARAM.CAL*`) | 8 | out of scope (Performer-owned) |

**30 unsupported remain, all deferred or out-of-scope.** No active op-gap work
queued; the next parity front is structural (10 scripts + 64-deep delay,
`docs/plans/2026-06-15-tt2-hw-parity.md`).
