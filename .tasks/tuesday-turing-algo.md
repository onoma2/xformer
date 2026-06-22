# tuesday-turing-algo

## Goal
Add a Music-Thing-style Turing machine (looping shift register, per-step bit-flip probability) as
the 16th Tuesday algorithm — deterministic on preview/apply, authentically live-evolving on the
live track. No new TrackMode, no model-storage change.

## Key files
- `src/apps/sequencer/engine/generators/TuringRegister.h` — shared shift-register core (port of o_c util_turing)
- `src/tests/unit/sequencer/TestTuringRegister.cpp` — core test (test-first)
- `src/apps/sequencer/engine/generators/TuesdayAlgoCore.{h,cpp}` — algo 15 deterministic (preview/apply)
- `src/apps/sequencer/engine/TuesdayTrackEngine.{h,cpp}` — algo 15 live drift (`_turingSR`, `generateTuring`)
- `src/apps/sequencer/model/TuesdaySequence.{h,cpp}`, `ui/pages/TuesdayEditPage.cpp` — range 0-15, name, randomize
- `docs/plans/2026-06-22-002-feat-tuesday-turing-algo-plan.md` (completed)

## Decisions log
- 2026-06-22: Shipped via /ce-plan → /ce-work (tdd: TuringRegister test-first; engine/UI build+manual, no harness). Studied both references: ported **o_c `util_turing.h`** (the real Turing machine); rejected performer-nx's `TuringAlgorithm` (it's a mislabeled 8-bit cellular automaton, not a Turing machine). Deterministic-vs-live is **RNG sourcing, not a toggle**: generator clocks with the seeded RNG (reproducible), live track clocks a persistent `_turingSR` with the running RNG (drifts); `power=0` = pure rotation = frozen loop. No `AlgoParams` field, no model growth — existing projects unaffected (index 15 newly allowed, 0-14 unchanged). Control map: seed→fill, flow→length, power→flip prob (0-16→0-255), ornament→pitch window, glide/gateLength reused. Commits `8a42c4b1`/`8e9b053c`/`f9f87512`/`e7a400f7`.
- 2026-06-22: Gate shipped **always-on melodic** (velocity 255), not register-tap-gated as the plan floated — register-tap would make `power` double-duty with AlgoGenerator's cooldown gate-thinning. Pitch carries the Turing identity. Register-derived "pulses" gating is a tuning follow-up.

## Open questions
- [ ] Manual sim play-test: select "Turing" on a Tuesday track, sweep `power` frozen→drift; confirm preview/apply is reproducible and `flow`/`ornament` change length/spread.

## Completed steps
- [x] U1 `TuringRegister` core (test-first, green). `8a42c4b1`
- [x] U2 deterministic Turing in `TuesdayAlgoCore`. `8e9b053c`
- [x] U3 live-drift Turing in `TuesdayTrackEngine`. `f9f87512`
- [x] U4 registered as algo 15 (range/name/randomize). `e7a400f7`
- [x] Sim + STM32 release clean; `TestTuringRegister` green.

## Notes
Resolves the long-standing "Turing as a Tuesday algo" actionable under `performer-improvements`
(the performer-nx fork survey flagged a shift-register sequencer as the one genuine gap). Tuesday
implements algos twice (live `TuesdayTrackEngine` + `TuesdayAlgoCore` for AlgoGenerator); Turing
lives in both, sharing the `TuringRegister` core. Deferred: register-tap gating, exposing length
beyond the flow range.
