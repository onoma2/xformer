# USB HID Keyboard Implementation — Task Status

Status: Active — USB keyboard end-to-end working, routed through Engine
Priority: High

## Overview

USB keyboard input flows through Engine to TeletypeScriptViewPage. The architecture mirrors MIDI: UsbH → Engine polls → handler → Ui ring buffer → page dispatch. All external input now goes through Engine.

## Key Files

| File | Role |
|------|------|
| `src/apps/sequencer/ui/Event.h` | KeyboardEvent class with HID keycode constants, modifiers, ASCII decode |
| `src/apps/sequencer/ui/Page.h/cpp` | keyboard() virtual handler + dispatchEvent routing |
| `src/apps/sequencer/ui/Ui.h/cpp` | Keyboard ring buffer, handleKeyboard(), hidKeycodeToAscii(), enqueueKeyboardEvent() |
| `src/apps/sequencer/ui/Screensaver.h/cpp` | consumeKey(KeyboardEvent) — wake on any key |
| `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.h/cpp` | keyboard() override — full keyboard input for Teletype script editor |
| `src/apps/sequencer/engine/Engine.h/cpp` | Keyboard/HID handlers, receiveKeyboard(), UsbH ref |
| `src/platform/stm32/drivers/UsbH.h/cpp` | recvKey() polling API, HID connect/disconnect callbacks |
| `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c` | HID key deduplication, ring buffer |
| `src/apps/sequencer/Sequencer.cpp` | Engine←→UsbH wiring, debug message callback |

## Decisions log

- 2026-05-10: Ring buffer approach for HID keys (ISR-safe, no rendering in USB context)
- 2026-05-10: Fixed `hid_get_type()` inversion, `HID_TYPE_OTHER` for protocol 0x00
- 2026-05-11: Removed debug callback that broke enumeration
- 2026-05-11: Keyboard keypresses verified end-to-end
- 2026-05-10: Added KeyboardEvent, Page::keyboard(), TeletypeScriptViewPage::keyboard()
- 2026-05-10: Fixed type confusion crash — context was `Ui*` cast to `MessageManager*`
- 2026-05-10: HID key deduplication in usbh_driver_hid.c (prev_keys comparison)
- 2026-05-10: Routed keyboard through Engine mirroring MIDI pattern
- 2026-05-10: Converted HID connect/disconnect messages to human-readable strings
- 2026-05-10: Keyboard letters converted to uppercase for Teletype parser compatibility
- 2026-05-10: Analyzed hardware Teletype keyboard shortcuts — multi-line selection clipboard needs architecture change, single-line Ctrl+C/V/X are safe to add now

## Completed steps
- [x] Fix `hid_is_connected()` / `hid_get_type()` inversions
- [x] Fix `analyze_descriptor()` interface_number for all protocols
- [x] HID key deduplication in C driver layer
- [x] KeyboardEvent class, Page::keyboard() dispatch, TeletypeScriptViewPage::keyboard() handler
- [x] Fix type confusion crash (Ui* vs MessageManager* context)
- [x] Route keyboard through Engine (mirrors MIDI pattern)
- [x] Human-readable HID connect/disconnect messages
- [x] Uppercase letter conversion for Teletype parser
- [x] Add Ctrl+Home/End, Delete key support

## Next actions
1. Add Ctrl+C/V/X clipboard shortcuts mapping to existing copyLine/pasteLine
2. Add Shift+Enter for insert-and-advance
3. Add [/] for script navigation
