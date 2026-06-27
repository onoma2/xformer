#pragma once

#include "RoutableListModel.h"
#include "model/FractalTrack.h"
#include "model/Scale.h"
#include "model/ModelUtils.h"

// Track-scope params only. Per-sequence params (loop/record/orn edges, order,
// rate, intensity, divisor, …) live in FractalSequenceListModel.
class FractalTrackListModel : public RoutableListModel {
public:
    FractalTrackListModel() {}

    void setTrack(FractalTrack &track) {
        _track = &track;
    }

    virtual int rows() const override {
        return _track ? Last : 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            formatName(Item(row), str);
        } else if (column == 1) {
            formatValue(Item(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            editValue(Item(row), value, shift);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        // No track-scope Fractal param is in the RouteResolve table (KD-18 routes
        // branchCount/path/ornamentRate/ornamentIntensity/recordTrigger, all
        // per-sequence). So no inline route indicator here.
        (void)row;
        return Routing::Target::None;
    }

private:
    enum Item {
        SourceA,
        BufferLength,
        Octave,
        Transpose,
        SlideTime,
        Delay,
        CvUpdateMode,
        Quantize,
        Scale,
        RootNote,
        ScaleRotate,
        Lock,
        RecordMuted,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case SourceA:           return "Source A";
        case BufferLength:      return "Buffer Length";
        case Octave:            return "Octave";
        case Transpose:         return "Transpose";
        case SlideTime:         return "Slide Time";
        case Delay:             return "Delay";
        case CvUpdateMode:      return "CV Update Mode";
        case Quantize:          return "Quantize";
        case Scale:             return "Scale";
        case RootNote:          return "Root Note";
        case ScaleRotate:       return "Scale Rotate";
        case Lock:              return "Lock";
        case RecordMuted:       return "Rec Muted";
        case Last:              break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case SourceA:
            _track->printSourceA(str);
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
        case Delay:
            str("%d", _track->trackDelay());
            break;
        case CvUpdateMode:
            str(FractalTrack::cvUpdateModeName(_track->cvUpdateMode()));
            break;
        case Quantize:
            _track->printQuantize(str);
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
        case RecordMuted:
            _track->printRecordMuted(str);
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case SourceA:
            _track->editSourceA(value, shift);
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
        case Delay:
            _track->setTrackDelay(_track->trackDelay() + value);
            break;
        case CvUpdateMode:
            _track->setCvUpdateMode(ModelUtils::adjustedEnum(_track->cvUpdateMode(), value));
            break;
        case Quantize:
            _track->editQuantize(value, shift);
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
        case RecordMuted:
            _track->editRecordMuted(value, shift);
            break;
        case Last:
            break;
        }
    }

    FractalTrack *_track = nullptr;
};
