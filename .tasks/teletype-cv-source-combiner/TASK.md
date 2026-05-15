# teletype-cv-source-combiner

## Goal
Replace accidental last-writer-wins behavior in Teletype CV output generation with an explicit raw/value-domain source pipeline. `CV`, `E.*`, `LFO.*`, and Geode should feed per-output source state, then one render pass should choose or combine raw `0..16383` values before the existing Performer voltage/range/offset/quantize output path. This is a semantics/architecture cleanup, not a RAM recovery task.

## Status
Paused. First implementation plan captured, but no code started.

## Key files
- `src/apps/sequencer/engine/TeletypeTrackEngine.h` — current flat arrays for CV, envelope, LFO, and Geode routing state.
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp` — current update order and `handleCv()` raw-to-output conversion path.
- `src/apps/sequencer/engine/TeletypeBridge.cpp` — Teletype callback boundary for `tele_cv`, `tele_env_*`, `tele_lfo_*`, and Geode callbacks.
- `teletype/src/ops/hardware.c` — existing Teletype op surface and numeric argument normalization.
- `.tasks/teletype-cv-source-combiner/plan.md` — staged implementation plan.

## Current source facts
- Teletype CV values are raw `0..16383`, mapped by Performer as `0 -> -5V`, `8192 -> ~0V`, `16383 -> +5V` in the base Bipolar5V domain before configured output range conversion.
- `E.*` currently uses the same raw CV domain, not a Telex-style VCA/gain domain.
- `E.T n` currently enables interpolation, jumps to `E.O` raw value, then slews to `E` raw target.
- `updateEnvelopes()`, `updateLfos()`, and `updateGeode()` can all call `handleCv()`; because update order is envelope, LFO, Geode, the current behavior is accidental source competition.
- Geode can produce numeric/control values, not only voltage-like CV. The first implementation must therefore operate in raw/value domain, not actual-output-voltage domain.

## Decisions log
- 2026-05-15: Do not use Telex VCA semantics. `E.*` stays a raw CV envelope (`0..16383`) rather than a normalized gain multiplier.
- 2026-05-15: First implementation uses a raw/value-domain law. Voltage-domain mixing and user-facing Teletype ops are deferred.
- 2026-05-15: User-facing op definition is intentionally deferred to the latest stage so architecture can settle before adding script surface area.
- 2026-05-15: Picked V0 source law: explicit priority mux `ENV active > GEODE routed/active > LFO active > CV held value`. This replaces update-order dominance without introducing new ops.
- 2026-05-15: Picked future combiner math law for later internal expansion: all sources normalize around raw center `8192`; `ADD/SUB` use centered bipolar math with clamp to `0..16383`; logic modes threshold raw values at `8192` by default.

## Open questions
- [ ] Should Geode routed-but-idle hold last raw value or relinquish ownership to LFO/CV?
- [ ] Should `CV` / `CV.SET` interrupt an active LFO or only update the held CV source for later use?
- [ ] Should Env completion return to the previously active source or always recompute by current priority law?
- [ ] Which user-facing op names and numeric IDs should expose source selection/combiner modes? Deferred until final stage.

## Completed steps
- [x] Source behavior inspected for `E.*`, raw-to-voltage mapping, LFO writes, and Geode writes.
- [x] Telex VCA model rejected for Performer Teletype.
- [x] First implementation scope narrowed to raw/value-domain source arbitration.
- [x] Task folder and initial plan created.

## Notes
- This task should not be sold as compatibility work or RAM work.
- Avoid introducing a string/parser surface for source names in the first implementation.
- Keep existing Teletype callback names and public C bridge functions during the first implementation; refactor internal ownership only.
