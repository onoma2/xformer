# USB Keyboard Implementation History

Three branches attempted USB HID keyboard support. All failed with the same symptom: **keyboard is never detected** (connect callback never fires). Below documents each attempt and the root cause analysis.

## Branches

- `archive/bad-usb-implementation` — qwen+gemini generated (17 files, +529/-19)
- `fix/USB-keyboard2` — incremental fix attempt (32 files, +816/-405)
- `try/USB-keyboard3` — aggressive diagnostics + low-level patches (25 files, +1338/-18)

---

## Attempt 1: `archive/bad-usb-implementation`

### Changes
- Registered `&usbh_hid_driver` in `device_drivers[]` array in `UsbH.cpp`
- Added `notify_connected`/`notify_disconnected` callbacks to `hid_config_t` struct
- Created `HidDriverHandler` struct in `UsbH.cpp` with connect/disconnect/message handlers
- Wired `hid_config` with designated initializers
- Added HID device tracking, keypress ring buffer, and dequeue in `UsbH.h/cpp`
- Set `driver_info.ifaceProtocol = 1` (boot keyboard protocol only)
- Created `KeyboardHelper` for HID→ASCII mapping
- Integrated keyboard events into `Engine` and `Ui` (Event.h, Page.cpp, Ui.cpp)
- TeletypeScriptViewPage changes for text entry

### Result
**FAILED** — No connection messages, keyboard does not work. Connect callback never called.

---

## Attempt 2: `fix/USB-keyboard2`

### Changes
- Added SET_PROTOCOL (boot mode) to HID driver state machine
- Relaxed `driver_info.ifaceProtocol = -1` (don't care)
- Created separate `UsbKeyboard` driver class (not in UsbH)
- Added `notify_connected(device_id)` (no HID_TYPE param, simpler signature)
- Added various UI integration: TextInputPage, TeletypeScriptViewPage, TeletypePatternViewPage
- Added manual "Detect keyboard" utility in SystemPage → Utilities
- Created `gemini-plan-usb-key.md` outlining architecture approach

### Result
**FAILED** — Still no detection. Manual "Detect keyboard" utility only toggles internal state, has no physical effect.

---

## Attempt 3: `try/USB-keyboard3` — Most Diagnostic

### Changes

#### a) HID driver diagnostics (usbh_driver_hid.c)
Called `notify_connected` at the earliest possible points:

1. **From `init()`** — fires IMMEDIATELY when driver is matched, before any descriptor parsing:
   ```c
   if (hid_config.notify_connected) {
       hid_config.notify_connected(i);
   }
   ```

2. **From `analyze_descriptor()`** — fires when `bInterfaceProtocol == 0x01` detected during descriptor analysis

#### b) Core.c patches (broke MIDI)
Two modifications to `usbh_core.c`:

1. **Force DATA stage to start with DATA1 toggle** — spec-correct for control transfers:
   ```c
   // set dev->toggle0 = 1 before starting DATA stage for IN/OUT
   ```

2. **Accept ERRSIZ as success for config descriptor reads**:
   ```c
   // Accept USBH_PACKET_CALLBACK_STATUS_ERRSIZ when
   // transferred_length >= wTotalLength
   ```

### Results

**Definitive conclusion from diagnostics:**
- `notify_connected` from `init()` **never fired**
- `notify_connected` from `analyze_descriptor()` **never fired**
- This proves the HID driver is **never matched by `find_driver()`** during enumeration
- Core.c patches **broke MIDI device detection**

---

## Root Cause Analysis

### What was proven
`find_driver()` in `usbh_core.c:68-102` never calls the HID driver's `init()`. The driver matching logic uses `CHECK_PARTIAL_COMPATIBILITY` macros which compare each registered driver's info against the detected device/interface descriptor fields.

### Driver matching matrix

| Driver | ifaceClass | Matches keyboard? |
|--------|-----------|-------------------|
| `usbh_hub_driver` | 0x09 | No — keyboard is HID (0x03), not hub |
| `usbh_midi_driver` | 0x01 (Audio), 0x03 (MIDI subclass) | No — HID class 0x03 ≠ Audio class 0x01 |
| `usbh_hid_driver` | 0x03 | **Should match** — but doesn't! |

### Key code flow (usbh_core.c)

```
usbh_poll()
  → LLD poll detected DEVICE_CONNECTED
  → device_enumeration_start(&usbh_device[0])
    → SET_ADDRESS, GET_DESCRIPTOR, SET_CONFIGURATION...
    → USBH_ENUM_STATE_FIND_DRIVER
      → device_register(usbh_buffer, length, dev)
        → iterates descriptors, on USB_DT_INTERFACE:
          → find_driver(dev, &device_info)
            → checks registered drivers one by one
            → if HID class matches, calls hid_driver.init()
            → init() returns drvdata or NULL
          → if find_driver succeeds:
            → calls analyze_descriptor() for sub-descriptors
            → returns when analyze_descriptor() returns true
```

### Hypotheses for why `init()` is never called

#### 1. Single-device slot architecture
`usbh_poll()` only uses `usbh_device[0]` (the root port). If a previous enumeration failed mid-way, the slot may be left in a state that blocks re-enumeration. The disconnect handler calls `device_remove()` on all slots, but any state contamination from failed enumeration could prevent future connect detection.

#### 2. Low-speed vs full-speed
Many USB keyboards are low-speed (1.5 Mbps). The LLD handles this in `poll_run()`:
```c
} else if ((REBASE(OTG_HPRT) & OTG_HPRT_PSPD_MASK) == OTG_HPRT_PSPD_LOW) {
    REBASE(OTG_HFIR) = 6000;
    REBASE(OTG_HCFG) = OTG_HCFG_FSLSPCS_6MHz;
    channels_init(dev);
    dev->dpstate = DEVICE_POLL_STATE_DEVRST;
    reset_start(dev);
}
```
If speed detection fails or the PHY doesn't properly negotiate low-speed, enumeration never completes and `USBH_POLL_STATUS_DEVICE_CONNECTED` never returns.

#### 3. Descriptor parsing failure
`analyze_descriptor()` returns true only when BOTH `endpoint_in_address` AND `report0_length` are set. The HID descriptor parsing uses a packed struct `hid_report_decriptor`:
```c
struct hid_report_decriptor {
    struct usb_hid_descriptor header;
    struct _report_descriptor_info {
        uint8_t bDescriptorType;
        uint16_t wDescriptorLength;
    } __attribute__((packed)) report_descriptors_info[];
} __attribute__((packed));
```
If `bDescriptorType` within `report_descriptors_info[0]` doesn't match `USB_DT_REPORT` (due to padding, alignment, or actual device descriptor variation), `report0_length` stays 0 and the driver never transitions to `STATE_GET_REPORT_DESCRIPTOR_READ_SETUP`. The `device_register()` function falls through, prints "Device driver isn't compatible", calls `device_remove()`, and moves on.

#### 4. Port connect/detection race
The `DEVICE_POLL_STATE_DEVCONN` state checks `PCDET && PCSTS` after 500ms debounce, but PCDET is never explicitly cleared. If the hardware auto-clears it, the check fails silently and the port stays in DEVCONN forever, never returning `USBH_POLL_STATUS_DEVICE_CONNECTED`.

#### 5. USB power/sequencing
The `powerOn()` sequence in `UsbH.cpp` enables VBUS via GPIO. If the keyboard draws more current than the port can supply (some keyboards need 500mA), the voltage droops, the device disconnects, and the DISCINT interrupt fires.

### Next steps to diagnose

1. **Enable USB debug logging** — set `CONFIG_ENABLE_USBH_DEBUG 1` in `SystemConfig.h` to see `LOG_PRINTF` from enumeration
2. **Test low-speed keyboard vs full-speed keyboard** — narrow down speed negotiation
3. **Manually verify PCDET clearing behavior** — add explicit `REBASE(OTG_HPRT) |= OTG_HPRT_PCDET` to clear the connect detect bit after detection
4. **Add diagnostic LED/blink on HID driver `init()`** — physical confirmation of driver matching
5. **Test with a USB hub** — hub driver handles multi-device enumeration which might work around port issues
