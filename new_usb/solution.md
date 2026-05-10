# USB HID Keyboard Solution for Performer

Based on analysis of the current master branch and the three failed USB branches (`archive/bad-usb-implementation`, `fix/USB-keyboard2`, `try/USB-keyboard3`), the root cause of the HID driver not being matched is the missing HID driver registration and initialization in the current code.

## Current State (master)
- Missing `#include "usbh_driver_hid.h"` in `src/platform/stm32/drivers/UsbH.cpp`
- Missing `&usbh_hid_driver` from the `device_drivers[]` array
- No call to `hid_driver_init()` in `UsbH::init()`
- No HID event handling (connect/disconnect/message) implemented.

## Proposed Solution (No Code Writing)

### Step 1: Integrate HID Driver Correctly
1. Add `#include "usbh_driver_hid.h"` to `src/platform/stm32/drivers/UsbH.cpp`.
2. Add `&usbh_hid_driver` to the `device_drivers[]` array (after `&usbh_midi_driver`).
3. Define a `hid_config_t` with appropriate callback functions (connect, disconnect, message) that forward events to the `UsbH` class.
4. Call `hid_driver_init(&hid_config)` in `UsbH::init()` after `midi_driver_init()`.

### Step 2: Implement HID Event Handling in `UsbH`
Add static handler functions (e.g., `HidDriverHandler::connectHandler`, `disconnectHandler`, `messageHandler`) that:
- On connect: call `g_usbh->hidConnectDevice(device, type)` (add stubs in `UsbH` if needed).
- On disconnect: call `g_usbh->hidDisconnectDevice(device)`.
- On message: decode the 8‑byte boot‑protocol keyboard report (modifiers + keycodes) and enqueue UI/engine events.
Ensure handlers match the signature expected by `hid_config_t`.

### Step 3: Diagnose Low‑Level USB Issues (If Needed)
If the above does not work, enable debugging and test hypotheses from history:
1. **Enable USB debug logging**: Set `CONFIG_ENABLE_USBH_DEBUG 1` in `src/platform/stm32/libs/libusbhost/include/usbh_config.h`.
2. **Test multiple keyboards**: Try both low‑speed and full‑speed models.
3. **Add explicit PCDET clearing**: In `usbh_lld_stm32f4.c`, after detecting a connect event, add `REBASE(OTG_HPRT) |= OTG_HPRT_PCDET;`.
4. **Add diagnostic indicators**: Toggle a GPIO/LED in `hid_driver_init()` and/or `hid_device_t::init()`.
5. **Check power sequencing**: Ensure VBUS enable GPIO can supply sufficient current.
6. **Apply ERRSIZ patch only if needed**: If config‑descriptor reads fail, consider the patch from `try/USB-keyboard3`, but regression‑test MIDI.

### Step 4: Verify End‑to‑End
- Confirm connect/disconnect messages appear in logs.
- Verify keypresses are decoded and forwarded to Engine/UI.
- Ensure existing MIDI and hub functionality remains unaffected.

## Summary
The immediate blocker is the missing HID driver registration. Fixing that (Steps 1‑2) should enable HID support. If low‑level USB issues persist, use the diagnostic approach (Step 3) to isolate the failure point among the five hypotheses (slot contamination, speed negotiation, descriptor parsing, port detect race, power sequencing) and apply the targeted remedy.
