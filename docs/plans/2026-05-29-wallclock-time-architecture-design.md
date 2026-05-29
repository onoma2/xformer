# Wallclock / Time Architecture — A + B

_2026-05-29. Validated design. Two parallel gaps in how xformer handles time: (A) the two newest continuous engines don't read the shared transport position; (B) real wall-time is scattered across call sites with no service. ER-101 (`OTHERS/ortagonal/er-101`) is the reference for B._

## Current state

Two distinct time sources, used unevenly:

**Transport `Clock::tickPosition()`** (`Clock.cpp:461`) — continuous fractional transport tick, `baseTick + elapsedUs/tickPeriodUs`. Tempo-scaled, freezes on stop. Read by six engines: Note, Curve, Indexed, DiscreteMap, Tuesday, Teletype (Free-mode / continuous phase). **Holdouts: PhaseFlux and Stochastic** re-derive phase from the discrete integer tick.

**Real wall-time** — `os::ticks()` called directly in three behavioral spots, no abstraction:
- `Engine.cpp:72` — `dt` for the global modulators (chaos/LFO run on real time).
- `MidiCvTrackEngine.cpp` — voice note timing.
- `MidiOutputEngine.cpp` — CC send rate-limit (50/s).

`os::ticks()` is 32-bit (wraps). The modulator-`dt`-vs-transport semantics are a flagged stability concern.

## ER-101 reference (what transfers, what doesn't)

ER-101 uses a single 64-bit free-running `wallclock()` as *the* reference; everything is a timestamp/duration against it.

**Kernel — `walltimer`** (`wallclock.h`): a timer is an absolute deadline `end_time` against the wallclock. `walltimer_restart` does `end_time += delay`, **not** `end_time = now + delay` — subdivisions accumulate from the previous deadline, not the service moment, so repeated firing never drifts.

**Transfers to xformer:**
1. Drift-free deadline pattern (`end_time += delay`) for wall-time consumers.
2. 64-bit wall reference — avoids the `os::ticks()` 32-bit wrap in long sessions.
3. Outlier-guarded period latch (`this_period > last/2 && < 2*last`, `interrupt_handlers.c`). ER-101 and performer-nx independently converge on bounding the slave period; the 0.5×–2× latch is the proven minimum, performer-nx's 4-phase pipeline is the deluxe version.

**Does NOT transfer:** ER-101's synthetic-clock generation + per-track tick-adjustment keep 4 independently-subdivided tracks drift-locked. xformer's transport `Clock` already distributes one tick to all engines simultaneously — cross-track sync is centralized. Importing that machinery re-solves a problem xformer doesn't have.

## Design

### B — `WallClock` service (the substrate)

A 64-bit µs free-running clock + a `WallTimer` value type:

```cpp
class WallClock {            // wraps os::ticks() into monotonic 64-bit µs
    uint64_t now() const;    // free-running, independent of play/stop and tempo
};

struct WallTimer {           // absolute-deadline timer, drift-free
    uint64_t endUs = 0;
    bool running = false;
    void schedule(uint64_t deadlineUs);
    void start(WallClock &c, uint64_t delayUs);
    bool elapsed(WallClock &c);     // true once, when now() > endUs
    void restart(uint64_t delayUs); // endUs += delayUs  (NOT now + delay)
};
```

Migrate the three scattered sites onto it:
- Global modulator `dt` → derived from `WallClock::now()` deltas; settles the dt-vs-transport tension by making "modulators run on real time" explicit and principled.
- MidiOutput CC rate-limit → `WallTimer` at 50/s, drift-free.
- MidiCv voice timing → `WallClock` timestamps.

Fold the **slave-period outlier guard** here too (start with the ER-101 0.5×–2× latch; the Phase 0 stability item's 4-phase pipeline is an optional later upgrade). This consolidates the Phase 0 slave-clock fix into the time module.

### A — transport consistency (parallel, independent)

Bring PhaseFlux and Stochastic onto `Clock::tickPosition()` for continuous phase, matching the other six engines, instead of integer-tick derivation. Smoother sub-tick phase + one consistent rule. Does not depend on B.

## Sequencing & verification

A and B are independent; B is the larger substrate. Each is a code-behavior change to engines — verify sim build + run, then STM32 release, and confirm timing parity (no audible drift, CC rate unchanged, modulator rate unchanged) against current behavior. The slave-period guard needs a hardware check against a jittery/swung external clock.

## Out of scope

ER-101 synthetic-clock / per-track tick-adjustment (xformer transport already centralizes cross-track sync). No transport `Clock` rework — `tickPosition()` stays as-is; A just adds two readers.
