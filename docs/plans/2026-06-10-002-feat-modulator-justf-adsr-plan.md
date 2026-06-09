---
id: feat-modulator-justf-adsr
schema: plan
title: "feat: JustF envelope spread for ADSR followers (unified rate/time spread)"
type: feat
status: completed
date: 2026-06-10
depth: standard
---

# feat: JustF envelope spread for ADSR followers

JustF (shipped: rate-link mode) spreads only the **Free phase rate** — LFO / Random
/ Chaos. An **ADSR** modulator ignores it: ADSR timing is gate + ms params
(attack/decay/release → ticks), never `freePhaseIncrement`, so the INTONE override is
inert for ADSR today.

This plan makes JustF meaningful for ADSR followers by recognizing that **an envelope
duration is a rate** (speed = 1/time). There is no separate envelope model: the
existing rate-spread helper is generalized with a **cap** argument and reused. Rate
spreads in Hz (cap 16 Hz, unchanged); envelope spreads in the same helper via an
ms↔Hz face (cap = the 12 ms floor expressed as a rate). One spread family, one
master-clamp, one ratio. While JustF is active a Free ADSR follower inherits M1's
envelope scaled by its index ratio — identical treatment to how a follower's rate
already derives from M1's rate. One gate fires a bank of related-speed envelopes:
harmonic INTONE makes them progressively snappier (M8 = 1/8 the time), subharmonic
stretches them longer. On exit the spread bakes into real params, same lifecycle as
the rate spread.

---

## Locked Decisions

- **Unified inherit model, rate and envelope.** A follower's rate derives from M1's
  rate; a follower's envelope derives from M1's envelope. JustF is a
  *relate-everything-to-M1* mode by definition — an LFO follower and an ADSR follower
  are treated identically. Independence = JustF off. This dissolves the
  "different shapes" concern: every shape spreads relative to M1, consistently.
- **Single spread helper, cap-parameterized.** Generalize the shipped
  `justfMasterHz` / `justfEffectiveHz` to take a `cap` argument (default 16 Hz). Rate
  calls it with the default → byte-identical behavior ("rate is rate", nothing
  changes). No second spread function with its own floor or its own inherit logic.
- **Envelope = the ms face of the same helper.** Convert each stage ms → speed
  (`1000/ms`), run it through `justfEffectiveHz` with cap = `1000/12 ≈ 83.3 Hz`,
  convert back. The master-clamp that keeps the fastest LFO ≤ 16 Hz is the exact same
  code that keeps the shortest envelope ≥ 12 ms — one clamp, two caps. Ratios are
  preserved across the clamp (the master lengthens, same as rate lowers M1's master).
- **Sustain is a level, not a time** — copied from M1 unscaled. Only attack, decay,
  release are durations and pass through the spread.
- **Amplitude is per-follower** — the one envelope param a follower keeps its own;
  not inherited from M1, not scaled. Inherit covers A/D/R + sustain level only.
- **INTONE host = ADSR page 2, slot 1.** Unlike the LFO case (INTONE on the page-1
  RATE cell), ADSR keeps attack in its real page-1 cell and hosts INTONE on page 2
  beside AMPLITUDE (page 2's only param). Page 1 stays a clean read-only view of the
  spread envelope; page 2 is the control surface. JustF still toggles on page 1 (the
  slot-1 / attack key, the rate-equivalent). Renders:
  `ui-preview/modulator-justf/justf-adsr-follower.png`, `justf-adsr-intone-page2.png`.
- **Zero stays zero.** A base stage of 0 ms (instant attack / no decay / no release)
  is preserved as 0 — the ms face returns 0 without touching the Hz helper, keeping
  the engine's existing "instant" special-cases.
- **Free-domain only**, same guard as the rate spread; Tempo modulators untouched.

---

## Scope Boundaries

In scope:
- Engine helper: generalize the rate helper with a cap; add its ms face.
- Engine ADSR branch: read inherited+spread durations when JustF is active.
- Bake: Free ADSR followers bake inherited+spread A/D/R/S on exit.
- UI: ADSR followers show their inherited+spread envelope under JustF (today the
  `!isADSR` guard excludes them); resolve where INTONE lives when the cursor is on ADSR.

### Deferred to Follow-Up Work
- INTONE knob taper (finer near noon) — carried from the JustF plan, not this build.

### Out of scope / known limitation
- **M1 not ADSR.** Inherit reads M1's A/D/R/S fields, which exist on every modulator
  regardless of shape (Chaos reuses attack/decay as P1/P2). If M1 is non-ADSR the
  inherited base is musically meaningless. Consistent with rate JustF using M1's rate
  field unconditionally. No guard; documented as a setup expectation (M1 should be
  ADSR for envelope spread to make sense).

---

## High-Level Technical Design

```
// Single spread helper, cap-parameterized. Default cap keeps rate identical.
justfMasterHz(m1Hz, intone, cap = 16):
  maxRatio = 1 + (N-1) * max(0, intone)
  return min(m1Hz, cap / maxRatio)          // clamp master so fastest follower <= cap

justfEffectiveHz(m1Hz, intone, index, cap = 16):
  hz = justfMasterHz(m1Hz, intone, cap) * intoneRatio(intone, index)
  return clamp(hz, 0.0001, cap)

// ms face of the SAME helper — pure unit conversion, no spread logic of its own.
justfEffectiveMs(m1Ms, intone, index):
  if m1Ms <= 0: return 0                     // instant stage preserved
  speed = justfEffectiveHz(1000/m1Ms, intone, index, 1000/12)   // 12 ms -> 83.3 Hz cap
  return round(1000 / speed)

// Engine.cpp loop, per follower under JustF:
  rateOverride = (Free) ? justfEffectiveHz(m1Hz, intone, index+1) : -1   // unchanged
  envBase      = (Free) ? &project.modulator(0) : nullptr                // M1 base

// ModulatorEngine::tick, ADSR branch entry (once, before the state machine):
  if envBase:
    attackMs  = justfEffectiveMs(envBase->attack(),  _intone, index+1)
    decayMs   = justfEffectiveMs(envBase->decay(),   _intone, index+1)
    releaseMs = justfEffectiveMs(envBase->release(), _intone, index+1)
    sustain   = envBase->sustain()           // level, unscaled
  else:
    attackMs = modulator.attack(); ...        // own params, unchanged
  // state machine reads these locals instead of modulator.attack()/decay()/...
```

*Directional guidance for review, not implementation specification.*

The master-clamp behavior is worth noting: at high INTONE the envelope master
lengthens (M1's effective attack grows) so the fastest follower's stage stays ≥ 12 ms
— the exact mirror of how high INTONE lowers M1's master Hz so the fastest LFO stays
≤ 16 Hz. Same code, ratios preserved.

---

## Implementation Units

### U1. Generalize the spread helper with a cap; add the ms face (TDD)

**Goal:** One cap-parameterized spread helper drives both domains. Rate keeps its
16 Hz behavior unchanged; envelope reuses it through an ms↔Hz face with a 12 ms-as-Hz
cap and zero-preservation.

**Requirements:** `cap` defaults to 16 (rate unchanged); `justfEffectiveMs` is a pure
conversion over `justfEffectiveHz` (no independent spread logic); 0 ms preserved;
shortest spread stage floored at 12 ms via the cap.

**Dependencies:** none (reuses `intoneRatio`).

**Files:**
- `src/apps/sequencer/engine/ModulatorEngine.h` — add a defaulted `float cap = 16.f`
  param to `justfMasterHz` and `justfEffectiveHz`; add
  `static int justfEffectiveMs(int m1Ms, float intone, int index)`.
- `src/tests/unit/sequencer/TestModulator.cpp` — extend the JustF CASE block.

**Approach:** Thread `cap` through `justfMasterHz` (replaces the hardcoded `16.f`) and
`justfEffectiveHz` (replaces the two `16.f` literals). `justfEffectiveMs`:
`if (m1Ms <= 0) return 0; float hz = justfEffectiveHz(1000.f / m1Ms, intone, index, 1000.f / 12.f); return int(1000.f / hz + 0.5f);`. Existing 3-arg call sites compile via the default.

**Execution note:** Test-first. Add failing cases before changing the helpers,
mirroring the existing CASE style (`std::fabs(...) < 1e-3f` for Hz; exact int for ms).

**Patterns to follow:** existing JustF statics + CASE blocks at `TestModulator.cpp:131-157`.

**Test scenarios:**
- Rate regression: `justfEffectiveHz(16.f, 1.f, 8)` (default cap) still `<= 16` — existing 3-arg cases stay green.
- Cap respected: `justfEffectiveHz(100.f, 1.f, 8, 80.f)` clamps at 80, not 16.
- Envelope M1 identity: `justfEffectiveMs(100, 1.f, 1) == 100` (index 1 unscaled).
- Harmonic shortens: `justfEffectiveMs(800, 1.f, 8)` → ~100 ms.
- Harmonic hits the 12 ms floor via the cap: `justfEffectiveMs(80, 1.f, 8)` → 12 (would be 10).
- Subharmonic lengthens: `justfEffectiveMs(100, -1.f, 8)` → ~800 ms.
- Zero preserved: `justfEffectiveMs(0, 1.f, 8) == 0` (no floor applied).
- Master-clamp preserves ratio: with a short M1 base + high INTONE, the M1 stage
  itself lengthens so M8 lands exactly at 12 ms (ratio M1:M8 intact).

**Verification:** TestModulator compiles; new cases pass; pre-existing JustF cases unchanged.

---

### U2. ADSR tick branch reads inherited+spread envelope

**Goal:** Under JustF, the ADSR state machine runs on M1's envelope spread by the
follower's index ratio instead of the follower's own params.

**Requirements:** Inherit M1 A/D/R/S; A/D/R via `justfEffectiveMs`; sustain unscaled;
Free-domain followers only; non-JustF behavior unchanged.

**Dependencies:** U1.

**Files:**
- `src/apps/sequencer/engine/ModulatorEngine.h` — `tick` gains
  `const Modulator *justfEnvBase = nullptr`; ADSR branch computes A/D/R/S locals at
  entry, reads locals throughout.
- `src/apps/sequencer/engine/Engine.cpp` — pass `envBase` in the modulator loop.

**Approach:** Add the optional trailing param after `rateHzOverride`. At the top of
the `if (shape == ADSR)` block compute four locals from `justfEnvBase` via
`justfEffectiveMs` when non-null, else from `modulator`. Replace the in-branch
`modulator.attack()/decay()/sustain()/release()` reads (`ModulatorEngine.h:143,161,162,180,183`)
with the locals. In `Engine.cpp`'s loop set
`const Modulator *envBase = (justf && modulator.rateDomain() == Free) ? &_project.modulator(0) : nullptr;`
and pass it. Other shape branches ignore the param.

**Patterns to follow:** the `rateHzOverride` threading — `Engine.cpp` resolves M1 from
`_project` and passes a per-follower override into `tick`; ADSR branch `ModulatorEngine.h:124-206`.

**Test scenarios:**
- Scaling math covered by U1.
- Sim audition (manual, see Verification): M1 ADSR + Free ADSR followers, one gate →
  followers fire progressively snappier at +INTONE, longer at −INTONE.
- Non-JustF regression: JustF off, an ADSR modulator plays its own envelope unchanged
  (locals fall through to `modulator.*`).

**Verification:** sim build runs; an ADSR bank under JustF audibly fans out envelope
speed by index; JustF off restores own-envelope playback. STM32 release green, under
the 983040 flash ceiling.

---

### U3. Bake inherited+spread envelope into Free ADSR followers

**Goal:** Leaving the page (or toggling JustF off) writes each Free ADSR follower's
inherited+spread A/D/R/S into its own params, so the bank persists.

**Requirements:** Free ADSR followers bake spread A/D/R + M1 sustain; Free non-ADSR
followers keep the rate bake; bake clears JustF state.

**Dependencies:** U1, U2.

**Files:**
- `src/apps/sequencer/ui/pages/ModulatorPage.cpp` — extend `bakeJustf` (`:38-55`).

**Approach:** In the Free-follower loop, branch on shape. ADSR follower →
`m.setAttack(justfEffectiveMs(m1.attack(), intone, i+1))`, same for decay/release,
`m.setSustain(m1.sustain())`. Non-ADSR → existing `setRate(justfEffectiveHz(...))`.
`m1` is `_project.modulator(0)`. Setters clamp. Keep the trailing
`setJustfActive(false)` / `setIntone(0.f)`.

**Patterns to follow:** existing `bakeJustf` body and its Free-domain guard.

**Test scenarios:**
- Bake an ADSR follower at harmonic INTONE → stored A/D/R equal the spread values,
  sustain equals M1's, JustF deactivates.
- Bake a mixed Free bank (some ADSR, some LFO) → ADSR get envelope, LFO get rate.
- Tempo-domain follower left untouched.
- (Validated via sim audition + reading values on the page; math is U1's coverage.)

**Verification:** after exiting with JustF active, ADSR followers' on-page A/D/R/S
read as the spread values and play that way with JustF now off.

---

### U4. UI — show inherited+spread envelope on ADSR followers; INTONE on page 2

**Goal:** Under JustF, an ADSR follower's page-1 A/D/S/R cells show the inherited+spread
values it's actually playing (read-only), instead of being excluded as today. INTONE
is hosted on page 2 slot 1 (beside AMPLITUDE); attack stays in its page-1 cell.

**Requirements:** Drop the `!isADSR` exclusion for the JustF display path; ADSR page 1
shows inherited+spread A/D/R/S read-only; INTONE renders/edits on page 2 slot 1;
AMPLITUDE stays the follower's own value (per-follower, not inherited); layout matches
the signed-off renders.

**Dependencies:** U1, U2.

**Files:**
- `src/apps/sequencer/ui/pages/ModulatorPage.cpp` — JustF display block (`:197-210`),
  the M2 tiny readout (`:269-278`), the ADSR value population in `draw`, and the
  INTONE edit/encoder path so it binds to page-2 slot 1 for ADSR.
- `ui-preview/pages_modulator.py`, `ui-preview/generate.py` — renders (already added:
  `modulator-justf-adsr-follower`, `modulator-justf-adsr-intone`).

**Approach:** For a Free ADSR follower under JustF, populate page-1 `values[1..4]`
(attack/decay/release via `justfEffectiveMs`, sustain from M1) drawn in the read-only
Medium color used for derived rate on M3–M8. AMPLITUDE (page 2 slot 0) keeps the
follower's own value. INTONE hosts on page 2 slot 1: relabel that footer slot to
"INTONE" and show the `%+.2f` value there when JustF is active; the encoder on page-2
slot 1 edits INTONE. The page-1 attack cell is no longer hijacked — no tiny readout
needed. JustF toggle stays on page 1's slot-1 (attack) key.

**Execution note:** UI proposal — already rendered and signed off
(`ui-preview/modulator-justf/justf-adsr-follower.png`, `justf-adsr-intone-page2.png`).
Re-render if the layout shifts during implementation.

**Patterns to follow:** existing JustF display blocks (`:197-210`, `:269-278`), the
M3–M8 derived-rate read-only rendering, and the chaos page-2 param handling;
`ui-preview/UI-VARIANTS.md` geometry.

**Test scenarios:** `Test expectation: none — display-only`. Validation is the rendered
PNG (no glyph collision, values inside safe area y=11..52) plus on-device read, not a unit test.

**Verification:** ADSR follower page 1 shows spread A/D/R/S read-only; page 2 shows
AMPLITUDE (own) + INTONE (editable) without collision; matches the signed-off renders.

---

## Deferred to Implementation

- (INTONE host and follower readout content were resolved during planning via
  ui-preview — see Locked Decisions and U4.)

---

## System-Wide Impact

- No new model fields, no serialization change, **no ProjectVersion bump** — JustF is
  transient and bake writes existing params.
- `justfMasterHz`/`justfEffectiveHz` gain a defaulted `cap` param; existing 3-arg call
  sites and tests are unaffected. `tick` gains one optional trailing param.
- Flash: helper generalization + ms face + ADSR branch wiring is small int/float math;
  must stay under the 983040 ceiling (last JustF build 950340). Confirm in U2's STM32 build.
- Resource: no new per-modulator state; a few int ops per ADSR follower per update.

---

## Verification Strategy

- U1: `TestModulator` green — rate cases unchanged, new cap + ms-face cases pass.
- U2/U3: sim build + manual modulator-page audition — ADSR bank fans out by index
  under JustF, bakes to real params on exit, plays unchanged with JustF off.
- U4: `ui-preview` render reviewed and signed off before the UI lands.
- Whole branch: STM32 release green and under the flash ceiling.
- Commit per unit when the user asks; do not auto-commit.
