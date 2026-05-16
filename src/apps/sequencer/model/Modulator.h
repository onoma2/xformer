#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "ModelUtils.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <cstdint>

class Modulator {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    enum class Shape : uint8_t {
        Sine,
        Triangle,
        SawUp,
        SawDown,
        Square,
        Random,
        ADSR,
        ChaosLorenz,
        ChaosLatoocarfian,
        Last
    };

    static const char *shapeName(Shape shape) {
        switch (shape) {
        case Shape::Sine:       return "Sine";
        case Shape::Triangle:  return "Triangle";
        case Shape::SawUp:     return "Saw Up";
        case Shape::SawDown:   return "Saw Down";
        case Shape::Square:    return "Square";
        case Shape::Random:    return "Random";
        case Shape::ADSR:      return "ADSR";
        case Shape::ChaosLorenz:       return "ChaosL";
        case Shape::ChaosLatoocarfian: return "ChaosLa";
        case Shape::Last:      break;
        }
        return nullptr;
    }

    enum class Mode : uint8_t {
        Free,
        Sync,
        Hold,
        Retrigger,
        Last
    };

    static const char *modeName(Mode mode) {
        switch (mode) {
        case Mode::Free:       return "Free";
        case Mode::Sync:       return "Sync";
        case Mode::Hold:       return "Hold";
        case Mode::Retrigger:  return "Retrig";
        case Mode::Last:       break;
        }
        return nullptr;
    }

    enum class RandomMode : uint8_t {
        Clocked,
        Triggered,
        Last
    };

    static const char *randomModeName(RandomMode mode) {
        switch (mode) {
        case RandomMode::Clocked:    return "Clocked";
        case RandomMode::Triggered:  return "Triggered";
        case RandomMode::Last:       break;
        }
        return nullptr;
    }

    //----------------------------------------
    // Properties
    //----------------------------------------

    // shape

    Shape shape() const { return _shape; }
    void setShape(Shape shape) {
        _shape = ModelUtils::clampedEnum(shape);
    }

    void editShape(int value, bool shift) {
        setShape(ModelUtils::adjustedEnum(shape(), value));
    }

    static bool isChaosShape(Shape shape) {
        return shape == Shape::ChaosLorenz || shape == Shape::ChaosLatoocarfian;
    }
    void printShape(StringBuilder &str) const {
        str(shapeName(shape()));
    }

    // rate (in ticks: 6..6144, musical divisions of PPQN)

    int rate() const { return _rate; }
    void setRate(int rate) {
        _rate = clamp(rate, 6, 6144);
    }

    void editRate(int value, bool shift);

    void printRate(StringBuilder &str) const;

    // depth (0-127)

    int depth() const { return _depth; }
    void setDepth(int depth) {
        _depth = clamp(depth, 0, 127);
    }

    void editDepth(int value, bool shift) {
        int multiplier = shift ? 1 : 4;
        setDepth(depth() + value * multiplier);
    }

    void printDepth(StringBuilder &str) const {
        str("%d", depth());
    }

    // offset (-64 to +63)

    int offset() const { return _offset; }
    void setOffset(int offset) {
        _offset = clamp(offset, -64, 63);
    }

    void editOffset(int value, bool shift) {
        int multiplier = shift ? 1 : 4;
        setOffset(offset() + value * multiplier);
    }

    void printOffset(StringBuilder &str) const {
        str("%+d", offset());
    }

    // phase (0-359 degrees)

    int phase() const { return _phase; }
    void setPhase(int phase) {
        while (phase < 0) phase += 360;
        while (phase >= 360) phase -= 360;
        _phase = phase;
    }

    void editPhase(int value, bool shift);

    void printPhase(StringBuilder &str) const {
        str("%d" "\xc2\xb0", phase());
    }

    // smooth (0-5000ms, slew time for random shapes)

    int smooth() const { return _smooth; }
    void setSmooth(int smooth) {
        _smooth = clamp(smooth, 0, 5000);
    }

    void editSmooth(int value, bool shift) {
        setSmooth(smooth() + value * (shift ? 1 : 50));
    }

    void printSmooth(StringBuilder &str) const {
        str("%dms", smooth());
    }

    // gateTrack (0-7, which track's gate triggers sync/retrigger/triggered-random)

    int gateTrack() const { return _gateTrack; }
    void setGateTrack(int track) {
        _gateTrack = clamp(track, 0, CONFIG_TRACK_COUNT - 1);
    }

    void editGateTrack(int value, bool shift) {
        setGateTrack(gateTrack() + value);
    }

    void printGateTrack(StringBuilder &str) const {
        str("T%d", gateTrack() + 1);
    }

    // randomMode (Clocked or Triggered)

    RandomMode randomMode() const { return _randomMode; }
    void setRandomMode(RandomMode mode) {
        _randomMode = ModelUtils::clampedEnum(mode);
    }

    void editRandomMode(int value, bool shift) {
        setRandomMode(ModelUtils::adjustedEnum(randomMode(), value));
    }

    void printRandomMode(StringBuilder &str) const {
        str(randomModeName(randomMode()));
    }

    // mode (Free/Sync/Retrigger for LFO shapes)

    Mode mode() const { return _mode; }
    void setMode(Mode mode) {
        _mode = ModelUtils::clampedEnum(mode);
    }

    void editMode(int value, bool shift) {
        setMode(ModelUtils::adjustedEnum(mode(), value));
    }

    void printMode(StringBuilder &str) const {
        str(modeName(mode()));
    }

    // attack (0-2000ms, for ADSR)

    int attack() const { return _attack; }
    void setAttack(int attack) {
        _attack = clamp(attack, 0, 2000);
    }

    void editAttack(int value, bool shift) {
        setAttack(attack() + value * (shift ? 1 : 20));
    }

    void printAttack(StringBuilder &str) const {
        str("%dms", attack());
    }

    // decay (0-2000ms, for ADSR)

    int decay() const { return _decay; }
    void setDecay(int decay) {
        _decay = clamp(decay, 0, 2000);
    }

    void editDecay(int value, bool shift) {
        setDecay(decay() + value * (shift ? 1 : 20));
    }

    void printDecay(StringBuilder &str) const {
        str("%dms", decay());
    }

    // sustain (0-127, for ADSR)

    int sustain() const { return _sustain; }
    void setSustain(int sustain) {
        _sustain = clamp(sustain, 0, 127);
    }

    void editSustain(int value, bool shift) {
        int absValue = (value < 0) ? -value : value;
        int multiplier = shift ? 4 : 1;
        if (absValue >= 4) multiplier *= 4;
        else if (absValue >= 2) multiplier *= 2;
        setSustain(sustain() + value * multiplier);
    }

    void printSustain(StringBuilder &str) const {
        str("%d", sustain());
    }

    // release (0-2000ms, for ADSR)

    int release() const { return _release; }
    void setRelease(int release) {
        _release = clamp(release, 0, 2000);
    }

    void editRelease(int value, bool shift) {
        setRelease(release() + value * (shift ? 1 : 20));
    }

    void printRelease(StringBuilder &str) const {
        str("%dms", release());
    }

    // amplitude (0-127, for ADSR)

    int amplitude() const { return _amplitude; }
    void setAmplitude(int amplitude) {
        _amplitude = clamp(amplitude, 0, 127);
    }

    void editAmplitude(int value, bool shift) {
        setAmplitude(amplitude() + value * (shift ? 1 : 4));
    }

    void printAmplitude(StringBuilder &str) const {
        str("%d", amplitude());
    }

    //----------------------------------------
    // Methods
    //----------------------------------------

    void clear() {
        setShape(Shape::Sine);
        setRate(96);
        setDepth(127);
        setOffset(0);
        setPhase(0);
        setSmooth(100);
        setGateTrack(0);
        setRandomMode(RandomMode::Clocked);
        setMode(Mode::Free);
        setAttack(100);
        setDecay(100);
        setSustain(100);
        setRelease(200);
        setAmplitude(127);
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_shape);
        writer.write(_rate);
        writer.write(_depth);
        writer.write(_offset);
        writer.write(_phase);
        writer.write(_smooth);
        writer.write(_gateTrack);
        writer.write(_randomMode);
        writer.write(_mode);
        writer.write(_attack);
        writer.write(_decay);
        writer.write(_sustain);
        writer.write(_release);
        writer.write(_amplitude);
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_shape);
        reader.read(_rate);
        reader.read(_depth);
        reader.read(_offset);
        reader.read(_phase);
        reader.read(_smooth);
        reader.read(_gateTrack);
        reader.read(_randomMode);
        reader.read(_mode);
        reader.read(_attack);
        reader.read(_decay);
        reader.read(_sustain);
        reader.read(_release);
        reader.read(_amplitude, 127);
    }

private:
    Shape _shape = Shape::Sine;
    uint16_t _rate = 96;
    uint8_t _depth = 127;
    int8_t _offset = 0;
    uint16_t _phase = 0;
    uint16_t _smooth = 100;
    uint8_t _gateTrack = 0;
    RandomMode _randomMode = RandomMode::Clocked;
    Mode _mode = Mode::Free;
    uint16_t _attack = 100;
    uint16_t _decay = 100;
    uint8_t _sustain = 100;
    uint16_t _release = 200;
    uint8_t _amplitude = 127;
};
