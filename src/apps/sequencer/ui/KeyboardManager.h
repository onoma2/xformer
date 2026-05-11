#pragma once

#include <cstdint>
#include <functional>

#include "core/utils/RingBuffer.h"

#include "engine/Engine.h"

class KeyboardManager {
public:
    using ProcessHandler = std::function<void(uint8_t keycode, uint8_t modifiers, uint8_t pressed)>;

    static char hidKeycodeToAscii(uint8_t keycode, uint8_t modifiers);
    static int hidKeycodeToStep(uint8_t keycode);

    KeyboardManager();

    void init(Engine &engine);
    void process();
    void setProcessHandler(ProcessHandler handler) { _processHandler = handler; }
    void enqueue(uint8_t keycode, uint8_t modifiers, uint8_t pressed);

private:
    struct ReceiveKeyboardEvent {
        uint8_t keycode;
        uint8_t modifiers;
        uint8_t pressed;
    };
    RingBuffer<ReceiveKeyboardEvent, 16> _receiveKeyboardEvents;
    ProcessHandler _processHandler;
};
