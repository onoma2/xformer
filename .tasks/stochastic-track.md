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
- 2026-05-21 (Container destructor-skip fix): Second layout commit on hardware (set track to Stochastic → init project → set track to Stochastic again → commit) was triggering hard reboot. Sim repro showed clean engine construction/destruction lifecycle but each Stochastic engine create leaked the prior `_lockedParents` heap block (~1.8 KB) because `Container::create<U>()` placement-new'd without calling the prior occupant's destructor. STM32's smaller newlib-managed heap exhausted after a few cycles; subsequent `new` allocations corrupted heap state and faulted. Fix is structural in `src/core/utils/Container.h`: `create<U>()` now stores a type-erased destructor pointer (`destructorThunk<U>`) on each construction and invokes it on the next create or on Container destruction. Container also gains its own destructor for scope-exit cleanup. Side effects: `Container::destroy(U*)` (typed-pointer overload) replaced with no-arg `Container::destroy()` since the destructor pointer is stored. Two call sites updated (LaunchpadController, ControllerManager). This is a generic improvement that fixes destructor-skip leaks for any future track engine type with non-trivial cleanup, not just Stochastic. Documented in `docs/performer-architecture.md` as an XFORMER addition over upstream Performer.
- 2026-05-21 (Engine rebuild-order fix): Init project on hardware was triggering hard reboot. Root cause: `Engine::update()` processed clock events at line 106 before `updateTrackSetups()` at line 137 rebuilt engine containers. `Project::clear()` (called from `ProjectPage::initProject` between `_engine.suspend()` and `_engine.resume()`) swapped each Track's union memory to NoteTrack via `Track::clear() → initContainer()`, but the engine container still held a StochasticTrackEngine whose `_stochasticTrack` reference now pointed at NoteTrack memory. `_engine.suspend()` calls `_clock.masterStop()` which queues clock events. On resume, the first `update()` cycle processed those events at line 106, triggering `Engine::reset()` → `StochasticTrackEngine::reset()` → `_stochasticTrack.sequence(pattern()).first()` → garbage offset read from NoteTrack memory → ARM hard fault → reboot. Pre-cleanup the Stochastic/Note layouts overlapped enough that garbage reads were non-fault-triggering; Cluster L cleanup shifted offsets and exposed the latent bug. Fix: move `updateTrackSetups()` to the top of `Engine::update()` (after the suspend-handling block, before clock-event processing). Establishes invariant that engine containers always match model trackMode before any engine code runs. STM32 release build clean; hardware confirmed no crash on init project.
- 2026-05-20 (Phase 2 landed + audit): Seven cluster commits implemented the deterministic-cluster plan from PHASE10.6 + PHASE10.7. Commit order: A (serialization) → Duration dictionary → L1 fan-out drops → 3-way exclusivity → Cleanup → B (ticket-mode state) → D (burst gate). Four unit tests added under `src/tests/unit/sequencer/` (`TestStochasticSequenceSerialization`, `TestStochasticDurationDictionary`, `TestStochasticL1Macros`, `TestStochasticGenerator`) — all passing in sim. Two notable agent decisions: (1) Cluster D raised minimum child gate from 1 to 6 ticks (~30ms at 120 BPM, PPQN=192) — judgment call beyond the plan, drops children that would produce sub-audio blips; (2) UI list model still has both old `Character` enum entry (calls `editComplexityMacro`) and new `Complexity` entry (calls `editComplexity` direct), both labeled "Complexity" — needs UX call on whether to drop one or rename. Hardware verification gate not yet crossed; next session: confirm burst audibility, ticket-mode no-machine-gun, variation gating, save/load round-trips. After hardware, resume into deferred hardware-tuning steps (scale-aware Marbles, bipolar Mutate, engine-clever Density/Complexity derivation curves).
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
- [ ] H5 `marblesSteps`: implement Steps quantization in the marbles distribution (scale-aware law from the Gemini-derived design), or remove the control if not landing in V5? — deferred to hardware-tuning session.
- [ ] M3 sleep behavior in ticket mode: keep the current free/aligned-only decrement, or extend to ticket mode? Parked.
- [ ] UX call on dual Character/Complexity list entries — keep both with different labels, drop the old Character entry, or rename one. Both currently display as "Complexity" which will confuse a user.
- [ ] Engine-clever derivation curves (Density → effective rate/variation/rest, Complexity → effective contour/linearity) — exact musical mappings to tune by ear on hardware.
- [ ] Bipolar Mutate behavior verification — permutation vs destructive distinction across realistic loop content.

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
