# spring-modulator

## Goal
Add a new **modulator shape** — a mallet-struck multi-mode resonator ("SPRING") — that produces a
control-rate gesture nothing else in the module can: a strike that **rings down with evolving,
inharmonic wobble**, with a **Pickup** selecting which part of the moving body you read
(position / velocity / kinetic / potential / total energy). Deliberately not a physics sim; it's the
smallest model that is *distinct* from LFO / ADSR / Random / Chaos.

Full spec: `docs/spring-modulator-spec.md`. Live prototype: `.scratch/spring-modulator.html`.

## Key files
- `docs/spring-modulator-spec.md` — the spec (control laws §4, clamping §6).
- `.scratch/spring-modulator.html` — JS prototype, knobs + ±5V scope (matches the spec).
- `src/apps/sequencer/model/Modulator.h` — add `Shape::Spring`; reuse fields Chaos-style.
- `src/apps/sequencer/engine/ModulatorEngine.h` — new `if (shape==Spring)` branch beside Chaos/Random; per-mod state `_springX[N][3]`, `_springV[N][3]`.
- `src/apps/sequencer/ui/pages/ModulatorPage.cpp` — 2-page footer; STRIKE in the rate slot.
- Precedent: the Chaos branch in `ModulatorEngine.h` (state integrator as a Shape) and the just-landed Random→universal-`Mode` refactor (commit `87483a1e` chain).

## Decisions log
Append-only.
- 2026-06-11: **It's a modulator, not a track** — Performer has no audio path; the audio physical-modelling refs (Plaits/DaisySP, Tuesday Wobbler) are reference for the *feel*, not portable code.
- 2026-06-11: **Universal `Mode` carries gate behaviour** (no new mechanics). Run = Free strike clock, Trig = gate edge, Gate = hold-displaced then release. Same path Random now uses.
- 2026-06-11: **Deterministic strike (no RNG)** — fixed impulse vector. So cloning params to other slots + changing only PICKUP gives phase-locked correlated outputs ("fake multitap" with the existing 8 slots). No follower/shared-state machinery needed.
- 2026-06-11: **Dropped gravity** — on a linear spring it's a pure DC offset (= Offset knob), a no-op. It belongs to the double-pendulum (nonlinear) shape.
- 2026-06-11: **X/Y/Z taps were degenerate** (3 identical springs scaled) → reworked into **3 inharmonic modes** (1 / 2.76 / 5.40) so the partials beat. The genuinely-different taps are Position vs Velocity vs Energy.
- 2026-06-11: **Soft saturation `5·tanh(s)`** instead of hard clamp — it was flat-topping the rails.
- 2026-06-11: **RING is exponential and high = longer** (decay time, not damping). CLANG is the inharmonicity dial (0 = plain decaying sine).
- 2026-06-11: **Names** — SPRING / STRIKE / TENSION / RING / CLANG / PICKUP (owner picked SPRING + CLANG + TENSION). Footer p1 `SHAPE·STRIKE·TENSION·RING·CLANG`, p2 `PICKUP` (default Position).
- 2026-06-11: Less-is-more — owner pushed hard for **few controls**; the spring is intrinsically a 2-DOF object (frequency + decay), everything else is standard plumbing or fake.

## Open questions
- [ ] TENSION→f0 range (`F_MIN/F_MAX`) and the `dt`-dependent f0 stability ceiling (`w_max·dt < 1.5`).
- [ ] Exact field-reuse map in `Modulator.h` + the PICKUP storage bits.
- [ ] DEPTH scales strike (spec) vs output — confirm by ear.
- [ ] Tempo-domain meaning of STRIKE (strike-per-division vs free Hz).

## Completed steps
- [x] Design conversation: distinct-modulator framing, Mode mapping, multitap-via-clone.
- [x] JS prototype with control laws, modes, pickups, soft-sat, exponential decay.
- [x] Names locked.
- [x] Spec written with control laws + clamping (`docs/spring-modulator-spec.md`).
- [x] 2026-06-11: **Firmware wired** — model `Shape::Spring` + helpers; engine 3-mode integrator branch (`springTensionHz/Zeta/Clang`, `_springX/_springV`); 2-page UI footer + value print + encoder map; PICKUP reuses `phase`; field reuse attack=TENSION/decay=RING/smooth=CLANG, no version bump.
- [x] 2026-06-11: **TDD** — `TestModulator` Spring cases (rings-on-Trig-strike, holds-in-Gate) RED on stub → GREEN. Sim + STM32 release build clean.
- [x] 2026-06-11: ui-preview render `modulator-list/modulator-list-spring.png`.

## Notes
- Sibling/next task: **double-pendulum-modulator** — coupled, chaotic, θ1/θ2 = natural 2-tap. Ref `WobblerV2/Sources/Pendulum.c` (full Lagrangian, semi-implicit Euler, `dTheta *= Damping` near-1, Speed→g, Mod→damping). That's the "energy sloshing / never-repeats" side the linear spring deliberately omits.
- Tuesday V1 `Wobbler.c` = the *fake* (Sine×AD envelope) — a reminder a plain decaying sine isn't distinct.
