# FractalTrack Implementation — Task Status

Status: Blocked (awaits stochastic completion + RAM budget)
Priority: Low
Last revision: 2026-05-22 (command/rule model contract added; capture semantics are model rules)

## Identity (single sentence)

FractalTrack is a parent-dependent command/rule sequencer over sampled melodic CV/gate material: parent tracks provide the motif, the engine keeps a small volatile trunk of section-sampled pitch/gate cells, and the model owns the rules that decide how that trunk is captured, read, sliced, locked, and later evolved. Identity: not a stochastic generator, not a normal note sequencer, not a persistent recorder, and not a high-fidelity CV recorder — a child melodic motif mangler with parent material -> trunk -> rule sequence -> output.

## Authoritative documents

- `docs/fractal-track-options-comparison.html` — **canonical architectural reference.**
  Substrate comparison across seven implementation options, end-product-aware MVP path
  with full field tables and engine function structure, late-stage feature compatibility,
  recommended landing order.
- `docs/superpowers/specs/2026-05-17-fractal-track-design.md` — historical spec.
  Identity summary at top is current; body describes the broader feature surface and
  reference porting. Superseded by the HTML for substrate + MVP decisions.
- `docs/superpowers/specs/2026-05-17-fractal-advanced-research.md` — post-MVP feature ideas.
- `docs/performer-architecture.md` — engine/timing/IO reference. Read for `cvOutput`/
  `gateOutput`, tick order guarantees, NoteTrack timing patterns, output routing.
- `.tasks/fractal-track-implementation/DICTIONARY.md` — vocabulary and ownership contract.
- `temp-ref/norns/compass/compass.lua` — conceptual reference only. Compass separates
  sampled material from a command sequence; Fractal applies the same split to CV/gate
  material and model-owned capture/read rules. Do not port its Softcut or pattern_time code.

## Core decisions (2026-05-20)

1. **Substrate: Mirror with section grid.** Small bounded buffer (default 64 cells × 16 bits
   = 128 B inline). One mirror grid per engine, never per pattern. No heap allocation.
   Each cell stores source CV, gate length, and valid state.
2. **Scope: melodic material mirror.** Fractal captures enough parent information to mirror
   and mangle a melodic motif: pitch/CV at Fractal section boundaries plus gate length at
   Fractal resolution. It intentionally does not chase every source CV movement, modulation
   wiggle, parent retrigger, Teletype tick event, or continuous curve inside a section.
   If a future feature needs high-fidelity CV recording, that is a different product or a
   separate post-MVP mode.
3. **Parent output is opaque.** Fractal does not know whether a parent value came from a
   parent sequencing event, a muted output, routing, a reset measure, a held note, an LFO,
   Teletype, or any other source-internal event. It reads only the parent's current logical gate/CV.
4. **Model owns command/capture rules.** Capture is not engine plumbing. It is one
   Fractal rule family alongside loop/read rules: Replace, Latch, Once, PunchIn,
   record extent, record trigger, and quantize-on-capture all describe how parent
   material is allowed to write the trunk.
5. **Engine owns trunk bytes and rule execution.** The engine reads parent logical outputs,
   measures gate length, writes/clears trunk cells, schedules playback, and applies the
   current model rules. The trunk itself is volatile engine state, not serialized model data.
6. **MVP behavior: mirror through rules.** Default rule set is Replace over the full record
   extent while unlocked; playback reads the loop window at the Fractal section rate. Lock
   write-protects the trunk; per-pattern loop window/rotation/loopMode lenses the same trunk
   into different playback slices. No mutation, no branches, no ornament at MVP.
7. **MVP serialization includes active command rules.** Active model fields are declared,
   initialized in `clear()`, serialized in `write()`, read with sanitize/clamp in `read()`.
   Future fields are only reserved when their semantics are already part of the command
   contract. Inert fields are hidden.
8. **No reseed.** FractalTrack is a child track — content originates from source or defaults
   to project root note when no capture exists. Mutations and branches evolve from that
   baseline. No internal RNG-driven content generation.
9. **Four-stage playback pipeline remains the long-term playback shape.** Engine has `play()` calling
   `mapLogicalToRead()` with fixed section phase, `readTrunk()`, future-stub
   `applyBranchTransform()`, future-stub `applyOrnamentation()`. Section phase is
   architectural and always present; neutral defaults make it an identity transform.
10. **Loop-boundary hook exists at MVP.** `onLoopBoundary()` is empty at MVP. Sleep,
   mutation, patience, branch-phase-advance all land in this single hook later.
11. **Pattern lens, single buffer.** The grid survives pattern changes. Each
   FractalSequence holds different loop window / rotation / divisor / playMode etc.

## Active fields at MVP

### FractalSequence (per pattern × 17)

Standard timing infrastructure: `divisor`, `clockMultiplier`, `playMode`, `firstSection`,
`lastSection`, `resetMeasure` (field names can map to existing Performer helpers later).

Mirror-grid playback: `loopFirst`, `loopLast`, `rotate`, `loopMode` (Loop / Once).

Capture rules: `recordFirst`, `recordLast`, `recordMode` (Replace / Latch / Once),
`punchMode` (Immediate / PunchIn), `recordQuantize` (Off / On; inactive until scale is active).

### FractalTrack (track-wide)

`sourceA` (which track supplies parent material), `bufferLength` (16/32/64/128, default 64),
`lock`, `recordTrigger` (`Routable<bool>` external write arm), `octave`, `transpose`,
`slideTime`, `cvUpdateMode`.

## Reserved fields at MVP (declared, serialized, inert)

### FractalSequence

- `scale`, `rootNote` — pure mirror doesn't use them except root fallback. Become active
  when recordQuantize, transforms, ornamentation, or scale-aware mutation land.
  Project root note is the fallback when trunk is empty.
- `runMode` — MVP plays forward only. Reverse/PingPong/Random land in `nextSectionIndex()`.
- `clockSource` (Internal / External) — External lands as CV-scan playback later.
- `loopPhase` — fractional rotation, lands in `mapLogicalToRead()`.
- `sectionPhase[4]` — fixed four-section playback lens. Always present; neutral defaults
  mean no bend. Section boundaries derive from the effective loop window and distribute
  remainder cells front-to-back.
- `loopBars` — bar-quantized loop length, lands in `effectiveLoopLast()`.
- `sleep` — between-loop section-count silence, lands in `onLoopBoundary()`.

### FractalTrack

- `sourceB` + `gateLogic` + `cvLogic` — two-source mixing. Lands by swapping
  `readResolvedSource()`.
- `routedScan` (`Routable<float>`) — CV source for clockSource=External.
- `complexity`, `patience`, `mutationProb`, `octaveShiftProb` — Proteus mutation macros.
- `density`, `tilt` — playback mask.
- `branchCount`, `trunkCycles`, `pathType`, `orderMode`, `branchTransformFlags` — branches.
- `ornamentProb`, `ornamentMode` — ornamentation.

### Explicitly NOT reserved

- **Snapshot bank.** Storage isn't a few bytes — it's another whole grid per snapshot. If
  added later, plan separately. Possibly heap.
- **Reseed field/state.** No reseed by design.

## Engine structure (locked at MVP, hooks for late features)

```
tick(uint32_t tick) {
    1. divisor + relativeTick math (NoteTrack pattern)
    2. resetMeasure boundary check → reset()
    3. if (shouldAdvanceSection(tick)) {
        new = nextSectionIndex(_sectionIndex);
        if (crossedLoopBoundary(_sectionIndex, new)) onLoopBoundary();
        _sectionIndex = new;
        sectionBoundary(tick, _sectionIndex);
    }
    4. drainQueues(tick)
}

sectionBoundary(tick, sectionIndex) {
    executeCaptureRule(tick, sectionIndex);
    play(tick, sectionIndex);
}

executeCaptureRule(tick, sectionIndex) {
    if (lock()) return;
    if (!captureRuleAllowsWrite(tick, sectionIndex)) return;
    int writeIdx = mapLogicalToRecord(sectionIndex);
    capture(writeIdx);
}

capture(writeIdx) {
    GateCV s = readResolvedSource();   // future: mix A+B per gateLogic/cvLogic
    _trunk[writeIdx] = encode(s);
}

play(tick, logicalSection) {
    int readIdx = mapLogicalToRead(logicalSection);  // loopPhase + fixed 4-section phase
    CellWord v = _trunk[readIdx];                   // future: fallback to root note if invalid
    v = applyBranchTransform(v, ...);                 // future: identity at MVP
    applyOrnamentation(v, logicalSection, tick);      // future: no-op at MVP
    scheduleOutput(v, tick);
}

onLoopBoundary() {
    // empty at MVP. Future: sleep, mutation, patience, branch advance, octave shift.
}

mapLogicalToRead(logicalSection) {
    int window = effectiveLoopLast() - loopFirst() + 1;
    int rotated = (logicalSection - loopFirst() + rotate()) % window;
    // apply loopPhase and fixed 4-section phase here
    return loopFirst() + (rotated + window) % window;
}

readResolvedSource() {
    return readSourceA();
    // future: if (sourceB != -1) return mix(readSourceA(), readSourceB(), gateLogic, cvLogic);
}

shouldAdvanceSection(tick) { return relativeTick % sectionDivisor == 0; }
// future: External branch reads routedScan, returns edge-detected scan position

nextSectionIndex(curr) { return (curr + 1) % bufferLength; }
// future: runMode dispatches forward/reverse/pingpong/random

effectiveLoopLast() { return loopLast(); }
// future: loopBars > 0 → derive from bar count

mapLogicalToRecord(logicalSection) {
    // MVP: record extent maps 1:1 to trunk positions and wraps at recordLast.
    // Future: punch/once/latch variants share this write-position mapper.
}

snapshotCell(sectionIndex) { /* engine API for UI-triggered capture-right-now */ }
```

## Buffer encoding

16 bits per cell:
- 11 bits CV (signed/fixed; ~5.9 cents per LSB across 10 V range)
- 4 bits gate length
  - 0 = rest / gate off
  - 1 = trigger / very short gate
  - 2..14 = proportional gate length within the Fractal section
  - 15 = full-section gate / tie
- 1 bit valid (0 = uncaptured, default to root note with gate off on playback)

Gate length is captured at Fractal resolution, not source-event resolution. At section start,
the engine samples CV and source gate. If gate is low, `gateLen=0`. If gate is high, the
cell is written with a provisional full-section value; a source falling edge inside the current
Fractal section updates that cell to the nearest 1..14 length bucket. If no fall occurs before
the next Fractal section, the cell remains 15.

Buffer max 128 cells × 2 bytes = 256 B inline. Default 64 cells = 128 B inline.

## RAM accounting

Model:
- FractalSequence includes fixed four-section phase fields. × 17 patterns remains well
  below the NoteTrack container gate.
- FractalTrack track-wide ≈ 60 B with all reserved.
- Total per FractalTrack ≈ 570 B. NoteTrack gate = 9544 B. Comfortable margin.

Engine container:
- TrackEngine base ≈ 100 B
- Sequence state + queues ≈ 200 B
- Counters + flags ≈ 50 B
- Inline buffer ≈ 256 B max, 128 B default
- Engine RNG, slide state ≈ 30 B
- Total ≈ 600 B at max buffer. TeletypeTrackEngine gate = 912 B. Comfortable.

Worst case 8 Fractal tracks × 600 B = 4.8 KB CCMRAM. Headroom ≈ 10 KB.

## Phased Implementation Plan

### Phase 0 — Contract cleanup and stage gates

Purpose: apply the stochastic-track lessons before writing the Fractal model. Stochastic started as a port, then needed repeated ownership, vocabulary, control-law, simplification, hardware, and adversarial-review passes. Fractal should not repeat that journey after code exists.

#### Lessons carried over from Stochastic

1. **Product identity first.** Fractal is a child mirror layer with trunk -> section phase -> branches -> ornamentation. Do not expose reference-module names as user modes, and do not let future mutation/branch ideas turn MVP into a standalone generator.
2. **Melodic mirror before fidelity.** Fractal stores a section-sampled melodic fact, not a
   full CV automation trace. Do not add per-tick capture or continuous recorder semantics
   to satisfy edge cases from curve/modulator/Teletype sources.
3. **Ownership before UI.** Fractal trunk is volatile engine state. Pattern lenses are sequence state. Parent/source config and track offsets are track state. UI must follow that split.
4. **Store evaluated material, not reasons.** The trunk cell stores the playback fact: valid, gate length, CV. It does not store source control intent, branch reason, or mutation provenance.
4. **Temporary list access is the MVP UI.** Final graphics wait until capture, lock, loop lens, RAM, and hardware behavior are proven.
5. **Runtime state must have exact scope.** Define what survives pattern change, transport restart, buffer length change, track mode change, project save/load, and power cycle.
6. **Capture is a model rule.** Do not bury write semantics in the engine as "unlocked means record." Replace/Latch/Once/PunchIn/record extent/record trigger are the command layer that decides when parent material may rewrite trunk cells.
7. **Reserved fields must be honest.** Declared serialized fields may be inert, but hidden. Do not expose inert routing/UI controls as if they work.
8. **Macro controls must decompose into real model fields.** Future branch/evolution controls must be edits over the same trunk/lens/rule model, not parallel hidden systems.
9. **Hardware is the truth gate.** Compile and simulator checks are not enough for capture timing, gate shape, source aliasing, lock feel, and loop slicing.

#### Phase 0 required edits

- Reconcile this `TASK.md` and `DICTIONARY.md` so there is one current contract. Remove or mark historical language that still assumes a separate manual record-armed phase.
- Define trunk cell playback exactly:
  - valid bit behavior
  - gate-low behavior for invalid cells
  - gate duration law for valid cells
  - CV fallback for uncaptured cells
- Define melodic capture command behavior exactly:
  - capture happens at Fractal section boundaries when model rules allow a write
  - lock blocks capture and trunk mutation
  - default Replace rule captures continuously while unlocked
  - Latch, Once, PunchIn, and recordTrigger are rule variants, not separate recorder modes
  - source motion between Fractal section boundaries is intentionally ignored except for
    gate-fall measurement inside the current Fractal section
  - parent provenance is opaque: Fractal does not inspect parent sequencing state, mutes,
    routing causes, reset-measure events, or held-note causes
  - source reads are logical engine outputs, lower-index guarded in engine as well as UI
- Define window hierarchy before model fields:
  - record extent (where writes land)
  - active loop window (what playback reads)
  - future mutation zone
  - invariant chain and clamp rules
- Define state lifetime:
  - trunk survives pattern change and transport reset
  - trunk is cleared on track mode change, buffer length change, explicit clear, power cycle
  - queued output behavior on pattern change/reset is specified
- Mark every field as one of:
  - active in MVP
  - reserved serialized and inert
  - deferred, not reserved
- Define stage gates below as acceptance criteria before Phase 1 begins.

#### Stage gates

**Gate A — Contract**
- `TASK.md` and `DICTIONARY.md` agree.
- No stale recording/capture language implies a separate manual recorder phase.
- Capture rules are model-owned; trunk storage is engine-owned.
- Melodic mirror scope is explicit; tick-rate CV fidelity is out of scope.
- Trunk ownership and pattern lens ownership are unambiguous.
- Every field has active/reserved/deferred status.

**Gate B — RAM**
- STM32 release build is the source of truth.
- `FractalTrack` stays below the current track container gate.
- `FractalTrackEngine` stays below the current engine container gate.
- `.data`, `.bss`, `.ccmram_bss`, changed model sizes, and changed engine sizes are recorded.

**Gate C — Serialization**
- Round-trip test covers all model fields.
- Invalid enum/index values sanitize.
- Reserved fields round-trip but do nothing.
- Trunk buffer is not serialized.

**Gate D — Engine Semantics**
- No STL containers in tick path.
- No tick-path allocation.
- Source reads are guarded in engine.
- Pattern change cannot replay stale queued output.
- Buffer length change clears trunk deterministically.

**Gate E — Hardware MVP**
- Mirror source at Fractal grid rate.
- Lock/unlock is audible.
- Loop window and rotate are audible.
- Pattern lens behavior is audible over the same trunk.
- Gate behavior is musically acceptable.
- Save/load preserves config but not trunk.

**Gate F — UI Truth**
- UI shows active behavior only.
- Reserved controls are hidden.
- Source invalidity is impossible or visible.
- Lock/capture status is obvious.

Phase 1 must not start until Gate A is complete.

### Phase 1 — Model with full serialization

Files:
- NEW `src/apps/sequencer/model/FractalSequence.h`
- NEW `src/apps/sequencer/model/FractalTrack.h`
- EDIT `src/apps/sequencer/model/Track.h` — add Fractal to TrackMode enum, Container, union
- EDIT `src/apps/sequencer/model/Routing.h` — add `FractalRoutedScan`, `FractalRecordTrigger`,
  and per-sequence routables for loopFirst/loopLast/rotate as needed; reserve target range
  with sentinel markers.
- EDIT `src/apps/sequencer/engine/Engine.h` — add `FractalTrackEngine` to TrackEngineContainer typedef
- EDIT `src/apps/sequencer/engine/Engine.cpp` — case `TrackMode::Fractal` in creation switch

Field set: all active + reserved fields from §"Reserved fields at MVP" above.

Acceptance:
- STM32 release build compiles
- `sizeof(FractalTrack)` probe reports value
- Serialization round-trip test passes (write → clear → read → all fields match)

### Phase 2 — Engine with hooks but minimal behavior

Files:
- NEW `src/apps/sequencer/engine/FractalTrackEngine.h`
- NEW `src/apps/sequencer/engine/FractalTrackEngine.cpp`

Behavior:
- Inline `_trunk[CONFIG_FRACTAL_MAX_SECTIONS]` buffer
- Tick loop structured per §"Engine structure" — every helper exists, future-feature
  helpers return MVP defaults
- `capture()` reads sourceA only via `readResolvedSource()`
- `play()` calls `mapLogicalToRead()` then reads trunk (no transform); invalid cells output
  root note CV with gate=off
- Lock toggle works
- `snapshotCell()` engine API exists, no UI yet

Acceptance:
- Assign a NoteTrack to sourceA in simulator
- Observe FractalTrack output mirroring source at its own divisor
- Lock, observe playback freezes on current snapshot
- Unlock, observe capture resumes
- Switch patterns, observe same grid with different loop windows

### Phase 3 — List UI

Files:
- NEW `src/apps/sequencer/ui/model/FractalTrackListModel.h`
- EDIT `src/apps/sequencer/ui/pages/TrackPage.cpp` — route to FractalTrackListModel

Items exposed at MVP (only active fields):
- Source A (clamped to track indices < this track's)
- Buffer Length (16/32/64/128)
- Record Mode (Replace active; Once/Latch may be hidden until implemented)
- Record Extent (recordFirst/recordLast if UI budget allows; otherwise full buffer default)
- Lock
- Per-pattern items from active set: divisor, clockMultiplier, playMode, firstSection, lastSection,
  resetMeasure, loopFirst, loopLast, rotate, loopMode
- Octave, Transpose, SlideTime, CvUpdateMode
- Clear action

Reserved fields exist in model but list model hides them. Add UI items when each feature
lands.

### Phase 4 — Hardware verification gate

Verify on STM32:
1. Create FractalTrack, assign NoteTrack as sourceA
2. Hear FractalTrack mirror source at its own section rate
3. Lock, hear playback continue from the frozen grid
4. Switch patterns, hear different loop windows slice the same grid
5. resetMeasure restart works on bar boundary
6. slideTime smooths playback CV
7. Project save/load round-trips all reserved fields correctly
8. `sizeof(FractalTrack)` under 9544 B, `sizeof(FractalTrackEngine)` under 912 B

**Stop here.** Hand off to user for hardware confirmation before any post-MVP work.

## Post-MVP feature gates (suggested landing order)

Each is independently scoped — lands inside an existing function or reserved field.

1. **Path / playback order modes** — extends `nextSectionIndex()` via `runMode`.
2. **Two-source mixing** — swap `readResolvedSource()` to honor sourceB + gateLogic + cvLogic.
3. **CV-scan playback (clockSource=External)** — extends `shouldAdvanceSection()` +
   `nextSectionIndex()`. Reads `routedScan`. Bypasses generative lifecycle while active.
4. **Bar-quantized loop length** — extends `effectiveLoopLast()`.
5. **Capture rule variants (Latch, Once, PunchIn, recordTrigger)** — extends
   `captureRuleAllowsWrite()` and `mapLogicalToRecord()`.
6. **Snapshot capture action** — wire UI to existing `snapshotCell()`.
7. **Sleep** — first occupant of `onLoopBoundary()`. Section-count silence after each loop pass.
8. **Density / Tilt** — new mask in `play()`.
9. **Proteus mutation + patience** — also in `onLoopBoundary()` alongside Sleep.
10. **Branches** — wrap trunk-read in `play()` with transform function. Add trunk/branch
    phase counters (reserved engine state).
11. **Ornamentation** — fill `applyOrnamentation()` stub with anticipation/mordent/run.
12. **Snapshot bank** — only feature requiring new structure. Plan separately.
13. **Visual page** — buffer waveform, loop window markers, lock indicator.

## Dependencies

| Task | Relationship |
|------|--------------|
| `resource-optimization` | **Blocks.** SRAM at 91.4%; FractalTrack needs ~600 B engine + ~570 B model headroom. |
| `stochastic-track-port` | **Watch.** Active on `feat/stochastic`. Higher priority, may consume remaining headroom. Borrow Sleep mechanics from its lifecycle hook structure. |

## Blocks

Nothing directly blocked.

## Shipped — 2026-06-27

Full track on `feat/fractal-track`. RAM concern resolved: `FractalTrackEngine` is **888 B ≤ the 912 union gate** (net-zero under the discriminated-union Container). Every in-scope KD wired — **presented == built**; commit list in the STATUS.md fractal block. Engine capture path + `recordGate()` overrides are **build-verified only** (headless `Engine` test seam still absent — the one open infra task). LoopMode Once deferred per spec; DiscreteMap/PhaseFlux decay-timer `activity()` noted in memory `project_fix_indexed_mute` for a future ghost-source refinement.
