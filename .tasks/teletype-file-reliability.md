# teletype-file-reliability

## Goal
Improve the reliability of TeletypeTrack saving and loading in the Performer project. The primary goal is to resolve random crashes and silent save failures caused by a critical stack buffer overflow, while simultaneously streamlining the binary project format by removing backwards compatibility.

**Plan:** [Teletype File Reliability: Unified Architecture Spec](../docs/superpowers/specs/2026-05-12-teletype-shadow-binary-design.md)

## Key files
- `src/apps/sequencer/model/FileManager.cpp` — Implement Shadow Binary (`.T9B`) Save/Load logic and harden existing text parser (thread safety, strict parsing).
- `src/apps/sequencer/model/TeletypeTrack.cpp` — Implement `writeShadow()` and `readShadow()` for raw VM state dumping.

## Decisions log
- 2026-05-12: Discovered a 32-byte stack buffer overflow in `FileManager.cpp` when reconstructing Teletype scripts. This corrupts the File Task stack on large scripts. (Resolved/Verified: already fixed in codebase).
- 2026-05-12: User confirmed that breaking backward compatibility for `.PRO` files is acceptable. Decided to streamline the `TeletypeTrack::read` and `write` methods, removing the legacy slot `0` I/O mapping checks, allowing for a cleaner serialization stream.
- 2026-05-12: Finalized Unified Architecture Spec marrying Shadow Binary for projects with Text for libraries. Identified critical race condition in static global buffers `ttSlot1`/`ttSlot2` and silent failures in script parsing.

## Completed steps
- [x] Researched the T9type storage architecture and compared it with original Monome Teletype.
- [x] Identified the `buffer[32]` overflow in text export.

## Notes
- Original Monome uses internal MCU flash; T9type uses the central SD-based Performer binary state.
- Keep an eye on File Task stack usage (~2048 bytes).
