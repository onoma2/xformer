# Spec: keyboard-injection entry point for the Xformer VCV plugin (teletype text input)

Status: proposed — firmware change to be implemented + tested **here**, in performer-phazer.
Created: 2026-06-14
Consumer: the Xformer VCV Rack plugin (Onomatopoeia/Xformer module), which runs this
firmware headless via `sim::Simulator`.

## Goal

Let the VCV plugin feed computer keystrokes into the firmware's existing keyboard
pipeline so the user can **type teletype scripts / use text-input pages** in Rack.
This needs **one small, additive firmware entry point** — and explicitly must NOT
touch the brittle `UsbH` driver or change any existing keyboard behaviour.

## How keyboard input flows today

1. `KeyboardManager::init()` registers a handler on the engine
   (`ui/KeyboardManager.cpp:58`):
   ```cpp
   _engine->setKeyboardReceiveHandler([this](uint8_t keycode, uint8_t modifiers, uint8_t pressed) {
       enqueue(keycode, modifiers, pressed);
   });
   ```
2. `Engine::receiveKeyboard()` (polled each frame) pulls HID events from the USB host
   and invokes that handler:
   ```cpp
   while (_usbH.recvKey(&keyEvent))
       _keyboardReceiveHandler(keyEvent.keycode, keyEvent.modifiers, keyEvent.pressed);
   ```
3. `KeyboardManager::enqueue` writes to a ring buffer; `KeyboardManager::process()`
   drains it each UI frame and dispatches `KeyboardEvent`s to the teletype/text pages
   (`teletypeScriptView`, `teletypePatternView`, `textInput`). On those pages the UI
   already stops mapping keys to steps (`Ui.cpp` `mapStepKeys`).

So the firmware already has a **registered, clean keyboard-receive handler that lands
on teletype**. The only reason nothing arrives in the plugin is that the sim's
`UsbH::recvKey()` is a stub returning `false`.

## Proposed change (the only firmware edit)

Add a public method on `Engine` that invokes the **already-registered** handler
directly — mirroring `receiveKeyboard()` but without the USB poll:

`engine/Engine.h` (public section, next to `receiveKeyboard()`):
```cpp
// Inject a HID keyboard event from outside the USB host (used by the VCV plugin,
// which has no UsbH). Routes through the same handler a real USB key would.
void sendKeyboardKey(uint8_t keycode, uint8_t modifiers, uint8_t pressed);
```

`engine/Engine.cpp`:
```cpp
void Engine::sendKeyboardKey(uint8_t keycode, uint8_t modifiers, uint8_t pressed) {
    if (_keyboardReceiveHandler) {
        _keyboardReceiveHandler(keycode, modifiers, pressed);
    }
}
```

That's it. No `UsbH` change, no `Ui` change, no `KeyboardManager` change.

### Why this is safe
- **Additive.** Existing code paths are untouched; `receiveKeyboard()` and the real
  USB path on hardware keep working exactly as before.
- **Reuses the registered handler** (`_keyboardReceiveHandler`), so the downstream
  behaviour is identical to a real USB keystroke — including the teletype/text routing
  and the `mapStepKeys` gating.
- **No-op when unused** (null handler guard), so it can't break the STM32 build or a
  sim run that never calls it.
- Keycodes/modifiers are USB HID usage codes — the same values `recvKey` delivers and
  that `KeyboardManager::hidKeycodeToAscii` / `hidKeycodeToStep` already decode.

### Open item to confirm during testing
Some text/teletype pages may require a keyboard to be "connected" before they accept
input. If so, the consumer must also call the existing public
`Engine::hidConnect(0, HID_TYPE_KEYBOARD)` once at startup — that's already public, no
firmware change. Verify whether teletype text input works without it; if not, document
that the consumer must send the connect event.

## Testing (here, before it ships)
1. **Functional (sim):** on a teletype script page, call
   `engine.sendKeyboardKey(<HID code>, <mods>, 1)` then `…, 0` for a few characters
   (e.g. `TR 1 1`) and confirm the line edits / the characters appear.
2. **Regression:** confirm the panel button matrix keyboard and the real USB-HID path
   (`receiveKeyboard`) are unaffected — `sendKeyboardKey` is purely additive.
3. **Edge:** rapid bursts — `enqueue` already drops the oldest event when the ring
   buffer is full, so no overflow handling is needed in the new method.

## Downstream (VCV plugin — context only, not firmware work)
Once `sendKeyboardKey` exists and the libs are rebuilt
(`build_xformer_libs.sh`), the plugin will:
- `FirmwareHost::sendHidKey(keycode, mod, pressed)` → `app->engine.sendKeyboardKey(...)`.
- A **focused** key-capture widget over the OLED (monome-rack style:
  `APP->event->setSelectedWidget(screen)` on click, `onSelectKey`/`onSelectText`),
  so typing is gated by focus and does NOT clash with the panel's z/a/q shortcuts.
- A **GLFW key → USB HID keycode** table (portable nearly verbatim from
  `monome-rack/src/teletype/TeletypeKeyboard.cpp`).
- Optionally `app->engine.hidConnect(0, HID_TYPE_KEYBOARD)` at boot per the open item.

This file documents the firmware contract; the plugin wiring lives in the VCVRack repo.
