# ui-keypress-helper-extraction

## Goal
Dedupe the panel `keyPress` context-menu idioms across the UI pages into one `BasePage` helper,
the panel-side analog of the existing `handleFunctionKeys` keyboard fold.

## Key files
- `src/apps/sequencer/ui/pages/BasePage.{h,cpp}` — `handleContextMenuKey(KeyPressEvent&)`
- 17 adopter pages (see plan) — prologue collapsed to `if (handleContextMenuKey(event)) return;`
- `src/apps/sequencer/ui/pages/TuesdayEditPage.{cpp,h}` — `contextShow()` override (no helper adoption)
- `docs/plans/2026-06-22-001-refactor-basepage-keypress-prologue-plan.md` (completed)

## Decisions log
- 2026-06-22: Shipped via /ce-plan → /ce-work (tdd style → characterization-first; no page-test harness exists, so build + manual smoke per the plan). Helper covers **idioms 1+2 only** (`isContextMenu`→`contextShow`, Page double-press→`contextShow(true)`). The bare `pageModifier→return` (idiom 3) is **NOT** bundled — review caught that bundling it at the top would fire before each page's Page+Step/QuickEdit/Function branches and swallow them; it stays inline per page. Adopter set defined by a scan (every `*.cpp` whose `keyPress` opens with ungated idiom 1 → idiom 2), not a hand-list — three review rounds corrected misses (the config/util pages, DiscreteMapSequencePage whose clean opener is in `keyPress` while a guard sits in `keyDown`, GeneratorPage whose `ensureBoundTrackContext` guard must precede the helper). 17 adopters, 6 exclusions (gated idiom 1: PatternPage/SystemPage; idiom-1-only: PhaseFluxEditPage/Stochastic/TT2IoConfig/TeletypeScriptView). Tuesday gets the `contextShow()` convention only (lacks idiom 2). Commits `ac4d8409` (U1), `552665e3` (U2, −186 lines), `154535e2` (U3).
- 2026-06-22: RAM audit (vs `50910ada`): `.text` 575,572 → 575,020 (**−552 B flash**); `.data`/`.bss`/`.ccmram_bss` all **unchanged** (0). Code-only dedup — no page gained a member.

## Open questions
- [ ] Manual smoke on the sim: context menu opens + Page+Step/quick-edit/nav still fire on a couple migrated pages (build-verified only; no page-test harness).

## Completed steps
- [x] U1 `BasePage::handleContextMenuKey` (idioms 1+2). `ac4d8409`
- [x] U2 migrate 17 pages (−186 lines, +17). `552665e3`
- [x] U3 TuesdayEditPage `contextShow()` refactor. `154535e2`
- [x] Sim + STM32 release clean at every step; RAM audit flat, flash −552 B.
- [x] PROJECT.md convention note added (shared input helpers on BasePage).

## Notes
The bundling-vs-split distinction is the whole correctness story: idiom 3's position is page-specific
(a trailing catch-all after Page+X handlers), so only idioms 1+2 are safe to extract. Deferred
tier (separate refactor): `Page+Step → named context` helper, Performer/Pattern hold-to-close keyUp.
