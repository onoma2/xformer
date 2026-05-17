#pragma once

#include "Config.h"
#include "RoutableListModel.h"
#include "model/StochasticTrack.h"
#include "model/Project.h"

class StochasticTrackListModel : public RoutableListModel {
public:
    enum Item {
        Density,
        Tilt,
        Jitter,
        Burst,
        Rotate,
        Lock,
        LoopFirst,
        LoopLast,
        CvUpdateMode,
        Scale,
        RootNote,
        Divisor,
        RunMode,
        Last
    };

    StochasticTrackListModel() {}

    void setTrack(StochasticTrack &track, Project &project) {
        _track = &track;
        _project = &project;
    }

    virtual int rows() const override {
        return Last;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (column == 0) {
            switch (Item(row)) {
            case Density:       str("Density"); break;
            case Tilt:          str("Tilt"); break;
            case Jitter:        str("Jitter"); break;
            case Burst:         str("Burst"); break;
            case Rotate:        str("Rotate"); break;
            case Lock:          str("Lock"); break;
            case LoopFirst:     str("Loop First"); break;
            case LoopLast:      str("Loop Last"); break;
            case CvUpdateMode:  str("CV Update"); break;
            case Scale:         str("Scale"); break;
            case RootNote:      str("Root Note"); break;
            case Divisor:       str("Divisor"); break;
            case RunMode:       str("Run Mode"); break;
            case Last:          break;
            }
        } else if (column == 1) {
            switch (Item(row)) {
            case Density:       _track->printDensity(str); break;
            case Tilt:          _track->printTilt(str); break;
            case Jitter:        _track->printJitter(str); break;
            case Burst:         _track->printBurst(str); break;
            case Rotate:        _track->printRotate(str); break;
            case Lock:          _track->printLock(str); break;
            case LoopFirst:     _track->printLoopFirst(str); break;
            case LoopLast:      _track->printLoopLast(str); break;
            case CvUpdateMode:  _track->printCvUpdateMode(str); break;
            case Scale:         _track->printScale(str, _project->scale()); break;
            case RootNote:      _track->printRootNote(str, _project->rootNote()); break;
            case Divisor:       _track->printDivisor(str); break;
            case RunMode:       _track->printRunMode(str); break;
            case Last:          break;
            }
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            switch (Item(row)) {
            case Density:       _track->editDensity(value, shift); break;
            case Tilt:          _track->editTilt(value, shift); break;
            case Jitter:        _track->editJitter(value, shift); break;
            case Burst:         _track->editBurst(value, shift); break;
            case Rotate:        _track->editRotate(value, shift); break;
            case Lock:          _track->editLock(value, shift); break;
            case LoopFirst:     _track->editLoopFirst(value, shift); break;
            case LoopLast:      _track->editLoopLast(value, shift); break;
            case CvUpdateMode:  _track->editCvUpdateMode(value, shift); break;
            case Scale:         _track->editScale(value, shift); break;
            case RootNote:      _track->editRootNote(value, shift); break;
            case Divisor:       _track->editDivisor(value, shift); break;
            case RunMode:       _track->editRunMode(value, shift); break;
            case Last:          break;
            }
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case Density:       return Routing::Target::StochasticDensity;
        case Tilt:          return Routing::Target::StochasticTilt;
        case Jitter:        return Routing::Target::StochasticJitter;
        case Burst:         return Routing::Target::StochasticBurst;
        case Rotate:        return Routing::Target::Rotate;
        case Scale:         return Routing::Target::Scale;
        case RootNote:      return Routing::Target::RootNote;
        case Divisor:       return Routing::Target::Divisor;
        default:            return Routing::Target::None;
        }
    }

private:
    StochasticTrack *_track = nullptr;
    Project *_project = nullptr;
};
