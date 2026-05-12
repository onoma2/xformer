# teletype-file-reliability

## Goal
Improve the reliability of TeletypeTrack saving and loading in the Performer project. The primary goal is to resolve random crashes and silent save failures caused by a critical stack buffer overflow, while simultaneously streamlining the binary project format by removing backwards compatibility.

**Plan:** [2026-05-12 Teletype Save/Load Fixes](../docs/plans/2026-05-12-teletype-save-load-fixes.md)

## Key files
- `src/apps/sequencer/model/FileManager.cpp` — Fix the `char buffer[32]` overflow in `writeScriptSection` and `writeScriptSectionRaw`.
- `src/apps/sequencer/model/TeletypeTrack.cpp` — Streamline the `read()` and `write()` binary serialization functions to drop old legacy data mapping.

## Decisions log
- 2026-05-12: Discovered a 32-byte stack buffer overflow in `FileManager.cpp` when reconstructing Teletype scripts. This corrupts the File Task stack on large scripts.
- 2026-05-12: User confirmed that breaking backward compatibility for `.PRO` files is acceptable. Decided to streamline the `TeletypeTrack::read` and `write` methods, removing the legacy slot `0` I/O mapping checks, allowing for a cleaner serialization stream.

## Open questions
- [ ] Should we bump the global project version enum in `ProjectVersion.h` to reflect the binary format change?

## Completed steps
- [x] Researched the T9type storage architecture and compared it with original Monome Teletype.
- [x] Identified the `buffer[32]` overflow in text export.

## Notes
- Original Monome uses internal MCU flash; T9type uses the central SD-based Performer binary state.
- Keep an eye on File Task stack usage (~2048 bytes).
