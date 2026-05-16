# usb-keyboard-function-keys-extraction

## Goal
Eliminate the duplicated F1-F5 `pressFunctionButton` dispatch switch block that appears in 17+ page `keyboard()` overrides. Extract into a single `BasePage::handleFunctionKeys()` helper so each page's `keyboard()` shrinks from ~12 lines to ~2 lines. Also evaluate secondary duplications (Enter/Escape → closeWithResult, Enter on row 0 → textInput.show) for similar treatment.

## Key files
- `src/apps/sequencer/ui/pages/BasePage.h` — add `handleFunctionKeys()` declaration
- `src/apps/sequencer/ui/pages/BasePage.cpp` — add `handleFunctionKeys()` implementation
- `src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp:1563` — Pattern A (F1-F5 → pb + BasePage fallback)
- `src/apps/sequencer/ui/pages/NoteSequenceEditPage.cpp:1128` — Pattern A
- `src/apps/sequencer/ui/pages/IndexedSequenceEditPage.cpp:2313` — Pattern A
- `src/apps/sequencer/ui/pages/DiscreteMapSequencePage.cpp:2004` — Pattern A
- `src/apps/sequencer/ui/pages/SongPage.cpp:420` — Pattern A
- `src/apps/sequencer/ui/pages/IndexedRouteConfigPage.cpp:351` — Pattern A
- `src/apps/sequencer/ui/pages/IndexedMathPage.cpp:557` — Pattern A
- `src/apps/sequencer/ui/pages/TuesdayEditPage.cpp:919` — Pattern A
- `src/apps/sequencer/ui/pages/MonitorPage.cpp:522` — Pattern A + F6
- `src/apps/sequencer/ui/pages/ModulatorPage.cpp:915` — Pattern A'
- `src/apps/sequencer/ui/pages/TrackPage.cpp:291` — Pattern B (F1-F5 → pb + ListPage fallback)
- `src/apps/sequencer/ui/pages/SystemPage.cpp:514` — Pattern B
- `src/apps/sequencer/ui/pages/RoutingPage.cpp:574` — Pattern B
- `src/apps/sequencer/ui/pages/LayoutPage.cpp:134` — Pattern B
- `src/apps/sequencer/ui/pages/PatternPage.cpp:386` — Pattern C (custom F1/F2 + pb for F3-F5)
- `src/apps/sequencer/ui/pages/PerformerPage.cpp:255` — Pattern C (custom F1/F2/F4 + pb for F3/F5)
- `src/apps/sequencer/ui/pages/ContextMenuPage.cpp:104` — separate F1-F5 → closeAndCallback variant
- `src/apps/sequencer/ui/pages/FileSelectPage.cpp:82` — custom Enter/Escape
- `src/apps/sequencer/ui/pages/ConfirmationPage.cpp:65` — custom Enter/Escape
- `src/apps/sequencer/ui/pages/TextInputPage.cpp:154` — custom text editor keyboard

## Decisions log
- *(empty until session starts)*

## Open questions
- [ ] Pattern C (PatternPage, PerformerPage) — can the custom F1/F2/F4 cases share structure with `handleFunctionKeys()` via callback or are they too unique?
- [ ] ContextMenuPage has its own F1-F5 → `closeAndCallback(i)` — worth a second helper?
- [ ] Enter/Escape → closeWithResult pattern — worth extracting? (only 3 pages)
- [ ] Should this task handle the `.tasks/usb-keyboard-manager-refactor.md` flat-file that already exists in `.tasks/`?
- [ ] RAM impact: ~0 B (no new state), but verify with STM32 release build

## Completed steps
- *(none yet)*

## Notes
Research session identified the pattern in AGENTS.md session on 2026-05-17. The 17 files with the F1-F5 dispatch block break into:
- Pattern A (10): switch with default:break, then BasePage::keyboard(event) after
- Pattern B (5): switch with default: ListPage::keyboard(event) inside
- Pattern C (2): custom F-key mix with pressFunctionButton
- ContextMenuPage: separate variant using closeAndCallback instead of pressFunctionButton
