# modulator-enhancements

## Goal
Rethink the modulator rate model so every shape runs on real wall-time, with an explicit
Free(Hz) / Tempo(clock-division) domain, and simplify the gate modes. Settled in design
conversation 2026-06-09 (grounded in the actual ModulatorEngine + the 5 parked shapers).

A modulator is **output-only**: shape generator → depth → (later: shaper) → 0–127 out.
There is no input concept. The earlier "input-reading modulators" framing was disproved —
an analyzer's input is the modulator's own post-depth signal (the modulator IS the input).

## Rate domain (the core change)
Today `_rate` is overloaded per shape (LFO = PPQN ticks, Chaos = Hz×10, hardcoded `dt`).
Replace with one explicit axis applied to ALL shapes:

- **Free** — rate in **wall-Hz**, tempo-independent. `phase += dt_seconds * rateHz`. Runs even
  when the transport is stopped (the engine update loop already computes `dt` continuously).
- **Tempo** — rate as a **clock division**, tempo-locked (existing PPQN behavior). Freezes when
  the transport stops — that's what sync means.

- **Default Free @ 0.05 Hz**, all modulators start Free.
- Toggle: **re-press the RATE F-key** (F2 when RATE already selected; encoder-press is taken by
  `editRate`'s coarse/fine modifier — confirmed in code).
- Free range: ~0.01–16 Hz (centi-Hz, low by design — modulation targets rarely move faster
  than a couple Hz; bursts covered, 50 Hz was overkill). Tempo: existing division table.

## Encoding (no version bump — policy: ProjectVersion.h)
- New `_rateDomain` (Free/Tempo), serialized **unconditionally**. No version enum, no read-guard.
- Reuse `_rate` (uint16_t), interpreted by domain: Free = centi-Hz (1..1600), Tempo = ticks (6..6144).
- `setRate`/clamp/sanitize and `editRate`/`printRate` become **domain-keyed** (not shape-keyed).
- Dev project files break across branches — already the accepted trade.

## Gate modes (simplify 4 → 3)
Gate behavior is orthogonal to the rate domain. `Mode`: Run / Trig / Gate.
- **Run** = free-running, gate ignored (was Free).
- **Trig** = reset phase to the offset on gate rising (Sync = Trig with offset 0 → merged).
- **Gate** = advance only while gate high, reset on rising (was Hold). Retrigger folds into Trig.
- Enum ordinals Run=0/Trig=1/Gate=2.

## Slices
1. **Model** (TDD) — `RateDomain` enum + `_rateDomain` field; domain-keyed rate
   accessors/clamp/print/edit; `Mode` → Run/Trig/Gate; `clear()` defaults Free @ centi-Hz 5.
   Serialize `_rateDomain` unconditionally. New `TestModulator` unit.
2. **Engine** — `ModulatorEngine::tick` gains `float dt`; Engine.cpp passes the dt it already
   computes (`Engine.cpp:74`). Free: `phase += dt*Hz` for every shape (chaos drops hardcoded
   `dt`/÷10). Tempo: existing PPQN; chaos-in-Tempo derives Hz from tempo×division. Gate modes
   Run/Trig/Gate in the phase-reset logic.
3. **UI** (`ModulatorPage`) — re-press RATE toggles domain; `printRate` domain-aware (model);
   mode names Run/Trig/Gate; value column nudged to x≥132 (clear of the level bar at 124..128).
   Renders: `ui-preview/modulator/modulator-rate-free-proposed.png`, `-rate-tempo-`, `-gatemodes-`.

Gate: sim build + run, STM32 release green, ui-preview before the UI slice. Commit when asked.

## Follow-on (NOT this build) — post-depth shaper slot
Insert a shaper stage between depth and the 0–127 quantize (in float): hosts the stateless
folds (None/Fold/Crease + off-center) AND the 5 parked stateful analyzers
(`.tasks/modulator-enhancements/parked-stateful-shapers.cpp`: Location/Envelope/FreqFollower/
Activity/ProgDiv). Each reads the modulator's own post-depth signal — drop-in (`apply(src01,
state&)→01`), per-modulator state arrays like `_adsrState[]`. Their hardcoded "~1 Hz @ 1 kHz"
constants must be re-derived against the now-explicit wall-time dt — which is exactly why this
follows slice 2. Musically richest on Chaos/Random/rate-modulated sources (the analyzer finds
macro-structure in unpredictable wiggle); Location/ProgDiv work on any shape.

## Key files
- `model/Modulator.h` + `model/Modulator.cpp` (rate/mode/serialize; `editRate`/`printRate`).
- `engine/ModulatorEngine.h` (tick + phase advance); `engine/Engine.cpp:74,195` (dt + tick call).
- `ui/pages/ModulatorPage.{h,cpp}`; `ui-preview/pages_modulator.py`.
- `engine/WallClock.h` (substrate, present). Design doc `docs/plans/2026-05-29-wallclock-time-architecture-design.md`.

## Reference
Governing scope note: the routing overhaul plan (`002`, R10) walled modulator expansion out of
scope — so this is net-new, unspecced-until-now work; this file is its spec.
