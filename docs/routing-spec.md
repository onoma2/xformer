# Routing — Spec / Manual (v0.8.0)

The routing system is a **modulation matrix**: any *source* (a CV input, a bus, a
modulator, a MIDI message…) can drive any *target* (a track/sequence param, a
play-state action, the transport, a global setting, or a CV bus). Up to 16 routes
exist at once.

Routing here is **modulate-around-a-base**, not overwrite: a route adds an offset
on top of the value you set on the param's own page, so params stay editable while
routed. This is the core difference from legacy Performer routing (which replaced
the value). See `routing-legacy-vs-matrix.md` for the provenance.

---

## 1. Three apply models

A route reaches its target through one of three paths — the target decides which.

### 1.1 Override (base + delta) — continuous params
For continuous per-track/global params (Transpose, Scale, Divisor, Octave, Tempo…).
The route turns the source into a **delta** — scaled by the route's `depth %`, bent
by its `shaper`, its `combine` mode deciding base-relative vs absolute — and writes
it to the override table. The effective value is your base plus that delta, clamped
to the param's range. Your hand-set **base is preserved**; routing modulates around it.

- **Combine = Modulate** (default): the delta is centered — the source sweeps the
  param up *and* down around the base.
- **Combine = Absolute**: the source sweeps the param across its full range from the
  base upward (a one-directional sweep).
- **scale source**: an optional second source that dynamically scales the route's
  depth (a VCA on the modulation amount). `None` = fixed depth (identity).

### 1.2 Trigger (raw source) — actions & transport
**Trigger targets** — **Run / Reset** plus the five **global transport** actions
(Play / Play-Toggle / Record / Record-Toggle / Tap-Tempo) — read the **raw** source
level only:

- **Run** — threshold (on above ~0.55, hysteresis-friendly).
- **Play / Record** — level-following: transport/record state mirrors the source level.
- **Reset / Play-Toggle / Record-Toggle / Tap-Tempo** — fire on a **rising edge**.

Depth, scale source, shaper and combine **do not apply** to trigger targets. The tab
editor hides those views for a trigger row and shows a source only.

### 1.3 Bus (sum-then-clamp)
Sources routed to a CV bus **sum** into the bus (clamped ±5 V), so several
modulators can feed one bus. Created on the dedicated **BUS tab**, not the param
bands. A bus routing to *itself* is blocked; cross-bus feedback (bus1→bus2→bus1) is
allowed — it's bounded and behaves like patching a CV output back to an input.

---

## 2. The tab editor

The routing page is a **tab editor**: a ring of tabs across the top, each showing a
column of param rows × 8 track cells.

Tabs:
- **Fixed bands** — `Pitch`, `Clock`, `Global`: the cross-engine params grouped by concept.
- **Engine pages** — one per track mode: that engine's own routable params.
- **BUS** — the four CV bus lanes.
- **MIDI** — the learned MIDI routes.

### 2.1 The three fixed bands
- **Pitch** — Scale, Root Note, Transpose, Octave, Slide Time, Rotate.
- **Clock** — Divisor, Clock Multiplier, **Run, Reset**, **Play, Play-Toggle, Record,
  Record-Toggle, Tap-Tempo**, **Mute, Fill, Fill Amount, Pattern**. (Clock + all
  per-track and global transport / play-state.)
- **Global** — Tempo, Swing, CV-Router Scan/Route, CV-Output Rotate, Gate-Output Rotate.

### 2.2 Rows: per-track vs global
- **Per-track rows** show 8 track cells; a track is a *member* of the route when its
  cell has a non-zero depth. Membership is implicit — dial a cell's depth up to add
  the track, down to 0 to drop it.
- **Global rows** (project + engine-scope targets: Tempo, Swing, Play, Record…) have
  no per-track dimension — one shared value shown across the columns.

### 2.3 Views
Cycle with **VIEW (F1)**: `SOURCE → DEPTH → SCALE → SHAPER → SOURCE`.

- **SOURCE** — which source drives the row (one source per row).
- **DEPTH** — per-track depth % (membership + amount).
- **SCALE** — the row's scale source (dynamic depth gain).
- **SHAPER** — the row's response curve (fold/crease variants).

**Trigger rows** (Run/Reset/transport) offer **SOURCE only** — the VIEW cycle stays
on SOURCE and the cells always show the source, since depth/scale/shaper are
meaningless for a raw-trigger consumer.

### 2.4 Editing a route
- Move the cursor to a row, **EDIT (F2)** to open the draft.
- **SOURCE** view + encoder picks the source; **DEPTH** view + encoder dials the cell's depth.
- **COMBINE (F3)** toggles Modulate ↔ Absolute.
- **COMMIT (F5)** writes the draft to the live route; **CANCEL (F4)** discards it.
  Edits are staged in a draft — nothing changes until Commit, so Cancel truly reverts.

### 2.5 MOD+ / MOD- inline setup
From a track or sequence param page you can add modulation to the param under the
cursor without opening the routing page:
- **MOD+** creates a route for that param (or **extends** the existing one onto the
  current track — one route per param, extra tracks are added to it, not minted as a
  second route).
- **MOD-** removes the current track from the param's route (freeing the route when
  its last track is dropped).

---

## 3. Sources

Source labels: `IN1-4` (CV inputs), `O1-8` (CV outputs), `B1-4` (CV buses), `G1-8`
(gate outputs), `M1-8` (modulators), MIDI (configured on the MIDI tab), `None`
(inert — a route with no source does nothing).

A route's **scale source** picker offers the same list minus MIDI, the route's own
primary source (no self-squaring), and any self-routing bus.

---

## 4. Target families

| Family | Examples | Apply model | Scope |
|---|---|---|---|
| Pitch/structural | Scale, Root Note, Transpose, Octave, Slide Time, Rotate | override | per-track |
| Clock | Divisor, Clock Multiplier | override | per-track |
| Transport (per-track) | Run, Reset | trigger | per-track |
| Transport (global) | Play, Play-Toggle, Record, Record-Toggle, Tap-Tempo | trigger | global |
| Play-state | Mute, Fill, Fill Amount, Pattern | level-driven | per-track |
| Global | Tempo, Swing, CV-Router Scan/Route, CV/Gate-Output Rotate | override | project |
| Bus | Bus CV 1-4 | sum-then-clamp | — |
| Per-engine | each track mode's own params (Algorithm, Chaos…, Fractal…, Stochastic…, DiscreteMap inlets, Indexed A/B, …) | override | per-track |

### 4.1 Play-state targets (Mute/Fill/FillAmount/Pattern)
Per-track, **level-driven**, re-evaluated each frame and idempotent:
- **Mute / Fill** — threshold 0.5.
- **Fill Amount** — source scaled to 0..100.
- **Pattern** — source scaled to the rounded pattern index (0..15); inert while a
  snapshot is active.

### 4.2 Global transport
Play/Record follow the source level; Play-Toggle/Record-Toggle/Tap-Tempo fire on a
rising edge. Routing
both Play *and* Play-Toggle (or Record + Record-Toggle) is allowed but can fight —
prefer one transport control per action.

---

## 5. BUS tab & CV router

Four CV bus lanes, each: live bipolar volts, its one feeding route (source + depth),
and CVR/TT activity lights. Buses are a mixing node — multiple routes sum into one
bus (clamped ±5 V). The independent interpolating **CV router** is a 4×4 matrix with
Scan/Route targets that can also feed the buses.

---

## 6. MIDI routing

A route whose source is MIDI is configured on the **MIDI tab** (port / channel /
event / CC-or-note). MIDI-learn assigns an incoming message to a route. When a slot
is reassigned from a CV source to MIDI, its value is cleared so it starts neutral
until the first matching MIDI message.
