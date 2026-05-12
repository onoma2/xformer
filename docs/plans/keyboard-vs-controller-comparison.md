---
id: keyboard-vs-controller-comparison
schema: analysis
date: 2026-05-11
topic: keyboard-vs-controller
---

# KeyboardManager vs ControllerManager — Structural Comparison

## Side-by-Side

| Aspect | ControllerManager (Launchpad) | KeyboardManager (Proposed) |
|--------|-------------------------------|---------------------------|
| **Role** | External hardware controller (grid + buttons + LEDs) | USB keyboard input translator |
| **Owned by** | `Ui` | `Ui` |
| **Constructor takes** | `Model &, Engine &` | `Engine &, MessageManager &` |
| **Owns sub-devices** | Yes — `LaunchpadController` → `LaunchpadDevice` variants via `Container<>` | No — no device hierarchy, just raw HID events |
| **Hot-plug lifecycle** | `connect(vendorId, productId)` → create controller → `disconnect()` → destroy | Always-on; HID connect/disconnect just changes messages |
| **`init()` flow** | None (constructor is enough) | Registers 3 Engine callbacks: keyboard receive, HID connect, HID disconnect |
| **`update()` / `process()`** | `_controller->update()` — draws LEDs to Launchpad display | Drains ring buffer, translates keycodes, synthesizes `KeyEvent`/`KeyboardEvent`, dispatches through `Screensaver` + `PageManager` |
| **Input direction** | **Bidirectional**: receives MIDI Note On/Off from Launchpad, sends LED colors back | **Unidirectional**: receives HID keycodes, dispatches events inward. No feedback to keyboard. |
| **Event dispatch** | Translates Launchpad button → model actions directly (bypasses `PageManager`) | Synthesizes `KeyEvent`/`KeyboardEvent` → dispatches through `PageManager` → pages consume |
| **State mutation** | Owns its own button state (`_device->buttonState()`) | Mutates `Ui`'s `KeyState` (passed by reference) for step-key synthesis |
| **Model access** | Direct — reads/writes `Project`, `PlayState`, sequences, track selection | None — pure translation layer, zero model knowledge |
| **Engine access** | `sendMidi()` for LED feedback, `togglePlay()`, reads `trackEngine()` | `isSuspended()` check only, handler registration at init |
| **Memory** | ~hundreds of bytes (device container, LED buffers, per-mode state) | ~76 bytes (ring buffer + handler pointers) |
| **Complexity** | ~1200 lines (full grid UI, drawing, navigation, modes) | ~85 lines (two static translators + ring buffer drain + dispatch) |

## Key Architectural Difference: Event Dispatch Path

This is the most important difference.

### ControllerManager: Direct Model Manipulation

```
Launchpad button press
  → MIDI Note On → LaunchpadDevice::recvMidi()
    → buttonDown(row, col) → dispatchButtonEvent()
      → sequenceEditStep() / patternButton() / etc.
        → Direct model mutation: sequence.step(i).toggleGate()
        → Direct engine calls: _engine.togglePlay()
```

ControllerManager **bypasses the page stack entirely**. It's a parallel input channel that manipulates the model/engine directly. The OLED UI is unaware of Launchpad actions except by observing model changes.

### KeyboardManager: Page Stack Integration

```
USB HID key press
  → Engine::receiveKeyboard() → callback → KeyboardManager::enqueue()
    → KeyboardManager::process()
      → hidKeycodeToStep() matches? → synthesize KeyEvent
        → screensaver.consumeKey(keyEvent)
        → pageManager.dispatchEvent(keyEvent)   ← SAME PATH as hardware buttons
      → No step match? → synthesize KeyboardEvent
        → screensaver.consumeKey(kbEvent)
        → pageManager.dispatchEvent(kbEvent)    ← pages consume via keyboard() virtual
```

KeyboardManager **feeds into the existing page dispatch stack**. This is correct because the keyboard emulates hardware buttons (Q-I/A-K = step buttons) and provides page-specific shortcuts. Pages consume events through their `keyboard()` override, falling through to `TopPage::keyboard()` for globals.

## What This Means for the Design

### 1. KeyboardManager Should NOT Own Sub-Controllers

ControllerManager has a `Container<>` and polymorphic `Controller*` because Launchpad has 5 hardware variants with different LED protocols. The keyboard has no such diversity — all USB keyboards speak the same HID protocol, already handled by `UsbH`.

**No `KeyboardDevice` hierarchy needed.** No `Container<>`. No polymorphism.

### 2. KeyboardManager Does NOT Need Model/Engine Access

ControllerManager takes `Model &` because it directly edits sequences, play state, and track selection. KeyboardManager is a pure translator — it converts HID bytes into events and lets the page stack handle the rest.

**Constructor should be `KeyboardManager(Engine &, MessageManager &)`** — Engine for handler registration and `isSuspended()`, MessageManager for HID connect/disconnect messages. No Model reference.

### 3. The `process()` Signature is Unique

ControllerManager's `update()` is self-contained — it only calls `_controller->update()` which draws LEDs.

KeyboardManager's `process()` needs to:
- Mutate `KeyState` (for step-key synthesis — `pageKeyState[keyCode] = isDown`)
- Use `KeyPressEventTracker` (for multi-press detection)
- Call `Screensaver::consumeKey()` (for wake-on-keypress)
- Call `PageManager::dispatchEvent()` (for page-level consumption)

This means `process()` must take these by reference. **This is correct and different from ControllerManager.** ControllerManager doesn't need any of these because it bypasses the page stack.

### 4. No `connect()`/`disconnect()` Lifecycle

ControllerManager creates/destroys a controller object on USB MIDI device plug/unplug. KeyboardManager doesn't need this — the USB keyboard is always "ready to accept events" once the HID driver reports a connection. The ring buffer is always allocated (56 bytes, negligible).

**No dynamic allocation. No `Container<>`. Static lifetime only.**

### 5. Handler Registration is KeyboardManager's `init()` Job

In `Ui::init()` currently:
```cpp
// These 3 lambdas should move to KeyboardManager::init():
_engine.setKeyboardReceiveHandler(...)
_engine.setHidConnectHandler(...)
_engine.setHidDisconnectHandler(...)
```

ControllerManager doesn't register Engine handlers — it receives MIDI through a different path (`Ui::handleMidi()` → `ControllerManager::recvMidi()`). The keyboard follows the callback pattern because HID events come from `Engine::receiveKeyboard()` which polls `UsbH::recvKey()`.

## Should KeyboardManager Follow ControllerManager More Closely?

**No.** The two managers solve fundamentally different problems:

| | ControllerManager | KeyboardManager |
|---|---|---|
| Analogy | A remote control with its own display | A translator for an existing input device |
| Concern | Bidirectional I/O, hardware variants, visual feedback | Unidirectional translation, event synthesis |
| Integration | Parallel to page stack (independent UI) | Embedded in page stack (augments existing UI) |
| Complexity driver | Grid drawing, navigation, multi-mode UI | Keycode tables, step mapping, modifier logic |

The only shared pattern is:
1. Owned by `Ui`
2. Takes `Engine &` for handler registration
3. Called from `Ui::update()` each frame

Everything else diverges because the input channels are architecturally different.
