# usb-keyboard-sim-parity

## Goal
Make the hardware USB keyboard reproduce the simulator's panel keymap wherever a USB-HID keyboard
can physically express it. The sim's SDL keymap is the reference; the sim keeps its extras (mouse,
scroll-to-rotate, F10-F12 fake jacks). Two input systems stay separate — this is parity, not a
shared code path. Split into a **Match bucket** (key→control assignment that changes on hardware)
and a **Keep bucket** (keyboard-only gap-fillers that stay untouched).

## Key files
- `src/apps/sequencer/ui/KeyboardManager.cpp` — `hidKeycodeToButton()` decode + `process()` dispatch
- `src/apps/sequencer/ui/KeyboardManager.h` — decode declaration
- `src/apps/sequencer/ui/MatrixMap.h` — `fromStep`/`fromTrack` key-code math (the target codes)
- `src/platform/sim/sim/frontend/Frontend.cpp:181` — the reference SDL keymap (`keys[info.index]`)
- `src/tests/unit/sequencer/TestKeyboardMapping.cpp` — mapping test (7 cases)
- `docs/plans/keyboard-binding-inventory.md` — full Match/Keep plan + per-page binding inventory

## Decisions log
- 2026-06-21: TT2 editor track-access gap closed. On the Teletype script/pattern pages the step-key gate is off, so plain Q-I types and track-select was unreachable from the keyboard. Added `Alt + Q-I` → `setSelectedTrackIndex` in both pages' `keyboard()` Alt branch (checked ahead of Alt+F/Alt+/). Switching to a non-TT2 track auto-closes the editor via the existing `draw()` guard. New `hidKeycodeToTrack` helper (derives from `hidKeycodeToButton`), TDD'd. Scoped to the editor only — elsewhere Q-I selects tracks without Alt. Commit `75b62d15`.
- 2026-06-21: Match bucket implemented TDD-first — `hidKeycodeToButton` returns the final Key code; `process()` dispatches it directly; dead `hidKeycodeToStep` retired. Commit `2175c5bd`.
- 2026-06-21: Encoder press is a Key code (`Key::Encoder=40`, dispatched like any button per `Ui.cpp:174-176`), so every Match item collapses into one HID→Key-code table — no special synthesis. `Space` is just a table entry.
- 2026-06-21: `Alt`→Page (sim) deliberately NOT adopted — it would collide with the `Alt`-escape-to-typing in the Keep bucket. Hardware Page stays on `Ctrl`. Accepted as "not possible without touching Keep."
- 2026-06-21: Panel-faithful layout chosen as canonical (owner pick): Q-I→tracks, A-K→steps 1-8, Z-row→steps 9-16 — mirrors the physical panel. Hardware moves to match the sim, not vice-versa.

## Open questions
- [ ] Hardware audition — flash and confirm the new mappings (tracks / modes / space) on a real unit + USB keyboard.
- [ ] TT2 context menu regression: master's `TeletypeScriptViewPage` had a context menu (SAVE Sc / LOAD Sc / SAVE T9 / LOAD T9 via Page+Shift) that the native port dropped. User wants it restored and opened on `Tab` (Page+Shift is taken by editing combos on native). Not yet done.

## Completed steps
- [x] Built the full keyboard-binding inventory (`docs/plans/keyboard-binding-inventory.md`), Match vs Keep buckets.
- [x] `TestKeyboardMapping.cpp` — RED then GREEN for the full Match table (q/a/z rows, 1-4, space, unmapped → -1).
- [x] `hidKeycodeToButton` implemented; `process()` wired to it; `hidKeycodeToStep` removed.
- [x] Sim build clean; mapping test green. Commit `2175c5bd`.
- [x] TT2 editor: `Alt + Q-I` → track select (`hidKeycodeToTrack`, both TT2 pages, TDD). Commit `75b62d15`.

## Notes
Keep bucket (untouched by design): `Tab`→context menu, `Up`/`Down`→encoder rotate, `Left`/`Right`,
Shift/Page modifier folding, the `Alt`-escape, the `mapStepKeys` text gate, and every per-page
`keyboard()` handler. Predecessor cleanup: `usb-keyboard-function-keys-extraction` (handleFunctionKeys fold).
