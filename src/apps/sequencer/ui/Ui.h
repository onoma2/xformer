#pragma once

#include "Config.h"
#include "MessageManager.h"
#include "Page.h"
#include "PageManager.h"
#include "Key.h"
#include "KeyPressEventTracker.h"
#include "Leds.h"
#include "ControllerManager.h"

#include "pages/Pages.h"

#include "drivers/ButtonLedMatrix.h"
#include "drivers/Encoder.h"
#include "drivers/Lcd.h"

#include "core/gfx/FrameBuffer.h"
#include "core/gfx/Canvas.h"
#include "core/utils/RingBuffer.h"
#include "core/midi/MidiMessage.h"

#include "engine/Engine.h"

#include "model/Model.h"
#include "Screensaver.h"

class Key;

class Ui {
public:
    Ui(Model &model, Engine &engine, Lcd &lcd, ButtonLedMatrix &blm, Encoder &encoder, Settings &settings);

    void init();
    void update();

    void showAssert(const char *filename, int line, const char *msg);

    MessageManager &messageManager() { return _messageManager; }

    void enqueueKeyboardEvent(uint8_t keycode, uint8_t modifiers);

private:
    void handleKeys();
    void handleEncoder();
    void handleMidi();
    void handleKeyboard();

    Model &_model;
    Engine &_engine;

    Lcd &_lcd;
    ButtonLedMatrix &_blm;
    Encoder &_encoder;

    struct ReceiveMidiEvent {
        MidiPort port;
        uint8_t cable;
        MidiMessage message;
    };
    RingBuffer<ReceiveMidiEvent, 16> _receiveMidiEvents;

    struct ReceiveKeyboardEvent {
        uint8_t keycode;
        uint8_t modifiers;
    };
    RingBuffer<ReceiveKeyboardEvent, 16> _receiveKeyboardEvents;

    uint8_t _frameBufferData[CONFIG_LCD_WIDTH * CONFIG_LCD_HEIGHT];
    FrameBuffer8bit _frameBuffer;
    Canvas _canvas;
    uint32_t _lastFrameBufferUpdateTicks;

    KeyState _pageKeyState;
    KeyState _globalKeyState;
    KeyPressEventTracker _keyPressEventTracker;
    Leds _leds;

    MessageManager _messageManager;

    PageManager _pageManager;
    PageContext _pageContext;
    Pages _pages;

    ControllerManager _controllerManager;
    uint32_t _lastControllerUpdateTicks;

    Screensaver _screensaver;
};
