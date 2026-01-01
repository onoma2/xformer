# Teletype Hardware I/O Implementation Analysis

## Overview
Teletype interacts with hardware via a combination of interrupt-driven events (for Inputs) and direct function calls triggered by script commands (for Outputs).

## Inputs

### 1. Trigger Inputs (`TR`)
Trigger inputs are handled via hardware interrupts which post events to the main event loop.

*   **Interrupt Handler:** `handler_Trigger` in `teletype/module/main.c`.
*   **Mechanism:**
    *   Reads the physical pin state using `gpio_get_pin_value`.
    *   Checks the "Edge Polarity" configured for that input (`scene_state.variables.script_pol`).
    *   If the edge matches (Rising/Falling/Both), it calls `run_script` for the corresponding script index (1-8).
*   **Variables:**
    *   `TR x` command uses `op_TR_get` to return the current logical state of the trigger input.
    *   `STATE x` command uses `op_STATE_get` to return the raw physical state of the input pin via `tele_get_input_state`.

### 2. CV Inputs (`IN` / `PARAM`)
CV inputs are polled periodically via a timer.

*   **Timer:** `adcTimer` (61ms period) triggers `handler_PollADC` in `teletype/module/main.c`.
*   **Mechanism:**
    *   `adc_convert` reads the raw ADC values.
    *   `ss_set_in` and `ss_set_param` update `scene_state.variables.in` and `param` after applying calibration and scaling.
*   **Script Access:**
    *   `IN`: Accessed via `op_IN` -> `op_IN_get`, returns `scene_state.variables.in`.
    *   `PARAM`: Accessed via `op_PARAM` -> `op_PARAM_get`, returns `scene_state.variables.param`.

## Outputs

### 1. CV Outputs (`CV 1` - `CV 4`)
CV outputs are updated by script commands and processed by a background timer for slewing.

*   **Command:** `CV x y` triggers `op_CV_set` in `teletype/src/ops/hardware.c`.
*   **Mechanism:**
    *   Updates `scene_state.variables.cv[x]`.
    *   Calls `tele_cv(x, value, slew_enabled)`.
    *   `tele_cv` sets a target value (`aout[i].target`).
    *   `cvTimer_callback` (running every 6ms) handles the interpolation (slewing) from the current value to the target value and writes to the DAC via SPI.
*   **Slew:** `CV.SLEW` sets the slew rate, handled in `tele_cv_slew`.

### 2. Trigger/Gate Outputs (`TR 1` - `TR 4`)
Trigger outputs are manipulated directly by script commands.

*   **Command:** `TR x y` triggers `op_TR_set`.
*   **Mechanism:**
    *   Calls `tele_tr(x, value)`.
    *   `tele_tr` sets the GPIO pin high or low immediately.
*   **Pulse:** `TR.PULSE x` uses a software timer (`trPulseTimer`) to automatically turn the gate off after a configured duration (`TR.TIME`).

## Summary Table

| Hardware | Script Op | Implementation Function | Underlying Mechanism |
| :--- | :--- | :--- | :--- |
| **TR In 1-8** | N/A (Triggers Script) | `handler_Trigger` | GPIO Interrupt -> `run_script` |
| **CV In** | `IN` | `op_IN_get` | `adcTimer` -> `adc[0]` -> `ss.in` |
| **Knob** | `PARAM` | `op_PARAM_get` | `adcTimer` -> `adc[1]` -> `ss.param` |
| **CV Out 1-4** | `CV x y` | `op_CV_set` | `tele_cv` -> `cvTimer` (Slew) -> DAC SPI |
| **TR Out 1-4** | `TR x y` | `op_TR_set` | `tele_tr` -> GPIO Write |

## Code References

*   **Trigger Logic:** `teletype/module/main.c` (Handlers)
*   **Ops Implementation:** `teletype/src/ops/hardware.c`
*   **Hardware Abstraction:** `teletype/src/teletype.c` (tele_cv, tele_tr)
*   **State Storage:** `teletype/src/state.h` (`scene_state_t`)
