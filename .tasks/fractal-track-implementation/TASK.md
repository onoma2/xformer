# FractalTrack Implementation — Task Status

Status: Blocked (awaits stochastic completion + RAM budget)
Priority: Low
Last revision: 2026-05-20 (substrate decided, MVP shape locked)

## Identity (single sentence)

FractalTrack is a step-sampled CV/gate child layer over one or two source tracks: it captures their gate+CV output once per step into a small grid (the trunk), then plays that grid back on loop with Proteus-style mutation evolving it at loop boundaries. On top of the trunk sit zero-storage branches (live math transforms — reverse, invert, transpose, etc.) and per-step ornamentation (trills, anticipations, mordents) that compose into a generative output. Identity: not a stochastic generator, not a step sequencer — a record-and-evolve child layer with trunk → branches → ornamentation as three composable layers, driven by TuesdayTrack-style track-level macro controls, not per-step programming.

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

## Core decisions (2026-05-20)

1. **Substrate: Mirror with step grid.** Small bounded buffer (default 64 cells × 16 bits
   = 128 B inline). One mirror grid per engine, never per pattern. No heap allocation.
2. **MVP behavior: pure mirror.** Continuous capture at step boundaries when not locked;
   playback at the same step rate; lock freezes the grid; per-pattern loop window/rotation/
   loopMode lenses the same grid into different playback slices. No mutation, no branches,
   no ornament at MVP.
3. **MVP serialization is complete.** Every known future field is declared in the model,
   initialized in `clear()`, serialized in `write()`, read with sanitize/clamp in `read()`
   from day one. Inert fields hold defaults; UI doesn't expose them yet. No layout churn
   later.
4. **No reseed.** FractalTrack is a child track — content originates from source or defaults
   to project root note when no capture exists. Mutations and branches evolve from that
   baseline. No internal RNG-driven content generation.
5. **Three-layer pipeline locked at MVP.** Engine has `play()` calling `mapLogicalToRead()`,
   `readTrunk()`, future-stub `applyBranchTransform()`, future-stub `applyOrnamentation()`.
   At MVP these are identity functions; later features fill them in without touching the
   tick loop.
6. **Loop-boundary hook exists at MVP.** `onLoopBoundary()` is empty at MVP. Sleep,
   mutation, patience, branch-phase-advance all land in this single hook later.
7. **Pattern lens, single buffer.** The grid survives pattern changes. Each
   FractalSequence holds different loop window / rotation / divisor / playMode etc.

## Active fields at MVP

### FractalSequence (per pattern × 17)

Standard Performer infrastructure: `divisor`, `clockMultiplier`, `playMode`, `firstStep`,
`lastStep`, `resetMeasure`.

Mirror-grid playback: `loopFirst`, `loopLast`, `rotate`, `loopMode` (Loop / Once).

### FractalTrack (track-wide)

`sourceA` (which track to mirror), `bufferLength` (16/32/64/128, default 64), `lock`,
`octave`, `transpose`, `slideTime`, `cvUpdateMode`.

## Reserved fields at MVP (declared, serialized, inert)

### FractalSequence

- `scale`, `rootNote` — pure mirror doesn't use them. Become active when transforms or
  scale-aware mutation land. Project root note is the fallback when trunk is empty.
- `runMode` — MVP plays forward only. Reverse/PingPong/Random land in `nextStepIndex()`.
- `clockSource` (Internal / External) — External lands as CV-scan playback later.
- `loopPhase` — fractional rotation, lands in `mapLogicalToRead()`.
- `loopBars` — bar-quantized loop length, lands in `effectiveLoopLast()`.
- `sleep` — between-loop step-count silence, lands in `onLoopBoundary()`.

### FractalTrack

- `sourceB` + `gateLogic` + `cvLogic` — two-source mixing. Lands by swapping
  `readResolvedSource()`.
- `recordMode` enum: Replace / Latch / Once. MVP = Replace. Once = auto-lock after one
  loop pass (explicit tape feel folded into mode).
- `punchMode` — Immediate / PunchIn. Lands in `capture()`.
- `recordTrigger` (`Routable<bool>`) — external arm. Composable with recordMode/punchMode.
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
    3. if (shouldAdvanceStep(tick)) {
        new = nextStepIndex(_stepIndex);
        if (crossedLoopBoundary(_stepIndex, new)) onLoopBoundary();
        _stepIndex = new;
        stepBoundary(tick, _stepIndex);
    }
    4. drainQueues(tick)
}

stepBoundary(tick, stepIndex) {
    if (!_lock) capture(stepIndex);
    play(tick, stepIndex);
}

capture(idx) {
    GateCV s = readResolvedSource();   // future: mix A+B per gateLogic/cvLogic
    _trunk[idx] = encode(s);
}

play(tick, logicalStep) {
    int readIdx = mapLogicalToRead(logicalStep);  // future: + loopPhase
    StepWord v = _trunk[readIdx];                   // future: fallback to root note if invalid
    v = applyBranchTransform(v, ...);                 // future: identity at MVP
    applyOrnamentation(v, logicalStep, tick);         // future: no-op at MVP
    scheduleOutput(v, tick);
}

onLoopBoundary() {
    // empty at MVP. Future: sleep, mutation, patience, branch advance, octave shift.
}

mapLogicalToRead(logical) {
    int window = effectiveLoopLast() - loopFirst() + 1;
    int rotated = (logical - loopFirst() + rotate()) % window;
    // future: add loopPhase fractional rotation here
    return loopFirst() + (rotated + window) % window;
}

readResolvedSource() {
    return readSourceA();
    // future: if (sourceB != -1) return mix(readSourceA(), readSourceB(), gateLogic, cvLogic);
}

shouldAdvanceStep(tick) { return relativeTick % stepDivisor == 0; }
// future: External branch reads routedScan, returns edge-detected scan position

nextStepIndex(curr) { return (curr + 1) % bufferLength; }
// future: runMode dispatches forward/reverse/pingpong/random

effectiveLoopLast() { return loopLast(); }
// future: loopBars > 0 → derive from bar count

snapshotCell(stepIndex) { /* engine API for UI-triggered capture-right-now */ }
```

## Buffer encoding

16 bits per cell:
- 14 bits CV (signed; ~6 cents per LSB across 10 V range — clean V/Oct)
- 1 bit gate
- 1 bit valid (0 = uncaptured, default to root note on playback)

Buffer max 128 cells × 2 bytes = 256 B inline. Default 64 cells = 128 B inline.

## RAM accounting

Model:
- FractalSequence ≈ 30 B with all reserved fields. × 17 patterns = 510 B per track.
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
- Inline `_trunk[CONFIG_FRACTAL_MAX_STEPS]` buffer
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
- Record Mode (Replace; Once and Latch greyed/hidden at MVP)
- Lock
- Per-pattern items from active set: divisor, clockMultiplier, playMode, firstStep, lastStep,
  resetMeasure, loopFirst, loopLast, rotate, loopMode
- Octave, Transpose, SlideTime, CvUpdateMode
- Clear action

Reserved fields exist in model but list model hides them. Add UI items when each feature
lands.

### Phase 4 — Hardware verification gate

Verify on STM32:
1. Create FractalTrack, assign NoteTrack as sourceA
2. Hear FractalTrack mirror source at its own step rate
3. Lock, hear playback continue from the frozen grid
4. Switch patterns, hear different loop windows slice the same grid
5. resetMeasure restart works on bar boundary
6. slideTime smooths playback CV
7. Project save/load round-trips all reserved fields correctly
8. `sizeof(FractalTrack)` under 9544 B, `sizeof(FractalTrackEngine)` under 912 B

**Stop here.** Hand off to user for hardware confirmation before any post-MVP work.

## Post-MVP feature gates (suggested landing order)

Each is independently scoped — lands inside an existing function or reserved field.

1. **Phase offset** — one line in `mapLogicalToRead()`.
2. **Path / playback order modes** — extends `nextStepIndex()` via `runMode`.
3. **Two-source mixing** — swap `readResolvedSource()` to honor sourceB + gateLogic + cvLogic.
4. **CV-scan playback (clockSource=External)** — extends `shouldAdvanceStep()` +
   `nextStepIndex()`. Reads `routedScan`. Bypasses generative lifecycle while active.
5. **Bar-quantized loop length** — extends `effectiveLoopLast()`.
6. **Recording variants (Latch, Once, PunchIn, recordTrigger)** — extends `capture()`.
7. **Snapshot capture action** — wire UI to existing `snapshotCell()`.
8. **Sleep** — first occupant of `onLoopBoundary()`. Step-count silence after each loop pass.
9. **Density / Tilt** — new mask in `play()`.
10. **Proteus mutation + patience** — also in `onLoopBoundary()` alongside Sleep.
11. **Branches** — wrap trunk-read in `play()` with transform function. Add trunk/branch
    phase counters (reserved engine state).
12. **Ornamentation** — fill `applyOrnamentation()` stub with anticipation/mordent/run.
13. **Snapshot bank** — only feature requiring new structure. Plan separately.
14. **Visual page** — buffer waveform, loop window markers, lock indicator.

## Dependencies

| Task | Relationship |
|------|--------------|
| `resource-optimization` | **Blocks.** SRAM at 91.4%; FractalTrack needs ~600 B engine + ~570 B model headroom. |
| `stochastic-track-port` | **Watch.** Active on `feat/stochastic`. Higher priority, may consume remaining headroom. Borrow Sleep mechanics from its lifecycle hook structure. |

## Blocks

Nothing directly blocked.
