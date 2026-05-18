#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"

#include "core/math/Math.h"

#include "StochasticTypes.h"

class StochasticSequence {
public:
    //----------------------------------------
    // Properties
    //----------------------------------------

    // trackIndex
    int trackIndex() const { return _trackIndex; }
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    // scale
    int scale() const { return _scale; }
    void setScale(int scale) { _scale = clamp(scale, -1, Scale::Count - 1); }
    const Scale &selectedScale(int defaultScale) const {
        return Scale::get(_scale != -1 ? _scale : defaultScale);
    }

    // rootNote
    int rootNote() const { return _rootNote; }
    void setRootNote(int rootNote) { _rootNote = clamp(rootNote, -1, 11); }
    int selectedRootNote(int defaultRootNote) const {
        return _rootNote != -1 ? _rootNote : defaultRootNote;
    }

    // divisor
    int divisor() const { return _divisor; }
    void setDivisor(int divisor) { _divisor = ModelUtils::clampDivisor(divisor); }

    // resetMeasure
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) { _resetMeasure = clamp(resetMeasure, 0, 128); }

    void printDivisor(StringBuilder &str) const {
        ModelUtils::printDivisor(str, _divisor);
    }

    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
    }

    void printResetMeasure(StringBuilder &str) const {
        if (_resetMeasure == 0) {
            str("Off");
        } else {
            str("%d", _resetMeasure);
        }
    }

    void editResetMeasure(int value, bool shift) {
        setResetMeasure(resetMeasure() + value);
    }

    void printScale(StringBuilder &str) const {
        if (scale() == -1) {
            str("Default");
        } else {
            str(Scale::name(scale()));
        }
    }

    void editScale(int value, bool shift) {
        setScale(scale() + value);
    }

    void printRootNote(StringBuilder &str) const {
        if (rootNote() == -1) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }

    void editRootNote(int value, bool shift) {
        setRootNote(rootNote() + value);
    }

    // Phase 7 Pattern controls (Per-pattern metadata)
    int size() const { return _size; }
    void setSize(int size) {
        _size = clamp(size, 2, CONFIG_STEP_COUNT);
        _first = clamp(int(_first), 0, int(_size) - 1);
        _last = clamp(int(_last), int(_first), int(_size) - 1);
    }

    int first() const { return _first; }
    void setFirst(int first) { _first = clamp(first, 0, int(_size) - 1); }

    int last() const { return clamp(int(_last), first(), int(_size) - 1); }
    void setLast(int last) { _last = clamp(last, first(), int(_size) - 1); }

    bool patternValid() const { return rhythmValid() && melodyValid(); }
    void setPatternValid(bool valid) { setRhythmValid(valid); setMelodyValid(valid); }

    uint32_t seed() const { return rhythmSeed(); }
    void setSeed(uint32_t seed) { setRhythmSeed(seed); }

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

    // Phase 8.2 Split Source Buffers
    const std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> &events() const { return _events; }
          std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> &events()       { return _events; }

    bool rhythmValid() const { return _rhythmValid; }
    void setRhythmValid(bool valid) { _rhythmValid = valid; }

    bool melodyValid() const { return _melodyValid; }
    void setMelodyValid(bool valid) { _melodyValid = valid; }

    uint32_t rhythmSeed() const { return _rhythmSeed; }
    void setRhythmSeed(uint32_t seed) { _rhythmSeed = seed; }

    uint32_t melodySeed() const { return _melodySeed; }
    void setMelodySeed(uint32_t seed) { _melodySeed = seed; }

    void clear() {
        _scale = -1;
        _rootNote = -1;
        _divisor = 12;
        _resetMeasure = 0;
        
        _size = 16;
        _first = 0;
        _last = 15;

        _rhythmValid = false;
        _melodyValid = false;
        _rhythmSeed = 0;
        _melodySeed = 0;

        for (auto &event : _events) event.clear();
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_scale);
        writer.write(_rootNote);
        writer.write(_divisor);
        writer.write(_resetMeasure);

        writer.write(_size);
        writer.write(_first);
        writer.write(_last);

        writer.write(_rhythmValid);
        writer.write(_melodyValid);
        writer.write(_rhythmSeed);
        writer.write(_melodySeed);

        for (const auto &event : _events) {
            writer.write(event.d0.raw);
            writer.write(event.d1.raw);
        }
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_scale);
        reader.read(_rootNote);
        reader.read(_divisor);
        reader.read(_resetMeasure);

        reader.read(_size);
        reader.read(_first);
        reader.read(_last);

        reader.read(_rhythmValid);
        reader.read(_melodyValid);
        reader.read(_rhythmSeed);
        reader.read(_melodySeed);

        for (auto &event : _events) {
            reader.read(event.d0.raw);
            reader.read(event.d1.raw);
        }

        sanitizeAfterRead();
    }

private:
    void sanitizeAfterRead() {
        _scale = clamp(int(_scale), -1, Scale::Count - 1);
        _rootNote = clamp(int(_rootNote), -1, 11);
        _divisor = ModelUtils::clampDivisor(_divisor);
        _resetMeasure = clamp(int(_resetMeasure), 0, 128);

        if (_size < 2 || _size > CONFIG_STEP_COUNT) {
            _size = 16;
            _first = 0;
            _last = 15;
            _rhythmValid = false;
            _melodyValid = false;
            _rhythmSeed = 0;
            _melodySeed = 0;
            for (auto &event : _events) event.clear();
            return;
        }

        _first = clamp(int(_first), 0, int(_size) - 1);
        _last = clamp(int(_last), int(_first), int(_size) - 1);
        _rhythmValid = _rhythmValid ? true : false;
        _melodyValid = _melodyValid ? true : false;

        for (int i = _size; i < CONFIG_STEP_COUNT; ++i) {
            _events[i].clear();
        }
    }

    int8_t _trackIndex = -1;
    int8_t _scale = -1;
    int8_t _rootNote = -1;
    uint8_t _divisor = 12;
    uint8_t _resetMeasure = 0;

    // Phase 7 Generation Parameters
    uint8_t _size;
    uint8_t _first;
    uint8_t _last;

    // Phase 8.2 Split Source Buffers
    std::array<StochasticSourceEvent, CONFIG_STEP_COUNT> _events;
    bool _rhythmValid;
    bool _melodyValid;
    uint32_t _rhythmSeed;
    uint32_t _melodySeed;

    friend class StochasticTrack;
};

static_assert(sizeof(StochasticSequence) <= 544, "StochasticSequence too large");
