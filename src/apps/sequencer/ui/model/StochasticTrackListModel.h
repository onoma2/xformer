#pragma once

#include "Config.h"
#include "RoutableListModel.h"
#include "model/StochasticTrack.h"
#include "model/Project.h"

class StochasticTrackListModel : public RoutableListModel {
public:
    enum Item {
        Mode,
        Complexity,
        Contour,
        Rate,
        Variation,
        Rest,
        Slide,
        Burst,
        BurstPitch,
        Density,
        Tilt,
        Jitter,
        Rotate,
        Lock,
        Size,
        First,
        Last,
        Range,
        Divisor,
        ResetMeasure,
        Sleep,
        Patience,
        Mutate,
        Jump,
        Scale,
        RootNote,
        CvUpdateMode,
        LastItem
    };

    StochasticTrackListModel() {}

    void setTrack(StochasticTrack &track, Project &project) {
        _track = &track;
        _project = &project;
    }

    virtual int rows() const override {
        return LastItem;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        auto &sequence = _track->sequence(_project->selectedPatternIndex());
        if (column == 0) {
            switch (Item(row)) {
            case Mode:          str("Mode"); break;
            case Complexity:    str("Complexity"); break;
            case Contour:       str("Contour"); break;
            case Rate:          str("Rate"); break;
            case Variation:     str("Variation"); break;
            case Rest:          str("Rest"); break;
            case Slide:         str("Slide"); break;
            case Burst:         str("Burst"); break;
            case BurstPitch:    str("Burst Pitch"); break;
            case Density:       str("Density"); break;
            case Tilt:          str("Tilt"); break;
            case Jitter:        str("Jitter"); break;
            case Rotate:        str("Rotate"); break;
            case Lock:          str("Lock"); break;
            case Size:          str("Size"); break;
            case First:         str("First"); break;
            case Last:          str("Last"); break;
            case Range:         str("Range"); break;
            case Divisor:       str("Clock/Div"); break;
            case ResetMeasure:  str("Reset Measure"); break;
            case Sleep:         str("Sleep"); break;
            case Patience:      str("Patience"); break;
            case Mutate:        str("Mutate"); break;
            case Jump:          str("Jump"); break;
            case Scale:         str("Scale"); break;
            case RootNote:      str("Root Note"); break;
            case CvUpdateMode:  str("CV Update"); break;
            case LastItem:      break;
            }
        } else if (column == 1) {
            switch (Item(row)) {
            case Mode:          str(_track->mode() == StochasticMode::Dice ? "Dice" : "Realtime"); break;
            case Complexity:    str("%d%%", _track->complexity()); break;
            case Contour:       str("%+d%%", _track->contour()); break;
            case Rate:          str("%d%%", _track->rate()); break;
            case Variation:     str("%+d%%", _track->variation()); break;
            case Rest:          str("%d%%", _track->rest()); break;
            case Slide:         str("%d%%", _track->slide()); break;
            case Burst:         _track->printBurst(str); break;
            case BurstPitch:    str(_track->burstPitch() == StochasticBurstPitch::Parent ? "Parent" : "Gen"); break;
            case Density:       _track->printDensity(str); break;
            case Tilt:          _track->printTilt(str); break;
            case Jitter:        _track->printJitter(str); break;
            case Rotate:        _track->printRotate(str); break;
            case Lock:          _track->printLock(str); break;
            case Size:          str("%d", sequence.size()); break;
            case First:         str("%d", sequence.first() + 1); break;
            case Last:          str("%d", sequence.last() + 1); break;
            case Range:         str("%d Oct", _track->range()); break;
            case Divisor:       sequence.printDivisor(str); break;
            case ResetMeasure:  sequence.printResetMeasure(str); break;
            case Sleep:         str("%d%%", _track->sleep()); break;
            case Patience:      str("%d%%", _track->patience()); break;
            case Mutate:        str("%d%%", _track->mutate()); break;
            case Jump:          str("%d%%", _track->jump()); break;
            case Scale:         sequence.printScale(str); break;
            case RootNote:      sequence.printRootNote(str); break;
            case CvUpdateMode:  _track->printCvUpdateMode(str); break;
            case LastItem:      break;
            }
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            auto &sequence = _track->sequence(_project->selectedPatternIndex());
            switch (Item(row)) {
            case Mode:          _track->setMode(ModelUtils::adjustedEnum(_track->mode(), value)); break;
            case Complexity:    _track->setComplexity(_track->complexity() + value); break;
            case Contour:       _track->setContour(_track->contour() + value); break;
            case Rate:          _track->setRate(_track->rate() + value); break;
            case Variation:     _track->setVariation(_track->variation() + value); break;
            case Rest:          _track->setRest(_track->rest() + value); break;
            case Slide:         _track->setSlide(_track->slide() + value); break;
            case Burst:         _track->editBurst(value, shift); break;
            case BurstPitch:    _track->setBurstPitch(ModelUtils::adjustedEnum(_track->burstPitch(), value)); break;
            case Density:       _track->editDensity(value, shift); break;
            case Tilt:          _track->editTilt(value, shift); break;
            case Jitter:        _track->editJitter(value, shift); break;
            case Rotate:        _track->editRotate(value, shift); break;
            case Lock:          _track->editLock(value, shift); break;
            case Size:          sequence.setSize(sequence.size() + value); break;
            case First:         sequence.setFirst(sequence.first() + value); break;
            case Last:          sequence.setLast(sequence.last() + value); break;
            case Range:         _track->setRange(_track->range() + value); break;
            case Divisor:       sequence.editDivisor(value, shift); break;
            case ResetMeasure:  sequence.editResetMeasure(value, shift); break;
            case Sleep:         _track->setSleep(_track->sleep() + value); break;
            case Patience:      _track->setPatience(_track->patience() + value); break;
            case Mutate:        _track->setMutate(_track->mutate() + value); break;
            case Jump:          _track->setJump(_track->jump() + value); break;
            case Scale:         sequence.editScale(value, shift); break;
            case RootNote:      sequence.editRootNote(value, shift); break;
            case CvUpdateMode:  _track->editCvUpdateMode(value, shift); break;
            case LastItem:      break;
            }
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case Density:       return Routing::Target::StochasticDensity;
        case Tilt:          return Routing::Target::StochasticTilt;
        case Jitter:        return Routing::Target::StochasticJitter;
        case Burst:         return Routing::Target::StochasticBurst;
        case Complexity:    return Routing::Target::StochasticComplexity;
        case Contour:       return Routing::Target::StochasticContour;
        case Rate:          return Routing::Target::StochasticRate;
        case Variation:     return Routing::Target::StochasticVariation;
        case Rest:          return Routing::Target::StochasticRest;
        case Slide:         return Routing::Target::StochasticSlide;
        case Sleep:         return Routing::Target::StochasticSleep;
        case Patience:      return Routing::Target::StochasticPatience;
        case Mutate:        return Routing::Target::StochasticMutate;
        case Jump:          return Routing::Target::StochasticJump;
        case Rotate:        return Routing::Target::Rotate;
        case Divisor:       return Routing::Target::Divisor;
        case Scale:         return Routing::Target::Scale;
        case RootNote:      return Routing::Target::RootNote;
        default:            return Routing::Target::None;
        }
    }

private:
    StochasticTrack *_track = nullptr;
    Project *_project = nullptr;
};
