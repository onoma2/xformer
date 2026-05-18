# FractalTrack Dictionary of Truth

This document is the immutable vocabulary and ownership contract for the FractalTrack track mode. Update it only when the model topology changes intentionally.

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
- The trunk is the single ground truth buffer. Branches are playback transforms over trunk, not separate buffers.
- Branches do NOT mutate the buffer. Only the evolution system (during trunk phase) mutates the buffer.
- Playback order is forward through the active loop window unless overridden by Order mode.
- Ornamentation is applied AFTER trunk or branch CV+gate is resolved.
- Phase 4 evolution (trunk mutation) and Phase 5 branches are independent - branches read whatever trunk exists, evolved or not.
- Lock blocks ALL buffer mutation: evolution, recording, boredom recapture.
- No Teletype parent. Teletype output is script-defined and too variable for predictable fractaling.

## Parent

- `Source A`, `Source B`: track indices (1-8) whose gate+CV output is recorded. Any engine type.
- Cycle safety: source tracks must have lower indices than the FractalTrack index (enforced by list model clamping).
- Gate Logic (8 modes): A, B, AND, OR, XOR, NAND, Random.
- CV Logic (8 modes): A, B, Sum, Avg, Min, Max, Random.
- Recording reads `gateOutput(0)` and `cvOutput(0)` from source engines each step.

Parent invariants:
- Source data is read at tick time. No snapshot of past data.
- Invalid source (index >= fractal index) silently produces idle output.
- Parent changes propagate immediately on next tick. FractalTrack does not cache parent state.
- No parent link loops (A links fractal to B, B links fractal to A).

## Buffer

- `_cvBuffer[]`: heap-allocated float array storing V/Oct voltages per step.
- `_gateBitmap[]`: heap-allocated bitmask (1 bit per step) storing gate state.
- `_validBitmap[]`: heap-allocated bitmask (1 bit per step) storing whether step has been recorded.
- Configurable length: 16/32/64/128/256 steps. Default 64.
- One buffer per FractalTrackEngine instance (not per pattern).
- Buffer is volatile engine state -- not serialized in project files.

Buffer invariants:
- Buffer cleared on: track mode change, buffer length change, explicit user clear, power cycle.
- Buffer survives transport reset and pattern change.
- Buffer length change clears buffer entirely -- no step preservation.
- Recording is overwrite-only in MVP (blend/overdub is post-MVP via weighted lerp).
- Recording arming is Fractal-local (`_recordArmed` flag), not coupled to global `_engine.recording()`.

## Loop

- `loopFirst`, `loopLast`: playback window boundaries within the buffer.
- `rotate`: circular shift of playback start within loopFirst..loopLast.
- `lock`: when true, buffer is read-only. Mutations, recording, and boredom recapture are blocked.
- `mutateFirst`, `mutateLast`: mutation zone -- subset of loop window where transforms may operate.

Loop invariants:
- `loopFirst <= mutateFirst <= mutateLast <= loopLast` (enforced by list model).
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
