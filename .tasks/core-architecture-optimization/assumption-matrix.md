# Original Performer Assumption/Mismatch Map

Built from current source. Classification: **still valid**, **stressed but acceptable**, or **false / causing mismatch**.

---

## Part 1 — Storage / Runtime Assumptions

| # | Assumption | Source Evidence | Classification |
|---|---|---|---|
| 1 | Model Container can hold largest track type | `Track.h:273` — `Container<NoteTrack, CurveTrack, ..., TeletypeTrack>` | **Stressed but acceptable** (Note/Curve genuinely need the max; smaller families pay tax) |
| 2 | Engine Container can hold largest engine type | `Engine.h:41` — `Container<..., TeletypeTrackEngine>` | **Stressed but acceptable** (only false if product caps Teletype count below 8) |
| 3 | Every track stores 17 full pattern objects | `NoteTrack.h:15` — `NoteSequenceArray` (17 sequences) | **False / mismatch** |
| 4 | Routing is lightweight sidecar | `RoutingEngine.h:30-65` — `RouteState` with `TrackState[8]` per route, 5 stateful shapers | **False / mismatch** |
| 5 | Track engines own CV/gate output directly | `Engine.cpp:546-645` — `updateTrackOutputs()` with 7+ output paths | **Stressed but acceptable** |
| 6 | Edit state is close to playback state | Note/Curve: direct sequence edit. Teletype: script text → parse → VM state → PatternSlot | **Stressed (Note/Curve) / False (Teletype)** |
| 7 | Track type is UI identity | `Track.cpp`, `Engine.cpp`, `OverviewPage.cpp`, `LaunchpadController.cpp` — ~15-20 switch/case sites | **Stressed but acceptable** |
| 8 | Runtime state is disposable | `RoutingEngine::_routeStates` envelope/freq/activity/progressive state; Teletype delay/Geode phases | **False / mismatch** |
| 9 | Pattern = X0X sequence grid | Note/Curve: grid. Teletype: 2 swap-buffer PatternSlots. Tuesday/Indexed: generators | **False / mismatch** |
| 10 | Routing target model is simple global passthrough | `Routing.h:115-160` — 54 target enums, `isPerTrackTarget()`, per-track `biasPct[8]`, `shaper[8]` | **False / mismatch** |

---

## Part 2 — Musical / Workflow Assumptions

| Assumption | Original Meaning | XFORMER Stress Point | Verdict |
|---|---|---|---|
| Pattern = X0X sequence grid | Grid of steps, gates, notes, curves | Teletype = VM script slots; Tuesday/Indexed = generator states | **False** |
| Track = self-contained sequencer lane | Owns pattern data, produces CV/gate | Teletype is VM inside track; Routing/bus CV/mutes affect outputs outside lane | **Stressed** |
| Edit state close to playback | Editing changes data engine reads | Teletype: script text → parsed → VM state → PatternSlot mirrors → file buffers | **False** |
| Output intent is track-owned | Track engines produce CV/gate | RoutingEngine, bus CV, CV routes, rotation, mutes all participate | **Stressed** |
| Track type is UI identity | UI branches on Note/Curve mode | 7 track types, capability matrix would be better | **Stressed** |
| Runtime state is disposable | Can reset/rebuild cheaply | Routing shapers, Teletype delays, Geode, LFO phases have time memory | **False** |

---

## Part 3 — Mismatch Breakdown

For each mismatch, separated into: **architecture problem**, **user-visible quirk**, **future feature friction**, **RAM/flash side benefit**, **verification needed**.

| # | Assumption | Architecture Problem | User-Visible Quirk | Future Feature Friction | RAM/Flash Side Benefit | Verification Needed |
|---|---|---|---|---|---|---|
| 1 | Model Container max-size | NoteTrack (~9,500 B) tax paid by all 7 track types | None — placement-new, unused bytes untouched | Adding track type inflates all slots or forces tax | Container footprint baseline; recoverable amount unknown/high-risk (requires redesigning copy/clear/snapshot and serialization) | Does lazy/pooled storage break value-copy semantics for copy/clear/snapshot? |
| 2 | Engine Container max-size | TeletypeTrackEngine (~904 B) inflates all 8 slots | None | Capping Teletype count or separate pool needed | ~1,832 B direct gap CCMRAM — only recoverable if product caps simultaneous Teletype tracks | Do users actually use >2 Teletype tracks simultaneously? |
| 3 | Pattern storage semantics | One copy/paste/clear model doesn't fit all | Teletype swap-buffer rollback vs Note/Curve value-copy differ | Adding new pattern family requires ClipBoard branching | Low — Teletype triple-store is designed | Is `CONFIG_SNAPSHOT_COUNT=1` ever likely to change? |
| 4 | Routing as sidecar | RoutingEngine coded as passthrough but behaves as modulation engine | Shaper state resets on project load/route change (audible?) | Each new shaper that expands shared TrackState fields adds unconditional CCMRAM | ~7.4 KB baseline; recoverable with conditional state layout | Do envelope/progressive dividers restart from zero or resume? Is reset-on-load acceptable? |
| 5 | Output ownership | 7+ distinct output paths, no single authority | Teletype bypasses routing — is this intentional for latency? | Future output features risk conflicting writes | None — clarity issue | Is Teletype CV excluded from "CV Output Rotate" intentionally? |
| 6 | Runtime state disposable | No formal lifecycle for time-accumulated state | Shaper state, Teletype delays reset unexpectedly? | Adding new time-memory feature requires lifecycle design | May reveal transactional buffers to eliminate | Verify reset behavior on pattern switch vs project load |
| 7 | Edit/play state separation | Teletype has 4-layer state (text → parsed → VM → PatternSlot → file) | Editing and playback are separated by parse/commit/sync | Pattern slot switching rollback semantics are load-bearing | Teletype backup consolidation: ~1,226 B | Can `ttSlot1Backup`/`ttSlot2Backup` be consolidated without breaking file parse failure atomicity? |
| 8 | Track type UI identity | ~15-20 files switch on TrackMode | None | Each new track type touches ~15-20 files | None — maintenance cost | Is capability-matrix UI dispatch worth it for 7 types? |

---

## Source-Grounded Findings

### Strongest Architectural Mismatches

1. **Routing evolved into a modulation engine** (~7.4 KB CCMRAM with mostly-zero state). The `RouteState[16]` array carries `TrackState[8]` per route, but only per-track targets using stateful shapers (Location, Envelope, FrequencyFollower, Activity, ProgressiveDivider) need the state. Global targets (Tempo, Swing, etc.) never need `TrackState`.

2. **Model/Engine containers pay max-size tax**. `Container<...>` uses `maxsizeof` to allocate the union. `NoteTrack` (~9,500 B) dominates the model container; `TeletypeTrackEngine` (~904 B) dominates the engine container. Both are genuine requirements for their respective types, but all other types pay the tax unconditionally.

3. **Teletype state ownership is scattered and load-bearing**. `scene_state_t` (VM runtime) → `PatternSlot` (swap/rollback) → `ttSlot` globals (file I/O) creates designed redundancy. The backup buffers (`ttSlot1Backup`, `ttSlot2Backup`) are ~2,452 B total and may be consolidatable.

4. **Pattern semantics diverged**. Note/Curve have 17 full `NoteSequence`/`CurveSequence` objects (value-copy semantics). Teletype has 2 `PatternSlot` swap buffers (rollback semantics). Tuesday/Indexed have parameter-only `TuesdaySequence`/`IndexedSequence` objects (generator state). The UI assumes uniformity but storage semantics differ.

### What Is Still Valid

- **Note/Curve model genuinely needs 17 sequences** — this is not waste.
- **Container placement-new approach** is safe; unused bytes are never touched.
- **Teletype dual-path output bypass** may be intentional for latency — verify before changing.
- **Track type count (7)** is still manageable with switch/case; capability matrix only worth it if types grow beyond ~8-10.

---

## Where Findings Go

This section maps which findings feed which downstream documents. It is **not an execution plan** — it is routing information for when those documents are worked on.

- Findings 1 and 2 (container max-size) feed `ram-recovery-experiments.md` **only if** product caps simultaneous Teletype tracks or a model redesign is undertaken. Otherwise they remain stressed-but-acceptable.
- Finding 3 (Teletype scattered state) feeds `ram-recovery-experiments.md` for ttSlot backup consolidation verification, and architecture research for pattern storage semantics redesign.
- Finding 4 (Routing as modulation) feeds both `ram-recovery-experiments.md` (conditional TrackState layout) and architecture research (explicit state lifecycle design).
- Finding 5 (output ownership) feeds documentation/architecture clarity only — no RAM recovery.
- Finding 8 (track type UI identity) feeds UI capability research only if track type count grows beyond ~8-10.
