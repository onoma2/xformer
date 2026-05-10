# Project Status

## Overview
This project develops firmware for the PER|FORMER/XFORMER sequencer, targeting STM32F405 hardware with a macOS/Linux simulator. The codebase consists of a hardware application and a simulator, with a clear separation between engine (runs tracks/clock/algorithms), model (holds project/tracks/settings/state), and UI (draws 128x64 LCD and handles keys/LEDs/MIDI).

## Current Work

### USB HID Keyboard Implementation
**Status:** In Progress
**Priority:** High

Efforts to add USB HID keyboard support have been undertaken in three branches, all failing with the symptom that the keyboard is never detected (connect callback never fires). The root cause analysis indicates the HID driver is never matched during enumeration.

**Current State (after changes):**
- **Present**: `#include "usbh_driver_hid.h"` in `src/platform/stm32/drivers/UsbH.cpp`
- **Present**: `&usbh_hid_driver` in the `device_drivers[]` array
- **Present**: Call to `hid_driver_init()` in `UsbH::init()`
- **Implemented**: HID event handling (connect/disconnect/message)

**Completed Steps:**
1. **Integrate the HID driver correctly** – **DONE** (header added, driver listed, initialized with callbacks).
2. **Implement HID event handling in `UsbH`** – **DONE** (connect/disconnect/message handlers added).

**Next Steps (if needed):**
3. If integration still fails, diagnose low-level USB issues using:
   - USB debug logging
   - Testing with low-speed and full-speed keyboards
   - Checking for PHY speed negotiation problems
   - Verifying HID descriptor parsing
   - Adding explicit PCDET clearing in the low-level driver
   - Verifying power sequencing and current supply
4. Verify end-to-end functionality: device detection, keypress decoding, and integration with existing MIDI and hub functionality.

**References:**
- Detailed history of attempts: `new_usb/usb-history.md`
- Solution outline: `new_usb/solution.md`
- Task breakdown: `.tasks/usb-hid-implementation/TASK.md`

### Launchpad Controller Extension
**Status:** Planning
**Priority:** Medium

Effort to extend Launchpad controller support to all six track types (Note, Curve, Indexed, DiscreteMap, Tuesday, Teletype) is in the planning phase. The work involves fixing regressions in the Note track, adding macro grid functionality, and implementing full grid-based editing for each track type.

**References:**
- Detailed plan: `.tasks/launchpad-track-port/TASK.md`

## Resource Footprint (Current Release Build)
- **Flash:** `.text` ~413 KB (≈39% of 1 MB), ample headroom.
- **SRAM (128 KB):** `.data` 6 KB + `.bss` ~110 KB → ~15 KB free.
  - Largest blocks: `Model` (~93 KB), LCD driver buffer (8 KB).
- **CCM RAM (64 KB):** `.ccmram_bss` ~44.7 KB → ~21 KB free.
  - Largest blocks: `Ui` (~22.9 KB), FreeRTOS static stacks, `Engine` (~6.5 KB).

## Recent Developments
No recent commits to report; the above outlines planned work.

## Next Steps
1. Begin implementation of USB HID keyboard support by integrating the driver into the main branch.
2. Following USB work, proceed with Launchpad controller extensions as per the staged plan.
