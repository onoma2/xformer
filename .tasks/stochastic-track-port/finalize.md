# Stochastic Track — Finalize Punch List

_Captured 2026-05-25 by a survey pass across stochastic engine/model/UI vs the
NoteTrack reference. Last gate before this task is considered complete. Items
ordered by severity — real bugs first, then performance-track parity gaps,
then dead code, then cross-page polish, then doc drift._

## Real bugs

- [ ] **`StochasticFeel` routing silently no-ops.** Enum is defined at
      `Routing::Target::StochasticFeel` (ID 67) and the routing UI exposes it,
      but `StochasticSequence::writeRouted` switch has no case for it. CV
      routing to Feel assigns but never writes the model. One-line fix.

## Inert UI

- [ ] **`_lock` toggle does nothing.** `StochasticTrack::_lock` is read/write
      from the Config page Lock entry but the engine never reads it. Per
      `LOCK-DESIGN-DEFERRED.md` this is by design pending Lock redesign.
      User-visible toggle that has no effect is misleading; either hide it
      until Lock lands or wire the deferred RNG-snapshot / A/B / tape design.

## Performance-track parity gaps (NoteTrack has, Stochastic doesn't)

- [ ] **No MIDI input path.** `TrackEngine` defines `receiveMidi`,
      `monitorMidi`, `clearMidiMonitoring`. NoteTrackEngine implements all
      three (the StepRecorder lives behind these). Stochastic doesn't
      override any of them. Tracks can't accept external MIDI to influence
      generation or get recorded into.
- [ ] **No `sequenceProgress()` override.** NoteTrackEngine returns a 0..1
      progress through the active sequence; UI uses this for progress bars.
      Stochastic returns the base-class default `-1.f`. Progress meter
      doesn't draw for stochastic tracks.
- [ ] **No `linkData()` override.** Stochastic tracks can't act as link
      source for downstream tracks. Track-linking propagation breaks at a
      stochastic node.
- [ ] **`_fillMuted` track field absent.** NoteTrack has a separate
      "mute the track when fill fires" toggle in addition to FillMode.
      Stochastic doesn't. Live performance gap.
- [ ] **No track-level `_rotate`.** NoteTrack has rotate at the track level
      (routed via `Routing::Target::Rotate`). Stochastic has rotate only at
      the sequence level. The two semantics are different; either confirm
      sequence-level is sufficient or add the track-level pair.
- [ ] **Missing routing targets:**
      - `StochasticPatienceMelody` — `PatienceRhythm` is routable, melody
        twin is not. Asymmetric — fix by adding the target alongside the
        rhythm one.
      - `StochasticRange` — bipolar 0..100 knob with rich CV-modulation
        potential (range sweeps as performance gesture). Not routable.
      - `StochasticMaskMelody` / `StochasticTiltMelody` — both are
        performance filters with obvious CV use. Not routable.
      - `StochasticBurstCount` / `StochasticBurstRate` / `StochasticLegato` /
        `StochasticRepeat` / `StochasticBurstHold` — design call required;
        some are LUT/enum (BurstCount/BurstHold) and may not want
        continuous CV.
- [ ] **`LaunchpadController` doesn't handle Stochastic.** Grep returns no
      `TrackMode::Stochastic` references in
      `ui/controllers/launchpad/LaunchpadController.cpp`. Launchpad users
      can't see or drive stochastic tracks. Big UX gap for the
      Launchpad-as-controller workflow.
- [ ] **`GeneratorPage` doesn't integrate Stochastic.** The Tier 4
      ContentSnapshot is wired at the model layer but no Generator A/B
      preview page surface for stochastic patterns. NoteTrack and CurveTrack
      get the full A/B + algo preview flow; Stochastic doesn't.
- [ ] **Python bindings missing.** `python/project.cpp` has no Stochastic
      Track exposure. Simulator-only concern; not on the device, but blocks
      Python-driven test/scripting.

## Dead code / wire-format waste

- [ ] **Dead engine includes.** `StochasticTrackEngine.h` includes
      `RecordHistory.h` and `StepRecorder.h`. Neither is used. Drop them.
- [ ] **Missing `override` on `activity()`.** `StochasticTrackEngine.h:50`
      declares `bool activity() const` without `override`. Compiles fine
      because the base is pure-virtual, but causes a persistent
      `-Winconsistent-missing-override` warning. Add the keyword.
- [ ] **Four reserved-zero bytes per sequence wire format**:
      `_last` (collapsed into Size), `_minDegree`, `_maxDegree`,
      `StochasticLevel` enum. Each writes a placeholder zero and reads it
      back as discard. ≈544 B per project file. Trivial cost but should be
      removed next time the project format intentionally breaks.
- [ ] **`event.slide` and `event.legato` bits dead.** Generator writes them
      false at storage time; cache never reads them. They survive as the
      `_lastStepContent` capture path via the cache-rolled cell, but the
      stored event bits themselves are not part of any audible path.
      Two bits per event × 64 events = 128 bits = 16 B per sequence
      available for repurpose. PHASE16 P5 has cell-flag pressure that could
      consume them.

## Cross-page inconsistencies

- [ ] **Quick-edit Random slot differs across pages.** Pitch and Duration
      pages handle `case 3` (visual step 12 = Random). Live page handles
      `case 6` (visual step 15 = Random). The Live placement is the
      Tuesday-style slot proposed in the design conversation; the Pitch /
      Duration slots predate that work. Decide if they should align — at
      least pick one convention and document it.
- [ ] **`_marblesBias` / `_marblesSpread` field naming carries "marbles".**
      The Bias/Spread law was replaced by a beta-distribution (no longer
      strictly Marbles-shaped). Field naming is stale relative to the
      semantic. Rename or accept (history note in a comment is fine).

## Documentation drift

- [ ] **`PITCH-LAW-FINAL.md` Step 3 says "octaves above currentOctave".**
      Implementation anchors the Range>50 field at octave 0 instead, so
      Contour can find candidates in both directions. Doc needs sync.
- [ ] **Knob inventory HTML mentions "marbles bell" / "Marbles bell" in
      some places.** Cosmetic but inconsistent with the now-landed
      beta-distribution language.

## Test coverage gaps

- [ ] No generator-level test for Burst Fit-curve math (only cache parity
      tests exercise it indirectly).
- [ ] No test for `undoRenewRhythm` / `undoRenewMelody`.
- [ ] No test for `captureLiveAsLoopRhythm` / `captureLiveAsLoopMelody`.
- [ ] No test for Patience log2 curve (`patienceLambda` is the unit under
      test, expose via friend or static and assert curve shape).
- [ ] No test for the engine `_undoSeed*` fields' capture/restore.

## Closure criteria

This task can be marked done when:
1. All "Real bugs" items resolved.
2. All "Inert UI" items either resolved or explicitly accepted (e.g.,
   hide Lock toggle until redesign lands).
3. Performance-track parity gaps reviewed; each item either landed or
   moved to a parking-lot task with explicit out-of-scope rationale.
4. Dead code + wire-format waste cleaned (free wins).
5. Cross-page inconsistencies aligned or documented as accepted.
6. Doc drift synced.
7. Test gaps closed for changes that already landed.
