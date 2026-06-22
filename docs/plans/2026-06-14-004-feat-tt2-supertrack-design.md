# TT2 Supertrack — Design (goals resolved, storage open)

Status: design. Goal tree resolved via grill session; the storage mechanism is
the one open decision (see §6). Capability constants are parametric (§4).

## 1. Goal

The capability driving this is a **TT2 scene bank** — 8–16 whole Teletype
programs switched live by the pattern gesture (§2). That needs ~30 KB, far past
a single track slot (current model gate: 10,120 B, §5), so a TT2 instance claims
the RAM of **4 plain tracks** ("eats a quad"); with 8 tracks that caps TT2 at **2 instances**
(structural, not a separate rule).

The scene bank — not script count — is what forces the quad. Scripts-per-scene
alone would fit in place: TT2 uses about 45% of its slot today (~5 KB free, §5). The
reshape does give track types heterogeneous RAM sizes, but that generality is the
*mechanism*, not the goal — the goal is the scene bank.

Cost unit is **RAM only**. CPU and output-lane count are not the budget metric.
The point is to **reuse** RAM the union already reserves, not add new RAM (§6).

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

Open tension to pin before §7.6 wiring: the pattern gesture is *seamless* for
Note/Curve tracks — they don't reset on a pattern nudge. A full TT2 reset on
every pattern switch would hard-reset all TT2 mid-performance. Reconcile: fire
the reset only when the resolved scene index actually changes (nudges within the
same scene index don't reset), or accept the wipe as intended.

## 3. Placement, lanes, outputs

- **Placement — fixed quads.** TT2 selectable only on track 0 or track 4; each
  eats its quad (0–3 / 4–7). "Max 2" is automatic and trivially enforced.
- **Donor slots stay as routable lanes.** Tracks 1–3 (and 5–7) are not
  independent tracks; they appear as routable output destinations that bind to
  the TT2 owner — a donor lane resolves to the anchor engine (passthrough,
  §7.2), not silence. Selecting TT2 on the anchor wipes the donor slots' prior
  data (dev: files break freely, no migration).
- **Outputs — internal 8/8, routed freely.** TT2 keeps its 8 CV / 8 TR internal
  outputs (`TeletypeOutputState.h`) and routes any to any physical output via
  the existing routing UI, not hard-tied to the eaten lanes.

## 4. Capability budget (measured, parametric)

Two constants, `scriptsPerScene` and `sceneCount`, with a compile-time budget
assert. Dial later without redesign.

Measured arena (model union, slots of one quad):

- Quad arena = 4 × 10,120 = **40,480 B**. 10,120 is the current measured
  `Track`/model slot gate from `PROJECT.md` (2026-06-13): `Project._tracks =
  80,960 B = 8 × 10,120`, with `CurveTrack ≈ 10,092 B` setting the model gate.
  Re-measure on STM32 before locking constants.
- Per-scene = one `TeletypeProgram`, program-only, **no scene-text field**
  (`TeletypeProgram.h`). 6 scripts = ≤2384 B. Each added script = **+304 B**
  (`TT2Script`). The 4×64 tracker is already inside that 2384.
- Active runtime = `TT2Runtime` ≤2160 B, **one copy** (scene-load resets it).
- `CONFIG_SNAPSHOT_COUNT = 1`, negligible.
- Usable for scenes ≈ 40,480 − 2,160 ≈ **38,320 B** before quad tag/metadata.

Curve (bounds are `<=` ARM figures; true sizes run slightly under, each count can
nudge up ~1):

| scripts/scene | bytes/scene | scenes |
|---|---|---|
| 10 (full Teletype) | ~3600 | ~10 |
| 8 | ~2992 | ~12 |
| 6 (current) | ~2384 | ~15–16 (≈1:1 with 16 pattern slots, no wrap) |

The 10×10 top row is plausible with the current measured gate, but it is still
not a promise: quad tag/metadata, alignment, future `TeletypeProgram` growth, and
the exact ARM layout decide the final count. Treat **10 scripts × 10 scenes** as
the target, not the contract; the STM32 `sizeof` probe and compile-time budget
assert are what actually pin it.

Engine side is not part of the arena: `TT2TrackEngine` (≤944 B) stays in the
anchor's engine container; donor engine slots are free bonus.

## 5. Current sizes (baseline)

- `TT2Track` = 4552 B (`TeletypeProgram` ≤2384 + `TT2Runtime` ≤2160 + index),
  using about 45% of one current 10,120 B model slot today (`TT2Track.h`).
- `TeletypeProgram`: 6 scripts (4 trigger + Metro + Init), 4 patterns, I/O
  mappings (`TeletypeProgram.h`).
- 8-track model union currently measures as `Project._tracks = 80,960 B =
  8 × 10,120`, always, regardless of modes (`Project.h:35`,
  `std::array<Track, CONFIG_TRACK_COUNT>`; `PROJECT.md`, 2026-06-13).

## 6. OPEN — storage mechanism

The RAM-reuse constraint forces a real choice. A TT2 arena must **physically
overlap** the quad's bytes to count as "eating 4 tracks" rather than adding
memory.

**Doubling pitfall (rejected):** a separate `std::array<TT2SuperTrack,2>` pool
beside the unchanged `std::array<Track,8>` is **new** RAM. ~80 KB union + 2×~40 KB
pool ≈ **160 KB — doubles the footprint.** Violates the reuse goal. A separate
pool can never reuse the union bytes; separate memory = added memory.

Reuse ⇒ overlap ⇒ a union. The open decision is *where* the overlap lives:

- **Option A — quad-level discriminated union.** `TrackArray` becomes 2 quads;
  each quad is `Container<4×Track | TT2SuperTrack>` over the same ~40 KB.
  Total ≈ 2 × max(4×10,120, TT2SuperTrack) ≈ **80 KB, identical to today's
  measured `Project._tracks` footprint if `TT2SuperTrack` stays below the quad
  gate.** Uses the
  same placement-new + tag pattern as the per-track `Container` (`Track.h:406`),
  lifted to quad scope. No raw `reinterpret_cast`. Cost: track indexing becomes
  `i → (quad i/4, sub i%4)`; donor-slot guards where a quad is in TT2 mode.

- **Option B — literal slot-spanning reinterpret.** TT2 at the anchor
  reinterprets the contiguous `Container`s of its 4 slots as one ~40 KB buffer.
  Also reuses RAM, but relies on union layout/contiguity assumptions and raw
  type-punning, with ownership guards scattered across every track loop.

Recommendation leaning A (type-safe, mirrors existing pattern), pending the
impact pass below.

`TT2SuperTrack` doesn't exist yet. Before committing the full §7 work, validate A
in isolation: build the bare `Quad = Container<4×Track | TT2SuperTrack>`, prove
construct/destruct/serialize round-trip and donor safety. Option B is the named
fallback if A proves unsound.

## 7. Impact surface — Option A (quad model), detailed

### 7.0 The decision that shrinks the work

`CONFIG_TRACK_COUNT` stays **8 = logical track count**. Every per-logical-track
array stays `[8]`: `PlayState` track state, `Routing` bias/override arrays,
`Engine` output-index arrays, `Clipboard`. They are NOT resized.

Only **two** things change shape:

1. **Model storage** becomes the quad-union (this is the entire RAM-reuse win).
2. **Engine stays flat-8** — `_trackEngines[8]`. Donor slots are output-address
   aliases for the anchor, not independent engines. They may point at the anchor
   for read paths, but owner-only lifecycle/tick paths must skip donors so the
   same TT2 engine is not ticked/restarted/change-patterned four times.
   Engines are small (944 B); no quad-union on the engine side.

Two helpers carry most of the rest: `anchorOf(i) = (i/4)*4` and `isDonor(i)`
(quad is TT2 and `i%4 != 0`).

So the work is: one real reshape (model storage) + owner-only engine dispatch
guards + output-address donor mapping + a bounded list of donor reject/redirect
(cross-track index refs) + serialization rewrite + UI masking.

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
- **Clipboard** (`ClipBoard.h`): copy/paste-track runs through a `Container` sized
  to one 10,120 B Track slot — it cannot hold the ~40 KB quad arena. Policy: disable
  copy/paste-track on a TT2 anchor and its donors, or deep-copy the active program
  only. Decide before touching the copy-track gesture.

### 7.2 Engine — donors are output aliases, not extra ticks

`updateTrackSetups()` (`Engine.cpp:610-654`) becomes quad-aware: a TT2 quad
creates ONE engine at the anchor. Donor `_trackEngines[i]` may be set to the same
anchor pointer to keep read paths non-null, but **that pointer alias is not a
license to dispatch lifecycle work through donors**.

Owner-only dispatch sites must call the anchor once and skip donors:

- `Engine::update()` tick loop (`Engine.cpp:183-190`) — donor aliases must not
  call `TT2TrackEngine::tick()` again.
- Transport/song lifecycle loops (`restart()`, `reset()`, `changePattern()`,
  e.g. `Engine.cpp:1049-1065`) — donor aliases must not restart/reset/reload the
  same TT2 engine multiple times.
- Consistency checks compare donor model state against donor alias semantics, not
  a separate donor engine mode.

Read paths may resolve a donor index to the anchor engine: output retrieval,
cross-track reads (`NoteTrackEngine` divisor-Y/harmony, `TeletypeTrackEngine`
read*), and `selectedTrackEngine()` stay non-null if donor selection is redirected
before type-specific UI code runs.

Output semantics are the other real engine-side task: when output lane *i*
belongs to a donor, the slot mapping must address the anchor's matching internal
TT2 output, not repeatedly read slot 0. This applies to both CV and gates:

- CV slot precompute (`Engine.cpp:708-716`) must map donor logical tracks to
  anchor CV slots 1..7 as intended.
- Gate output reads (`Engine.cpp:731-734`) must map donor logical tracks to
  anchor TR slots 1..7 as intended.
- Rotation masks from routing (`RoutingEngine` gate/CV rotate masks) need the
  same donor-to-anchor-slot normalization or rotation will target the wrong TT2
  output lane.

### 7.3 Cross-track index references — donor redirect (correctness, not safety)

Anchor-aliasing (§7.2) removes the crash risk, so these are about *correctness*:
a stored "track N" index naming a donor now reads the anchor, which may not be
intended. Redirect at the setter (or accept anchor semantics):

- `NoteSequence::setMasterTrackIndex` (`NoteSequence.h:718`), `setDivisorYTrack`
  (`NoteSequence.h:515`) — harmony master and divisor-Y links.
- `Project::setCvOutputTrack` (`Project.h:359`), `setGateOutputTrack`
  (`Project.h:371`) — output routing destination validity.
- `Routing` per-track target bitmask `Route::_tracks` (`Routing.h:736-758`,
  `isRouted` `Routing.cpp:242-248`) — donor bit maps to anchor for ownership,
  but output-lane routing must still preserve the donor lane number where it is
  selecting one of TT2's 8 internal CV/TR outputs.
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

- Track list / `TrackModeListModel`: a TT2 quad shows one track at the anchor.
  Donors are hidden from the track *list* but still appear as routing
  *destinations* under the anchor — resolving the §3 "routable lanes" vs "hidden"
  tension.
- Mode select: TT2 (`TrackMode::TeletypeV2`) selectable only on anchors 0/4;
  setting it on a donor redirects to the anchor.
- Per-track UI loops (track grid, performer LEDs, routing track-select) skip
  donors. Specific page file:line need verification before implementation —
  treat the UI list as behavioral, not yet line-cited.
- Track-type page safety is a hard gate: any selected-track redirect away from a
  donor must happen before type-specific page callbacks can call
  `selectedNoteSequence()`, `selectedCurveSequence()`, `selectedTrackEngine().as<T>()`,
  or direct `Track::*Track()` accessors. `TopPage` track-select remapping and any
  TT2 editor pages need the same stale-page protection already required by
  `PROJECT.md`.

### 7.6 Pattern-switch wiring

Global pattern index → TT2 scene index with modulo wrap (existing
`TeletypeTrack` precedent). Donor lanes inherit the anchor's pattern/scene.

### 7.7 Effort shape

| Part | Nature | Size |
|---|---|---|
| Model quad-union storage + `track(i)` resolver | real reshape | the hard part |
| `tracks()` facade / caller fix | API | small, isolated |
| Donor engines alias anchor | engine semantics | one setup change plus owner-only dispatch skips |
| TT2 donor output lanes | output semantics | gate + CV slot mapping, including rotation masks |
| Cross-track donor redirect | correctness | ~6 setters, listed |
| Clipboard copy-track policy | decision | disable on TT2, or deep-copy program |
| Serialization quad-union | rewrite, no migration | moderate |
| UI anchor/donor masking | display + safety logic | moderate, needs UI audit and stale-page guard |

## 8. Resolved decisions (locked)

1. Capability = TT2 scene bank (8–16 programs, needs >1 slot → quad). The quad
   reshape is the mechanism; heterogeneous track sizes fall out of it, not the goal.
2. Cost = RAM only; TT2 = 4 plain tracks; max 2 (structural).
3. Default 8 uniform tracks until TT2 selected.
4. Fixed quads (0–3 / 4–7).
5. Donor slots = routable lanes bound to the owner; donor engine references may
   alias the anchor for read/output paths, but lifecycle/tick dispatch is
   owner-only; data wiped on select.
6. Outputs: internal 8/8, routed freely.
7. Scenes = Performer pattern slots; switch pattern = switch scene; modulo wrap.
8. Scene load resets + re-runs Init (real-Teletype semantics); one active runtime.
   Open: scope the reset to actual scene-index change vs every pattern nudge (§2).
9. `scriptsPerScene` / `sceneCount` parametric with compile-time budget assert.
10. Storage overlaps the quad bytes (no separate pool). Mechanism A vs B open.
