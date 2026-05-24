# Phase 15 — Pitch Math Review

_Stub drafted 2026-05-22. Status: design queue; depends on Phase 14B landing first._

## Why this exists

`StochasticGenerator::generateDegree` (`src/apps/sequencer/engine/StochasticGenerator.cpp:493`) produces musically surprising output at default knob values. The walkthrough in `PHASE14B-SEED-LOG.md` ("Pitch math — carrying forward unchanged") shows that starting from C with all knobs at defaults, the next note's peak probability is at F# rather than near C — because the marbles bell overpowers the kernel that's supposed to re-center on `lastDegree`.

Phase 14B explicitly carries today's formulas forward unchanged so the seed+log architecture change doesn't bundle a musicality shift. Phase 15 is the dedicated cleanup pass on the math.

## Scope

`generateDegree` only — the melody picker. Out of scope: rhythm pickers (`pickDuration`, burst LUTs), Marbles mode toggling, scale/quantization, routing.

## Issues to fix

Each item is a known divergence between the formula's apparent intent and its audible effect at default knob values. Order is by audibility / blast radius, not implementation order.

### 1. Marbles bell dominates kernel at defaults

**Symptom:** `marblesSpread = 50` (the default) produces a 5× peak-to-edge weight ratio. Multiplied against the kernel triangle, the joint peak lands on the bell center (slot 6 in chromatic) regardless of where `lastDegree` puts the kernel. New users hear a sequencer that ignores its own continuity rules at default settings.

**Diagnosis:** `marblesLeak = 1 + marblesSpread/10` gives leak=6 at spread=50, while `mtri = (width - dist) * 10` gives a max of 60 at center. Combined range is 6..66 — over 10× edge-to-center. The Phase 12 "×10 scaling" fix applied to mtri made the bell too sharp.

**Direction:**
- Either rebalance so default spread=50 is genuinely neutral (mtri scaled less aggressively, e.g., ×3 instead of ×10).
- Or change the default to `marblesSpread = 100` so the bell is wide enough to be near-uniform at defaults.
- Or restructure: marbles bell as additive bias, not multiplicative factor. `weight = base × kernel + marblesBoost` instead of `× marbles`.

### 2. No octave equivalence in the kernel

**Symptom:** With `range = 2` and `lastDegree = 0` (C in oct 1), candidate slot 12 (C in oct 2) gets `d = 12 → tri = max(0, 13-12)*10 = 10` — minimum possible. The musically natural "same note, octave up" move gets the worst kernel score; meanwhile slot 6 (F# in oct 1) gets a much higher score for being closer in slot-index space.

**Diagnosis:** kernel distance is computed in flat slot-index space. Doesn't know about octaves.

**Direction:**
- Compute kernel distance modulo `activeNotes`, then scale separately for octave distance.
- Or: separate two-dimensional kernel — degree-within-octave kernel + octave kernel — and combine.

### 3. Sieve cuts before kernel applies

**Symptom:** With `stepsSieve = 50` (chromatic, range=1), K=6, surviving slots are {0, 6, 4, 8, 3, 9} (top by `universalDegreeBoost`). The kernel can never pull toward slots 1, 2, 5, 7, 10, 11 — even if `lastDegree` is right next to one. Two knobs that the user thinks operate orthogonally interact in an opaque way.

**Diagnosis:** Sieve is a hard binary filter applied first. Kernel multiplies survivors. There's no path for the kernel to "vote against" the sieve.

**Direction:**
- Make sieve a soft weight rather than a hard cut. Top-K get +boost, others stay at lower weight but still pickable.
- Or: document the interaction explicitly in the UI — when sieve is partial, the kernel only operates on survivors.

### 4. Contour drift swamps kernel at high knob values

**Symptom:** `drift = contour × signedDist / 2`. At `contour = 100` and `i - lastIdx = 11`, drift = 550 vs max `tri = 130`. The kernel triangle becomes irrelevant; the knob's effective behavior shifts from "directional bias" to "force the walk in one direction."

**Diagnosis:** Contour drift uses scaled units (×10) but no clamp relative to triangle size.

**Direction:**
- Clamp drift to a fraction of `kernelWidth × 10` (the existing maxDrift is `(kernelWidth > 2 ? kernelWidth : 2) * 10` — but it's applied to total drift, not per-slot, so the asymmetry persists).
- Or: cap `|contour|` at a lower internal max, expose UI as "drift strength" rather than a magnitude knob.

### 5. kernelLeak too small to matter

**Symptom:** `kernelLeak = complexity/10` gives max 10 against `tri` max 130. A low-complexity walk can never reach degrees more than `kernelWidth` slots away from `lastDegree`. The "complexity" knob feels like it has a cliff rather than a gradient.

**Diagnosis:** Leak doesn't scale with `tri`. Leak was the fallback for high-complexity leaps; at the ×10 tri scaling, the leak floor lost relative weight.

**Direction:**
- Scale leak with `kernelWidth` (e.g., `leak = kernelWidth × complexity/100`) so high complexity produces a real floor.
- Or: redefine complexity as kernel sharpness vs flatness on a single axis (no separate leak).

### 6. base flat-10 vs ticket-mode crossover

**Symptom:** With all tickets = 0, `base = 10` for every slot — flat. Set one ticket to 5, suddenly every other unset slot becomes `base = 0 = silent`. User intent ("I want some slots to be more likely") is interpreted as exclusion of the unmentioned slots.

**Diagnosis:** `anyTicket` flag flips the meaning of "0" from "default" to "exclude." Confusing UX, no UI indication.

**Direction:**
- Three states explicit: `-1` = exclude, `0` = default-flat, `1..100` = weighted. Today's `-1` already means exclude; widen to make `0` semantic-default everywhere.
- Or: drop the crossover. Always treat `0` as flat-default, require explicit `-1` for exclude.

### 7. Contour edge trap needs repeat-run pressure

**Symptom:** With low `Complexity` and positive `Contour`, the melody can climb to the top of the active palette and then repeat the top note indefinitely. Example shape: C major, `Range=1`, `Steps≈92`, `Bias≈17`, `Spread≈53`, `Complexity≈9`, `Contour≈+24` can collapse into a long B run. Negative `Contour` mirrors this at the bottom.

**Diagnosis:** `drift = contour × signedDist / 2` rewards candidates above `lastDegree` and punishes candidates below it. At the top edge there is no higher candidate, so the same note keeps a nonzero kernel while all downward release notes can be clamped to zero. The formula has directional pressure but no edge release or anti-stutter law.

**Direction:**
- Add a small same-degree run counter inside melody generation.
- First repeat remains allowed.
- Second repeat is weakened.
- Third repeat is strongly weakened.
- Further repeats are nearly blocked unless no other survivor has weight.
- Do not expose a new user knob. This is a musical law for `Contour`, not a product control.
- Audit whether the repeat counter applies only to exact absolute degree, or also to same scale degree across octaves after the octave-equivalence fix.

Candidate fixed law:

```
repeatPenalty = repeatCount <= 0 ? 100 :
                repeatCount == 1 ? 55 :
                repeatCount == 2 ? 20 :
                5;

if candidateDegree == lastDegree:
    weight = weight * repeatPenalty / 100
```

Important behavior target: positive `Contour` should produce ascending phrases with brief edge touches, not top-note traps. Negative `Contour` should produce descending phrases with brief bottom touches, not bottom-note traps.

### 8. Marbles reference: Bias/Spread are continuous before quantization

**Reference:** `temp-ref/marbles/random/output_channel.cc` and `temp-ref/marbles/random/distributions.h`.

Marbles does not apply `Bias` / `Spread` as a discrete triangle over scale degrees. It first draws a continuous `0..1` random value, reshapes that value, then quantizes/smooths later:

```
u = uniform random 0..1
value = BetaDistributionSample(u, spread, bias)
value += degenerate_amount * (bias - value)
value += bernoulli_amount * (bernoulli_value - value)
voltage = range(value)
quantized = Quantize(voltage, steps)
```

Behavior:
- `Bias` moves the center of the continuous field.
- Low `Spread` collapses toward the `Bias` value directly.
- Middle `Spread` gives a beta-shaped cloud around the bias.
- High `Spread` becomes Bernoulli-like: mostly low or high, with `Bias` setting the split.

Current Stochastic math is different:

```
candidate notes -> triangle around Bias -> clipped at palette edges -> multiplied into note weights
```

Implication for Phase 15: if `Bias` / `Spread` are intended to feel Marbles-like, review replacing the discrete triangle with a continuous shaped pitch-position draw before scale-degree selection. This would also avoid some edge clipping weirdness because low spread near an edge would mean "stay near this position" rather than "lose half the triangle."

### 9. Range could become bipolar: field width vs octave-jump chance

**Symptom:** Today's `Range` is the only way to get large pitch jumps, but it works by expanding the whole candidate field. `Range=4` does not mean "mostly local melody with occasional big jumps"; it means every pitch roll sees four octaves all the time. The result can feel vertically loose instead of melodically jumpy.

**Direction:** Re-map `Range` around a center point without adding a new knob:

```
Range = 50
  one-octave normal melody

Range > 50
  current behavior, mapped upward
  50..100 -> 1..4 octave candidate field

Range < 50
  keep one-octave candidate field
  add per-note octave jump chance
  lower value -> higher jump chance / stronger leap tendency
```

Behavior target:
- `Range 80`: melody lives across several octaves.
- `Range 20`: melody mostly walks in one octave, but some notes leap out.
- `Range 50`: stable one-octave default.

Implementation note: for the lower half, pick the base scale degree in the one-octave field, then maybe add an octave offset to the heard note. Do **not** let the jump offset fully poison melodic continuity; the next contour/complexity decision should probably remember the base degree, not the jumped heard degree.

This reuses the musical meaning of existing `Jump` behavior (occasional large displacement) without adding a new user control. Exact mapping needs fixture review.

### 10. Knob edits should sculpt; NewM should reroll

Architectural invariant to verify before implementation:

```
knob turn reshapes existing seed result
NewM changes seed
Patience M changes seed
Mutate edits result
```

That gives users control: knobs sculpt, `NewM` rerolls.

Before claiming current behavior, audit the invalidation paths for:
- `Complexity`
- `Contour`
- `Bias`
- `Spread`
- `Steps`
- `Range`
- `Pitch Tickets`

Desired Phase 15 behavior: changing these parameters should keep the same melody seed and reshape the weighted choices from that seed. It should not silently become a full new-seed melody unless the user triggers `NewM`, `Patience M`, or an explicit mutation path.

### 11. Parameter ownership: vocabulary, thinning, emphasis, shape

Correct conceptual split for the pitch controls:

```
Vocabulary:
  Scale
  Root Note
  Range
  Low Degree / High Degree if active
  explicit excludes

Vocabulary thinning:
  Steps

Note emphasis:
  Pitch Tickets

Region shape:
  Bias
  Spread

Motion shape:
  Complexity
  Contour
```

Do **not** describe tickets as vocabulary. Scale/root/range define what notes exist. Tickets say which existing notes matter more.

Current issue: `Steps`, `Pitch Tickets`, `Bias` / `Spread`, and `Complexity` / `Contour` all affect final probability, but the current multiplicative formula lets any one layer erase the others.

Phase 15 direction:
- `Steps 100` should be transparent.
- Lower `Steps` should become soft landmark emphasis unless set extremely low.
- `Pitch Tickets` are durable user emphasis, not a fragile post-filter.
- `Bias` / `Spread` should be transparent at defaults and bounded away from defaults.
- `Complexity` / `Contour` should be transparent at defaults and bounded away from defaults.
- Final probability should preserve layer intent: vocabulary gate × step thinning × ticket emphasis × region shape × motion shape.

### 12. Revamp target: Range × Bias × Spread as Marbles-style continuous region roll

Current Stochastic code treats `Range`, `Bias`, and `Spread` as a discrete triangle over the candidate list:

```
Range expands allowedCount
Bias chooses biasPos inside allowedCount
Spread chooses triangle width over allowedCount
regionWeight = triangle(candidateIndex, biasPos, width)
```

That means changing `Range` changes the probability law itself. Wider range does not merely give the melody more room; it also moves the `Bias` point and changes the effective width of `Spread`.

Marbles does the cleaner version:

```
u = uniform 0..1
value = BetaDistributionSample(u, spread, bias)
value += low-spread collapse toward bias
value += high-spread Bernoulli low/high pull
voltage = range(value)
quantize later
```

Range modes in Marbles are output scaling after the continuous roll:

```
NARROW:   value * 2  + 0    // 0..2V
POSITIVE: value * 5  + 0    // 0..5V
FULL:     value * 10 - 5    // -5..+5V
```

Phase 15 target law:

```
1. Build vocabulary field:
   Scale + Root + explicit excludes + Low/High + Range mode.

2. Roll region position:
   x = marblesPosition(seedDraw, Bias, Spread)
   where x is continuous 0..1 across the active field.

3. Map x into the musical field:
   position = x * (fieldCount - 1)

4. Convert position into candidate weights:
   nearest notes get strongest region weight,
   neighbors get softened weight,
   far notes keep only the configured transparent/default floor.

5. Apply layers:
   final = vocabularyGate × stepsWeight × ticketEmphasis × regionWeight × motionWeight
```

User-facing behavior:

- `Bias` moves the center/lean inside the active musical field.
- `Spread` controls how concentrated or extreme the region draw is.
- `Range` defines the playable field, not the triangle math.
- Defaults must be transparent enough that `Bias=50`, `Spread=50`, `Range=50` do not overpower `Complexity`, `Contour`, or tickets.

Range mapping direction:

```
Range = 50
  one-octave field, no extra jump pressure

Range > 50
  wider field, mapped 50..100 -> 1..4 octaves

Range < 50
  one-octave field, plus octave-displacement chance after base degree selection
  lower value -> stronger chance of a large jump
```

Important continuity rule: for `Range < 50`, the melody continuity memory should remember the base degree before octave displacement. The heard jump should not make the next `Complexity` / `Contour` decision think the whole melody has permanently moved octaves.

Implementation constraint: do not add knobs. This is a behavior-law rewrite for existing `Range`, `Bias`, and `Spread`.

Fixture requirements:

```
RangeBiasSpreadFixture:
  scale = C major
  seed fixed
  tickets all default
  steps = 100
  complexity = default transparent
  contour = 0

  case A: Range=50, Bias=50, Spread=50
    distribution is near-neutral across one octave

  case B: Range=80, Bias=50, Spread=50
    distribution covers multiple octaves without changing center semantics

  case C: Range=20, Bias=50, Spread=50
    base melody remains one-octave; occasional octave jumps appear

  case D: Bias=0 / 100
    concentration moves to low / high edge without edge-triangle collapse

  case E: Spread low / mid / high
    low = near Bias, mid = cloud, high = low/high extreme tendency
```

## Duration / Variation / Rest law

Scope: generation law only. Do not include windowing, phrase scaling, reset boundaries, mask/rank, or playback transforms in this section.

Current behavior to replace:

```
duration weight[i] = ticketOrFlat[i] * localKernel(NoteDuration, Variation, i)
rest = sequential Bernoulli after the duration roll
```

Problem:

- `Note Duration` and `Duration Tickets` fight inside one multiplicative field.
- `Variation` mostly widens/leaks around `Note Duration`; it does not provide a clear way to explore the full duration field.
- Carefully edited tickets can be overpowered by a distant `Note Duration`.
- A `Note Duration` setting can still dominate when the user expects tickets to be the programmed duration source.

New user contract:

```
Note Duration = home duration
Duration Tickets = programmed duration source / emphasis
Variation = who owns the duration roll
Rest = independent silence chance
```

Variation zones:

```
low Variation    -> Note Duration owns the roll
middle Variation -> Duration Tickets own the roll
high Variation   -> exploration owns the roll, with full-field and opposite-side access
```

Example:

```
Note Duration = 1/16
Duration Tickets = only 1/2 high

Variation low  -> mostly 1/16
Variation mid  -> mostly 1/2
Variation high -> 1/2 remains important, but wider/extreme durations become reachable
```

Duration weighting shape:

```
homeWeight[i]     = bell around Note Duration
ticketWeight[i]   = Duration Ticket value, or flat field if all tickets are zero
exploreWeight[i]  = full-field floor + opposite-side bell around mirrored Note Duration

homeAmount   = strongest at Variation=0, fades out by the ticket zone
ticketAmount = strongest in the middle zone
exploreAmount = rises after the ticket zone, strongest at Variation=100

weight[i] =
  homeWeight[i] * homeAmount
  + ticketWeight[i] * ticketAmount
  + exploreWeight[i] * exploreAmount
```

Rest law:

```
rest = random(0..99) < Rest
```

A rest still consumes the chosen duration.

Dice contract:

```
duration dice and rest dice must be independent deterministic streams
changing Rest must not reroll durations
changing Note Duration / Variation / Duration Tickets must not reshuffle rests
```

Implementation targets:

```
StochasticGenerator::pickDuration()
StochasticGenerator::mutateRhythmOne() duration path
rhythm generation/cache path that currently decides duration/rest
```

Fixture requirements:

```
DurationVariationRestFixture:
  fixed rhythmSeed
  fixed slot index set
  all non-duration rhythm controls held constant

  case A: all tickets zero, NoteDuration=1/16, Variation low
    durations cluster near 1/16

  case B: only 1/2 ticket high, NoteDuration=1/16, Variation low
    NoteDuration still dominates

  case C: only 1/2 ticket high, NoteDuration=1/16, Variation middle
    ticketed 1/2 dominates

  case D: only 1/2 ticket high, NoteDuration=1/16, Variation high
    1/2 remains likely, but full-field/opposite durations appear

  case E: Rest changes from 0 to 100
    rest decisions change, duration picks remain identical

  case F: Variation / tickets / NoteDuration change
    duration picks change, rest decisions remain identical
```

## Slide law

Scope: per-cell slide on/off + slide time, picked from a single knob.

Background: vinx's AcidGenerator scores each gated step on how musically appropriate a slide is — smaller pitch intervals to the next note score higher; large leaps score negative. Top-K candidates by score get slide. The "interval-aware musicality" puts slides where they sing (adjacent-step legato) rather than scattering randomly across leaps. See `temp-ref/vinx-performer/src/apps/sequencer/engine/generators/AcidGenerator.cpp:401` (`updateLayerSlide`).

Stochastic adoption — Approach 1: per-cell deterministic, score-modulated threshold.

Slide LUT (idea): one knob (Slide), one per-cell field (`event.slideSlot`, 3 bits). Slot 0 = no slide. Slots 1..N = multipliers on `track.slideTime` — e.g. `{none, ×0.5, ×1.0, ×2.0, ×4.0}` for five slots.

Cache walk pick per cell:

```
For each cell:
  next = next audible cell's pitch (degree + octave * notesPerOctave)
  interval = |cellPitch - nextPitch|

  scoreBoost  = (interval <= 1) ? +30
              : (interval <= 3) ? +16
              :                   -8
  scoreBoost += (cellIdx % motifLength == 0) ? +10 : 0
  scoreBoost += (motifLength > 4)            ?  +4 : 0

  effectiveKnob = clamp(seq.slide() + scoreBoost, 0, 100)

  if cellRng.nextRange(100) >= effectiveKnob:
    slideSlot = 0                    // no slide
  else:
    slideSlot = 1 + cellRng.nextRange(N)   // pick from LUT slots 1..N
```

`motifLength` derived once per cache rebuild from rhythmSeed (e.g. `2 + (seedHash & 0x3)` → 2..5). Constant within one rebuild so the motif boundary bias is stable across the cycle. Independent across rebuilds — same seed produces same motifLength deterministically.

Knob behavior (one Slide knob, two outputs):
- Knob = 0 → every cell scores below threshold → all `slideSlot = 0` → no slides.
- Knob = 50 → mid threshold. Cells with small intervals score above; cells with leaps score below. Slides land on legato-friendly transitions. Slot choice randomized across {1..N} so time varies per cell.
- Knob = 100 → every cell scores above. Every cell slides. Time still varies via slot pick.

Engine at trigger time (read from cache cell):
- `cell.slideSlot()` (code) tells which slot.
- If slot == 0 → no slew on this event (gate-on without slide).
- Else `effectiveTime = slideLut[slot] * track.slideTime()`. Call `Slide::applySlide` (code) with that time.

Live mode: engine rolls slideSlot per event via `_rng`, writes to `event.slideSlot`. Same score logic if engine has access to next-pitch lookahead (cache does; engine writes ahead-of-time may not). Simplest Live: score-free Bernoulli at knob threshold + random slot pick. Loop mode: score-modulated pick via keyed RNG as above.

Net knobs (ui):
- **Slide** (ui) on LIVE pitch row — single control, drives both on/off density and time-slot distribution per cell.
- **SlideTime** (ui) on STOCH CFG — global slide-character scalar (existing). Slots multiply this.

Storage delta: drop existing `event.slide` (code) bit, add `event.slideSlot` (code) 3-bit field. Net +2 bits per event.

Audible test cases (golden):

```
SlideLaw:
  case A: Slide = 0, sequence has mixed adjacent and leap intervals
    no slides anywhere
  case B: Slide = 50, sequence has mostly adjacent intervals
    most cells slide; time slots distributed across {1..N}
  case C: Slide = 50, sequence has mostly leap intervals
    few cells slide (negative score boost suppresses)
  case D: Slide = 100
    every cell slides, time slots distributed
  case E: same as B but knob scrubbed to 70 and back to 50
    deterministic — slide pattern at 50 matches the first run
```

Score-only contribution (Approach 2, vinx-exact top-K sort by score, exact density): noted but not chosen — Approach 1 fits the per-cell streaming cache walk; Approach 2 would require a global sort per rebuild. Possible upgrade if exact-density behavior matters more than per-cell determinism later.

## Test plan

Before changing any formula, capture **golden traces** of generation output at canonical knob settings:

```
TestStochasticPitchGolden:
  for each (complexity, contour, marblesSpread, marblesBias, stepsSieve) in canonical set:
    seed RNG with fixed seed
    generate 16 cells starting from lastDegree=0 (C)
    record degree sequence + per-cell weight distribution
```

Canonical set: each knob at {0, 25, 50, 75, 100}, all others at default. ~25 cases. Captures audible behavior at known points.

Add targeted parameter fixtures for the contour edge case:

```
ContourEdgeFixture:
  scale = C major
  range = 1
  all pitch tickets = 0
  stepsSieve = 92
  marblesBias = 17
  marblesSpread = 53
  complexity = 9

  case A: contour = +24
    generate 64 cells
    assert same-degree run length never exceeds 3 unless no alternate survivor has weight
    assert sequence can release downward after touching B

  case B: contour = -24
    generate 64 cells
    assert same-degree run length never exceeds 3 unless no alternate survivor has weight
    assert sequence can release upward after touching C
```

Add an isolation view to the fixture output: fixed RNG stream, before/after weight distribution, and highlighted same-degree run lengths. This matches the HTML playground used to discover the issue and avoids confusing "new dice luck" with formula effect.

After each formula change, regenerate golden traces and audit: "does this change make musical sense at points where the old behavior was already correct?"

## Scope of audible change

Phase 15 *will* change audible behavior. That's the point — today's defaults aren't musical. Communicate clearly in the patch:
- "Defaults now produce C-D-E-F-style walks under low complexity (today: drift to F#)."
- "Octave doubling in the kernel — same note in next octave is now a high-probability candidate."
- "stepsSieve no longer hard-excludes; partial values produce soft preference."

Existing user presets that depended on the old weirdness may sound different. Migration: snapshot before/after on stock presets, document deltas in the release notes.

## Phasing relative to 14B

Phase 15 starts only after Phase 14B is **fully landed and hardware-verified**. Reasoning:
- 14B is a storage architecture change; 15 is an audible change.
- Bundling = can't isolate regressions.
- Golden-trace tests written for 15 also serve as parameter-coverage tests (`PHASE14B-SEED-LOG.md`, "Parameter coverage test"), so write them at the Patch B → C boundary anyway.

## Open questions

1. Should duration law land in Phase 15 or split to its own Phase 15R patch? The accepted law is documented above and should not be left as an open-ended "maybe touch duration" item.
2. Marbles bell as additive vs multiplicative — change shape or rebalance constants?
3. Is `universalDegreeBoost` (the sieve ranking function) right for non-chromatic scales? The kernel anchors at N/2, N/3, N/4 fractions of `activeNotes` — fine for chromatic and major, but for pentatonic (5 notes) and microtonal scales these anchors land on weird positions.
4. Should `generateDegree` be deterministic-per-cell (Phase 14B keyed RNG) or carry `lastDegree` (today)? Phase 14B's plan is "replay melody pass for [N..end] after REGEN_CELL"; Phase 15 could revisit this if a per-cell-anchor approach feels more musical.

## Next step

Wait for 14B to land. Then:
1. Write the golden-trace test harness as part of Patch B prep.
2. Capture baselines.
3. Open Phase 15 as a separate branch.
