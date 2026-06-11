#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Routing.h"

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
        Spring,
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
        case Shape::ChaosLorenz:       return "Lorenz";
        case Shape::ChaosLatoocarfian: return "Latoocarfian";
        case Shape::Spring:    return "Spring";
        case Shape::Last:      break;
        }
        return nullptr;
    }

    // Spring shape reuses: rate=STRIKE, attack=TENSION, decay=RING, smooth=CLANG,
    // phase=PICKUP (0..4). See docs/spring-modulator-spec.md.
    static bool isSpringShape(Shape shape) { return shape == Shape::Spring; }
    static const char *springPickupName(int p) {
        static const char *n[5] = { "Position", "Velocity", "Kinetic", "Potential", "Total" };
        return (p >= 0 && p < 5) ? n[p] : nullptr;
    }
    int springPickup() const { return clamp(int(_phase), 0, 4); }
    void setSpringPickup(int p) { setPhase(clamp(p, 0, 4)); }
    void editSpringPickup(int v, bool) { setSpringPickup(springPickup() + v); }

    // Gate behavior (orthogonal to the rate domain): Run = free-running (gate ignored);
    // Trig = reset phase to offset on gate rising (a 0 offset gives hard sync); Gate =
    // advance only while gate high, reset on rising (windowed).
    enum class Mode : uint8_t {
        Run,
        Trig,
        Gate,
        Last
    };

    static const char *modeName(Mode mode) {
        switch (mode) {
        case Mode::Run:   return "FREE";
        case Mode::Trig:  return "TRIG";
        case Mode::Gate:  return "HOLD";
        case Mode::Last:  break;
        }
        return nullptr;
    }

    // Rate domain: Free = wall-time Hz (tempo-independent, runs when stopped);
    // Tempo = clock division (tempo-locked, freezes with the transport).
    enum class RateDomain : uint8_t {
        Free,
        Tempo,
        Last
    };

    static const char *rateDomainName(RateDomain domain) {
        switch (domain) {
        case RateDomain::Free:  return "FREE";
        case RateDomain::Tempo: return "TEMPO";
        case RateDomain::Last:  break;
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

    // rate — domain-keyed: Free = centi-Hz (1..1600 = 0.01..16Hz), Tempo = PPQN ticks (6..6144).

    int rate() const { return _rate; }
    void setRate(int rate) {
        _rate = (_rateDomain == RateDomain::Free) ? clamp(rate, 1, 1600) : clamp(rate, 6, 6144);
    }
    float rateHz() const { return _rate / 100.f; }   // Free-domain reading

    void editRate(int value, bool shift);
    void printRate(StringBuilder &str) const;

    // rateDomain — Free (wall-Hz) vs Tempo (clock division). cycle = re-press RATE.
    RateDomain rateDomain() const { return _rateDomain; }
    void setRateDomain(RateDomain domain) {
        _rateDomain = ModelUtils::clampedEnum(domain);
        setRate(_rate);   // re-clamp the raw value into the new domain's range
    }
    void cycleRateDomain() {
        setRateDomain(_rateDomain == RateDomain::Free ? RateDomain::Tempo : RateDomain::Free);
    }
    void printRateDomain(StringBuilder &str) const { str(rateDomainName(_rateDomain)); }

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

    // Output floor (silence/rest level) in 0..127 output units, where 64 = 0V at the
    // CV stage. Reuses offset as the baseline: default offset 0 -> rest at 0V.
    int floorValue() const { return clamp(64 + int(_offset), 0, 127); }

    // invert: flip the output around its range (unipolar envelopes duck; bipolar phase-flip)

    bool invert() const { return _invert; }
    void setInvert(bool invert) { _invert = invert; }

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

    // gateSource: what gates Trig/Hold and triggered-random. Any track gate,
    // CV-IN, routing bus, or another modulator (self excluded = protected).
    Routing::Source gateSource() const { return _gateSource; }
    void setGateSource(Routing::Source source) { _gateSource = source; }
    void printGateSource(StringBuilder &str) const { Routing::printSourceAbbrev(_gateSource, str); }

    // Curated selectable set, in cycle order; self-modulator filtered at runtime.
    static const Routing::Source *gateSourceList(int &count) {
        using S = Routing::Source;
        static const Routing::Source list[] = {
            S::GateOut1, S::GateOut2, S::GateOut3, S::GateOut4,
            S::GateOut5, S::GateOut6, S::GateOut7, S::GateOut8,
            S::CvIn1, S::CvIn2, S::CvIn3, S::CvIn4,
            S::BusCv1, S::BusCv2, S::BusCv3, S::BusCv4,
            S::Mod1, S::Mod2, S::Mod3, S::Mod4, S::Mod5, S::Mod6, S::Mod7, S::Mod8,
        };
        count = int(sizeof(list) / sizeof(list[0]));
        return list;
    }
    static bool gateSourceAllowed(Routing::Source s, int selfModIndex) {
        int n; auto *list = gateSourceList(n);
        for (int i = 0; i < n; ++i) {
            if (list[i] == s) {
                return !(Routing::isModulatorSource(s) &&
                         Routing::modulatorSourceIndex(s) == selfModIndex);
            }
        }
        return false;
    }
    static Routing::Source cycleGateSource(Routing::Source cur, int dir, int selfModIndex) {
        int n; auto *list = gateSourceList(n);
        int idx = 0;
        for (int i = 0; i < n; ++i) { if (list[i] == cur) { idx = i; break; } }
        int step = (dir >= 0) ? 1 : -1;
        for (int k = 0; k < n; ++k) {
            idx = (idx + step + n) % n;
            Routing::Source s = list[idx];
            if (!(Routing::isModulatorSource(s) &&
                  Routing::modulatorSourceIndex(s) == selfModIndex)) {
                return s;
            }
        }
        return cur;
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
        setRateDomain(RateDomain::Free);
        setRate(5);     // 0.05 Hz (wall-time, all start Free)
        setDepth(25);  // ~±1V default
        setOffset(0);
        setPhase(0);
        setSmooth(100);
        setGateSource(Routing::Source::GateOut1);
        setMode(Mode::Run);
        setAttack(900);   // P1 = 45 for chaos
        setDecay(1240);  // P2 = 62 for chaos
        setSustain(100);
        setRelease(200);
        setAmplitude(127);
        setInvert(false);
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_shape);
        writer.write(_rate);
        writer.write(_depth);
        writer.write(_offset);
        writer.write(_phase);
        writer.write(_smooth);
        writer.write(_gateSource);
        writer.write(_mode);
        writer.write(_attack);
        writer.write(_decay);
        writer.write(_sustain);
        writer.write(_release);
        writer.write(_amplitude);
        writer.write(_rateDomain);
        writer.write(_invert);
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_shape);
        reader.read(_rate);
        reader.read(_depth);
        reader.read(_offset);
        reader.read(_phase);
        reader.read(_smooth);
        reader.read(_gateSource);
        reader.read(_mode);
        reader.read(_attack);
        reader.read(_decay);
        reader.read(_sustain);
        reader.read(_release);
        reader.read(_amplitude);
        reader.read(_rateDomain);
        reader.read(_invert);

        // Sanitize raw-read fields against bad/old file data: out-of-range enums
        // would fall through engine switches; rate is clamped to its domain range.
        _shape = ModelUtils::clampedEnum(_shape);
        _mode = ModelUtils::clampedEnum(_mode);
        _rateDomain = ModelUtils::clampedEnum(_rateDomain);
        setRate(_rate);
    }

private:
    Shape _shape = Shape::Sine;
    uint16_t _rate = 96;
    uint8_t _depth = 25;
    int8_t _offset = 0;
    uint16_t _phase = 0;
    uint16_t _smooth = 100;
    Routing::Source _gateSource = Routing::Source::GateOut1;
    Mode _mode = Mode::Run;
    RateDomain _rateDomain = RateDomain::Free;
    uint16_t _attack = 100;
    uint16_t _decay = 100;
    uint8_t _sustain = 100;
    uint16_t _release = 200;
    uint8_t _amplitude = 127;
    bool _invert = false;
};
