---
id: modulation-phase1-plan
schema: plan
title: "plan: modulation phase 1 — retire legacy + MODULATE/REMOVE rename + draft/commit core"
type: plan
status: draft
scope: next
date: 2026-06-04
implements: modulation-ui-spec   # docs/plans/2026-06-04-013-modulation-ui-spec.md
related:
  - routing-legacy-vs-matrix
---

# Modulation Phase 1 Implementation Plan

> **For Claude:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan
> task-by-task. The contract is FROZEN in `docs/plans/2026-06-04-013-modulation-ui-spec.md`
> (§12 phase 1 + the draft/commit half of phase 2, bundled per the resume brief). Do NOT
> re-open the design. If implementation hits a case the spec does not cover, STOP and amend
> spec 013 — do not improvise.

**Goal:** Retire the legacy per-route editor for migrated targets, replace the context ROUTE slot
with a single state-dependent **MOD+ / MOD-** slot (per-track membership; see spec 013 §15), gate
it migrated-only, and land the door-agnostic draft→commit + membership ops both doors will use.

**Architecture:** The draft/commit core is a pure header-only model helper `model/RouteDraft.h`
(same shape as `RouteApply.h`/`RouteFork.h`/`RouteShaper.h`/`RouteBrowse.h`): a `Draft` value =
a working copy of one `Routing::Route` + its slot index + an `isNew` flag. The live `Route` is
untouched until `commit`; `cancel` reverts (and frees the slot if `isNew`); `canCommit` requires
a real source. Eligibility reuses the existing `RouteFork::migrated`/`migratedGlobal` gates. UI
work renames/gates the context menu and drops the legacy `showRoute`/`RouteListModel` fallback.

**Tech Stack:** C++ (STM32 firmware + sim), custom UnitTest framework (`UNIT_TEST`/`CASE`/
`expect*`), CMake sim test runner, `ui-preview/` for OLED render checks.

**Scope discipline:** Only the named work. No new keys/LEDs/labels beyond MODULATE / REMOVE MOD.
No refactor of surrounding routing code. The inline param-door editing surface (horizontal bar,
press value↔depth, SRC/COMBINE F-keys) is **phase 3**, not here.

**STOP POINT:** After Task 1 (RouteDraft model + tests, Codex-gated, sim green) — that is the
first reviewable unit. Present it and wait before any UI task.

---

## Carry-forwards (already on branch — keep, do not redo)

- `Routing::findRoute` `isPerTrackTarget` fix (`Routing.cpp:180`) — correct, leave it.
- depth-0-on-create — absorbed into `RouteDraft::create` (Task 1), so the MVP's
  `RoutingPage::tabCreateRoute` depth-0 loop is superseded when Task 5 retires that path.

## Superseded (drop during this phase)

- The MVP auto-chain `TopPage::editRoute` → `beginModulate` → source overlay → depth modal
  (spec 013 §14). The inline row + draft/commit replaces it.

---

### Task 1: `RouteDraft.h` — draft/commit staging core (FIRST REVIEWABLE UNIT)

**Files:**
- Create: `src/apps/sequencer/model/RouteDraft.h`
- Test:   `src/tests/unit/sequencer/TestRouteDraft.cpp`
- Modify: `src/tests/unit/sequencer/CMakeLists.txt` (register the test)

Public API (header-only, firmware untouched until later tasks wire it):

```cpp
#pragma once

#include "Routing.h"
#include "RouteApply.h"
#include "core/Debug.h"

// Draft/commit staging for one modulation. The live Route is untouched until commit().
// A draft for a freshly-allocated slot is freed on cancel(); an existing route reverts.
namespace RouteDraft {

struct Draft {
    Routing::Route route;   // working copy (the staging buffer)
    int routeIndex = -1;    // slot in Routing (-1 = no slot)
    bool isNew = false;     // slot was freshly allocated by create()
};

// Stage a fresh modulation: allocate a slot, set target/tracks, combine=Modulate,
// all depth 0, source None (inert). routeIndex stays -1 if no empty slot.
inline Draft create(Routing &routing, Routing::Target target, int trackIndex) {
    Draft d;
    int idx = routing.findEmptyRoute();
    if (idx < 0) {
        return d;
    }
    auto &r = routing.route(idx);
    r.clear();
    r.setTarget(target);
    r.setTracks(1 << trackIndex);
    r.setCombine(RouteApply::Combine::Modulate);
    for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
        r.setDepthPct(t, 0);
    }
    // source stays None: nothing audible until commit() with a real source
    d.route = r;
    d.routeIndex = idx;
    d.isNew = true;
    return d;
}

// Begin editing an existing committed route (copy live -> draft).
inline Draft begin(const Routing &routing, int routeIndex) {
    Draft d;
    d.route = routing.route(routeIndex);
    d.routeIndex = routeIndex;
    d.isNew = false;
    return d;
}

// Source is required to commit.
inline bool canCommit(const Draft &d) {
    return d.routeIndex >= 0 && d.route.source() != Routing::Source::None;
}

// Write draft -> live. Caller guarantees canCommit().
inline void commit(Routing &routing, const Draft &d) {
    routing.route(d.routeIndex) = d.route;
}

// Revert: existing route is already untouched; a new slot is freed.
inline void cancel(Routing &routing, const Draft &d) {
    if (d.isNew && d.routeIndex >= 0) {
        routing.route(d.routeIndex).clear();
    }
}

} // namespace RouteDraft
```

**Step 1: Write the failing test** — `src/tests/unit/sequencer/TestRouteDraft.cpp`

```cpp
#include "UnitTest.h"

#include "apps/sequencer/model/RouteDraft.h"
#include "apps/sequencer/model/Routing.h"

UNIT_TEST("RouteDraft") {

    CASE("create allocates an inert slot with phase-1 defaults") {
        Routing routing;
        auto d = RouteDraft::create(routing, Routing::Target::Transpose, 2);
        expectTrue(d.routeIndex >= 0, "slot allocated");
        expectTrue(d.isNew, "isNew set");
        expectEqual(int(d.route.target()), int(Routing::Target::Transpose), "target");
        expectEqual(int(d.route.tracks()), int(1 << 2), "tracks = current track only");
        expectEqual(int(d.route.combine()), int(RouteApply::Combine::Modulate), "Modulate");
        expectEqual(int(d.route.source()), int(Routing::Source::None), "source None");
        for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
            expectEqual(d.route.depthPct(t), 0, "depth 0");
        }
    }

    CASE("create returns routeIndex -1 when no empty slot") {
        Routing routing;
        for (int i = 0; i < CONFIG_ROUTE_COUNT; ++i) {
            routing.route(i).setTarget(Routing::Target::Transpose);
        }
        auto d = RouteDraft::create(routing, Routing::Target::Octave, 0);
        expectEqual(d.routeIndex, -1, "no slot");
    }

    CASE("canCommit requires a real source") {
        Routing routing;
        auto d = RouteDraft::create(routing, Routing::Target::Transpose, 0);
        expectFalse(RouteDraft::canCommit(d), "source None -> cannot commit");
        d.route.setSource(Routing::Source::CvIn1);
        expectTrue(RouteDraft::canCommit(d), "source set -> can commit");
    }

    CASE("editing the draft does NOT touch the live route before commit") {
        Routing routing;
        int idx;
        { auto c = RouteDraft::create(routing, Routing::Target::Transpose, 0);
          c.route.setSource(Routing::Source::CvIn1);
          RouteDraft::commit(routing, c); idx = c.routeIndex; }
        // begin editing the committed route, change depth on the draft only
        auto d = RouteDraft::begin(routing, idx);
        d.route.setDepthPct(0, 75);
        expectEqual(routing.route(idx).depthPct(0), 0, "live unchanged pre-commit");
        RouteDraft::commit(routing, d);
        expectEqual(routing.route(idx).depthPct(0), 75, "live updated on commit");
    }

    CASE("cancel on a new draft frees the slot") {
        Routing routing;
        auto d = RouteDraft::create(routing, Routing::Target::Transpose, 0);
        int idx = d.routeIndex;
        RouteDraft::cancel(routing, d);
        expectEqual(int(routing.route(idx).target()), int(Routing::Target::None), "slot freed");
        expectEqual(routing.findEmptyRoute(), idx, "slot reusable");
    }

    CASE("cancel on an existing draft leaves the live route intact") {
        Routing routing;
        int idx;
        { auto c = RouteDraft::create(routing, Routing::Target::Transpose, 0);
          c.route.setSource(Routing::Source::CvIn1);
          c.route.setDepthPct(0, 40);
          RouteDraft::commit(routing, c); idx = c.routeIndex; }
        auto d = RouteDraft::begin(routing, idx);
        d.route.setDepthPct(0, 90);
        RouteDraft::cancel(routing, d);
        expectEqual(routing.route(idx).depthPct(0), 40, "live unchanged");
        expectEqual(int(routing.route(idx).source()), int(Routing::Source::CvIn1), "src kept");
    }
}
```

**Step 2: Register the test** — `src/tests/unit/sequencer/CMakeLists.txt`, in the routing block
(near `register_sequencer_test(TestRouteFork TestRouteFork.cpp)`):

```cmake
register_sequencer_test(TestRouteDraft TestRouteDraft.cpp)
```

**Step 3: Run the test, expect FAIL** (RouteDraft.h not created yet)

Run: `make -C build/sim/release TestRouteDraft && build/sim/release/src/tests/unit/sequencer/TestRouteDraft`
Expected: build error — `RouteDraft.h: No such file`.

**Step 4: Create `src/apps/sequencer/model/RouteDraft.h`** (the API block above).

**Step 5: Run the test, expect PASS**

Run: `make -C build/sim/release TestRouteDraft && build/sim/release/src/tests/unit/sequencer/TestRouteDraft`
Expected: all 6 cases pass.

**Step 6: Confirm no firmware regression + zero warnings**

Run: `make -C build/sim/release sequencer` — expect clean, zero warnings (header is unreferenced
by firmware so far; this only proves it compiles under the build's flags via the test).

**Step 7: Codex-gate the unit** (model unit — required by CLAUDE.md). Hand `RouteDraft.h` +
`TestRouteDraft.cpp` to Codex; address findings.

**Step 8: Commit**

```bash
git add src/apps/sequencer/model/RouteDraft.h \
        src/tests/unit/sequencer/TestRouteDraft.cpp \
        src/tests/unit/sequencer/CMakeLists.txt
git commit -m "feat(routing): RouteDraft draft/commit staging core (spec 013 phase 1)"
```

**>>> STOP. Present Task 1 for review before any UI task. <<<**

---

### Task 2: `RouteDraft::removeTrack` + `isTrackModulated` membership ops (model, TDD — NEXT REVIEWABLE UNIT)

The MOD+/MOD- slot needs two model facts: is THIS track modulated for the param (label state),
and remove THIS track (MOD- action — per-track, frees the slot only when last). Add both to
`RouteDraft.h` (broaden its top comment from "draft/commit staging" to "draft/commit + membership
ops for a modulation") and TDD them. Codex-gate (model unit).

**Files:** Modify `src/apps/sequencer/model/RouteDraft.h`; modify `src/tests/unit/sequencer/TestRouteDraft.cpp`.

```cpp
// Is this track currently modulated for the target? (label: MOD- when true, MOD+ when false)
inline bool isTrackModulated(const Routing &routing, Routing::Target target, int trackIndex) {
    return routing.findRoute(target, trackIndex) >= 0;
}

// MOD- : remove this track from the modulation. Per-track: clear the track bit + zero its depth;
// when it was the last track, free the whole slot. Global target: free the slot. No-op if absent.
inline bool removeTrack(Routing &routing, Routing::Target target, int trackIndex) {
    int r = routing.findRoute(target, trackIndex);
    if (r < 0) {
        return false;
    }
    auto &route = routing.route(r);
    if (Routing::isPerTrackTarget(target)) {
        route.setTracks(route.tracks() & ~(1 << trackIndex));
        route.setDepthPct(trackIndex, 0);
        if (route.tracks() == 0) {
            route.clear();
        }
    } else {
        route.clear();
    }
    return true;
}
```

TDD cases to add: (a) isTrackModulated false before create / true after commit on that track;
(b) removeTrack on a single-track per-track modulation frees the slot (target None, reusable);
(c) removeTrack on a multi-track modulation clears only the named track's bit + zeroes its depth,
leaves the other track modulating (build the 2-track route by hand: create, setSource, setTracks
to two bits, commit); (d) removeTrack on a global modulation frees the slot; (e) removeTrack
returns false when the param isn't modulated on that track.

Run, fail→pass, Codex-gate, **amend the Task-1 commit** (same RouteDraft module) or a fresh
commit `feat(routing): RouteDraft membership ops (isTrackModulated/removeTrack)`.

**>>> STOP. Present Task 2 for review before the UI tasks. <<<**

---

### Task 3: State-dependent MOD+ / MOD- context slot (migrated pages)

Replace the legacy ROUTE slot's fixed label with MOD+ (this track not modulated) / MOD- (this
track modulated), computed at `contextShow` from `RouteDraft::isTrackModulated`. Renders verified:
`ui-preview/modulation/modulation-ctx-*.png` (MOD+/MOD- = 22px, fit cleanly).

**Pages:** `NoteSequencePage`, `PhaseFluxSequencePage`, `TrackPage` (Note/PhaseFlux rows),
`ProjectPage` (global). The label is dynamic, so pass a state-chosen item array (two `static const`
arrays differing only in the modulation slot, picked at `contextShow` by membership), or compute
the item title at show time. Keep `ContextAction::Route` as the internal token.

**Step 1:** Implement the state-chosen label per page.
**Step 2: Build** `make -C build/sim/release sequencer` — clean, zero warnings.
**Step 3: Hardware-verify**: a Note Transpose row reads MOD+ unmodulated, MOD- once modulated.
**Step 4: Commit** `feat(routing): state-dependent MOD+/MOD- context slot (migrated pages)`.

---

### Task 4: Wire MOD+ / MOD- handlers

**Files:** each migrated page's `contextAction`.
- **MOD+** (this track not modulated): the create path — `RouteDraft::create(routing, target,
  trackIndex)` → source picker → on COMMIT `RouteDraft::commit`, on CANCEL `RouteDraft::cancel`.
  (Add-this-track-to-an-existing-multi-track-modulation cannot arise in phase 1 — multi-track is
  built by the matrix door, phase 5 — so MOD+ here is always create.) The inline depth-editing
  surface is **phase 3**; phase-1 MOD+ commits inert (depth 0) once a source is picked.
- **MOD-** (this track modulated): `RouteDraft::removeTrack(routing, target, trackIndex)`.

**Step 1:** Implement both handlers (route the existing `initRoute` target/track resolution).
**Step 2: Build** clean.
**Step 3: Hardware-verify**: MOD+ → source → COMMIT creates an inert modulation; the slot flips
to MOD-; MOD- clears it; CANCEL on a fresh pick frees the slot.
**Step 4: Commit** `feat(routing): MOD+ create (RouteDraft) / MOD- removeTrack handlers`.

---

### Task 5: Migrated-only gating — hide non-migrated entries

The MOD+/MOD- slot must appear only where the row's target is migrated; non-migrated pages' ROUTE
entry is hidden.

**Files:**
- Migrated pages: `contextActionEnabled(Route)` returns
  `RouteFork::migrated(track.trackMode(), target, key, range) || RouteFork::migratedGlobal(target, key, range)`
  for the focused row's target (mirrors the `TopPage::editRoute` gate).
- Non-migrated pages (`CurveSequencePage`, `TuesdaySequencePage`, `Stochastic*Page`,
  `DiscreteMapSequence*Page`, `IndexedSequence*Page`, `MidiCvTrack`/`TrackPage` non-Note rows):
  drop the ROUTE item from `contextMenuItems[]` (and its enum/case) so it does not render
  (spec §12.1 "non-migrated context entry hidden").

**Step 1: Render check** a non-migrated page's menu confirms no MOD+/ROUTE item.
**Step 2:** Implement the enabled-gate (migrated) + item removal (non-migrated).
**Step 3: Build** clean.
**Step 4: Hardware-verify**: Curve/Tuesday rows show no MOD slot; Note/PhaseFlux/global do.
**Step 5: Commit** `feat(routing): gate MOD slot to migrated targets; hide non-migrated entries`.

---

### Task 6: Retire the legacy editor path

With non-migrated targets unreachable (Task 5), `TopPage::editRoute` no longer needs the legacy
`showRoute`/`RouteListModel` fallback, and the MVP `beginModulate` auto-chain is superseded
(spec §14).

**Files:**
- `src/apps/sequencer/ui/pages/TopPage.cpp` — `editRoute`: drop the `setMode(Mode::Routing)` +
  `showRoute(...)` legacy branch and the MVP `beginModulate` auto-chain. The create path now
  lives in the page MOD+ handler (Task 4) via `RouteDraft`; reduce/retire `editRoute` accordingly.
- `src/apps/sequencer/ui/pages/RoutingPage.cpp` / `.h` — stop entering the legacy per-route editor
  (`showRoute`) and `beginModulate`. Leave `RouteListModel` in the tree but unreferenced (deletion
  is phase 9 cleanup, not now).

**Step 1:** Read `TopPage.cpp:34-83` + `RoutingPage.cpp` (showRoute/beginModulate) before editing.
**Step 2:** Remove the legacy fallback + auto-chain; confirm the MOD+ create path is the only entry.
**Step 3: Build** clean, zero warnings.
**Step 4: Hardware-verify**: non-migrated has no path into any route editor; migrated MOD+/MOD- work.
**Step 5: Commit** `feat(routing): retire legacy route editor + MVP auto-chain`.

---

## Verification (end of phase)

- `make -C build/sim/release` test suite green except the 5 pre-existing failures
  (DiscreteMapSequence, StochasticDurationDictionary, HarmonyInversionIssue, CurveTrackLfoShapes
  ×2 — NOT ours).
- `make -C build/stm32/release sequencer` builds clean, zero warnings.
- All model units Codex-gated.
- OLED label changes render-checked in `ui-preview/` before firmware.

## Out of scope (later phases — do NOT build here)

- Inline param-door editing: horizontal bipolar bar, press value↔depth, SRC/COMBINE F-keys (§4) — **phase 3**.
- Spread sub-view (Shift+S5) — **phase 4**.
- Matrix grid — **phase 5**. Per-engine migration — **phase 6**. MIDI LEARN / shaper — **phase 7**.
- Deleting `RouteListModel` / old `writeTarget` half — **phase 9** cleanup.
