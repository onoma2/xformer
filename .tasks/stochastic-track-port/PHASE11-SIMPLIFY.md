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

## Phase 12 follow-ups (2026-05-21, this session)

### Density slot → Gate Length spread
- `_density` field repurposed via `gateLength()` / `setGateLength()` accessor pair.
- New `pickGateLength(durationTicks, spread, rng)` static helper in engine.
- Math: triangular distribution around hardcoded 50% center. spread=0 → exact 50%, spread=100 → 10..100% triangular.
- Floored at 10% of duration AND at `kMinAudibleGateTicks = 6` (≈16 ms @ 192 PPQN, 120 BPM).
- Replaces two hardcoded `(durationTicks * 50) / 100` sites (Live + Locked playback).
- Default 0 → matches prior hardcoded 50/50 behavior. Routing target `StochasticGeneratorDensity` kept for save compat.

### Drift restored — `_contour` re-enabled
- Reverted Phase 11 drop. Contour back in the pitch picker via Complexity kernel.
- `int drift = (contour × signedDist) / 20` added to per-degree kernel weight.
- Positive contour boosts ascending picks; negative, descending. Symmetric around lastIdx.
- UI: `Contour` re-added to flat items[] in the pitch block. Routing target `StochasticContour` preserved.

### Repeat probability — `_linearity` slot
- `_linearity` field aliased as `repeatProb()` / `setRepeatProb()`.
- Engine caches last-emitted event in `StochasticSourceEvent _lastEvent` + `bool _lastEventValid`.
- At triggerStep, Bernoulli `rng < repeatProb` bypasses generation and reuses cached event (note, octave, duration, articulation). Works in both Live and Loop modes — freezes / clusters at playback layer.
- `_lastEventValid` cleared on `reset()` / `restart()`.
- No routing target.

### Patience split — rhythm vs melody
- `_patience` (existing, routing target `StochasticPatience` preserved) → `patienceRhythm()`.
- `_accentProb` slot repurposed → `patienceMelody()`. Default 100 (off).
- Engine: split `_loopCycleCount` and `_loopCycleCountMelody`. Each rolls its own Poisson CDF, invalidates its own domain.
- Resets independently on its own `setXxxValid(false)` fire, on `renewRhythm()` / `renewMelody()`, on `refreshLoopSources()`, on engine reset/restart.
- No routing for `patienceMelody` (matches Repeat treatment).

### Accent gate output dropped silently
- No second physical gate jack on this device. `_accentOutput` member removed, `accent` field stripped from `Gate` struct and `LockedParentEvent`/`LockedChild`. `gateOutput(int)` returns `_gateOutput` for any index.
- Model field `_accentProb` kept (now serves as `patienceMelody`). UI list row dropped.
- `event.setAccent(...)` still written by generator — harmless dead write preserved for event-shape stability.

### Mask + Tilt redefined as performance pair (Loop-only filter + resonance)
- **Mask** = cutoff knob 0..100, default 100 (open). Already instant.
- **Tilt** = duration-based bipolar bias.
  - `weight = (tilt/100) × ((durSlot - 3.5) / 3.5) + noise`
  - tilt=+100 → long-duration events get top priority (rank 0), survive Mask cuts
  - tilt=-100 → short-duration events survive
  - tilt=0 → noise only, random rank
- Engine caches `_lastAppliedTilt` and `_lastAppliedSize`. At pattern-cycle boundary, if either changed, `generateMaskRanks` runs on existing event buffer (content untouched). Makes Tilt a real-time performance knob.
- `mutateRhythmOne` drops `oldRank` preservation; calls `generateMaskRanks` after writing the new event so duration-driven ranks stay current.
- Re-rank only in Loop mode. Live mode events still get rank=0 (Mask/Tilt do nothing in Live, matches intentional asymmetry — Live is the "raw stochastic stream").

### Loop page UI wiring
- Step buttons:
  - Top row (red LEDs): 0=Patience R, 1=Patience M, 2=Mutate, 3=Jump, 4=Sleep.
  - Bottom row left (green LEDs): 8=First, 9=Last, 10=Size, 11=Rotate.
  - Bottom row right (green LEDs): 14=Mask, 15=Tilt.
  - Held-step inverts colour.
- Held labels truncated: `PR %d`, `PM %d`, `M %+d`, `J %d`, `S %d`, `FRST %d`, `LAST %d`, `SIZE %d`, `ROT %+d`, `MASK %d`, `TILT %+d`.
- Macro readout row (no held step): `PR# PM# M# J# S#` tiny font at top.
- Tiny truncated labels `MK#` / `TL+#` under the boredom bars.
- **Boredom bars split** — left half = rhythm patience, right half = melody patience, 2px center gap, both grow rightward from their respective left edge.
- **Bar math** uses `patienceMeter(loops, patience)` = `min(1, loops / λ)` — linear "tension building" meter scaled to expected regen point (λ). Engine roll still uses `patienceProbability` (Poisson CDF) — independent. Bar visibly fills before regen fires now.
- **Double-click guard** on F3 NewR / F4 NewM. Single press shows `Press again - renew R` (or `M`); confirmed double-press fires the renew. Uses kernel `event.count() == 2` mechanism. F1/F2 (mode toggles) stay single-press.

### Phase 12 round 2 — Codex adversarial fixes (2026-05-22)

After Phase 12 landed, three Codex adversarial reviews flagged real bugs in
the mutation pipeline, the Mask/Tilt window scoping, the user-facing law
laws, and the musicality of the continuous knobs. All findings worth acting
on were applied.

**Mutation pipeline (Codex round 1):**
- `resetMeasure()` preemption guard: only rolls patience when the reset
  preempts a natural wrap that hasn't already fired (no double-counting).
- Mutation targets the active playback window `[first, last]` instead of the
  whole `size` buffer — was the most likely cause of "negative mutate doesn't
  seem to work" when First/Last narrowed the window.
- `rollPatience()` runs BEFORE the mutate roll on each loop boundary;
  mutate apply checks `rhythmValid()` / `melodyValid()` and skips a domain
  about to be regenerated by patience, so mutation writes never get wiped.

**Mask + Tilt window scoping (Codex round 2):**
- `generateMaskRanks` now ranks only events in `[first, last]`, assigning
  ranks `0..(windowSize - 1)`. Off-window events keep stale rank (never read).
- Mask filter at trigger compares against `windowSize`, not `size`.
- Re-rank fires on Tilt OR Size OR First OR Last change. Engine caches
  `_lastAppliedTilt`, `_lastAppliedSize`, `_lastAppliedFirst`, `_lastAppliedLast`.

**User-facing law fidelity (Codex round 3):**
- **Burst divisor mismatch**: `generateRhythmEvent`'s burst gate now uses the
  ACTUAL `sequence.divisor() × (CONFIG_PPQN/CONFIG_SEQUENCE_PPQN) × frac.num /
  frac.den` for parent tick math, instead of the legacy `getDurationMultiplier`
  helper (which assumed divisor=1/16). Burst no longer silently dead at slow
  divisors where the parent is plenty long enough.
- **First/Last window leak**: `triggerStep` snaps `_patternIndex` into
  `[first, last]` BEFORE reading `events[readIndex]`. Window edits take
  effect on the next event; no stale off-window event sounds.
- **Steps serialization**: deserialization clamp tightened from `1..100` to
  `0..100` to match the setter. Steps=0 round-trips correctly.
- **SHAPE toggle removal**: F1 SHAPE handler removed; `nextPage` no longer
  skips Marbles; Marbles page always draws the bell; CORE hero footer slot
  cleared; "SHAPE ON/OFF" label gone. Marbles params (Bias/Spread/Steps)
  always shape pitch — UI now matches that engine reality.

**Musicality round 1 (Codex round 4):**
- **Contour cap**: drift clamped to `kernelWidth` magnitude so large pools
  don't make Contour overpower Complexity. Same Contour value now feels
  consistent across 7-tone vs 12-tone × 4 octave pools.
- **Patience smoothing**: dropped the knob-100 off-sentinel discontinuity.
  Knob 0..100 → λ ∈ [1, 80] linear. `rollPatience` always rolls in Loop mode.
  Track Lock is the proper "freeze" gesture; patience is now a normal
  continuous knob.
- **Steps stepped display**: Marbles hero shows `STEP <%>  (K/N)` so user
  sees both the raw % AND the actual sieve count. Engine unchanged.
- **Burst dead-zone hint**: `printBurst` appends `*` when the current
  noteDuration + divisor combo can't produce audible burst (parentTicks < 96).

**Musicality round 2 — integer-truncation cliffs (this round):**
Found the same `KNOB / N` integer-divide cliff in five places. Fix shape is
uniform: multiply the triangle term by 10 so the kernel keeps a strong
center lead vs the leakage floor. Same fix applied to:
- **Variation** (`pickDuration`): old `kernel + spread/10` → new
  `(width-dist)×10 + spread/10`. At VAR=10: P(center) jumps from old 22%
  cliff to new 61% smooth ramp.
- **Complexity** (`generateDegree`): `tri × 10 + kernelLeak + drift`.
  Drift also scaled (`/20` → `/2`) to match. Drift cap scaled (×10) to match.
  Initial-pick fallback when `lastIdx < 0` raised from 10 to 100.
- **Marbles Spread** (`generateDegree`): bell triangle ×10 so the
  `marblesLeak` floor doesn't overwhelm the bell at low spread values.
- **Sleep** (engine): `(sleep × 4 + 5) / 10` (round, was floor). Knob 1..2
  used to silently produce 0 sleeps; now knob=2 → 1 sleep, knob=3 → 1.

**Untouched (correctly):**
- Bernoulli rolls (Rest, Slide, Legato, Burst probability) — already linear.
- Patience — already float math after round 1 fix.
- Burst LUT picks — use subtraction, no division cliff.
- Mutate kernel — pure multiplication.
- Steps sieve K — explicit-stepped, by design.

### Final Phase 12 reserved-slot table

| Slot | Engine reads? | UI shows? | Repurpose status |
|---|---|---|---|
| `_density` | yes | yes (as "Gate Length") | repurposed — gate length spread |
| `_contour` | yes | yes (as "Contour") | reused — Drift directional bias |
| `_linearity` | yes | yes (as "Repeat") | repurposed — repeat probability |
| `_marblesMode` | no | no | reserved — flag stays in save |
| `_level` | no | no | reserved — enum stays in save |
| `_minDegree` / `_maxDegree` | no | no | reserved — slots only |
| `_accentProb` | yes | yes (as "Patience M") | repurposed — melody patience |
| `_marblesSteps` | yes | yes (as "Steps") | repurposed — pitch sieve cutoff |
| `_variation` sign | no | no (display abs) | storage signed for forward compat, abs() at getter |

## Open questions

- [ ] Final UI label for repurposed Steps knob: "Steps" / "Tones" / "Sieve" / "Anchors".
- [ ] Whether to also rename `Note Duration` → "Duration" and `Variation` → "Duration Spread" in the UI now that they have parallel roles. Probably no — current names are recognizable, the conceptual rename is internal.
- [ ] Whether to add hero page numerics overlay showing the live Steps cutoff (e.g. "5/12 degrees" when knob = 40% on a 12-tone scale).
