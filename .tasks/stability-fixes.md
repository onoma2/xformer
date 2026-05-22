# stability-fixes

## Goal
Track narrow stability fixes found during adversarial reviews. This task is for crash, corrupt-state, out-of-bounds, and bad-serialization fixes only; musical semantics and UI polish belong in their feature tasks.

## Key files
- `src/apps/sequencer/model/Modulator.h` — serialized modulator state needs post-read sanitization.
- `src/apps/sequencer/engine/ModulatorEngine.h` — ADSR tick math can divide by zero for small nonzero envelope times.
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` — generator page callbacks need selected-track type safety.
- `src/apps/sequencer/engine/generators/SequenceBuilder.h` — generator builder relative indexing can exceed sequence bounds.
- `src/apps/sequencer/engine/generators/RandomGenerator.cpp` — Random generator writes the full 64-step pattern through relative builder indices.
- `src/apps/sequencer/engine/Engine.cpp` — CV Router output computation and bus safety/write ordering need deterministic engine timing.
- `src/apps/sequencer/engine/RoutingEngine.cpp` — bus target writes and stateful shaper reset criteria need review.
- `src/apps/sequencer/model/CvRoute.h` — lane getters should guard invalid lane indices like setters already do.
- `src/apps/sequencer/ui/pages/MonitorPage.cpp` — Scope is draw-rate sampled and should not be treated as timing proof.

## Decisions log
- 2026-05-22: CvRoute lane getter guards added as low-cost hardening. Current callers appear valid, so this is backlog hardening rather than a known live crash.
- 2026-05-22: Scope/CV Router/Bus audit added recommended follow-ups. CV Router physical-output ordering is the only critical stability/timing fix from that audit; bus writer observability/priority is recommended next. Scope remains monitor-only unless promoted to a diagnostic tool.
- 2026-05-22: Initial scope is stability only. Excluded semantics/UI-only issues: modulator wall-clock vs transport-clock behavior, modulator scaling feel, generator A/B label polish.

## Open questions
- [ ] Should generator builders use absolute 0..63 step addressing, or should generators be restricted to `builder.length()`?
- [ ] Should generator page invalid context no-op, close, or revert+close on draw/LED callbacks?

## Backlog
- [ ] Sanitize `Modulator::read()` after deserialization. Bad project data can set `_rate=0` or invalid enums, leading to divide-by-zero or invalid engine paths.
- [ ] Clamp ADSR computed tick counts to at least 1 in `ModulatorEngine`. Small nonzero attack/decay/release values currently compute zero ticks.
- [ ] Add bound-track/type guards to `GeneratorPage` draw/LED/input/context callbacks. `updateLeds()` currently uses type-specific accessors after selected-track changes.
- [ ] Fix generator relative indexing out-of-bounds. `RandomGenerator::update()` writes 64 values through `SequenceBuilder::setValue()`, which adds `firstStep`.
- [ ] Reorder CV Router computation before physical CV output composition. `updateTrackOutputs()` currently reads `_cvRouteOutputs` before `updateOverrides()` recomputes them, making CV Route physical outputs one engine update stale.
- [ ] Add deterministic bus writer feedback or priority. RoutingEngine, CV Router, and Teletype can all write bus CV; current behavior is implicit last-writer-wins with no per-bus owner/debug signal.
- [ ] Review bus target shaper reset criteria. Bus routes reset state on target/tracks/shaper changes, but source/bias/depth/min/max edits can leave stateful shapers carrying stale state.
- [ ] Guard `CvRoute::inputSource()` and `CvRoute::outputDest()` against invalid lanes. Setters already guard; getters currently index arrays directly and rely on every caller passing 0..3.

## Completed steps
- [x] 2026-05-22 adversarial review identified the first four stability issues.
- [x] 2026-05-22 Scope/CV Router/Bus audit identified one critical timing fix and two recommended bus-system hardening items.

## Notes
- Keep this task narrow. Do not use it for feature feel, labels, generator workflow improvements, or modulator musical behavior unless they can crash, corrupt memory, or load unsafe serialized state.
