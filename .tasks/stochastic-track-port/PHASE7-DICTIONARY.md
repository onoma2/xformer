# V5 Stochastic Dictionary of Truth

This document supersedes the original Phase 7 dictionary. It is the current
vocabulary, ownership, and behavior contract for the reimagined Stochastic
track.

Update this file only when the model topology changes intentionally. Do not let
implementation convenience, temporary UI pages, or reference-module vocabulary
silently redefine these terms.

## Provenance

- MeloDICER contributes direct rhythm controls, direct pitch probability controls,
  and the split idea of looped versus continuously generated rhythm/melody.
- SIG contributes explicit probability weighting for pitch, duration, rests,
  articulations, captured loops, linearity, contour, ratchets, and barline
  references.
- Proteus contributes phrase regeneration through patience, complexity as
  human-like melodic/rhythmic vocabulary, loop mutation, and bounded register
  movement.
- Tuesday contributes burst as child events inside one parent event, not merely
  same-pitch retrigger count.
- Marbles contributes macro distribution shape: spread, bias, and steps.
- Performer contributes the product contract: user scales up to 32 degrees,
  normal root/scale/transpose/static octave semantics, routing conventions,
  track/page shell, pattern ownership, and STM32 RAM gates.

Do not expose module names as final user-facing modes. The track should feel
Performer-native.

## Global Rules

- User-facing labels should be short, preferably one word.
- Internal names should follow existing codebase style.
- Project file version must not be bumped during this development stage.
- `Run` remains excluded. Random playback order is not legible enough for the
  core stochastic model.
- `First` and `Last` are stochastic pattern-buffer indices, not Performer
  step-loop controls.
- Burst child hits do not advance pattern position and do not count toward
  `Size`.
- `Patience == 100` means never auto-regenerate. It is not lock.
- `Lock` is a hard runtime evaluated-output freeze. It is not persistence.
- Pattern-defining stochastic controls are sequence-owned unless explicitly
  listed as track-owned.

## Layer Model

```text
Generator layer
  creates parent events and optional burst child hits

Looper layer
  stores, windows, repeats, masks, mutates, and regenerates generated material

Output scheduler
  turns evaluated parent/child events into timed CV and gate output
```

The user-facing UI may expose this as control granularity levels, but the engine
should remain one coherent generator + looper system.

## Control Granularity Levels

### Level 1: Core

Immediate generative music. One global source mode controls both rhythm and
melody.

Generator controls:
- `Complexity`: macro richness for pitch, rhythm, and burst decisions.
- `Density`: generator-level sound/rest amount.
- `Shape`: macro distribution mode.
- `Spread`: extends `Shape`; width of the active distribution.
- `Bias`: extends `Shape`; center or lean of the active distribution.
- `Steps`: extends `Shape`; stepped/discrete character of the distribution.
- `Burst`: probability or amount of child hits.
- `Burst Count`: extends `Burst`; how many child hits.
- `Burst Rate`: extends `Burst`; how tightly or widely child hits are spaced.
- `Burst Pitch`: extends `Burst`; parent pitch versus generated child pitches.

Looper controls:
- `Mode`: `Live` or `Loop` for both rhythm and melody.
- `Size`: number of parent events in the generated phrase.
- `First`: first parent event included in playback.
- `Last`: last parent event included in playback.
- `Rotate`: non-destructive loop read offset.
- `Patience`: loop regeneration interval; 100 means never auto-regenerate.

### Level 2: Direct

MeloDICER-style control. Rhythm and melody source modes can be split.

Generator controls:
- `Rhythm`: extends Level 1 `Mode`; rhythm source is `Live` or `Loop`.
- `Melody`: extends Level 1 `Mode`; melody source is `Live` or `Loop`.
- `Rate`: extends `Complexity`; explicit base parent-event duration.
- `Variation`: extends `Complexity`; rhythmic deviation around `Rate`.
- `Rest`: extends `Density`; explicit stochastic rest probability.
- `Legato`: extends rhythm articulation; chance to continue gate instead of
  retriggering.
- `Slide`: extends pitch articulation; chance or amount of glide.
- `Accent`: extends articulation; chance of accented event.
- `Range`: extends `Shape`; generated melodic span across scale/octaves.
- `Low Degree`: extends `Range`; lower generated pitch boundary.
- `High Degree`: extends `Range`; upper generated pitch boundary.
- `Pitch Tickets`: extends `Shape`; explicit per-degree pitch weights.

Looper controls:
- Level 1 looper controls remain available.
- Split `Rhythm` and `Melody` modes allow stable groove/fresh melody or fresh
  rhythm/stable melody without introducing a second engine.

### Level 3: Weights / Advanced

Explicit probability programming and advanced loop operations.

Generator controls:
- `Duration Tickets`: extends `Rate` and `Variation`; explicit parent-duration
  weights.
- `Rest Tickets`: extends `Rest`; optional weighted rest material if implemented.
- `Degree Rotate`: extends `Pitch Tickets`; shifts generated degrees through the
  active scale.
- `Ticket Rotate`: extends `Pitch Tickets`; rotates ticket weights/mask while
  excluded degrees remain excluded.
- `Linearity`: extends `Complexity`; local melodic smoothing toward nearby
  pitches.
- `Contour`: extends `Complexity`; upward/downward motion bias.

Looper controls:
- `Mask`: extends `Density`; deterministic playback thinning of stored loop
  events.
- `Tilt`: extends `Mask`; early/late priority bias for what survives masking.
- `Mutate`: extends `Patience`; changes stored loop material gradually instead
  of full regeneration.
- `Jump`: extends loop evolution; loop-level bounded register movement.
- `Lock`: hard evaluated-output freeze.

## Core Semantic Split

```text
Density = generator decides how much sound exists.
Rest = direct rhythm-generator silence.
Mask = looper decides how much stored material is allowed through.
Tilt = mask priority bias.
Patience = when loop source is regenerated.
Mutate = how loop source changes without full regeneration.
```

Do not call deterministic loop thinning `Density`. It is `Mask`.

## Pitch

- `Scale`: Performer scale selection, including user scales up to 32 degrees.
- `Root`: Performer root note for scale-degree conversion.
- `Transpose`: normal Performer pitch offset applied after generation.
- `Octave`: normal static full-track octave offset, like Note track.
- `Range`: pre-selection generated pitch span, possibly across multiple scale
  octaves.
- `Low Degree`: lower pre-selection generated degree bound.
- `High Degree`: upper pre-selection generated degree bound.
- `Pitch Tickets`: per-degree pitch weights.
- `Degree Rotate`: non-destructive generated-degree rotation.
- `Ticket Rotate`: non-destructive ticket/mask rotation.

Pitch invariants:
- Pitch tickets define selectable pitch material.
- Excluded pitch degrees cannot be selected.
- Ticket rotation must keep excluded degrees excluded.
- `Range`, `Low Degree`, and `High Degree` filter before selection.
- `Spread`, `Bias`, and `Steps` are Level 1 macro shaping controls. Once the
  user edits explicit pitch tickets, these controls must not secretly compete
  with tickets unless the UI makes their relationship explicit.

## Marbles Macro Shape

- `Shape`: macro distribution mode.
- `Spread`: bandwidth/width of the distribution.
- `Bias`: center/lean of the distribution.
- `Steps`: discrete/stepped character of the distribution.

Marbles-shape invariants:
- Shape controls belong to Level 1.
- Shape may initialize or macro-shape probability material, but it must not be an
  undisclosed second pitch-weight system once ticket editing is exposed.
- Spread/Bias/Steps never enable excluded pitch degrees.

## Rhythm

- `Rate`: base parent-event duration.
- `Variation`: bipolar bias toward durations longer or shorter than `Rate`.
- `Rest`: stochastic chance that a generated parent event is silent.
- `Legato`: stochastic chance that a parent event continues the previous gate
  instead of firing a new one.
- `Slide`: chance or amount of pitch glide into the parent event.
- `Accent`: chance or amount of event accent.

Rhythm invariants:
- Parent duration is not burst timing.
- `Rest` is generation-time silence.
- `Density` is the generator-level macro for sound/rest amount.
- `Mask` is deterministic loop playback thinning and must not consume RNG.

## Duration Tickets

Duration tickets are Level 3 generator controls. They define weighted selection
of parent-event durations. They do not define gate probability, loop masking, or
burst child timing.

Approved duration set (revised 2026-05-20, was previously 1/2, 1/4, 1/8, 1/16, 3/16,
5/16, 7/16, 1/8T — felt too slow in hardware testing):

```text
1/4
1/8
1/16
1/32
1/64
1/8T
1/16T
3/16
```

Duration-ticket invariants:
- The dictionary is now shift-down focused: faster straight durations plus two triplets
  and one dotted, dropping the slow 1/2 and the irregular 5/16, 7/16.
- `1/64` is approved (previous spec forbade it; revised here).
- `1/16T` is the fastest approved parent-triplet duration.
- Quintuplets are deferred until there is a clear UI and clocking contract.
- `_divisor` and `_clockMultiplier` are intentionally ignored in duration-ticket mode
  (tickets express absolute musical duration; divisor sets the rate-mode grid).
  To slow a ticket-mode track, pick longer ticket slots (1/4 is the slowest).
- When duration tickets are inactive, use `Rate + Variation`.
- When duration tickets are active, they replace `Rate + Variation` for parent
  duration selection. Variation does NOT apply in ticket mode (engine code must gate
  the variation block on `!durationTicketsActive()` — see PHASE10.6 bug H1).
- A selected parent duration is the time container for any burst child hits.

## Burst

- `Burst`: probability or macro amount of child hits inside a parent event.
- `Burst Count`: amount of child hits produced inside one parent event.
- `Burst Rate`: spacing/compression of child hits inside the parent duration.
- `Burst Pitch`: whether child hits inherit the parent pitch or re-evaluate
  pitch.

Burst invariants:
- Burst is visible from Level 1.
- Child hits are scheduled inside the selected parent-event duration.
- Child hits do not advance pattern position.
- Child hits do not count toward `Size`.
- If a parent event is silent or masked, its child hits are silent too.
- Same-pitch repeats are ratchets; user-facing `Burst` should imply audible
  child-event behavior.

## Looper

- `Mode`: Level 1 coupled `Live`/`Loop` for rhythm and melody.
- `Rhythm`: Level 2 rhythm source mode, `Live` or `Loop`.
- `Melody`: Level 2 melody source mode, `Live` or `Loop`.
- `Size`: number of generated parent events in the source phrase.
- `First`: first parent event included in playback.
- `Last`: last parent event included in playback.
- `Rotate`: non-destructive loop read offset.
- `Patience`: loop regeneration interval; maximum means never auto-regenerate.
- `Mutate`: loop-source mutation without full regeneration.
- `Jump`: loop-level bounded register shift.
- `Mask`: deterministic playback thinning.
- `Tilt`: early/late priority bias for `Mask`.
- `Lock`: runtime evaluated-output freeze.

Looper invariants:
- Level 1 `Mode = Live` means `Rhythm Live + Melody Live`.
- Level 1 `Mode = Loop` means `Rhythm Loop + Melody Loop`.
- Level 2 may split the source modes.
- `Patience` and `Mutate` affect only Loop domains.
- `Live` domains are already fresh and must not be mutated.
- `Mask` changes must not consume RNG.
- Lowering and raising `Mask` must remove and restore the same parent events
  for the same generated pattern.
- `Tilt` affects mask priority generation; it is not a second density
  probability.
- `Lock` sits above generator, looper, patience, mutation, mask, and source
  edits.

## Timing Model

```text
pattern cycle
  parent event
    burst child hit
```

- Parent events advance the pattern position.
- Parent event timing comes from `Rate + Variation` or `Duration Tickets`.
- Burst child-hit timing comes from `Burst Rate` and `Burst Count`.
- Mask filters parent events after generation.
- Lock captures evaluated parent events plus evaluated child hits as output
  events.

## Ownership

Sequence-owned:
- Pattern source modes, generator controls, pitch tickets, pitch range,
  duration tickets, burst controls, pattern window, mask/tilt, patience,
  mutate, jump, source event buffers, and generated ranks.

Track-owned:
- Standard Performer track infrastructure: `cvUpdateMode`, `slideTime`,
  static `octave`, `transpose`, fill state, and other established track-wide
  performance offsets.

Engine-owned:
- Runtime playback state, RNG state, current loop position, runtime lock/hold
  buffer, and timed output queues.

UI-owned:
- Presentation, page level, edit focus, and shortcuts only. UI must not define
  stochastic truth.
