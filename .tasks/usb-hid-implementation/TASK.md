# USB HID Keyboard Implementation — Task Status

Status: Active — USB keyboard end-to-end working, routed through Engine
Priority: High

## Overview

USB keyboard input flows through Engine to both TeletypeScriptViewPage and TeletypePatternViewPage. The architecture mirrors MIDI: UsbH → Engine polls → handler → Ui ring buffer → page dispatch. All external input now goes through Engine.

## Key Files

| File | Role |
|------|------|
| `src/apps/sequencer/ui/Event.h` | KeyboardEvent class with HID keycode constants, modifiers, ASCII decode |
| `src/apps/sequencer/ui/Page.h/cpp` | keyboard() virtual handler + dispatchEvent routing |
| `src/apps/sequencer/ui/Ui.h/cpp` | Keyboard ring buffer, handleKeyboard(), hidKeycodeToAscii(), enqueueKeyboardEvent() |
| `src/apps/sequencer/ui/Screensaver.h/cpp` | consumeKey(KeyboardEvent) — wake on any key |
| `src/apps/sequencer/ui/pages/TeletypeScriptViewPage.h/cpp` | keyboard() override — script editor |
| `src/apps/sequencer/ui/pages/TeletypePatternViewPage.h/cpp` | keyboard() override — pattern editor |
| `src/apps/sequencer/engine/Engine.h/cpp` | Keyboard/HID handlers, receiveKeyboard(), UsbH ref |
| `src/platform/stm32/drivers/UsbH.h/cpp` | recvKey() polling API, HID connect/disconnect callbacks |
| `src/platform/stm32/libs/libusbhost/src/usbh_driver_hid.c` | HID key deduplication, ring buffer |
| `src/apps/sequencer/Sequencer.cpp` | Engine←→UsbH wiring, debug message callback |

## Keyboard Shortcuts — TeletypeScriptViewPage (Script Editor)

Matches hardware Teletype edit mode where applicable. Hardware has multi-line selection model; Performer uses single-line edit buffer.

| Shortcut | Action | Hardware match? |
|----------|--------|-----------------|
| Enter | Commit line, advance | Yes |
| Shift+Enter | Commit, insert blank line | Yes (inserts new cmd) |
| Backspace | Delete char before cursor | Yes |
| Delete | Delete char after cursor | No hardware equivalent |
| Arrows | Cursor left/right, history up/down | Yes |
| Escape | Clear edit buffer | No hardware equivalent |
| Tab | Insert space | No hardware equivalent |
| Home/End | Cursor start/end | Yes |
| Ctrl+Home/End | Cursor start/end | Yes |
| Ctrl+C | Copy line | Yes |
| Ctrl+V | Paste line | Yes |
| Ctrl+X | Cut line (copy+clear) | Yes |
| [ / ] | Previous/next script | Yes |
| Letters a-z | Auto-uppercased for parser | Required by parser |

**What's NOT implemented** (requires architecture changes):
- Ctrl+Z (undo) — needs undo stack
- Multi-line selection clipboard — needs selection model
- Alt+Up/Down (move lines) — needs selection model
- Ctrl+N/P (emacs nav) — trivial, arrows already work

## Keyboard Shortcuts — TeletypePatternViewPage (Pattern Editor)

Matches hardware Teletype pattern mode. Performer uses same cell-based editing model.

| Shortcut | Action | Hardware match? |
|----------|--------|-----------------|
| ↑/↓ | Move up/down | Yes |
| Alt+↑/↓ | Page up/down (8 rows) | Yes |
| ←/→ | Move left/right | Yes |
| Enter | Commit edit, advance | Yes |
| Shift+Enter | Insert/duplicate row | Yes |
| Backspace | Backspace digit | Yes |
| Delete | Delete row | Alt+Delete in HW |
| Space | Toggle 0/1 | Yes |
| Escape | Cancel edit | No hardware equivalent |
| Ctrl+Home/End | Jump to start/end | No direct match |
| Ctrl+C/V/X | Copy/paste/cut value | Alt+C/V/X in HW |
| [ / ] | Decrement/increment by 1 | Yes |
| Digits 0-9 | Numeric entry | Yes |
| - / _ | Negate value | Yes |

## Analysis: Universal Keyboard Shortcuts Across Pages

The hardware Teletype has per-mode keyboard handling (edit_mode.c, pattern_mode.c, etc.) — each mode defines its own shortcuts. This matches Performer's architecture where each Page can override `keyboard()`.

**What could be universal** (Page base class, available everywhere):
- Space: toggle play/stop
- Escape: close current page/modal

**What is page-specific** and should stay that way:
- Ctrl+C/V/X — different meaning per page (text clipboard vs value clipboard vs pattern clipboard)
- Arrow keys — navigation semantics vary per page
- [/] — script nav vs value increment
- Enter — commit edits vs confirm dialogs

## Key Files

### New or modified for TeletypePatternViewPage keyboard
- `TeletypePatternViewPage.h` — added `_valueCopyBuffer` (int16_t) member, `keyboard()` override
- `TeletypePatternViewPage.cpp` — 184-line `keyboard()` handler with all shortcuts

## Decisions log

- 2026-05-10: Ring buffer approach for HID keys (ISR-safe, no rendering in USB context)
- 2026-05-10: Fixed `hid_get_type()` inversion
- 2026-05-11: Keyboard keypresses verified end-to-end
- 2026-05-10: KeyboardEvent class, Page dispatch, TeletypeScriptViewPage handler
- 2026-05-10: Fixed type confusion crash (Ui* vs MessageManager* context)
- 2026-05-10: Route keyboard through Engine (mirrors MIDI pattern)
- 2026-05-10: Uppercase letter conversion for Teletype parser
- 2026-05-10: Analyzed hardware Teletype shortcuts — single-line clipboard safe, multi-line needs arch change
- 2026-05-10: Added Ctrl+C/V/X, Shift+Enter, [/] to ScriptViewPage
- 2026-05-10: Added full keyboard handler to PatternViewPage matching hardware

## Completed steps
- [x] HID driver fixes (hid_get_type, dedup, ring buffer)
- [x] KeyboardEvent class, page dispatch, Engine routing
- [x] Ui keyboard ring buffer, handleKeyboard(), ASCII mapping
- [x] TeletypeScriptViewPage::keyboard() — char input, Ctrl+C/V/X, Shift+Enter, [/], etc.
- [x] TeletypePatternViewPage::keyboard() — arrow nav, Ctrl+C/V/X, Enter, digits, Space, [/], etc.
- [x] Screensaver wake on keyboard
- [x] Human-readable HID connect/disconnect
- [x] Task wiki updated with full shortcut analysis

## Next actions
1. Test both keyboards on hardware
2. Consider adding Space/Escape as universal shortcuts in Page base class
3. After hardware test: squash/cleanup branch for merge
