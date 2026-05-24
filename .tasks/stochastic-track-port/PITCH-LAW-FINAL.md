# Pitch law — final design

_Captured 2026-05-24. Supersedes the multiplicative "final weight" formulas
in PHASE15-PITCH-MATH-REVIEW.md (sections 11 / 12). MaskM / TiltM stay
out of this — they are trigger-time audibility, not part of generation._

## Five sequential decisions

Each step makes its own choice and passes results forward. No back-feeding
into earlier steps. Tickets are blind to motion; motion consumes upstream
output without re-weighting tickets.

### Step 1 — Class roll

Pick scale-degree class (mod activeNotes).

- Inputs: Pitch Tickets (−1 exclude / 0 flat / 1..100 weighted), recent
  class history.
- Process: `weight[class] = ticketAmount × recencyPenalty(classRunLength)`.
  Weighted random over non-excluded classes.
- Recency penalty curve: `100 / 55 / 20 / 5` for run lengths 0 / 1 / 2 / 3+.
- Blind to lastPitch, Complexity, Contour, Range, Bias, Spread.

### Step 2 — Class repeat check

Resolves Complexity's "uniformity vs movement" semantic.

- If `class != lastClass` → accept.
- If `class == lastClass` → roll `accept = (rng < Complexity)`; on reject,
  return to Step 1 with this class added to recency.
- Bounded retries (max 3) so heavy single-ticket emphasis never hangs the
  picker.
- Low Complexity → repeats kept (stepwise feel). High Complexity → forced
  movement.

### Step 3 — Pitch candidate set

Build the set of absolute pitches `(class + octave × activeNotes)` from the
chosen class. Range owns this.

- `Range = 50` → single pitch at `currentOctave`.
- `Range > 50` → `width = 1 + ((Range − 50) × 3 / 50)` octaves above
  `currentOctave`. Up to 4 octaves at Range = 100.
- `Range < 50` → single octave baseline; per-slot displacement chance
  `(50 − Range) × 2`%. If rolled, set includes `±1` octave neighbors.
- Output: sorted-ascending list of absolute pitches.

### Step 4 — Region roll

Pick one pitch from the set via continuous beta-distribution shaped by
Bias + Spread.

- Inputs: sorted pitch set, Bias, Spread.
- Process: `x = BetaDistributionSample(rng, Spread, Bias)`. Map
  `x ∈ [0..1]` to a position in the sorted set. Pick the pitch at that
  position.
- Bias = 0 → favors lowest. Bias = 100 → favors highest. Bias = 50 →
  centered.
- Spread low → collapse to Bias. Spread mid → cloud around Bias. Spread
  high → Bernoulli-like edges.
- **Step 4 is octave-blind.** It consumes a flat list. Set size 1
  (Range = 50 default) → trivial pick, Bias/Spread are no-ops. Set size
  2..N → meaningful shaping.

### Step 5 — Direction nudge

Contour applies direction preference vs lastPitch.

- Inputs: chosen pitch (Step 4), full pitch set (Step 3), lastAbsolutePitch,
  Contour (−100..+100).
- Process: `direction = sign(pitch − lastPitch)`. If Contour sign matches
  → accept. If mismatch → roll `swap = (rng < |Contour|)`; on hit, replace
  with the nearest pitch in the set that goes the desired direction. If no
  such pitch exists → accept original.
- **Step 5 is also octave-blind.** Consumes the set as a flat list.

## Knob ownership

| Knob          | Step | Decision                                       |
|---------------|------|------------------------------------------------|
| Pitch Tickets | 1    | Probability per class                          |
| (recency)     | 1    | Automatic anti-stutter                         |
| Complexity    | 2    | Repeat tolerance                               |
| Range         | 3    | Pitch candidate set construction               |
| Bias          | 4    | Set position preference                        |
| Spread        | 4    | Set position concentration                     |
| Contour       | 5    | Direction preference vs lastPitch              |

Each knob owns exactly one decision. No multiplicative tangling.

## Independence contract

- Tickets define probability over classes only. Blind to motion.
- Motion knobs (Complexity, Contour) never re-weight tickets.
- Region knobs (Bias, Spread) and direction knob (Contour) consume flat
  pitch sets without knowing about octave structure.

## Anti-collapse promise

User can turn one ticket way up to emphasize a class. The line emphasizes
that class but never collapses to a single repeated note. Mechanisms:

- Step 1 recency penalty divides the favored class's weight after each
  play. Run length 3+ cuts it to 5% of nominal.
- Step 2 forced re-roll under high Complexity overrides ticket dominance
  when the user wants movement.
- Step 3 multi-octave fan-out at Range > 50 spreads the same class across
  octaves, producing audible motion even if class is fixed.
- Bounded retries in Steps 2 and 5 prevent infinite loops.

## Transparent defaults

```
Scale          = chromatic / last-used
Root           = C
Pitch Tickets  = all 0  (default-flat; recency still active)
Complexity     = mid    (to be calibrated; ~50% repeat-rejection rate)
Range          = 50     (single-pitch field, Steps 3..5 inert)
Bias           = 50     (centered)
Spread         = high enough to be near-uniform (likely 70..80)
Contour        = 0      (no direction preference)
```

At all defaults: only Steps 1 and 2 affect output. Steps 3..5 are no-ops
because the candidate set has one element. Predictable musical baseline.

## Open calibration questions

- Complexity-to-repeat-probability curve. Linear `accept = Complexity / 100`?
  Or a curve that compresses the middle?
- Spread default value. Depends on beta-distribution calibration — needs
  empirical pass with golden traces.
- Optional safety nets (decide before implementation):
  - Soft-cap ticket curve: map 0..100 to weight 1..4 via log so a single
    knob can't dominate. Hard exclusion still via `-1`.
  - Per-class minimum floor: every non-excluded class gets `≥ 1/N` weight
    summed into total; prevents accidental starvation.

## Out of scope here

- MaskM / TiltM (trigger-time audibility filter, separate concern).
- Duration / Variation / Rest law (rhythm, see PHASE15R section).
- Slide law (per-cell articulation, see PHASE15 Slide section).
- Marbles-mode toggle (the continuous beta-distribution always runs here).
