# tt2-track-copy-ui

## Goal
Give the native TT2 track reachable clipboard/copy operations in the UI, matching other Performer
tracks. TT2 tracks route the Track button to the TT2 I/O config page (not the Track page that hosts
copy/paste), so whole-track copy/paste was unreachable despite `ClipBoard` already supporting it.

## Key files
- `src/apps/sequencer/ui/pages/TT2IoConfigPage.cpp` — context menu (COPY/PASTE/TTLOAD/TTSAVE)
- `src/apps/sequencer/model/ClipBoard.cpp` — `copyTrack`/`pasteTrack` (already TT2-capable)
- `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp` — line-level copy only (`copyLine`/`pasteLine`)

## Decisions log
- 2026-06-21: Added COPY/PASTE to the TT2 I/O config context menu (Page+Shift / Tab) + renamed scene LOAD/SAVE → TTLOAD/TTSAVE; order COPY/PASTE/TTLOAD/TTSAVE; PASTE gated on `canPasteTrack`. Whole-track copy/paste now reachable for TT2 (was model-only). Commit `05d4e6e4`.
- 2026-06-21: TT2 script/pattern editor pages have NO context menu — master's `TeletypeScriptViewPage` had SAVE/LOAD Sc/T9 via Page+Shift; dropped in the native port. The I/O config page is the only TT2 context menu.

## Open questions
- [ ] Script-level copy missing: only line-level `copyLine`/`pasteLine` exist; no whole-script *clipboard* copy (slot→slot) or cross-track script copy. (Per-script *file* save/load now exists — see `tt2-script-save-load`.)
- [x] Restore a context menu on the script editor on Tab — done in `tt2-script-save-load` (SAVE Sc / LOAD Sc on Tab + Page+Shift).
- [x] TT2 I/O enum audit vs master TT1 — done (inputs complete; output-dest routing intentionally absent).

## Completed steps
- [x] COPY/PASTE/TTLOAD/TTSAVE on the TT2 I/O config context menu. Sim clean. Commit `05d4e6e4`.

## Notes
`ClipBoard::copyTrack`/`pasteTrack` already handle TT2 (full Track assignment + `setTrackMode`).
Cross-mode paste works — paste a copied TT2 track onto a Note track via that track's own paste.
