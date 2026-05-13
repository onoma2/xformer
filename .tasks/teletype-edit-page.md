# teletype-edit-page

## Goal
Replace the empty TeletypeEditPage placeholder with a proper multi-mode configuration page using the ListModel pattern. Consolidates all Teletype-specific I/O mapping, CV calibration, and timing setup into one place — no more digging through LayoutPage + ScriptViewPage for config.

## Key files
- `src/apps/sequencer/ui/pages/TeletypeEditPage.h` — page header, rewrite as ListPage subclass
- `src/apps/sequencer/ui/pages/TeletypeEditPage.cpp` — page impl, multi-mode switching
- `src/apps/sequencer/ui/pages/Pages.h` — already registered (`teletypeEdit` member), no change needed
- `src/apps/sequencer/model/TeletypeTrack.h` — model has all accessors: `editTriggerInputSource()`, `editCvInSource()`, `editTriggerOutputDest()`, `editCvOutputDest()`, `activeSlot()`, etc.
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` — currently has the display-only I/O grid; stays untouched

### ListModels to create (in `src/apps/sequencer/ui/model/`)
- `TeletypeInputListModel.h` — 10 rows: 4× TriggerInputSource + 6× CvInputSource (IN, PARAM, X, Y, Z, T)
- `TeletypeOutputListModel.h` — 8 rows: 4× TriggerOutputDest + 4× CvOutputDest
- `TeletypeCvConfigListModel.h` — 4 rows per output: Range, Offset, Quantize, RootNote; PREV/NEXT to select output 1-4
- `TeletypeTimingListModel.h` — 5-6 rows: Timebase, ClockDiv, ClockMult, MIDI Source, Boot Script

## Decisions log
- 2026-05-13: Scoped to TeletypeEditPage only. LayoutPage stays untouched. TeletypeScriptViewPage stays focused on script editing.

## Open questions
- [ ] Copy-on-edit or direct edit? ClockSetupPage uses direct; MidiOutputPage uses copy-on-commit.
- [ ] Destructive commit of auto-assigned I/O? When user changes TI/TO mapping manually, track owns the mapping.
- [ ] CV CAL mode: show which output is selected in the header or footer?
- [ ] Should CV output selection persist or reset on page enter?

## Completed steps
- [ ] Study existing ListModel pattern (ClockSetupPage, SystemPage) — DONE
- [ ] Design multi-mode structure: INPUT / OUTPUT / CV / TIMING — DONE
- [ ] Create `TeletypeInputListModel.h`
- [ ] Create `TeletypeOutputListModel.h`
- [ ] Create `TeletypeCvConfigListModel.h`
- [ ] Create `TeletypeTimingListModel.h`
- [ ] Rewrite `TeletypeEditPage.h`
- [ ] Rewrite `TeletypeEditPage.cpp`
- [ ] Verify build compiles

## Notes
- Pattern follows `SystemPage` multi-mode approach: `setMode()` → `setListModel()` swaps active model
- Function keys: F1=INPUT, F2=OUTPUT, F3=CV, F4=TIMING
- Enums already support `edit*()` + `print*()` pattern used by ListModel
- CV CAL mode uses copy-on-edit for per-output config? Or direct edit on active slot?
