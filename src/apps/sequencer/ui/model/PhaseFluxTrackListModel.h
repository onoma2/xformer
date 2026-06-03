#pragma once

#include "Config.h"
#include "RoutableListModel.h"
#include "model/PhaseFluxTrack.h"

class PhaseFluxTrackListModel : public RoutableListModel {
public:
    void setTrack(PhaseFluxTrack *track) { _track = track; }

    virtual int rows() const override { return _track ? Last : 0; }
    virtual int columns() const override { return 2; }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (!_track) return;
        if (column == 0) formatName(Item(row), str);
        else if (column == 1) formatValue(Item(row), str);
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (!_track) return;
        if (column == 1) editValue(Item(row), value, shift);
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case SlideTime: return Routing::Target::SlideTime;
        case Octave:    return Routing::Target::Octave;
        case Transpose: return Routing::Target::Transpose;
        default:        return Routing::Target::None;
        }
    }

private:
    enum Item {
        PlayMode,
        FillMode,
        CvUpdateMode,
        SlideTime,
        Octave,
        Transpose,
        Last
    };

    static const char *itemName(Item item) {
        switch (item) {
        case PlayMode:     return "Play Mode";
        case FillMode:     return "Fill Mode";
        case CvUpdateMode: return "CV Update";
        case SlideTime:    return "Slide Time";
        case Octave:       return "Octave";
        case Transpose:    return "Transpose";
        case Last:         break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const { str(itemName(item)); }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case PlayMode:     _track->printPlayMode(str); break;
        case FillMode:     str(PhaseFluxTrack::fillModeName(_track->fillMode())); break;
        case CvUpdateMode: _track->printCvUpdateMode(str); break;
        case SlideTime:    str("%d", _track->slideTime()); break;
        case Octave:       str("%+d", _track->octave()); break;
        case Transpose:    str("%+d", _track->transpose()); break;
        case Last:         break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case PlayMode:     _track->editPlayMode(value, shift); break;
        case FillMode:     _track->setFillMode(ModelUtils::adjustedEnum(_track->fillMode(), value)); break;
        case CvUpdateMode: _track->editCvUpdateMode(value, shift); break;
        case SlideTime:    if (!_track->routeOverridden(ParamKey::SlideTime)) _track->setSlideTime(ModelUtils::adjusted(_track->slideTime(), value, 0, 100)); break;
        case Octave:       if (!_track->routeOverridden(ParamKey::Octave)) _track->setOctave(ModelUtils::adjusted(_track->octave(), value, -10, 10)); break;
        case Transpose:    if (!_track->routeOverridden(ParamKey::Transpose)) _track->setTranspose(ModelUtils::adjusted(_track->transpose(), value, -100, 100)); break;
        case Last:         break;
        }
    }

    PhaseFluxTrack *_track = nullptr;
};
