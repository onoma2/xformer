# TT2 MOD.* / G.* — modulator & Geode control ops

Created: 2026-06-14
Branch: feat/tt2-v2-native
Status: requirements (pre-plan)

## Problem

TrackMode::TeletypeV2 has no native way for a script to touch the modulator or
Geode engines. The legacy `TeletypeTrackEngine` carries its own duplicate
envelope/LFO/Geode generators (`setEnv*`/`setLfo*`/`setGeode*`/`triggerGeodeVoice`)
driven by the legacy `E.*`/`LFO.*`/`G.*` ops. That duplication is the deletion
target when the bridge goes.

Performer already has the real engines those ops were imitating:

- **Modulator engine** — 8 global slots (`Mod1..Mod8`), unified Shape palette
  (Sine/Triangle/SawUp/SawDown/Square/Random/ADSR/Lorenz/Latoocarfian/Spring),
  Mode {Run/Trig/Gate}, RateDomain {Free/Tempo}, edited by `ModulatorPage`,
  routed as matrix sources and direct CV-out assignments.
- **Geode engine** — standalone `GeodeConfig` (`Project::_geode`) + `GeodeEngine`
  (polyrhythmic voice generator). When `geode.active`, `Engine.cpp` reads M1=clock,
  M2=run, runs the generator, and writes the 6 voice levels into modulator slots
  M3–M8 via `ModulatorEngine::setVoiceOutput`.

The native TT2 dialect should let advanced scripts reach into these existing,
already-functioning engines — not redefine modulator control.

## Framing: BUS in spirit

`MOD.*` and `G.*` are **thin accessor ops**, modeled on `BUS`. `BUS i` reads a
CV-bus lane, `BUS i v` writes it, through `TT2Host` into an engine feature the UI
already drives. `MOD.*`/`G.*` are the same shape: sophisticated internals exposed
for advanced users, with the modulator/Geode **UI remaining the primary control
surface**. No new modulator model, no new shapes, no slot-ownership redesign.

This is explicitly **not** the earlier "evolved/redefined modulator control"
direction. The modulator engine rework in `.tasks/modulator-enhancements.md`
(Free/Tempo rate, output-only, 3 gates) is its own UI-driven track; these ops
expose whatever fields the engine ends up offering, nothing more.

## Decisions

### D1 — Shared global pool, accessed via TT2Host
`MOD.*` addresses the existing global `Mod1..Mod8` (`Project::modulator(n)`,
1-based). `G.*` addresses the single global `GeodeConfig`/`GeodeEngine`. Both reach
the engine through new `hostModulator*` / `hostGeode*` methods on `TT2Host`,
mirroring how `BUS`/`RT`/`W*` already reach `Engine`. The op layer stays off
`Engine.h`.

### D2 — Generic accessor verbs over existing fields
Every field `ModulatorPage` edits gets a get/set accessor; the slot's live output
gets a read; trigger gets a verb. No per-shape ops (no native `E.*`/`LFO.*`). The
legacy `E.*`/`LFO.*`/`G.*` stay in the bridge and die with it — native TT2
implements `MOD.*` + `G.*` only.

### D3 — Contention: direct field write, last writer wins
Modulator depth/rate/shape are **not** routing targets — modulators are routing
*sources* (`Mod1..Mod8`) and a Geode sink (`setVoiceOutput`); their own fields are
edited only by `ModulatorPage`. There is no `routedValue` base+delta arbitration on
them. So a script write sets the stored field directly: `MOD.DEPTH n v` writes
`modulator(n)` depth; the knob can re-set it; whoever moved last holds — same as
editing the field from two UI places. No shadow layer, no latch, no new machinery.
`ModulatorPage` shows live field values, so a script write is visible immediately.

A summing knob+script model (script trims around a knob base) would require making
modulator fields routing targets first — a separate engine change, out of scope for
a thin accessor op.

### D4 — Native units per field, no blanket CV normalization
Modulator fields are not a uniform 0..127 domain — each has its own range. Ops
expose **native units**, matching the `MI.*` precedent (which returns raw 0..127),
not the `CV`/`BUS` 0..16383 domain:

- slot output read (`MOD n`): native **0..127** (same shape as `MI.NV`)
- `MOD.DEPTH`: 0..127
- `MOD.RATE`: domain-keyed — Free = centi-Hz 1..1600 (0.01–16 Hz), Tempo = PPQN
  ticks 6..6144
- `MOD.SHAPE` / `MOD.MODE`: enum index

Script author types the same numbers the page shows. To drive a 14-bit CV out from
a modulator value, the script scales explicitly (e.g. `CV 1 (MOD 3 * 128)`) — the
same explicit bridge a script already does with a MIDI velocity. Forcing 0..16383
on a setting field would invent resolution the engine can't use (depth has 128 real
steps).

## Op surface (product behavior)

Exact op names and the exhaustive field list are a planning detail; the surface
below is the behavior contract.

### MOD.* (per slot `n`, 1-based 1..8)
- **`MOD n`** — read slot's current output value (native 0..127)
- **`MOD.SHAPE n [v]`** — get/set Shape enum
- **`MOD.RATE n [v]`** — get/set rate (native domain units)
- **`MOD.DEPTH n [v]`** — get/set depth (0..127)
- **`MOD.MODE n [v]`** — get/set Mode (Run/Trig/Gate)
- **`MOD.OFF n [v]`** — get/set offset
- **`MOD.TRIG n`** — trigger the slot (for Trig/Gate/ADSR shapes)
- Remaining editable `Modulator` fields (rateDomain, invert, smooth, ADSR
  attack/decay/sustain/release, Spring params) follow the same get/set pattern;
  which to expose is a planning enumeration, not new behavior.

### G.* (single global Geode engine)
- **`G.TIME [v]`**, **`G.INTONE [v]`**, **`G.RAMP [v]`**, **`G.CURVE [v]`** —
  get/set the Geode globals (native `GeodeConfig` ranges)
- **`G.MODE [v]`** — get/set Geode mode
- **`G.ON [v]`** — get/set `active`
- **`G.DIV n [v]`**, **`G.REP n [v]`** — per-voice divs / repeats
- **`G.TUNE n [num den]`** — per-voice tune ratio
- **`G.V n`** — trigger voice `n`; **`G.VA`** — trigger all voices

Voices keep landing in modulator slots M3–M8 exactly as `Engine.cpp` wires them
today; `MOD.*` shapes those slots' depth/offset/invert while the Geode generator
drives their level.

## Scope boundaries

In scope:
- Native `MOD.*` + `G.*` accessor ops reading/writing existing `Modulator` /
  `GeodeConfig` fields and `GeodeEngine` triggers via new `TT2Host` methods.
- Removing `MOD.*`/`G.*` from the TT2 UnsupportedOp set; unit tests for the
  accessors (get/set roundtrip, trigger effect, clamp behavior).

Deferred / out of scope:
- The modulator engine rework (`.tasks/modulator-enhancements.md`) — independent
  UI-driven track; these ops adapt to whatever it lands.
- Any new editor UI — these are script-only internals (like `BUS`).
- A summing knob+script arbitration model (would need modulator routing targets).
- The legacy `E.*`/`LFO.*`/`G.*` bridge ops and `TeletypeTrackEngine`'s duplicate
  generators — they stay until bridge deletion.
- MIDI-out / any non-modulator concern.

## Dependencies

- `TT2Host` gains `hostModulator*` / `hostGeode*` methods; `TT2TrackEngine`
  implements them against `Project::modulator(n)` / `Project::geode()` /
  `GeodeEngine`.
- Reuses the existing `ScopedHost` script-execution wiring; no new dispatch
  architecture.

## Success criteria

- A TT2 script can read a modulator slot's output and get/set its editable fields
  in native units, with changes reflected live on `ModulatorPage`.
- A script can configure the Geode globals + per-voice rhythm and trigger voices,
  with voices appearing in M3–M8 as today.
- `MOD.*`/`G.*` are no longer UnsupportedOp; the legacy bridge ops are untouched.
- No new shadow/arbitration machinery added to `Modulator`.
