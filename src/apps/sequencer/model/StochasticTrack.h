#pragma once

#include "Config.h"
#include "Types.h"
#include "StochasticSequence.h"
#include "Serialize.h"
#include "Routing.h"

class StochasticTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using StochasticSequenceArray = std::array<StochasticSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    enum class MarblesMode : uint8_t {
        Off,
        On,
        Last
    };

    enum class FillMode : uint8_t {
        None,
        Gates,
        NextPattern,
        Condition,
        Last
    };

    enum class CvUpdateMode : uint8_t {
        Gate,
        Always,
        Last
    };

    //----------------------------------------
    // Properties
    //----------------------------------------

    // degreeTickets
    int degreeTicket(int index) const { return _degreeTickets[index]; }
    void setDegreeTicket(int index, int tickets) {
        _degreeTickets[index] = clamp(tickets, -1, 127);
    }

    // linearity
    int linearity() const { return _linearity; }
    void setLinearity(int linearity) {
        _linearity = clamp(linearity, 0, 100);
    }

    // degreeRotation
    int degreeRotation() const { return _degreeRotation; }
    void setDegreeRotation(int rotation) {
        _degreeRotation = clamp(rotation, -CONFIG_USER_SCALE_SIZE, CONFIG_USER_SCALE_SIZE);
    }

    // maskRotation
    int maskRotation() const { return _maskRotation; }
    void setMaskRotation(int rotation) {
        _maskRotation = clamp(rotation, -CONFIG_USER_SCALE_SIZE, CONFIG_USER_SCALE_SIZE);
    }

    // lock
    bool lock() const { return _lock; }
    void setLock(bool lock) {
        _lock = lock;
    }

    // fillMuted
    bool fillMuted() const { return _fillMuted; }
    void setFillMuted(bool fillMuted) {
        _fillMuted = fillMuted;
    }

    // loopFirst
    int loopFirst() const { return _loopFirst; }
    void setLoopFirst(int loopFirst) {
        _loopFirst = clamp(loopFirst, 0, CONFIG_STEP_COUNT - 1);
    }

    // loopLast
    int loopLast() const { return std::max(loopFirst(), int(_loopLast)); }
    void setLoopLast(int loopLast) {
        _loopLast = clamp(loopLast, loopFirst(), CONFIG_STEP_COUNT - 1);
    }

    // accentProb
    int accentProb() const { return _accentProb; }
    void setAccentProb(int accentProb) {
        _accentProb = clamp(accentProb, 0, 100);
    }

    // legatoProb
    int legatoProb() const { return _legatoProb; }
    void setLegatoProb(int legatoProb) {
        _legatoProb = clamp(legatoProb, 0, 100);
    }

    // marblesMode
    MarblesMode marblesMode() const { return _marblesMode; }
    void setMarblesMode(MarblesMode mode) {
        _marblesMode = ModelUtils::clampedEnum(mode);
    }

    // marblesSpread
    int marblesSpread() const { return _marblesSpread; }
    void setMarblesSpread(int spread) {
        _marblesSpread = clamp(spread, 0, 100);
    }

    // marblesBias
    int marblesBias() const { return _marblesBias; }
    void setMarblesBias(int bias) {
        _marblesBias = clamp(bias, 0, 100);
    }

    // marblesSteps
    int marblesSteps() const { return _marblesSteps; }
    void setMarblesSteps(int steps) {
        _marblesSteps = clamp(steps, 0, 100);
    }

    // power
    int power() const { return _power.get(isRouted(Routing::Target::Power)); }
    void setPower(int power, bool routed = false) {
        _power.set(clamp(power, 0, 100), routed);
    }

    // skew
    int skew() const { return _skew.get(isRouted(Routing::Target::Skew)); }
    void setSkew(int skew, bool routed = false) {
        _skew.set(clamp(skew, -100, 100), routed);
    }

    // looseness
    int looseness() const { return _looseness.get(isRouted(Routing::Target::Looseness)); }
    void setLooseness(int looseness, bool routed = false) {
        _looseness.set(clamp(looseness, 0, 100), routed);
    }

    // minDegree
    int minDegree() const { return _minDegree; }
    void setMinDegree(int degree) {
        _minDegree = clamp(degree, 0, 127);
    }

    // maxDegree
    int maxDegree() const { return std::max(minDegree(), int(_maxDegree)); }
    void setMaxDegree(int degree) {
        _maxDegree = clamp(degree, minDegree(), 127);
    }

    // slideTime
    int slideTime() const { return _slideTime.get(isRouted(Routing::Target::SlideTime)); }
    void setSlideTime(int slideTime, bool routed = false) {
        _slideTime.set(clamp(slideTime, 0, 100), routed);
    }

    // octave
    int octave() const { return _octave.get(isRouted(Routing::Target::Octave)); }
    void setOctave(int octave, bool routed = false) {
        _octave.set(clamp(octave, -10, 10), routed);
    }

    // transpose
    int transpose() const { return _transpose.get(isRouted(Routing::Target::Transpose)); }
    void setTranspose(int transpose, bool routed = false) {
        _transpose.set(clamp(transpose, -100, 100), routed);
    }

    // rotate
    int rotate() const { return _rotate.get(isRouted(Routing::Target::Rotate)); }
    void setRotate(int rotate, bool routed = false) {
        _rotate.set(clamp(rotate, -CONFIG_STEP_COUNT, CONFIG_STEP_COUNT), routed);
    }

    // fillMode
    FillMode fillMode() const { return _fillMode; }
    void setFillMode(FillMode fillMode) {
        _fillMode = ModelUtils::clampedEnum(fillMode);
    }

    // cvUpdateMode
    CvUpdateMode cvUpdateMode() const { return _cvUpdateMode; }
    void setCvUpdateMode(CvUpdateMode cvUpdateMode) {
        _cvUpdateMode = ModelUtils::clampedEnum(cvUpdateMode);
    }

    // Biases
    int gateBias() const { return _gateBias.get(isRouted(Routing::Target::GateProbabilityBias)); }
    void setGateBias(int gateBias, bool routed = false) {
        _gateBias.set(clamp(gateBias, -100, 100), routed);
    }

    int retriggerBias() const { return _retriggerBias.get(isRouted(Routing::Target::RetriggerProbabilityBias)); }
    void setRetriggerBias(int retriggerBias, bool routed = false) {
        _retriggerBias.set(clamp(retriggerBias, -100, 100), routed);
    }

    int lengthBias() const { return _lengthBias.get(isRouted(Routing::Target::LengthBias)); }
    void setLengthBias(int lengthBias, bool routed = false) {
        _lengthBias.set(clamp(lengthBias, -100, 100), routed);
    }

    int noteBias() const { return _noteBias.get(isRouted(Routing::Target::NoteProbabilityBias)); }
    void setNoteBias(int noteBias, bool routed = false) {
        _noteBias.set(clamp(noteBias, -100, 100), routed);
    }

    // sequences
    const StochasticSequenceArray &sequences() const { return _sequences; }
          StochasticSequenceArray &sequences()       { return _sequences; }

    const StochasticSequence &sequence(int index) const { return _sequences[index]; }
          StochasticSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    //----------------------------------------
    // Methods
    //----------------------------------------

    StochasticTrack() { clear(); }

    void clear() {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            _degreeTickets[i] = 10;
        }
        _linearity = 0;
        _degreeRotation = 0;
        _maskRotation = 0;
        _lock = false;
        _fillMuted = true;
        _loopFirst = 0;
        _loopLast = CONFIG_STEP_COUNT - 1;
        _accentProb = 0;
        _legatoProb = 0;
        _marblesMode = MarblesMode::Off;
        _marblesSpread = 50;
        _marblesBias = 50;
        _marblesSteps = 100;
        _power.setBase(100);
        _skew.setBase(0);
        _looseness.setBase(0);
        _minDegree = 0;
        _maxDegree = 127;
        _slideTime.setBase(0);
        _octave.setBase(0);
        _transpose.setBase(0);
        _rotate.setBase(0);
        _fillMode = FillMode::None;
        _cvUpdateMode = CvUpdateMode::Gate;
        _gateBias.setBase(0);
        _retriggerBias.setBase(0);
        _lengthBias.setBase(0);
        _noteBias.setBase(0);

        for (auto &sequence : _sequences) {
            sequence.clear();
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) writer.write(_degreeTickets[i]);
        writer.write(_linearity);
        writer.write(_degreeRotation);
        writer.write(_maskRotation);
        writer.write(_lock);
        writer.write(_fillMuted);
        writer.write(_loopFirst);
        writer.write(_loopLast);
        writer.write(_accentProb);
        writer.write(_legatoProb);
        writer.write(static_cast<uint8_t>(_marblesMode));
        writer.write(_marblesSpread);
        writer.write(_marblesBias);
        writer.write(_marblesSteps);
        _power.write(writer);
        _skew.write(writer);
        _looseness.write(writer);
        writer.write(_minDegree);
        writer.write(_maxDegree);
        _slideTime.write(writer);
        _octave.write(writer);
        _transpose.write(writer);
        _rotate.write(writer);
        writer.write(static_cast<uint8_t>(_fillMode));
        writer.write(static_cast<uint8_t>(_cvUpdateMode));
        _gateBias.write(writer);
        _retriggerBias.write(writer);
        _lengthBias.write(writer);
        _noteBias.write(writer);

        for (const auto &sequence : _sequences) {
            sequence.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) reader.read(_degreeTickets[i]);
        reader.read(_linearity);
        reader.read(_degreeRotation);
        reader.read(_maskRotation);
        reader.read(_lock);
        reader.read(_fillMuted);
        reader.read(_loopFirst);
        reader.read(_loopLast);
        reader.read(_accentProb);
        reader.read(_legatoProb);
        uint8_t marblesMode;
        reader.read(marblesMode);
        _marblesMode = static_cast<MarblesMode>(marblesMode);
        reader.read(_marblesSpread);
        reader.read(_marblesBias);
        reader.read(_marblesSteps);
        _power.read(reader);
        _skew.read(reader);
        _looseness.read(reader);
        reader.read(_minDegree);
        reader.read(_maxDegree);
        _slideTime.read(reader);
        _octave.read(reader);
        _transpose.read(reader);
        _rotate.read(reader);
        uint8_t fillMode;
        reader.read(fillMode);
        _fillMode = static_cast<FillMode>(fillMode);
        uint8_t cvUpdateMode;
        reader.read(cvUpdateMode);
        _cvUpdateMode = static_cast<CvUpdateMode>(cvUpdateMode);
        _gateBias.read(reader);
        _retriggerBias.read(reader);
        _lengthBias.read(reader);
        _noteBias.read(reader);

        for (auto &sequence : _sequences) {
            sequence.read(reader);
        }
    }

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;

    int8_t _degreeTickets[CONFIG_USER_SCALE_SIZE];
    uint8_t _linearity;
    int8_t _degreeRotation;
    int8_t _maskRotation;
    bool _lock;
    bool _fillMuted;
    uint8_t _loopFirst;
    uint8_t _loopLast;
    uint8_t _accentProb;
    uint8_t _legatoProb;
    MarblesMode _marblesMode;
    uint8_t _marblesSpread;
    uint8_t _marblesBias;
    uint8_t _marblesSteps;
    
    Routable<uint8_t> _power;
    Routable<int8_t> _skew;
    Routable<uint8_t> _looseness;

    uint8_t _minDegree;
    uint8_t _maxDegree;
    
    Routable<uint8_t> _slideTime;
    Routable<int8_t> _octave;
    Routable<int8_t> _transpose;
    Routable<int8_t> _rotate;
    FillMode _fillMode;
    CvUpdateMode _cvUpdateMode;
    Routable<int8_t> _gateBias;
    Routable<int8_t> _retriggerBias;
    Routable<int8_t> _lengthBias;
    Routable<int8_t> _noteBias;

    StochasticSequenceArray _sequences;

    friend class Track;
};
