---
id: basepage-keypress-prologue
schema: plan
title: "refactor: fold the context-menu keyPress idioms into BasePage::handleContextMenuKey"
type: refactor
status: completed
date: 2026-06-22
depth: standard
---

# BasePage context-menu keyPress fold

Extract the two duplicated context-menu-open idioms at the top of panel `keyPress` into one
`BasePage` helper, migrate the pages that open `keyPress` with those two idioms verbatim, and
refactor `TuesdayEditPage`'s inline context menu onto the shared `contextShow()` virtual.
Behavior stays identical everywhere — no key binding changes.

---

## Summary

Most pages open `keyPress` with the same two context-menu gestures:

1. `if (key.isContextMenu()) { contextShow(); consume; return; }`
2. `if (key.pageModifier() && event.count() == 2) { contextShow(true); consume; return; }`

There is no shared base for this (unlike the USB-keyboard side, already folded into
`BasePage::keyboard`/`handleFunctionKeys`). This refactor adds
`BasePage::handleContextMenuKey(event) -> bool` holding those two idioms, called at the top of
each adopting page.

**Critically, a third idiom is NOT extracted.** Many pages also have a one-line catch-all
`if (key.pageModifier()) return;` — but it sits at the *end* of `keyPress`, **after** that page's
own `Page+Step` / `Page+QuickEdit` / `Page+Function` handlers. Bundling it into a top-of-handler
helper would fire before those branches and swallow them (kills quick-edit and named-context
bindings). So idiom 3 stays inline, untouched, wherever each page currently has it.

This is a behavior-neutral refactor.

---

## Problem Frame

A cross-page survey of `keyDown`/`keyUp`/`keyPress` found the real duplication is the `keyPress`
context-menu prologue. (`keyDown`/`keyUp` carry no extractable duplication — already folded by
`StepSelection<N>`.) The pattern to mirror exists: `BasePage::handleFunctionKeys` is the same
"common handler returns bool, page falls through" shape on the keyboard path.

The trap, caught in review: the prologue is **not** a contiguous 3-block sequence on most pages.
Idioms 1 and 2 are at the top, but the bare `pageModifier → return` is a catch-all placed *after*
page-specific `Page+X` handlers, e.g.:
- `NoteSequenceEditPage` — `isQuickEdit()` (Page+Step quick-edit) sits between idiom 2 and the bare return.
- `CurveSequenceEditPage`, `IndexedSequenceEditPage`, `CurveSequencePage` — `Page+Step` named-context branches sit there.
- `TrackPage` — function-key handling sits there.

On many pages idioms 1+2+3 are contiguous (the ListPage subclasses, `ModulatorPage`, `SongPage`,
the config pages); on others a `Page+X` handler sits between idiom 2 and the bare return. Either
way, the universally safe extraction is **idioms 1+2 only**; idiom 3 is a one-liner whose position
is page-specific and must not move.

The consume/bubble model is confirmed: `PageManager` dispatches unconsumed events downward
(`src/apps/sequencer/ui/PageManager.cpp`), and `TopPage` handles Page+nav
(`src/apps/sequencer/ui/pages/TopPage.cpp`). Idioms 1+2 consume; the inline idiom 3 returns
without consuming so Page+X bubbles to `TopPage`. The helper only owns the two consuming cases,
so the bubble path is preserved by leaving idiom 3 inline.

---

## Scope Boundaries

In scope:
- `BasePage::handleContextMenuKey` helper (idioms 1+2).
- Migration of pages whose `keyPress` opens with idioms 1+2 verbatim; their inline idiom-3 catch-all and all `Page+X` handlers stay untouched.
- `TuesdayEditPage`: convert its inline `showContextMenu(ContextMenu(...))` to a `contextShow()` override.

Out of scope (excluded with reason — full verified list with citations in U2):
- `TuesdayEditPage` — idiom 1 only; gets the `contextShow()` refactor instead of the helper (U3).
- Gated idiom 1: `PatternPage` (`&& !_modal`), `SystemPage` (`_mode` check).
- Idiom-1-only pages lacking idiom 2 (`PhaseFluxEditPage`, `StochasticSequenceEditPage`, `TT2IoConfigPage`, `TeletypeScriptViewPage`) — the helper would add a page-double gesture.
- `PerformerPage` — only the bare idiom 3.

### Deferred to Follow-Up Work
- `Page+Step → named context` helper (Curve/Indexed/DiscreteMap share the branch shape, divergent step→action maps). Marginal; separate refactor.
- Performer/Pattern hold-to-close `keyUp` consolidation. Two pages, divergent side effects.

---

## Key Technical Decisions

- **Helper covers idioms 1+2 only; idiom 3 stays inline.** Bundling the bare `pageModifier → return` into a top-of-handler helper would fire before each page's `Page+Step`/`Page+QuickEdit`/`Page+Function` branches and swallow them. Idiom 3 is one line, its position is page-specific (a trailing catch-all), and it stays exactly where it is. This is the central correctness constraint.
- **`handleContextMenuKey` returns `bool`, consumes in both cases.** `isContextMenu` → `contextShow()`; `pageModifier && count == 2` → `contextShow(true)`; both consume + return true; else return false. Position-neutral: idiom 2 already sits at the top before any `Page+Step` single-press branch on every adopting page, so moving it into a top-called helper preserves order exactly (double-press Page+Step still resolves to the context menu where it does today).
- **Adoption criterion = contiguous ungated idiom 1 → idiom 2 in `keyPress`.** A page adopts if those two blocks appear contiguous, preceded only by required safety/mode guards (which stay before the helper). The helper replaces the two blocks in place; the page's idiom-3 catch-all and every `Page+X` handler are left untouched. Divergence — a gated idiom 1, or a non-guard branch between idioms 1 and 2 — means exclude, do not force-fit. Scan `keyPress`, not `keyDown`.
- **Tuesday adopts the `contextShow()` convention only.** `TuesdayEditPage` inlines `showContextMenu(ContextMenu(contextMenuItems, ContextAction::Last, contextAction, contextActionEnabled))`, already owns `contextAction`/`contextActionEnabled`, but has no `contextShow()` override and lacks idiom 2 (it uses `pageModifier` only as a step-key guard). The behavior-neutral change is wrapping the inline menu in a `contextShow(bool)` override and routing `isContextMenu` through it. It does not call the helper.

---

## Implementation Units

### U1. Add `BasePage::handleContextMenuKey`

**Goal:** A shared helper holding context-menu idioms 1+2, mirroring `handleFunctionKeys`.

**Dependencies:** none.

**Files:**
- `src/apps/sequencer/ui/pages/BasePage.h` (declare)
- `src/apps/sequencer/ui/pages/BasePage.cpp` (define)

**Approach:** Add `bool handleContextMenuKey(KeyPressEvent &event);`. Body, in order: `isContextMenu` → `contextShow()` + consume + return true; `pageModifier() && event.count() == 2` → `contextShow(true)` + consume + return true; else return false. Relies on the existing `BasePage::contextShow()` virtual. No page changes in this unit. Do **not** include the bare `pageModifier` return.

**Patterns to follow:** `BasePage::handleFunctionKeys` (bool-return, common-first shape on the keyboard path). The exact idiom text in `NoteSequenceEditPage::keyPress` is the reference.

**Execution note:** Characterization-first — read the current idiom-1/2 blocks from `NoteSequenceEditPage` and reproduce the consume + return-order exactly.

**Test scenarios:**
- Build sim + STM32 release clean with the helper added but unused.
- Test expectation: none — helper not yet called; pure addition. Pages are not unit-testable without the page+engine harness; correctness is verified at the adopter sites in U2.

**Verification:** Both targets build; `handleContextMenuKey` compiles and is callable; no page calls it yet.

---

### U2. Migrate pages with verbatim idioms 1+2

**Goal:** Replace each adopting page's two context-open blocks with `if (handleContextMenuKey(event)) return;`, leaving idiom 3 and all `Page+X` handlers in place.

**Dependencies:** U1.

**Discovery (the adopter set is defined by this scan, not a hand-list):** the complete candidate
set is every `src/apps/sequencer/ui/pages/*.cpp` whose **`keyPress`** contains both
`key.isContextMenu()` and `pageModifier() && event.count() == 2`. A page adopts if, within
`keyPress`, an **ungated** idiom 1 is immediately followed by idiom 2 — preceded only by required
safety/mode guards (a track-bind / type / mode check that consumes-and-returns on failure), which
stay in place before the helper. Scan `keyPress` specifically: some pages (e.g.
`DiscreteMapSequencePage`) also carry an unrelated `isContextMenu`/page-modifier block in
`keyDown` that must be ignored. Re-run the scan before implementing. As of plan-writing it yields
the 17 adopters and 6 exclusions below.

**Files — 17 adopters (verify the ungated 1→2 pair in `keyPress` before swapping):**
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp` (then `isQuickEdit`, then bare return — both stay)
- `src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` (then `Page+Step` branches, then bare return — stay)
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp` (then `Page+Step` branches, then bare return — stay)
- `src/apps/sequencer/ui/pages/NoteSequencePage.cpp`
- `src/apps/sequencer/ui/pages/CurveSequencePage.cpp` (then `Page+Step`, then bare return — stay)
- `src/apps/sequencer/ui/pages/TrackPage.cpp` (then function-key handling, then bare return — stay)
- `src/apps/sequencer/ui/pages/IndexedSequencePage.cpp`
- `src/apps/sequencer/ui/pages/TuesdaySequencePage.cpp`
- `src/apps/sequencer/ui/pages/DiscreteMapSequenceListPage.cpp`
- `src/apps/sequencer/ui/pages/PhaseFluxSequencePage.cpp`
- `src/apps/sequencer/ui/pages/IndexedStepsPage.cpp`
- `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp` (clean 1→2 in `keyPress`; the `pageModifier()||!_sequence` guard + mask tracking are in `keyDown`, untouched)
- `src/apps/sequencer/ui/pages/GeneratorPage.cpp` (helper goes **after** the leading `ensureBoundTrackContext()` guard)
- `src/apps/sequencer/ui/pages/ModulatorPage.cpp`
- `src/apps/sequencer/ui/pages/ProjectPage.cpp`
- `src/apps/sequencer/ui/pages/SongPage.cpp`
- `src/apps/sequencer/ui/pages/UserScalePage.cpp`

**Excluded (have idiom 1 and/or 2 in `keyPress` but fail the ungated 1→2 rule — verified at plan-writing):**
- `PatternPage.cpp` — idiom 1 is gated: `if (key.isContextMenu() && !_modal)`.
- `SystemPage.cpp` — idiom 1 is gated by a `_mode == Calibration/Settings` check.
- `PhaseFluxEditPage.cpp`, `StochasticSequenceEditPage.cpp`, `TT2IoConfigPage.cpp`, `TeletypeScriptViewPage.cpp` — idiom 1 only (no idiom 2); the 1+2 helper would add a page-double gesture they do not have. (`TuesdayEditPage` is also idiom-1-only — handled in U3.)

**Approach:** Per page: locate the contiguous idiom-1-then-idiom-2 pair in `keyPress`. Replace those two blocks with the single helper call **in place**. Any required guard that legitimately precedes them — a track-bind / mode / type safety check that consumes-and-returns on failure (e.g. `GeneratorPage`'s `ensureBoundTrackContext()`) — stays before the helper call. Leave everything after — `Page+Step`/`Page+QuickEdit`/`Page+Function` branches, the bare `pageModifier` catch-all, `ListPage::keyPress` delegation — byte-for-byte unchanged. If the 1→2 pair is gated or non-contiguous, exclude the page and note why.

**Patterns to follow:** ListPage subclasses run the prologue then call `ListPage::keyPress(event)`; the helper call goes before that delegation. The bare `pageModifier` return that immediately follows on the pure subclasses stays inline (do not fold it in).

**Execution note:** Behavior-neutral — diff each page to confirm only the two context-open blocks changed and nothing below moved.

**Test scenarios:**
- Build sim + STM32 release clean after the batch.
- Manual smoke per page (sim): context menu opens on its gesture; page-double-press opens it in `doubleClick` mode where it did before.
- **Regression guard (the whole point of the split):** `Page+Step` quick-edit / named-context branches still fire, and `Page+X` navigation still bubbles to `TopPage`. A page where Page+Step or quick-edit stops working means idiom 3 was wrongly pulled into the helper.

**Verification:** Both targets build; on the sim, context menu, Page+Step/QuickEdit/Function behaviors, and Page+nav are identical to before on every migrated page; diffs show only the two context-open blocks collapsed to one call.

---

### U3. Refactor TuesdayEditPage onto the `contextShow()` virtual

**Goal:** Convert Tuesday's inline context-menu open to a `contextShow(bool)` override and route its `isContextMenu` branch through it. No helper adoption.

**Dependencies:** none (independent of U1/U2; shares the `contextShow()` convention).

**Files:**
- `src/apps/sequencer/ui/pages/TuesdayEditPage.h` (add `void contextShow(bool doubleClick = false) override;`)
- `src/apps/sequencer/ui/pages/TuesdayEditPage.cpp` (define `contextShow`; route the inline `isContextMenu` block through it)

**Approach:** Add a `contextShow(bool doubleClick)` override whose body is the existing inline call: `showContextMenu(ContextMenu(contextMenuItems, int(ContextAction::Last), contextAction-lambda, contextActionEnabled-lambda, doubleClick))`. Replace the inline block in `keyPress` (`if (key.isContextMenu()) { showContextMenu(...); consume; return; }`) with `if (key.isContextMenu()) { contextShow(); event.consume(); return; }`. Leave Tuesday's `pageModifier` step-guards and everything else untouched. Tuesday does not call `handleContextMenuKey` (it lacks idiom 2; adding it would create a new page-double gesture).

**Patterns to follow:** The `contextShow()` override shape used by the adopter pages and by `TeletypeScriptViewPage` (added this session).

**Execution note:** Behavior-neutral — menu items, gesture, and `doubleClick` plumbing unchanged; only the call site moves into the virtual.

**Test scenarios:**
- Build sim + STM32 release clean.
- Manual smoke (sim): Tuesday's context menu opens on the same gesture with the same items; step keys and `pageModifier` step-guards behave unchanged.
- Test expectation: none — pure call-site move into the virtual.

**Verification:** Both targets build; Tuesday context menu opens identically; `contextShow()` is the single menu entry point; `keyPress` no longer inlines the `ContextMenu` construction.

---

## System-Wide Impact

- **Surfaces touched:** `BasePage` (new helper), 17 pages (two context-open blocks collapsed each), `TuesdayEditPage` (menu entry moved to a virtual). No engine, timing, or IO changes.
- **Risk:** one — wrongly pulling idiom 3 (the bare `pageModifier` return) into the helper would swallow `Page+Step`/`Page+QuickEdit`/`Page+Function` on the middle-branch pages. The design excludes idiom 3 from the helper specifically to avoid this; the U2 regression guard confirms each page's Page+Step still fires.
- **No binding changes:** purely deduplication of the two context-open idioms.

---

## Verification (whole plan)

- Sim (`make -C build/sim/debug sequencer`) and STM32 release (`make -C build/stm32/release sequencer`) build clean.
- On the sim, for each migrated page and Tuesday: context menu opens on the same gesture, page-double-press opens it in doubleClick mode where it did before, **Page+Step / quick-edit / function bindings still fire**, and Page+X navigation still bubbles to TopPage.
- Diffs show only the two context-open blocks collapsed (U2) and the Tuesday call-site move (U3) — idiom 3, Page+X handlers, and ListPage delegation untouched.
- No page-level unit-test harness exists; verification is build + targeted manual smoke (consistent with the TT2 page work this session).
