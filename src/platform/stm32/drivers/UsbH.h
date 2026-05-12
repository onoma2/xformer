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

    bool recvKey(HidKeyEvent *event);

    using HidConnectCallback = void(*)(uint8_t device_id, HID_TYPE type, void *context);
    using HidDisconnectCallback = void(*)(uint8_t device_id, void *context);
    using DebugMessageCallback = void(*)(const char *msg, void *context);

    void setHidCallbacks(HidConnectCallback onConnect, HidDisconnectCallback onDisconnect, void *context) {
        _hidConnectCallback = onConnect;
        _hidDisconnectCallback = onDisconnect;
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
        clearHidState(device);
        if (_hidDisconnectCallback) {
            _hidDisconnectCallback(device, _hidCallbackContext);
        }
    }

    void enqueueHidReport(uint8_t device, const uint8_t *data, uint32_t length);
    void processHidReports();
    void clearHidState(uint8_t device);
    void enqueueHidKey(uint8_t device, uint8_t modifiers, uint8_t keycode, uint8_t pressed);

    struct HidReport {
        uint8_t device;
        uint8_t length;
        uint8_t data[8];
    };

    static constexpr uint8_t HidReportBufferSize = 8;
    static constexpr uint8_t HidKeyEventBufferSize = 16;

    UsbMidi &_usbMidi;

    uint8_t _midiDevices = 0;
    uint8_t _hidDevices = 0;

    HidReport _hidReports[HidReportBufferSize];
    volatile uint8_t _hidReportHead = 0;
    volatile uint8_t _hidReportTail = 0;

    HidKeyEvent _hidKeyEvents[HidKeyEventBufferSize];
    volatile uint8_t _hidKeyHead = 0;
    volatile uint8_t _hidKeyTail = 0;

    uint8_t _hidPrevKeys[USBH_HID_MAX_DEVICES][6] = {};
    uint8_t _hidPrevKeyCount[USBH_HID_MAX_DEVICES] = {};

    HidConnectCallback _hidConnectCallback = nullptr;
    HidDisconnectCallback _hidDisconnectCallback = nullptr;
    void *_hidCallbackContext = nullptr;

    DebugMessageCallback _debugMsgCallback = nullptr;
    void *_debugMsgContext = nullptr;

    friend struct MidiDriverHandler;
    friend struct HidDriverHandler;
};
