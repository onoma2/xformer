# Fractal Track — Current Design Summary

**Status:** Pre-implementation. Blocked on stochastic completion + RAM budget.
**Last updated:** 2026-05-22
**Canonical details:** DICTIONARY.md (vocabulary/ownership), TASK.md (phases/gates)

---

## What It Is

FractalTrack is a child melodic mirror track. A parent track (Note, Curve, Stochastic, Tuesday, Teletype — any lower-index engine) supplies gate+CV. Fractal samples that output at its own section rate into a small inline trunk buffer, then plays it back through configurable loop windows, capture rules, and (post-MVP) mutation/branch/ornamentation transforms.

It is not a stochastic generator, not a note sequencer, not a CV recorder, and not a high-fidelity automation trace. One cell per Fractal section. Intra-section motion is intentionally discarded.

## Substrate

**Mirror with section grid.** One inline `uint16_t` trunk per engine instance. Default 64 cells (128 B), max 128 cells (256 B). No heap. No per-pattern buffer. Pattern switch changes config only; trunk survives.

**Cell encoding (16 bits):**
- bits 0–10: CV (11-bit fixed-point, ~5.9 cents/LSB)
- bits 11–14: gate length (0=rest, 1=trigger, 2–14=proportional, 15=full/tie)
- bit 15: valid (uncaptured cells output project root note with gate off)

## Ownership

| Owner | What it owns |
|-------|-------------|
| **FractalSequence** (×17 patterns) | Timing (divisor, playMode, resetMeasure), loop lenses (loopFirst, loopLast, rotate, loopMode), record extent (recordFirst, recordLast), capture rules (recordMode, punchMode, recordQuantize), reserved future fields |
| **FractalTrack** (×1, track-wide) | 17 sequences, sourceA, bufferLength, lock, recordTrigger (Routable), octave, transpose, slideTime, cvUpdateMode, **scale group (scale+scaleRotate, inherit) — ornament-only, trunk stays raw**, branchCount, path, ornamentRate, ornamentIntensity, ornFirst, ornLast (**branchCount · path · ornamentRate · ornamentIntensity all Routable** — the four live performance controls), reserved future fields |
| **FractalTrackEngine** (×1, volatile) | Trunk buffer, parent resolution, gate-length measurement, capture/read/loop-boundary rule execution, RNG, evolution history, branch state |

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
| FractalTrackEngine (CCMRAM) | ~600 B incl. 256 B inline trunk | 944 B | 0 B (under max) |

Both fit with large margin. The model is cheap despite ×17 sequences because FractalSequence carries **no per-step array** (one cell per section); the engine + inline trunk live in CCMRAM, so nothing touches SRAM additively.

## Stage Gates (before Phase 1 code)

| Gate | Must pass |
|------|-----------|
| **A — Contract** | TASK + DICTIONARY agree. Every field has owner + active/reserved/deferred status. |
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
branches (concatenated, chained, generative, bit-word Path) → ornaments (rate/intensity, scale-aware) + ornament zone → two-source mixing → track-delay → visual page.
Live performance controls (Routable): branch count · path · ornament rate · ornament intensity.

Deferred (later phase, nice-to-have):
mutation/evolution (entire subsystem — KD-3/6/10) · CV-scan · capture variants (Blend/Once/PunchIn) · bar-quantized loop · beat-offset · snapshot · sleep · density/tilt · two-axis capture model (Event cadence + Feel/microtiming — KD-14b, the recorder direction).

## Key Design Constraints

- **No RNG content generation.** All content comes from parent capture or defaults to project root note.
- **No heap in engine.** Inline buffer, pre-allocated BSS only.
- **No cache between model and engine.** Parent reads are live; trunk writes are immediate.
- **No reseed.** Mutations evolve from captured baseline.
- **Section phase is always active** (4-section lens, neutral defaults = identity transform).
