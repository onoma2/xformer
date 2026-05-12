# teletype-file-reliability

## Goal
Fix the three real bugs in TeletypeTrack file saving/loading: redundant legacy I/O in binary serialization, silent parse failures in the text track loader, and no rollback when a text load fails (wipes the working track).

**Governing spec:** [Teletype File Saving: Actual Problems & Minimal Fixes](../docs/superpowers/specs/2026-05-12-teletype-saving-reality-check.md)
**Supersedes:** `docs/superpowers/specs/2026-05-12-teletype-shadow-binary-design.md` (Shadow Binary was debunked by graphify audit — no stack overflows, no race conditions, no stream corruption exist)

## Key files
- `src/apps/sequencer/model/TeletypeTrack.cpp` — Phase 0: streamline binary write/read (remove legacy I/O block)
- `src/apps/sequencer/model/FileManager.cpp` — Phase 1: add strict parsing to `readTeletypeTrack()`. Phase 2: add snapshot/rollback.
- `src/apps/sequencer/ui/pages/TeletypePatternViewPage.cpp` — Phase 1: wire `fs::INVALID_DATA` to "INVALID TRACK FILE" message

## Decisions log
- 2026-05-12: Discovered a 32-byte stack buffer overflow in `FileManager.cpp` when reconstructing Teletype scripts. This corrupts the File Task stack on large scripts. (Resolved/Verified: already fixed in codebase).
- 2026-05-12: User confirmed that breaking backward compatibility for `.PRO` files is acceptable. Decided to streamline the `TeletypeTrack::read` and `write` methods, removing the legacy slot `0` I/O mapping checks, allowing for a cleaner serialization stream.
- 2026-05-12: Graphify audit (17,340-node graph) debunked Shadow Binary spec. No evidence of stream corruption, stack overflow, or RTOS race conditions. Real problems are: (1) ~46 bytes redundant legacy I/O in binary write, (2) `readTeletypeTrack` silently drops bad lines, (3) no rollback on failed text load. New spec: `2026-05-12-teletype-saving-reality-check.md`.
- 2026-05-12: Shadow Binary `.T9B` rejected. `scene_state_t` contains pointers and can't be raw-dumped. FNV hash already catches binary corruption. Cooperative single-task model means no race conditions on `ttSlot1`/`ttSlot2`.

## Open questions
- [ ] Should `#S4P1`/`#S4P2` text labels be renamed? Deferred — breaking change for existing user files, not a correctness issue.

## Completed steps
- [x] Researched the T9type storage architecture and compared it with original Monome Teletype.
- [x] Identified the `buffer[32]` overflow in text export.
- [x] Graphify audit mapped full save/load call chains through 17,340-node codebase graph.
- [x] Debunked Shadow Binary spec claims (no stack overflow, no race condition, no stream corruption).
- [x] Wrote reality-check spec identifying 3 real problems with phased fix plan.
- [x] Phase 0: Removed legacy I/O block from `TeletypeTrack::write()` and `read()`. Hardware verified — save/load round-trips correctly with minimal scripts.
- [x] Phase 1: Added strict parsing to `readTeletypeTrack()` — tracks `success`, returns `fs::INVALID_DATA` on parse failure. UI shows "INVALID TRACK FILE". New `INVALID_DATA` error code added to `fs::Error`. Hardware verified.
- [x] Phase 2: Added snapshot/rollback to `readTeletypeTrack()` and `readTeletypeScript()`. Failed load restores previous track/script state instead of wiping. Hardware verified.

## Notes
- Phase 0 saved ~46 bytes per project file by eliminating redundant legacy I/O serialization.
- Binary payload is ~4.5KB total (TeletypeTrack). Tiny relative to full project.
- `ttSlot1`/`ttSlot2` are anonymous-namespace globals, not static. Protected by cooperative single-task model — no mutex needed.
- `readTeletypeScript` already has `success` tracking and returns `INVALID_CHECKSUM` on parse failure. `readTeletypeTrack` does not — this is the asymmetry Phase 1 fixes.
- Existing plan in `docs/plans/2026-05-12-teletype-save-load-fixes.md` is Phase 0 of this task — already has code provided.
