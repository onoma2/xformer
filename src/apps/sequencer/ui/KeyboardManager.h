#pragma once

#include <cstdint>

#include "SystemConfig.h"
#include "Key.h"
#include "Event.h"
#include "KeyPressEventTracker.h"
#include "Screensaver.h"
#include "PageManager.h"
#include "MatrixMap.h"

#include "core/utils/RingBuffer.h"

#include "engine/Engine.h"

class KeyboardManager {
public:
    static char hidKeycodeToAscii(uint8_t keycode, uint8_t modifiers);
    static int hidKeycodeToStep(uint8_t keycode);

    KeyboardManager();

    void init(Engine &engine);
    void process(KeyState &pageKeyState, KeyState &globalKeyState,
                 KeyPressEventTracker &tracker,
                 Screensaver &screensaver,
                 PageManager &pageManager);
    void enqueue(uint8_t keycode, uint8_t modifiers, uint8_t pressed);

private:
    struct ReceiveKeyboardEvent {
        uint8_t keycode;
        uint8_t modifiers;
        uint8_t pressed;
    };
    RingBuffer<ReceiveKeyboardEvent, 16> _receiveKeyboardEvents;
    Engine *_engine;
};
