# Fractal Track — Current Design Summary

**Status:** Pre-implementation. Blocked on stochastic completion + RAM budget.
**Canonical:** this file + `docs/superpowers/specs/2026-05-17-fractal-track-design.md` are the current truth (the two are kept in sync). The design doc carries the KD-by-KD detail; this 1-pager is the at-a-glance view.

---

## What It Is

FractalTrack is a child melodic mirror track. A parent track (Note, Curve, Stochastic, Tuesday, Teletype — any lower-index engine) supplies gate+CV. Fractal samples that output at its own section rate into a small inline trunk buffer, then plays it back through configurable loop windows, capture rules, and (post-hardware, in scope) branch + ornamentation transforms. Mutation/evolution is deferred.

It is not a stochastic generator, not a note sequencer, and not a high-fidelity continuous automation trace. One cell per Fractal section — section-sampled. Intra-section motion is discarded **except the gate onset**, which Feel mode (KD-14b) keeps (one onset per cell).

## Substrate

**Mirror with section grid.** One inline `uint16_t` trunk per engine instance. Default 64 cells (128 B), max 128 cells (256 B). No heap. No per-pattern buffer. Pattern switch changes config only; trunk survives.

**Cell encoding (16 bits):**
- bits 0–10: CV — signed 11-bit (`c ∈ [-1024,1023]`), semitones rel root: `semis = c·60/1024` (±5 oct, LSB ≈ 5.86 cents); `c=0` = root. See KD-2 for encode/clamp.
- bits 11–14: gate length (0=rest, 1=trigger, 2–14=proportional, 15=full/tie)
- bit 15: valid (uncaptured cells output project root note with gate off)
- (Feel mode, KD-14b: a parallel `uint8 _onset[]` array holds the per-cell gate-onset phase — not packed in the cell.)

## Ownership

| Owner | What it owns |
|-------|-------------|
| **FractalSequence** (×17 patterns) | Timing (divisor, clockMultiplier, resetMeasure, runMode), loop lenses (loopFirst, loopLast, rotate, orderMode, loopMode-Loop), record extent (recordFirst, recordLast), recordMode (Replace/Latch). *Deferred-reserved: punchMode, recordQuantize, clockSource, loopBars, beatOffset, loopPhase.* |
| **FractalTrack** (×1, track-wide) | 17 sequences, sourceA, sourceB, gateLogic, cvLogic, bufferLength, lock, recordTrigger (Routable), octave, transpose, slideTime, cvUpdateMode, trackDelay, **scale group (scale+scaleRotate, inherit) — ornament-only, trunk stays raw**, branchCount, path, branchSeed, branchPool, ornamentRate, ornamentIntensity, ornFirst, ornLast, captureCadence, captureFidelity (**branchCount · path · ornamentRate · ornamentIntensity all Routable** — the four live performance controls). *Deferred-reserved: routedScan/clockSource, density, tilt, mutation params.* |
| **FractalTrackEngine** (×1, volatile) | Trunk buffer + Feel onset array, parent resolution, observe-over-section gate/CV measurement, capture/read/loop-boundary rule execution, branch state, track-delay queue, RNG (branch-seed). *Deferred: evolution history.* |

Trunk is **not serialized.** Save/load preserves model config; trunk starts empty on power cycle.

## Core Pipeline (MVP)

```
tick → divisor math → shouldAdvanceSection?
  → sectionBoundary:
      1. executeCaptureRule  (if !lock and rule allows: read source → write trunk)
      2. play                (mapLogicalToRead → read trunk → schedule output)
  → drainQueues
```

Default capture rule: **Replace** — continuously overwrite trunk cells from source while unlocked.
Lock write-protects the trunk. Branch transforms and ornamentation continue while locked.

## Window Hierarchy

```
recordFirst ≤ loopFirst ≤ ornFirst ≤ ornLast ≤ loopLast ≤ recordLast ≤ bufferLength-1
```

- **Record extent** — where capture writes land
- **Loop window** — what playback reads (per-pattern lens over the same trunk)
- **Ornament zone** — where ornaments are eligible to fire (mutation/evolution deferred; this was the mutation zone)

## RAM

Tracks are a **discriminated union** (`Container` sized to the largest member), so adding FractalTrack/Engine costs **zero net RAM** as long as each stays under the union max — the ×8 slots already exist and are already counted in the current SRAM/CCMRAM figures.

| Item | Size | Union max (gate) | Net cost |
|------|------|------------------|----------|
| FractalTrack (model, SRAM) | ~600 B | 9544 B | 0 B (under max) |
| FractalTrackEngine (CCMRAM) | ~730 B incl. 256 B trunk + 128 B Feel onset array | 912 B | 0 B (under max) |

Engine gate is **912 B** (TeletypeTrackEngine, PROJECT.md:299; the TT2 `static_assert` allows ≤944 — 912 is the conservative gate). Both fit. The model is cheap despite ×17 sequences because FractalSequence carries **no per-step array** (one cell per section); the engine + trunk + Feel onset array live in CCMRAM, so nothing touches SRAM additively. The ~182 B engine headroom under 912 is why **mutation (~256 B history) stays deferred** — it can't co-exist with Feel.

## Stage Gates (before Phase 1 code)

| Gate | Must pass |
|------|-----------|
| **A — Contract** | 1-pager + design doc agree. Every field has owner + active/reserved/deferred status. |
| **B — RAM** | STM32 sizeof probes under container gates. |
| **C — Serialization** | Round-trip test for all model fields. Invalid enums sanitize. |
| **D — Engine** | No STL, no heap in tick path. Source reads guarded. |
| **E — Hardware** | Mirror, lock, loop window, pattern lens audible on STM32. |
| **F — UI** | Active fields only. Reserved hidden. |

## Phase Order

0. Contract cleanup + stage gates
1. Model + serialization + round-trip test
2. Engine (minimal behavior: Replace capture, forward playback, lock)
3. List UI (active fields only)
4. Hardware verification → **stop, hand off to user**

Post-MVP, in scope (the playable identity — each lands in reserved fields/hooks):
branches (concatenated, chained, generative, bit-word Path) → ornaments (rate/intensity, scale-aware) + ornament zone → two-source mixing → track-delay → **two-axis capture (Section/Event cadence × Quantized/Feel fidelity — KD-14b)** → visual page.
Live performance controls (Routable): branch count · path · ornament rate · ornament intensity.

Deferred (later phase, nice-to-have):
mutation/evolution (entire subsystem — KD-3/6/10) · CV-scan · capture variants (Blend/Once/PunchIn) · bar-quantized loop · beat-offset · snapshot · sleep · density/tilt.

## Key Design Constraints

- **No RNG content generation.** All content comes from parent capture or defaults to project root note.
- **No heap in engine.** Inline buffer, pre-allocated BSS only.
- **No cache between model and engine.** Parent reads are live; trunk writes are immediate.
- **No content reseed.** No RNG *generates* trunk content — it all comes from parent capture (the deferred mutation evolves from the captured baseline, never dice-rolls new notes). This does **not** forbid the branch **transform-assignment seed** (KD-12), which is structural (it picks *which deterministic transform* each branch applies, from captured material) and regenerates on trunk edit — no content is invented.
- **Section phase is always active** (4-section lens, neutral defaults = identity transform).
