# V5 Stochastic Dictionary of Truth

This document supersedes the original Phase 7 dictionary. It is the current
vocabulary, ownership, and behavior contract for the reimagined Stochastic
track.

Update this file only when the model topology changes intentionally. Do not let
implementation convenience, temporary UI pages, or reference-module vocabulary
silently redefine these terms.

## Phase 11 — Simplification Update (2026-05-21)

The pitch and duration pickers were unified. **Level branches and mode toggles
no longer gate engine math.** The authoritative spec is
`PHASE11-SIMPLIFY.md`; sections below describing per-Level visibility or
"alternative-mode" engine branches are historical context, not current contract.

**Engine now:**
- One pitch picker. Tickets × Complexity kernel × Marbles distro × Steps sieve.
  Always multiplicative. Steps is the **pitch-side sieve** parallel to Mask
  (rank-based universal-LUT cutoff, 0..100, high = open).
- One duration picker. Duration tickets × noteDuration kernel × variation spread.
  Always combine. `durationTicketsActive` no longer branches engine.
- Marbles toggle, Contour, Linearity, Density, variation sign — all dropped from
  engine math. Model fields kept as reserved slots for serialization stability.

**Reserved slots** (field/getter/setter/storage intact, engine ignores, UI hidden):
`_density`, `_contour`, `_linearity`, `_marblesMode` (flag), `_level` (enum),
plus `_variation` sign bit (storage signed; getter returns `abs()`).

**Repurposed:** `_marblesSteps` (was Marbles permutation reseed interval, never
read by engine) → pitch Steps sieve cutoff. Clamp relaxed to 0..100, default 100.

**UI surface (single flat list, semantically grouped):**
- Playback: Rhythm, Melody, Refresh
- Pitch: Complexity, Bias, Spread, Steps, Range, MinDegree, MaxDegree
- Rhythm: NoteDuration, Variation, Rest, Slide, Legato, Accent
- Burst: Burst, BurstCount, BurstRate, BurstPitch
- Pattern: Mask, Tilt
- Window: Size, First, Last, Rotate
- Evolution: Sleep, Patience, Mutate, Jump

Duration ticket bar labels are now divisor-aware at draw time via
`StochasticSequence::printSlotDuration()` — they track clock-divisor changes
instead of lying with hardcoded 1/16-centered strings.

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
- `Patience`: Proteus-style Poisson CDF regeneration. Knob 0..99 → λ ∈ [1, 50]
  linear; each loop boundary rolls `F(k; λ) = Σᵢ e^(-λ) λⁱ / i!` against
  uniform 0..1, fires regen when it crosses. Shared counter across rhythm and
  melody Loop domains. Knob 100 = off sentinel. Stochastic "tension builds,
  then breaks" feel — replaces the V5 deterministic `2^bucket` countdown.

### Level 2: Direct

MeloDICER-style control. Rhythm and melody source modes can be split.

Generator controls:
- `Rhythm`: extends Level 1 `Mode`; rhythm source is `Live` or `Loop`.
- `Melody`: extends Level 1 `Mode`; melody source is `Live` or `Loop`.
- `Note Duration`: extends `Complexity`; explicit base parent-event duration as
  a divisor-relative LUT slot (0..7 → ×8, ×4, ×3, ×2, ×4/3, ×1, ×2/3, ×1/2 of
  the sequence divisor). Field renamed from `rate` (2026-05-21).
- `Variation`: extends `Complexity`; bipolar LUT-slot substitution bias around
  `Note Duration` (Approach 3 — see `Rhythm` section).
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
- `Duration Tickets`: extends `Note Duration` and `Variation`; explicit
  parent-duration weights across the 8-slot LUT.
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

- `Note Duration`: base parent-event duration. LUT slot 0..7 mapped to
  divisor-relative multipliers (×8, ×4, ×3, ×2, ×4/3, ×1, ×2/3, ×1/2). Slot 5
  (×1) equals the sequence's clock divisor; all other slots scale relative.
- `Variation`: bipolar -100..+100; per-event probability of LUT-slot
  substitution with triangular weight kernel toward adjacent slots.
  - Sign = direction in LUT (positive = shorter, matches Melodicer CW).
  - |variation|/100 = substitution probability per event AND max-jump distance
    scaling (1..4 slots based on |var|).
  - Applied at generation time inside `generateRhythmEvent` so each event
    bakes its own randomized durationIndex into the stored buffer.
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
- Engine step advancement is **event-driven** for all modes (ticket-mode and
  Live/Loop alike): `_eventDuration` is set on each trigger and the next trigger
  fires after `_eventElapsed >= _eventDuration` ticks. Divisor-aligned timing
  removed 2026-05-21 since it ignored the LUT slot.

## Duration Tickets

Duration tickets are Level 3 generator controls. They define weighted selection
of parent-event durations. They do not define gate probability, loop masking, or
burst child timing.

Approved duration set (revised 2026-05-21 — divisor-relative multipliers,
sorted descending by ticks):

```text
slot 0: ×8       (= 1/2 when divisor=1/16)
slot 1: ×4       (= 1/4)
slot 2: ×3       (= 3/16)
slot 3: ×2       (= 1/8)
slot 4: ×4/3     (= 1/8T)
slot 5: ×1       (= divisor — 1/16 default)
slot 6: ×2/3     (= 1/16T)
slot 7: ×1/2     (= 1/32)
```

Duration-ticket invariants:
- LUT entries are **divisor-relative fractions**, not absolute ticks.
  `StochasticTrackEngine::getDurationFraction(idx) → {num, den}` is canonical.
  Engine computes `ticks = (divisor × num) / den`. The legacy
  `getDurationMultiplier(idx)` helper is kept for tests and assumes
  divisor=1/16.
- The dictionary is now divisor-relative: changing the sequence divisor scales
  the entire table proportionally. Slot 5 (×1) always equals the divisor.
- `1/64` removed (replaced by 1/2 at slot 0 — adds long-range, drops the floor
  that exceeded the perceptual minimum gate).
- `1/16T` remains the fastest approved parent-triplet duration.
- Quintuplets are deferred until there is a clear UI and clocking contract.
- `_clockMultiplier` is intentionally ignored in duration-ticket mode (tickets
  pick LUT slots; multiplier already applies to divisor scaling).
- When duration tickets are inactive, use `Note Duration + Variation` (was
  `Rate + Variation`).
- When duration tickets are active, they replace `Note Duration + Variation`
  for parent-duration selection. Variation does NOT apply in ticket mode
  (engine code must gate the variation block on `!durationTicketsActive()` —
  see PHASE10.6 bug H1).
- A selected parent duration is the time container for any burst child hits.

## Burst

- `Burst`: probability that a parent event triggers a burst. Hard-gated by a
  Cluster F minimum-duration check — burst is skipped when parent ticks < 96
  (1/8 at PPQN=192). Below that threshold the children would pack into
  perceptual mush.
- `Burst Count`: probability-bias knob 0..100% over a fixed LUT
  `{2, 3, 4, 5}` children. Triangular weight bias picks one count value per
  parent burst.
- `Burst Rate`: probability-bias knob 0..100% over a fixed LUT of spacing
  denominators `{parent/2, parent/3, parent/4, parent/5, parent/6}`. Picked
  slot index is baked into the event (`StochasticSourceEvent::burstRate`
  field repurposed as the spacing slot 0..4) so Loop-mode playback is
  consistent across iterations.
- `Burst Pitch`: whether child hits inherit the parent pitch (`Parent`) or
  re-evaluate fresh pitch per child (`Generate`).

Burst invariants:
- Burst is visible from Level 1.
- Child hits are scheduled inside the selected parent-event duration.
- Child hits do not advance pattern position.
- Child hits do not count toward `Size`.
- If a parent event is silent or masked, its child hits are silent too.
- Same-pitch repeats are ratchets; user-facing `Burst` should imply audible
  child-event behavior.
- Engine widened from 4 to 5 children per parent (`kMaxChildren = 5` matches
  the burstCount LUT maximum).
- Count auto-clips at eval time if the picked spacing slot can't hold the
  picked count children (Option C — "count = max intent, spacing wins").
- Two-layer Cluster F gate: generation skips storing count when parent ticks
  too short; eval-time safety net also zeroes count if the stored event's
  effective duration is too short at playback.

## Looper

- `Mode`: Level 1 coupled `Live`/`Loop` for rhythm and melody.
- `Rhythm`: Level 2 rhythm source mode, `Live` or `Loop`.
- `Melody`: Level 2 melody source mode, `Live` or `Loop`.
- `Size`: number of generated parent events in the source phrase.
- `First`: first parent event included in playback.
- `Last`: last parent event included in playback.
- `Rotate`: non-destructive loop read offset.
- `Patience`: Poisson CDF regeneration timer. Knob 0..99 → λ ∈ [1, 50]; CDF
  rolled at each loop boundary against shared `_loopCycleCount`. P(regen)
  rises monotonically with each survived loop, resets to 0 on fire. Fires
  invalidate whichever domains are currently in Loop mode. Knob 100 = off
  (no auto-regen). Avg loops before regen: knob 0 ≈ 1.3, 50 ≈ 21, 99 ≈ 43.
- `Mutate`: bipolar -100..+100 loop-source mutation per loop end.
  - Sign selects algorithm: positive = Marbles-style permutation (swap two
    events), negative = Proteus-style destructive regen (replace one event).
  - |mutate|/100 = probability of substitution per loop iteration AND
    inverse coupling for bias strength: low magnitude → strong tonal anchor,
    high → uniform random.
  - Destructive pitch path uses a universal scale-degree weight LUT
    (`universalDegreeBoost(degInOct, N)`) with anchors at simple integer
    fractions of N: root, N/2, N/3, 2N/3, N/4, 3N/4. Works for any scale
    size (5-degree, 12-EDO, 43-EDO, microtonal) without touching the Scale
    class. Effective weight: `ticket × (100 + biasStrength × universalBoost)`.
    User-curated tickets multiply through.
  - Destructive rhythm path uses an anchor LUT centered on the current
    `Note Duration` slot, triangular kernel falling off across the 8 slots.
    `Variation` direction bias is additive (preserves user intent during
    mutation events). Effective weight per slot:
    `100 + biasStrength × anchorKernel + variationDirection × variationMag`.
  - Knob 0 = no mutate. Knob ~30 = rare + tonal. Knob ~70 = frequent +
    moderately spread. Knob 100 = every loop, fully uniform random pick on
    both pitch and rhythm axes.
- `Jump`: Proteus-style octave walk per loop end.
  - Random ±1 step from current `_jumpRegister`, reflected at the
    `kJumpMaxRange = ±2` bounds. Walks `… 0 → +1 → +2 → +1 → 0 → -1 → -2 …`
    over time rather than snap-toggle.
  - **Non-destructive**: `_jumpRegister` is applied as a playback-time octave
    offset only. Stored events are never modified. Setting jump=0 stops new
    walks; current register persists until engine reset.
  - Differs from Proteus (which mutates stored octaves) but matches Proteus's
    bounded random-walk arithmetic.
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
- Parent event timing comes from `Note Duration + Variation` or
  `Duration Tickets`.
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
