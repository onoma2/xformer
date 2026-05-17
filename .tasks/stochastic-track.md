# stochastic-track

## Goal
Implement the Enhanced Stochastic Track, a probability-driven melodic sequencer combining Vinx-style step probabilities with global stochastic constraints (Pitch Weight Table, Linearity, Marbles-style distribution shaping) and a hardware-safe heap-allocated event lock buffer.

## Key files
- `src/apps/sequencer/model/StochasticTrack.h` — Model definition (Track settings, PWT, Marbles)
- `src/apps/sequencer/model/StochasticSequence.h` — Sequence step data (64 steps, 16 layers)
- `src/apps/sequencer/engine/StochasticEngine.h` — Core evaluation engine (Heap-allocated lock)
- `src/apps/sequencer/ui/pages/StochasticSequenceEditPage.h` — Grid editor
- `src/apps/sequencer/ui/pages/StochasticConfigPage.h` — Global settings UI

## Decisions log
- 2026-05-17: Initializing task wiki. Consolidated "Enhanced" design with Vinx port plan.
- 2026-05-17: Decision: Use heap-allocated lock buffer to save CCMRAM (nongotiable for RAM acceptance).
- 2026-05-17: Decision: Implement Marbles control law (Spread/Bias/Steps) for pitch shaping as an optional mode.
- 2026-05-17: Spec Alignment: Adopted 7-phase plan from `2026-05-17-enhanced-stochastic-track-design.md`.

## Open questions
- [ ] Should we use a dedicated `StochasticPitchWeightTable` sub-model or keep it inline in `StochasticTrack`?
- [ ] Does the Pitch Weight Table need its own UI page or a sub-section of `StochasticConfigPage`?
- [ ] Confirm exact `sizeof(StochasticTrack)` on hardware before finalizing serialization.

## Completed steps
- [x] Comprehensive research of Vinx port requirements
- [x] Design of Enhanced Stochastic features (PWT, Linearity, Marbles)
- [x] RAM/CCMRAM budget assessment and mitigation strategy (heap lock)
- [x] Update task wiki plan in accordance to spec

## Planned Phases (from Spec)

### Phase 1: Model & Serialization
- [x] Implement `StochasticSequence` (64 steps, bit-packed).
- [x] Implement `StochasticTrack` (16 patterns + Snapshot, PWT, Marbles params, range).
- [x] Integrate into `Track` container and `Project` selection.
- [x] **Validation:** `sizeof(StochasticTrack)` probe and Project save/load.

### Phase 2: Engine Foundations
- [ ] Port `StochasticEngine` skeleton.
- [ ] Replace `std::vector` and `std::sort` with fixed arrays/logic.
- [ ] Implement heap-allocated `_lockedSteps` cache.
- [ ] **Validation:** Engine CCMRAM gate check (< 912 B).

### Phase 3: Global Logic
- [ ] Implement Pitch Weight Table (PWT) weighted selection.
- [ ] Implement Linearity (melodic smoothing).
- [ ] Implement Marbles-style pitch distribution (Spread/Bias/Steps).

### Phase 4: Loop Controls
- [ ] Implement Captured Event Lock logic (freeze evaluated results).
- [ ] Implement Loop Windowing (First/Last) and Rotation.
- [ ] **Validation:** Verify locked-loop invariant (source edits don't change lock).

### Phase 5: Dynamics
- [ ] Implement Accent Probability.
- [ ] Implement Legato/Slew Probability.

### Phase 6: UI Development
- [ ] Implement `StochasticSequenceEditPage` (Grid + Layer switching).
- [ ] Implement `StochasticConfigPage` (List settings + PWT visualization).
- [ ] Implement `StochasticSequencePage` (Pattern list).

### Phase 7: Validation & Optimization
- [ ] Final hardware verification (STM32 release).
- [ ] Generator system integration (`StochasticSequenceBuilder`).

## Notes
- `StochasticTrack` target size: 8,854 B (NoteTrack gate: 9,544 B).
- `StochasticEngine` target size: < 912 B (via heap allocation).
- Reference Vinx implementation: `temp-ref/vinx-performer/src/apps/sequencer/{model,engine,ui}/Stochastic*`
 8,854 B (NoteTrack gate: 9,544 B).
- `StochasticEngine` target size: < 912 B (via heap allocation).
- Reference Vinx implementation: `temp-ref/vinx-performer/src/apps/sequencer/{model,engine,ui}/Stochastic*`
