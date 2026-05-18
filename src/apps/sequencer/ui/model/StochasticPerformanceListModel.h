#pragma once

#include "Config.h"
#include "RoutableListModel.h"
#include "model/StochasticTrack.h"
#include "model/Project.h"

class StochasticPerformanceListModel : public RoutableListModel {
public:
    enum Item {
        RhythmMode,
        MelodyMode,
        Complexity,
        Contour,
        Linearity,
        Shape,
        Spread,
        Bias,
        Rate,
        Variation,
        Rest,
        SlideProb,
        Legato,
        Burst,
        BurstCount,
        BurstRate,
        BurstPitch,
        Density,
        Tilt,
        Sleep,
        Patience,
        Mutate,
        Jump,
        Size,
        First,
        Last,
        Rotate,
        Lock,
        Range,
        MinDegree,
        MaxDegree,
        LastItem
    };

    StochasticPerformanceListModel() {}

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
            case RhythmMode:    str("Rhythm"); break;
            case MelodyMode:    str("Melody"); break;
            case Complexity:    str("Complexity"); break;
            case Contour:       str("Contour"); break;
            case Linearity:     str("Linearity"); break;
            case Shape:         str("Shape"); break;
            case Spread:        str("Spread"); break;
            case Bias:          str("Bias"); break;
            case Rate:          str("Rate"); break;
            case Variation:     str("Variation"); break;
            case Rest:          str("Rest"); break;
            case SlideProb:     str("Slide Prob"); break;
            case Legato:        str("Legato"); break;
            case Burst:         str("Burst"); break;
            case BurstCount:    str("Burst Count"); break;
            case BurstRate:     str("Burst Rate"); break;
            case BurstPitch:    str("Burst Pitch"); break;
            case Density:       str("Density"); break;
            case Tilt:          str("Tilt"); break;
            case Sleep:         str("Sleep"); break;
            case Patience:      str("Patience"); break;
            case Mutate:        str("Mutate"); break;
            case Jump:          str("Jump"); break;
            case Size:          str("Size"); break;
            case First:         str("First"); break;
            case Last:          str("Last"); break;
            case Rotate:        str("Rotate"); break;
            case Lock:          str("Lock"); break;
            case Range:         str("Range"); break;
            case MinDegree:     str("Low Degree"); break;
            case MaxDegree:     str("High Degree"); break;
            case LastItem:      break;
            }
        } else if (column == 1) {
            switch (Item(row)) {
            case RhythmMode:    str(_track->rhythmMode() == StochasticSourceMode::Loop ? "Loop" : "Live"); break;
            case MelodyMode:    str(_track->melodyMode() == StochasticSourceMode::Loop ? "Loop" : "Live"); break;
            case Complexity:    str("%d%%", _track->complexity()); break;
            case Contour:       str("%+d%%", _track->contour()); break;
            case Linearity:     _track->printLinearity(str); break;
            case Shape:         _track->printMarblesMode(str); break;
            case Spread:        _track->printMarblesSpread(str); break;
            case Bias:          _track->printMarblesBias(str); break;
            case Rate:          str("%d%%", _track->rate()); break;
            case Variation:     str("%+d%%", _track->variation()); break;
            case Rest:          str("%d%%", _track->rest()); break;
            case SlideProb:     str("%d%%", _track->slide()); break;
            case Legato:        _track->printLegatoProb(str); break;
            case Burst:         _track->printBurst(str); break;
            case BurstCount:    str("%d", _track->burstCount()); break;
            case BurstRate:     str("%d%%", _track->burstRate()); break;
            case BurstPitch:    str(_track->burstPitch() == StochasticBurstPitch::Parent ? "Parent" : "Gen"); break;
            case Density:       _track->printDensity(str); break;
            case Tilt:          _track->printTilt(str); break;
            case Sleep:         str("%d%%", _track->sleep()); break;
            case Patience:      str("%d%%", _track->patience()); break;
            case Mutate:        str("%d%%", _track->mutate()); break;
            case Jump:          str("%d%%", _track->jump()); break;
            case Size:          str("%d", sequence.size()); break;
            case First:         str("%d", sequence.first() + 1); break;
            case Last:          str("%d", sequence.last() + 1); break;
            case Rotate:        _track->printRotate(str); break;
            case Lock:          _track->printLock(str); break;
            case Range:         str("%d Oct", _track->range()); break;
            case MinDegree:     _track->printMinDegree(str); break;
            case MaxDegree:     _track->printMaxDegree(str); break;
            case LastItem:      break;
            }
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            auto &sequence = _track->sequence(_project->selectedPatternIndex());
            switch (Item(row)) {
            case RhythmMode:    _track->setRhythmMode(ModelUtils::adjustedEnum(_track->rhythmMode(), value)); break;
            case MelodyMode:    _track->setMelodyMode(ModelUtils::adjustedEnum(_track->melodyMode(), value)); break;
            case Complexity:    _track->setComplexity(_track->complexity() + value); break;
            case Contour:       _track->setContour(_track->contour() + value); break;
            case Linearity:     _track->editLinearity(value, shift); break;
            case Shape:         _track->editMarblesMode(value, shift); break;
            case Spread:        _track->editMarblesSpread(value, shift); break;
            case Bias:          _track->editMarblesBias(value, shift); break;
            case Rate:          _track->setRate(_track->rate() + value); break;
            case Variation:     _track->setVariation(_track->variation() + value); break;
            case Rest:          _track->setRest(_track->rest() + value); break;
            case SlideProb:     _track->setSlide(_track->slide() + value); break;
            case Legato:        _track->editLegatoProb(value, shift); break;
            case Burst:         _track->editBurst(value, shift); break;
            case BurstCount:    _track->setBurstCount(_track->burstCount() + value); break;
            case BurstRate:     _track->setBurstRate(_track->burstRate() + value); break;
            case BurstPitch:    _track->setBurstPitch(ModelUtils::adjustedEnum(_track->burstPitch(), value)); break;
            case Density:       _track->editDensity(value, shift); break;
            case Tilt:          _track->editTilt(value, shift); break;
            case Sleep:         _track->setSleep(_track->sleep() + value); break;
            case Patience:      _track->setPatience(_track->patience() + value); break;
            case Mutate:        _track->setMutate(_track->mutate() + value); break;
            case Jump:          _track->setJump(_track->jump() + value); break;
            case Size:          sequence.setSize(sequence.size() + value); break;
            case First:         sequence.setFirst(sequence.first() + value); break;
            case Last:          sequence.setLast(sequence.last() + value); break;
            case Rotate:        _track->editRotate(value, shift); break;
            case Lock:          _track->editLock(value, shift); break;
            case Range:         _track->setRange(_track->range() + value); break;
            case MinDegree:     _track->editMinDegree(value, shift); break;
            case MaxDegree:     _track->editMaxDegree(value, shift); break;
            case LastItem:      break;
            }
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case Density:       return Routing::Target::StochasticDensity;
        case Tilt:          return Routing::Target::StochasticTilt;
        case Burst:         return Routing::Target::StochasticBurst;
        case Contour:       return Routing::Target::StochasticContour;
        case Rate:          return Routing::Target::StochasticRate;
        case Variation:     return Routing::Target::StochasticVariation;
        case Rest:          return Routing::Target::StochasticRest;
        case SlideProb:     return Routing::Target::StochasticSlide;
        case Sleep:         return Routing::Target::StochasticSleep;
        case Patience:      return Routing::Target::StochasticPatience;
        case Mutate:        return Routing::Target::StochasticMutate;
        case Jump:          return Routing::Target::StochasticJump;
        case Rotate:        return Routing::Target::Rotate;
        default:            return Routing::Target::None;
        }
    }

private:
    StochasticTrack *_track = nullptr;
    Project *_project = nullptr;
};
