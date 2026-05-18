#pragma once

#include "Config.h"
#include "RoutableListModel.h"
#include "model/StochasticTrack.h"
#include "model/Project.h"

class StochasticConfigListModel : public RoutableListModel {
public:
    enum Item {
        Divisor,
        ResetMeasure,
        Scale,
        RootNote,
        Octave,
        Transpose,
        CvUpdateMode,
        SlideTime,
        LastItem
    };

    StochasticConfigListModel() {}

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
            case Divisor:       str("Clock/Div"); break;
            case ResetMeasure:  str("Reset Measure"); break;
            case Scale:         str("Scale"); break;
            case RootNote:      str("Root Note"); break;
            case Octave:        str("Octave"); break;
            case Transpose:     str("Transpose"); break;
            case CvUpdateMode:  str("CV Update"); break;
            case SlideTime:     str("Slide Time"); break;
            case LastItem:      break;
            }
        } else if (column == 1) {
            switch (Item(row)) {
            case Divisor:       sequence.printDivisor(str); break;
            case ResetMeasure:  sequence.printResetMeasure(str); break;
            case Scale:         sequence.printScale(str); break;
            case RootNote:      sequence.printRootNote(str); break;
            case Octave:        _track->printOctave(str); break;
            case Transpose:     _track->printTranspose(str); break;
            case CvUpdateMode:  _track->printCvUpdateMode(str); break;
            case SlideTime:     _track->printSlideTime(str); break;
            case LastItem:      break;
            }
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            auto &sequence = _track->sequence(_project->selectedPatternIndex());
            switch (Item(row)) {
            case Divisor:       sequence.editDivisor(value, shift); break;
            case ResetMeasure:  sequence.editResetMeasure(value, shift); break;
            case Scale:         sequence.editScale(value, shift); break;
            case RootNote:      sequence.editRootNote(value, shift); break;
            case Octave:        _track->editOctave(value, shift); break;
            case Transpose:     _track->editTranspose(value, shift); break;
            case CvUpdateMode:  _track->editCvUpdateMode(value, shift); break;
            case SlideTime:     _track->editSlideTime(value, shift); break;
            case LastItem:      break;
            }
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case Octave:        return Routing::Target::Octave;
        case Transpose:     return Routing::Target::Transpose;
        case SlideTime:     return Routing::Target::SlideTime;
        default:            return Routing::Target::None;
        }
    }

private:
    StochasticTrack *_track = nullptr;
    Project *_project = nullptr;
};
