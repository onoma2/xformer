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
- 2026-05-20 (grilling session): L1/L2/L3 redesign synthesized after reading SIG/Proteus/Marbles/Melodicer references and verifying current code. Full design at `.tasks/stochastic-track-port/PHASE10.7-L1-REDESIGN.md`. Key decisions: L1 follows Proteus archetype (single-knob macros driving sophisticated engine algorithms, NOT edit-time fan-outs). L1 = Density + Complexity (renamed from Character) + Shape (mutually exclusive with Complexity). Each macro writes ONE field; engine-clever derivation interprets L2 sub-controls from the macro when L2 fields are at default; explicit L2 user edits override. Burst moves to L2 only. Range + Min/Max both kept. Looper (Mode, Refresh, Size, First, Last, Rotate, Patience, Mask, Tilt, Mutate, Lock) is universal across all levels. Drop _accent (no second-gate use case), keep _legato + _slide. Mutate becomes bipolar int8_t (-100..+100): center=lock, +=Marbles permutation, -=Proteus destructive. Marbles Steps gets scale-aware weight law for 2..32-degree scales. Pitch generation is 3-way exclusive: tickets > Marbles > Complexity. Variation is L2-only and must be gated out of duration-ticket path. Exact derivation curves deferred to hardware tuning in Phase 2.
- 2026-05-20: Phase 1 systematic debugging pass against PHASE7/9/10 contracts. Surfaced 12 confirmed bugs and 3 design clarifications. Full catalog at `.tasks/stochastic-track-port/PHASE10.6-BUG-CATALOG.md`. Confirmed intentional: divisor and clockMultiplier are ignored in duration-ticket mode (tickets express absolute musical duration, divisor sets the rate-mode step — two modes, two semantics, no competition). `_level` confirmed sequence-owned and must be serialized — per-pattern level recall is intentional UX. PHASE7-DICTIONARY line 224 ("use clock divisor to make the whole track slower") needs amendment because it does not apply in ticket mode. Three implementation clusters: A=serialization symmetry, B=ticket-mode state machine, C=macro contract. Variation is L2-only and must be gated out of the ticket path; lock state must not be serialized; `_density`/`_durationTickets[8]`/`_level` must be serialized. Sleep behavior in ticket mode parked for later. No fixes applied yet — Phase 2 starts with Cluster A under TDD.
- 2026-05-19: Decision: V4 Ownership Correction and UI Unstubbing complete. Achieved balanced S3 Ticket Page geometry (baseY=46, barMaxH=26). Moved bank switching to Left/Right arrow keys to match Performer conventions. Integrated QuickEdit shortcuts (INIT/EVEN/RAND) on steps 10-12. Verified RAM/Flash safety on STM32 release.
- 2026-05-19: Decision: V4 Ownership Correction Plan created. We will pack events to 48 bits (6 bytes) to save 4KB of Track RAM, enabling us to safely move 29 pattern-defining fields (like generator params and pitch tickets) to Sequence ownership.
- 2026-05-18: Phase 7b complete. Re-topologized model for RAM safety (Sequence size: 16B, Track buffer: 2KB). Implemented MeloDICER-style duration variation and dynamic Burst Count/Rate semantics. Verified full evaluated lock and project load stability.
- 2026-05-17: Phase 8 UI Stubs complete. Implemented list-based editors for both global Track parameters and per-step Sequence probabilities. Users can now edit all stochastic metrics on hardware.
- 2026-05-17: Phase 6 repair complete. Retopologized performance layer to Density/Tilt/Jitter. Implemented true ranked rest-priority for density thinning and stable timing jitter. Verified lock invariant.
- 2026-05-17: Phase 5 complete. Implemented Accent and Legato probabilities. Accent is mapped to a secondary gate output (index 1), and Legato ties the gate to the next note while triggering a slide. Both are captured in the lock buffer.
- 2026-05-17: Phase 4 complete. Implemented track-level Loop Windowing (loopFirst/loopLast) and Sequence Rotation. Fixed the lock invariant by capturing gateOffset and slide state.
- 2026-05-17: Initializing task wiki. Consolidated "Enhanced" design with Vinx port plan.
- 2026-05-17: Decision: Use heap-allocated lock buffer to save CCMRAM (nongotiable for RAM acceptance).
- 2026-05-17: Decision: Implement Marbles control law (Spread/Bias/Steps) for pitch shaping as an optional mode.
- 2026-05-17: Spec Alignment: Adopted 7-phase plan from `2026-05-17-enhanced-stochastic-track-design.md`.

## Open questions
- [ ] Should we use a dedicated `StochasticPitchWeightTable` sub-model or keep it inline in `StochasticTrack`?
- [ ] Does the Pitch Weight Table need its own UI page or a sub-section of `StochasticConfigPage`?
- [ ] Confirm exact `sizeof(StochasticTrack)` on hardware before finalizing serialization.
- [ ] Macro contract (Cluster C): what should `editDensityMacro` and `editCharacterMacro` actually do? Options for Density: (a) set Density only, (b) reset Rest on first invocation, (c) keep coupling but document it. Same question for Character (Complexity/Contour/Linearity).
- [ ] H5 `marblesSteps`: implement Steps quantization in the marbles distribution, or remove the control if not landing in V5?
- [ ] M3 sleep behavior in ticket mode: keep the current free/aligned-only decrement, or extend to ticket mode? Parked.

## Completed steps
- [x] Comprehensive research of Vinx port requirements
- [x] Design of Enhanced Stochastic features (PWT, Linearity, Marbles)
- [x] RAM/CCMRAM budget assessment and mitigation strategy (heap lock)
- [x] Update task wiki plan in accordance to spec
- [x] Phase 9: V4 Ownership Correction (Sequence owns generative identity)
- [x] Phase 9.1: UI Unstubbing (S0/S1/S2 real hardware mapping)
- [x] Phase 9.2: Ticket Page Refinement (Banking, Arrow navigation, QuickEdit)

## Planned Phases (from Spec)

### Phase 1: Model & Serialization
- [x] Implement `StochasticSequence` (64 steps, bit-packed).
- [x] Implement `StochasticTrack` (16 patterns + Snapshot, PWT, Marbles params, range).
- [x] Integrate into `Track` container and `Project` selection.
- [x] **Validation:** `sizeof(StochasticTrack)` probe and Project save/load.

### Phase 2: Engine Foundations
- [x] Port `StochasticEngine` skeleton.
- [x] Replace `std::vector` and `std::sort` with fixed arrays/logic.
- [x] Implement heap-allocated `_lockedSteps` cache.
- [x] **Validation:** Engine CCMRAM gate check (< 912 B).

### Phase 3: Global Logic
- [x] Implement Pitch Weight Table (PWT) weighted selection.
- [x] Implement Linearity (melodic smoothing).
- [x] Implement Marbles-style pitch distribution (Spread/Bias/Steps).

### Phase 4: Loop Controls
- [x] Implement Captured Event Lock logic (freeze evaluated results).
- [x] Implement Loop Windowing (First/Last) and Rotation.
- [x] **Validation:** Verify locked-loop invariant (source edits don't change lock).

### Phase 5: Dynamics
- [x] Implement Accent Probability.
- [x] Implement Legato/Slew Probability.

### Phase 6: Tuesday-Derived Stochastic Mechanics
- [x] Implement Power (Density) and Skew (Tilt) deterministic logic.
- [x] Implement Micro-gate / Step Burst Scheduler.
- [x] Implement Timing Looseness (Jitter).
- [x] **Validation:** Confirm deterministic thinning and jitter lock.

### Phase 7: Proteus-Inspired Buffer Evolution
- [x] Implement Phase 7b: Rebuild core for Track-owned generator and pattern buffer.
- [ ] Implement melody-lifecycle generation logic (Complexity/Contour).
- [ ] Implement Patience / Boredom reset.
- [ ] Implement per-loop mutation and octave shifts.

### Phase 8: UI Development
- [x] Implement `StochasticSequenceEditPage` stub (Boring List).
- [x] Implement `StochasticConfigPage` stub (Boring List).
- [x] Implement final visual UI (Ticket Bar Page with Banking).
- [ ] Implement Layer switching visualization for Performance Page.

### Phase 9: Validation & Optimization
- [x] Final hardware verification (STM32 release) of memory gates.
- [ ] Generator system integration (`StochasticSequenceBuilder`).

## Notes
- `StochasticTrack` target size: 8,854 B (NoteTrack gate: 9,544 B).
- `StochasticEngine` target size: < 912 B (via heap allocation).
- Reference Vinx implementation: `temp-ref/vinx-performer/src/apps/sequencer/{model,engine,ui}/Stochastic*`
