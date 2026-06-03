#pragma once

#include "core/midi/MidiMessage.h"
#include "UsbMidi.h"

#include <cstdint>

// Stub USB HID driver for simulator
struct HidKeyEvent {
    uint8_t device_id;
    uint8_t modifiers;
    uint8_t keycode;
    uint8_t pressed;
};

enum HID_TYPE {
    HID_TYPE_NONE = 0,
    HID_TYPE_OTHER = 1,
    HID_TYPE_MOUSE = 2,
    HID_TYPE_KEYBOARD = 3,
};

class UsbH {
public:
    UsbH(UsbMidi &) {}   // sim stub: param kept for API parity with the hardware driver
    UsbH() {}

    void init() {}
    void process() {}
    void powerOn() {}
    void powerOff() {}
    bool powerFault() const { return false; }

    bool recvKey(HidKeyEvent *) { return false; }

    using HidConnectCallback = void(*)(uint8_t device_id, HID_TYPE type, void *context);
    using HidDisconnectCallback = void(*)(uint8_t device_id, void *context);
    using DebugMessageCallback = void(*)(const char *msg, void *context);

    void setHidCallbacks(HidConnectCallback, HidDisconnectCallback, void *) {}
    void setDebugMessageCallback(DebugMessageCallback, void *) {}
};
