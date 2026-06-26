#pragma once

#include "Config.h"
#include "RoutableListModel.h"
#include "model/FractalTrack.h"
#include "model/Project.h"
#include "model/Scale.h"
#include "model/ModelUtils.h"

class FractalTrackListModel : public RoutableListModel {
public:
    FractalTrackListModel() {}

    void setTrack(FractalTrack &track, Project &project, Engine *engine = nullptr) {
        _track = &track;
        _project = &project;
        _engine = engine;
    }

    virtual int rows() const override {
        return _track ? Last : 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        auto &sequence = _track->sequence(_project->selectedPatternIndex());
        if (column == 0) {
            formatName(Item(row), str);
        } else if (column == 1) {
            formatValue(Item(row), sequence, str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            auto &sequence = _track->sequence(_project->selectedPatternIndex());
            editValue(Item(row), sequence, value, shift);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case SlideTime:     return Routing::Target::SlideTime;
        case Octave:        return Routing::Target::Octave;
        case Transpose:     return Routing::Target::Transpose;
        case Divisor:       return Routing::Target::Divisor;
        case RunMode:       return Routing::Target::RunMode;
        default:            return Routing::Target::None;
        }
    }

private:
    enum Item {
        // track-wide
        SourceA,
        BufferLength,
        Octave,
        Transpose,
        SlideTime,
        CvUpdateMode,
        Scale,
        RootNote,
        ScaleRotate,
        Lock,
        // per-sequence (active pattern)
        Divisor,
        ClockMultiplier,
        ResetMeasure,
        RunMode,
        LoopFirst,
        LoopLast,
        RecordFirst,
        RecordLast,
        RecordMode,
        Record,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case SourceA:           return "Source A";
        case BufferLength:      return "Buffer Length";
        case Octave:            return "Octave";
        case Transpose:         return "Transpose";
        case SlideTime:         return "Slide Time";
        case CvUpdateMode:      return "CV Update Mode";
        case Scale:             return "Scale";
        case RootNote:          return "Root Note";
        case ScaleRotate:       return "Scale Rotate";
        case Lock:              return "Lock";
        case Divisor:           return "Divisor";
        case ClockMultiplier:   return "Clock Mult";
        case ResetMeasure:      return "Reset Measure";
        case RunMode:           return "Run Mode";
        case LoopFirst:         return "Loop First";
        case LoopLast:          return "Loop Last";
        case RecordFirst:       return "Record First";
        case RecordLast:        return "Record Last";
        case RecordMode:        return "Record Mode";
        case Record:            return "Record";
        case Last:              break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, const FractalSequence &sequence, StringBuilder &str) const {
        switch (item) {
        case SourceA:
            if (_track->sourceA() < 0) {
                str("None");
            } else {
                str("Track %d", _track->sourceA() + 1);
            }
            break;
        case BufferLength:
            str("%d", _track->bufferLength());
            break;
        case Octave:
            str("%+d", _track->octave());
            break;
        case Transpose:
            str("%+d", _track->transpose());
            break;
        case SlideTime:
            str("%d%%", _track->slideTime());
            break;
        case CvUpdateMode:
            str(FractalTrack::cvUpdateModeName(_track->cvUpdateMode()));
            break;
        case Scale:
            str(_track->scale() < 0 ? "Default" : Scale::name(_track->scale()));
            break;
        case RootNote:
            if (_track->rootNote() < 0) {
                str("Default");
            } else {
                Types::printNote(str, _track->rootNote());
            }
            break;
        case ScaleRotate:
            if (_track->scaleRotate() < 0) {
                str("Default");
            } else {
                str("%d", _track->scaleRotate());
            }
            break;
        case Lock:
            ModelUtils::printYesNo(str, _track->lock());
            break;
        case Divisor:
            sequence.printDivisor(str);
            break;
        case ClockMultiplier:
            str("%.2fx", sequence.clockMultiplier() * 0.01f);
            break;
        case ResetMeasure:
            if (sequence.resetMeasure() == 0) {
                str("off");
            } else {
                str("%d %s", sequence.resetMeasure(), sequence.resetMeasure() > 1 ? "bars" : "bar");
            }
            break;
        case RunMode:
            str(Types::runModeName(sequence.runMode()));
            break;
        case LoopFirst:
            str("%d", sequence.loopFirst() + 1);
            break;
        case LoopLast:
            str("%d", sequence.loopLast() + 1);
            break;
        case RecordFirst:
            str("%d", sequence.recordFirst() + 1);
            break;
        case RecordLast:
            str("%d", sequence.recordLast() + 1);
            break;
        case RecordMode:
            str(sequence.recordMode() == 1 ? "Latch" : "Replace");
            break;
        case Record:
            str(sequence.recordTrigger() ? "On" : "Off");
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, FractalSequence &sequence, int value, bool shift) {
        switch (item) {
        case SourceA:
            _track->setSourceA(_track->sourceA() + value);
            break;
        case BufferLength:
            _track->setBufferLength(_track->bufferLength() + value);
            break;
        case Octave:
            _track->setOctave(_track->octave() + value);
            break;
        case Transpose:
            _track->setTranspose(_track->transpose() + value);
            break;
        case SlideTime:
            _track->setSlideTime(ModelUtils::adjustedByStep(_track->slideTime(), value, 5, !shift));
            break;
        case CvUpdateMode:
            _track->setCvUpdateMode(ModelUtils::adjustedEnum(_track->cvUpdateMode(), value));
            break;
        case Scale:
            _track->setScale(_track->scale() + value);
            break;
        case RootNote:
            _track->setRootNote(_track->rootNote() + value);
            break;
        case ScaleRotate:
            _track->setScaleRotate(_track->scaleRotate() + value);
            break;
        case Lock:
            _track->setLock(value > 0);
            break;
        case Divisor:
            sequence.editDivisor(value, shift);
            break;
        case ClockMultiplier:
            sequence.setClockMultiplier(sequence.clockMultiplier() + value * (shift ? 10 : 1));
            break;
        case ResetMeasure:
            sequence.setResetMeasure(ModelUtils::adjustedByPowerOfTwo(sequence.resetMeasure(), value, shift));
            break;
        case RunMode:
            sequence.setRunMode(ModelUtils::adjustedEnum(sequence.runMode(), value));
            break;
        case LoopFirst:
            sequence.setLoopFirst(sequence.loopFirst() + value);
            break;
        case LoopLast:
            sequence.setLoopLast(sequence.loopLast() + value);
            break;
        case RecordFirst:
            sequence.setRecordFirst(sequence.recordFirst() + value);
            break;
        case RecordLast:
            sequence.setRecordLast(sequence.recordLast() + value);
            break;
        case RecordMode:
            sequence.setRecordMode(clamp(sequence.recordMode() + value, 0, 1));
            break;
        case Record:
            sequence.setRecordTrigger(value > 0 ? 1 : 0);
            break;
        case Last:
            break;
        }
    }

    FractalTrack *_track = nullptr;
    Project *_project = nullptr;
    Engine *_engine = nullptr;
};
