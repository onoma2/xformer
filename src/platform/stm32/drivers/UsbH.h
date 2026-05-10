#pragma once

#include "UsbMidi.h"
#include "usbh_driver_hid.h"

#include <cstdint>

class UsbH {
public:
    UsbH(UsbMidi &usbMidi);

    void init();

    void process();

    void powerOn();
    void powerOff();
    bool powerFault();

    using HidConnectCallback = void(*)(uint8_t device_id, HID_TYPE type, void *context);
    using HidDisconnectCallback = void(*)(uint8_t device_id, void *context);
    using HidKeyCallback = void(*)(uint8_t modifiers, uint8_t keycode, void *context);
    using DebugMessageCallback = void(*)(const char *msg, void *context);

    void setHidCallbacks(HidConnectCallback onConnect, HidDisconnectCallback onDisconnect, HidKeyCallback onKey, void *context) {
        _hidConnectCallback = onConnect;
        _hidDisconnectCallback = onDisconnect;
        _hidKeyCallback = onKey;
        _hidCallbackContext = context;
    }

    void setDebugMessageCallback(DebugMessageCallback cb, void *context) {
        _debugMsgCallback = cb;
        _debugMsgContext = context;
    }

private:
    void midiConnectDevice(uint8_t device, uint16_t vendorId, uint16_t productId) {
        _midiDevices |= (1 << device);
        _usbMidi.connect(vendorId, productId);
    }

    void midiDisconnectDevice(uint8_t device) {
        _midiDevices &= ~(1 << device);
        _usbMidi.disconnect();
    }

    bool midiDeviceConnected(uint8_t device) {
        return _midiDevices & (1 << device);
    }

    void midiEnqueueMessage(uint8_t device, uint8_t cable, const MidiMessage &message) {
        _usbMidi.enqueueMessage(cable, message);
    }

    void midiEnqueueData(uint8_t device, uint8_t cable, uint8_t data) {
        _usbMidi.enqueueData(cable, data);
    }

    bool midiDequeueMessage(uint8_t *device, uint8_t *cable, MidiMessage *message) {
        *device = 0;
        return _usbMidi.dequeueMessage(cable, message);
    }

    void hidConnectDevice(uint8_t device, HID_TYPE type) {
        _hidDevices |= (1 << device);
        if (_hidConnectCallback) {
            _hidConnectCallback(device, type, _hidCallbackContext);
        }
    }

    void hidDisconnectDevice(uint8_t device) {
        _hidDevices &= ~(1 << device);
        if (_hidDisconnectCallback) {
            _hidDisconnectCallback(device, _hidCallbackContext);
        }
    }

    UsbMidi &_usbMidi;

    uint8_t _midiDevices = 0;
    uint8_t _hidDevices = 0;

    HidConnectCallback _hidConnectCallback = nullptr;
    HidDisconnectCallback _hidDisconnectCallback = nullptr;
    HidKeyCallback _hidKeyCallback = nullptr;
    void *_hidCallbackContext = nullptr;

    DebugMessageCallback _debugMsgCallback = nullptr;
    void *_debugMsgContext = nullptr;

    friend struct MidiDriverHandler;
    friend struct HidDriverHandler;
    friend struct HidDebugHandler;
};
