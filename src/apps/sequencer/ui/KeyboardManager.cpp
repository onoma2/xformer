#include "KeyboardManager.h"

char KeyboardManager::hidKeycodeToAscii(uint8_t keycode, uint8_t modifiers) {
    bool shifted = modifiers & 0x22;

    if (keycode >= 0x04 && keycode <= 0x1D) {
        return shifted ? keycode - 0x04 + 'A' : keycode - 0x04 + 'a';
    }
    if (keycode >= 0x1E && keycode <= 0x26) {
        if (shifted) {
            static const char shiftedNums[] = "!@#$%^&*(";
            return shiftedNums[keycode - 0x1E];
        }
        return keycode - 0x1E + '1';
    }
    if (keycode == 0x27) return shifted ? ')' : '0';
    if (keycode == 0x2C) return ' ';

    if (keycode == 0x2D) return shifted ? '_' : '-';
    if (keycode == 0x2E) return shifted ? '+' : '=';
    if (keycode == 0x2F) return shifted ? '{' : '[';
    if (keycode == 0x30) return shifted ? '}' : ']';
    if (keycode == 0x31) return shifted ? '|' : '\\';
    if (keycode == 0x33) return shifted ? ':' : ';';
    if (keycode == 0x34) return shifted ? '"' : '\'';
    if (keycode == 0x35) return shifted ? '~' : '`';
    if (keycode == 0x36) return shifted ? '<' : ',';
    if (keycode == 0x37) return shifted ? '>' : '.';
    if (keycode == 0x38) return shifted ? '?' : '/';

    return 0;
}

int KeyboardManager::hidKeycodeToButton(uint8_t keycode) {
    // Panel-faithful USB-keyboard layout, mirroring the sim's SDL keymap.
    static constexpr uint8_t trackKeycodes[] = { 0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C };
    static constexpr uint8_t stepLowKeycodes[] = { 0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E };
    static constexpr uint8_t stepHighKeycodes[] = { 0x1D, 0x1B, 0x06, 0x19, 0x05, 0x11, 0x10, 0x36 };

    for (int i = 0; i < 8; ++i) {
        if (keycode == trackKeycodes[i]) return MatrixMap::fromTrack(i);
        if (keycode == stepLowKeycodes[i]) return MatrixMap::fromStep(i);
        if (keycode == stepHighKeycodes[i]) return MatrixMap::fromStep(8 + i);
    }
    switch (keycode) {
    case 0x1E: return Key::Play;
    case 0x1F: return Key::Tempo;
    case 0x20: return Key::Pattern;
    case 0x21: return Key::Performer;
    case 0x2C: return Key::Encoder;
    default: return -1;
    }
}

KeyboardManager::KeyboardManager() :
    _engine(nullptr),
    _messageManager(nullptr)
{
}

void KeyboardManager::init(Engine &engine, MessageManager &messageManager)
{
    _engine = &engine;
    _messageManager = &messageManager;

    _engine->setKeyboardReceiveHandler([this] (uint8_t keycode, uint8_t modifiers, uint8_t pressed) {
        enqueue(keycode, modifiers, pressed);
    });

    _engine->setHidConnectHandler([this] (uint8_t device_id, int type) {
        (void)device_id;
        switch (type) {
        case 3: _messageManager->showMessage("KEYBOARD CONNECTED", 3000); break;
        case 2: _messageManager->showMessage("MOUSE CONNECTED", 3000); break;
        default: _messageManager->showMessage("HID CONNECTED", 3000); break;
        }
    });

    _engine->setHidDisconnectHandler([this] (uint8_t device_id) {
        (void)device_id;
        _messageManager->showMessage("DEVICE REMOVED", 3000);
    });
}

void KeyboardManager::process(KeyState &pageKeyState, KeyState &globalKeyState,
                               KeyPressEventTracker &tracker,
                               Screensaver &screensaver,
                               PageManager &pageManager,
                               bool mapStepKeys)
{
    while (_receiveKeyboardEvents.readable()) {
        auto event = _receiveKeyboardEvents.read();
        char ch = hidKeycodeToAscii(event.keycode, event.modifiers);

        if (!_engine->isSuspended()) {
            int keyCode = -1;
            if (mapStepKeys) {
                keyCode = (event.modifiers & 0x44) ? -1 : hidKeycodeToButton(event.keycode);
            }
            if (keyCode >= 0) {
                bool isDown = event.pressed != 0;
                pageKeyState[keyCode] = isDown;
                globalKeyState[keyCode] = isDown;

                // Build a local key state that includes USB modifier bits.
                // USB HID reports modifiers as a bitmask (byte 0) with no
                // separate key events for Shift/Ctrl. We OR the USB modifier
                // state into a copy of globalKeyState so that
                // Key::shiftModifier() and Key::pageModifier() see the USB
                // keyboard's Shift/Ctrl state. This copy is ephemeral — we
                // never write USB modifier bits into globalKeyState itself,
                // so they can't persist after the USB keyboard is disconnected
                // or after modifiers are released without a new key event.
                KeyState localKeyState = globalKeyState;
                if (event.modifiers & 0x22) localKeyState[Key::Shift] = true;
                if (event.modifiers & 0x11) localKeyState[Key::Page] = true;
                Key key(keyCode, localKeyState);

                KeyEvent keyEvent(isDown ? Event::KeyDown : Event::KeyUp, key);
                screensaver.consumeKey(keyEvent);
                pageManager.dispatchEvent(keyEvent);
                if (isDown) {
                    KeyPressEvent keyPressEvent = tracker.process(key);
                    screensaver.consumeKey(keyPressEvent);
                    pageManager.dispatchEvent(keyPressEvent);
                }
                continue;
            }

            if (event.pressed) {
                KeyboardEvent kbEvent(event.keycode, event.modifiers, ch, event.pressed);
                screensaver.consumeKey(kbEvent);
                pageManager.dispatchEvent(kbEvent);
            }
        }
    }
}

void KeyboardManager::enqueue(uint8_t keycode, uint8_t modifiers, uint8_t pressed)
{
    if (!_receiveKeyboardEvents.writable()) {
        _receiveKeyboardEvents.read();
    }
    _receiveKeyboardEvents.write({ keycode, modifiers, pressed });
}