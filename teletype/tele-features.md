# Teletype Additional Features Analysis

## 1. Control Flow Implementation
Teletype implements control flow using a "Post-Command" modification system.

*   **Mechanism:** `mod_IF`, `mod_LOOP`, etc., are "modifiers" that run *before* or *around* a command.
*   **Execution State (`exec_state_t`):** Maintains flags like `if_else_condition`, `while_continue`, `breaking`, `loop_iterator`.
*   **Key Ops (from `controlflow.c`):**
    *   `IF x: cmd`: Executes `cmd` only if `x` is non-zero. Sets `if_else_condition`.
    *   `ELIF x: cmd`: Executes `cmd` if previous `IF` failed and `x` is true.
    *   `ELSE: cmd`: Executes `cmd` if previous `IF`/`ELIF` failed.
    *   `L a b: cmd`: Loops from `a` to `b`, executing `cmd`. Updates an iterator variable `I`.
    *   `W x: cmd`: While loop. Executes `cmd` while `x` is true. Has a safety depth limit (`WHILE_DEPTH`) to prevent infinite lockups.
    *   `EVERY x: cmd`: Executes `cmd` every `x` times it is called.
    *   `SCRIPT x`: Runs another script (subroutine). Checks for recursion depth/overflow.

## 2. Grid Integration
Deep integration with Monome Grid is a core feature, effectively turning the Grid into a physical UI extension.

*   **Source:** `teletype/module/grid.c`
*   **Modes:**
    *   **Tracker:** Visualizes pattern data on the grid.
    *   **Live/Edit:** Uses grid for mode switching and editing shortcuts.
    *   **User Scripts:** Scripts can query grid state (`G.BTN`, `G.FDR`) and set LEDs (`G.LED`).
*   **Virtual Control:** Scripts can emulate grid presses (`G.KEY`) to trigger internal logic without physical hardware.
*   **Faders:** Support for "virtual faders" drawn on the grid, with smoothing/slewing logic (`grid_process_fader_slew`).

## 3. I2C Ecosystem (Teletype Bus)
Teletype acts as a master controller for other Monome/Whimsical Raps modules via I2C (II).

*   **Implementation:** `teletype/src/ops/i2c.c` & `teletype_io.h`
*   **Generic Ops:** `II` ops allow sending raw commands to any address.
*   **Dedicated Ops:** Specific ops files exist for deep integration:
    *   **Ansible:** `ops/ansible.c` (CV/Gate expansion, Kria/Meadow/Earthsea modes)
    *   **Just Friends:** `ops/justfriends.c` (Geode synth voice, run commands)
    *   **Whimsical Raps:** `ops/wslash*.c` (W/ synth interaction)
    *   **ER-301, Disting:** `ops/er301.c`, `ops/disting.c` (External module control)
*   **Mechanism:** `tele_ii_tx` sends data. Scripts can "fire and forget" or query values.

## 4. Operation Library Scope
The `teletype/src/ops` directory reveals a vast standard library beyond simple hardware I/O.

*   **Math:** `maths.c` (Add, Sub, Mul, Div, Mod, Abs, Sgn, Avg, Min, Max).
*   **Logic:** `maths.c` (AND, OR, XOR, Bitwise shifts/rotates).
*   **Randomness:** `seed.c` (PRNG with seeding, "drunk" walks, probability).
*   **Patterns:** `patterns.c` (Array manipulation, shifting, sorting).
*   **Stack:** `stack.c` (Push/Pop values for RPN-like logic).
*   **Variables:** `variables.c` (Internal state management).
*   **Music Theory:** `scale.c` (Quantization to scales/chords).
*   **Turtle:** `turtle.c` (2D "turtle graphics" pointer moving over the pattern data).

## 5. Keyboard Input
Keyboard input is processed via a lookup table and modifiers.

*   **Source:** `teletype/module/keyboard_helper.h` (and likely implementation in `main.c` or similar if `.c` is missing/merged).
*   **Handling:** Raw HID codes are translated. Modifiers (Ctrl, Alt, Shift) are tracked to trigger specific edit functions (Copy/Paste) vs text entry.

## Summary
Teletype is not just a trigger sequencer but a **complete VM environment** with a rich standard library, dedicated hardware drivers for the "Monome Ecosystem" (Grid + I2C modules), and a structured control flow language.
