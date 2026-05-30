# Clock subsystem map

_Surveyed 2026-05-30. Research artifact — no direction chosen. The `wallclock-time-architecture` design doc covers the time-source slice (continuous `tickPosition()` vs wall time, and the slave-period filter). This maps the rest of `Clock`: master generation, slave input, MIDI sync out + swing, tap/nudge, sub-tick scheduling._

## Inputs / sources

- **Master (internal):** generated from `_masterBpm`, set by `Engine` from `_project.tempo()` × nudge (`Engine.cpp:137`). `NudgeTempo` adds a ±10% transient; `TapTempo` derives BPM from tap intervals.
- **Slave (external clock in):** `slaveTick(slave)` (`Clock.cpp:129`). Measures `periodUs`, derives `_slaveTickPeriodUs` (raw — the filter gap, owned by wallclock task) and `_slaveSubTickPeriodUs`. Display BPM smoothed separately (`:165`).
- **MIDI clock in:** consumed as a slave source via the same path.

## Sub-tick scheduling

`_slaveSubTicksPending` / `_slaveSubTickPeriodUs` (`:140-160`) subdivide each external pulse into `_ppqn` internal ticks, with a clamp against rate overload. The internal tick stream is what `tickPosition()` interpolates over.

## Output — MIDI sync out + swing

- `outputMidiMessage(Start/Stop/Continue/Tick)` (`:357-418`) emits MIDI real-time clock. **MIDI sync out exists** (`outputMidiMessage(MidiMessage::Tick)`, `:418`).
- **Swing** is applied to the *output* tick: `Groove::applySwing(tick, _output.swing)` (`:421-424`), configured via `outputConfigureSwing` (`:278`). `Groove.h` holds the swing curve.

## Helpers

- `TapTempo` — interval-averaged BPM.
- `NudgeTempo` — transient tempo bend (used for the ±10% nudge).
- `Groove` — swing application to output clock.

## What's already owned elsewhere

- Continuous `tickPosition()` (interpolated transport position) and the **slave-period outlier filter** → `wallclock-time-architecture` (do not duplicate here).
- The 1ms minimum clock-pulse floor → already shipped (performer-improvements Phase 1).

## Candidates (deferred — not chosen)

- **Swing scope:** swing is applied only to MIDI *output* clock (`_output.swing`), not the internal tick stream — confirm whether internal/track swing is intended or a gap. Groove may be underused.
- **Source unification:** master / slave / MIDI-in are three entry points into one tick stream; document the precedence and handoff (what happens on source change mid-play) as a contract.
- Tap/nudge feed master BPM through `Engine`; fine as is.

## Relation to other maps

- Time-source half lives in `wallclock-time-architecture` — this map is the structural complement (generation/sync/swing), not the µs-precision half.
- The internal tick stream this produces is consumed by `engine-update-contention-map` (the per-tick loop) and by every track engine.
