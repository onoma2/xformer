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

int KeyboardManager::hidKeycodeToStep(uint8_t keycode) {
    static constexpr uint8_t stepKeycodes[] = {
        0x14, 0x1A, 0x08, 0x15, 0x17, 0x1C, 0x18, 0x0C,
        0x04, 0x16, 0x07, 0x09, 0x0A, 0x0B, 0x0D, 0x0E,
    };
    for (int i = 0; i < 16; ++i) {
        if (keycode == stepKeycodes[i]) {
            return i;
        }
    }
    return -1;
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
            int step = -1;
            if (mapStepKeys) {
                step = (event.modifiers & 0x55) ? -1 : hidKeycodeToStep(event.keycode);
            }
            if (step >= 0) {
                bool isDown = event.pressed != 0;
                int keyCode = MatrixMap::fromStep(step);
                pageKeyState[keyCode] = isDown;
                globalKeyState[keyCode] = isDown;
                Key key(keyCode, globalKeyState);

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
