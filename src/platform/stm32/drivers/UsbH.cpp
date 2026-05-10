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

struct HidDebugHandler {
    static void debugMsg(const char *msg) {
        if (g_usbh && g_usbh->_debugMsgCallback) {
            g_usbh->_debugMsgCallback(msg, g_usbh->_debugMsgContext);
        }
    }
};

struct HidDriverHandler {
    static void connectHandler(uint8_t device_id, HID_TYPE type) {
        DBG("HID device connected (id=%d, type=%d)", device_id, type);
        if (g_usbh) {
            g_usbh->hidConnectDevice(device_id, type);
            if (g_usbh->_debugMsgCallback) {
                char msg[64];
                snprintf(msg, sizeof(msg), "HID %d t=%d", device_id, (int)type);
                g_usbh->_debugMsgCallback(msg, g_usbh->_debugMsgContext);
            }
        }
    }

    static void disconnectHandler(uint8_t device_id) {
        DBG("HID device disconnected (id=%d)", device_id);
        if (g_usbh) {
            g_usbh->hidDisconnectDevice(device_id);
        }
    }

    static void messageHandler(uint8_t device_id, const uint8_t *data, uint32_t length) {
        if (length != 8) return;
        uint8_t modifiers = data[0];
        for (int i = 2; i < 8; i++) {
            uint8_t keycode = data[i];
            if (keycode == 0) continue;
            DBG("HID key: mod=%02x key=%02x", modifiers, keycode);
            if (g_usbh && g_usbh->_debugMsgCallback) {
                char msg[32];
                snprintf(msg, sizeof(msg), "K:%02x M:%02x", keycode, modifiers);
                g_usbh->_debugMsgCallback(msg, g_usbh->_debugMsgContext);
            }
            if (g_usbh && g_usbh->_hidKeyCallback) {
                g_usbh->_hidKeyCallback(modifiers, keycode, g_usbh->_hidCallbackContext);
            }
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
	hid_config.debug_msg = &HidDebugHandler::debugMsg;
	hid_driver_init(&hid_config);
	usbh_init(lld_drivers, device_drivers);
}

void UsbH::process() {
    uint32_t time_us = HighResolutionTimer::us();

    usbh_poll(time_us);

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

void UsbH::powerOn() {
    gpio_set(USB_PWR_EN_PORT, USB_PWR_EN_PIN);
}

void UsbH::powerOff() {
    gpio_clear(USB_PWR_EN_PORT, USB_PWR_EN_PIN);
}

bool UsbH::powerFault() {
    return !gpio_get(USB_PWR_FAULT_PORT, USB_PWR_FAULT_PIN);
}
