#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"

#include "core/math/Math.h"

#include "StochasticTypes.h"

#include <array>
#include <cstdint>

class StochasticSequence {
public:
    StochasticSequence() { clear(); }
    StochasticSequence(int trackIndex) : _trackIndex(trackIndex) { clear(); }

    int trackIndex() const { return _trackIndex; }
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    // scale
    int scale() const { return _scale; }
    void setScale(int scale) { _scale = clamp(scale, -1, Scale::Count - 1); }
    const Scale &selectedScale(int defaultScale) const { return Scale::get(scale() < 0 ? defaultScale : scale()); }
    void editScale(int value, bool shift) { setScale(scale() + value); }
    void printScale(StringBuilder &str) const { str(scale() < 0 ? "Default" : Scale::name(scale())); }

    // rootNote
    int rootNote() const { return _rootNote; }
    void setRootNote(int rootNote) { _rootNote = clamp(rootNote, -1, 11); }
    int selectedRootNote(int defaultRootNote) const { return rootNote() < 0 ? defaultRootNote : rootNote(); }
    void editRootNote(int value, bool shift) { setRootNote(rootNote() + value); }
    void printRootNote(StringBuilder &str) const {
        if (rootNote() < 0) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    // divisor
    int divisor() const { return _divisor; }
    void setDivisor(int divisor) { _divisor = ModelUtils::clampDivisor(divisor); }
    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }
    void printDivisor(StringBuilder &str) const {
        ModelUtils::printDivisor(str, divisor());
    }

    // resetMeasure
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) { _resetMeasure = clamp(resetMeasure, 0, 127); }
    void editResetMeasure(int value, bool shift) {
        setResetMeasure(ModelUtils::adjustedByPowerOfTwo(resetMeasure(), value, shift));
    }
    void printResetMeasure(StringBuilder &str) const {
        if (resetMeasure() == 0) {
            str("Off");
        } else {
            str("%d", resetMeasure());
        }
    }

    // Phase 7 Pattern controls (Per-pattern metadata)
    int size() const { return _size; }
    void setSize(int size) { _size = clamp(size, 2, CONFIG_STEP_COUNT); }

    int first() const { return _first; }
    void setFirst(int first) { _first = clamp(first, 0, int(_size) - 1); }

    int last() const { return _last; }
    void setLast(int last) { _last = clamp(last, int(_first), int(_size) - 1); }

    bool patternValid() const { return _patternValid; }
    void setPatternValid(bool valid) { _patternValid = valid; }

    uint32_t seed() const { return _seed; }
    void setSeed(uint32_t seed) { _seed = seed; }

    void printNote(StringBuilder &str, int note, int defaultRootNote, int defaultScale) const {
        int rootNote = selectedRootNote(defaultRootNote);
        const auto &scale = selectedScale(defaultScale);
        scale.noteName(str, note, rootNote, Scale::Short1);
    }

    // Phase 6 scaffolding stubs for UI list (to keep current pages building)
    struct StepStub {
        int note() const { return 0; }
        bool gate() const { return false; }
        int gateProbability() const { return 0; }
        int noteVariationProbability() const { return 0; }
        int noteOctaveProbability() const { return 0; }
        int length() const { return 0; }
        int lengthVariationRange() const { return 0; }
        int lengthVariationProbability() const { return 0; }
        int retrigger() const { return 0; }
        int retriggerProbability() const { return 0; }
        Types::Condition condition() const { return Types::Condition::Off; }
        int gateOffset() const { return 0; }
        bool slide() const { return false; }
        bool accent() const { return false; }
        bool legato() const { return false; }
        void setGate(bool) {}
        void setGateProbability(int) {}
        void setNote(int) {}
        void setNoteVariationProbability(int) {}
        void setNoteOctaveProbability(int) {}
        void setLength(int) {}
        void setLengthVariationRange(int) {}
        void setLengthVariationProbability(int) {}
        void setRetrigger(int) {}
        void setRetriggerProbability(int) {}
        void setCondition(Types::Condition) {}
        void setGateOffset(int) {}
        void setSlide(bool) {}
        void setAccent(bool) {}
        void setLegato(bool) {}
    };

    const StepStub step(int index) const { return StepStub(); }
          StepStub step(int index)       { return StepStub(); }

    void clear() {
        _scale = -1;
        _rootNote = -1;
        _reserved[0] = 0; // was playMode
        _reserved[1] = 0; // was runMode
        _divisor = 12;
        _resetMeasure = 0;
        
        _size = 16;
        _first = 0;
        _last = 15;
        _patternValid = false;
        _seed = 0;
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_scale);
        writer.write(_rootNote);
        writer.write(_reserved[0]);
        writer.write(_reserved[1]);
        writer.write(_divisor);
        writer.write(_resetMeasure);

        writer.write(_size);
        writer.write(_first);
        writer.write(_last);
        writer.write(_patternValid);
        writer.write(_seed);
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_scale);
        reader.read(_rootNote);
        reader.read(_reserved[0]);
        reader.read(_reserved[1]);
        reader.read(_divisor);
        reader.read(_resetMeasure);

        reader.read(_size);
        reader.read(_first);
        reader.read(_last);
        reader.read(_patternValid);
        reader.read(_seed);
    }

private:
    int8_t _trackIndex = -1;
    int8_t _scale = -1;
    int8_t _rootNote = -1;
    uint8_t _reserved[2]; // was playMode, runMode
    uint8_t _divisor = 12;
    uint8_t _resetMeasure = 0;

    // Phase 7 Generation Parameters
    uint8_t _size;
    uint8_t _first;
    uint8_t _last;
    bool _patternValid;
    uint32_t _seed;

    friend class StochasticTrack;
};
