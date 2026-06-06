# Routing Flash Reduction Notes

Snapshot: 2026-06-05.

Routing is under active development. It is acceptable to bake in structural changes now if they simplify ownership and reduce flash. Do not add project-version or migration ceremony unless release prep explicitly starts.

## Current Culprit

`Routing::writeTarget(...)` is a large custom function:

- Current size: ~11,020 B
- Source: `src/apps/sequencer/model/Routing.cpp`
- Main issue: one central function owns every track-mode routing decision.

Current ownership:

```text
Routing::writeTarget
  knows every TrackMode
  knows which targets are track vs sequence for each mode
  manually loops every pattern for every mode
  has Stochastic special cases
```

This creates a large branch tree and duplicates the same "fan out to all patterns" shape across modes.

## Proposal

Move per-mode routing application out of `Routing::writeTarget`.

Target ownership:

```text
Routing::writeTarget
  denormalize once
  handle Project / PlayState
  iterate selected tracks
  call Track::writeRouted(target, intValue, floatValue)
```

Then:

```text
Track::writeRouted
  handles generic Track shell targets:
    CvOutputRotate
    GateOutputRotate
    Run
  switches on active TrackMode
  calls concrete track.writeRouted(...)
```

Then each concrete track owns its own fanout:

```text
NoteTrack::writeRouted
  track-level Note targets
  otherwise sequence.writeRouted(...) for all patterns

CurveTrack::writeRouted
  track-level Curve targets, including CurveRate
  sequence / chaos / wavefolder targets fan out to sequences

TuesdayTrack::writeRouted
  Tuesday / sequence targets fan out to sequences

StochasticTrack::writeRouted
  track-level targets
  Divisor / Scale / RootNote / Rotate / stochastic targets fan out correctly

PhaseFluxTrack::writeRouted
  track-level targets
  sequence targets fan out
```

`DiscreteMapTrack` and `IndexedTrack` are already close to this model.

## Expected Benefit

This is not a micro-optimization. It fixes ownership.

Expected flash effect:

- `Routing::writeTarget(...)` should shrink materially.
- Some bytes will move into concrete track methods.
- Net win is likely a few KB, not the full 11 KB.
- The refactor gives a clean size checkpoint before considering table-driven setters.

## Behavior Risks

Preserve these existing quirks exactly:

- `Rotate` differs for Stochastic: sequence-level, not track-level.
- `CurveRate` is Curve track-level despite being grouped with wavefolder targets.
- `DiscreteMapRangeHigh` and `DiscreteMapRangeLow` route through `DiscreteMapTrack::writeRouted`, which then fans out.
- `IndexedTrack::writeRouted` currently forwards unknown targets to all sequences.
- Teletype remains no-op unless explicitly supported.

## Verification

Measure before and after:

```bash
make -C build/stm32/release sequencer
/opt/homebrew/bin/arm-none-eabi-size -A build/stm32/release/src/apps/sequencer/sequencer
/opt/homebrew/bin/arm-none-eabi-nm --print-size --size-sort --radix=d \
  build/stm32/release/src/apps/sequencer/sequencer \
  | /opt/homebrew/bin/arm-none-eabi-c++filt \
  | rg 'Routing::writeTarget|Track::writeRouted|writeRouted'
```

Functional checks:

- Same target.
- Same selected track mask.
- Same selected track modes.
- Same routed/base values after `writeTarget`.
- Cover project targets, play-state targets, track shell targets, track-level targets, sequence-level targets, and mode-specific targets.

Do this before table-driven routing. Table-driven member setters may save more later, but the first cut should reduce wrong ownership and give a measured flash checkpoint.
