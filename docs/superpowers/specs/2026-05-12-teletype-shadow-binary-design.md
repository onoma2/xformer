# Teletype File Reliability: Unified Architecture Spec
**Date**: 2026-05-12
**Topic**: Teletype Track File Serialization & Library Management

## 1. Goal
Provide a robust, dual-path architecture for Teletype file management. 
1. **Automatic Project Persistence**: Untangle the massive Teletype virtual machine state from the core Performer project serialization (`.PRO` files) using a Shadow Binary (`.T9B`). This prevents stream corruption and memory faults during automated save/load cycles.
2. **Manual Library Curation**: Preserve and clarify the existing user-facing UI actions (`SAVE SCRIPT` and `SAVE TT`) which export human-readable text files (`.TXT`) for manual hacking, sharing, and library building.

## 2. Architecture: The Split Model

### 2.1 The Shadow Binary (Automatic Project State)
Instead of serializing heavy Teletype structs directly inside the `Project::write` payload, `FileManager` orchestrates a parallel binary save/load process.

*   **`FileManager::writeProject`**: After handling the primary `.PRO` file, it checks if any track is a `TeletypeTrack`. If true, it opens a parallel file (e.g., `PROJECTS/001.T9B`) and delegates serialization.
*   **`TeletypeTrack::writeShadow` / `readShadow`**: New methods specifically designed to read/write the `scene_state_t` and `_patternSlots` arrays as raw, contiguous binary data. This is the fastest, safest method for the MCU and bypasses the fragile `ProjectVersion` stream.
*   **Data Flow**: User presses "Save Project" -> `.PRO` is written (skipping heavy TT data) -> `.T9B` is written silently in the background.

### 2.2 The Text Library (Manual User State)
The UI provides explicit actions for users to manage their Teletype code independently of the sequencer project.

*   **Single Script Export (`SAVE SCRIPT` / `LOAD SCRIPT`)**:
    *   Triggered from `TeletypeScriptViewPage`.
    *   Calls `FileManager::writeTeletypeScript()`.
    *   Outputs `TELS/*.TXT`: A plain text file containing exactly one script (e.g., Script 1).
*   **Full Track Export (`SAVE TT` / `LOAD TT`)**:
    *   Triggered from `TeletypePatternViewPage`.
    *   Calls `FileManager::writeTeletypeTrack()`.
    *   Outputs `TELT/*.TXT`: A comprehensive text file containing the *entire* track state (Global scripts 1-3, plus both A and B PatternSlots including their local scripts, metros, P-patterns, and I/O routing).

## 3. Error Handling & Edge Cases
*   **Missing `.T9B` File**: If a user shares a `.PRO` file without its matching `.T9B` shadow file, the sequencer must not crash. `FileManager` catches the `FILE_NOT_FOUND` error and initializes the `TeletypeTrack` to a blank default state.
*   **Text File Parsing**: If a user manually edits a `TELT/*.TXT` file on their computer and introduces a syntax error, `FileManager::readTeletypeTrack` will safely reject the invalid line and preserve the rest of the valid state, without causing an MCU hard fault.
*   **Stack Overflows**: By isolating the read/write logic into dedicated `Shadow` methods, we avoid deeply nested stack allocation inside `VersionedSerializedWriter`. Buffer boundaries will be strictly clamped during `.T9B` read operations.

## 4. Testing Strategy
*   **Simulator (`build/sim/debug`)**: Mandatory first verification.
*   **Shadow Binary Test**: Save a project with a Teletype track -> verify both `.PRO` and `.T9B` are created. Delete `.T9B` -> verify project loads safely with a blank Teletype track.
*   **Text Library Test**: Ensure `SAVE SCRIPT` and `SAVE TT` still produce valid, readable `.TXT` files that can be loaded back into a blank project successfully.
