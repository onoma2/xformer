# TT2 Supertrack — Design (goals resolved, storage open)

Status: design. Goal tree resolved via grill session; the storage mechanism is
the one open decision (see §6). Capability constants are parametric (§4).

## 1. Goal

Reshape the track model so track types can have **heterogeneous RAM sizes** as a
first-class concept, with TT2 as the first heavy citizen. A TT2 instance costs
the RAM of **4 plain tracks** ("eats a quad"). With 8 tracks total, that caps
TT2 at **2 instances** — the cap is structural, not a separate rule.

Cost unit is **RAM only**. CPU and output-lane count are not the budget metric.

The point is to **reuse** RAM the union already reserves, not add new RAM. See
the doubling pitfall in §6.

## 2. What the arena buys

A TT2 quad unlocks a bigger Teletype brain:

- More scripts per scene (toward original Teletype's 10: 8 numbered + Metro +
  Init). Performer has 4 trigger inputs, so numbered scripts beyond 4 are
  call/CV-fired, not hardware-jack-fired.
- **Scenes** = full script-bank switches. A scene holds a complete program
  (all scripts + its own 4×64 pattern tracker).

### Scene = Performer pattern slot

A TT2 **scene** maps onto a Performer **pattern slot**. Switching the global
pattern switches the active scene — reuses the existing pattern-switch gesture,
no new mechanism. Performer has `CONFIG_PATTERN_COUNT = 16`
(`src/apps/sequencer/Config.h:51`); if TT2 carries fewer than 16 scenes, the
global pattern index **wraps via modulo** into the scene count — the exact
precedent already in `TeletypeTrack` (`clamp(patternIndex,0,n-1) % PatternSlotCount`,
`src/apps/sequencer/model/TeletypeTrack.h`).

Term note: **Performer pattern-slot** (the switch gesture, = a TT2 **scene**) is
distinct from TT2's own **pattern tracker** (4×64), which lives *inside* each
scene.

### Scene-switch semantics

Scene load **resets like real Teletype**: reload that scene's scripts +
patterns, reset variables/runtime, re-run Init. Only **one active runtime**
(`TT2Runtime`) is needed — scenes are program-only storage.

## 3. Placement, lanes, outputs

- **Placement — fixed quads.** TT2 selectable only on track 0 or track 4; each
  eats its quad (0–3 / 4–7). "Max 2" is automatic and trivially enforced.
- **Donor slots stay as routable lanes.** Tracks 1–3 (and 5–7) are not
  independent tracks; they appear as routable output destinations that bind to
  the TT2 owner. Selecting TT2 on the anchor wipes the donor slots' prior data
  (dev: files break freely, no migration).
- **Outputs — internal 8/8, routed freely.** TT2 keeps its 8 CV / 8 TR internal
  outputs (`TeletypeOutputState.h`) and routes any to any physical output via
  the existing routing UI, not hard-tied to the eaten lanes.

## 4. Capability budget (measured, parametric)

Two constants, `scriptsPerScene` and `sceneCount`, with a compile-time budget
assert. Dial later without redesign.

Measured arena (model union, slots of one quad):

- Quad arena = 4 × 9544 = **38,176 B**. 9544 is the `Container` ceiling, set by
  `StochasticTrack`/`PhaseFluxTrack` (`PhaseFluxTrack.h:175`,
  `StochasticTrack.h:200`) — not by TT2.
- Per-scene = one `TeletypeProgram`, program-only, **no scene-text field**
  (`TeletypeProgram.h`). 6 scripts = ≤2384 B. Each added script = **+304 B**
  (`TT2Script`). The 4×64 tracker is already inside that 2384.
- Active runtime = `TT2Runtime` ≤2160 B, **one copy** (scene-load resets it).
- `CONFIG_SNAPSHOT_COUNT = 1`, negligible.
- Usable for scenes ≈ 38,176 − 2,160 ≈ **36,000 B**.

Curve (bounds are `<=` ARM figures; true sizes run slightly under, each count can
nudge up ~1):

| scripts/scene | bytes/scene | scenes |
|---|---|---|
| 10 (full Teletype) | ~3600 | ~10 |
| 8 | ~2992 | ~12 |
| 6 (current) | ~2384 | ~15–16 (≈1:1 with 16 pattern slots, no wrap) |

Engine side is not part of the arena: `TT2TrackEngine` (≤944 B) stays in the
anchor's engine container; donor engine slots are free bonus.

## 5. Current sizes (baseline)

- `TT2Track` = 4552 B (`TeletypeProgram` ≤2384 + `TT2Runtime` ≤2160 + index),
  using 48% of one 9544 slot today (`TT2Track.h`).
- `TeletypeProgram`: 6 scripts (4 trigger + Metro + Init), 4 patterns, I/O
  mappings (`TeletypeProgram.h`).
- 8-track model union ≈ 8 × 9544 ≈ **76 KB**, always, regardless of modes
  (`Project.h:35`, `std::array<Track, CONFIG_TRACK_COUNT>`).

## 6. OPEN — storage mechanism

The RAM-reuse constraint forces a real choice. A TT2 arena must **physically
overlap** the quad's bytes to count as "eating 4 tracks" rather than adding
memory.

**Doubling pitfall (rejected):** a separate `std::array<TT2SuperTrack,2>` pool
beside the unchanged `std::array<Track,8>` is **new** RAM. 76 KB union + 2×38 KB
pool ≈ **152 KB — doubles the footprint.** Violates the reuse goal. A separate
pool can never reuse the union bytes; separate memory = added memory.

Reuse ⇒ overlap ⇒ a union. The open decision is *where* the overlap lives:

- **Option A — quad-level discriminated union.** `TrackArray` becomes 2 quads;
  each quad is `Container<4×Track | TT2SuperTrack>` over the same ~38 KB.
  Total ≈ 2 × max(4×9544, ~38 KB TT2) ≈ **76 KB, identical to today.** Uses the
  same placement-new + tag pattern as the per-track `Container` (`Track.h:406`),
  lifted to quad scope. No raw `reinterpret_cast`. Cost: track indexing becomes
  `i → (quad i/4, sub i%4)`; donor-slot guards where a quad is in TT2 mode.

- **Option B — literal slot-spanning reinterpret.** TT2 at the anchor
  reinterprets the contiguous `Container`s of its 4 slots as one 38 KB buffer.
  Also reuses RAM, but relies on union layout/contiguity assumptions and raw
  type-punning, with ownership guards scattered across every track loop.

Recommendation leaning A (type-safe, mirrors existing pattern), pending the
impact pass below.

## 7. Impact surface — Option A (quad model), detailed

### 7.0 The decision that shrinks the work

`CONFIG_TRACK_COUNT` stays **8 = logical track count**. Every per-logical-track
array stays `[8]`: `PlayState` track state, `Routing` bias/override arrays,
`Engine` output-index arrays, `Clipboard`. They are NOT resized.

Only **two** things change shape:

1. **Model storage** becomes the quad-union (this is the entire RAM-reuse win).
2. **Engine stays flat-8** — `_trackEngines[8]` with donor entries = `nullptr`.
   Engines are small (944 B); no reuse pressure, so no quad-union on the engine
   side. This halves the structural churn.

Two helpers carry most of the rest: `anchorOf(i) = (i/4)*4` and `isDonor(i)`
(quad is TT2 and `i%4 != 0`).

So the work is: one real reshape (model storage) + a bounded list of mechanical
null-guards (engine) + a bounded list of donor reject/redirect (cross-track
index refs) + serialization rewrite + UI masking.

### 7.1 Model storage — the actual reshape

- `Project.h:35` `using TrackArray = std::array<Track, 8>` → quad storage
  (`std::array<Quad, 2>`, `Quad = Container<4×Track | TT2SuperTrack>`).
- The `Container` + pointer union lifts from per-`Track` (`Track.h:405-417`) to
  per-quad; `initContainer()` (`Track.h:352-397`) and `setTrackIndex()`
  (`Track.h:306-343`) become quad-aware.
- `project.track(i)` (`Project.h:350`) becomes a resolver: `(i/4, i%4)`; a donor
  sub returns the anchor (or a donor proxy).
- **`tracks()` full-array accessor (`Project.h:347`) breaks** — it can no longer
  hand back a `std::array<Track,8>&`. Needs a logical-view facade or the few
  callers that iterate it change. (Flagged: only real API casualty.)
- `Project.cpp` track init/clear loops (`9-10`, `51-53`) iterate the logical view.

### 7.2 Engine — the null-donor invariant (mechanical guards)

`updateTrackSetups()` (`Engine.cpp:610-654`) becomes quad-aware: TT2 quad creates
ONE engine at the anchor, donor slots get `_trackEngines[i] = nullptr`.

Every deref then needs a null-guard (or anchor redirect). Bounded list:

- Tick loop `Engine.cpp:183-194`; broadcast `update()` ×2 `284-296`; global
  `reset()` `867-869`; song `restart()` `~1050`; `changePattern()` `~1065`.
- Output retrieval: `gateOutput()` `~733`, `cvOutput()` `~754`, and the slot
  precompute `708-716` (don't increment slot for a null donor).
- Cross-track engine reads (already bounds-checked, add `!= nullptr`):
  `RoutingEngine` reset target `~276`; `NoteTrackEngine` divisor-Y gate `~232`,
  harmony master `~941`; `TeletypeTrackEngine` readTrigger/readCv/readPattern/
  readStepIndex (`~911`, `~844`, `~1000`, `~716`).

### 7.3 Cross-track index references — donor reject/redirect

A stored "track N" index can now name a donor. Reject at the setter or redirect
to anchor at use:

- `NoteSequence::setMasterTrackIndex` (`NoteSequence.h:718`), `setDivisorYTrack`
  (`NoteSequence.h:515`) — harmony master and divisor-Y links.
- `Project::setCvOutputTrack` (`Project.h:359`), `setGateOutputTrack`
  (`Project.h:371`) — output routing destination validity.
- `Routing` per-track target bitmask `Route::_tracks` (`Routing.h:736-758`,
  `isRouted` `Routing.cpp:242-248`) — donor bit maps to anchor.
- `Song::Slot` per-track pattern/mute (`Song.h:22`) — donor → anchor.
- `Project::setSelectedTrackIndex`/`selectedTrack` (`Project.h:446,487`) — no
  donor selection (redirect to anchor).

### 7.4 Serialization — NO migration (contract)

Quad-union write/read with a one-byte discriminant per quad (track-mode set vs
TT2SuperTrack). `Project.cpp:198/255`, `Track.cpp:165-256`.

Per the no-migration rule: write the new layout **unconditionally**, no version
gate, no flat-8 fallback. Old dev files break. (Agents recommended a version
bump + migration — rejected as a contract violation.)

Scenes ride the existing pattern storage path (scene = pattern slot), so the TT2
scene bank is not a new serialization concept — it's the pattern array of the
TT2SuperTrack.

### 7.5 UI — anchor shows, donors masked

- Track list / `TrackModeListModel`: a TT2 quad shows one track at the anchor;
  donor rows hidden (or shown read-only as "→ anchor").
- Mode select: TT2 (`TrackMode::TeletypeV2`) selectable only on anchors 0/4;
  setting it on a donor redirects to the anchor.
- Per-track UI loops (track grid, performer LEDs, routing track-select) skip
  donors. Specific page file:line need verification before implementation —
  treat the UI list as behavioral, not yet line-cited.

### 7.6 Pattern-switch wiring

Global pattern index → TT2 scene index with modulo wrap (existing
`TeletypeTrack` precedent). Donor lanes inherit the anchor's pattern/scene.

### 7.7 Effort shape

| Part | Nature | Size |
|---|---|---|
| Model quad-union storage + `track(i)` resolver | real reshape | the hard part |
| `tracks()` facade / caller fix | API | small, isolated |
| Engine null-guards | mechanical | ~12–15 sites, listed |
| Cross-track reject/redirect | mechanical | ~6 sites, listed |
| Serialization quad-union | rewrite, no migration | moderate |
| UI anchor/donor masking | display logic | moderate, needs UI audit |

## 8. Resolved decisions (locked)

1. Reshape track model; heterogeneous sizes first-class; TT2 first heavy citizen.
2. Cost = RAM only; TT2 = 4 plain tracks; max 2 (structural).
3. Default 8 uniform tracks until TT2 selected.
4. Fixed quads (0–3 / 4–7).
5. Donor slots = routable lanes bound to the owner; data wiped on select.
6. Outputs: internal 8/8, routed freely.
7. Scenes = Performer pattern slots; switch pattern = switch scene; modulo wrap.
8. Scene load resets + re-runs Init (real-Teletype semantics); one active runtime.
9. `scriptsPerScene` / `sceneCount` parametric with compile-time budget assert.
10. Storage overlaps the quad bytes (no separate pool). Mechanism A vs B open.
