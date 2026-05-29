# HID Keyboard Layering — extract decode out of UI into `hid/`

_2026-05-29. Validated design. Scope: relocate USB-keyboard HID decode out of the UI layer into a neutral `hid/` module, give USB mouse a stubbed home there, no behavior change._

## Problem

`ui/KeyboardManager` straddles three layers in one UI-folder class:

1. **HID protocol decode** — `hidKeycodeToAscii`, `hidKeycodeToStep`, the `stepKeycodes` table. Pure USB-HID knowledge, zero UI dependency.
2. **Event buffering** — a 16-deep ring buffer of raw key events.
3. **UI dispatch** — `process()` into `PageManager`/`Screensaver`/`KeyState`, plus connect-message UI.

Layers 1-2 don't belong next to `PageManager`. The active **usb-mouse-to-route** task wants HID *mouse* input as routing-source CV — a non-UI consumer — and today has no shared HID-decode layer to use; it would either duplicate decode or reach sideways into UI code.

## Goal

A shared device-layer for HID decode that both the UI keyboard path and the engine/routing mouse path can include, without forcing keyboard through an abstraction it doesn't need.

## Decisions

- **Approach: sibling decode headers, no device base class.** Keyboard and mouse share almost no decode logic (keycode→ascii/step vs report-bytes→dx/dy/buttons). The genuine shared surface is the raw-report buffering and the Engine pump pattern — small and concrete. A `HidDevice` base / generic intake would be a framework for two dissimilar devices (the same over-abstraction trap flagged on the Sequence base). Rejected.
- **Location: new `apps/sequencer/hid/` module.** Decode is HID-protocol knowledge owned by neither UI nor engine. Both layers include *down* into `hid/`, never sideways into each other.
- **Stateless decode → free functions in a namespace**, not a class with statics.
- **Engine handler signature unchanged this pass.** `KeyboardReceiveHandler(keycode, modifiers, pressed)` stays; the typed `Report` is constructed at the `enqueue` boundary. Touching Engine HID handlers is the mouse task's territory.

## Module structure

```
apps/sequencer/hid/
  Keyboard.h / .cpp   keycode→ascii/step, keycode tables, raw key-event struct
  Mouse.h             STUB — decode/accumulator deferred to usb-mouse-to-route
```

### `hid/Keyboard.h`

```cpp
namespace Hid::Keyboard {
    struct Report {            // raw HID key event (was KeyboardManager::ReceiveKeyboardEvent)
        uint8_t keycode, modifiers, pressed;
    };
    char keycodeToAscii(uint8_t keycode, uint8_t modifiers);
    int  keycodeToStep(uint8_t keycode);   // 0..15, or -1 if not a step key
}
```

- The two functions move verbatim (bodies unchanged) into `hid/Keyboard.cpp`.
- The raw-event struct becomes the shared `Hid::Keyboard::Report` instead of a private nested struct.
- All current callers are internal to `KeyboardManager.cpp` (two call sites) — no external consumers, fully self-contained.

### `hid/Mouse.h` (stub)

Header-only placeholder. No functions, no committed struct (the `[buttons, dx, dy, wheel]` boot-report shape is an unverified hypothesis in the task notes, not frozen here). Documents that decode and the `[0,1]` accumulator math are deferred to usb-mouse-to-route, which is gated on the libusbhost enumeration crash root-cause. Purpose: the home exists and is named, so the "where does mouse decode live" decision is already made when that task resumes.

## `KeyboardManager` changes

- ring buffer → `RingBuffer<Hid::Keyboard::Report, 16>`
- `process()` calls `Hid::Keyboard::keycodeToAscii(...)` / `keycodeToStep(...)`
- drops the two static decls + nested struct; `#include "hid/Keyboard.h"`
- stays in `ui/`, keeps buffering + dispatch — only loses the decode

## Build & test

- Include root is `apps/sequencer/`, so `#include "hid/Keyboard.h"` resolves from UI and engine.
- CMake: add `hid/Keyboard.cpp` to the explicit source list. Mouse.h is header-only — no entry.
- Test: `src/tests/unit/sequencer/TestHidKeyboard.cpp` — assert `keycodeToAscii` across letters / numbers / shifted symbols, and `keycodeToStep` for the 16 step keys + the −1 miss. Add target to the test `CMakeLists.txt` (same pattern as `TestTeletypeV2*`).
- Verify: sim build (compiles consumers + runs the test), then STM32 release (confirms the pure code move is RAM/flash-neutral within linker noise).

## Out of scope

Engine handler-signature change, any `KeyboardManager` behavior change, mouse decode implementation. The mouse task picks up an existing home.
