# tt2-pattern-shortcuts

## Goal
Port the original monome Teletype pattern-mode keyboard editing shortcuts onto the native TT2
pattern view: musical value nudges, digit transpose, and keyboard length/start/end editing.

## Key files
- `src/apps/sequencer/ui/pages/TeletypePatternViewPage.{cpp,h}` — keyboard handlers + field-edit mode
- `src/apps/sequencer/engine/TeletypeNativeOps.{cpp,h}` — `tt2TransposeSemitones` over `tt2EtTable`
- `src/tests/unit/sequencer/TestTT2Transpose.cpp` — transpose math test
- `docs/plans/2026-06-21-001-feat-tt2-pattern-shortcuts-plan.md` — the plan (completed)

## Decisions log
- 2026-06-21: Shipped all 6 units. Transpose is **cell-only** and uses **ET-quantize-and-shift** (verbatim `transpose_n_value` over the existing `tt2EtTable`), not a raw ±N — resolved from `master:teletype/module/pattern_mode.c`, not a user call. Bracket handler moved off `event.ch()` (nulled under Ctrl/Alt) to keycodes `0x2F`/`0x30` + modifiers. Digit transpose uses the original `0=10, 1=11` remap. `Alt+L/S/E` field-edit is a new sub-mode (the page had no len/start/end field editor); its indicator is a cleared-box overlay (collision-proof; not ui-previewed — there's no pattern-view renderer). Commits `e25bacd1` (U1, test-first), `9adc6e53` (U2-U5), `db0f6cd3` (U6).

## Open questions
- [ ] Runtime confirmation: the keyboard interactions are build-verified only (not unit-testable without the page/engine) — play in sim/hardware to confirm nudges/transpose/field-edit and the field-edit indicator position.

## Completed steps
- [x] U1 `tt2TransposeSemitones` (TDD, `TestTT2Transpose` green). `e25bacd1`
- [x] U2-U5 nudges (semitone/fifth/octave), digit transpose, `Shift+L/S/E` set-to-cursor. `9adc6e53`
- [x] U6 `Alt+L/S/E` numeric field-edit sub-mode + overlay indicator. `db0f6cd3`
- [x] Sim + STM32 release build clean.

## Notes
Map ported: `Alt+[ ]`=±1 semitone, `Ctrl+[ ]`=±fifth(7), `Shift+[ ]`=±octave(12); `Alt+0-9` up /
`Shift+Alt+0-9` down transpose; `Shift+L/S/E` set length/start/end to cursor; `Alt+L/S/E` numeric
field edit. Plain `[ ]`=±1 raw unchanged. Remaining original-only features (undo, Help mode,
live-mode `~`/dashboard/turtle views, multi-line script select/move) are still deferred.
