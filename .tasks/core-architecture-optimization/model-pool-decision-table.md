# Model Pool Architecture Decision Table

Consolidated, source-probe-corrected decision table for replacing `Track::_container` and `Engine::_trackEngineContainers` with static typed pools or a fixed arena. Reconciles claims from `assumption-matrix.md`, `architecture-research-directions.md`, `state-lifecycle-contract.md`, `architecture-change-decision-gates.md`, `ram-recovery-experiments.md`, and `source-probe-investigation.md`.

Not an implementation plan — a go/no-go decision artifact.

---

## 0. The Narrow Question

> Can we replace `Track::_container` with static typed pools or a fixed arena while preserving deterministic playback, stable engine references, and no allocation in the realtime path?

**Short answer: No, with the current product semantics ("any track can be any type"). Pools save zero or negative RAM at realistic caps.**

---

## 1. Stale Claim Corrections

Claims from existing analysis documents that are superseded by newer probe data:

| Document | Stale Claim | Correction | Source |
|---|---|---|---|
| `architecture-research-directions.md` L108 | "Model Container pays NoteTrack tax" | **CurveTrack (~10,092 B) dominates, not NoteTrack (~9,612 B).** The "max-size tax" conclusion still holds — smaller types pay CurveTrack's size — but the dominant type is CurveTrack. | `ram-recovery-experiments.md` host probe: NoteTrack=9,612, CurveTrack=10,092, Track=10,120 |
| `assumption-matrix.md` L1 | Model Container can hold largest track type — "NoteTrack (~9,500 B) tax paid by all 7 track types" | **CurveTrack (~10,092 B) is the tax.** All 7 non-Curve types pay CurveTrack's size in the Container. | Same host probe data |
| `source-probe-investigation.md` (original) | Project loads use `_requestLock` for model mutation | **Project loads use `Engine::suspend()`** (ProjectPage.cpp:213, :196, :146). Track mode UI changes use `lock()`. These are two different mechanisms with different engine behaviors. | Source trace correction |
| `source-probe-investigation.md` (original) | Destroy-before-create gap only in model Container | **Both model and engine containers** lack destroy-before-create. `Track::setTrackMode()` and `Engine::updateTrackSetups()` both do `create<New>()` without `destroy<Old>()`. | Source trace correction |
| `architecture-change-decision-gates.md` | Model container / lazy sequence storage: "No for now" | **Confirmed correct.** Per-type pools are negative savings at realistic caps. Lazy sequence storage still fails Gate 5 (save-format change). Pool verdict does not supersede this. | This document's Scenario 2 analysis |

---

## 2. Mutation Boundary (Source-Probe Corrected)

| Mutation Path | Concurrency Guard | Engine Behavior | Engine Runs During? |
|---|---|---|---|
| **Track mode UI change** | `Engine::lock()/unlock()` (`TrackModeListModel::toProject()`, `ClipBoard::paste()`) | Full pause — `update()` returns immediately at `_locked` check | No — engine skips entire cycle |
| **Project load** | `Engine::suspend()/resume()` (`ProjectPage.cpp:213, :196, :146`) | Partial pause — consumes ticks/MIDI, updates CV/gate overrides, but **stops track engines and `updateTrackSetups()`** | Partial — clock/MIDI consumed, no track engines |
| **Python/test paths** | None — engine not running | No concurrency | N/A |

**Pool implication:** Model mutation always happens when engine track engines are stopped or suspended. Pool allocation/deallocation cannot collide with engine references. The mutation boundary is safe for any pool strategy.

---

## 3. Object Lifetime Gaps (Source-Probe Corrected)

| Container | Create Path | Destroy Before Create? | Fix |
|---|---|---|---|
| **Model `Track::_container`** | `setTrackMode()` → `initContainer()` → `create<NewType>()` | **NO** — placement new over old object, no destructor call | Add `destroy<OldType>()` before `create<NewType>()` |
| **Engine `_trackEngineContainers[i]`** | `updateTrackSetups()` → `trackContainer.create<XTrackEngine>(...)` | **NO** — same pattern | Add `destroy<OldType>()` before `create<NewType>()` |

**Safe-by-coincidence:** All current Track and TrackEngine types have trivial destructors. Any future type with owned heap memory would leak or corrupt.

**`SANITIZE_TRACK_MODE` is `CONFIG_ENABLE_SANITIZE`-gated** (SystemConfig.h:26, currently 1). In release builds with `CONFIG_ENABLE_SANITIZE=0`, `Container::as<T>()` is an unchecked `reinterpret_cast` — wrong-type access is silent undefined behavior. A stored discriminant would make this check unconditional.

**These gaps are real bugs but do not require a pool architecture to fix.** They are local fixes: add `destroy<>()` calls and add a `_typeIndex` to `Container`.

---

## 4. ARM Sizes — Consolidated from All Sources

Current sizes are ARM MonitorPage values after P15 unless noted.

### Model Track Types

| Type | ARM Size | Notes |
|---|---|---|---|
| **NoteTrack** | **9,544 B** | Current max type after P15; dominates `Track::_container`. |
| **CurveTrack** | **9,480 B** | 64 B below NoteTrack after P15. |
| **IndexedTrack** | **7,496 B** | Larger than early estimates, but below Note/Curve. |
| **TeletypeTrack** | **7,104 B** | `scene_state_t` (4,640 B) + 2× PatternSlot (2,452 B) + small fields/padding. |
| **DiscreteMapTrack** | **2,196 B** | 32 stages + 17 sequence metadata. |
| **TuesdayTrack** | **718 B** | 17 TuesdaySequence parameter objects + track fields. |
| **MidiCvTrack** | **24 B** | Minimal stub type. |

**Container per track = Track=9,560 B. 8 tracks × 9,560 B = 76,480 B in `.bss`.**

### Engine Track Types

| Type | ARM Size | Source | Notes |
|---|---|---|---|
| **TeletypeTrackEngine** | 912 B | MonitorPage SIZES page 6 | Current max type. GeodeEngine, LFO, envelope, CV slew, 8×CV+8×gate |
| **NoteTrackEngine** | ~590 B | estimate (Modulove comparison) | |
| **CurveTrackEngine** | ~590 B | estimate | |
| **TuesdayTrackEngine** | ~500 B | estimate | |
| **DiscreteMapTrackEngine** | ~400 B | estimate | |
| **IndexedTrackEngine** | ~400 B | estimate | |
| **MidiCvTrackEngine** | ~300 B | estimate | |

**Container per track = TrackEngineContainer=912 B. 8 tracks × 912 B = 7,296 B in `.ccmram`.**

### Key Derived Numbers

| Quantity | Value | Location |
|---|---|---|
| Model Container total | 76,480 B | `.bss` |
| Engine Container total | 7,296 B | `.ccmram` |
| **Combined Container total** | **83,776 B** | |
| TeletypeTrackEngine direct gap | (912 - 588) × 8 = 2,592 B conditional | `.ccmram` |
| RoutingEngine TrackState total | 56 B × 8 × 16 = 7,168 B | `.ccmram` |

---

## 5. Pool Shape Analysis — Pass/Fail Matrix

### Option A: Per-Type Pools with Caps

Each type gets `alignas(Type) uint8_t pool[N_type][sizeof(Type)]`. `Track` stores `(typeIndex, poolSlotIndex)` instead of `Container`.

**RAM = sum(cap[type] × sizeof(type))**

| Capacity Scenario | Model Pool RAM | vs Status Quo (80,960 B) | Product Constraint | Verdict |
|---|---|---|---|---|
| **Unlimited caps (8 of each)** | 8 × (10,092 + 200 + 500 + 1,800 + 4,000 + 2,800 + 9,612) = 8 × 29,004 = **232 KB** | **+144 KB (3× worse)** | None | **FAIL** |
| **Realistic caps: 4C/4N/2T/2I/1D/1Tu/1M** | 4×10,092 + 4×9,612 + 2×2,800 + 2×4,000 + 1×1,800 + 1×500 + 1×200 = **94,916 B** | **+13,956 B (17% worse)** | Must support 4 Curve + 4 Note simultaneously | **FAIL** |
| **Aggressive caps: 3C/3N/1T/1I/0/0/0** | 3×10,092 + 3×9,612 + 1×2,800 + 1×4,000 = **63,504 B** | **−17,456 B (22% savings)** | Max 3 Curve, 3 Note, 1 Teletype, no Tues/DM/MIDI | **Product constraint needed** |
| **Minimal caps: 2C/2N/1T/1I/0/0/0** | 2×10,092 + 2×9,612 + 1×2,800 + 1×4,000 = **46,608 B** | **−34,352 B (42% savings)** | Max 2 Curve, 2 Note, 1 Teletype, 1 Indexed | **Severe product constraint** |

**Per-type pools require product constraints on simultaneous type counts to save any RAM.** Without constraints, they are worse than the current Container. The realistic "4C/4N" cap is already 17% worse than status quo because per-type pools don't benefit from union sharing.

### Option B: Fixed Arena with Placement-New (Status Quo)

This is exactly the current `Container<T...>`. Already minimum static representation for "any track can be any type." No change possible without constraints.

### Option C: Hybrid — Extract Teletype from Both Containers

Remove TeletypeTrack from model Container and TeletypeTrackEngine from engine Container. Current measurements show model extraction is not useful because TeletypeTrack is already below NoteTrack; only engine extraction can reduce a container gate.

| Component | Per-Track | ×8 Tracks | Extra Pool | Total | Location |
|---|---|---|---|---|---|
| Model Container (max = NoteTrack) | 9,560 B | 76,480 B | — | — | `.bss` |
| TeletypeTrackPool[2] | — | — | 2 × 7,104 = 14,208 B | — | `.bss` |
| Engine Container (max = NoteTrackEngine) | ~590 B | ~4,720 B | — | — | `.ccmram` |
| TeletypeTrackEnginePool[2] | — | — | 2 × 912 = 1,824 B | — | `.ccmram` |
| **Total hybrid** | | | | **worse for model RAM unless Teletype count is capped and model semantics change** | |

Extracting TeletypeTrack from the model container saves nothing under current gates because `Track::_container` is sized by NoteTrack. Extracting TeletypeTrackEngine remains plausible only as a separate engine-side cap/lifecycle decision.

### Option D: Lazy Sequence Storage (Active Pattern Only)

Store only the active pattern fully; inactive patterns as deltas or compressed. Attacks CurveTrack/NoteTrack size directly.

| What | Mechanism | Savings | Gate Score |
|---|---|---|---|
| Lazy sequence storage | 17 patterns → 1 active + 16 compressed | 8 × 16 × ~560 B = ~71,680 B theoretical | **Fails Gate 5** (save-format change) and Gate 3 (local fixes work: sequence packing). Already scored "No for now" in decision-gates. |

### Pass/Fail Summary

| Option | RAM Delta | Product Constraint Needed? | Passes Decision Gates? |
|---|---|---|---|
| **A: Per-type pools (realistic caps)** | **+13,956 B (worse)** | Yes (cap simultaneous types) | **FAIL** — Gate 4 (no simpler contract), Gate 7 (negative payoff) |
| **B: Fixed arena (status quo)** | 0 | No | N/A — no change proposed |
| **C: Hybrid (extract Teletype)** | **+4,896 B (worse)** | Yes (cap Teletype to 2) | **FAIL** — Gate 7 (negative payoff) |
| **D: Lazy sequence storage** | −71,680 B theoretical | No, but save-format change | **FAIL** — Gate 5 (migration), Gate 3 (local fixes work) |
| **Local fixes only** | 0 + bug fixes | No | **PASS** — no architecture change needed |

---

## 6. Engine Reference Stability

| Question | Answer | Implication for Pools |
|---|---|---|
| Can engines cache pointers after setup? | **YES** — each XTrackEngine stores `_xTrack` as a reference from `Track.noteTrack()` etc. | Per-type pools must return stable addresses (static arrays, no compaction) |
| Can track objects move during engine use? | **NO** — `Track` lives in `std::array<Track, 8>` in `Project`. Address stable. | Pools with fixed-slot addresses preserve this |
| Does `setTrackMode()` invalidate references? | **YES** — `initContainer()` does placement new over old type. But `updateTrackSetups()` reconstructs the engine immediately. | Pools must destroy old type, construct new type, and update engine reference atomically within lock/suspend |
| What about `Engine::suspend()`? | Engine consumes ticks/MIDI but does NOT call `updateTrackSetups()` or run track engines. Model mutation is safe under suspend. | Pool mutation under suspend is safe — no engine references active |

**Conclusion:** Per-type static pools with fixed-slot addresses and destroy-then-create under lock/suspend would preserve reference stability. The issue is not reference stability — it's that the math doesn't work.

---

## 7. Decision Against Existing Analysis

### `architecture-change-decision-gates.md` says "No for now" on model container/lazy storage

**This decision table confirms that verdict and adds quantitative evidence.** The "No for now" was based on Gate 5 (migration path) and Gate 3 (local fixes). This analysis adds:

- **Gate 7 (net payoff) FAILS for per-type pools:** Realistic-cap pools are 5-7 KB *worse* than the current Container (Scenario 2: +13,956 B). Only aggressive product constraints produce savings, which makes this a product decision, not an architecture change.
- **The Container is already the optimal static representation** for "any track can be any type." No pool architecture beats it without constraining which types can coexist.

### `assumption-matrix.md` says Model Container max-size is stressed but acceptable

**Confirmed.** The Container pattern itself works. The tax is real (~10 KB per track for the CurveTrack-dominated union) but cannot be reduced without either:
1. Product-level caps on simultaneous type counts (rejected by Gate 7 — negative payoff at realistic caps)
2. Lazy sequence storage (rejected by Gate 5 — save-format change)
3. Reducing CurveTrack/NoteTrack per-pattern overhead (local optimization, not pool architecture)

### `architecture-research-directions.md` Direction 1 (Model Container)

**Stale claim corrected:** Direction 1 says "NoteTrack dominates." CurveTrack (~10,092 B) actually dominates. The direction's conclusion ("smaller track types pay the max-type tax") is still correct, just the dominant type was wrong.

**Recommendation:** Update Direction 1 to reference CurveTrack as the dominant type and link to this decision table's quantitative analysis showing per-type pools fail Gate 7.

---

## 8. Verdict

**STAY WITH TRACK A — no pool architecture change.**

The current `Container<T...>` is the optimal static representation for the "any track can be any type" product semantics. Per-type pools are worse at realistic caps and only save RAM with aggressive product constraints that limit simultaneous type counts. The Container already provides union-of-max-size storage with zero overhead beyond the max type size.

**The real wins are elsewhere:**

| Change | Estimated Savings | Complexity | Decision Gate |
|---|---|---|---|
| RoutingEngine union refactor (phased plan) | ~3,072 B CCMRAM | Medium | Already scoped — proceed |
| Fix destroy-before-create in Container | 0 B (bug fix) | Low | Local fix, not architecture |
| Add `_typeIndex` discriminant to Container | 0 B (safety) | Low | Local fix, not architecture |
| ARM `sizeof` verification | 0 B (measurement) | Low | Prerequisite for all estimates |
| Lazy sequence storage (separate project) | ~71 KB theoretical | Very high | Fails Gate 5 — only if product changes format |

**Actionable follow-ups (not a pool project):**

1. **Fix destroy-before-create:** Add `Container::destroy<OldType>()` calls in `Track::setTrackMode()` and `Engine::updateTrackSetups()` before `create<NewType>()`. Bug fix, not architecture.

2. **Add discriminant to Container:** Store `uint8_t _typeIndex` in `Container`, check it in `as<T>()` unconditionally. Replace `SANITIZE_TRACK_MODE` with Container-internal validation that works in all builds. Safety improvement, not RAM optimization.

3. **ARM sizeof verification:** Add `static_assert(sizeof(X) <= N)` for each Track and TrackEngine type. Build for STM32, confirm sizes. Measurement, not implementation.

4. **Update `architecture-research-directions.md` Direction 1:** Correct "NoteTrack dominates" to "CurveTrack dominates (~10,092 B)." Link to this decision table.

5. **Proceed with RoutingEngine union refactor** (`routingengine-refactor-phased-plan.md`): This is the high-ROI architecture change. ~3 KB CCMRAM from actual waste, no product constraints, no save-format changes.
