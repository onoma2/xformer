#include "UsbH.h"
#include "HighResolutionTimer.h"

#include "SystemConfig.h"

#include "core/Debug.h"
#include "core/midi/MidiMessage.h"

#include "usbh_core.h"
#include "usbh_lld_stm32f4.h"
#include "usbh_driver_hub.h"
#include "usbh_driver_ac_midi.h"
#include "usbh_driver_hid.h"

#include <cstdio>

#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/usart.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/usb/dwc/otg_hs.h>
#include <libopencm3/usb/dwc/otg_fs.h>

#define USB_PWR_EN_PORT GPIOC
#define USB_PWR_EN_PIN GPIO9
#define USB_PWR_FAULT_PORT GPIOA
#define USB_PWR_FAULT_PIN GPIO8

static UsbH *g_usbh;

static const usbh_dev_driver_t *device_drivers[] = {
	&usbh_hub_driver,
	&usbh_midi_driver,
	&usbh_hid_driver,
	nullptr
};

static const usbh_low_level_driver_t * const lld_drivers[] = {
#ifdef USE_STM32F4_USBH_DRIVER_FS
	&usbh_lld_stm32f4_driver_fs,
#endif
#ifdef USE_STM32F4_USBH_DRIVER_HS
	&usbh_lld_stm32f4_driver_hs,
#endif
	nullptr
};

static constexpr size_t WriteBufferSize = 64;
static uint8_t writeBuffer[2][WriteBufferSize];
static size_t writeBufferSize;
static size_t writeBufferIndex;
static size_t writeBufferPos;

struct MidiDriverHandler {

    static void connectHandler(int device, uint16_t vendorId, uint16_t productId, uint32_t maxPacketSize) {
        DBG("MIDI device connected (id=%d, vendorId=%04x, productId=%04x, maxPacketSize=%ld)", device, vendorId, productId, maxPacketSize);
        if (g_usbh) {
            g_usbh->midiConnectDevice(device, vendorId, productId);
        }
        writeBufferSize = std::min(WriteBufferSize, size_t(maxPacketSize));
    }

    static void disconnectHandler(int device) {
        DBG("MIDI device disconnected (id=%d)", device);
        if (g_usbh) {
            g_usbh->midiDisconnectDevice(device);
        }
    }

    static void recvHandler(int device, uint8_t *data) {
        if (!g_usbh) return;
        uint8_t cable = data[0] >> 4;
        uint8_t code = data[0] & 0xf;
        MidiMessage message;

        switch (code) {
        case 0x0:
        case 0x1:
        case 0x4:
        case 0x6:
        case 0x7:
            return;
        case 0x5:
            message = MidiMessage(data[1]);
            g_usbh->midiEnqueueMessage(device, cable, message);
            break;
        case 0x2:
        case 0xC:
        case 0xD:
            message = MidiMessage(data[1], data[2]);
            g_usbh->midiEnqueueMessage(device, cable, message);
            break;
        case 0x3:
        case 0x8:
        case 0x9:
        case 0xA:
        case 0xB:
        case 0xE:
            message = MidiMessage(data[1], data[2], data[3]);
            g_usbh->midiEnqueueMessage(device, cable, message);
            break;
        case 0xF:
            g_usbh->midiEnqueueData(device, cable, data[1]);
            return;
        }
    }

    static bool write(uint8_t device, uint8_t cable, const MidiMessage &message) {
        bool flushed = true;

        if (message.isSystemExclusive()) {
            const uint8_t *payloadData = message.payloadData();
            size_t payloadLength = message.payloadLength();
            if (payloadData && payloadLength > 0) {
                size_t messageLength = payloadLength + 2;
                size_t writeSize = ((messageLength + 3) / 4) * 4;
                if (writeBufferPos + writeSize >= writeBufferSize) {
                    flush(device);
                    flushed = true;
                }

                size_t messageIndex = 0;
                while (messageIndex < messageLength) {
                    uint8_t *p = &writeBuffer[writeBufferIndex][writeBufferPos];
                    size_t chunkSize;
                    switch (messageLength - messageIndex) {
                    case 1:
                        p[0] = 0x5;
                        chunkSize = 1;
                        break;
                    case 2:
                        p[0] = 0x6;
                        chunkSize = 2;
                        break;
                    case 3:
                        p[0] = 0x7;
                        chunkSize = 3;
                        break;
                    default:
                        p[0] = 0x4;
                        chunkSize = 3;
                        break;
                    }
                    p[0] |= cable << 4;
                    for (size_t i = 0; i < chunkSize; ++i) {
                        uint8_t byte;
                        if (messageIndex == 0) {
                            byte = 0xf0;
                        } else if (messageIndex == messageLength - 1) {
                            byte = 0xf7;
                        } else {
                            byte = payloadData[messageIndex - 1];
                        }
                        p[1 + i] = byte;
                        ++messageIndex;
                    }
                    for (size_t i = chunkSize; i < 3; ++i) {
                        p[1 + i] = 0;
                    }
                    writeBufferPos += 4;
                }
            }
        } else {
            if (writeBufferPos + 4 >= writeBufferSize) {
                flush(device);
                flushed = true;
            }
            uint8_t *p = &writeBuffer[writeBufferIndex][writeBufferPos];
            p[0] = message.status() >> 4 | (cable << 4);
            p[1] = message.status();
            p[2] = message.data0();
            p[3] = message.data1();
            writeBufferPos += 4;
        }

        return flushed;
    }

    static void flush(uint8_t device) {
        if (writeBufferPos > 0) {
            usbh_midi_write(device, writeBuffer[writeBufferIndex], writeBufferPos, &writeCallback);
            writeBufferIndex = (writeBufferIndex + 1) % 2;
            writeBufferPos = 0;
        }
    }

    static void writeCallback(uint8_t bytes_written) {
    }
};

static const midi_config_t midi_config = {
    .read_callback = &MidiDriverHandler::recvHandler,
    .notify_connected = &MidiDriverHandler::connectHandler,
    .notify_disconnected = &MidiDriverHandler::disconnectHandler,
};

static hid_config_t hid_config = {};

struct HidDriverHandler {
    static void connectHandler(uint8_t device_id, HID_TYPE type) {
        if (g_usbh) {
            g_usbh->hidConnectDevice(device_id, type);
        }
    }

    static void disconnectHandler(uint8_t device_id) {
        if (g_usbh) {
            g_usbh->hidDisconnectDevice(device_id);
        }
    }

    static void messageHandler(uint8_t device_id, const uint8_t *data, uint32_t length) {
        if (g_usbh) {
            // Keep the C HID driver close to the hardware-tested path; parse reports after usbh_poll().
            g_usbh->enqueueHidReport(device_id, data, length);
        }
    }
};



UsbH::UsbH(UsbMidi &usbMidi) :
    _usbMidi(usbMidi)
{}

void UsbH::init() {
    g_usbh = this;

	rcc_periph_clock_enable(RCC_GPIOA);
	rcc_periph_clock_enable(RCC_GPIOC);

	gpio_mode_setup(USB_PWR_EN_PORT, GPIO_MODE_OUTPUT, GPIO_PUPD_NONE, USB_PWR_EN_PIN);
    gpio_clear(USB_PWR_EN_PORT, USB_PWR_EN_PIN);

	gpio_mode_setup(USB_PWR_FAULT_PORT, GPIO_MODE_INPUT, GPIO_PUPD_NONE, USB_PWR_FAULT_PIN);

	rcc_periph_clock_enable(RCC_OTGFS);

	gpio_mode_setup(GPIOA, GPIO_MODE_AF, GPIO_PUPD_NONE, GPIO11 | GPIO12);
	gpio_set_af(GPIOA, GPIO_AF10, GPIO11 | GPIO12);

	hub_driver_init();
	midi_driver_init(&midi_config);
	hid_config.hid_in_message_handler = &HidDriverHandler::messageHandler;
	hid_driver_init(&hid_config);
	usbh_init(lld_drivers, device_drivers);
}

void UsbH::process() {
    uint32_t time_us = HighResolutionTimer::us();

    usbh_poll(time_us);
    processHidReports();

    for (int i = 0; i < USBH_HID_MAX_DEVICES; ++i) {
        bool connected = hid_is_connected(i);
        bool wasConnected = (_hidDevices & (1 << i));

        if (connected && !wasConnected) {
            HidDriverHandler::connectHandler(i, hid_get_type(i));
        } else if (!connected && wasConnected) {
            HidDriverHandler::disconnectHandler(i);
        }
    }

    uint8_t device = 0;
    uint8_t cable = 0;
    MidiMessage message;
    bool flushed = false;
    while (midiDequeueMessage(&device, &cable, &message)) {
        if (midiDeviceConnected(device)) {
            flushed = MidiDriverHandler::write(device, cable, message);
            if (flushed) {
                break;
            }
        }
    }
    if (!flushed) {
        MidiDriverHandler::flush(device);
    }

}

bool UsbH::recvKey(HidKeyEvent *event) {
    if (_hidKeyTail == _hidKeyHead) {
        return false;
    }
    *event = _hidKeyEvents[_hidKeyTail];
    _hidKeyTail = (_hidKeyTail + 1) % HidKeyEventBufferSize;
    return true;
}

void UsbH::enqueueHidReport(uint8_t device, const uint8_t *data, uint32_t length) {
    if (device >= USBH_HID_MAX_DEVICES || !data || length < 8) {
        return;
    }

    uint8_t nextHead = (_hidReportHead + 1) % HidReportBufferSize;
    if (nextHead == _hidReportTail) {
        _hidReportTail = (_hidReportTail + 1) % HidReportBufferSize;
    }

    HidReport &report = _hidReports[_hidReportHead];
    report.device = device;
    report.length = 8;
    for (uint8_t i = 0; i < report.length; ++i) {
        report.data[i] = data[i];
    }
    _hidReportHead = nextHead;
}

void UsbH::processHidReports() {
    while (_hidReportTail != _hidReportHead) {
        HidReport report = _hidReports[_hidReportTail];
        _hidReportTail = (_hidReportTail + 1) % HidReportBufferSize;

        uint8_t currentKeys[6] = {};
        uint8_t currentCount = 0;
        for (uint8_t i = 2; i < report.length && currentCount < 6; ++i) {
            if (report.data[i] != 0) {
                currentKeys[currentCount++] = report.data[i];
            }
        }

        uint8_t *previousKeys = _hidPrevKeys[report.device];
        uint8_t previousCount = _hidPrevKeyCount[report.device];
        uint8_t modifiers = report.data[0];

        for (uint8_t ci = 0; ci < currentCount; ++ci) {
            bool found = false;
            for (uint8_t pi = 0; pi < previousCount; ++pi) {
                if (currentKeys[ci] == previousKeys[pi]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                enqueueHidKey(report.device, modifiers, currentKeys[ci], 1);
            }
        }

        for (uint8_t pi = 0; pi < previousCount; ++pi) {
            bool found = false;
            for (uint8_t ci = 0; ci < currentCount; ++ci) {
                if (previousKeys[pi] == currentKeys[ci]) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                enqueueHidKey(report.device, modifiers, previousKeys[pi], 0);
            }
        }

        for (uint8_t i = 0; i < 6; ++i) {
            previousKeys[i] = currentKeys[i];
        }
        _hidPrevKeyCount[report.device] = currentCount;
    }
}

void UsbH::clearHidState(uint8_t device) {
    if (device >= USBH_HID_MAX_DEVICES) {
        return;
    }
    _hidPrevKeyCount[device] = 0;
    for (uint8_t i = 0; i < 6; ++i) {
        _hidPrevKeys[device][i] = 0;
    }
}

void UsbH::enqueueHidKey(uint8_t device, uint8_t modifiers, uint8_t keycode, uint8_t pressed) {
    uint8_t nextHead = (_hidKeyHead + 1) % HidKeyEventBufferSize;
    if (nextHead == _hidKeyTail) {
        _hidKeyTail = (_hidKeyTail + 1) % HidKeyEventBufferSize;
    }

    HidKeyEvent &event = _hidKeyEvents[_hidKeyHead];
    event.device_id = device;
    event.modifiers = modifiers;
    event.keycode = keycode;
    event.pressed = pressed;
    _hidKeyHead = nextHead;
}

void UsbH::powerOn() {
    gpio_set(USB_PWR_EN_PORT, USB_PWR_EN_PIN);
}

void UsbH::powerOff() {
    gpio_clear(USB_PWR_EN_PORT, USB_PWR_EN_PIN);
}

bool UsbH::powerFault() {
    return !gpio_get(USB_PWR_FAULT_PORT, USB_PWR_FAULT_PIN);
}
