#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Scale.h"

#include "core/math/Math.h"

#include <array>
#include <cstdint>

class StochasticSequence {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using Length = UnsignedValue<4>;
    using LengthVariationRange = SignedValue<4>;
    using LengthVariationProbability = UnsignedValue<4>;
    using Note = SignedValue<7>;
    using NoteOctave = SignedValue<3>;
    using NoteVariationProbability = UnsignedValue<4>;
    using NoteOctaveProbability = UnsignedValue<4>;

    using Retrigger = UnsignedValue<3>;
    using GateProbability = UnsignedValue<4>;
    using RetriggerProbability = UnsignedValue<4>;
    using GateOffset = SignedValue<4>;
    using Condition = UnsignedValue<7>;
    using StageRepeats = UnsignedValue<3>;
    using StageRepeatMode = UnsignedValue<3>;

    enum class Layer {
        Gate,
        GateProbability,
        GateOffset,
        Slide,
        Retrigger,
        RetriggerProbability,
        Length,
        LengthVariationRange,
        LengthVariationProbability,
        Note,
        NoteOctave,
        NoteVariationProbability,
        NoteOctaveProbability,
        Condition,
        StageRepeats,
        StageRepeatMode,
        Last
    };

    class Step {
    public:
        Step() { clear(); }

        void clear() {
            _data0.raw = 0;
            _data1.raw = 0;
            setLength(Length::Max / 2); // Default to some length
        }

        bool gate() const { return _data0.gate ? true : false; }
        void setGate(bool gate) { _data0.gate = gate; }

        bool slide() const { return _data0.slide ? true : false; }
        void setSlide(bool slide) { _data0.slide = slide; }

        int length() const { return _data0.length; }
        void setLength(int length) { _data0.length = Length::clamp(length); }

        int lengthVariationRange() const { return LengthVariationRange::Min + _data0.lengthVariationRange; }
        void setLengthVariationRange(int lengthVariationRange) { _data0.lengthVariationRange = LengthVariationRange::clamp(lengthVariationRange) - LengthVariationRange::Min; }

        int lengthVariationProbability() const { return _data0.lengthVariationProbability; }
        void setLengthVariationProbability(int lengthVariationProbability) { _data0.lengthVariationProbability = LengthVariationProbability::clamp(lengthVariationProbability); }

        int note() const { return Note::Min + _data0.note; }
        void setNote(int note) { _data0.note = Note::clamp(note) - Note::Min; }

        int noteOctave() const { return NoteOctave::Min + _data0.noteOctave; }
        void setNoteOctave(int noteOctave) { _data0.noteOctave = NoteOctave::clamp(noteOctave) - NoteOctave::Min; }

        int noteVariationProbability() const { return _data0.noteVariationProbability; }
        void setNoteVariationProbability(int noteVariationProbability) { _data0.noteVariationProbability = NoteVariationProbability::clamp(noteVariationProbability); }

        int noteOctaveProbability() const { return _data0.noteOctaveProbability; }
        void setNoteOctaveProbability(int noteOctaveProbability) { _data0.noteOctaveProbability = NoteOctaveProbability::clamp(noteOctaveProbability); }

        bool bypassScale() const { return _data1.bypassScale ? true : false; }
        void setBypassScale(bool bypassScale) { _data1.bypassScale = bypassScale; }

        int retrigger() const { return _data1.retrigger; }
        void setRetrigger(int retrigger) { _data1.retrigger = Retrigger::clamp(retrigger); }

        int gateProbability() const { return _data1.gateProbability; }
        void setGateProbability(int gateProbability) { _data1.gateProbability = GateProbability::clamp(gateProbability); }

        int retriggerProbability() const { return _data1.retriggerProbability; }
        void setRetriggerProbability(int retriggerProbability) { _data1.retriggerProbability = RetriggerProbability::clamp(retriggerProbability); }

        int gateOffset() const { return GateOffset::Min + _data1.gateOffset; }
        void setGateOffset(int gateOffset) { _data1.gateOffset = std::max(0, GateOffset::clamp(gateOffset)) - GateOffset::Min; }

        Types::Condition condition() const { return Types::Condition(int(_data1.condition)); }
        void setCondition(Types::Condition condition) { _data1.condition = int(ModelUtils::clampedEnum(condition)); }

        int stageRepeats() const { return _data1.stageRepeats; }
        void setStageRepeats(int stageRepeats) { _data1.stageRepeats = StageRepeats::clamp(stageRepeats); }

        int stageRepeatMode() const { return _data1.stageRepeatMode; }
        void setStageRepeatMode(int stageRepeatMode) { _data1.stageRepeatMode = StageRepeatMode::clamp(stageRepeatMode); }

        void write(VersionedSerializedWriter &writer) const {
            writer.write(_data0.raw);
            writer.write(_data1.raw);
        }

        void read(VersionedSerializedReader &reader) {
            reader.read(_data0.raw);
            reader.read(_data1.raw);
        }

        bool operator==(const Step &other) const {
            return _data0.raw == other._data0.raw && _data1.raw == other._data1.raw;
        }

        bool operator!=(const Step &other) const {
            return !(*this == other);
        }

    private:
        union {
            uint32_t raw;
            BitField<uint32_t, 0, 1> gate;
            BitField<uint32_t, 1, 1> slide;
            BitField<uint32_t, 2, Length::Bits> length;
            BitField<uint32_t, 6, LengthVariationRange::Bits> lengthVariationRange;
            BitField<uint32_t, 10, LengthVariationProbability::Bits> lengthVariationProbability;
            BitField<uint32_t, 14, Note::Bits> note;
            BitField<uint32_t, 21, NoteOctave::Bits> noteOctave;
            BitField<uint32_t, 24, NoteVariationProbability::Bits> noteVariationProbability;
            BitField<uint32_t, 28, NoteOctaveProbability::Bits> noteOctaveProbability;
        } _data0;

        union {
            uint32_t raw;
            BitField<uint32_t, 0, 1> bypassScale;
            BitField<uint32_t, 1, Retrigger::Bits> retrigger;
            BitField<uint32_t, 4, GateProbability::Bits> gateProbability;
            BitField<uint32_t, 8, RetriggerProbability::Bits> retriggerProbability;
            BitField<uint32_t, 12, GateOffset::Bits> gateOffset;
            BitField<uint32_t, 16, Condition::Bits> condition;
            BitField<uint32_t, 23, StageRepeats::Bits> stageRepeats;
            BitField<uint32_t, 26, StageRepeatMode::Bits> stageRepeatMode;
        } _data1;
    };

    using StepArray = std::array<Step, CONFIG_STEP_COUNT>;

    StochasticSequence() { clear(); }
    StochasticSequence(int trackIndex) : _trackIndex(trackIndex) { clear(); }

    int trackIndex() const { return _trackIndex; }
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    // scale
    int scale() const { return _scale; }
    void setScale(int scale) {
        _scale = clamp(scale, -1, Scale::Count - 1);
    }
    void editScale(int value, bool shift) {
        setScale(scale() + value);
    }
    void printScale(StringBuilder &str) const {
        str(scale() < 0 ? "Default" : Scale::name(scale()));
    }
    const Scale &selectedScale(int defaultScale) const {
        return Scale::get(scale() < 0 ? defaultScale : scale());
    }

    // rootNote
    int rootNote() const { return _rootNote; }
    void setRootNote(int rootNote) {
        _rootNote = clamp(rootNote, -1, 11);
    }
    void editRootNote(int value, bool shift) {
        setRootNote(rootNote() + value);
    }
    void printRootNote(StringBuilder &str) const {
        if (rootNote() < 0) {
            str("Default");
        } else {
            Types::printNote(str, rootNote());
        }
    }
    // selectedRootNote
    int selectedRootNote(int defaultRootNote) const {
        return rootNote() < 0 ? defaultRootNote : rootNote();
    }

    // playMode
    Types::PlayMode playMode() const { return _playMode; }
    void setPlayMode(Types::PlayMode playMode) {
        _playMode = ModelUtils::clampedEnum(playMode);
    }
    void editPlayMode(int value, bool shift) {
        setPlayMode(ModelUtils::adjustedEnum(_playMode, value));
    }
    void printPlayMode(StringBuilder &str) const {
        str(Types::playModeName(_playMode));
    }

    // runMode
    Types::RunMode runMode() const { return _runMode; }
    void setRunMode(Types::RunMode runMode) {
        _runMode = ModelUtils::clampedEnum(runMode);
    }
    void editRunMode(int value, bool shift) {
        setRunMode(ModelUtils::adjustedEnum(_runMode, value));
    }
    void printRunMode(StringBuilder &str) const {
        str(Types::runModeName(_runMode));
    }

    // firstStep
    int firstStep() const { return _firstStep; }
    void setFirstStep(int firstStep) {
        _firstStep = clamp(firstStep, 0, CONFIG_STEP_COUNT - 1);
    }

    // lastStep
    int lastStep() const { return _lastStep; }
    void setLastStep(int lastStep) {
        _lastStep = clamp(lastStep, 0, CONFIG_STEP_COUNT - 1);
    }

    // divisor
    int divisor() const { return _divisor; }
    void setDivisor(int divisor) {
        _divisor = clamp(divisor, 1, 255);
    }
    void editDivisor(int value, bool shift) {
        setDivisor(ModelUtils::adjustedByStep(divisor(), value, 1, !shift));
    }
    void printDivisor(StringBuilder &str) const {
        str("%d", divisor());
    }

    // resetMeasure
    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) {
        _resetMeasure = clamp(resetMeasure, 0, 127);
    }

    const StepArray &steps() const { return _steps; }
          StepArray &steps()       { return _steps; }

    const Step &step(int index) const { return _steps[index]; }
          Step &step(int index)       { return _steps[index]; }

    void clear() {
        setScale(-1);
        setRootNote(-1);
        setPlayMode(Types::PlayMode::Aligned);
        setRunMode(Types::RunMode::Forward);
        setFirstStep(0);
        setLastStep(CONFIG_STEP_COUNT - 1);
        setDivisor(12);
        setResetMeasure(0);

        for (auto &step : _steps) {
            step.clear();
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_scale);
        writer.write(_rootNote);
        writer.write(static_cast<uint8_t>(_playMode));
        writer.write(static_cast<uint8_t>(_runMode));
        writer.write(_firstStep);
        writer.write(_lastStep);
        writer.write(_divisor);
        writer.write(_resetMeasure);

        for (const auto &step : _steps) {
            step.write(writer);
        }
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_scale);
        reader.read(_rootNote);
        uint8_t playMode, runMode;
        reader.read(playMode);
        reader.read(runMode);
        _playMode = static_cast<Types::PlayMode>(playMode);
        _runMode = static_cast<Types::RunMode>(runMode);
        reader.read(_firstStep);
        reader.read(_lastStep);
        reader.read(_divisor);
        reader.read(_resetMeasure);

        for (auto &step : _steps) {
            step.read(reader);
        }
    }

private:
    int8_t _trackIndex = -1;
    int8_t _scale = -1;
    int8_t _rootNote = -1;
    Types::PlayMode _playMode = Types::PlayMode::Aligned;
    Types::RunMode _runMode = Types::RunMode::Forward;
    uint8_t _firstStep = 0;
    uint8_t _lastStep = CONFIG_STEP_COUNT - 1;
    uint8_t _divisor = 12;
    uint8_t _resetMeasure = 0;
    StepArray _steps;

    friend class StochasticTrack;
};
