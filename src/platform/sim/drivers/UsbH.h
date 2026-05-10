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
    UsbH(UsbMidi &usbMidi) : _usbMidi(usbMidi) {}
    UsbH() : _usbMidi(_defaultUsbMidi) {}

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

private:
    UsbMidi _defaultUsbMidi;
    UsbMidi &_usbMidi;
};
