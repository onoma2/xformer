#pragma once

#include "Key.h"

#include "engine/MidiPort.h"

#include "core/midi/MidiMessage.h"

class Event {
public:
    enum Type {
        KeyUp,
        KeyDown,
        KeyPress,
        Encoder,
        Midi,
        Keyboard,
    };

    Event(Type type) :
        _type(type)
    {}

    Type type() const { return _type; }

    bool consumed() const { return _consumed; }
    void consume() { _consumed = true; }

    template<typename T>
    inline T &as() {
        return *static_cast<T *>(this);
    }

private:
    Type _type;
    bool _consumed = false;
};

class KeyEvent : public Event {
public:
    KeyEvent(Type type, const Key &key) :
        Event(type),
        _key(key)
    {}

    const Key &key() const { return _key; }

private:
    Key _key;
};

class KeyPressEvent : public KeyEvent {
public:
    KeyPressEvent(Type type, const Key &key, int count) :
        KeyEvent(type, key),
        _count(count)
    {}

    int count() const { return _count; }

private:
    int _count;
};

class EncoderEvent : public Event {
public:
    EncoderEvent(int value, bool pressed) :
        Event(Event::Encoder),
        _value(value),
        _pressed(pressed)
    {}

    int value() const { return _value; }
    bool pressed() const { return _pressed; }

private:
    int _value;
    bool _pressed;
};

class MidiEvent : public Event {
public:
    MidiEvent(MidiPort port, const MidiMessage &message) :
        Event(Event::Midi),
        _port(port),
        _message(message)
    {}

    MidiPort port() const { return _port; }

    const MidiMessage &message() const { return _message; }

private:
    MidiPort _port;
    MidiMessage _message;
};

class KeyboardEvent : public Event {
public:
    KeyboardEvent(uint8_t keycode, uint8_t modifiers, char ch, uint8_t pressed = 1) :
        Event(Event::Keyboard),
        _keycode(keycode),
        _modifiers(modifiers),
        _ch(ch),
        _pressed(pressed)
    {}

    uint8_t keycode() const { return _keycode; }
    uint8_t modifiers() const { return _modifiers; }
    bool isPressed() const { return _pressed; }

    bool shift() const { return _modifiers & 0x22; }
    bool ctrl() const { return _modifiers & 0x11; }
    bool alt() const { return _modifiers & 0x44; }
    bool gui() const { return _modifiers & 0x88; }

    char chRaw() const { return _ch; }
    char ch() const {
        if (_modifiers & (0x11 | 0x44 | 0x88)) return 0;
        return _ch;
    }

    static constexpr uint8_t KeyEnter = 0x28;
    static constexpr uint8_t KeyEscape = 0x29;
    static constexpr uint8_t KeyBackspace = 0x2A;
    static constexpr uint8_t KeyTab = 0x2B;
    static constexpr uint8_t KeySpace = 0x2C;
    static constexpr uint8_t KeyLeft = 0x50;
    static constexpr uint8_t KeyRight = 0x4F;
    static constexpr uint8_t KeyUp = 0x52;
    static constexpr uint8_t KeyDown = 0x51;
    static constexpr uint8_t KeyDelete = 0x4C;
    static constexpr uint8_t KeyHome = 0x4A;
    static constexpr uint8_t KeyEnd = 0x4D;
    static constexpr uint8_t KeyF1 = 0x3A;
    static constexpr uint8_t KeyF2 = 0x3B;
    static constexpr uint8_t KeyF3 = 0x3C;
    static constexpr uint8_t KeyF4 = 0x3D;
    static constexpr uint8_t KeyF5 = 0x3E;
    static constexpr uint8_t KeyF6 = 0x3F;

private:
    uint8_t _keycode;
    uint8_t _modifiers;
    char _ch;
    uint8_t _pressed;
};
