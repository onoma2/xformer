# FractalTrack Dictionary of Truth

This document is the immutable vocabulary and ownership contract for the FractalTrack track mode. Update it only when the model topology changes intentionally.

**Identity (2026-05-22 revision):** FractalTrack is a step-sampled CV/gate **child layer**
over one or two source tracks. It captures source gate+CV into a small bounded step grid
(the trunk, encoded as CV plus gate length), plays the grid back on loop, evolves it via mutation at loop boundaries, and
exposes fixed four-section phase, branches, and ornamentation as zero-storage playback
lenses on top. Driven by TuesdayTrack-style track-level macros, not per-step programming.
Substrate: Mirror with step grid. Canonical architectural reference:
`docs/fractal-track-options-comparison.html`.

## Provenance

- **Proteus spec** contributes loop-boundary lifecycle (complexity, patience, mutation, octave shift), deterministic density/rest-order thinning, sleep/boredom, and mutation zone anchors.
- **Hallucigenia (VCV Rack)** contributes mutation history concept, selection pressure, and evolution depth -- but FractalTrack applies these to per-loop-cycle step selection, not per-step independent rolls.
- **Bloom v2 (Qu-Bit)** contributes trunk/branch/ornamentation three-layer architecture, path navigation, branch transforms (reverse/inverse/transpose/mutate/randomize), order playback modes, per-step modifiers (ratchet, slew), and per-step classical ornamentation.
- **Vinx LogicTrackEngine** contributes per-tick parent engine resolution pattern and 2-input gate/CV mixing logic (8 gate modes, 8 CV modes).
- **Performer** contributes the product contract: step-aligned timing, track/page shell, divisor/scale/root conventions, routing infrastructure, Container/Routable patterns, and STM32 RAM gates.

Do not expose module names as final user-facing modes. The track should feel Performer-native.

## Global Rules

- User-facing labels should be short, preferably one word.
- Internal names should follow existing codebase style.
- The trunk is the single ground truth buffer. Section phase and branches are playback
  transforms over trunk, not separate buffers.
- Branches do NOT mutate the buffer. Only the evolution system (during trunk phase) mutates the buffer.
- Playback order is forward through the active loop window unless overridden by `runMode`.
- Fixed four-section phase is applied in `mapLogicalToRead()` before the trunk read.
- Ornamentation is applied AFTER section phase and branch transform resolve CV+gate.
- Phase 4 evolution (trunk mutation) and Phase 5 branches are independent — branches read whatever trunk exists, evolved or not.
- Lock blocks trunk buffer mutation only: evolution, recording, and Sleep-bounded recapture. Branch transforms and ornamentation continue while locked.
- All lower-index engine types supported as parent sources. Fractal reads `gateOutput(0)` and `cvOutput(0)` from any engine, including Teletype.
- **Child track invariant.** Content originates from source captures or defaults to the
  project root note. There is no internal RNG-driven content generation. No reseed
  feature. Mutations and branches evolve from a source-captured or root-defaulted
  baseline.
- **Trunk is inline, not heap.** Engine-state buffer at 128 cells × 2 bytes max = 256 B.
  Single buffer per engine, never per pattern.
- **Section phase is architectural.** There are always four phase sections per effective
  loop window. No user enable flag. Neutral section values make the transform identity;
  the engine may fast-path neutral phase internally.
- **MVP serialization is complete.** All known future fields are declared and serialized
  from day one. Inert fields hold sensible defaults; UI exposes only active fields.

## Parent

- `Source A`, `Source B`: track indices (1-8) whose gate+CV output is recorded. Any engine type is supported as parent, including Teletype, NoteTrack, Stochastic, Tuesday, etc.
- Cycle safety: source tracks must have lower indices than the FractalTrack index (enforced by list model clamping).
- Gate Logic (8 modes): A, B, AND, OR, XOR, NAND, Random.
- CV Logic (8 modes): A, B, Sum, Avg, Min, Max, Random.
- Recording reads `gateOutput(0)` and `cvOutput(0)` from source engines each step.

Parent invariants:
- Source data is read at tick time. No snapshot of past data.
- Invalid source (index >= fractal index) silently produces idle output.
- Parent changes propagate immediately on next tick. FractalTrack does not cache parent state.
- No parent link loops (A links fractal to B, B links fractal to A).

## Record

- `recordMode`: Replace (default) / Latch / Once.
  - **Replace**: every step boundary while not locked, overwrite buffer[stepIndex] with source gate+CV.
  - **Latch**: only write to buffer when source gate=1. Silent steps keep their previous content. Enables surgical per-step overdubbing.
  - **Once**: capture one loop pass starting from current step, then auto-lock. The explicit "tape feel" — press record, hear one loop captured, recorder stops. Lock can be released to record again.
- `punchMode`: Immediate (default) or PunchIn.
  - **Immediate**: recording begins on first step boundary after unlock.
  - **PunchIn**: after unlock, engine enters a wait state and continues playback. Recording starts on the first step boundary where resolved source gate=1.
- `recordTrigger` (`Routable<bool>`): external arm. When routed, capture only fires while
  this CV is high. Composable with recordMode and punchMode — e.g. recordMode=Latch +
  recordTrigger gates surgical overdub by an external CV.
- `loopMode`: Loop (default) or Once.
  - **Loop**: at `loopLast`, wrap to `loopFirst` and continue.
  - **Once**: after reaching `loopLast`, gates go off and step advancement stops. Transport restart or pattern recall resets.
- `recordQuantize`: Off (default) or On.
  - **Off**: source CV written to buffer as-is.
  - **On**: source CV is quantized to the active FractalSequence scale before writing. Gate is unchanged. Requires `scale` to be active (scale becomes active when transforms or quantize-on-record land — see Scale Activation).
- `snapshotCell(stepIndex)`: engine API that captures source immediately (not at next step boundary) and writes to the given cell. UI action item, no model field. Like sample-and-hold on demand.

Record invariants:
- PunchIn respects source gate from the *resolved* parent (after Gate/CV Logic mixing). If combined gate=0, punch never fires.
- Latch mode: a muted step stays muted if the source gate is off at that step position. Only explicitly gated source steps trigger writes.
- RecordMode and PunchMode are orthogonal: Replace+PunchIn = punch in and overwrite; Latch+PunchIn = punch in and latch; Latch+Immediate = every step in the loop gets a chance to latch if gate fires.
- recording is always overwrite of the 32-bit step word. There is no blend/lerp (deferred to post-MVP).
- LoopMode=Once is a playback mode -- it does not affect recording. You can record a loop in Once mode (record fills the buffer, playback stops after one pass).
- RecordQuantize applies on capture only. Playback is always raw buffer value. Quantization-on-playback is deferred.

## Record Extent

- `recordFirst`, `recordLast`: the recording target window within the buffer. Default `recordFirst=0`, `recordLast=bufferLength-1`.
- When recording, the engine writes to `recordFirst..recordLast` and wraps from `recordLast` back to `recordFirst`, looping within the extent.
- The loop window `loopFirst..loopLast` is a subset of the record extent. The user records into the full extent, then chooses a narrower loop window for playback.
- Clear buffer only clears the extent `recordFirst..recordLast`, not the full buffer length.
- `loopBars` bar-quantized mode derives the loop window relative to `recordFirst`.

Record extent invariants:
- `recordFirst <= loopFirst <= loopLast <= recordLast <= bufferLength-1` (enforced by list model).
- Recording targets `recordFirst..recordLast`. The record position wraps from `recordLast` to `recordFirst`, not from `bufferLength-1` to 0.
- Steps outside the loop window within the record extent are stored but muted during playback.
- Record extent is model state (serialized). Buffer content within the extent is volatile engine state (not serialized).

## Buffer

- `_trunk[]`: **inline** array of `uint16_t`, one per step (NOT heap, NOT per-pattern).
  Each word is bitpacked:
  - bits 0-10: CV (11-bit fixed-point, ~5.9 cents per LSB across 10 V range)
  - bits 11-14: gate length (4 bits)
  - bit 15: valid (1 bit, set when step has been captured)
- Configurable length: 16/32/64/128 steps. Default 64. Max 128 = 256 B inline.
- One buffer per FractalTrackEngine instance (not per pattern). Lives in engine state, not model.
- Buffer is volatile engine state — not serialized in project files.

Buffer invariants:
- Buffer is ONE per engine, NOT one per pattern. Pattern switching is a config-only operation (changes divisor, scale, loop window, mutation params, etc.) over the same live buffer.
- Buffer is NOT swapped or cleared on pattern change. Matches the existing codebase pattern: other tracks' `changePattern()` swaps the sequence pointer; Fractal does the same for per-pattern config but the buffer survives.
- Buffer cleared on: track mode change, buffer length change, explicit user clear, power cycle.
- Buffer survives transport reset and pattern change.
- Buffer length change clears buffer entirely — no step preservation.
- No `_recordArmed` flag. Capture is the default state when not locked.
- **Invalid (uncaptured) cells**: playback outputs project root note CV with gate=off. This is the child-track fallback when no source content has reached the cell.
- Gate length values:
  - 0 = rest / gate off
  - 1 = trigger / very short gate
  - 2..14 = proportional gate length within the Fractal step
  - 15 = full-step gate / tie
- Capture stores gate length at Fractal step resolution. At step start, source CV and gate
  are sampled. If gate is low, `gateLen=0`. If gate is high, the cell is provisionally
  full length; a source falling edge before the next Fractal step updates that cell to the
  nearest 1..14 bucket. If no fall occurs, it remains 15.
- Playback schedules gate-off from `gateLen` using the current Fractal step duration.

## Scale Activation

`scale` and `rootNote` are FractalSequence fields, declared and serialized at MVP, but
**inert** until scale-aware behaviors land. Pure mirror plays raw captured CV — scale
doesn't gate anything.

Scale activates when any of these are enabled:
- `recordQuantize=On` — captured CV snapped to scale before storage
- Branch transforms that need scale degrees (transpose-by-degree, invert-around-root)
- Scale-aware ornamentation (next/prev scale note for runs, mordents, anticipations)
- Mutation pitch selection biased toward scale neighbors
- UI display of note names

When scale is inactive: source CV passes through verbatim, root note serves only as the
fallback CV for invalid (uncaptured) cells.

## Sleep

- `sleep` (0..100, per-pattern, default 0): step count of silence after each loop pass.
  Borrowed from StochasticTrack. After playhead wraps from `loopLast` to `loopFirst`,
  engine inserts `_sleepRemaining = sleepScaled` ticks of silence before the next loop
  pass begins. Sleep is step-counted, not bar-locked.

Sleep invariants:
- Sleep fires at the loop boundary, inside `onLoopBoundary()` alongside mutation.
- During sleep, both capture and playback pause; gate stays low, CV held at last value.
- Per-pattern Sleep enables pattern slots to switch between continuous and sparse playback
  (pattern 1 = sleep 0, pattern 2 = sleep 12). Switching at bar boundaries via song mode
  alternates dense and sparse sections.
- Sleep is distinct from loopMode=Once (which stops permanently after one pass) and from
  density (which masks individual cells inside the loop).
- Sleep does not interact with the trunk content; the same content plays after the silence.

## Loop

- `loopFirst`, `loopLast`: playback window boundaries within the buffer. When `loopBars` is non-zero, `loopLast` is derived from bar count (see loopBars below).
- `loopBars` (0-16): bar-quantized loop length. 0 = manual mode (use `loopLast` directly). When >0, auto-compute `loopLast = loopFirst + (loopBars * measureDivisor() / divisor()) - 1`, clamped to `[loopFirst, bufferLength-1]`.
- `beatOffset` (-16 to +16 beats, default 0): shift the loop window by N beats relative to measure bar lines. Applied by adding `beatOffset * (measureDivisor() / 4)` ticks to loopFirst. Positive = later in measure, negative = earlier. Does not change loop length.
- `loopPhase` (float, 0.0 to 1.0, default 0.0): free rotation of playback start within the loop window, as a fraction of loop length. Applied like CurveTrack `globalPhase`: `phasedPos = fmodf(stepFraction + loopPhase, 1.0f)` maps to `loopFirst + int(phasedPos * (loopLast - loopFirst + 1))`. At 0.0 = normal start. At 0.5 = start halfway through the loop. Wraps naturally.
- `sectionPhase[4]`: fixed per-pattern playback lens over four derived sections of the
  effective loop window. Each section has neutral defaults and no enable flag.
  Recommended packed fields per section:
  - `offset` (`int8_t`): signed section-local phase offset, scaled to section length
  - `shape` (`uint8_t`): Linear/None, Push, Pull, Swing, Drunk/Hash later
  - `depth` (`uint8_t`): 0..100 amount applied by shape
- `rotate`: circular shift of playback start within loopFirst..loopLast by integer steps.
- `lock`: when true, trunk buffer mutations are blocked (evolution, recording, boredom recapture). Branch transforms and ornamentation continue to operate normally while locked. This is NOT an output freeze -- it protects the recorded trunk content from further change while generative playback variations still run.
- `mutateFirst`, `mutateLast`: mutation zone -- subset of loop window where transforms may operate.

Loop invariants:
- `recordFirst <= loopFirst <= mutateFirst <= mutateLast <= loopLast <= recordLast <= bufferLength-1` (full extent invariant chain, enforced by list model).
- When `loopBars > 0`, changes to `divisor` or `measureDivisor` (tempo/PPQN) auto-recompute `loopLast`. User edits to `loopLast` are ignored while `loopBars > 0`.
- `beatOffset` is applied before `rotate` and `loopPhase`. Window shifts first, then playback start rotates within the shifted window via step offset (rotate) or fractional position (loopPhase).
- Section boundaries derive from the current effective loop window after `loopBars` and
  `beatOffset`. Remainder cells are distributed front-to-back:
  `s0 = base + (rem > 0)`, `s1 = base + (rem > 1)`,
  `s2 = base + (rem > 2)`, `s3 = base`.
- Section phase is applied after `rotate` and global `loopPhase` in `mapLogicalToRead()`,
  then branch transforms read the phased trunk position.
- `loopPhase` wraps naturally -- applying 0.5 to a 64-step loop places the start at step 32 on the first pass, step 0 wraps around to the end.
- Steps OUTSIDE the mutation zone are anchors -- they replay recorded values unchanged.
- Mutation zone scopes evolution, octave shift, and boredom recapture. Does NOT scope recording, playback, density, or lock.
- Density operates on full loopFirst..loopLast (non-destructive replay mask).
- Default: mutateFirst = loopFirst, mutateLast = loopLast (all steps mutable).

## Mutation (Trunk Evolution)

- `complexity` (0-100): controls how aggressively mutation pitch selection works. Low = repeat anchors, mid = prefer +/-1 scale degree, high = full weighted range.
- `patience` (0-100): boredom timer. At each loop boundary, compute P_reset = f(loopCount, patience). Linear ramp: `P_reset = loopCount x k x (100 - patience) / 100`. Max patience = never resets.
- `mutationProb` (0-100): per-loop-boundary probability of mutating one step in the mutation zone.
- `octaveShiftProb` (0-100): per-loop-boundary probability of offsetting CVs in mutation zone by +/-12 semitones.

Mutation invariants:
- Complexity affects mutation pitch selection only. Recording and recapture always come from source outputs directly.
- Mutation re-rolls CV only -- gate and valid bits are never changed by mutation.
- Octave shift is constrained to +/-1 octave from original base.
- Boredom recapture is an internal engine event (`_autoCapturePending`), distinct from user record arm.

## Evolution (Per-Loop-Cycle Memory)

- `MutationHistory`: circular buffer of 16 `MutationRecord` entries. Each record: stepIndex, oldCV, newCV, loopCount.
- `SelectionPressure`: mode-selectable bias function over history. Three modes:
  - **Even-Spread (Explore)**: bias toward steps least-recently-touched.
  - **Hot-Spot (Cluster)**: bias toward recently-mutated steps + neighbors.
  - **Pattern-Follow (Mimic)**: bias toward steps whose current CV matches a historical mutation source.
- `EvolutionDepth` (0-100%): controls how strongly history biases step selection. 0% = purely random, 100% = full pressure weighting.

Evolution invariants:
- History does NOT learn "good" vs "bad" -- success scoring is deferred (post-MVP).
- Pressure biases toward exploration, recency, or pattern -- not "better" outcomes.
- History is still written when EvolutionDepth=0 (depth only controls read-side weighting, not recording).
- Persistent engine RNG (`_rng`) is used for underlying randomness -- evolution system biases RNG rolls, does not replace them.
- History is cleared on: track mode change, buffer length change, explicit clear, power cycle.

## Branches (Non-Destructive Playback Transforms)

- `branchCount` (0-7): number of branch transforms cycled through.
- `trunkCycles` (1-16): loop repetitions of trunk before switching to branches.
- `branchTransformFlags` (uint8_t bitmask): per-branch transform type.
- `pathType` (0-7): navigation pattern from curated LUT.
- `orderMode` (0-6): playback direction for trunk.

Branch transforms:
1. **Reverse**: play trunk steps backwards
2. **Inverse**: mirror pitch around center note
3. **Transpose**: offset all CV by fixed semitones
4. **Mutate**: per-step probability of re-rolling CV via complexity/scale
5. **Randomize**: each step gets random CV from active scale

Branch invariants:
- Branches have NO buffer storage. They are trunk+math at playback time.
- No buffer mutation during branch phase. Only trunk phase can mutate the recorded buffer.
- Branches automatically reflect evolved trunk -- no manual sync needed.
- Path LUT is stored in flash/rodata (~64 B, easily editable).

## Ornamentation (Per-Step Flourishes)

- `ornamentProb` (0-100%): per-step ornamentation likelihood.
- `ornamentMode`: Off / 2-Step / 4-Step / Max (trills).

Two-step: Anticipation, Suspension, Syncopation, Octave Up, Fifth Up, Half Turn Toward/Away.
Four-step: Run Toward/Away, Turn, Arp Toward/Away, Mordent Up/Down.
Max: Full 8-step trills.

Ornamentation invariants:
- Zero per-step storage -- evaluated in tick path.
- Applied after trunk or branch resolves CV+gate.
- Reuses existing gate queue (same scheduling as ratchet/trill).
- Ornamentation may advance playback by 1-8 micro-steps (for anticipation/suspension/mordents/trills).

## Density (Deterministic Thinning)

- `density` (0-100): deterministic ranked rest-priority thinning over loopFirst..loopLast.
- `tilt` (-100 to +100): bias rest-priority toward front (negative) or back (positive) of loop.

Density invariants:
- Density is non-destructive -- reads `_gateBitmap`, writes to per-tick replay mask.
- Density does NOT modify `_gateBitmap`. Only recording and mutation change the bitmap.
- Changing density up and down produces the same steps disappearing/reappearing (stable priority order).
- Muted steps schedule gate-off at step start. No gate hang.
- Density operates on full loop window, not mutation zone.

## Clock Source

- `clockSource` (Internal/External): controls how the playback step position is determined.
  - **Internal** (default): step advancement driven by clock ticks per the standard divisor system. PlayMode (Aligned/Free) applies normally.
  - **External**: step position is directly mapped from a routed CV (`routedScan` on FractalTrack). PlayMode and divisor are ignored. The CV value is floor-truncated to an integer step index within the loop window. Edge detection (step change) triggers output.
- `routedScan` (Routable<float> on FractalTrack): the CV source for External clock mode. A 0.0-to-1.0 CV maps to `loopFirst..loopLast`.

Clock Source invariants:
- External mode bypasses all clock-based step advancement. No divisor, no phase accumulation, no bar alignment.
- External mode ignores PlayMode entirely — it's not Free-mode-with-CV, it's CV-as-position.
- Edge detection uses the DiscreteMap scanner pattern: `int(cv)` truncates to discrete step, `step != _lastStep` fires once per crossing.
- Recording still uses clock-relative step position in both modes. External mode only affects playback.

## Timing Model

```text
parent tick -> fractal triggerStep() -> current phase determines behavior

RECORDING PHASE (recordArmed=true):
  read source gate+CV -> write to buffer[stepIndex]
  advance stepIndex

PLAYING PHASE (recordArmed=false, trunk phase):
  read buffer[stepIndex] -> if valid: output CV+gate
  at loopLast -> loopFirst boundary:
    1. Boredom reset roll
    2. Evolution: select step via pressure+depth+history, mutate CV
    3. Octave shift roll
    4. Record mutation to history
    5. Decrement trunkCycle counter
    6. If trunkCycles expired -> switch to branch phase

PLAYING PHASE (branch phase):
  read buffer[stepIndex] -> apply current branch transform -> output
  at loopLast -> loopFirst boundary:
    1. Advance to next branch in path
    2. Decrement branchCycle counter
    3. If branchCycles expired -> switch to trunk phase

ORNAMENTATION (both phases):
  after CV+gate resolved, roll ornamentProb
  if triggered: replace/modify CV or insert micro-steps per ornamentMode
```

## Ownership

- FractalSequence owns: per-pattern params (divisor, scale, root, runMode, firstStep, lastStep, resetMeasure).
- FractalTrack owns: 17 sequences + track-level params (all KD params).
- FractalTrackEngine owns: parent resolution, buffer management, evolution, branches, ornamentation.
- MutationHistory owns: 16-record circular buffer (inline in engine).
- Engine RNG owns: persistent `_rng` seed for all random rolls.
- UI owns: presentation and editing, not mutation truth.
- Path LUT owns: branch navigation patterns (in flash/rodata).
- Buffer is volatile engine state, not model state. Survives transport/pattern changes but not power cycle.
