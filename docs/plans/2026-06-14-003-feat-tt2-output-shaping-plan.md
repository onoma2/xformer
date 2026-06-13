---
id: feat-tt2-output-shaping
schema: plan
title: "feat: TT2 output shaping (CV slew + TR pulse)"
type: feat
status: completed
date: 2026-06-14
depth: standard
---

# feat: TT2 output shaping (CV slew + TR pulse)

## Summary

Make native TT2 outputs *shaped* instead of instant-snap: per-CV **slew** (ramp
toward target over a set time) + **offset**, and per-TR **timed pulses** with
rest **polarity**. A per-ms shaping pass in `TT2TrackEngine::update(dt)` advances
CV ramps and ends TR pulses; the ops wire onto the `TT2Variables` storage that
already exists (`cv_slew[8]`, `cv_off[8]`, `tr_pol[8]`, `tr_time[8]`). This is the
second engine mechanic in the `teletype_v2.md` roadmap (after trigger-input
firing) — it's what makes a TT2 script *sound* musical rather than steppy.

---

## Scope Boundaries

**In scope:** native CV slew engine + offset; TR pulse timers + polarity; the ops `CV.SLEW`, `CV.OFF`, `TR.POL`, `TR.TIME`, `TR.PULSE` / `TR.P`, `TR.TOG`; extending the existing `CV`/`TR` writes to seed ramps / honor polarity; `cvOutput()` returning the ramped value.

### Deferred to Follow-Up Work
- **IN / PARAM inputs** — separate roadmap item.
- **CV.CAL / calibration** — per-channel output calibration.
- **The editor I/O grid** — output-dest routing UI.
- **Width/divisor pulse variants** (`TR.W`, `TR.D`) and metro-relative pulse width — the legacy engine has these; first cut does fixed-ms pulses (`TR.TIME`).

---

## Key Technical Decisions

- **Native linear ms ramp — NOT Performer `Slide`.** Upstream `tele_cv` is a *linear* interpolation over T ms; the legacy `TeletypeTrackEngine` reuses Performer's *exponential* `Slide` with a `sqrt(ms)` fudge (and even comments the mismatch). Per the decided "native" direction, TT2 owns a linear ramp in `TT2OutputState`: seed a constant per-ms step from `(target − current) / slewMs` and accumulate until settled. Sub-LSB precision via fixed-point current (e.g. `raw << 8`) so slow ramps don't stall on integer truncation.
- **Slew time + offset come from `TT2Variables`.** `CV.SLEW n ms` sets `cv_slew[n]`; `CV.OFF n v` sets `cv_off[n]`. A `CV n v` write computes target = `v + cv_off[n]`, and if `cv_slew[n] > 0` seeds the ramp, else snaps. Offset applied at target-compute (upstream semantics).
- **`cvOutput()` returns the ramped *current*, not the target.** Today it returns `targetRaw`; with slew it must return the in-flight value. A channel mid-ramp stays "active" so it keeps emitting (revisit the `cvDirty`-gated early-return in `cvOutput`).
- **TR pulse = one-shot ms timer.** `TR.P`/`TR.PULSE n` sets the channel to its active level (per `tr_pol[n]`), arms `pulseRemainingMs = tr_time[n]`; the per-ms pass decrements and, on expiry, restores the rest level. `TR.TIME n ms` sets `tr_time[n]`; `TR.POL n p` sets rest polarity; `TR.TOG n` flips; plain `TR n v` sets level directly. Mirrors upstream `tele_tr_pulse` / legacy `updatePulses`.
- **Shaping advances by whole-ms in `update(dt)`** alongside the existing delay/metro/trigger passes (TT2 already accumulates `whole` ms there).

---

## High-Level Technical Design

Directional guidance for review, not implementation spec.

```
TT2OutputState (extended)
  per CV: targetRaw, currentQ (raw<<8 fixed), stepQ (per-ms delta), remainingMs
  per TR: level, restLevel, pulseRemainingMs

CV n v        -> target = v + cv_off[n]; if cv_slew[n]>0 seed ramp (stepQ, remainingMs) else snap currentQ=target
per-ms pass   -> currentQ += stepQ*ms; remainingMs -= ms; if settled currentQ = targetQ
cvOutput(n)   -> rawToVolts(currentQ >> 8)

TR.P n        -> level = active(tr_pol[n]); pulseRemainingMs = tr_time[n]
per-ms pass   -> pulseRemainingMs -= ms; if expired level = rest(tr_pol[n])
```

---

## Implementation Units

### U1. Extend TT2OutputState + pure shaping-advance helpers

**Goal:** Output-state fields for the ramp + pulse, and the pure per-ms advance functions (the testable core).
**Dependencies:** none.
**Files:** `src/apps/sequencer/engine/TeletypeOutputState.h`, `src/tests/unit/sequencer/TestTeletypeV2OutputShaping.cpp` (new), `src/tests/unit/sequencer/CMakeLists.txt`.
**Approach:** Add to `TT2CvOutput`: `currentQ` (int32 fixed `raw<<8`), `stepQ` (int32 per-ms), `remainingMs` (int16). Add to `TT2TrOutput`: `restLevel`, `pulseRemainingMs` (int16). `init` settles current=target=0, rest=0. Add free helpers: `tt2SeedCvSlew(cv, targetRaw, slewMs)` (compute stepQ/remainingMs or snap), `tt2AdvanceCvSlew(cv, ms)` (accumulate, clamp, settle), `tt2ArmTrPulse(tr, restPol)`/`tt2AdvanceTrPulse(tr, ms)`. Bump the three size asserts.
**Execution note:** Test-first on the advance helpers.
**Patterns to follow:** upstream `tele_cv` linear-ramp math; legacy `updatePulses` decrement-then-restore (`TeletypeTrackEngine.cpp` ~1168).
**Test scenarios:**
- Seed slew 0→16383 over 100ms: after 50ms current ≈ half-scale (linear); after ≥100ms current == target and `remainingMs==0`.
- `slewMs==0` seed snaps current to target immediately.
- Negative ramp (high→low) interpolates downward and settles.
- Sub-LSB: a slow ramp (e.g. 10 LSB over 10000ms) makes monotonic progress per ms, doesn't stall at 0.
- Pulse: arm with `tr_time`; after < time, level still active; after ≥ time, level restored to rest; advancing an unarmed channel is a no-op.

### U2. Per-ms shaping pass in TT2TrackEngine + ramped output read

**Goal:** Drive the helpers each update and emit the ramped/pulsed values.
**Dependencies:** U1.
**Files:** `src/apps/sequencer/engine/TT2TrackEngine.h`.
**Approach:** In `update(dt)`, after the existing `whole`-ms accumulation, advance every CV ramp and TR pulse by `whole` ms (alongside delays/metro). `cvOutput(i)` returns `rawToVolts(currentQ >> 8)` and must keep emitting while a ramp is active (adjust the `cvDirty` early-return so mid-ramp channels aren't zeroed). `gateOutput(i)` returns the current (possibly pulsed) level.
**Patterns to follow:** the existing `update(dt)` ms loop (delays/metro/trigger sampling); legacy `cvOutput`/pulse readback.
**Test scenarios:** `Test expectation: none (engine integration)` — behavior is covered by U1 helper tests + on-device. Integration check (sim/device): a slewed `CV` glides instead of snapping; a `TR.P` makes a gate of the set length.

### U3. Output-shaping ops

**Goal:** Wire the op surface onto the new state + variable storage.
**Dependencies:** U1, U2.
**Files:** `src/apps/sequencer/engine/TeletypeNativeOps.cpp`, `src/tests/unit/sequencer/TestTeletypeV2OutputShaping.cpp`.
**Approach:** `CV.SLEW n ms` → `cv_slew[n]`; `CV.OFF n v` → `cv_off[n]`; extend `opCv` set so target = `v + cv_off[n]` and it calls `tt2SeedCvSlew` with `cv_slew[n]`. `TR.TIME n ms` → `tr_time[n]`; `TR.POL n p` → `tr_pol[n]` (+ update rest level); `TR.PULSE`/`TR.P n` → arm pulse at `tr_time[n]`; `TR.TOG n` → flip level. Register all in the op table (these enum ids are in `op_enum`; confirm `TR.P` vs `TR.PULSE` aliasing as ADD/`+` did). Get/set arity per the existing op pattern.
**Patterns to follow:** existing `opCv`/`opTr` in `TeletypeNativeOps.cpp`; the Q/pattern op get/set dispatch.
**Test scenarios:**
- `CV.SLEW 1 100` then `CV 1 16383`: output state ramps (remainingMs seeded ~100), not instant.
- `CV.OFF 1 1000` then `CV 1 5000`: target == 6000 (offset applied).
- `TR.TIME 1 50` then `TR.P 1`: level active, pulse armed at 50.
- `TR.POL 1 0` vs `1`: rest level follows polarity; `TR.TOG` flips the current level.
- Get forms read back the configured values (`CV.SLEW 1` → set value, etc.).
- Symbol/alias: `TR.P` and `TR.PULSE` hit the same handler.

### U4. Verify + roadmap flip

**Goal:** Confirm end-to-end and update the roadmap.
**Dependencies:** U1, U2, U3.
**Files:** `docs/teletype_v2.md` (output-shaping → Done), full `TestTeletypeV2*` suite + STM32 release.
**Approach:** Suite + STM32 green; sim/device: slewed CV glides, `TR.P` gates at the set width. Update Current State (output shaping done; remaining engine mechanic = IN/PARAM).
**Test scenarios:** `Test expectation: none (verification + docs)`.

---

## System-Wide Impact

- `TT2OutputState` grows (ramp + pulse fields per channel) — bump its size asserts; it lives inside `TT2TrackEngine` (watch `sizeof(TT2TrackEngine) <= 944`) and is **not** serialized, so no model/save impact.
- `cvOutput()` semantics change (returns ramped current, not target) — verify nothing else assumed `targetRaw` was the live output.
- No op-dispatch/runtime changes beyond the new handlers; no flash concern post-`-Os`.

---

## Risks & Mitigations

- **`cvDirty` gating zeroing a mid-ramp channel** — `cvOutput` returns 0 when not dirty; a ramp must keep the channel live. Mitigation: U2 explicitly revisits the dirty early-return; test a multi-ms ramp emits non-zero throughout.
- **Fixed-point ramp truncation** stalling slow slews. Mitigation: `raw<<8` precision + the sub-LSB test scenario.
- **`update(dt)` advancing by `whole` ms** loses sub-ms ramp resolution. Mitigation: the existing `_msAccum` remainder already carries sub-ms; ramp granularity of 1ms is musically fine (matches upstream RATE_CV-ish cadence) — note, don't over-engineer.
- **Size asserts** (`TT2OutputState`, `TT2TrackEngine`) trip. Mitigation: bump; confirm STM32 links.

---

## Verification

- U1/U3 unit tests green (ramp seed/advance/settle, sub-LSB, pulse arm/expire; ops set/seed/readback).
- Full `TestTeletypeV2*` suite + STM32 release green.
- Sim/device: a `CV.SLEW`+`CV` glides audibly; `TR.P` emits a gate of `TR.TIME` length; offset/polarity behave.
- `docs/teletype_v2.md` Current State updated (output shaping → Done).
- Execution posture: U1 helpers test-first.
