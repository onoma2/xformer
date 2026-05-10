# USB HID Keyboard Implementation — Task Status

Status: Active — keyboard keypresses verified end-to-end (`K:04 M:XX` on OLED)
Priority: High

## Overview

Add USB HID keyboard support to the PER|FORMER/XFORMER sequencer firmware. After fixing multiple bugs in the HID driver, keyboards and mice now enumerate correctly and keypresses are received via interrupt IN polling. Next step: wire keypresses to sequencer UI/engine.

## Key Files

| File | Role |
|------|------|
| `src/platform/stm32/drivers/UsbH.cpp` | USB host init, driver registration, HID polling, key dequeue |
| `src/platform/stm32/drivers/UsbH.h` | UsbH class, HID device tracking, callbacks, key ring buffer |
| `src/platform/stm32/libs/libusbhost/include/usbh_driver_hid.h` | HID driver header, HID_TYPE enum, HidKeyEvent, hid_read_key() |
| `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c` | HID driver: enumeration, polling, key ring buffer |
| `src/platform/stm32/libs/libusbhost/src/usbh_core.c` | USB host core (untouched — debug instrumentation caused regression) |
| `src/apps/sequencer/Sequencer.cpp` | HID callback wiring (connect/disconnect/key display) |

## Decisions log

- 2026-05-10 (session 2): Hardware test confirmed MIDI works, USB keyboard/mouse no response.
- 2026-05-10 (session 2): Debug instrumentation in `usbh_core.c` caused regression (MIDI broken). Reverted.
- 2026-05-10: Decided C function-pointer callbacks instead of `MessageManager.h` include in platform driver.
- 2026-05-11: Fixed `hid_get_type()` inverted condition — was returning NONE for connected devices.
- 2026-05-11: Added `HID_TYPE_OTHER` for protocol 0x00, always save `interface_number`.
- 2026-05-11: Added `debug_msg` callback to `hid_config_t` — **caused regression** (MIDI broken, mouse LED blink-die). Calling callbacks from inside USB enumeration state machine breaks timing. Reverted.
- 2026-05-11: Hardware test: mouse shows `HID 0 t=2` (MOUSE), keyboard shows `HID 0 t=3` (KEYBOARD). Both low-speed (1.5Mbps). The "Usb KeyBoard" mouse device has wrong USB descriptors (protocol 0x01 instead of 0x02).
- 2026-05-11: Key ring buffer approach: `HidKeyEvent` ring buffer in `usbh_driver_hid.c`, `hid_read_key()` API to dequeue, `UsbH::process()` reads and calls `_hidKeyCallback`. No callbacks during USB state machine — safe.
- 2026-05-11: **Keyboard keypresses work!** `K:04 M:00` (keycode 0x04 = 'a') displayed on OLED. Full pipeline verified: enumeration → descriptor parsing → report descriptor → interrupt IN polling → key data → ring buffer → `process()` → callback → OLED.

## Bugs Found & Fixed

### Bug 1-4: Fixed in session 1 (blocking delay, g_usbh order, layer violation, hid_is_connected)
### Bug 5: `hid_get_type()` inverted (session 2) — returned NONE for connected devices
### Bug 6: `analyze_descriptor()` didn't save interface_number for all protocols (session 2)
### Bug 7: `debug_msg` callback in USB state machine caused regression (session 2) — reverted

**Root cause of regression**: Calling `MessageManager::showMessage()` (which does OLED rendering) from inside the USB enumeration/transfer state machine interferes with USB timing. The ring buffer approach avoids this by deferring processing to the UI task.

## Architecture: Key Event Ring Buffer

```
USB interrupt → hid_in_message_handler() [C callback, USB task context]
                     ↓
             enqueue_key() → HidKeyEvent ring buffer [lock-free, ISR-safe]
                     
UsbH::process() [UI task context, 50Hz]
    ↓
hid_read_key() → _hidKeyCallback → MessageManager::showMessage() [UI task, safe]
```

- Ring buffer: 8 slots, `HidKeyEvent` struct (device_id, modifiers, keycode, pressed)
- `enqueue_key()` called from `event()` in USB task context — no OLED/rendering calls
- `hid_read_key()` + callback called from `UsbH::process()` in UI task — safe to render

## Hardware Test Results (2026-05-11)

| Device | VID/PID | Speed | Type | Connect | Keys | Notes |
|--------|---------|-------|------|---------|------|-------|
| Launchpad (MIDI) | — | Full | — | Works ✅ | N/A | No regression |
| Logitech mouse | 0x046d/0xc018 | Low (1.5Mbps) | t=2 (MOUSE) ✅ | `HID 0 t=2` | N/A | Protocol 0x02 correct |
| Generic keyboard | 0xc0f4/0x06f5 | Low (1.5Mbps) | t=3 (KEYBOARD) ✅ | `HID 0 t=3` | `K:XX M:XX` ✅ | Keys verified! |
| Generic mouse* | 0xc0f4/0x06f5 | Low (1.5Mbps) | t=3 (KEYBOARD) ⚠️ | `HID 0 t=3` | N/A | Wrong USB descriptors (protocol 0x01 but is a mouse) |

*The "Usb KeyBoard" mouse is a cheap generic mouse that reports bInterfaceProtocol=0x01 (keyboard) in its USB descriptors.

## RAM Impact
- `hid_device[2]`: ~572 bytes BSS
- `key_buffer[8]`: 32 bytes BSS (8 × `HidKeyEvent`)
- Total: ~604 bytes new BSS, ~15KB SRAM free — acceptable

## Open questions
- [ ] Design `KeyboardController` class (similar to `LaunchpadController`) — maps QWERTY keys to sequencer actions
- [ ] Wire keypress events to sequencer UI/engine (track selection, play/stop, etc.)
- [ ] Add SET_IDLE(0) and SET_PROTOCOL(boot) for reliable keyboard operation on all devices
- [ ] Fix GET_REPORT_DESCRIPTOR `wIndex` — currently hardcoded to 0, should use `interface_number` for multi-interface devices
- [ ] Consider reducing `USBH_HID_BUFFER` from 256 to 8 bytes (boot protocol reports are always 8 bytes)
- [ ] Mouse support — forward mouse events to UI for cursor/parameter control?

## Completed steps
- [x] Fix `hid_is_connected()` inversion
- [x] Fix `hid_get_type()` inversion, add `HID_TYPE_OTHER`
- [x] Fix `analyze_descriptor()` interface_number for all protocols
- [x] Remove MessageManager layer violation
- [x] HID connect/disconnect/key event ring buffer (safe, no regression)
- [x] Keyboard keypresses arrive and display on OLED: `K:04 M:00`

## Next actions
1. Design `KeyboardController` class (similar to `LaunchpadController`) for sequencer integration
2. Map QWERTY keys to sequencer actions (track selection, play/stop, etc.)
3. Add SET_IDLE(0) and SET_PROTOCOL(boot) for reliable keyboard operation on all devices
4. Fix GET_REPORT_DESCRIPTOR `wIndex` to use `interface_number`
5. Reduce `USBH_HID_BUFFER` from 256 to 8 bytes

## References
- `new_usb/usb-history.md` – detailed history of three failed attempts
- `new_usb/solution.md` – condensed analysis and solution outline