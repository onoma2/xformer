# USB HID Keyboard Implementation — Task Status

Status: Active — investigating `analyze_descriptor()` failure
Priority: High

## Overview

Add USB HID keyboard support to the PER|FORMER/XFORMER sequencer firmware. Currently the master branch lacks HID driver registration and initialization, causing keyboards to never be detected. Three previous attempts failed with the same symptom (connect callback never fires) due to low‑level USB issues and missing driver integration.

## Key Files

| File | Role |
|------|------|
| `src/platform/stm32/drivers/UsbH.cpp` | Main USB host initialization, driver registration |
| `src/platform/stm32/drivers/UsbH.h` | UsbH class definition, HID device tracking |
| `src/platform/stm32/libs/libusbhost/include/usbh_driver_hid.h` | HID driver header |
| `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c` | HID driver implementation |
| `src/platform/stm32/libs/libusbhost/src/usbh_core.c` | USB host core (device enumeration, driver matching) |
| `src/platform/stm32/libs/libusbhost/include/usbh_config.h` | USB host configuration (enable debug logging) |
| `src/apps/sequencer/ui/MessageManager.h` | UI message display (layer violation in current code) |
| `new_usb/usb-history.md` | Detailed history of three failed attempts |
| `new_usb/solution.md` | Condensed analysis and solution outline |

## Decisions log

- 2026-05-10 (session 2): Hardware test confirmed: firmware boots, MIDI Launchpad works. USB keyboard/mouse: no response at all. No HID connect message shown.
- 2026-05-10 (session 2): Added `UsbDebugState` struct instrumentation to `usbh_core.c` to show enumeration state on OLED. Build caused regression: mouse LED blinks then dies, Launchpad boots to screensaver mode (MIDI broken).
- 2026-05-10 (session 2): Reverted `usbh_core.c` to master. Stripped to minimal HID-only build: driver in `device_drivers[]`, `hid_driver_init()`, `hid_is_connected()` poll. No debug struct, no `usbh_core.c` changes.
- 2026-05-10: Code review of feat/USB-keyboard4 identified 4 critical bugs causing black screen on hardware. Driver integration (Steps 1-2 from plan) is structurally correct but implementation has bugs.
- 2026-05-10: Three previous branches (archive/bad-usb-implementation, fix/USB-keyboard2, try/USB-keyboard3) all failed at the same point — `find_driver()` never matched the HID driver during enumeration. This branch finally registers the driver correctly but introduced new crash bugs.
- 2026-05-10: Decided the `hid_is_connected()` inversion in `usbh_driver_hid.c:380` is the most critical bug — it returns `true` for `STATE_INACTIVE`, making the polling loop in `UsbH::process()` call connect handlers for empty slots every cycle.
- 2026-05-10: Decided `#include "apps/sequencer/ui/MessageManager.h"` in a platform driver is a layer violation that should be removed. HID events should use callback/function-pointer pattern instead.

## Critical Bugs Found (feat/USB-keyboard4, 2026-05-10)



## RAM Impact
- `hid_device[2]` adds ~572 bytes BSS (2× 256-byte `buffer[]` + 2× 4-byte `report_data[]` + overhead)
- With ~15KB SRAM free, this is tight but acceptable
- `USBH_HID_BUFFER` could be reduced from 256 to 8 (boot protocol keyboard reports are 8 bytes)

## Open questions
- [ ] Which of the 5 low-level USB hypotheses is actually the root cause for enumeration failure?
- [ ] Does the firmware boot correctly without a USB keyboard connected (black screen vs enumeration crash)?
- [ ] Should `USBH_HID_BUFFER` be reduced to save RAM?

## Completed steps
- [x] Add `#include "usbh_driver_hid.h"` to UsbH.cpp
- [x] Add `&usbh_hid_driver` to `device_drivers[]` array
- [x] Call `hid_driver_init(&hid_config)` in `UsbH::init()`
- [x] Implement `HidDriverHandler` with connect/disconnect/message handlers
- [x] Add HID device tracking (`_hidDevices` bitmask) in UsbH
- [x] Add HID polling in `UsbH::process()`
- [x] Add `usbh_driver_hid.c` to CMakeLists.txt
- [x] Fix `hid_is_connected()` inversion: `== STATE_INACTIVE` → `!= STATE_INACTIVE`
- [x] Remove MessageManager layer violation, replace with C function-pointer callbacks
- [x] Restore power sequence to match master (`gpio_clear` in init, `powerOn()` in Sequencer.cpp)
- [x] Remove `g_usbh = this` relocation bug (restore before init)
- [x] Tested minimal HID build — MIDI Launchpad works, but no HID detection (keyboard/mouse)
- [x] Hardware test of session 3 minimal build: MIDI Launchpad works, mouse/keyboard show `HID 0 t=0` (type=NONE means `analyze_descriptor()` fails)
- [ ] Investigate why `analyze_descriptor()` returns false for HID keyboards/mice (returns `HID_TYPE_NONE`)
- [ ] Consider enabling `CONFIG_ENABLE_USBH_DEBUG` for USART trace or adding minimal safe instrumentation to `usbh_core.c`
- [ ] Fix low-speed USB device enumeration or descriptor parsing

## References
- `new_usb/usb-history.md` – detailed history of three failed attempts
- `new_usb/solution.md` – condensed analysis and solution outline
- `.cowork/sessions/2026-05-10-0357-brainstorm-855445.md` – brainstorming session
