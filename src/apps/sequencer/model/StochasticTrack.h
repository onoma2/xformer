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

    // Biases
    int gateBias() const { return _gateBias; }
    void setGateBias(int gateBias) {
        _gateBias = clamp(gateBias, -100, 100);
    }

    int retriggerBias() const { return _retriggerBias; }
    void setRetriggerBias(int retriggerBias) {
        _retriggerBias = clamp(retriggerBias, -100, 100);
    }

    int lengthBias() const { return _lengthBias; }
    void setLengthBias(int lengthBias) {
        _lengthBias = clamp(lengthBias, -100, 100);
    }

    int noteBias() const { return _noteBias; }
    void setNoteBias(int noteBias) {
        _noteBias = clamp(noteBias, -100, 100);
    }

    // sequences
    const StochasticSequenceArray &sequences() const { return _sequences; }
          StochasticSequenceArray &sequences()       { return _sequences; }

    const StochasticSequence &sequence(int index) const { return _sequences[index]; }
          StochasticSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Methods
    //----------------------------------------

    StochasticTrack() { clear(); }

    void clear() {
        for (int i = 0; i < CONFIG_USER_SCALE_SIZE; ++i) {
            _degreeTickets[i] = 10; // Default ticket count for active scale degrees.
        }
        _linearity = 0;
        _degreeRotation = 0;
        _maskRotation = 0;
        _lock = false;
        _loopFirst = 0;
        _loopLast = CONFIG_STEP_COUNT - 1;
        _accentProb = 0;
        _legatoProb = 0;
        _marblesMode = MarblesMode::Off;
        _marblesSpread = 50;
        _marblesBias = 50;
        _marblesSteps = 100;
        _minDegree = 0;
        _maxDegree = 127;
        _gateBias = 0;
        _retriggerBias = 0;
        _lengthBias = 0;
        _noteBias = 0;

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
        writer.write(_loopFirst);
        writer.write(_loopLast);
        writer.write(_accentProb);
        writer.write(_legatoProb);
        writer.write(static_cast<uint8_t>(_marblesMode));
        writer.write(_marblesSpread);
        writer.write(_marblesBias);
        writer.write(_marblesSteps);
        writer.write(_minDegree);
        writer.write(_maxDegree);
        writer.write(_gateBias);
        writer.write(_retriggerBias);
        writer.write(_lengthBias);
        writer.write(_noteBias);

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
        reader.read(_minDegree);
        reader.read(_maxDegree);
        reader.read(_gateBias);
        reader.read(_retriggerBias);
        reader.read(_lengthBias);
        reader.read(_noteBias);

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
    uint8_t _loopFirst;
    uint8_t _loopLast;
    uint8_t _accentProb;
    uint8_t _legatoProb;
    MarblesMode _marblesMode;
    uint8_t _marblesSpread;
    uint8_t _marblesBias;
    uint8_t _marblesSteps;
    uint8_t _minDegree;
    uint8_t _maxDegree;
    int8_t _gateBias;
    int8_t _retriggerBias;
    int8_t _lengthBias;
    int8_t _noteBias;

    StochasticSequenceArray _sequences;

    friend class Track;
};
