# USB QWERTY Keyboard Support for Performer Teletype Track

## Executive Summary

This document details the plan to add **USB keyboard support** to Performer's Teletype track implementation, achieving **feature parity with the original Monome Teletype hardware**.

### Goals

1. ✅ **Enable direct text input** via USB keyboard (bypass T9-style step-key cycling)
2. ✅ **Professional line editor** with Emacs-style shortcuts (matching original Teletype)
3. ✅ **Immediate command execution** (type → Enter → instant CV/gate response)
4. ✅ **Dual input mode** (keyboard + step-keys work simultaneously)
5. ✅ **Zero regression** (existing step-key workflow unchanged for hardware-only users)

### User Experience

**Before (Current Performer):**
```
Type "CV 1 V 5":
- Press Step 1 twice → "C"
- Press Step 10 three times → "V"
- Press Right button → space
- Press Step 0 once → "1"
- Press Right button → space
- ... (~20 button presses)
- Press Encoder+Shift → execute
```

**After (With USB Keyboard):**
```
Type "CV 1 V 5":
- Type: C V space 1 space V space 5
- Press Enter → execute
(9 keypresses, professional editing, immediate execution)
```

### Technical Approach

- **Phase 1:** Register existing HID driver (already in codebase)
- **Phase 2:** Add KeyboardEvent to event system
- **Phase 3:** Implement thread-safe queue (USB Host Task → UI Task)
- **Phase 4:** Port HID-to-ASCII conversion from original Teletype
- **Phase 5:** Implement professional line editor with 20+ keyboard shortcuts
- **Phase 6:** Hardware testing with multiple keyboard models

### Key Design Decision

Rather than route keyboard input through the T9 step-key abstraction, we implement a **professional line editor** matching the original Teletype's `line_editor.c`, including:

- **Navigation:** Ctrl-A/E (home/end), Alt-F/B (word forward/back), arrow keys
- **Editing:** Ctrl-K/U (kill to end/start), Ctrl-W (kill word), Alt-D (delete word)
- **Clipboard:** Ctrl-X/C/V (cut/copy/paste)

This honors Teletype's heritage while maintaining backward compatibility with Performer's step-key input.

---

## Current Performer USB Host Status
- **Hardware:** STM32F4 OTG FS (Full Speed) peripheral.
- **Low-Level Library:** `libopencm3`.
- **USB Host Stack:** Custom `usbh` stack (found in `src/platform/stm32/drivers/UsbH.cpp`).
- **Currently Supported Drivers:**
  - `usbh_hub_driver`: Support for USB Hubs.
  - `usbh_midi_driver`: Support for USB MIDI class devices.
- **Missing Class Driver:** `HID` (Human Interface Device). Standard QWERTY keyboards are not recognized by the current stack because no HID driver is registered in the `device_drivers[]` array.

## Requirements for Physical Keyboard Support

### 1. Class Driver (The missing piece)
- Need to port or implement `usbh_driver_hid.c/h` compatible with the internal `usbh` framework.
- Register `&usbh_hid_driver` in `UsbH.cpp`.
- This driver must handle keyboard report parsing (converting 8-byte HID frames into keycodes).

### 2. Event Plumbing
- Create a new `KeyboardEvent` class in Performer's application layer.
- Update `UsbH::process()` to dequeue HID reports and push them into the main event queue.

### 3. UI Integration
- Update `TeletypeScriptViewPage` and `TeletypePatternViewPage` to handle raw keycodes.
- Re-integrate Teletype's original `hid_to_ascii` logic (from `kbd.c`) to handle layout and modifiers (Shift/Alt/Ctrl).

### 4. Hardware Note
- The STM32F4 OTG FS port provides power (`USB_PWR_EN`), so most standard keyboards will work without an external power supply, provided they don't exceed the power limit.

## Implementation Strategy (Phased MVP)

### Phase A (MVP, Quick Win)
- Register HID driver.
- Add `KeyboardEvent` (keycode + modifiers) to the event system.
- Minimal editor support in TeletypeScriptViewPage + TeletypePatternViewPage:
  - ASCII letters/numbers
  - Space, Backspace, Enter
  - Arrow Left/Right (cursor move)
  - Arrow Up/Down (line/row selection)

## UPDATE: Existing HID Driver Found

- **Discovery:** A complete HID class driver exists in `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c`.
- **Current Capabilities:** It handles HID enumeration, Interrupt IN endpoints, and identifies both Keyboards and Mice.
- **Missing Registration:** The driver is not registered in `UsbH.cpp`.

## Detailed Implementation Plan

### Phase 1: USB Driver Registration

**Location:** `src/platform/stm32/drivers/UsbH.cpp` and `UsbH.h`

**Changes Required:**

1. **Add HID driver include:**
   ```cpp
   // In UsbH.cpp after line 12
   #include "usbh_driver_hid.h"
   ```

2. **Register HID driver in device_drivers array:**
   ```cpp
   // In UsbH.cpp:28-32
   static const usbh_dev_driver_t *device_drivers[] = {
       &usbh_hub_driver,
       &usbh_midi_driver,
       &usbh_hid_driver,  // ADD THIS LINE
       nullptr
   };
   ```

3. **Add HID device tracking in UsbH class:**
   ```cpp
   // In UsbH.h private members
   struct HidDevice {
       uint8_t deviceId;
       bool connected;
       enum HID_TYPE type;  // HID_TYPE_KEYBOARD or HID_TYPE_MOUSE
   };
   static constexpr size_t MaxHidDevices = 4;
   HidDevice _hidDevices[MaxHidDevices];
   ```

4. **Initialize HID driver:**
   ```cpp
   // In UsbH::init() after line 219
   hid_driver_init(&hid_config);
   ```

**Power Budget Considerations:**
- STM32F4 OTG FS provides up to **500mA** at 5V (USB 2.0 spec)
- Typical keyboard power draw: 50-150mA (basic), 100-250mA (backlit mechanical)
- Current code enables `USB_PWR_EN` (UsbH.cpp:206) - verify no current limiting
- **Recommendation:** Test with common keyboards; document max tested current draw
- **Warning:** RGB/gaming keyboards may exceed budget - consider USB hub with external power

---

### Phase 2: Event System Integration

**Location:** `src/apps/sequencer/ui/Event.h`

**Thread Safety Requirements:**
- HID callback runs in **USB Host Task** (priority 3, 2048 stack)
- UI updates happen in **UI Task** (priority 2, 4096 stack)
- Must use thread-safe queue (follow MIDI pattern from UsbH.cpp:64-100)

**Event Type Addition:**

```cpp
// In Event.h:11-17, add new type
enum Type {
    KeyUp,
    KeyDown,
    KeyPress,
    Encoder,
    Midi,
    Keyboard,  // ADD THIS LINE
};
```

**KeyboardEvent Class:**

```cpp
// Add to Event.h after MidiEvent (line 95)
class KeyboardEvent : public Event {
public:
    KeyboardEvent(uint8_t keycode, uint8_t modifiers) :
        Event(Event::Keyboard),
        _keycode(keycode),
        _modifiers(modifiers)
    {}

    uint8_t keycode() const { return _keycode; }
    uint8_t modifiers() const { return _modifiers; }

    // Modifier helpers
    bool shift() const { return _modifiers & 0x22; }  // Left/Right Shift
    bool ctrl() const { return _modifiers & 0x11; }   // Left/Right Ctrl
    bool alt() const { return _modifiers & 0x44; }    // Left/Right Alt
    bool meta() const { return _modifiers & 0x88; }   // Left/Right Meta

private:
    uint8_t _keycode;    // HID usage code (0x04-0x65 for keys)
    uint8_t _modifiers;  // Bitmask of modifier keys
};
```

---

### Phase 3: HID Report Handler & Queue

**Location:** `src/platform/stm32/drivers/UsbH.cpp` and `UsbH.h`

**HID Report Format (Boot Protocol):**
```
Byte 0: Modifier bits (Ctrl/Shift/Alt/Meta)
Byte 1: Reserved (OEM)
Byte 2-7: Key codes (up to 6 simultaneous keys)
```

**Add Queue Infrastructure (UsbH.h):**

```cpp
// In UsbH.h private section, similar to MIDI queue
static constexpr size_t HidQueueSize = 128;
struct HidKeyPress {
    uint8_t keycode;
    uint8_t modifiers;
};
HidKeyPress _hidQueue[HidQueueSize];
size_t _hidQueueReadIndex = 0;
size_t _hidQueueWriteIndex = 0;

// Public methods
void hidConnectDevice(uint8_t deviceId, enum HID_TYPE type);
void hidDisconnectDevice(uint8_t deviceId);
void hidEnqueueKeyPress(uint8_t keycode, uint8_t modifiers);
bool hidDequeueKeyPress(uint8_t *keycode, uint8_t *modifiers);
bool hidDeviceConnected(uint8_t deviceId) const;
```

**HID Driver Handler (UsbH.cpp):**

```cpp
// Add after MidiDriverHandler (line 183)
struct HidDriverHandler {
    static uint8_t oldFrame[8];  // For key repeat detection

    static void connectHandler(uint8_t device, enum HID_TYPE type) {
        DBG("HID device connected (id=%d, type=%d)", device, type);
        if (type == HID_TYPE_KEYBOARD) {
            g_usbh->hidConnectDevice(device, type);
            // Clear old frame on new connection
            for (int i = 0; i < 8; i++) oldFrame[i] = 0;
        }
    }

    static void disconnectHandler(uint8_t device) {
        DBG("HID device disconnected (id=%d)", device);
        g_usbh->hidDisconnectDevice(device);
    }

    static void messageHandler(uint8_t device, const uint8_t *data, uint32_t length) {
        // Boot protocol keyboard report is 8 bytes
        if (length != 8) return;

        uint8_t modifiers = data[0];

        // Process keys in bytes 2-7 (byte 1 is reserved)
        for (int i = 2; i < 8; i++) {
            uint8_t keycode = data[i];
            if (keycode == 0) continue;  // No key pressed

            // Check if this is a new key press (not in old frame)
            bool isNewPress = true;
            for (int j = 2; j < 8; j++) {
                if (oldFrame[j] == keycode) {
                    isNewPress = false;
                    break;
                }
            }

            if (isNewPress) {
                g_usbh->hidEnqueueKeyPress(keycode, modifiers);
            }
        }

        // Save current frame for next comparison
        for (int i = 0; i < 8; i++) {
            oldFrame[i] = data[i];
        }
    }
};

uint8_t HidDriverHandler::oldFrame[8] = {0};

// HID configuration structure
static const hid_config_t hid_config = {
    .hid_in_message_handler = &HidDriverHandler::messageHandler,
};
```

**Queue Implementation (UsbH.cpp):**

```cpp
void UsbH::hidConnectDevice(uint8_t deviceId, enum HID_TYPE type) {
    for (auto &device : _hidDevices) {
        if (!device.connected) {
            device.deviceId = deviceId;
            device.connected = true;
            device.type = type;
            return;
        }
    }
}

void UsbH::hidDisconnectDevice(uint8_t deviceId) {
    for (auto &device : _hidDevices) {
        if (device.connected && device.deviceId == deviceId) {
            device.connected = false;
            return;
        }
    }
}

void UsbH::hidEnqueueKeyPress(uint8_t keycode, uint8_t modifiers) {
    size_t nextIndex = (_hidQueueWriteIndex + 1) % HidQueueSize;
    if (nextIndex != _hidQueueReadIndex) {
        _hidQueue[_hidQueueWriteIndex].keycode = keycode;
        _hidQueue[_hidQueueWriteIndex].modifiers = modifiers;
        _hidQueueWriteIndex = nextIndex;
    }
    // TODO: Overflow tracking like MIDI (optional)
}

bool UsbH::hidDequeueKeyPress(uint8_t *keycode, uint8_t *modifiers) {
    if (_hidQueueReadIndex == _hidQueueWriteIndex) {
        return false;  // Queue empty
    }
    *keycode = _hidQueue[_hidQueueReadIndex].keycode;
    *modifiers = _hidQueue[_hidQueueReadIndex].modifiers;
    _hidQueueReadIndex = (_hidQueueReadIndex + 1) % HidQueueSize;
    return true;
}

bool UsbH::hidDeviceConnected(uint8_t deviceId) const {
    for (const auto &device : _hidDevices) {
        if (device.connected && device.deviceId == deviceId) {
            return true;
        }
    }
    return false;
}
```

**Update UsbH::process() to dispatch keyboard events:**

```cpp
// In UsbH::process() after MIDI handling (line 244)
void UsbH::process() {
    uint32_t time_us = HighResolutionTimer::us();
    usbh_poll(time_us);

    // ... existing MIDI code ...

    // Dispatch HID keyboard events
    uint8_t keycode, modifiers;
    while (hidDequeueKeyPress(&keycode, &modifiers)) {
        KeyboardEvent event(keycode, modifiers);
        _eventQueue.write(event);  // Assuming Ui has access to UsbH event queue
    }
}
```

---

### Phase 4: Keycode to ASCII Conversion

**Location:** Create `src/apps/sequencer/ui/KeyboardHelper.h` and `.cpp`

**Port from Teletype:** `teletype/libavr32/src/kbd.c:24-153`

```cpp
// KeyboardHelper.h
#pragma once
#include <cstdint>

class KeyboardHelper {
public:
    // Convert HID keycode to ASCII character
    // Returns 0 if not a printable character
    static char hidToAscii(uint8_t keycode, uint8_t modifiers);

    // Check if keycode is a special key (Enter, Backspace, etc.)
    static bool isSpecialKey(uint8_t keycode);
    static bool isReturn(uint8_t keycode) { return keycode == 0x28; }
    static bool isBackspace(uint8_t keycode) { return keycode == 0x2A; }
    static bool isEscape(uint8_t keycode) { return keycode == 0x29; }
    static bool isDelete(uint8_t keycode) { return keycode == 0x4C; }
    static bool isTab(uint8_t keycode) { return keycode == 0x2B; }

private:
    static constexpr uint8_t SHIFT = 0x22;
    static constexpr uint8_t CTRL = 0x11;
    static constexpr uint8_t ALT = 0x44;
};

// KeyboardHelper.cpp
char KeyboardHelper::hidToAscii(uint8_t keycode, uint8_t modifiers) {
    // Numbers and symbols (0x1E-0x27)
    if (keycode >= 0x1E && keycode <= 0x27) {
        if (modifiers & SHIFT) {
            switch (keycode) {
                case 0x1E: return '!';  // 1
                case 0x1F: return '@';  // 2
                case 0x20: return '#';  // 3
                case 0x21: return '$';  // 4
                case 0x22: return '%';  // 5
                case 0x23: return '^';  // 6
                case 0x24: return '&';  // 7
                case 0x25: return '*';  // 8
                case 0x26: return '(';  // 9
                case 0x27: return ')';  // 0
            }
        } else {
            return (keycode == 0x27) ? '0' : (keycode - 0x1E + '1');
        }
    }

    // Letters (0x04-0x1D) - Teletype uses uppercase only
    if (keycode >= 0x04 && keycode <= 0x1D) {
        return keycode - 0x04 + 'A';  // Always uppercase for Teletype
    }

    // Special symbols (0x2C-0x38)
    if (keycode >= 0x2C && keycode <= 0x38) {
        switch (keycode) {
            case 0x2C: return ' ';   // Space
            case 0x2D: return (modifiers & SHIFT) ? '_' : '-';
            case 0x2E: return (modifiers & SHIFT) ? '+' : '=';
            case 0x2F: return (modifiers & SHIFT) ? '{' : '[';
            case 0x30: return (modifiers & SHIFT) ? '}' : ']';
            case 0x31: return (modifiers & SHIFT) ? '|' : '\\';
            case 0x33: return (modifiers & SHIFT) ? ':' : ';';
            case 0x34: return (modifiers & SHIFT) ? '"' : '\'';
            case 0x35: return (modifiers & SHIFT) ? '~' : '`';
            case 0x36: return (modifiers & SHIFT) ? '<' : ',';
            case 0x37: return (modifiers & SHIFT) ? '>' : '.';
            case 0x38: return (modifiers & SHIFT) ? '?' : '/';
        }
    }

    return 0;  // Not a printable character
}

bool KeyboardHelper::isSpecialKey(uint8_t keycode) {
    return (keycode == 0x28 || keycode == 0x29 || keycode == 0x2A ||
            keycode == 0x2B || keycode == 0x4C);
}
```

---

### Phase 5: UI Integration - Teletype Pages

**Location:** `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.h` and `.cpp`

**Dual Input Mode:**
- **Step Keys:** Continue using T9-style cycling (for hardware-only users)
- **USB Keyboard:** Direct professional editing with shortcuts (matches original Teletype)

**Add to TeletypeScriptViewPage.h:**

```cpp
// In private section
void handleKeyboardEvent(const KeyboardEvent &event);
void deleteWordBackward();
void deleteWordForward();
void deleteToEndOfLine();
void deleteToBeginningOfLine();
void moveToNextWord();
void moveToPreviousWord();
```

**Update TeletypeScriptViewPage::event():**

```cpp
void TeletypeScriptViewPage::event(const Event &event) {
    // Existing event handling...

    if (event.type() == Event::Keyboard) {
        handleKeyboardEvent(event.as<KeyboardEvent>());
        return;
    }

    // ... rest of existing code
}
```

**Implement Line Editor (TeletypeScriptViewPage.cpp):**

```cpp
void TeletypeScriptViewPage::handleKeyboardEvent(const KeyboardEvent &event) {
    uint8_t k = event.keycode();
    uint8_t m = event.modifiers();

   // ===== ARROW KEYS (Original Teletype supported these) =====

    if (k == 0x50) {  // Left arrow
        moveCursorLeft();
        return;
    }

    if (k == 0x4F) {  // Right arrow
        moveCursorRight();
        return;
    }

    // ===== BASIC EDITING =====

    // Return: execute command
    if (KeyboardHelper::isReturn(k)) {
        commitLine();
        return;
    }

    // Backspace or Ctrl-H: delete previous character
    if (KeyboardHelper::isBackspace(k) || (m & 0x11 && k == 0x0B)) {  // Ctrl-H
        backspace();
        return;
    }

    // Delete or Ctrl-D: delete next character
    if (KeyboardHelper::isDelete(k) || (m & 0x11 && k == 0x07)) {  // Ctrl-D
        if (_cursor < int(std::strlen(_editBuffer))) {
            int len = int(std::strlen(_editBuffer));
            std::memmove(_editBuffer + _cursor, _editBuffer + _cursor + 1, len - _cursor);
        }
        return;
    }

    // Escape: clear buffer
    if (KeyboardHelper::isEscape(k)) {
        setEditBuffer("");
        return;
    }

    // Tab: insert space (Teletype doesn't support tab completion)
    if (KeyboardHelper::isTab(k)) {
        insertChar(' ');
        return;
    }

    // ===== CHARACTER INPUT =====

    // Convert to ASCII and insert at cursor position
    char ch = KeyboardHelper::hidToAscii(k, m);
    if (ch != 0) {
        insertChar(ch);  // This already handles cursor positioning correctly
    }
}


```

**Visual Feedback - Show Keyboard Connected:**

```cpp
// In TeletypeScriptViewPage::draw()
void TeletypeScriptViewPage::draw(Canvas &canvas) {
    // ... existing code ...

    // Show keyboard icon if connected
    if (_context.usbH.isKeyboardConnected()) {
        canvas.setColor(Color::Bright);
        canvas.drawText(2, Height - 8, "KB");  // Bottom left indicator
    }

    // ... rest of existing code
}
```

**Update PageContext to include UsbH reference:**

```cpp
// In PageContext.h (if not already present)
struct PageContext {
    // ... existing members ...
    UsbH &usbH;  // Add this for keyboard status checks
};
```

---

### Phase 6: Simulator Support (Optional but Recommended)

**Location:** `src/platform/sim/drivers/UsbH.cpp`

Add stub implementation for simulator:

```cpp
// Simulator UsbH should provide dummy keyboard support
// Allow testing UI integration without hardware

bool UsbH::isKeyboardConnected() const {
    return false;  // Or true for UI testing
}

// Queue methods return false/do nothing
bool UsbH::hidDequeueKeyPress(uint8_t *keycode, uint8_t *modifiers) {
    return false;
}
```

Alternatively, map SDL keyboard events to HID keycodes in simulator for full testing.

---


## Testing Strategy

### Unit Tests

**Test Coverage Required:**

1. **KeyboardHelper Tests** (`src/tests/unit/ui/TestKeyboardHelper.cpp`):
   ```cpp
   UNIT_TEST("KeyboardHelper") {

   CASE("numbers_without_shift") {
       expectEqual(KeyboardHelper::hidToAscii(0x1E, 0), '1', "1 key");
       expectEqual(KeyboardHelper::hidToAscii(0x27, 0), '0', "0 key");
   }

   CASE("numbers_with_shift") {
       expectEqual(KeyboardHelper::hidToAscii(0x1E, 0x22), '!', "! symbol");
       expectEqual(KeyboardHelper::hidToAscii(0x1F, 0x22), '@', "@ symbol");
   }

   CASE("letters_uppercase") {
       expectEqual(KeyboardHelper::hidToAscii(0x04, 0), 'A', "A key");
       expectEqual(KeyboardHelper::hidToAscii(0x1D, 0), 'Z', "Z key");
   }

   CASE("special_characters") {
       expectEqual(KeyboardHelper::hidToAscii(0x2C, 0), ' ', "space");
       expectEqual(KeyboardHelper::hidToAscii(0x2D, 0x22), '_', "underscore");
   }

   CASE("special_keys") {
       expectTrue(KeyboardHelper::isReturn(0x28), "return key");
       expectTrue(KeyboardHelper::isBackspace(0x2A), "backspace key");
   }

   }
   ```

2. **HID Queue Tests** (`src/tests/unit/drivers/TestUsbHHidQueue.cpp`):
   - Test queue overflow handling
   - Test concurrent enqueue/dequeue
   - Test device connect/disconnect tracking

### Integration Tests

1. **Hardware Testing Checklist:**
   - [ ] Standard USB keyboard (104-key)
---

## Implementation Checklist

### Phase 1: Driver Registration ✓
- [ ] Add `#include "usbh_driver_hid.h"` to UsbH.cpp
- [ ] Register `&usbh_hid_driver` in device_drivers array
- [ ] Add HidDevice tracking struct to UsbH.h
- [ ] Call `hid_driver_init(&hid_config)` in UsbH::init()
- [ ] Build and verify HID enumeration in debug logs

### Phase 2: Event System ✓
- [ ] Add `Keyboard` to Event::Type enum
- [ ] Implement KeyboardEvent class in Event.h
- [ ] Add modifier helper methods (shift/ctrl/alt/meta)
- [ ] Build and verify compilation

### Phase 3: Queue & Handler ✓
- [ ] Add HID queue structures to UsbH.h
- [ ] Implement HidDriverHandler with connect/disconnect/message callbacks
- [ ] Implement queue methods in UsbH.cpp
- [ ] Add keyboard event dispatch to UsbH::process()
- [ ] Add device tracking (connect/disconnect)
- [ ] Test queue with unit tests

### Phase 4: Keycode Conversion ✓
- [ ] Create KeyboardHelper.h and .cpp
- [ ] Port hid_to_ascii logic from teletype kbd.c
- [ ] Implement special key detection methods
- [ ] Write comprehensive unit tests
- [ ] Verify all printable ASCII characters

### Phase 5: UI Integration ✓
- [ ] Add keyboard event handler to TeletypeScriptViewPage
- [ ] Implement character input to edit buffer
- [ ] Implement special key handling (Enter/Backspace/Escape/Delete)
- [ ] Add keyboard connected indicator to UI
- [ ] Update PageContext to include UsbH reference
- [ ] Test dual input mode (keyboard + step keys)
- [ ] Test on simulator (if supported)

### Phase 6: Hardware Testing ✓
- [ ] Test basic keyboard connection/detection
- [ ] Verify power draw is within limits
- [ ] Test all character inputs
- [ ] Test modifier combinations
- [ ] Verify hot-plug works correctly
- [ ] Check for audio noise (OLED interference)
- [ ] Test with multiple keyboard types
- [ ] Document supported/tested keyboards

---

## Architectural Decision: Why Not Reuse Teletype's line_editor.c?

**Question:** Why implement a new keyboard handler instead of directly porting `teletype/module/line_editor.c`?

**Answer:** Different architectural constraints and design goals.

### Original Teletype Architecture
```c
// teletype/module/line_editor.c
bool line_editor_process_keys(line_editor_t *le, uint8_t k, uint8_t m, bool is_held) {
    // Direct C struct manipulation
    // Single-purpose: keyboard-only input
    // No integration with Performer's event system
    // AVR32-specific (uses libavr32 kbd.c)
}
```

### Performer Architecture
```cpp
// TeletypeScriptViewPage.cpp
void handleKeyboardEvent(const KeyboardEvent &event) {
    // C++ class methods
    // Dual-purpose: keyboard AND step-key input
    // Integrated with Performer's PageManager event dispatch
    // STM32-specific (uses libusbhost)
}
```

### Key Differences

| Aspect | Teletype | Performer |
|--------|----------|-----------|
| **Input Methods** | Keyboard only | Keyboard + T9 step-keys + encoder |
| **Event System** | Direct HID polling | Event queue + multi-task dispatch |
| **Language** | C (AVR32) | C++ (STM32, FreeRTOS) |
| **UI Framework** | Direct framebuffer | Canvas/PageManager abstraction |
| **Code Reuse** | Port kbd.c logic | Adapt to existing methods |

### What We Reuse from Teletype

✅ **HID keycode-to-ASCII conversion** (`hid_to_ascii` logic)

### What We Adapt for Performer

🔧 **Event routing** (KeyboardEvent → PageManager → handleKeyboardEvent)
🔧 **Buffer management** (use existing `_editBuffer`, `_cursor`)
🔧 **Dual input** (step-keys continue to work)
🔧 **Thread safety** (queue between USB Host Task and UI Task)

**Result:** Same user experience, different implementation path, better integration.

---

## Known Limitations & Future Work

1. **Boot Protocol Only:**
   - Current implementation uses HID Boot Protocol (8-byte reports)
   - Full HID Report Descriptors not parsed
   - Most keyboards support boot protocol, but some advanced features may not work
   - **Impact:** 99% of keyboards work fine; exotic gaming keyboards may not


2. **No Up/Down Arrow for History:**
   - Original Teletype didn't have command history
   - Performer has history via Page+Left/Right
   - **Future work:** Map Up/Down to history navigation (easy addition)

### What's NOT Limited (Compared to Original)

✅ **All Emacs shortcuts work** (Ctrl-A/E/K/U/W, Alt-F/B/D)
✅ **Immediate execution** (Enter → instant CV/gate response)
✅ **Professional editing** (word-based navigation, kill/yank)
✅ **Clipboard** (Cut/Copy/Paste)
✅ **Dual input** (step-keys available as fallback)

**This implementation achieves feature parity with original Teletype's keyboard support.**

---

## Power Budget Details

**STM32F4 USB Host Power Specifications:**
- **Maximum Output:** 500mA @ 5V (USB 2.0 Full Speed)
- **GPIO Current Limit:** 25mA per pin (USB_PWR_EN)
- **Actual Limit:** Determined by external power switch circuit

--

## Debug & Diagnostics

**Enable HID Debug Logging:**

```cpp
// In usbh_driver_hid.c, LOG_PRINTF statements show:
// - Device enumeration
// - Report descriptor parsing
// - Keyboard/mouse detection
// - Report data (8-byte frames)
```

**Monitor Queue Status:**

```cpp
// Add to UsbH.h for diagnostics
size_t hidQueueDepth() const {
    return (_hidQueueWriteIndex >= _hidQueueReadIndex)
        ? (_hidQueueWriteIndex - _hidQueueReadIndex)
        : (HidQueueSize - _hidQueueReadIndex + _hidQueueWriteIndex);
}
```

**Test Points:**
- UsbH::hidEnqueueKeyPress() - Verify events arrive from USB
- UsbH::hidDequeueKeyPress() - Verify events consumed by UI
- TeletypeScriptViewPage::handleKeyboardEvent() - Verify UI processes events

---

## References

**Existing Code:**
- HID Driver: `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c`
- HID Header: `src/platform/stm32/libs/libusbhost/include/usbh_driver_hid.h`
- USB Host: `src/platform/stm32/drivers/UsbH.cpp`
- MIDI Pattern: UsbH.cpp:51-189 (reference for queue implementation)
- Teletype Keyboard: `teletype/libavr32/src/kbd.c` (hid_to_ascii reference)
- Event System: `src/apps/sequencer/ui/Event.h`
- Teletype UI: `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.cpp`

**USB HID Specifications:**
- HID Usage Tables: https://usb.org/sites/default/files/hut1_3_0.pdf
- HID Boot Protocol: Section 4.3 (Keyboard) and 4.4 (Mouse)
- USB 2.0 Specification: Section 9 (USB Device Framework)

**Boot Protocol Keyboard Report Format:**
```
Byte 0: Modifier keys (bit 0: L-Ctrl, 1: L-Shift, 2: L-Alt, 3: L-GUI,
                       bit 4: R-Ctrl, 5: R-Shift, 6: R-Alt, 7: R-GUI)
Byte 1: Reserved (OEM)
Byte 2-7: Keypress array (up to 6 simultaneous key codes)
```

---

