# tt2-script-save-load

## Goal
Per-script file save/load on the native TT2 track — save/load a single script slot to/from an SD
file — plus restoring the script-editor context menu the native port had dropped.

## Key files
- `src/apps/sequencer/engine/TT2SceneSerializer.{h,cpp}` — `tt2SerializeScript`/`tt2DeserializeScript`
- `src/apps/sequencer/model/FileDefs.h`, `FileManager.{h,cpp}` — `FileType::TeletypeV2Script` + `writeTt2Script`/`readTt2Script`
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.{cpp,h}` — context menu (SAVE Sc/LOAD Sc), Tab/Page+Shift openers
- `src/tests/unit/sequencer/TestTT2ScriptSerializer.cpp`

## Decisions log
- 2026-06-21: Shipped. Per-script files are TEXT (like scenes): `tt2SerializeScript` prints one slot's command lines via `tt2PrintCommand`; `tt2DeserializeScript` reads + `loadScriptText`, replacing only that slot. `FileType::TeletypeV2Script` (TT2SC/.TXT). FileManager methods mirror the scene path with a single-slot atomic swap on clean parse (via `gTeletypeLoadScratch`). UI: restored `contextShow` on `TeletypeScriptViewPage` (SAVE Sc / LOAD Sc), opened on **Tab** (replaces insert-space) AND **Page+Shift**; save/load mirror the scene flow (fileSelect + confirmation + suspend/busy/resume). Commits `2a8a44fb` (TDD) + `b9b8e812` (UI).
- 2026-06-21: Tab no longer inserts a space in the script editor (owner-approved) — it opens the menu.

## Open questions
- [ ] Runtime confirmation: the serializer round-trip is unit-tested, but the SD write/read + menu flow are build-verified only — save-then-load on sim/hardware to confirm.

## Completed steps
- [x] `tt2SerializeScript`/`tt2DeserializeScript` + FileType + FileManager I/O (TDD, `TestTT2ScriptSerializer` green). `2a8a44fb`
- [x] Script-editor context menu SAVE Sc / LOAD Sc on Tab + Page+Shift. `b9b8e812`
- [x] Sim + STM32 release clean.

## Notes
Restores the script-editor context menu master had (the native port dropped it). Distinct from a
clipboard slot→slot script copy (that's still open under `tt2-track-copy-ui`). Reuses the proven
scene file-I/O path (`FileManager` + `tt2*Scene` callbacks).
