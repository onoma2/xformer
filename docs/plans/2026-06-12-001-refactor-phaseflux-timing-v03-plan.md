---
id: refactor-phaseflux-timing-v03
schema: plan
title: "refactor: PhaseFlux timing v0.3 — top-down declared period"
type: refactor
status: active
date: 2026-06-12
deepened: 2026-06-12
origin: docs/plans/2026-06-11-phaseflux-timing-v0.3-spec.md
---

# refactor: PhaseFlux timing v0.3 — top-down declared period

## Summary

Implement the v0.3 timing model: a declared period (8-slot bar table) divided
by per-cell weights, replacing the bottom-up duration sum. Engine math first
(test-first on the re-enabled cumulative-table harness), then model fields +
serialization (clean break, no migration), routing/param-table rows, edit-page
UI with the W-TR transfer gesture, and finally docs + task-wiki bookkeeping.

---

## Problem Frame

v0.2 pattern length is emergent (Σ cell durations), so every cell edit moves
the grid, four controls share one axis, and skip count leaks into timing. Full
analysis and the accepted model live in the origin spec.

---

## Requirements

- R1. 256 musical events (16 cells × 16 pulses) reachable at musical spacing — period table up to 32 bars. *(Uniform musical spacing is the equal-weight case: 32 bars ÷ 256 = eighths. Unequal weights trade uniform spacing for expressive per-cell share by design — a documented constraint, not a defect.)*
- R2. Pulse spacing within a cell depends only on that cell's weight, the period, and its pulseCount; skip never changes any span.
- R3. Sync by construction: only musical period values representable; pattern length constant under all per-cell edits; endpoint exact.
- R4. Phasing between two PhaseFlux tracks: Rate free-drift (exists) + routable globalPhase (delta).
- R5. Weight transfer editing (W-TR) conserves Σweights so exactly one boundary moves — mirrors Indexed DUR-TR.
- R6. LenN macro becomes the Even morph (additive all-cells weight nudge, 0 = as-authored, max = flat grid).
- R7. Deletions land fully: stageDivisor field, cycleLength mode, min-cycle floor — no dead remnants.

---

## Scope Boundaries

- No hardware flash/audition in this plan (sim build + ctest only — standing cadence).
- No changes to curve/pitch pipelines (v0.2 order stays), no changes to other track engines.
- No finer Rate resolution (routing covers it if ever needed).
- Weight display format (raw / share % / musical fraction) is decided **during** U5 via ui-preview renders — the render-and-pick step is in-scope; pre-deciding it is not.

### Deferred to Follow-Up Work

- Per-cell effective pulse-rate display ("≈1/16") on the edit page: considered in spec §8; do only if it falls out trivially during U5, otherwise its own pass.
- Capturing this rework as a `docs/solutions/` learning (no knowledge base exists yet).

---

## Context & Research

### Relevant Code and Patterns

- `src/apps/sequencer/model/PhaseFluxMath.cpp` — `computeCumulativeTicks` (the function being replaced); `src/apps/sequencer/model/PhaseFluxMath.h`.
- `src/apps/sequencer/model/PhaseFluxSequence.h/.cpp` — fields, accessors, serialization (sequence-level write/read at .cpp:169–236; cycleLength is bit-packed with `_pitchRate` at 0x20).
- `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp` — `detectLayoutChange` (:201), `rebuildCumulativeTable` (:215–246), skip-silence path (Fixed-mode behavior becomes the only behavior).
- `src/apps/sequencer/model/ParamTablePhaseFlux.cpp` — routable rows; `ParamKey::Phase` row already exists (:19), `LenNudge` row (:22), `Divisor` row (:13).
- `src/apps/sequencer/ui/model/PhaseFluxSequenceListModel.h` — Divisor/CycleLength/ClockMult/LenNudge rows (items, names, print, edit).
- `src/apps/sequencer/ui/pages/PhaseFluxEditPage.cpp` — P1 Len encoder (slot 0), MACRO P0 slot 3 (LenN), scope renderers consuming `cumulativeTicks`.
- **Transfer pattern**: `IndexedSequenceEditPage.cpp` `_durationTransfer` (toggle on same F-key, clamped pair edit) — mirror exactly.
- **Float routed values**: DiscreteMap rangeHigh/rangeLow are the existing `routedValue` float users — pattern for routing globalPhase.
- Sim test loop: `cmake --build build/sim/debug --target <Test> && ctest --test-dir build/sim/debug -R PhaseFlux`.

### Institutional Learnings

- Engine first, cosmetics last (standing refactor-priority rule) — unit order honors it.
- No migration/sentinels/version ceremony — clean break, write/read new layout unconditionally (standing policy; spec §7).
- Stale prebuilt test binaries can mask failures — rebuild before trusting a baseline (seen this branch).
- `TestPhaseFluxCumulativeTable` is **disabled** (commented out in `src/tests/unit/sequencer/CMakeLists.txt:40`, references the pre-redesign divisor model) — U1 rewrites and re-registers it.

---

## Key Technical Decisions

- **Period ticks derivation**: period slot → bar fraction {1/4, 1/2, 1, 2, 4, 8, 16, 32} × ticks-per-measure (engine's `measureDivisor()`), then Rate once (`× 100 / rate`). Cells never see ticks — only weights.
- **`computeCumulativeTicks` v2 signature**: `(traversal, weights[16], evenMorph, periodTicks, cumulativeTicks[17]) → periodTicks`. Pure, model-only, fully unit-testable. `fixedCycleLength`, per-cell divisor ticks, sequence divisor, clock multiplier, and the min-cycle floor all leave the signature.
- **Weight accessors**: rename `stageLen()/setStageLen()` → `weight()/setWeight()` (same 7-bit storage, default 64). Code says what it means; no installed base to protect.
- **Even morph**: `weight' = (weight == 0) ? 0 : weight + (64 − weight) × morph / 64`, morph 0..64. Interpolates each **nonzero** cell toward the default weight (morph 64 → exactly 64 → flat grid, exact, no tolerance); **weight-0 cells are skipped** so spec §3's "weight 0 removes the span" invariant holds (owner decision 2026-06-12). Stored in the renamed `_evenMorph` int8 slot (was `_lenNudge`); unipolar, negatives excluded. (The additive formula in spec §5 is superseded by this lerp — spec §3/§5 updated to match.)
- **globalPhase routability**: accessor switches from plain field read to the routed-value read with the existing `ParamKey::Phase` row (clamp 0..1, float path per DiscreteMap precedent). No new ParamKey needed.
- **Period routability**: mint `ParamKey::Period` (append in the PhaseFlux block of `model/RouteParamKey.h`); add `Routing::Target::Period` (before `Last` — accepts the route-file renumber under clean-break policy), the `RouteResolve::targetToParamKey` case, and its reverse via `RouteBrowse::paramKeyToTarget`; replace the PhaseFlux Divisor table row with a Discrete Period {0..7} row; audit `RouteBrowse` (its Clock band hardcodes `Divisor`, valid for other tracks — Period surfaces via the PhaseFlux engine page; confirm the row count).
- **stageDivisor is live, not inert**: it has no UI edit path but the engine reads it every rebuild (`PhaseFluxTrackEngine.cpp:222` → `stageDivisorTicks`). U3 must confirm `periodTicks` fully re-homes its tick contribution (a tick-equivalence assertion), not merely delete the read.
- **W-TR "next"**: next cell in traversal order, loop-aware, skipping zero-weight cells (a zero-weight cell has no boundary to move).

---

## Open Questions

### Resolved During Planning

- Mode (Redivide/Hold): deleted — skip = mute only, weight = time only (owner, 2026-06-12).
- LenN fate: repurposed as Even morph (owner, 2026-06-12).
- Period top slot: 32 bars, settled by R1.
- CumulativeTable test status: confirmed disabled; rewrite + re-enable in U1.
- Even morph & weight-0 cells: morph skips weight-0 cells (lerp toward 64 for nonzero only), preserving "weight 0 removes the span" (owner, 2026-06-12, deeper review).
- Unit sequencing: U2+U3 land atomically (unit tests link the whole app); U2 ⊥ U1; U4 depends on U3 (deeper review).

### Deferred to Implementation

- Weight display format: rendered three ways in ui-preview during U5; owner picks from real renders (per display-proposal rule).
- Exact silence behavior details in skipped spans (gate low for the whole span vs suppressing pulses) — follow the existing Fixed-mode skip path; verify against engine reality in U3.

---

## Implementation Units

### U1. Timing math v2 in PhaseFluxMath (test-first)

**Goal:** New `computeCumulativeTicks` — weights divide a declared periodTicks; Even morph; floor and mode deleted.

**Requirements:** R2, R3, R6, R7

**Dependencies:** None

**Files:**
- Modify: `src/apps/sequencer/model/PhaseFluxMath.h`, `src/apps/sequencer/model/PhaseFluxMath.cpp`
- Test: `src/tests/unit/sequencer/TestPhaseFluxCumulativeTable.cpp` (rewrite)
- Modify: `src/tests/unit/sequencer/CMakeLists.txt` (re-register the test)

**Approach:**
- Implement v2 per Key Technical Decisions; the old function is deleted in the same U2+U3 atomic landing (it cannot coexist — U2 removes the fields its call site reads).
- `W = Σ weight'` over all cells (`weight'` = morph-applied weight, weight-0 cells skipped by the morph; skip flag absent from the math); `cumulativeTicks[i+1] = round(periodTicks × partialΣ / W)`; endpoint forced exact.

**Execution note:** Test-first — the rewritten cumulative-table test is the primary harness; write it red against the new signature first.

**Test scenarios:**
- Happy path: 4 equal weights, 12 zero → four equal spans summing exactly to periodTicks; boundaries at 1/4 marks.
- Happy path: unequal weights {64,32,32} → spans in 2:1:1 ratio, endpoint exact.
- Edge: single nonzero weight → that cell owns the whole period.
- Edge: all weights zero → returns 0 (silent pattern), table zeroed.
- Edge: weight 0 cell between nonzero cells → zero-width (cumulative[i+1] == cumulative[i]).
- Edge: rounding — pick periodTicks and weights that don't divide evenly; assert interior boundaries rounded, endpoint == periodTicks exactly (R3).
- Happy path: Even morph max → every nonzero-weight cell becomes exactly weight 64 (flat grid, exact); morph 0 → identity (R6).
- Edge: weight-0 cells stay 0 at every morph value — the "weight 0 removes the span" invariant survives the morph (R6, owner decision).
- Happy path: traversal order applied — same weights, different traversal → boundaries follow the walk.
- Covers R3: every period slot value as periodTicks input → endpoint exact for each.

**Verification:** New test green via ctest; no caller of the old signature remains by end of U3.

---

### U2. Model fields + serialization (clean break)

**Goal:** Period slot field; weight rename; Even morph field; delete stageDivisor, CycleLengthMode, and their plumbing.

**Requirements:** R1, R6, R7

**Dependencies:** None (model fields are independent of the v2 math). **Lands atomically with U3** — every unit test links `sequencer_shared` (the whole app, incl. engine + edit page), so U2's deletions don't compile until U3 re-homes them. U2 and U3 share one green commit; their verifications are joint, not per-unit.

**Files:**
- Modify: `src/apps/sequencer/model/PhaseFluxSequence.h`, `src/apps/sequencer/model/PhaseFluxSequence.cpp`
- Test: `src/tests/unit/sequencer/TestPhaseFluxSequenceSerialization.cpp`, `src/tests/unit/sequencer/TestPhaseFluxSize.cpp`

**Approach:**
- Sequence level: `_divisor` (uint16) → `_period` (uint8 slot 0..7, default = the 1-bar slot); delete `CycleLengthMode` enum + field + accessors + the 0x20 bit-pack in the `_pitchRate` byte; `_lenNudge` → `_evenMorph` (0..64); `clockMultiplier` accessor kept (UI relabel only, U5).
- Stage level: delete `stageDivisor` bitfield (bits → spare); rename stageLen accessors to weight; fresh default per spec §3 (cells 1–4 weight 64 active, rest weight 0 + skip).
- write()/read(): new layout unconditionally; update field validation clamps.
- **Serialization test is a rewrite, not a patch**: `TestPhaseFluxSequenceSerialization.cpp` references `stageDivisor`/`StageDivisorSlot` in ~8 bodies (incl. the bit-collision probe). Replace each with a weight-field probe and re-probe the freed stage bits to confirm they are spare — a shallow delete leaves dead refs that fail to compile and regresses the layout coverage.

**Patterns to follow:** existing bitfield/accessor conventions in the same file.

**Test scenarios:**
- Happy path: default round-trip — fresh sequence serializes and restores byte-identical (period slot, weights, evenMorph, no cycleLength byte-pack).
- Happy path: non-default round-trip — every new/renamed field at a non-default value survives write/read.
- Edge: weight extremes 0 and 127 round-trip; period slots 0 and 7 round-trip.
- Edge: bit-collision probe rewritten for the new stage layout — the formerly-stageDivisor bits read back as spare.
- Error path: read clamps out-of-range period slot and evenMorph to valid range.

**Verification (joint with U3):** Serialization + size tests green; sim builds clean; `grep` finds no `stageDivisor`, `CycleLength`, or `lenNudge` outside docs.

---

### U3. Engine consumes the new model

**Goal:** `rebuildCumulativeTable` builds from period slot + weights + Even morph + Rate; skip's only effect is silence within its span.

**Requirements:** R2, R3, R7

**Dependencies:** U1, U2. **Lands atomically with U2** (see U2 — the build is only green once both land; also re-points the mechanical consumers: `PhaseFluxSequenceListModel.h` rows and the UI-side cumulative recompute at `PhaseFluxEditPage.cpp:1311`).

**Files:**
- Modify: `src/apps/sequencer/engine/PhaseFluxTrackEngine.cpp`, `src/apps/sequencer/engine/PhaseFluxTrackEngine.h`
- Test: `src/tests/unit/sequencer/TestPhaseFluxPerTickDerivation.cpp` (update if signatures shift), `src/tests/unit/sequencer/TestPhaseFluxTrackEngine.cpp`

**Approach:**
- Derive periodTicks (slot fraction × `measureDivisor()` × 100/rate) in the engine; call v2; delete the old call path, `fixedCycle` read, and divisor consumption (:201, :233, :242, scope path ~:1314 — also U5's scope renderers).
- Skip: route through the existing Fixed-mode silence path (span kept, output silent); confirm pulseCount=0 behavior unchanged (span kept, no pulses).
- `detectLayoutChange` cache fields: period slot, per-cell weights, evenMorph, rate, **and `measureDivisor()`** — it's live project state (tempo/time-sig), and v0.3 makes the whole period proportional to it, so a tempo change must trigger a rebuild or R3 sync drifts.
- **Tick-equivalence**: verify periodTicks fully re-homes the deleted stageDivisor tick contribution — at the default weights and 1-bar period the cycle length and boundaries match the v0.2 default-config result (stageDivisor was live in the old math, not inert).

**Test scenarios:**
- Happy path: period 1 bar, 4 active equal cells → slot boundaries at quarter-bar ticks; doubling Rate=50 doubles periodTicks.
- Integration: skipping a cell changes no boundary in the cumulative table (R2) and the engine emits no gate inside that span.
- Integration: weight edit on one cell changes only that cell's share (renormalized) while cycleTicks stays exactly periodTicks (R3).
- Integration: `measureDivisor` change (tempo/time-sig shift) → `detectLayoutChange` returns true → cycleTicks matches the new bar length (R3).
- Edge: tick-equivalence — default sequence's cycleTicks/boundaries equal the v0.2 default-config baseline (re-homes stageDivisor's contribution).
- Edge: all-zero weights → engine treats the pattern as idle (no schedule), no division by zero.
- Edge: min period slot × Rate 150 × full pulseCount — confirm behavior with the deleted min-cycle floor (pulses may legitimately collapse; assert no crash / defined result).

**Verification (joint with U2):** Full PhaseFlux ctest suite green; sim `sequencer` target builds clean.

---

### U4. Routing + param table

**Goal:** Period routable (discrete slot row replacing Divisor); globalPhase reads through routing; labels Rate / Even.

**Requirements:** R4

**Dependencies:** U2, U3 (the Period-override integration test needs the engine reading the routed period, which is U3).

**Files:**
- Modify: `src/apps/sequencer/model/ParamTablePhaseFlux.cpp`, `src/apps/sequencer/model/PhaseFluxSequence.h` (routed accessors), `src/apps/sequencer/model/RouteParamKey.h` (mint `ParamKey::Period`), `src/apps/sequencer/model/RouteResolve.h` (`targetToParamKey` case), `src/apps/sequencer/model/Routing.h` (`Target::Period` before `Last`), `src/apps/sequencer/model/RouteBrowse.h` (audit Clock band + `paramKeyToTarget`)
- Test: `src/tests/unit/sequencer/TestParamTablePhaseFlux.cpp`, `src/tests/unit/sequencer/TestRouteBrowse.cpp`

**Approach:**
- Mint `ParamKey::Period` (PhaseFlux block, append-on-use); add `Routing::Target::Period`, `RouteResolve::targetToParamKey` case → `ParamKey::Period`, and confirm `RouteBrowse::paramKeyToTarget` (reverse scan) resolves it — else a newly created Period route carries `Target::None` and is silently dead.
- Replace the PhaseFlux Divisor row with Period {0..7, Discrete}; relabel Clock Mult → "Rate"; relabel Len Nudge → "Even" **and change its route range from {−64,64} to {0,64}** (unipolar; a negative override would breach the morph contract).
- globalPhase accessor → routed float read on the existing `ParamKey::Phase` row (DiscreteMap float-override precedent).
- `RouteBrowse` Clock band hardcodes `Divisor` (still valid for other track modes — leave it); Period surfaces via the PhaseFlux engine page (`enginePageParams`); confirm the PhaseFlux engine-page row count.

**Test scenarios:**
- Happy path: param table rows match the new set (names, ranges incl. Even {0,64}, kinds) — update the parity test.
- Happy path: `paramKeyToTarget(ParamKey::Period)` ≠ `Target::None` (reverse-lookup resolves) — new assertion in TestRouteBrowse.
- Integration: a route override on Phase moves the effective globalPhase read while the base field is untouched; clamp 0..1 holds at override extremes.
- Integration: Period override slews across slot indices and the engine layout follows (via routedValueInt clamp 0..7).
- Edge: PhaseFlux `enginePageParams` row count asserted (Divisor gone, Period present, Even relabeled).

**Verification:** TestParamTablePhaseFlux + TestRouteBrowse + routing tests green.

---

### U5. Edit-page UI + W-TR transfer

**Goal:** Weight encoder with W-TR transfer toggle; Period/Rate/Even rows; Cycle row gone; ui-preview render for the weight display pick.

**Requirements:** R5, plus UI surfacing of R1–R3

**Dependencies:** U2, U3

**Files:**
- Modify: `src/apps/sequencer/ui/pages/PhaseFluxEditPage.cpp`, `src/apps/sequencer/ui/pages/PhaseFluxEditPage.h`, `src/apps/sequencer/ui/model/PhaseFluxSequenceListModel.h`
- Modify: `ui-preview/pages_phaseflux.py` (or the relevant renderer) for the weight-display variants
- Test: existing UI-adjacent tests if any break; otherwise `Test expectation: none — UI wiring verified via sim build + ui-preview renders`

**Approach:**
- P1 slot 0: Len → Weight; W-TR is a toggle, mirroring `IndexedSequenceEditPage::_durationTransfer` clamp logic; Δ moves to next-in-traversal nonzero-weight cell.
- W-TR toggle guard (exact, since the F-key handler overwrites `_selectedSlot` unconditionally): on F0 press at the correct set+page, if slot 0 is **already** selected, flip `_weightTransfer`; otherwise select slot 0 and clear `_weightTransfer`. Mirrors Indexed's "already in Duration mode → toggle" — no new state var.
- W-TR lifecycle: clear `_weightTransfer` on any set change (Left/Right), any topic-page change (F5 Next), and on `enter()`; gate the encoder's transfer path on the exact set+page+slot being active.
- W-TR with one nonzero cell (next == self): clamp Δ to 0, no-op — consistent with the clamp-stops-transfer rule, no message.
- Sequence list: also update `routingTarget()` for the renamed Divisor→Period row to return `Target::Period` (the row's own routing binding, separate from U4's table).
- Macro plumbing follows the Even rename: `shake()` MACRO slot randomizes 0..64 (was −64..64), `initTopic`/`zeroMacros` zero the renamed field, macro row label "Even" with a 0..64 value format.
- Sequence list: Divisor item → Period (slot names "1/4 bar".."32 bars"); CycleLength item removed; Clock Mult → Rate; macro page slot 3 LenN → Even.
- Render the three weight-display candidates (raw 0..127 / share % / musical fraction) in ui-preview, open for the owner, pick, then wire the chosen format. Include a **weight-0 grid-cell variant** in the render set: a zeroed cell (no span) must read differently from both a skipped cell (X-pattern) and a normal cell, since weight-0 and skip are now distinct states.
- Scope renderers: replace their divisor/cycle reads with the new period path (they already consume `cumulativeTicks`; only the inputs change).

**Execution note:** ui-preview render BEFORE presenting any display-format choice (standing display-proposal rule).

**Test scenarios:**
- Happy path: W-TR on cell i: encoder +Δ → weight_i +Δ, weight_next −Δ, Σ unchanged (assert via sequence state in a unit test if the edit path is testable headless; otherwise verified in sim).
- Edge: W-TR clamps — next at 0 stops the transfer; current at 127 stops it.
- Edge: W-TR skips zero-weight neighbors and wraps at the loop end (traversal order).
- Edge: W-TR with a single nonzero cell → encoder no-ops (next == self), weights unchanged.

**Verification:** Sim sequencer builds clean; ui-preview renders shown and format picked; manual sim pass over the edited pages.

---

### U6. Docs + task wiki

**Goal:** Fold v0.3 into the canonical PhaseFlux spec; update the task wiki.

**Requirements:** bookkeeping for all

**Dependencies:** U1–U5

**Files:**
- Modify: `docs/phaseflux-spec.md` (§3 timing superseded by v0.3 — declared period model; mark v0.2 timing prose superseded, reference the v0.3 spec)
- Modify: `.tasks/STATUS.md` (dated entry under the phaseflux workstream), `.tasks/phaseflux/task.md` (current-state snapshot)

**Approach:** Follow the existing STATUS.md entry style (dated, dense, commit-anchored). Wiki entry states: timing v0.3 landed, what died (stageDivisor, cycleLength, floor, LenN), what's new (Period, Weight, W-TR, Even, routable Phase), and that hardware audition is pending.

**Test scenarios:** Test expectation: none — documentation-only unit.

**Verification:** Spec and wiki read coherently against the shipped code.

---

## Risk Analysis & Mitigation

- **Branch mixing**: work lands on the current checkout (`refactor/routing-matrix`), which is code-complete for the routing overhaul and awaiting hardware audition. Risk: interleaving two big workstreams in one branch history. Mitigation: per standing policy no new worktree unless asked; commits are cleanly scoped per unit so they can be cherry-picked if the owner later wants separation. Flag at first commit.
- **Engine silence path**: skip-as-mute reuses the Fixed-mode path that has had less exercise than Adaptive. Mitigation: U3 integration scenarios target it directly.
- **Scope renderers**: three OLED scopes consume the timing table; U3/U5 keep them on `cumulativeTicks` so they follow automatically, but visual verification happens only in sim — one manual pass required.
- **Flash ceiling**: net code delta is negative (deletions: floor, mode, dead field, old math) — no pressure expected; STM32 release build checked at the end anyway.
- **Routing bridges**: the old `Routing::Target::Divisor` legacy mapping for PhaseFlux must not dangle after the Period swap — U4 explicitly audits `RouteFork`/bridge tables.

---

## Sources & References

- Origin spec: `docs/plans/2026-06-11-phaseflux-timing-v0.3-spec.md`
- v0.2 pipeline precedent: `docs/plans/2026-06-11-phaseflux-pipeline-reorder-design.md`
- Transfer gesture precedent: Indexed `_durationTransfer`; ER-101 "transferring pulses" (external design lineage)
- Family timing survey: in-spec §1 table (NoteTrack / DiscreteMap / Indexed / PhaseFlux)
