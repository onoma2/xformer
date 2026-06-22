---
id: helical-aliveness
schema: plan
title: "feat: make the helical generator less static (continuous state + Marbles warp)"
type: feat
status: active
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

In scope: U1 continuous-state feedback (+ identity offset), U2 Marbles warp (+ bias/spread knobs).
Out of scope: the swarm / polyphony; the "steps" role-hierarchy quantization; gate-length depth knob.

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

## U2. Marbles distribution warp (organic variety)

**Goal:** reshape the fold remainder into a clustered/skewed distribution like the original.

**Dependencies:** U1.

**Files:**
- `src/apps/sequencer/engine/generators/HelicalAr.h`
- `src/apps/sequencer/engine/generators/HelicalGenerator.h` / `.cpp`
- `src/tests/unit/sequencer/TestHelicalAr.cpp`

**Approach:** Port the warp (`helical.instr:357-363`) before quantizing to note:
`u = influence/span`; `skew = bias<0.5 ? 1+(0.5-bias)·10 : 1/(1+(bias-0.5)·10)`;
`uSkew = pow(u, skew)`; `P = pow(2, (spread-0.5)·6)`;
`marblesU = pow(uSkew,P)/(pow(uSkew,P)+pow(1-uSkew,P)+1e-6)`; `warped = marblesU·span`.
`note = round(warped)`; feed back `pitch = warped`. `bias`/`spread` in [0,1], default 0.5 (=
identity warp, behaviour reduces to U1). Add `Bias` + `Spread` to `HelicalGenerator::Params`/`Param`
and editParam/printParam; footer shows the first five (Seed, Range, Base, Law, Bias), `Spread` rides
the context-menu aux slot (replacing the BASE quick-edit). Indexed-only, unchanged registration.

**Test scenarios (add):**
- `bias=spread=0.5` reproduces U1 output exactly (identity warp).
- `spread` toward 1 clusters noteIndex toward the span edges; `bias` shifts the histogram centre.
- determinism holds for all bias/spread.

**Verification:** `TestHelicalAr` green; sim builds; sweeping Bias/Spread visibly changes the
note distribution; default still behaves like U1.

---

## Sequencing

Land U1 first and listen — continuous state alone likely fixes "static." U2 is additive (defaults to
no-op) and adds organic shaping if U1 still feels too regular. Constants borrow the original verbatim
(`17.31`, `0.5`, `16.17` if the law needs the norm; `kIdentity` from `giVoiceOffsets`), tuned at
implementation time.
