# USB HID Keyboard Implementation — Task Status

Status: Active — `hid_get_type()` bug fixed, awaiting hardware test
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
| `new_usb/usb-history.md` | Detailed history of three failed attempts |
| `new_usb/solution.md` | Condensed analysis and solution outline |

## Decisions log

- 2026-05-10 (session 2): Hardware test confirmed: firmware boots, MIDI Launchpad works. USB keyboard/mouse: no response at all. No HID connect message shown.
- 2026-05-10 (session 2): Added `UsbDebugState` struct instrumentation to `usbh_core.c` to show enumeration state on OLED. Build caused regression: mouse LED blinks then dies, Launchpad boots to screensaver mode (MIDI broken).
- 2026-05-10 (session 2): Reverted `usbh_core.c` to master. Stripped to minimal HID-only build: driver in `device_drivers[]`, `hid_driver_init()`, `hid_is_connected()` poll. No debug struct, no `usbh_core.c` changes.
- 2026-05-10: Decided to remove `#include "apps/sequencer/ui/MessageManager.h"` from platform driver, replaced with C function-pointer callbacks.
- 2026-05-10: Decided `hid_is_connected()` inversion was most critical bug — returned `true` for `STATE_INACTIVE`.
- 2026-05-11: Found and fixed `hid_get_type()` inverted condition — `if (hid_is_connected(device_id))` returned `HID_TYPE_NONE` for connected devices. Changed to `if (!hid_is_connected(device_id))`.
- 2026-05-11: Added `HID_TYPE_OTHER` enum value for HID devices with protocol 0x00 (no boot protocol specified).
- 2026-05-11: Improved `analyze_descriptor()` to always save `interface_number` and classify all HID interface protocols (previously only set `interface_number` for keyboard/mouse).

## Bugs Found & Fixed

### Bug 1: `hid_is_connected()` inverted (fixed in previous session)
`usbh_driver_hid.c:380` — `== STATE_INACTIVE` → `!= STATE_INACTIVE`

### Bug 2: Layer violation (fixed in previous session)
Removed `#include "apps/sequencer/ui/MessageManager.h"` from `UsbH.cpp`, replaced with C function-pointer callbacks.

### Bug 3: 10ms blocking delay (fixed in previous session)
Removed `hal::Delay::delay_us(10000)` from `UsbH::init()`.

### Bug 4: `g_usbh = this` after init (fixed in previous session)
Restored `g_usbh = this` before `usbh_init()`.

### Bug 5: `hid_get_type()` inverted condition (fixed this session)
`usbh_driver_hid.c:386` — `if (hid_is_connected(device_id))` returned `HID_TYPE_NONE` for connected devices. Changed to `if (!hid_is_connected(device_id))`. This is why `HID 0 t=0` was reported on hardware — the device was actually connected with a valid type, but `hid_get_type()` unconditionally returned NONE.

### Bug 6: `analyze_descriptor()` didn't save `interface_number` for all protocols (fixed this session)
Only saved `interface_number` for keyboard (0x01) and mouse (0x02) protocols. Now saves for all HID interfaces. Also added `HID_TYPE_OTHER` classification for protocol 0x00 devices.

## RAM Impact
- `hid_device[2]` adds ~572 bytes BSS (2× 256-byte `buffer[]` + 2× 4-byte `report_data[]` + overhead)
- With ~15KB SRAM free, this is tight but acceptable
- `USBH_HID_BUFFER` could be reduced from 256 to 8 (boot protocol keyboard reports are 8 bytes)

## Open questions
- [x] ~~Which of the 5 low-level USB hypotheses is actually the root cause?~~ — Bug 5 (`hid_get_type()` inverted) explains `HID 0 t=0`
- [ ] Does the firmware correctly report HID type after the fix? (keyboard=2, mouse=1, other=1)
- [ ] Does the HID polling loop correctly read keyboard input data?
- [ ] Should `USBH_HID_BUFFER` be reduced to save RAM?
- [ ] SET_IDLE and SET_PROTOCOL (boot mode) commands are not sent — needed for reliable keyboard operation?

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
- [x] Hardware test of session 3 minimal build: MIDI Launchpad works, mouse/keyboard show `HID 0 t=0`
- [x] Fix `hid_get_type()` inverted condition: `if (hid_is_connected(id))` → `if (!hid_is_connected(id))`
- [x] Add `HID_TYPE_OTHER` to enum and classify all HID interface protocols

## Next actions
1. Flash build to hardware, verify MIDI still works
2. Test keyboard — should now show `HID 0 t=2` (KEYBOARD) instead of `HID 0 t=0`
3. If type display is correct but no key data arrives, investigate polling and SET_IDLE/SET_PROTOCOL
4. Consider reducing `USBH_HID_BUFFER` from 256 to save RAM

## References
- `new_usb/usb-history.md` – detailed history of three failed attempts
- `new_usb/solution.md` – condensed analysis and solution outline
- `.cowork/sessions/2026-05-10-0357-brainstorm-855445.md` – brainstorming session