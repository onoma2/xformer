# Teletype File Reliability: Shadow Binary Design Spec
**Date**: 2026-05-12
**Topic**: Teletype Track File Serialization Refactor

## 1. Goal
Untangle the massive Teletype virtual machine state (Scripts, Patterns, Pattern Slots) from the core Performer project serialization loop (`.PRO` files). By isolating Teletype data into its own parallel binary file, we prevent stream corruption, eliminate `ProjectVersion` thrashing for non-Teletype users, and ensure safe, robust serialization.

## 2. Architecture (Approach A: Shadow Binary)
Instead of serializing the heavy Teletype structs directly inside the `Project::write` / `Project::read` payload, `FileManager` will orchestrate a parallel save/load process.

### 2.1 Components
*   **`FileManager`**: Modified `writeProject` and `readProject` methods. After handling the primary `.PRO` file, `FileManager` will check if any track is a `TeletypeTrack`. If true, it opens a parallel file (e.g., `PROJECTS/001.T9B`) and delegates serialization to the track.
*   **`TeletypeTrack`**: The existing `write()` and `read()` methods (called by `Project` via `VersionedSerializedWriter`) will be heavily stripped down. They will now *only* serialize lightweight configuration (like I/O routing or active track states).
*   **`TeletypeTrack::writeShadow` / `readShadow`**: New methods designed specifically to read/write the massive `scene_state_t` and `_patternSlots` arrays using a direct file stream (or a dedicated serializer), totally bypassing the primary `ProjectVersion` stream.

## 3. Data Flow
### Save Flow:
1. User presses "Save Project".
2. `FileManager::writeProject` handles the main `.PRO` serialization.
3. `TeletypeTrack::write` writes 0-2 bytes to the `.PRO` stream (e.g., just confirming the track mode).
4. `FileManager::writeProject` concludes the `.PRO` file.
5. `FileManager` detects Teletype usage, creates `PROJECTS/001.T9B`.
6. `FileManager` calls `TeletypeTrack::writeShadow(writer)`.
7. `TeletypeTrack` writes all scripts and pattern slots to `.T9B`.

### Load Flow:
1. User presses "Load Project".
2. `FileManager::readProject` loads `.PRO`.
3. `TeletypeTrack::read` parses its minimal data.
4. `FileManager` looks for `PROJECTS/001.T9B`.
    *   **Success**: Calls `TeletypeTrack::readShadow(reader)`.
    *   **Failure (File Missing)**: Gracefully initializes `TeletypeTrack` to a blank default state (no crash).

## 4. Error Handling & Edge Cases
*   **Missing `.T9B` File**: If a user shares a `.PRO` file without its matching `.T9B` shadow file, the sequencer must not crash. `FileManager` will catch the `FILE_NOT_FOUND` error and `TeletypeTrack` will automatically call `clear()` to instantiate a blank, functional script environment.
*   **Stack Overflows**: By isolating the read/write logic into dedicated `Shadow` methods, we avoid deeply nested stack allocation inside `VersionedSerializedWriter`. Buffer boundaries will be strictly clamped during `.T9B` read operations to prevent memory corruption.

## 5. Testing Strategy
*   **Simulator (`build/sim/debug`)**: Mandatory first verification.
*   **Save Cycle Verification**: Save a project with a Teletype track -> verify both `.PRO` and `.T9B` are created on the virtual SD card.
*   **Load Cycle Verification**: Close simulator, reopen, load project -> verify scripts and patterns restore correctly.
*   **Fault Injection**: Delete the `.T9B` file from the virtual SD card, load the `.PRO` project -> verify Performer continues running safely with a blank Teletype track.

## 6. Discarded Approaches
During brainstorming, two other approaches were considered and explicitly rejected:
*   **Approach B (Auto-Folder with Text Files)**: Exporting the project as a folder containing `project.pro` alongside individual `.txt` script files. *Discarded due to severe SD card latency risks when opening/writing/closing 20+ small text files during a performance-critical save operation.*
*   **Approach C (Flattened Internal Binary)**: Keeping the data inside the `.PRO` file but dumping raw memory blocks. *Discarded due to extreme fragility; any future struct changes would irreversibly corrupt all legacy `.PRO` files via stream misalignment.*