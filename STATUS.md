# Project Status

## Overview
This project develops firmware for the PER|FORMER/XFORMER sequencer, targeting STM32F405 hardware with a macOS/Linux simulator. The codebase consists of a hardware application and a simulator, with a clear separation between engine (runs tracks/clock/algorithms), model (holds project/tracks/settings/state), and UI (draws 128x64 LCD and handles keys/LEDs/MIDI).

## Current Work

### USB HID Keyboard Implementation
**Status:** Active, keyboard keypresses verified end-to-end
**Priority:** High

USB HID driver is working end-to-end: keyboard and mouse enumerate correctly, keypresses arrive via interrupt IN polling and display on OLED. Seven bugs were found and fixed across two sessions. A regression from debug callbacks was identified and reverted. Key events use a safe ring buffer pattern (no UI callbacks during USB state machine).

**Completed:**
- HID driver integration (header, driver list, init, callbacks)
- 7 bugs fixed (inverted logic x2, missing interface_number, layer violation, blocking delay, g_usbh ordering, debug callback regression)
- Key event ring buffer (enqueue in USB task, dequeue in UI task)
- Keyboard keypresses verified: `K:04 M:00` (keycode 0x04 = 'a') on OLED
- MIDI Launchpad works with no regression

**Next Steps:**
1. Design `KeyboardController` class (similar to `LaunchpadController`) for sequencer UI integration
2. Map QWERTY keys to sequencer actions
3. Add SET_IDLE(0) and SET_PROTOCOL(boot) for reliable keyboard operation
4. Fix GET_REPORT_DESCRIPTOR `wIndex` to use `interface_number`

**References:**
- `.tasks/usb-hid-implementation/TASK.md`

### Launchpad Controller Extension
**Status:** Planning
**Priority:** Medium

**References:**
- `.tasks/launchpad-track-port/TASK.md`

## Resource Footprint (Current Release Build)
- **Flash:** `.text` ~413 KB (≈39% of 1 MB), ample headroom.
- **SRAM (128 KB):** `.data` 6 KB + `.bss` ~110 KB → ~15 KB free.
- **CCM RAM (64 KB):** `.ccmram_bss` ~44.7 KB → ~21 KB free.