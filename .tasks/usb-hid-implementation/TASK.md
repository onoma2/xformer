# USB HID Keyboard Implementation — Task Status

Status: Active — HID enumeration working, testing keyboard polling
Priority: High

## Overview

Add USB HID keyboard support to the PER|FORMER/XFORMER sequencer firmware. The master branch lacks HID driver registration and initialization. After fixing multiple bugs, HID devices now enumerate correctly and report their type (keyboard/mouse/other). Current focus: verifying keyboard polling works (key data arrives via interrupt IN endpoint).

## Key Files

| File | Role |
|------|------|
| `src/platform/stm32/drivers/UsbH.cpp` | Main USB host initialization, driver registration, HID polling |
| `src/platform/stm32/drivers/UsbH.h` | UsbH class definition, HID device tracking, callbacks |
| `src/platform/stm32/libs/libusbhost/include/usbh_driver_hid.h` | HID driver header (HID_TYPE enum, api) |
| `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c` | HID driver implementation (enumeration, polling, reports) |
| `src/platform/stm32/libs/libusbhost/src/usbh_core.c` | USB host core (device enumeration, driver matching) |
| `src/platform/stm32/libs/libusbhost/include/usbh_config.h` | USB host configuration (MAX_DEVICES, buffer sizes) |
| `new_usb/usb-history.md` | Detailed history of three failed attempts |
| `new_usb/solution.md` | Condensed analysis and solution outline |

## Decisions log

- 2026-05-10 (session 2): Hardware test confirmed: firmware boots, MIDI Launchpad works. USB keyboard/mouse: no response at all.
- 2026-05-10 (session 2): Added `UsbDebugState` struct instrumentation to `usbh_core.c` — caused regression (MIDI broken). Reverted.
- 2026-05-10: Decided to use C function-pointer callbacks instead of `MessageManager.h` include in platform driver.
- 2026-05-10: Decided `hid_is_connected()` inversion was most critical bug.
- 2026-05-11: Fixed `hid_get_type()` inverted condition — was returning NONE for connected devices.
- 2026-05-11: Added `HID_TYPE_OTHER` for protocol 0x00 devices. Improved `analyze_descriptor()` to always save `interface_number`.
- 2026-05-11: Added `debug_msg` callback to `hid_config_t` for diagnostic messages on OLED (avoids touching `usbh_core.c`).
- 2026-05-11: **Hardware test results**: Mouse (Logitech 0x046d/0xc018) shows `HID 0 t=2` (MOUSE) ✅. Keyboard (0xc0f4/0x06f5, low-speed 1.5Mbps) shows `HID 0 t=3` (KEYBOARD) ✅. Both devices correctly identified, connect/disconnect works properly (`HID rm` on physical disconnect).
- 2026-05-11: Both test devices are **low-speed** (1.5 Mbps), connected through a USB hub on macOS — the type T=3 for "Usb KeyBoard" mouse was correct (its descriptors report protocol 0x01).
- 2026-05-11: Added keypress debug display: `messageHandler` shows `K:XX M:XX` (keycode, modifier) on OLED to verify interrupt IN polling.

## Bugs Found & Fixed

### Bug 1: `hid_is_connected()` inverted (session 1)
`usbh_driver_hid.c:380` — `== STATE_INACTIVE` → `!= STATE_INACTIVE`

### Bug 2: Layer violation (session 1)
Removed `#include "apps/sequencer/ui/MessageManager.h"` from `UsbH.cpp`, replaced with C function-pointer callbacks.

### Bug 3: 10ms blocking delay (session 1)
Removed `hal::Delay::delay_us(10000)` from `UsbH::init()`.

### Bug 4: `g_usbh = this` after init (session 1)
Restored `g_usbh = this` before `usbh_init()`.

### Bug 5: `hid_get_type()` inverted condition (session 2)
`usbh_driver_hid.c:386` — `if (hid_is_connected(device_id))` returned `HID_TYPE_NONE` for connected devices. Changed to `if (!hid_is_connected(device_id))`. Explained `HID 0 t=0` false positive — device was connected with valid type but getter always returned NONE.

### Bug 6: `analyze_descriptor()` didn't save `interface_number` for all protocols (session 2)
Only saved `interface_number` for keyboard (0x01) and mouse (0x02) protocols. Now saves for all HID interfaces. Added `HID_TYPE_OTHER` for protocol 0x00.

## RAM Impact
- `hid_device[2]` adds ~572 bytes BSS (2× 256-byte `buffer[]` + 2× 4-byte `report_data[]` + overhead + debug callback pointer)
- With ~15KB SRAM free, this is tight but acceptable
- `USBH_HID_BUFFER` could be reduced from 256 to 8 (boot protocol keyboard reports are 8 bytes)

## Hardware Test Results (2026-05-11)

| Device | VID/PID | Speed | Type Reported | Connect | Disconnect | Keys |
|--------|---------|-------|---------------|---------|------------|------|
| Logitech mouse | 0x046d/0xc018 | Low (1.5Mbps) | t=2 (MOUSE) ✅ | `HID 0 t=2` | `HID rm` ✅ | N/A |
| Generic keyboard | 0xc0f4/0x06f5 | Low (1.5Mbps) | t=3 (KEYBOARD) ✅ | `HID 0 t=3` | `HID rm` ✅ | TBD |
| Launchpad (MIDI) | — | Full Speed | — | Works ✅ | Works ✅ | N/A |

Note: The "Usb KeyBoard" mouse device has `bInterfaceProtocol = 0x01` in its USB descriptors, which the HID spec classifies as keyboard. This is a cheap generic mouse with incorrect descriptors — not a firmware bug.

## Debug Messages (OLED)

| Message | Meaning |
|---------|---------|
| `HID init` | Driver slot allocated during enumeration |
| `HID no slot` | No free HID device slots |
| `HID desc ok` | `analyze_descriptor()` succeeded — endpoint + report descriptor found |
| `HID polling` | GET_REPORT_DESCRIPTOR succeeded, entering poll loop |
| `HID rdesc fail` | GET_REPORT_DESCRIPTOR USB transfer failed |
| `HID read err` | Interrupt IN endpoint read error |
| `HID rm` | Device removed (physical disconnect or enumeration failure) |
| `K:XX M:XX` | Keypress data: keycode and modifier bytes (temporary debug) |

## Open questions
- [ ] Does the keyboard polling work end-to-end? (key data arrives via interrupt IN)
- [ ] Should `USBH_HID_BUFFER` be reduced from 256 to 8 for boot protocol keyboards?
- [ ] SET_IDLE and SET_PROTOCOL (boot mode) commands not sent — needed for reliable keyboard operation?
- [ ] GET_REPORT_DESCRIPTOR `wIndex` is hardcoded to 0 — should use `interface_number` for multi-interface devices

## Completed steps
- [x] Add `#include "usbh_driver_hid.h"` to UsbH.cpp
- [x] Add `&usbh_hid_driver` to `device_drivers[]` array
- [x] Call `hid_driver_init(&hid_config)` in `UsbH::init()`
- [x] Implement `HidDriverHandler` with connect/disconnect/message handlers
- [x] Add HID device tracking (`_hidDevices` bitmask) in UsbH
- [x] Add HID polling in `UsbH::process()`
- [x] Add `usbh_driver_hid.c` to CMakeLists.txt
- [x] Fix `hid_is_connected()` inversion
- [x] Remove MessageManager layer violation, use C function-pointer callbacks
- [x] Restore power sequence to match master
- [x] Remove `g_usbh = this` relocation bug
- [x] Fix `hid_get_type()` inverted condition
- [x] Add `HID_TYPE_OTHER` to enum and classify all HID interface protocols
- [x] Add `debug_msg` callback to `hid_config_t` for OLED diagnostics
- [x] Add keypress debug display (`K:XX M:XX`) in `messageHandler`
- [x] Hardware test: mouse=MOUSE(t=2), keyboard=KEYBOARD(t=3), connect/disconnect works
- [ ] Verify keyboard keypresses arrive and are displayed on OLED
- [ ] Wire keypresses to sequencer UI/engine
- [ ] Add SET_IDLE(0) and SET_PROTOCOL(boot) for reliable keyboard operation

## References
- `new_usb/usb-history.md` – detailed history of three failed attempts
- `new_usb/solution.md` – condensed analysis and solution outline
- `.cowork/sessions/2026-05-10-0357-brainstorm-855445.md` – brainstorming session