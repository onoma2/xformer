# Spring Modulator — spec

A new **modulator shape**: a mallet-struck multi-mode resonator. Hit it and it
rings down with internal wobble; a **Pickup** selects which part of the moving
body you read (position / velocity / energy). It is deliberately *not* a faithful
physics sim — it is the smallest model that produces a modulation shape nothing
else in the module (LFO / ADSR / Random / Chaos) can make.

Status: **spec / not implemented.** Prototype: `.scratch/spring-modulator.html`.

---

## 1. Why a new shape (not a track, not a tweak)

- A single decaying sine is just `LFO × AD-envelope` — not distinct (this is what
  Tuesday's old "Wobbler" faked: `Sine(phase) × envelope`).
- The distinct thing is a **struck object that rings down with evolving timbre** —
  inharmonic partials that beat against the fundamental and die at different rates.
- Precedent for "run a tiny dynamical system at control rate as a modulator
  Shape": the **Chaos** modulators (`ModulatorEngine.h` Lorenz / Latoocarfian).
  Spring slots in the same way — a new `Modulator::Shape::Spring`, driven by the
  universal gate `Mode`, returning before the LFO path.

This is a modulator, full stop. Performer has no audio path; the value is the
control-rate gesture, not synthesis.

---

## 2. The model

Three independent damped harmonic oscillators ("modes"/partials) at **fixed
inharmonic frequency ratios**:

```
R = { 1.0, 2.76, 5.40 }      // metal-ish; mode i frequency = base × R[i]
```

Each mode holds `(x_i, v_i)` = position, velocity. Integrated with **semi-implicit
Euler** at the modulator control rate. A **strike** is a deterministic velocity
impulse to all modes (no RNG — see §7). Upper modes are voiced by **Clang** and
damped faster, so the timbre evolves as the body settles.

The modes are **uncoupled** — they do not trade energy. (Energy sloshing between
partials, the "never quite repeats" character, is the double-pendulum's job, a
separate shape. Gravity is also omitted: on a linear spring a constant force is a
pure DC offset, identical to `Offset`, so it adds nothing.)

---

## 3. Controls

Footer, page 1: `SHAPE · STRIKE · TENSION · RING · CLANG`. Page 2: `PICKUP`.
(Two-page shape, like ADSR/Chaos; `Pg 1/2` shows in the header.)

| Control | Slot | Range | Meaning |
|---|---|---|---|
| **SHAPE** | F1 | enum | = Spring |
| **STRIKE** | F2 (rate) | Free Hz / Tempo div | Free-mode mallet clock; re-press toggles Free↔Tempo domain (same as every shape). Idle in Trig/Gate. |
| **TENSION** | F3 | exp pitch | Fundamental ring pitch (spring tension). |
| **RING** | F4 | 0..max | Ring length. **High = longer.** Exponential. |
| **CLANG** | F5 | 0..max | Inharmonicity: 0 = single decaying sine (fundamental only); up = partials beat. |
| **PICKUP** | page 2 | enum (default **Position**) | Output: Position / Velocity / Kinetic / Potential / Total energy. |
| DEPTH | routing/std | 0..127 | Strike force (harder hit = bigger ring). |
| OFFSET | routing/std | bias | Output bias. |
| MODE | routing overlay | Run/Trig/Gate | Gate behaviour (§5), the universal `Mode`. |

---

## 4. Control laws

Let `dt` = the modulator tick period in seconds (the `dt` passed to
`ModulatorEngine::tick`, `Engine.cpp:243`). Normalise each knob to its working
value first; the laws below use those.

**Frequencies** (exponential pitch — musical):
```
f0   = F_MIN · (F_MAX / F_MIN) ^ (tension01)          // tension01 = TENSION / max
w0   = 2π · f0
w_i  = w0 · R[i]                                        // i = 0,1,2
```

**Damping** (exponential, inverted so RING-high = long):
```
ζ    = ζ_LO · (ζ_HI / ζ_LO) ^ (1 − ring01)             // ζ_LO≈0.0015, ζ_HI≈1.2
c_i  = 2·ζ·w_i · (1 + 0.8·i)                            // upper modes die faster
```

**Integration** (semi-implicit Euler, per tick, per mode):
```
a_i  = −(w_i²)·x_i − c_i·v_i
v_i += a_i·dt
x_i += v_i·dt
```

**Strike** (deterministic impulse — Free clock wrap, Trig edge, or Gate release):
```
V0   = w0 · K_STRIKE · (DEPTH/127)                     // K_STRIKE ≈ 0.7
v_i += V0      for all i                                 // identical kick, no RNG
```

**Clang voicing** (amplitude weight per mode, clang01 = CLANG/max):
```
A[0] = 1 ;  A[1] = clang01 ;  A[2] = 0.6·clang01
```

**Pickup → scalar `s`:**
```
Position :  s = Σ A[i]·x_i                              // bipolar
Velocity :  s = (Σ A[i]·v_i) / w0                       // bipolar (quadrature of position)
Kinetic  :  s = 4 · Σ A[i]² · (½·v_i² / w0²)            // unipolar (≥0)
Potential:  s = 4 · Σ A[i]² · (½·R[i]²·x_i²)            // unipolar (≥0)
Total    :  s = Kinetic + Potential                     // unipolar — the pure decay envelope
```

**Output** (modulator value 0..127, then `bipolarOutput`):
```
value = clamp( 64 + 63·tanh(s) + OFFSET , 0, 127 )      // soft-sat fills ±5V, no flat-top
_currentValue = bipolarOutput(value, invert)
```
Bipolar pickups swing ±5 V around centre (64); unipolar (energy) pickups rest at
0 V (64) and rise to +5 V (`s ≥ 0`).

---

## 5. Mode (Run / Trig / Gate)

Spring uses the universal gate `Mode` (it returns before the LFO mode logic, like
Chaos/Random):

| Mode | Behaviour |
|---|---|
| **Run** (Free) | Internal pulse on the **Strike** clock strikes the spring; it rings and is re-struck each period. Gate ignored. |
| **Trig** | A gate rising edge strikes it (one ring per edge). |
| **Gate** | While the gate is high the mass is **pinned displaced** (output flat at the pulled-back position); on the falling edge it is released and rings back. |

Gate-held pin:
```
x_i = (K_STRIKE / R[i]) · A[i] ;  v_i = 0        // while gate high
// on release: integrate from there (v=0) → rings down
```

---

## 6. Clamping & numerical safety

The whole reason this gets a dedicated section — an unguarded resonant integrator
will flat-top or blow up.

1. **Output: soft saturation, never hard clip.** `value = 64 + 63·tanh(s) + offset`,
   then clamp `0..127`. `tanh` rounds into the rails so partial sums and Free
   re-strike buildup compress gracefully instead of flat-topping. (Hard `min/max`
   clipping was visibly wrong in the prototype.)
2. **State runaway guard.** Clamp `|x_i| ≤ X_MAX` and `|v_i| ≤ V_MAX` every tick
   (e.g. `X_MAX = 4`, `V_MAX = 4·w0`). Bounds fixed-point/float drift and caps a
   pathological resonance.
3. **Integrator stability.** Semi-implicit Euler is stable while `w_i·dt < 2`. The
   top partial is `w_max = 2π·f0·R[2] (=5.40)`. **Clamp `f0` so `w_max·dt < 1.5`**:
   `f0 ≤ 1.5 / (2π · 5.40 · dt)`. The TENSION→f0 map must respect this ceiling at
   the actual `dt`; clamp on the encoding side.
4. **Energy pickups unipolar.** Enforce `s ≥ 0`; map to `64..127` (rest 0 V → +5 V).
5. **Strike clock.** Free domain = wall-clock Hz, Tempo = PPQN division — reuse the
   existing `freePhaseIncrement` / `rate` domain machinery (`ModulatorEngine.h`),
   the same path Chaos/Random use.
6. **DEPTH** scales the strike impulse (not the output) so a soft hit genuinely
   rings smaller; **OFFSET** biases the final value pre-clamp.
7. **Reset/init.** `x_i = v_i = 0`, strike-clock accumulator = 0.

---

## 7. Determinism & multitap

The strike is a **fixed impulse vector — no RNG.** Therefore two modulators with
identical params, the same gate source, and the same reset produce **bit-identical
state** (the engine ticks them with the same `dt`). So the "fake multitap" works:
clone the knobs onto another slot, change only **PICKUP** (Position on M1,
Velocity on M2, Total energy on M3…) and you get **phase-locked, correlated CV
streams from one virtual body** — without any shared-state / follower machinery.
(Contrast JustF/Geode followers; Spring needs none.)

Determinism is also why no-RNG is mandatory per the spec rules (PROJECT spec
guidance §16).

---

## 8. Integration points

**Model** (`model/Modulator.h`):
- Add `Shape::Spring` to the enum + `shapeName`.
- Backing fields — reuse existing fields Chaos-style (no new storage, **no
  ProjectVersion bump**): `rate` = STRIKE (+ `rateDomain`); `attack` = TENSION;
  `decay` = RING; `smooth` = CLANG; PICKUP = a small enum that reuses spare bits of
  an existing field (e.g. low bits of `phase`). `depth`/`offset`/`invert` stay
  universal. Final field map is the implementer's call; the constraint is reuse.

**Engine** (`engine/ModulatorEngine.h`):
- New `if (shape == Spring)` branch near the Chaos/Random branches, returning after
  writing `_currentValue`. Reads `mode()` for Run/Trig/Gate.
- Per-modulator state arrays: `float _springX[N][3], _springV[N][3]` (24 B each),
  plus the existing `_phaseAccumulator` (strike clock) and `_lastGate`. ≈ 24 B ×
  CONFIG_MODULATOR_COUNT extra RAM — note against the budget.

**UI** (`ui/pages/ModulatorPage.cpp`):
- Spring is a 2-page shape (`isSpring → total_pages = 2`).
- Page-1 footer `SHAPE·STRIKE·TENSION·RING·CLANG`; STRIKE in the rate slot
  (re-press = domain toggle, so `rateOnF2` includes Spring); page-2 footer
  `PICKUP` (default Position).
- Values render through the existing PhaseFlux-style param list.

---

## 9. What is intentionally NOT modelled

- **No inter-mode coupling.** Modes are independent decaying sines summed; they do
  not exchange energy. The chaotic "evolving wander" is the **double-pendulum**
  shape (next), not this one.
- **No gravity / pendulum nonlinearity.** On a linear spring it is a DC offset =
  `Offset`. It belongs to the pendulum.
- **Fixed 3 inharmonic partials.** No selectable material; Clang only voices/decays
  the upper two. A material select could come later as one extra enum, not knobs.
- **No multi-output hardware gate.** A pickup's CV can act as a thresholded gate
  *source* internally (via `gateFromLevel`, e.g. to clock another modulator), but
  modulators can't drive a physical gate jack (those are track-only).

---

## 10. Open items

- TENSION→f0 range (`F_MIN/F_MAX`) and the `dt`-dependent `f0` ceiling (§6.3) need
  pinning to the real modulator tick rate.
- Exact field reuse map (§8) + the PICKUP storage bits.
- Whether DEPTH scales strike or output (spec says strike; confirm by ear).
- Tempo-domain meaning for STRIKE (strike-per-division) vs free Hz.
