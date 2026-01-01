# Teletype Script Management Analysis

## Overview
Teletype's script management involves editing scripts using a line editor, saving them to flash memory, and real-time interaction via keyboard shortcuts.

## Script Structure
*   **Storage:** Scripts are stored in `scene_state.scripts`, which is an array of `scene_script_t` structures.
*   **Capacity:** 
    *   `TOTAL_SCRIPT_COUNT` includes 8 Trigger scripts (1-8), Metro (`M`), Init (`I`), and potentially others.
    *   Each script has a fixed number of lines (`SCRIPT_MAX_COMMANDS`, typically 6).
*   **Representation:** Each line is a `tele_command_t` struct, containing tokenized data (ops, values) rather than raw text, though text is reconstructed for display.

## Editing Scripts

### 1. Edit Mode (`M_EDIT`)
*   **Source:** `teletype/module/edit_mode.c`
*   **Activation:** `set_edit_mode_script(script_index)` switches the UI to edit mode for a specific script.
*   **Line Editor:** Uses `line_editor_t` (implemented in `teletype/module/line_editor.c`) to handle text manipulation for the currently selected line.
    *   Supports standard navigation (Left/Right/Home/End) and editing (Backspace/Delete).
    *   Supports Cut/Copy/Paste (Ctrl-X/C/V) using a global `copy_buffer`.
    *   Supports Undo/Redo (Ctrl-Z) with a history depth of 3.

### 2. Input Processing
*   **Keyboard Handling:** `process_edit_keys` in `edit_mode.c` intercepts keystrokes.
    *   **Navigation:** Up/Down arrows change the selected line (`line_no1`).
    *   **Entry:** Enter key triggers parsing.
*   **Parsing:**
    *   When a line is entered, it is parsed via `parse()` (Ragel-generated scanner).
    *   If valid (`E_OK`), the command is stored in `scene_state` via `ss_overwrite_script_command`.
    *   If invalid, an error message is displayed.

## Execution
*   **Real-time:** Scripts are executed immediately upon trigger (see `tele-io.md`).
*   **Concurrency:** Scripts run on the main thread. Long loops or complex logic can block the UI/System tick, though the system is generally fast enough for this not to be a major issue.

## Storage (Flash)

### 1. Scene Serialization
*   **Source:** `teletype/src/scene_serialization.c`
*   **Format:** Text-based format.
    *   Starts with Scene Description text.
    *   Scripts are marked with `#` followed by the ID (1-8, M, I).
    *   Each line of the script is written as plain text.
    *   Patterns and Grid data follow.
*   **Saving:** `serialize_scene` converts the binary `scene_state` (including tokenized commands reconstructed to text) into a text stream.
*   **Loading:** `deserialize_scene` reads the text stream, parses each line back into `tele_command_t`, and populates `scene_state`.

### 2. Flash Management
*   **Source:** `teletype/module/flash.c`
*   **Storage:** Uses the microcontroller's internal flash or NVRAM.
*   **Structure:** `nvram_data_t` holds an array of scenes (`SCENE_SLOTS`).
*   **Operations:**
    *   `flash_write`: Serializes the scene and writes to the flash sector.
    *   `flash_read`: Reads from flash and deserializes into `scene_state`.

## Summary Table

| Feature | Implementation Function | Source File |
| :--- | :--- | :--- |
| **Edit Mode** | `process_edit_keys` | `teletype/module/edit_mode.c` |
| **Line Editing** | `line_editor_process_keys` | `teletype/module/line_editor.c` |
| **Parsing** | `parse` | `teletype/src/teletype.c` (wraps scanner) |
| **Serialization** | `serialize_scene` | `teletype/src/scene_serialization.c` |
| **Flash Write** | `flash_write` | `teletype/module/flash.c` |
