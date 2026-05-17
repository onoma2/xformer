# Phase 7 Dictionary of Truth

This document is the immutable vocabulary and ownership contract for the reimagined Stochastic track core. Update it only when the model topology changes intentionally.

## Provenance

- MeloDICER contributes rhythm rate/variation/rest/legato, melody probability controls, dice/realtime behavior, pattern length, and lock behavior.
- SIG contributes event probability, note/octave/duration weighting, captured loops, repeat, linearity, contour, portamento, ratchet, and force-barline references.
- Proteus contributes explicit `New`, complexity, deterministic density/rest-order thinning, sleep, patience/boredom, mutation, and bounded octave movement.
- Tuesday contributes the useful burst idea: child events inside a parent event, not only same-pitch retrigger count.
- Marbles contributes spread/bias as a bandwidth window over selectable pitch mass.
- Performer contributes the actual product contract: user scales up to 32 degrees, normal root/scale/transpose/static octave semantics, routing conventions, track/page shell, and STM32 RAM gates.

Do not expose module names as final user-facing modes. The track should feel Performer-native.

## Global Rules

- User-facing labels should be short, preferably one word.
- Internal names should follow existing codebase style.
- `Run` is excluded from Phase 7 because "order of random" is not legible enough for the core model.
- Playback order is forward through the active stochastic pattern window.
- `First` and `Last` are stochastic pattern-buffer indices, not Performer step-loop controls.
- Burst child hits do not advance pattern position and do not count toward `Size`.
- `Lock` is a hard evaluated-event freeze.
- `Patience == 100` means never auto-new; it is not lock.

## Pitch

- `Scale`: Performer scale selection, including user scales up to 32 degrees.
- `Root`: Performer root note for scale-degree conversion.
- `Transpose`: normal Performer pitch offset applied after generation.
- `Octave`: normal static full-track octave offset, like Note track.
- `Range`: pre-selection generated pitch span, possibly across multiple scale octaves.
- `Tickets`: per-degree pitch weights where excluded degrees cannot be selected.
- `Spread`: Marbles-style bandwidth window over selectable ticket mass, from tight to broad/flat.
- `Bias`: center position of the Marbles-style window over selectable ticket mass.
- `Jump`: bounded probabilistic octave displacement in the generated pitch domain, separate from static track `Octave`.

Pitch invariants:
- `Tickets` define selectable pitch material.
- `Spread` and `Bias` reshape ticket mass; they are not a second undisclosed pitch-weight system.
- Excluded degrees stay excluded regardless of Spread/Bias.
- `Range` filters before selection, not after selection.

## Generator

- `New`: manually, externally, or automatically creates a fresh generated pattern.
- `Mode`: `Dice` repeats a generated pattern; `Realtime` continuously generates events without reusing the pattern.
- `Complexity`: controls how much of the ticket pool and movement space the melody may explore, from simple repetition to richer melodic/rhythmic structure.
- `Linearity`: advanced bias toward pitches near the previous parent event.
- `Contour`: advanced bias toward ascending or descending motion.

Generator invariants:
- `Complexity` gates pitch behavior and explored vocabulary, not event audibility.
- Low complexity may use a small subset of available tickets even when the ticket pool is large.
- Mid complexity should prefer adjacent scale-degree runs.
- High complexity may use the full allowed pool with wider weighted jumps.

## Pattern

- `Size`: number of generated parent events in the pattern buffer.
- `First`: first parent event included in stochastic playback.
- `Last`: last parent event included in stochastic playback.
- `Lock`: freezes evaluated parent events and burst child hits.

Pattern invariants:
- `Size` is not derived from `First` and `Last`.
- The active playback window is `First..Last`.
- Parent events advance the pattern position.
- Step/grid UI, if present, edits generated or captured parent events; it is not the primary stochastic programming model.

## Rhythm

- `Rate`: base duration of generated parent events.
- `Variation`: bipolar bias toward durations longer or shorter than `Rate`.
- `Rest`: stochastic chance that a generated parent event is silent.
- `Legato`: stochastic chance that a parent event continues the previous gate instead of firing a new one.
- `Slide`: chance or amount of pitch glide into the parent event.

Rhythm invariants:
- `Rate` is the parent-event duration, not a pitch value.
- `Rest` is stochastic generation-time silence.
- `Density` is deterministic pattern thinning. It is not the same as `Rest`.

## Burst

- `Burst`: probability that a parent event contains child hits.
- `Rate`: subdivision spacing for child hits inside one parent event.
- `Count`: amount or probability of child hits produced inside one parent event.
- `Pitch`: whether child hits inherit the parent pitch or re-evaluate pitch.

Burst invariants:
- Child hits are scheduled inside the parent event duration.
- Child hits do not advance the pattern position.
- Child hits do not count toward `Size`.
- If a parent event is hidden by density, its child hits are hidden too.
- Same-pitch repeats are ratchets; `Burst` should mean child-event behavior if exposed as Burst.

## Evolution

- `Density`: deterministic parent-event thinning using the generated pattern's stable rest order.
- `Tilt`: early/late bias used when generating the density rest order.
- `Sleep`: wait time inserted between completed pattern cycles.
- `Patience`: rate at which automatic `New` becomes likely; maximum means never auto-new.
- `Mutate`: probability that existing parent events are regenerated per cycle.

Evolution invariants:
- Density changes must not consume RNG.
- Lowering and raising Density must remove and restore the same parent events for the same generated pattern.
- Tilt affects rest-order generation; it is not a second density probability.
- Patience controls only automatic `New`; manual `New`, mutation, playback, and lock semantics remain separate.
- Mutation operates on existing parent events at pattern-cycle boundaries while unlocked.

## Timing Model

```text
pattern cycle
  parent event
    burst child hit
```

- Parent event timing comes from Rhythm `Rate` and `Variation`.
- Burst child-hit timing comes from Burst `Rate` and `Count`.
- Density filters parent events after generation.
- Lock captures evaluated parent events plus evaluated child hits.

## Ownership

- Track owns generator controls, pitch controls, rhythm controls, burst controls, and evolution controls.
- Pattern buffer owns generated parent events, density ranks, and captured burst child hits.
- Output scheduler owns timed gate/CV playback.
- UI owns presentation and editing, not stochastic truth.
