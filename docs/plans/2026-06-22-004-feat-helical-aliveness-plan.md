---
id: helical-aliveness
schema: plan
title: "feat: make the helical generator less static (continuous state + Marbles warp)"
type: feat
status: completed
date: 2026-06-22
depth: standard
---

# Helical aliveness

The shipped `HelicalGenerator` (`Mode::Helical`) collapses into a short periodic orbit after a few
steps — far more static than Nebulae 2's `helical.instr`. This restores the two mechanisms that keep
the original alive and which our mono port quantized away or dropped.

## Diagnosis

Our `HelicalAr::step` (`src/apps/sequencer/engine/generators/HelicalAr.h`) does:
`influence = fmod(prevDur·17.31 + prevPitch·0.5, span)` → `note = round(influence)` →
`duration = durationForNote(note)` (one of ~`span` discrete values, clamped to int ticks) →
feed back `prevDur = duration`. Because duration is a function of the **rounded note** and is the
dominant feedback term, the system reduces to a finite note→note map → period‑2..4 cycle. The
convergence guard only catches period‑1 (a stuck single note), so 2–3 note loops survive.

`helical.instr` (`VhGenHelicalVoice`, `kStep==1`) avoids this:
- **Continuous unclamped duration feedback** — `kDurState = max(kBase·kLaw, 0.001)`; only the audio
  output `kDurAudio` is clamped (`helical.instr:393-398`). State stays real-valued → the fold is a
  genuine stretch-and-fold chaotic map, not a finite table.
- **`kIdentity` fold offset** — an irrational per-voice constant added before the fold
  (`helical.instr:356`, `giVoiceOffsets[]` line 21) keeps the map off trivial fixed points.
- **Marbles distribution warp** — `kSkew` (bias) + `kMarblesU` clustering S-curve
  (`helical.instr:357-363`) reshape the uniform fold remainder into an organic, non-uniform spread.
- **16-voice swarm** — each voice folds its neighbour's state (`helical.instr:349-351`), a coupled
  lattice. **Unrecoverable in a mono port**; A+B below are the faithful mono approximation, not a
  reproduction of the swarm.

## Scope

In scope: U1 continuous-state feedback (+ identity offset); U2 Helicity (law-depth knob) + floorless
centered law. Out of scope: the Marbles distribution warp (dropped — continuous state + helicity gave
enough variety); the swarm / polyphony; the "steps" role-hierarchy; gate-length depth knob.

---

## U1. Continuous-state feedback (primary fix)

**Goal:** state stays real-valued so the fold is chaotic, not a finite cycle.

**Execution note:** test-first — add a "non-periodic over N steps" test before the change.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalAr.h`
- `src/tests/unit/sequencer/TestHelicalAr.cpp`

**Approach:** In `step()`:
- Compute the √freq law from the **continuous** folded pitch, not the rounded note:
  `octaves = influence / scaleSize`, `sqrtFreq = pow(2, octaves·0.5)`,
  `rawDur = base · (lawDir≥0 ? sqrtFreq : 1/sqrtFreq)` — a continuous float.
- **Output** `durationTicks = clamp(lround(rawDur), minTicks, maxTicks)` (unchanged contract).
- **Feed back the continuous `rawDur`** (`dur = rawDur`), not the clamped int.
- Add a fixed irrational **identity offset** into the fold input
  (`raw = dur·W_DUR + pitch·W_PITCH + kIdentity`, e.g. `kIdentity = 1.123f`) so there is no trivial
  fixed point; relax the repeat-nudge guard to a light backstop.
- `note = round(influence)` (Indexed step note) and the gate-from-remainder are unchanged.
- Keep `durationForNote(int note, …)` as the static law-shape helper for the monotonicity test;
  `step()` uses the continuous path internally (same formula).

**Test scenarios (add):**
- **non-periodic:** over 64 steps the noteIndex stream has no period ≤ 8 (no short cycle).
- determinism / octave bound / law monotonicity / gate+duration bounds — still green.
- coupling / no-collapse — still green.

**Verification:** `TestHelicalAr` green incl. the new non-periodic case; sim builds; the generated
Indexed sequence audibly evolves rather than looping every few steps.

---

## U2. Helicity (law depth) + floorless centered law

**Goal:** widen the rhythmic range. The √freq exponent was hardcoded at 0.5, and the long law pinned
duration to `>= base` — so durations only spanned ~2:1 (192–384). Expose the exponent ("helicity")
and centre the law on `base` so duration swings below and above it.

**Dependencies:** U1.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalAr.h`
- `src/apps/sequencer/engine/generators/HelicalGenerator.h` / `.cpp`
- `src/tests/unit/sequencer/TestHelicalAr.cpp`

**Approach:** Law becomes `base · 2^(octavesCentered · depth)` where
`octavesCentered = (influence − span/2) / scaleSize` (centred → no base floor) and `depth` is the
helicity (0.5 = sqrt-freq, 1 = freq-proportional, up to 2 = exaggerated). Add `Helicity` to
`HelicalGenerator::Params`/`Param` (stored ×10, range 1–20, default 8 = 0.8), footer label **HELI**
(fifth slot — all five fit; the footer already renders longer labels like `NEW RAND`). Lower the
generator's clamp floor to ~12 ticks so short notes are reachable. Indexed-only, unchanged registration.

**Test scenarios (add):**
- higher helicity → wider durationTicks range (ratio at depth 1.5 > 1.5× the ratio at depth 0.3).
- existing determinism / bounds / liveliness still green at the new defaults.

**Verification:** `TestHelicalAr` green; sim builds; the HELI knob audibly opens the rhythmic range
(~2:1 at 0.8 up to ~13:1 at 2.0), with durations both shorter and longer than `base`.

---

## Sequencing

Both units shipped. U1 (continuous state) killed the short cycles; U2 (helicity + floorless law)
opened the rhythmic range. Marbles was dropped — the two together gave enough variety without it.
Constants borrow the original verbatim (`17.31`, `0.5`; `kIdentity` from `giVoiceOffsets`).
