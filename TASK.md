# TASK — wallclock-time-architecture (rewrite, 2026-06-09)

Unify xformer time handling. Two independent halves:
- **A** — phase-source unification: PhaseFlux + Stochastic re-derive phase from the
  discrete integer tick while every other engine reads the continuous
  `Clock::tickPosition()`. Move them onto `tickPosition()`.
- **B** — wall-time: real (tempo-independent) time used by the modulator Free(Hz)
  mode, MIDI CC rate-limiting, and MIDI-CV voice scheduling.

> **Status correction:** the old STATUS block predates `engine/WallClock.h` landing.
> The B substrate is **already built and adopted**. What remains is one more site,
> the slave-clock guard, and all of A. Scope is much smaller than the old block said.

---

## Decision / proposition (the way)

**32-bit microseconds + wrap-safe deltas. No 64-bit. Two separate clocks.**

- Keep a 32-bit µs free-running `WallClock` (already exists). All consumers measure
  **deltas**, never absolute timestamps, so wrap-safe subtraction (`(int32_t)(now-end)>=0`)
  is exact for any interval under the ~71.6 min wrap. The intervals here are ms.
- Do **not** build a 64-bit µs service or a software counter-extension. The 64-bit
  framing was inherited from ER-101, whose model is **absolute scheduling** — which
  xformer does not do (the transport already centralizes cross-track sync).
- Keep **wall time and musical tick as two clocks.** Catalyst can run everything off
  one sample counter because it is sample-locked; xformer cannot, because the Free(Hz)
  modulator mode is deliberately tempo-*independent*. Two clocks is correct here.

### Precedent: 4MS Catalyst (`../DSP/4ms-catalyst/src/clock.hh`)
A shipping product runs its entire clock/tempo/divider/tap system on `uint32_t` with
zero 64-bit:
- Time base is a free-running `uint32_t` **sample counter** (`Controls::TimeNow()`);
  BPM stored as `bpm_in_ticks` (samples per beat).
- `Clock::Timer`: `set = TimeNow(); Check(): TimeNow() - set >= duration` — wrap-safe
  unsigned delta. (xformer's `WallTimer` already mirrors this.)
- Tap tempo = `time_now - prev_tap_time` (a delta), clamped to the bpm range.
- Phase is counter-derived float: `GetPhase() = cnt / bpm_in_ticks` — exactly the
  pattern Part A wants for PhaseFlux/Stochastic.

### Reference: ER-101 (`OTHERS/ortagonal/er-101/`, `wallclock.h`, `res-er-clock.md`)
Source of the drift-free `end_time += delay` restart idiom (already in `WallTimer`)
and the slave-clock **0.5×–2× period outlier latch**. NOT importing its
synthetic-clock / tick-adjustment layer.

---

## Current state (what already exists)

`engine/WallClock.h` — DONE and Catalyst-shaped:
- `WallClock::now()` → 32-bit µs from `HighResolutionTimer::us()`, play/stop & tempo
  independent.
- `WallTimer` — absolute-deadline, **wrap-safe** (`(int32_t)(now-endUs)>=0`) and
  **drift-free** (`restart(delay)` does `endUs += delay`, not `now + delay`).

Already migrated onto it:
- Modulator `dt` — `Engine.cpp:63` (`_lastWallUs = _wallClock.now()`), `:73-74`
  (`dt = (nowUs - _lastWallUs) * 1e-6f`, wrap-safe delta).
- MIDI CC rate-limit — `MidiOutputEngine.cpp:30-33` (`WallClock().now()`, 20 ms gate).
- Also used by `CvGateToMidiConverter.h`, `TeletypeBridge.cpp`.

---

## Remaining work

### B1 — CLOSED / won't-do (2026-06-09): os::ticks() is the correct clock here
Investigated `MidiCvTrackEngine` voice scheduling. The retrigger gate pulse is
**implicitly defined by os-tick (~1 ms) granularity** via wrap arithmetic:
`gateOutput()` returns `(voice.ticks - os::ticks()) >= delay` with `voice.ticks`
stamped at allocation (`:215`) and `delay = RetriggerDelay(2)` when retrigger is on.
`voice.ticks - now` is 0 only while `now` hasn't advanced past the stamp, so the gate
is held low for ~one os-tick after a re-strike — that IS the retrigger edge. Moving to
µs would collapse that ~1 ms low window to ~1 µs and the retrigger edge would vanish.
So os::ticks() is load-bearing, not incidental, at this site. **Not migrated.** The
"unify all wall-time" goal does not apply to a deliberately tick-quantized gate pulse.
(The other `os::ticks()` uses — uptime stat, RNG seeds, `UpdateReducer` — were already
out of scope as non-timing.)

### B2 — slave-clock outlier guard
Add the ER-101 0.5×–2× period latch on the external-clock interval so a single
jittery/garbage clock edge can't yank tempo. This **absorbs** the slave-clock filter
item parked under `performer-improvements` Phase 0 — this is its proper home.

### A — phase-source unification — PARKED with cadence data (2026-06-09)
Original premise: PhaseFlux + Stochastic re-derive phase from the integer tick while
six engines use `Clock::tickPosition()`; migrate them for sub-tick smoothness.

**Investigation found the premise weak — parked, not scheduled.**
- The "6 engines use tickPosition" framing is misleading. CurveTrack uses it for its
  *free-run oscillator* mode (`_freePhase += deltaTicks*speed`, `CurveTrackEngine.cpp:190`)
  and separately slews CV toward target in `update(dt)`. Not a universal smooth-phase idiom.
- PhaseFlux's continuous phase (`_stagePhase`) is produced *inside*
  `PhaseFluxMath::deriveTickPosition()`, the same call that does discrete cell/slot
  selection — no clean read to redirect; migrating means restructuring that shared helper.
- **Cadence**: CONFIG_PPQN=192. `tick()` fires per PPQN tick (`Engine.cpp:176`);
  PhaseFlux computes CV there and **ignores `dt`** in `update()` (`:820`). Result: CV is a
  zero-order-hold staircase at PPQN resolution — ~2.6 ms/step @120 BPM (~1.3 ms @240,
  ~5.2 ms @60). For a modulation/pitch control signal that stepping is sub-audible and
  matches the sequencer's native tick resolution; the existing code already calls it a
  "smooth envelope" (`:670`).
- Stochastic is event-triggered (LUT trigger steps at integer-tick boundaries) — no
  meaningful continuous phase to migrate.

**Conclusion:** the `tickPosition`/`deriveTickPosition` migration buys ~nothing the PPQN
sampling doesn't already give, for real restructure risk. If smoother PhaseFlux CV is ever
wanted, the cheap, correct mechanism is inter-tick slew in `update(dt)` (mirroring
CurveTrack), NOT a phase-source change. Revisit only if PPQN-step staircase proves audible
in practice. U2/U3 closed pending that evidence.

---

## Preliminary steps

1. **Read** the design doc + Catalyst `clock.hh` + ER-101 `wallclock.h` to confirm the
   slave-guard math before touching the clock path.
2. **B1** (smallest, isolated): convert MidiCv voice scheduling to `WallTimer`. Sim +
   STM32 build; verify note timing unchanged.
3. **B2**: add the slave-clock 0.5×–2× latch where the external clock interval is
   measured; hardware-audition against a deliberately jittery clock.
4. **A** (separate commit): point PhaseFlux + Stochastic phase at `tickPosition()`;
   verify timing parity vs the integer-tick path (no audible drift, identical at
   integer tick boundaries).
5. Update STATUS block; close the `performer-improvements` slave-clock item as folded.

---

## Resource
Trivial. No 64-bit emulation, no counter-extension plumbing, no new buffers. B1/B2 are
a few lines each; A swaps a phase expression in two engines.

## Gate
Sim build + run, STM32 release green, timing parity check on A, hardware audition on
the B2 slave guard. Commit per half. No ProjectVersion bump (no serialized layout change).

## Key files
- `engine/WallClock.h` (substrate, done) · `engine/Engine.cpp:63,73-74` (dt)
- `engine/MidiCvTrackEngine.cpp:113,215` (B1) · `engine/MidiOutputEngine.cpp:30` (done ref)
- `engine/PhaseFluxTrackEngine.cpp` + `engine/StochasticTrackEngine.cpp` (A)
- `engine/Clock.{h,cpp}` (`tickPosition()`)
- Design: `docs/plans/2026-05-29-wallclock-time-architecture-design.md`
- Precedent: `../DSP/4ms-catalyst/src/clock.hh` · `OTHERS/ortagonal/er-101/wallclock.h`
