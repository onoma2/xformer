# Phase 10.7: L1/L2/L3 Redesign (Proteus-Style Macros + 3-Way Exclusivity)

## Provenance

Synthesized 2026-05-20 from a multi-round grilling session against the references:
- **SIG** (Stochastic Inspiration Generator) — ticket hierarchy + Loop + secondary articulations
- **Proteus** — single-knob Complexity + Density + Patience + Mutate
- **Marbles** — Shape/Spread/Bias/Steps + Deja vu permutation
- **Melodicer** — Rate+Variation rhythm pair + DICE/LOCK + dice/realtime = Loop/Live
- **Tuesday** — Burst (child hits inside parent)

This document supersedes Cluster C of `PHASE10.6-BUG-CATALOG.md` with a complete L1/L2/L3
redesign. The bug catalog remains the source of truth for Clusters A (serialization) and B
(ticket-mode state machine), which are unaffected by this redesign.

## Helicopter View

> "Stochastic track attempts to capture all the best with its levels and
> loop/patience/mutate and tickets and stuff. Everything stochastic integrated in
> Performer's infrastructure of patterns, routing, tick-based clock etc."

Stochastic = SIG (tickets) + Proteus (macros) + Marbles (shape) + Tuesday (burst) +
Melodicer (rate/variation) — wired into Performer's pattern/clock/routing/tick layer.

## Design Principles (Confirmed)

1. **Proteus archetype at L1** — single-knob macros driving sophisticated algorithms,
   not edit-time fan-outs.
2. **L1 = "3 knobs for fun." L3 = "microtonal experimental playground."**
3. **Simple > clever.** Where consistency between levels can't be maintained cleanly,
   prefer explicit semantics over magic preservation.
4. **Macros are 1-to-1 with single fields.** Engine sophistication, not edit clobbering.
5. **Engine-clever derivation:** L1 macro writes one field; engine internally derives
   effective behavior for L2 sub-controls when those are at default; L2 user edits
   override when present.
6. **3-way exclusivity for pitch generation:** Tickets > Marbles > Complexity.
7. **Looper is universal** — visible/editable at all levels.
8. **Per-pattern recall** — sequence-owned controls survive pattern switches.

## L1 — Three Generator Knobs

Active at L1 by default (Shape=Off):

| Knob | Field written | Engine role |
|---|---|---|
| **Density** | `_density` (0..100) | Sound vs rest amount; engine-clever to Rate/Variation/Rest at default |
| **Complexity** | `_complexity` (0..100) | Melodic richness; engine-clever to Contour/Linearity at default |
| **Shape** | `_marblesMode` (Off/On) | Submode toggle: when On, Marbles algorithm replaces Complexity |

When **Shape=On**, the visible L1 generator surface becomes:

| Knob | Field |
|---|---|
| Density | `_density` |
| Shape | `_marblesMode` (On) |
| Spread | `_marblesSpread` |
| Bias | `_marblesBias` |
| Steps | `_marblesSteps` |

Complexity is bypassed when Shape=On.

### Naming change

`Character` → `Complexity` at all levels. One name, one field. Matches Proteus.

## L2 — Direct Sub-Controls

L2 adds (on top of the L1 surface) the direct controls that each macro implicitly drives:

| Under L1 macro | L2 adds |
|---|---|
| Density | Rate, Variation, Rest |
| Complexity | Contour, Linearity |
| Shape (when on) | Spread, Bias, Steps (same as L1 surface) |
| (independent) | Burst, BurstCount, BurstRate, BurstPitch |
| (independent) | Legato, Slide |
| (independent) | Range, MinDegree, MaxDegree |
| Mode | Rhythm (live/loop), Melody (live/loop) — split source modes |

**Dropped from current implementation:** `_accent` and its routing target. No use case
for second-gate accent in target patches; saves RAM + UI lines.

### Engine-clever derivation contract

When an L2 field is at its sentinel-default value, the engine derives its effective
value from the L1 macro. When the user has edited the L2 field (it's no longer at
default), the explicit user value overrides.

Concretely:
- `_density` defaults to 100. When user moves Density at L1, `_density` changes. Engine
  derives effective `_rate`/`_variation`/`_rest` based on `_density` when those L2 fields
  are at their defaults (TBD per implementation tuning).
- When user moves Rate at L2 (i.e. `_rate != default`), Rate is read directly; derivation
  doesn't apply.
- Same pattern for Complexity → Contour/Linearity.

**Exact derivation curves are deferred to Phase 2 implementation.** Will be designed and
tuned on hardware. The architecture is fixed; the curve mapping is open.

## L3 — Truth Tables (Advanced)

L3 adds the explicit per-element programming tables:

| Control | Field | Edited via |
|---|---|---|
| Pitch Tickets | `_degreeTickets[CONFIG_USER_SCALE_SIZE]` | StochasticSequenceEditPage (bar chart) |
| Duration Tickets | `_durationTickets[8]` | StochasticSequenceEditPage (separate page) |
| Degree Rotate | `_degreeRotation` | List model |
| Ticket Rotate | `_maskRotation` | List model |

L3 also exposes all L2 sub-controls for direct programming.

## Universal — Looper + Performance (All Levels)

Always visible:
- **Mode** (Live/Loop, couples both domains)
- **Refresh** (manual loop repopulate, Melodicer-DICE-style)
- **Size**, **First**, **Last**, **Rotate** (loop window + offset)
- **Patience** (auto-refresh interval; 100 = never; exponential 1..99 mapping per PHASE10)
- **Mask**, **Tilt** (deterministic loop thinning)
- **Mutate** (bipolar -100..+100: see below)
- **Lock** (transient output freeze — engine-only, not serialized)

## Performer-Native Controls (preserved at all levels)

These are part of Performer's standard track interface. They are sequence-owned, persist
through the redesign, and remain editable at any level. The redesign must not remove
or hide them:

- **Divisor** (`_divisor`) — step duration in clock ticks (rate-mode grid)
- **Clock Multiplier** (`_clockMultiplier`) — per-sequence clock rate scaler
- **Reset Measure** (`_resetMeasure`) — measure-based pattern restart
- **Scale** (`_scale`) — scale selection (overrides project default if set)
- **Root Note** (`_rootNote`) — root (overrides project default if set)

Note: `_divisor` and `_clockMultiplier` are intentionally ignored in duration-ticket
mode (tickets express absolute musical duration). They still apply in rate mode and
remain editable.

## Engine Rules

### Pitch generation (3-way exclusive)

In `StochasticGenerator::generateDegree`:

```
if (sequence.pitchTicketsActive()) {
    // Use ticket weights directly. Bypass Complexity/Contour/Linearity/Marbles.
    return sample_from_ticket_cdf();
}

if (sequence.marblesMode() == MarblesMode::On) {
    // Marbles scale-aware algorithm.
    weights = compute_scale_aware_marbles_weights(scale, spread, bias, steps);
    return sample_from_marbles_cdf(weights);
}

// Complexity path.
weights = uniform_over_allowed_degrees();
apply_complexity_penalty(weights, _complexity, lastDegree);
apply_contour_bias(weights, effective_contour);  // derived if at default
apply_linearity_pull(weights, effective_linearity, lastDegree);  // derived if at default
return sample(weights);
```

Today's code doesn't enforce this exclusivity — Complexity penalties still modify ticket
weights when both are set. The fix adds the explicit `pitchTicketsActive()` early-exit.

### Duration generation (2-way)

```
if (sequence.durationTicketsActive()) {
    return select_duration_from_tickets();
}

// Rate + Variation path.
return rate_with_variation(_rate, _variation);
```

Today's code applies Variation in BOTH paths (bug H1). The fix gates Variation on
`!durationTicketsActive()`.

### Duration ticket dictionary (revised 2026-05-20)

Shift-down dictionary with straight + triplet emphasis. Replaces the original PHASE7
table (1/2, 1/4, 1/8, 1/16, 3/16, 5/16, 7/16, 1/8T) which felt "too slow."

| Slot | Duration | Ticks (PPQN=192) | Math |
|---|---|---|---|
| 0 | 1/4   | 192 | PPQN |
| 1 | 1/8   |  96 | PPQN/2 |
| 2 | 1/16  |  48 | PPQN/4 |
| 3 | 1/32  |  24 | PPQN/8 |
| 4 | 1/64  |  12 | PPQN/16 |
| 5 | 1/8T  |  64 | PPQN/3 (3 notes per quarter) |
| 6 | 1/16T |  32 | PPQN/6 (3 notes per eighth) |
| 7 | 3/16  | 144 | PPQN·3/4 (dotted-eighth feel) |

PHASE7-DICTIONARY.md (controlling spec) is amended in this redesign to match.

Implementation: update `StochasticTrackEngine::getDurationMultiplier` (lines 26–38) and
the UI labels in `StochasticPerformanceListModel.h` / `StochasticSequenceEditPage.cpp`
duration-ticket slot labels.

### Marbles scale-aware Steps law

For arbitrary scale sizes (2..32 degrees), compute weights from relative degree position:

| Degree position in scale | Weight |
|---|---|
| Root (i = 0) | 255 |
| Middle (i ≈ N/2) | 192 |
| Quarter positions (i ≈ N/4, 3N/4) | 160 |
| Intermediate | 128, 96, 64 (cascading) |
| Adjacent to root (i = 1, i = N-1) | 32 |

Apply Marbles masking formula:
```
amount = steps_knob_position / 100.0
mask = clamp(8.0 * (weight/255.0 - amount) + 0.5, 0.0, 1.0)
final_weight = weight * mask
```

Build CDF, apply Spread/Bias beta distribution, sample. Closes bug H5.

### Bipolar Mutate (Marbles deja-vu + Proteus destructive in one field)

`_mutate` is repurposed from `Routable<uint8_t>` (0..100) to `Routable<int8_t>`
(-100..+100). Migration cost: existing dev-project save files misload (already accepted
per PHASE9-V4 line 192).

Per-step evaluation in `StochasticTrackEngine::triggerStep`:
- `mutate == 0` — Locked linear playback.
- `mutate > 0` — Non-destructive permutation (Marbles deja-vu).
  Probability `p = (mutate / 100.0)²` to jump to a random index in the loop buffer.
- `mutate < 0` — Destructive single-step replacement (Proteus mutation).
  Probability `p = (|mutate| / 100.0)²` to regenerate one step.

Replaces the current single-direction mutation logic at `TrackEngine.cpp:438-447`.

### Density combination law

`_density` (sound chance) is orthogonal to `_rest` (explicit silence). Existing engine
law in `generateRhythmEvent`:
```
densityGate = (rng < density)
restGate = (rng < rest)
event.setRest(!densityGate || restGate)
```
This is correct: density gates sound, rest adds explicit silence on top. No change needed
here. The Cluster C fix is removing the `editDensityMacro` fan-out to `_rest`.

### Variation gating (H1 fix)

`StochasticTrackEngine.cpp:307-316` applies Variation to `mult` unconditionally. Variation
is L2-only and is replaced by Duration Tickets at L3. Fix: gate the variation block on
`!sequence.durationTicketsActive()`.

### Intentional design choices (NOT bugs)

Recorded so future agents don't try to "fix" these:

- **Divisor ignored in duration-ticket mode.** Tickets express absolute musical duration;
  divisor sets the rate-mode step. Two modes, two semantics.
- **clockMultiplier ignored in duration-ticket mode.** Same rationale.
- **PHASE7-DICTIONARY.md:224** ("use the sequence clock divisor to make the whole track
  slower") needs amendment — this does not apply in ticket mode.

### Parked (decide later)

- **Sleep in ticket mode.** Currently `_sleepRemaining` only decrements in Free/Aligned
  branches. Decide whether ticket mode should consult it later.

## Ownership

### Sequence-owned (per-pattern, serialized)

`_density`, `_complexity`, `_contour`, `_linearity`, `_marblesMode`, `_marblesSpread`,
`_marblesBias`, `_marblesSteps`, `_rate`, `_variation`, `_rest`, `_legato`, `_slide`,
`_range`, `_minDegree`, `_maxDegree`, `_mutate` (bipolar), `_patience`, `_mask`, `_tilt`,
`_rotate`, `_size`, `_first`, `_last`, `_level`, `_durationTickets[8]`, `_degreeTickets[32]`,
`_rhythmMode`, `_melodyMode`, `_burst`, `_burstCount`, `_burstRate`, `_burstPitch`,
`_degreeRotation`, `_maskRotation`, `_sleep`, `_jump`, `_events[]`, `_rhythmValid`,
`_melodyValid`, `_rhythmSeed`, `_melodySeed`.

### Track-owned

`_octave`, `_transpose`, `_slideTime`, `_cvUpdateMode`, `_fillMode`.

### Engine-only (not serialized)

`_lock`, `_lockedParents[]`, all `_relativeTick`/`_patternIndex`/`_eventElapsed` runtime
state, `_rng` state.

### Dropped (Cluster L1 cleanup)

`_accent` + `_accentProb` + accent routing target.
Track-side: `_gateBias`, `_retriggerBias`, `_lengthBias`, `_noteBias`, `_loopFirst`,
`_loopLast`, `_modeInternal`, `_fillMuted` (engine never reads).
Sequence-side: `_reservedJitter`.

## UI Implementation Surface

The redesign touches two surfaces:

| Surface | Role |
|---|---|
| `StochasticPerformanceListModel` (single list-model page) | All macros + L1/L2/L3 list items + universal looper |
| `StochasticSequenceEditPage` | L3 graphical ticket editing (pitch tickets bar chart + duration tickets table) |

`_level` field on sequence determines which items the list model shows:
- L1: 3 generator knobs + universal looper. Pitch tickets edited via StochasticSequenceEditPage only.
- L2: L1 + sub-controls (Rate/Variation/Rest, Contour/Linearity, Burst suite, Legato/Slide, Range/Min/Max).
- L3: L2 + degree/ticket rotations + advanced loop ops.

Existing `coreItems[]`, `directItems[]`, `weightsItems[]` arrays in
`StochasticPerformanceListModel.h` need to be re-curated against this surface.

## Implementation Order (Phase 2)

Suggested ordering:

1. **Cluster A** (Serialization) — independent prerequisite. Add `_density`,
   `_durationTickets`, `_level` to write/read; drop `_lock` from Track write/read.
2. **Cluster B** (Ticket-mode state) — independent prerequisite. Fix C5/B2/B3 reset
   semantics + H1 variation gating.
3. **Cluster D** (Burst gate) — independent fix. Parent gate-OFF no longer masks
   child ONs. Recommended: skip parent OFF push when `hasChildren`, let last child's
   OFF be the natural gate-low (parent gate becomes the burst envelope).
4. **Duration dictionary** — update `getDurationMultiplier` to new 8-slot table
   (1/4, 1/8, 1/16, 1/32, 1/64, 1/8T, 1/16T, 3/16). Update UI labels.
5. **L1 redesign — drop the fan-outs.**
   - `editDensityMacro` writes `_density` only.
   - `editCharacterMacro` → rename → `editComplexityMacro`, writes `_complexity` only.
   - Rename UI label "Character" → "Complexity".
6. **3-way exclusivity in `generateDegree`.** Add `pitchTicketsActive()` early-exit.
7. **Scale-aware Marbles Steps law.** Implement weight-from-relative-position +
   masking formula. Closes H5.
8. **Bipolar Mutate.** Change `_mutate` storage to int8_t. Add permutation (+) and
   destructive (-) per-step evaluation.
9. **Engine-clever derivation.** Add sentinel-default detection for `_rate`/`_variation`/
   `_rest` (under Density) and `_contour`/`_linearity` (under Complexity). Wire the
   derivation curves; tune on hardware.
10. **Cleanup pass.** Drop `_accent`, dead Track residue, `_reservedJitter`,
    `#define DBG_STO_ENABLE` (Cluster L2).
11. **List model re-curation.** Update `coreItems[]`/`directItems[]`/`weightsItems[]`.

Each step is independently testable. TDD entry point per cluster.

## Acceptance

- L1 with all defaults (Density=100, Complexity=50, Shape=Off, no tickets) produces
  audible output via the Complexity algorithm.
- Setting any pitch ticket > 0 immediately bypasses Complexity and Marbles for pitch
  generation. Macros still affect rhythm density.
- Enabling Shape bypasses Complexity entirely. Spread/Bias/Steps drive distribution.
- Setting L2 Rate to a non-default value overrides density's Rate derivation.
- Mutate at +50 produces audible non-destructive jumps; at -50 produces audible
  destructive replacements; at 0 produces stable locked playback.
- Project save/load preserves Density, Complexity, Duration Tickets, Level, all sequence-
  owned fields. Does NOT preserve Lock.
- Switching patterns recalls per-pattern level alongside other sequence material.
- Refresh repopulates Loop source domains without changing Patience.

## References

- `PHASE7-DICTIONARY.md` — vocabulary contract (needs amendment re: divisor in ticket mode)
- `PHASE9-V4-OWNERSHIP.md` — ownership rules (Lock = engine-only, transient)
- `PHASE10-V5-CONTROL-GRANULARITY.md` — original V5 spec; this doc revises the L1 macro mechanics
- `PHASE10.6-BUG-CATALOG.md` — bug catalog; this doc resolves Cluster C and informs A/B
- `temp-ref/docs/ProteusManual.pdf` — single-knob macro archetype
- `temp-ref/docs/sig.pdf` — ticket hierarchy + secondary articulations
- `temp-ref/docs/marbles.pdf` — Shape/Spread/Bias/Steps + deja-vu
- `temp-ref/docs/melodicer manual en 1.1 web.pdf` — Rate+Variation pair, dice/realtime modes
