# Teletype Porting Research

This document outlines the architecture of the monome Teletype codebase and provides a strategy for porting its scripting functionality to the "Performer" project.

## 1. Architecture Overview

The Teletype codebase is structured as a **stack-based interpreter** with a clean separation between parsing, execution logic, and hardware operations.

### Core Components

*   **Interpreter Loop** (`src/teletype.c`, `src/scanner.rl`, `src/command.c`):
    *   **Parsing**: Uses a Ragel-based state machine (`scanner.rl`) to tokenize input strings into a `tele_command_t` structure.
    *   **Execution**: The `process_command` function iterates through tokens (Right-To-Left for stack processing). It maintains a `command_state_t` (the data stack) and an `exec_state_t` (execution context for loops/recursion).
    *   **Dispatch**: Operations are looked up in a global `tele_ops` array and executed via function pointers (`.get` for values, `.set` for actions).

*   **Operation System** (`src/ops/`):
    *   Functionality is modular. Each file (e.g., `maths.c`, `controlflow.c`) defines a set of operations.
    *   **Mechanism**: Macros like `MAKE_GET_OP` and `MAKE_GET_SET_OP` define op metadata (name, parameter count, return value).

*   **State Management** (`src/state.h`):
    *   **`scene_state_t`**: A monolithic structure holding the entire "world" state: variables (A-Z), scripts, patterns, grid state, and hardware cache (CV/TR values).

## 2. Portability Assessment

| Component | Portability | Notes |
| :--- | :--- | :--- |
| **Interpreter Logic** | **High** | `teletype.c`, `command.c`, and `scanner.rl` are standard C. |
| **Math & Logic Ops** | **High** | `maths.c` (random, euclidian, logic) and `controlflow.c` are self-contained. |
| **Hardware Ops** | **Low** | `hardware.c` calls platform-specific functions (e.g., `tele_cv`, `tele_tr`) that must be re-implemented. |
| **State Struct** | **Medium** | `scene_state_t` is coupled to Teletype's specific feature set (4 CVs, 4 TRs, Grid). |

## 3. Implementation Strategy: "Boot to Teletype"

To free up memory from "Classic" Performer functionality, a **Shared Memory / Dual-Boot** architecture is recommended.

### A. Shared Memory Architecture (RAM Optimization)
Define a master union to ensure `ClassicState` and `TeletypeState` occupy the same physical memory address but never run simultaneously.

```c
typedef union {
    ClassicState_t classic;  // Existing heavy data structures
    scene_state_t teletype;  // Teletype's ~15KB+ state struct
} AppState_t;

// Allocate in the largest RAM section (e.g., DTCM or AXI SRAM)
AppState_t app_state; 
```

### B. Boot Selector
In the `main()` loop, check a condition (button press, persistent flag) to determine which "App" to initialize.

```c
typedef enum { APP_CLASSIC, APP_TELETYPE } AppMode_t;

int main(void) {
    Hardware_Init(); // Shared hardware init
    
    AppMode_t mode = Check_Boot_Button() ? APP_TELETYPE : APP_CLASSIC;

    if (mode == APP_TELETYPE) {
        Teletype_Init(&app_state.teletype); 
        while(1) Teletype_Loop();
    } else {
        Classic_Init(&app_state.classic);
        while(1) Classic_Loop();
    }
}
```

## 4. Hardware Abstraction Layer (HAL)

You must create a bridge between Teletype's logic and Performer's hardware drivers. Create a file `teletype_hal.c`:

```c
// Defines the boundary between Teletype logic and Performer Hardware

// From Teletype's hardware.h
void tele_cv(uint8_t channel, uint16_t value, uint8_t slew_flag) {
    // Teletype uses 0-16383 for 0-10V. Map to Performer's DAC.
    Performer_DAC_Write(channel, Map_Tele_To_Performer(value));
}

void tele_tr(uint8_t channel, uint8_t state) {
    // Map to Performer's Gate Outputs
    Performer_Gate_Set(channel, state);
}

int16_t ss_get_in(scene_state_t *ss) {
    // Read Performer's ADC
    return Performer_ADC_Read(0);
}
```

## 5. File Migration Plan

### Files to Copy
Move these files to a `src/teletype_core/` folder in Performer:
1.  **Core Logic**: `src/teletype.c`, `src/teletype.h`
2.  **Command Handling**: `src/command.c`, `src/command.h`
3.  **Scanner**: `src/scanner.c` (Generated from `scanner.rl` if Ragel is not available).
4.  **Math/Logic Ops**: `src/ops/maths.c`, `src/ops/controlflow.c`.
5.  **State**: `src/state.c`, `src/state.h`.

### Files to Re-implement or Exclude
*   `src/ops/hardware.c`: **Rewrite** using the HAL approach above.
*   `src/ops/grid.c`: Include only if Performer supports Monome Grid.
*   `src/ops/usb_disk_mode.c`: Exclude unless USB Host support is desired.
