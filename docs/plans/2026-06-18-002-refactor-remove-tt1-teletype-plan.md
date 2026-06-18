---
id: refactor-remove-tt1-teletype
schema: plan
title: "refactor: remove legacy TT1 Teletype track"
type: refactor
status: completed
date: 2026-06-18
depth: deep
---

# Remove Legacy TT1 Teletype Track

> **For Claude:** REQUIRED SUB-SKILL: use superpowers:executing-plans to implement this plan unit-by-unit.

**Goal:** Delete the legacy TT1 Teletype track (the `scene_state_t` C-VM bridge implementation) now that TT2 (native reimplementation) is at functional parity. Reclaims ~2.4 KB CCMRAM (TT1's engine is the largest `TrackEngineContainer` arm), removes a whole duplicated teletype file-I/O + bridge stack, and drops the 10th `Track`/engine union arm. The vendored `teletype/*.c` library and all `TT2*` code stay — TT2 depends on them.

**Non-goal:** shrinking the model. `NoteTrack` (9468) / `TT2Track` (9520) set the per-track union floor; removing TT1's `TeletypeTrack` arm (7104) doesn't lower it. Model size is a separate union-vs-heap conversation.

---

## Problem Frame

Two parallel Teletype implementations exist:

- **TT1** — `Track::TrackMode::Teletype`, backed by `TeletypeTrack` (model) + `TeletypeTrackEngine` + `TeletypeBridge` (the vendored C-VM `scene_state_t`), with its own editor page, list model, and FileManager text I/O (`#IO`/`#S4`/`#M`/`#S1-3`/`#PATS` per-slot format with rollback staging).
- **TT2** — `Track::TrackMode::TeletypeV2`, native C++ (`TT2Track` + `TT2TrackEngine` + `TeletypeNativeOps`), now at parity (10 scripts, 64-deep delay, 8 triggers, clock-synced metro, scene save/load).

TT1 is dead weight. Measured costs (STM32 release):

- **Engine container:** `TeletypeTrackEngine` = 936 B is the **largest** of the 10 `TrackEngineContainer` arms; `_trackEngineContainers[8]` reserves max-arm × 8. Dropping it lowers the arm to `NoteTrackEngine` (640) → **~2.4 KB CCMRAM reclaimed** (the now-tight pool, ~2.5 KB free).
- **Model union:** `TeletypeTrack` arm = 7104 B, covered by the larger `TT2Track`/`NoteTrack` → 0 marginal model RAM, but it's a dead union arm.
- **Duplication:** the entire TT1 file-I/O path (per-field parsers, 256 B line buffers, section writers, rollback scratch) parallels TT2's; the FileManager scene-load scratch is currently a TT1/TT2 union purely to share with TT1.

Per the project's no-real-projects rule, breaking serialized track-mode IDs is acceptable.

---

## Scope Boundaries

**In scope — delete (TT1-only):**
- `engine/TeletypeBridge.{cpp,h}`, `engine/TeletypeTrackEngine.{cpp,h}`
- `model/TeletypeTrack.{cpp,h}`
- `ui/pages/TeletypeEditPage.{cpp,h}`
- `ui/model/TeletypeTrackListModel.h`
- their entries in `src/apps/sequencer/CMakeLists.txt`

**In scope — strip TT1 branches:**
- `model/Track.h`/`.cpp` — the `Teletype` enum value, `TeletypeTrack` from the `Container<>`, all `case TrackMode::Teletype` arms
- `engine/Engine.h` (drop `TeletypeTrackEngine` from `TrackEngineContainer`) + `engine/Engine.cpp` (track-engine factory + TT1 branches)
- `model/Project.cpp`, `model/ClipBoard.cpp`
- `model/FileManager.{h,cpp}` — `FileType::TeletypeScript`/`TeletypeTrack`, the TT1 read/write + per-field parsers + section writers, and **collapse the load-scratch union back to a plain `TeletypeProgram` staging**
- UI consumers: `ui/pages/TopPage.cpp`, `TrackPage.{cpp,h}`, `LayoutPage.cpp`, `ListPage.cpp`, `OverviewPage.cpp`, `MonitorPage.cpp`, `ui/LedPainter.cpp`, `ui/controllers/launchpad/LaunchpadController.cpp`, `ui/model/CvOutputListModel.h`, `ui/model/GateOutputListModel.h`
- `python/project.cpp` enum binding

**Out of scope — do NOT touch (shared / separate effort):**
- The vendored `teletype/*.c` C library (parse, op tables, `scene_serialization`, `state.c`, …) — TT2 depends on it.
- All `TT2*` symbols, `TeletypeNativeOps.cpp`, `TeletypeScriptViewPage`, `TeletypePatternViewPage` (TT2 pages; they already guard on `TeletypeV2`).
- **Renaming TT2 → "Teletype"** (now redundant naming) — deferred follow-up, larger churn.
- **Model size reduction** (union-vs-heap) — separate effort.

### Deferred to Follow-Up Work
- Rename `TeletypeV2`/`TT2*` to plain `Teletype`/`TT*` now that disambiguation is unneeded.
- Model union-vs-heap track allocation to lower the 8×9.5 KB floor.

---

## Key Technical Decisions

- **Enum + serialization: compact, keep serialize==ordinal.** Serialization is *explicit* via `Track::trackModeSerialize()` (`Track.h:104`), and its wire IDs currently equal the enum ordinals (Note=0 … Teletype=6, Stochastic=7, PhaseFlux=8, TeletypeV2=9); the read side casts the wire byte back through the ordinal. So removing `TrackMode::Teletype` (ordinal/wire 6) without touching `trackModeSerialize` would desync — a saved Stochastic (wire 7) would deserialize to whatever now sits at ordinal 7. **Decision: compact.** Drop the `Teletype` enum value AND its `trackModeSerialize`/deserialize case, letting Stochastic/PhaseFlux/TeletypeV2 become **6/7/8** in both the enum and the wire map (invariant preserved, minimal code). This renumbers those three modes' wire IDs; old serialized projects with them misread — accepted under the no-real-projects rule. Alternative (preserve 7/8/9 with a hole at 6) was rejected: it requires an explicit wire↔enum map divorced from ordinals, more code, for zero benefit without real projects. Any test asserting wire IDs (and the seed/demo project) must be updated/re-verified after.
- **Vendored C lib stays.** Removing TT1 removes the *bridge* (`TeletypeBridge`, `scene_state_t` usage in our code), not the vendored teletype sources — TT2 compiles against them.
- **FileType: compact, no on-disk break.** Remove `FileType::TeletypeScript`/`TeletypeTrack` and compact `TeletypeV2Program` 4 → 2 (`FileDefs.h:13`), reindexing `fileTypeInfos` to `{PROJECTS, SCALES, TT2}`. Safe because TT2 scenes are NAME-prefixed `.txt` with **no persisted FileType byte** — only Project=0/UserScale=1 write binary `FileHeader`s and both keep their values. The enum value only drives `slotPath` (dir) + `slotInfo` dispatch, which the reindex keeps mapping `TeletypeV2Program → "TT2"`. Preserving 4 with placeholder table holes was rejected (dead clutter, no benefit).
- **FileManager scratch union collapses.** The `TeletypeLoadScratch` union (added to share TT1 + TT2 load buffers) loses its `tt1` member; it becomes a plain `CCMRAM_BSS TeletypeProgram` staging. The TT1 read path and its alias references (`ttActiveClip`, etc.) are deleted with it.

---

## Implementation Units

> **Compile-ordering note:** `TrackMode::Teletype`, the `Container<>`/`TrackEngineContainer` arms, the deleted classes, and the TT1 unit tests are mutually referencing. Strip the *consumers* (UI cases, engine branches/APIs, FileManager methods) and the *TT1 tests* while the enum/classes still exist (dead-case deletion + test removal compile fine), then remove the enum value + Container arms + class files + CMake source entries together in the structural unit. Every unit below — including the per-unit deletions and their CMake entries — must leave **both trees and the unit-test target** building.

### U1. Strip TT1 from UI consumers

**Goal:** Remove every `TrackMode::Teletype` / `TeletypeTrack` / `TeletypeTrackEngine` reference from the UI layer, and delete the TT1-only page + list model **together with their CMake entries** (so the build never references a deleted source).

**Dependencies:** none.

**Files:**
- Delete: `ui/pages/TeletypeEditPage.{cpp,h}`, `ui/model/TeletypeTrackListModel.h`
- Modify: `src/apps/sequencer/CMakeLists.txt` — **remove `ui/pages/TeletypeEditPage.cpp` (line ~163) and `ui/model/TeletypeTrackListModel.h` (line ~175) in this unit**, since the files are deleted here
- Modify: `ui/pages/TopPage.cpp` (the 3 `case TrackMode::Teletype` arms **and** the `page+track-6 → _engine.panicTeletype()` "TT PANIC" gesture at `:143` — delete it; TT2 uses per-track `KILL`, there's no global TT2 panic), `ui/pages/TrackPage.{cpp,h}` (4 touchpoints), `ui/pages/LayoutPage.cpp`, `ui/pages/ListPage.cpp`, `ui/pages/OverviewPage.cpp`, `ui/pages/MonitorPage.cpp`, `ui/LedPainter.cpp`, `ui/controllers/launchpad/LaunchpadController.cpp` (~13 cases), `ui/model/CvOutputListModel.h`, `ui/model/GateOutputListModel.h`, `ui/pages/Pages.h` (drop `teletypeEdit` member + include)

**Approach:** Delete the `case TrackMode::Teletype:` arms (TT1's routing into `pages.track`/`teletypeScriptView` is dead — TT2 keeps its own `TeletypeV2` cases). Remove the TT1-specific output-count branches in `Cv/GateOutputListModel.h` (the `TeletypeTrack::TriggerOutputCount`/`CvOutputCount` guards). Drop the `TeletypeEditPage` instance from `Pages`. Delete the `panicTeletype` UI gesture.

**Patterns to follow:** the existing per-mode `switch` structure; how a non-existent mode is simply absent.

**Test scenarios:** Test expectation: none — pure removal; covered by the U5 full-suite + both-trees build + sim audition. Verify no remaining `TeletypeEditPage`/`TeletypeTrackListModel`/`TrackMode::Teletype`/`panicTeletype` references in the UI tree.

**Verification:** UI tree has zero TT1 references; sim builds (enum/classes still present elsewhere, so it links).

---

### U2. Remove TT1 unit tests

**Goal:** Stop the unit-test target compiling TT1 symbols before U3 deletes the FileManager TT1 APIs and U4 deletes the classes. `TestTeletypeTrackNoText` calls the TT1 FileManager APIs (`TestTeletypeTrackNoText.cpp:150`) that U3 removes, so test cleanup must come first.

**Dependencies:** none. **U3 and U4 depend on this** (their deletions break the test target otherwise).

**Files:**
- Delete: `src/tests/unit/sequencer/TestTeletypeTrackNoText.cpp` (directly compiles `TeletypeTrack`, `TrackMode::Teletype`, and the TT1 FileManager text APIs)
- Modify: `src/tests/unit/sequencer/TestPhaseFluxSize.cpp` — remove `#include "apps/sequencer/engine/TeletypeTrackEngine.h"` (`:10`) and any `TeletypeTrackEngine` size print/assert
- Modify: `src/tests/unit/sequencer/CMakeLists.txt` — drop `register_sequencer_test(TestTeletypeTrackNoText …)` (`:109`); keep `TestPhaseFluxSize` (`:49`)
- Audit: any other unit test asserting track-mode **wire IDs** — update to the compacted Stochastic/PhaseFlux/TeletypeV2 = 6/7/8

**Approach:** `TestTeletypeTrackNoText` is a TT1-only characterization test — delete it. `TestPhaseFluxSize` only *includes* the TT1 engine for a size comparison; drop that include + line. Grep the test tree for hard-coded `trackModeSerialize` expectations and fix to the compacted IDs.

**Test scenarios:** Test expectation: none — test removal/repair; the test target building + passing at U5 is the proof.

**Verification:** unit-test target has zero `TeletypeTrack`/`TeletypeTrackEngine`/`TrackMode::Teletype` references; `cmake -S . -B build` reconfigures clean and the suite still builds (classes still present, so non-TT1 tests link).

---

### U3. Strip TT1 from engine + FileManager

**Goal:** Remove TT1 from the engine (factory, branches, **and the public TT1 metro/panic APIs**) and the FileManager file I/O, collapsing the load-scratch union to a plain TT2 staging.

**Dependencies:** U1 (the `panicTeletype` UI caller must be gone first), U2 (`TestTeletypeTrackNoText` calls the FileManager TT1 APIs deleted here — it must be gone first).

**Files:**
- Modify: `engine/Engine.h` — delete the public TT1 controls `panicTeletype()`, `setTeletypeMetroAll(int16_t)`, `setTeletypeMetroActiveAll(bool)`, `resetTeletypeMetroAll()` (`:182`); `engine/Engine.cpp` — their implementations (`:449`), the track-engine factory `case Teletype` (`Container::create<TeletypeTrackEngine>`), and the ~5 `trackMode() == Teletype` branches at the CV/gate output points (`:451-475`, `:638`)
- Modify: `model/FileManager.h`/`.cpp` — remove `writeTeletypeScript`/`readTeletypeScript`/`writeTeletypeTrack`/`readTeletypeTrack` (slot + path), the TT1 section writers (`writeSlotIo`/`writeScriptSection*`/`writePatterns`), per-field parsers (`parseTriggerInputSource`/`parseCvInputSource`/`parseTriggerOutputDest`/`parseCvOutputDest`/`parseTimeBase`/`quantizeScaleName`/…), the `readTeletypeTrack` body, the `slotInfo` TT1 branches, and the `fileTypeInfos` TELS/TELT entries; collapse `TeletypeLoadScratch` union → plain `CCMRAM_BSS TeletypeProgram gTeletypeLoadScratch;` and update `readTt2Program` to placement-new into it directly (no `.tt2`/`.tt1` members, no alias references)
- Modify: `model/FileDefs.h` — remove `FileType::TeletypeScript`/`TeletypeTrack`; **compact `TeletypeV2Program` 4 → 2** (Project=0, UserScale=1, TeletypeV2Program=2, Settings=255)
- Modify: `model/Project.cpp` (TT1 branch), `model/ClipBoard.cpp` (2 cases)

**Approach:** Engine factory: delete the `Teletype` construction case, leave `TeletypeV2`. The 4 public `*Teletype*` APIs are deleted outright (no TT2 global-all equivalent is wired; TT2 metro is per-track via `M`/`M.ACT`). FileManager: only the TT1 `TeletypeScript`/`TeletypeTrack` types + helpers go; the TT2 `TeletypeV2Program` path stays. **FileType compaction is safe on disk** — TT2 scenes are NAME-prefixed `.txt` with no persisted `FileHeader`/FileType byte (only Project=0/UserScale=1 write binary headers, both unchanged), so `TeletypeV2Program`'s value only drives `slotPath` (the "TT2" directory) and `slotInfo` dispatch; reindex `fileTypeInfos` to the 3 surviving entries `{PROJECTS, SCALES, TT2}` so `fileTypeInfos[int(TeletypeV2Program)=2]` still yields `TT2`. Union collapse mirrors the pre-union shape — a single `TeletypeProgram` staging in CCMRAM.

**Patterns to follow:** the TT2 `writeTt2Program`/`readTt2Program` post-collapse shape; `slotPath`/`fileTypeInfos` indexing (`FileManager.cpp:43`); the engine factory switch.

**Test scenarios:**
- `TestTT2SceneSerializer` still green — proves the union collapse didn't regress TT2 load.
- Test expectation otherwise: none (removal); covered by U5. The `slotPath`/`slotInfo` reindex is verified by the U5 sim scene save→load (file lands in/loads from `TT2/`).

**Verification:** engine + FileManager have zero TT1 references; no `*Teletype*` public API on `Engine`; `TeletypeV2Program == 2` and `fileTypeInfos[2]` is the TT2 entry; `TestTT2SceneSerializer` passes; sim builds + saves/loads a TT2 scene.

---

### U4. Remove the enum value, union arms, classes, and compact serialization

**Goal:** The structural removal — delete `TrackMode::Teletype`, **compact** the serialize/deserialize map, drop `TeletypeTrack`/`TeletypeTrackEngine` from the two `Container<>`s, delete the class files, prune CMake sources, drop the python binding.

**Dependencies:** U1, U2, U3 (all consumers, engine APIs, and TT1 tests stripped first).

**Files:**
- Modify: `model/Track.h` — remove the `Teletype` enum value, `TeletypeTrack` from the `Container<>` (`:405`), the `teletypeTrack()` accessor, all `case TrackMode::Teletype` (name/shortName/char/`trackModeSerialize`/sanitize/clear switches); **compact `trackModeSerialize` so Stochastic/PhaseFlux/TeletypeV2 return 6/7/8** (matching their new ordinals — the serialize==ordinal invariant); `model/Track.cpp` (the ~7 cases)
- Modify: `engine/Engine.h` — remove `TeletypeTrackEngine` from `TrackEngineContainer` (`:47`)
- Delete: `engine/TeletypeBridge.{cpp,h}`, `engine/TeletypeTrackEngine.{cpp,h}`, `model/TeletypeTrack.{cpp,h}`
- Modify: `src/apps/sequencer/CMakeLists.txt` — drop `engine/TeletypeBridge.cpp`, `engine/TeletypeTrackEngine.cpp`, `model/TeletypeTrack.cpp` (the UI sources were removed in U1)
- Modify: `src/apps/sequencer/python/project.cpp` — drop the `.value("Teletype", …)` enum binding (`:267`)
- Modify: any `sizeof(Track)` / `sizeof(TrackEngineContainer)` exact `static_assert` that the arm removal shifts (re-measure)

**Approach:** With all consumers/tests stripped, removing the enum value, the `Container<>` arms, and compacting `trackModeSerialize` compiles. `Container::as<T>()` is type-based, so dropping an arm only changes the union size, not call sites. Re-measure and update exact-size asserts.

**Patterns to follow:** the `Container<>` arm lists (`Track.h:405`, `Engine.h:47`); the explicit `trackModeSerialize` switch (`Track.h:104`).

**Test scenarios:**
- A model serialize/round-trip test (or new CASE) confirming Stochastic/PhaseFlux/TeletypeV2 serialize to **6/7/8** and round-trip.
- Otherwise none — structural; proof is both trees + full suite at U5.

**Verification:** the live-symbol gate (see Verification section) returns nothing — no `TrackMode::Teletype`, TT1-header includes, `class TeletypeTrack`/`TeletypeTrackEngine`, or `TeletypeBridge::` in `src/` (incidental comment mentions in surviving TT2/shared files are allowed); both trees build.

---

### U5. Verify, re-measure, demo

**Goal:** Confirm the removal is clean end-to-end and quantify the RAM reclaim.

**Dependencies:** U1, U2, U3, U4.

**Files:**
- Possibly modify: the seed/demo project setup (where the default project is seeded) if the wire-ID compaction changed which mode a seeded track resolves to.

**Approach:** Reconfigure + build all unit tests; run the full suite; build sim and STM32 release. Re-measure `arm-none-eabi-size -A`: expect `.ccmram_bss` down ~2.4 KB (engine container arm 936→640 × 8), `.bss` roughly flat (model floor unchanged). Audition the sim: seeded TT2 demo track loads/runs; a TT2 scene save/load round-trips.

**Test scenarios:**
- Full suite green (`TeletypeV2|TT2|Geode|Modulator|Routing` + the rest), including the U4 serialize-ID CASE.
- Both trees build, STM32 links with no region overflow.
- Sim: seed project loads, TT2 track edits/runs, scene save→load round-trips.
- RAM: CCMRAM dropped ~2.4 KB vs. pre-removal baseline.

**Verification:** suite 100%, both trees clean, CCMRAM reclaim confirmed by measurement, sim demo healthy.

---

## Verification

- **Live-symbol gate** (not a blanket text match — TT2/shared files legitimately mention `TeletypeTrack` in comments/`static_assert` strings, e.g. `TT2TrackEngine.h:217`, `TeletypeProgram.h:58`, and those stay): the following returns nothing across `src/` (incl. `src/tests/`):
  ```
  rg -n 'TrackMode::Teletype\b|#include "(TeletypeTrack|TeletypeTrackEngine|TeletypeBridge)|\bclass TeletypeTrack\b|\bclass TeletypeTrackEngine\b|\bTeletypeBridge::|panicTeletype|setTeletypeMetro' src/
  ```
  Incidental comment/doc references to "TeletypeTrack" in surviving TT2/shared files are out of scope (optional tidy, not required).
- Full `TestTeletypeV2*` + `TestRouting*` + model suites green; `TestTT2SceneSerializer` proves the FileManager union collapse is safe; a serialize CASE confirms Stochastic/PhaseFlux/TeletypeV2 wire IDs are 6/7/8.
- Sim + STM32 release **and the unit-test target** all build; STM32 links with no region overflow.
- `arm-none-eabi-size -A`: `.ccmram_bss` ≈ −2.4 KB; `.data+.bss` ≈ flat.
- Sim audition: seed project + a TT2 scene save/load both healthy.

## Risks

- **Wire-ID compaction** — `trackModeSerialize` IDs for Stochastic/PhaseFlux/TeletypeV2 move 7/8/9 → 6/7/8; old serialized projects with those modes misread. Accepted (no real projects). Mitigation: the U4 serialize CASE pins the new IDs; re-verify the seed/demo project after U5 and fix the seed if a track resolves wrong.
- **Stale CMake / test references** — deleting a source (or an API) while CMake or a unit test still references it breaks the build. Mitigation: each unit removes the CMake entries for the files **it** deletes (U1 for UI, U4 for engine/model); **U2 removes the TT1 unit tests before U3 deletes the FileManager TT1 APIs and U4 deletes the classes.**
- **FileManager union collapse regressing TT2 load** — collapsing the shared scratch must keep TT2 staging + atomic swap intact. Mitigation: `TestTT2SceneSerializer` + a sim scene load.
- **Hidden TT1 coupling** — a TT2 or shared file unexpectedly depending on a TT1 symbol (e.g., the now-deleted `Engine` `*Teletype*` APIs). Mitigation: the grep gate; both-trees + test-target build at every unit.
- **Size-assert drift** — dropping the union arm changes `sizeof(Track)`/`TrackEngineContainer`; exact `static_assert`s may trip. Mitigation: re-measure and update in U4.

## Rollback

Each unit is a commit; revert in reverse. U4 (structural) is the point of no easy return — if a hidden coupling surfaces there, revert U4 and re-strip the missed consumer/test before retrying. U1/U2/U3 are independently safe (dead-branch deletion + test removal, classes still present).
