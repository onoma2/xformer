# Teletype UI Screens and Structures

## Overview
Teletype features several distinct UI modes, each serving a specific purpose in the workflow. The screens are rendered using a text-based system on a graphical display, managed by `scene_state` structures.

## UI Modes (Screens)

### 1. Live Mode (`M_LIVE`)
The default operational mode.
*   **Sub-modes:**
    *   **Live (Default):** Displays command history and a command entry line.
    *   **Grid (`SUB_MODE_GRID`):** Visualizes the Grid state (if connected).
    *   **Tracker (`SUB_MODE_TRACKER`):** *Mentioned in some contexts, but likely part of Pattern mode.*
    *   **Dash (`SUB_MODE_DASH`):** A customizable dashboard displaying variable values (`A`, `B`, `X`, `Y`, etc.) or user text.
    *   **Vars (`SUB_MODE_VARS`):** Dedicated view for system variables.
*   **Input:** Accepts immediate commands via keyboard (repl-style) which are executed but not saved to scripts unless explicitly done so.
*   **Implementation:** `teletype/module/live_mode.c`

### 2. Edit Mode (`M_EDIT`)
Used for writing and modifying scripts.
*   **Structure:** Displays the lines of the currently selected script (1-8, M, I).
*   **Input:** Standard text editing controls (arrows, backspace, copy/paste) managed by `line_editor.c`.
*   **Implementation:** `teletype/module/edit_mode.c`

### 3. Pattern Mode (`M_PATTERN`)
A tracker-like interface for the 4 internal pattern arrays.
*   **Visuals:** Displays 4 columns of data (patterns 0-3).
*   **Navigation:** Arrow keys move the cursor.
*   **Editing:** Values can be changed numerically, nudged, or computed.
*   **Implementation:** `teletype/module/pattern_mode.c`

### 4. Preset Read/Write (`M_PRESET_R`, `M_PRESET_W`)
Interfaces for loading and saving Scenes (projects).
*   **Read:** Browses stored scenes in flash memory to load.
*   **Write:** Allows entering a description and saving the current state to a flash slot.
*   **Implementation:** `teletype/module/preset_r_mode.c`, `teletype/module/preset_w_mode.c`

### 5. Help Mode (`M_HELP`)
Displays reference documentation.
*   **Implementation:** `teletype/module/help_mode.c`

## Underlying Structures

### Scene State (`scene_state_t`)
The central data structure holding all session data. Defined in `teletype/src/state.h`.

*   **Variables (`scene_variables_t`):**
    *   `a, b, c, d`: General purpose registers.
    *   `x, y, z, t`: General purpose registers.
    *   `cv[4]`: CV output values.
    *   `tr[4]`: Trigger output states.
    *   `m`: Metronome interval.
*   **Scripts (`scene_script_t`):**
    *   Array of `tele_command_t` (tokenized lines).
    *   Each script corresponds to a trigger input or special event.
*   **Patterns (`scene_pattern_t`):**
    *   4 independent patterns.
    *   Each has `val[64]` (array of values), `len` (length), `start`, `end`, etc.
    *   Used by the tracker interface.
*   **Grid (`scene_grid_t`):**
    *   Stores state for connected Monome Grid devices.

### Display Rendering
*   **Buffer:** `region line[8]` (global in `main.c`).
*   **Mechanism:** Each mode updates this buffer. The system then renders these regions to the hardware display.
*   **Dirty Flags:** Extensive use of bitmasks (`dirty`, `D_ALL`, `D_INPUT`, etc.) to optimize repainting only changed areas.

## Code References
*   **Mode Enumeration:** `teletype/module/globals.h` (`tele_mode_t`)
*   **State Definition:** `teletype/src/state.h`
*   **Mode Implementations:** `teletype/module/*_mode.c`
