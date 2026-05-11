#include "KeyboardManager.h"

KeyboardManager::KeyboardManager()
{
}

void KeyboardManager::init(Engine &engine)
{
    (void)engine;
}

void KeyboardManager::process()
{
    while (_receiveKeyboardEvents.readable()) {
        auto event = _receiveKeyboardEvents.read();
        _processHandler(event.keycode, event.modifiers, event.pressed);
    }
}

void KeyboardManager::enqueue(uint8_t keycode, uint8_t modifiers, uint8_t pressed)
{
    if (!_receiveKeyboardEvents.writable()) {
        _receiveKeyboardEvents.read();
    }
    _receiveKeyboardEvents.write({ keycode, modifiers, pressed });
}
