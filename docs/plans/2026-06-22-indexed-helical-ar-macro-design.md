# Indexed Helical AR Macro — Design

**Status:** design, validated via brainstorm 2026-06-22. Awaiting /ce-plan or implementation.

**Goal:** Add a new Indexed-track macro that fills the sequence with a deterministic autoregressive map ported from the Nebulae 2 "helical" generator — keeping its signature **pitch↔duration coupling** (rhythm steers the next note; the note sets that step's time). Mono, deterministic, ~4 knobs.

**Scope:** one new populator method on `IndexedSequence` + its pure-logic helper + macro registration/UI. No model-storage change, no new TrackMode, no Generator-framework change.

---

## Origin & why Indexed

The cool part of `helical.instr` (`VhGenHelicalVoice`, the `kStep==1` block) is the coupling: each voice's next pitch is a fold-feedback of its *previous duration* (heavily weighted) + previous pitch, and the resulting note's frequency sets the next duration via a √freq "law." Note-step tracks (Tuesday/Note) can't carry continuous per-step duration, so the coupling dies there.

**Indexed** is the right host: its steps carry a first-class 10-bit `duration` (0–1023 ticks) *separate* from gate length, and the engine advances on `_effectiveStepDuration` — so the sequence breathes (variable step time). It also already has a macro-populator system (`populateWithMacroRhythm/…RandomMelody`, 8 today). The `Generator`/`SequenceBuilder` framework only targets Note/Curve, so this is a **macro, not a Generator Mode**.

Reference verdict: port the helical AR core; ignore performer-nx's `TuringAlgorithm` (a mislabeled cellular automaton). Drop the Nebulae polyphony (16 voices, swarm coupling, intone ratios) — Performer is mono per track.

---

## Home

New method, 9th in the macro set:

```
IndexedSequence::populateWithMacroHelical(firstStep, lastStep, octaveRange, base, lawDir, seed)
```

Writes each step's `noteIndex` / `duration` / `gateLength`. Reads `selectedScale()` for the √freq law and degrees-per-octave; the track's scale + root do the musical mapping at playback (the macro never owns a scale). Invoked from the same Indexed macro UI as the others; re-apply regenerates.

---

## Algorithm (per step)

State: two floats, `pitch` and `dur`. The **seed** sets the initial pair. **No RNG** — fully deterministic.

1. `raw = prevDur·W_DUR + prevPitch·W_PITCH` — fixed ~35:1 weights (duration dominates → rhythm steers melody; the helical signature). Not knobs.
2. **Fold:** `influence = raw mod span`, `span = octaveRange × scaleSize`. The nonlinear wrap is the engine of variety.
3. **Note:** `noteIndex = round(influence)` → `setNoteIndex`; track scale+root map it to pitch at playback.
4. **Duration:** resolve note → frequency (track scale), `duration = base × law(√freq)`, `lawDir` picks higher-pitch-longer vs -shorter; clamp to Indexed's tick range → `setDuration`.
5. **Gate length:** the fold remainder `influence − round(influence)` (a free, decorrelated [0,1) value the rounding discards) → remapped to **20–100% of the step's duration**. Per-step breathing gate, deterministic, reproducible per seed. Every step gates (no rests in core).
6. **Feedback:** `pitch = influence`, `dur = duration` → next step. The duration just computed steering the next note *is* the coupling.

---

## Controls (4)

- **octave range** — pitch span (fold modulus, in the track scale); also reshapes rhythm via the law.
- **base** — duration / tempo scaling.
- **law direction** — high pitch → longer | shorter.
- **seed** — initial state + reproducibility (scrubs the chaotic map's initial conditions).
- (loop length = the apply range, free; scale + root = the track's own settings.)

Same seed + knobs → identical sequence; preview/re-apply is stable.

---

## Scope boundaries (YAGNI — deferred, not rejected)

Intentionally held back from the helical; add only if the core feels plain:
- Marbles distribution warp (bias skew + spread-P clustering).
- "Steps" role-hierarchy (tension/release quantization to roots/octaves).
- Rests / gate on-off.
- Gate-length depth knob (always-on from the fold remainder for now).
- Multi-voice / swarm / intone ratios (Nebulae polyphony).
- Not a Generator Mode — an Indexed macro.

---

## Testing (TDD)

Extract the map as a pure helper — `helicalArStep(pitch, dur, span, base, lawDir) → {noteIndex, durTicks, gateLen, newPitch, newDur}` — and unit-test like `TuringRegister`:
- **determinism:** same seed+params → identical step stream.
- **coupling:** altering the duration-feedback path shifts the pitch trajectory.
- **octave range:** notes stay within `[root, root + octaveRange×scaleSize]`.
- **law direction:** higher pitch → monotonically longer (or shorter).
- **bounds:** `durTicks` in Indexed's tick range; gate length in 20–100%.
- **convergence guard:** a fixed-point-prone seed doesn't collapse to one repeated note.

The `populate…` method (writing into the sequence) is build + manual verified, like the other macros.

---

## Open / implementation-time

- **Macro-UI param surface:** existing macros take one simple param; this needs ~4. Verify whether the Indexed macro UI supports a per-macro param panel or needs a small entry built.
- **Convergence nudge:** detect/break fixed points (the helical avoided this via 16-voice offsets we dropped).
- **Calibration:** `base→ticks` mapping; the `W_DUR`/`W_PITCH`/`NORM` constants (borrow helical's 17.31 / 0.5 / 16.17 as starting points, tune).
- **Naming:** "Helical" (homage) vs "Drift" / "Coupled" / "AR".
