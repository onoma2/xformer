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

1. Should Phase 15 also touch `pickDuration` and burst LUTs? Triangle scaling there has the same ×10 cliff issue. **Probably yes — same review pass.**
2. Marbles bell as additive vs multiplicative — change shape or rebalance constants?
3. Is `universalDegreeBoost` (the sieve ranking function) right for non-chromatic scales? The kernel anchors at N/2, N/3, N/4 fractions of `activeNotes` — fine for chromatic and major, but for pentatonic (5 notes) and microtonal scales these anchors land on weird positions.
4. Should `generateDegree` be deterministic-per-cell (Phase 14B keyed RNG) or carry `lastDegree` (today)? Phase 14B's plan is "replay melody pass for [N..end] after REGEN_CELL"; Phase 15 could revisit this if a per-cell-anchor approach feels more musical.

## Next step

Wait for 14B to land. Then:
1. Write the golden-trace test harness as part of Patch B prep.
2. Capture baselines.
3. Open Phase 15 as a separate branch.
