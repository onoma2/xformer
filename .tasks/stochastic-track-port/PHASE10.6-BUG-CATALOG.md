# Phase 10.6: Bug Catalog (Phase 1 Investigation, 2026-05-20)

## Origin

Phase 1 of systematic debugging against the V5 stochastic implementation. Findings cross-verified
against `PHASE7-DICTIONARY.md`, `PHASE9-V4-OWNERSHIP.md`, and `PHASE10-V5-CONTROL-GRANULARITY.md`.
Evidence gathered with no fixes applied. This catalog is the persistent state for the Phase 2
(implementation) follow-up.

## Status legend

- [ ] open — not yet fixed
- [x] fixed — has landed
- [~] parked — deferred by user decision

## Cluster A — Serialization Symmetry (one diff)

Pattern: `StochasticSequence::clear()` / `sanitizeAfterRead()` know about fields that
`write()` / `read()` forgot. PHASE9-V4 line 192 specifically warned read/write order
must remain symmetric.

- [ ] **C1** `_density` not serialized. File `src/apps/sequencer/model/StochasticSequence.h`
  lines 518–574 (`write`) and 576–658 (`read`). Add at the position alongside `_mask` / `_tilt` Routables.
- [ ] **C2** `_durationTickets[8]` not serialized. Same file. 8 bytes, place near `_degreeTickets[]`.
- [ ] **C3** `_level` not serialized. Sequence-owned per design (per-pattern level recall is intentional);
  add as `uint8_t` enum cast with the other enum reads. Default `StochasticLevel::Core` on out-of-range.
- [ ] **H2** `StochasticTrack::_lock` is currently serialized — `StochasticTrack.h:189` (write) and
  `:213` (read). PHASE9-V4 Ownership Table line 55 says lock is engine-only, **must not** be serialized.
  Remove from the stream while touching this cluster. Keep `_lock` as transient track state.

**Acceptance:** project save/load round-trip preserves Density, DurationTickets, Level;
does NOT preserve Lock state.

## Cluster B — Ticket-Mode State Machine (one diff)

Pattern: the duration-ticket branch in `StochasticTrackEngine::tick()` has its own
event-based timing (`_eventElapsed`, `_eventDuration`) that is not symmetric with the
Free/Aligned branches' `_relativeTick` / `_lastFreeStepIndex` reset semantics.

- [ ] **C5** Locked replay + duration-tickets re-triggers every tick.
  `StochasticTrackEngine.cpp:217–271` (lock path) never writes `_eventDuration`.
  The ticket gate at 128–130 uses `_eventDuration==0` (or elapsed≥duration). With lock active and
  no prior free triggerStep, this fires every tick. Fix shape: write `_eventDuration` from
  `_lockedParents[readIndex].durationTicks` in the locked branch (or refactor so both branches
  funnel through one duration-publish step).
- [ ] **B2** `resetMeasure()` doesn't clear `_eventDuration`. `StochasticTrackEngine.cpp:72–76`.
  After a measure boundary reset, ticket mode waits up to 384 ticks (old 1/2 note) before
  next fire. Asymmetric with Aligned mode where `_relativeTick=0` fires immediately.
  Fix shape: zero `_eventDuration` in `resetMeasure()` so next tick re-triggers, OR set
  `_eventElapsed = _eventDuration` to force immediate retrigger.
- [ ] **B3** `restart()` resets neither `_eventElapsed` nor `_eventDuration`.
  `StochasticTrackEngine.cpp:103–109`. Same problem as B2 plus uninitialized counters.
  Fix shape: zero both, like `reset()` does.
- [ ] **H1** Variation incorrectly applied in ticket mode.
  `StochasticTrackEngine.cpp:307–316`. Variation is L2-only per user; tickets are L3.
  Currently the 2×/½ block runs regardless of mode. Fix shape: gate the variation block
  on `!sequence.durationTicketsActive()`.

**Acceptance:** ticket-mode reset/restart behaves identically to Aligned reset/restart
(immediate first event after boundary). Lock+tickets does not machine-gun. Variation
moves duration only in rate mode.

## Cluster D — Burst Gate Scheduling (new, 2026-05-20)

User-reported symptom: "most of burst gates don't reach output."

- [ ] **D1** Parent gate-OFF masks early child gate-ONs.
  `StochasticTrackEngine.cpp:382-389`. When `hasChildren=true`, `gateLen = min(durationTicks*50/100, 10)`.
  The parent OFF at `tick+10` fires `_gateOutput = false`, cutting any child whose
  ON time falls before tick+10. With 4 children at burstRate=50 in a 48-tick parent
  (1/16), child offsets ≈ 4, 9, 14, 19. Child 1 (offset 4) gets its own brief
  gate; child 2 (offset 9) fires ON at tick+9 then parent OFF at tick+10 forces low
  → child 2 effectively gets a 1-tick gate that may not trigger downstream modules.
  Children at offsets 14, 19 fire after parent OFF and retrigger cleanly.
  **Net: 1–2 early children silently truncated.**
  Same defect in the locked-replay branch (`triggerStep` 217–271, same gateLen logic).

  Fix shape options:
  - (a) Skip the parent gate-OFF push entirely when `hasChildren` — let the last child's
        OFF be the natural gate-low. Parent gate becomes the envelope of the whole burst.
  - (b) Compute `gateLen = max(10, last_child_off_tick)` so parent OFF never precedes a
        child ON.
  - (c) Make the gate queue popper prefer gate-ON over gate-OFF on same-tick collision
        (tie-break). Doesn't help with non-collision case where parent OFF lands between
        a child's ON and OFF.

  Recommended: (a) for cleanest semantics. Parent gate spans the whole parent duration
  (the burst envelope); children retrigger via their own low→on→off cycles inside it.

## Cluster F — Burst at Short Parent Durations (musical issue, deferred)

Surfaced 2026-05-21 during hardware feature verification. Phase 2 Cluster D fixed the
parent-gate-masking bug and added a 6-tick child gate floor. Both correct individually.
But the underlying burst math doesn't scale gracefully to short parent durations.

- [~] **F1** Burst at parent durations ≤1/16 produces sub-audible mush. With divisor at
  1/16 (48 ticks per step) and BurstCount near max (3 children), child spacing is ~12
  ticks (~60 ms at 120 BPM). At divisor 1/32 (24 ticks), children pack to ~6 ticks and
  the floor either drops them or merges them with the parent. Perceptually the burst
  becomes a thickened click, not distinct hits.

  Possible resolution paths (pick one or combine when implementing):
  - **Hard duration gate.** No burst evaluation when `durationTicks < threshold`
    (e.g. <96 ticks = below 1/8). Simplest; surfaces as "burst silently disabled at fast
    parent durations" which is musically fine but needs UI signal so users don't think
    burst is broken.
  - **Count-shrinking law.** Child count caps based on durationTicks: full count only
    above 1/4, halved at 1/8, single child at 1/16, zero below. Smooth degradation,
    no UI surprise.
  - **Burst probability gate.** Multiply BurstProbability by a duration-scaling factor:
    full at long parents, ramped down to zero below a threshold. User-facing behavior is
    "fewer bursts on fast parents," which is musical.
  - **Different burst model.** Replace child-events-with-own-CV with ratchet-over-parent
    (repeated gates at the parent's CV, no separate scheduling). Simpler at fast
    durations but loses BurstPitch=Generate variation.

  Recommended: count-shrinking law (graceful degradation) + a UI hint when the effective
  count falls below the configured count.

  Out of Phase 2 scope. Implementation gated on user confirmation of approach.

## Cluster E — Performer-Native Controls (preservation, not bug)

Add to L1 redesign acceptance: do NOT remove or hide these Performer-native sequence
controls from the model or UI. They remain editable at any level:

- [x] `_divisor` (step duration in clock ticks)
- [x] `_clockMultiplier` (per-sequence clock rate)
- [x] `_resetMeasure` (measure-based pattern restart)
- [x] `_scale` (scale selection)
- [x] `_rootNote` (root)
- [x] `_size`, `_first`, `_last` (loop window)

These are part of the standard Performer track interface and must survive the L1/L2/L3
redesign regardless of which level the user is on. They live alongside the universal
looper layer.

Note that in duration-ticket mode `_divisor` and `_clockMultiplier` are intentionally
ignored (per Cluster C design decisions) but the fields stay and remain editable —
they govern rate-mode and any future modes.

## Cluster C — Macro Contract (independent)

L1 macros must derive through the same fields L2/L3 expose, not own their own coupled state.
Currently the L1 macros write through to multiple sequence-owned fields, clobbering anything
a user set at L2/L3.

- [ ] **H3** `editDensityMacro` couples `Rest = 100 − Density`.
  `StochasticSequence.h:335–341`. Combined with the engine law
  `isRest = !densityGate || restGate`, Density=80 yields ~36% rests, not 20%.
  Dictionary lines 200–203 treat Density and Rest as orthogonal.
- [ ] **H4** `editCharacterMacro` writes Complexity + Contour + Linearity in lockstep.
  `StochasticSequence.h:343–350`. Overwrites L3 edits. PHASE10 acceptance:
  *"Macro edits and lower-level edits do not stack as contradictory hidden systems."*

**Open question for design:** what SHOULD `editDensityMacro` do? Options:
- (a) only set Density, leave Rest alone (cleanest orthogonality)
- (b) set Density only, but on first invocation reset Rest to 0
- (c) keep the coupling but document it in the macro decomposition spec
Same question for `editCharacterMacro`.

## Independent

- [ ] **H5** `marblesSteps` exposed in UI but engine ignores it.
  `StochasticGenerator.cpp:206–212` reads only `marblesSpread` and `marblesBias`.
  Sequence stores `_marblesSteps` (serialized), UI exposes "Steps" control
  (`StochasticPerformanceListModel.h:241`, coreItems line 283). The slider does nothing.
  PHASE7 line 184 marks Steps required at Level 1.
  Fix shape: add Steps quantization to the marbles distribution in `generateDegree`,
  or remove the control if not landing in V5.
- [ ] **H6** `mutateMelodyOne` passes mod-degree where absolute degree expected.
  `StochasticGenerator.cpp:59`. `eval.degree()` returns 0..(activeNotes−1) only.
  `generateDegree` uses `lastDegree` for proximity penalties expecting
  0..range*activeNotes−1. Mutation can drift toward octave 0.
  Fix shape: reconstruct absolute lastDegree as `degree + octave * activeNotes`.
- [ ] **#4-UI-badge** `pitchTicketsActive()` is UI-only — generator ignores it.
  `StochasticSequenceEditPage.cpp:135, 137` show ON/OFF badge based on
  `pitchTicketsActive()`. Generator always uses tickets path. When all tickets are 0,
  generator falls to uniform random over included degrees, but the badge says OFF
  despite excluded degrees (`-1` entries) still filtering output. UX bug: badge lies.
  Fix shape: either widen `pitchTicketsActive()` to "any non-default state including
  exclusions" or rephrase the badge.
- [ ] **H7** `generateJumpOctave()` is a stub returning 0.
  `StochasticGenerator.cpp:268–270`. Actual jump logic lives in
  `StochasticTrackEngine.cpp:427–433`. Dead helper. Either implement it or remove.

## Low — dead state cleanup

- [ ] **L1** Dead Track-level fields still serialized, never read by engine:
  `_loopFirst`, `_loopLast`, `_gateBias`, `_retriggerBias`, `_lengthBias`, `_noteBias`,
  `_fillMode`, `_fillMuted` (engine ignores; only UI reads), `_modeInternal`,
  `_reservedJitter` on Sequence. Includes their routing targets
  (`GateProbabilityBias`, etc.) which write to unused memory.
- [ ] **L2** `#define DBG_STO_ENABLE` left on at `StochasticTrackEngine.cpp:18`.
  `printf` per triggerStep. Intended debug from commit 058a24d0.
- [ ] **L3** `reset()` writes `_eventElapsed = 0; _eventDuration = 0;` twice
  (lines 87–90). Cosmetic.

## Parked (decide later)

- [~] **M3** `sleep` ignored in ticket mode. `StochasticTrackEngine.cpp:144, 157`
  decrement `_sleepRemaining` only in Free/Aligned branches. Ticket branch never
  consults it. User decision: keep as-is for now, figure out later.

## Design decisions confirmed during this investigation (NOT bugs)

Recording so future agents don't try to "fix" these.

- **C4 ✗** Duration-ticket mode ignores `sequence.divisor()`. **Intentional.**
  Tickets express absolute musical duration; divisor sets the rate-mode step.
  Two modes, two semantics. Letting divisor through would either double-scale or
  force divisor into being a meta-scale-of-the-ticket with no clean musical reading.
- **clockMultiplier ignored in ticket mode.** Same rationale as C4. Intentional.
- **`_level` ownership = Sequence (per-pattern).** Switching patterns recalls each
  pattern's editing level alongside its other material. PHASE7 lines 312–313 say
  page level is UI-owned, but project decision is to make it part of pattern
  identity. Wiki line corrected here.
- **PHASE7-DICTIONARY.md:224** *"No `1 bar`: use the sequence clock divisor to make
  the whole track slower"* is misleading given C4 — divisor does not slow the track
  in ticket mode. To slow ticket mode, user picks longer tickets (1/2, 7/16).
  Docs update needed in PHASE7 dictionary.

## Cluster ordering recommendation

Suggested Phase 2 implementation order:

1. **Cluster A (Serialization)** — small, isolated, high impact. Project save/load works correctly after.
2. **Cluster B (Ticket-mode state)** — fixes the most user-visible runtime defects.
3. **Cluster C (Macro contract)** — needs design call on the coupling question first.
4. **Independent** — pick by user priority (H5/H6 most impactful).
5. **Low** — cleanup pass after the above land.

Each cluster is independently testable. TDD entry point for Cluster A: write a serialization
round-trip test for `StochasticSequence` that sets Density=33, three duration ticket weights,
and Level=Direct, writes, clears, reads back, asserts equality.

## References

- `PHASE7-DICTIONARY.md` — vocabulary and behavior contract
- `PHASE9-V4-OWNERSHIP.md` — ownership rules, especially the engine-only Lock
- `PHASE10-V5-CONTROL-GRANULARITY.md` — control granularity model and macro
  decomposition rules
- `TASK.md` — master phase plan
