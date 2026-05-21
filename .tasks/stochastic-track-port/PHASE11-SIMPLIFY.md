# Phase 11 — Simplification Refactor

_Drafted 2026-05-21. Status: planned, not implemented._

## Why

Current stochastic surface carries:
- 3 mutually exclusive pitch picker modes (current weight-mod / Marbles / future Proteus)
- 7 pitch knobs (Complexity, Linearity, Contour, Bias, Spread, Steps, Marbles toggle)
- 2 mutually exclusive duration modes (tickets vs noteDuration+variation)
- 3 UI Levels (Core/Direct/Weights) gating which params are visible
- 2 hardcoded label tables that lie when divisor != 1/16

Net: hard to dial in a sound, knobs interact non-obviously, UI hides things behind modes.

Goal: collapse to **one always-on picker per domain**, **multiplicative tickets as the base weight**, **flat UI list**, **divisor-aware labels everywhere**.

## Final control surface (target state)

### Rhythm domain
- **Note Duration** = duration-focused Bias (kernel center on 8-slot divisor-relative LUT)
- **Variation** = duration-focused Spread (kernel width, 0..100 symmetric)
- Rest, Slide, Legato, Accent (Bernoulli per-event)
- Burst, Burst Count, Burst Rate, Burst Pitch
- Duration tickets (per-slot multiplicative base weight LUT)

### Pitch domain
- **Complexity** = move-distance kernel from lastDegree (linearity baked in)
- **Bias** = Marbles distribution center (continuous bipolar around 50)
- **Spread** = Marbles distribution width
- **Steps** = pitch sieve (universal LUT rank cutoff, 0..100, high=open)
- Range / Min Degree / Max Degree (allowed-degree window)
- Pitch tickets (per-degree multiplicative base weight LUT)

### Cross-domain
- Sleep, Patience (Poisson CDF), Mutate (bipolar permute/destructive), Jump (Proteus walk)
- Mask + Tilt (rhythm step-rank sieve, parallel to Steps)

### Pages (unchanged)
- CORE / MARBLES / DIRECT / LOOP hero pages cycled by F5 NEXT
- Duration ticket editor page, Pitch ticket editor page, Burst Pitch ticket editor page

## Symmetry locked in by the refactor

| | Hard sieve | Distribution shape | Soft per-element weight |
|---|---|---|---|
| Rhythm | Mask + Tilt | (none — Mask is the only sieve) | density tickets, Rest |
| Pitch  | Steps (universal LUT cutoff) | Marbles Bias + Spread | pitch tickets |
| Duration | (none — duration is always emitted) | noteDuration (Bias) + Variation (Spread) | duration tickets |

| | Bias (center) | Spread (width) | Kernel from "last" |
|---|---|---|---|
| Pitch | Marbles Bias | Marbles Spread | Complexity |
| Duration | noteDuration | Variation | — |

## Drop list

| Surface | Item | Outcome | Reserved slot? |
|---|---|---|---|
| Engine | `densityGate` Bernoulli roll | already removed 2026-05-21 | yes (`_density`) |
| Engine | `contour` math | covered by Marbles Bias | yes (`_contour`) |
| Engine | `linearity` math | folded into Complexity kernel | yes (`_linearity`) |
| Engine | `variation` sign | becomes symmetric 0..100; load-time `abs()` migration | storage stays signed for forward compat |
| Engine | `marblesMode` toggle | always-on; transparent defaults | yes (flag stays in save/load) |
| Engine | mutate-rhythm directional bias from variation sign | follows from variation sign drop | code change only |
| Engine | `durationTicketsActive` mode branch | tickets always multiplicative base | flag dropped entirely (engine + UI) |
| Model  | `StochasticLevel` enum branches at UI level | flat list, no level gating | enum stays as reserved field, ignored |
| UI lists | `coreItems[]` / `directItems[]` / `weightsItems[]` | collapse to single flat list | code deletion |
| UI labels | hardcoded `durationTicketLabels[]` | replace with runtime `ModelUtils::printDivisor(divisor * num / den)` | static array deleted |
| Vestigial | `_marblesSteps` permutation reseed (dead code, never read by engine) | repurposed as the **pitch Steps sieve cutoff** | field reused, clamp relaxed 0..100, default 100 |
| Engine | `minDegree` / `maxDegree` integer clamps in `generateDegree` and `mutateMelodyOne` | redundant with Range (octave coverage) + pitch tickets at 0 (per-degree exclude) + Spread/Bias (soft window) | yes (`_minDegree`, `_maxDegree` storage + getter + setter + serialization kept; routing target stays) |

## Engine math (target sketches)

### Pitch picker (single unified path)

```cpp
// 1. Allowed degree window
buildAllowedDegrees(minDegree, maxDegree, range);

// 2. Steps sieve (universal LUT cutoff)
//    Rank each allowed degree by universalDegreeBoost(degInOct, N).
//    Keep top K = clamp(allowedCount * steps() / 100, 1, allowedCount).
//    Survivors become the picker domain.

// 3. Per-survivor weight: multiplicative combine
for each survivor s:
    base    = sequence.pitchTicket(s)           // base LUT, defaults flat 10
    kernel  = complexityKernel(s, lastDegree, complexity, N)
    marbles = marblesShape(s, bias, spread)     // beta-derived weight at degree s
    w(s)    = base * kernel * marbles

// 4. Weighted random pick over survivors.
```

`complexityKernel`:
```cpp
int dist = |s - lastIdx|;
int width   = 1 + (complexity * N) / 50;
int leakage = (complexity * baseUniform) / 100;
return max(0, width - dist) + leakage;
```

Low complexity → tight kernel, REPEAT/±1 dominant, "soft gravity" (linearity-equivalent).
High complexity → wide kernel + leakage, leaps allowed, NEW-dominant.
Mid → stepwise motion within a small window.

### Duration picker (single unified path)

```cpp
for each LUT slot i in 0..7:
    base    = sequence.durationTicket(i)         // defaults flat 10 if all zero
    center  = sequence.noteDuration()            // 0..7
    width   = 1 + (variation * 4) / 100          // ~1..5 slots wide
    kernel  = max(0, width - |i - center|)
    w(i)    = base * (kernel + leakage)
roll weighted pick over the 8 slots → durationIndex
```

No more `durationTicketsActive` branch. Tickets always combine. When all tickets are 10 (flat default), they multiply uniformly and contribute nothing distinctive — same audible behaviour as today's "tickets off" path.

## UI changes

- Drop the `marblesMode` toggle from the L1 list and the Marbles hero page. The page just shows Bias / Spread / Steps directly (Steps now meaning "pitch sieve cutoff").
- Drop the three `coreItems` / `directItems` / `weightsItems` arrays. One flat `items[]` listing every sequence-level param, sorted by domain.
- `printNoteDuration` already divisor-aware. Mirror that into `drawDurationPage` ticket labels — compute on the fly from `sequence.divisor()` and the LUT `{num, den}` per slot.
- Remove `durationTicketsActive()` references from the hero pages — ticket editor is always visible.
- Rename UI label "Marbles Steps" → "Steps" (or "Tones" / "Sieve" — pick during implementation).

## Migration notes

- Patches with `variation < 0`: load-time `abs()`. They lose the "always-shorter" directional pull. Will hear duration spread instead of skew. Acceptable.
- Patches with `marblesMode == Off`: load unchanged. Engine now always runs the distribution. With `bias=50, spread=100, steps=100` (transparent defaults), output is sonically identical to today's "Marbles off" path on existing patches that didn't touch those values.
- Patches with `marblesSteps < 100`: previously dead, now engages the pitch sieve. Audible change.
- Patches with `contour != 0` or `linearity > 0`: those engine paths gone. Sound flattens slightly. The patch was using a knob that no longer exists; reserved field stays for future use.
- Patches that route to `StochasticGeneratorDensity`, `StochasticContour`, `StochasticLinearity`: routing writes succeed (fields update) but have no audible effect.

## Order of commits

1. **Engine drop** — pure deletion: contour, linearity, marblesMode branch, densityTickets branch, variation sign. Tests stay green (test the picker doesn't crash, output is reasonable; we don't have golden audio tests for these laws).
2. **Steps sieve wire** — extract `universalDegreeBoost` to a shared helper, add sieve step in `generateDegree`, repurpose `_marblesSteps` clamp.
3. **Complexity kernel rewrite** — replace the current complexity/contour/linearity weight modulation with the move-distance kernel sketch above.
4. **Duration kernel wire** — implement multiplicative combine in `selectDurationTicket` / `generateRhythmEvent`. Kill `durationTicketsActive` branch.
5. **UI flat list** — collapse the three `items[]` arrays into one. Drop Level-gated cell rendering.
6. **Divisor-aware ticket labels** — runtime label formatter in `drawDurationPage`.
7. **Dictionary update** — sweep PHASE7-DICTIONARY.md with all the new semantics and the reserved-slot table.

Each commit independently sim-builds + STM32-builds + passes the 4 stochastic test suites. No hardware required to verify (the laws are testable in sim).

## Follow-up adjustments (post-implementation)

- **Boredom indicator math sync (2026-05-21).** Old V5 `2^bucket` formula
  saturated the on-screen "boredom" bar at 100% within a few loops while the
  Poisson engine kept waiting for many more loops. Indicator now calls
  `StochasticTrackEngine::patienceProbability(loops, patience)` — same math
  as the engine roll. At patience=100 (off sentinel) the indicator reads 0%.
- **Divisor-aware short ticket labels (2026-05-21).** Hardcoded
  `durationTicketLabels[]` array removed. `StochasticSequence::printSlotDuration()`
  now computes per-slot labels at draw time from `divisor × LUT fraction`
  via new `ModelUtils::printDivisorShort()` (omits the leading raw-tick value;
  outputs just `"x/y"` / `"x/yT"`).
- **MinDegree / MaxDegree dropped silently (2026-05-21).** Engine clamps in
  `generateDegree` and `mutateMelodyOne` removed; controls removed from the
  flat UI list. Model fields, getters, setters, routing targets, serialization
  all kept as reserved slots. Pitch tickets at 0 cover per-degree exclusion;
  Range covers octave coverage; Spread + Bias cover soft windowing.

## Open questions

- [ ] Final UI label for repurposed Steps knob: "Steps" / "Tones" / "Sieve" / "Anchors".
- [ ] Whether to also rename `Note Duration` → "Duration" and `Variation` → "Duration Spread" in the UI now that they have parallel roles. Probably no — current names are recognizable, the conceptual rename is internal.
- [ ] Whether to add hero page numerics overlay showing the live Steps cutoff (e.g. "5/12 degrees" when knob = 40% on a 12-tone scale).
