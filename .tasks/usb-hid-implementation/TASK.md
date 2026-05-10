# USB HID Keyboard Implementation — Task Status

Status: Planning
Priority: High

## Overview

Add USB HID keyboard support to the PER|FORMER/XFORMER sequencer firmware. Currently the master branch lacks HID driver registration and initialization, causing keyboards to never be detected. Three previous attempts failed with the same symptom (connect callback never fires) due to low‑level USB issues and missing driver integration.

## Key Files

| File | Role |
|------|------|
| `src/platform/stm32/drivers/UsbH.cpp` | Main USB host initialization, driver registration |
| `src/platform/stm32/libs/libusbhost/include/usbh_driver_hid.h` | HID driver header |
| `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c` | HID driver implementation |
| `src/platform/stm32/libs/libusbhost/src/usbh_core.c` | USB host core (device enumeration, driver matching) |
| `src/platform/stm32/libs/libusbhost/include/usbh_config.h` | USB host configuration (enable debug logging) |

## Current Blockers (master)

- Missing `#include "usbh_driver_hid.h"` in `UsbH.cpp`
- Missing `&usbh_hid_driver` from `device_drivers[]` array
- No call to `hid_driver_init()` in `UsbH::init()`
- No HID event handling (connect/disconnect/message) implemented.

## Root‑Cause Analysis (from failed branches)

All three failed branches showed that the HID driver’s `init()` was never called, proving `find_driver()` never matched the HID driver during enumeration. Five hypotheses were identified:

1. **Single‑device slot contamination** – failed enumeration leaves `usbh_device[0]` in a bad state.
2. **Low‑speed vs. full‑speed negotiation** – PHY speed detection fails, preventing enumeration completion.
3. **HID descriptor parsing failure** – `analyze_descriptor()` never returns `true` because `bDescriptorType` in the HID descriptor doesn’t match `USB_DT_REPORT` (padding/alignment/device variation).
4. **Port connect/detection race** – `PCDET` bit not cleared, port stuck in `DEVCONN` state.
5. **USB power/sequencing** – VBUS droop due to over‑current disconnects the device.

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
If Step 1‑2 still fails, enable debugging and test hypotheses:
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

## Implementation Order

1. **Integrate driver** (Steps 1‑2) – expected to enable basic HID detection.
2. **If still failing, run diagnostics** (Step 3) to isolate the failure point among the five hypotheses.
3. **Apply targeted remedy** and regression‑test.
4. **Final verification** (Step 4).

## Effort Estimate

- Driver integration & event handling: ~2‑3 hours
- Diagnostics & low‑level fixes (if needed): ~2‑5 hours depending on issue
- Verification: ~1 hour

## Risks

- Low‑level USB issues may persist; debug logs essential.
- Ensure MIDI remains functional after any core.c patches.
- USB power budget: verify keyboard current draw <500 mA.

## References

- `new_usb/usb-history.md` – detailed history of three failed attempts.
- `new_usb/solution.md` – condensed analysis and solution outline.