# stability-fixes

## Goal
Narrow crash, corrupt-state, out-of-bounds, and bad-serialization fixes found during adversarial reviews. Stability only — musical semantics and UI polish belong in their feature tasks. Re-extracted from `performer-improvements` Phase 0 on 2026-05-29 (the merge crowded a feature-integration task with unrelated crash fixes; see decisions log).

## Key files
- `src/apps/sequencer/engine/Engine.cpp` — CV Router output computation / write ordering.
- `src/apps/sequencer/model/Modulator.h` — serialized modulator state needs post-read sanitization.
- `src/apps/sequencer/engine/ModulatorEngine.h` — ADSR tick math can divide by zero for small nonzero envelope times.
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` — generator page callbacks need selected-track type safety.
- `src/apps/sequencer/engine/generators/SequenceBuilder.h` / `RandomGenerator.cpp` — relative indexing can exceed sequence bounds.
- `src/apps/sequencer/engine/RoutingEngine.cpp` — bus target writes + stateful shaper reset criteria.
- `src/apps/sequencer/model/CvRoute.h` — lane getters should guard invalid indices like setters already do.

## Items

**Critical (timing/stability):**
- [ ] **CV Router output one update stale** — `Engine.cpp`: `updateTrackOutputs()` reads `_cvRouteOutputs` before `updateOverrides()` recomputes them. Reorder CV Router computation before physical CV output composition. (Do this first.)

**Crash / corruption:**
- [ ] **Modulator deserialization sanitize** — `Modulator::read()`: bad project data can set `_rate=0` or invalid enums → divide-by-zero / invalid engine path.
- [ ] **ADSR zero-tick clamp** — `ModulatorEngine.h`: small nonzero attack/decay/release compute zero ticks; clamp to ≥1.
- [ ] **GeneratorPage bound-track/type guards** — draw/LED/input/context callbacks use type-specific accessors after the selected track changes.
- [ ] **Generator relative-index OOB** — `RandomGenerator::update()` writes 64 values via `SequenceBuilder::setValue()`, which adds `firstStep`.

**Hardening (recommended, not known-live):**
- [ ] **Bus writer arbitration** — RoutingEngine / CV Router / Teletype all write bus CV, implicit last-writer-wins; add priority or observability.
- [ ] **Bus shaper stale state** — source/bias/depth/min/max edits don't trigger shaper reset (only target/tracks/shaper do).
- [ ] **CvRoute getter guards** — `inputSource()`/`outputDest()` index arrays unguarded while setters already guard. Backlog hardening — current callers appear valid.

## Moved out / already done
- **Slave-clock scheduling-period filter** → now owned by `wallclock-time-architecture` (the WallClock service folds in the slave-period outlier guard; ER-101's 0.5×–2× latch is the proven minimum). Not here.
- **Already done in main, do not re-add:** SortedQueue drop-oldest overflow guard (`SortedQueue.h:32`, landed 2026-05-23); `SANITIZE_TRACK_MODE` enabled via `CONFIG_ENABLE_SANITIZE=1` (`TrackEngine.h:25`).

## Open questions
- [ ] Generator builders: absolute 0..63 addressing, or restrict to `builder.length()`?
- [ ] Generator page invalid context: no-op, close, or revert+close on draw/LED callbacks?

## Decisions log
- 2026-05-22: CvRoute lane getter guards are low-cost hardening. Current callers appear valid, so this is backlog hardening, not a known live crash.
- 2026-05-22: Scope/CV Router/Bus audit added recommended follow-ups. CV Router physical-output ordering is the only critical stability/timing fix from that audit; bus writer observability/priority is recommended next. Scope remains monitor-only unless promoted to a diagnostic tool.
- 2026-05-22: Initial scope is stability only. Excluded semantics/UI-only issues: modulator wall-clock vs transport-clock behavior, modulator scaling feel, generator A/B label polish.
- 2026-05-22: Adversarial review identified the first four stability issues; Scope/CV Router/Bus audit identified one critical timing fix and two recommended bus-system hardening items.
- 2026-05-29: Folded into `performer-improvements` Phase 0, then re-extracted to a standalone task — stability/crash fixes are a different concern from fork-feature integration, and the slave-clock item had migrated to `wallclock-time-architecture`.

## Notes
- Items GeneratorPage + Generator-OOB touch generator code that moved to `feat/generator` (generator-preview-apply); verify against that branch before fixing.
- Keep this task narrow. Not for feature feel, labels, generator workflow, or modulator musical behavior unless they can crash, corrupt memory, or load unsafe serialized state.
