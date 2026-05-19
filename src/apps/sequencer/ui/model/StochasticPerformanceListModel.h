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
            case RhythmMode:    str(sequence.rhythmMode() == StochasticSourceMode::Loop ? "Loop" : "Live"); break;
            case MelodyMode:    str(sequence.melodyMode() == StochasticSourceMode::Loop ? "Loop" : "Live"); break;
            case Complexity:    sequence.printComplexity(str); break;
            case Contour:       sequence.printContour(str); break;
            case Linearity:     sequence.printLinearity(str); break;
            case Shape:         sequence.printMarblesMode(str); break;
            case Spread:        sequence.printMarblesSpread(str); break;
            case Bias:          sequence.printMarblesBias(str); break;
            case Rate:          sequence.printRate(str); break;
            case Variation:     sequence.printVariation(str); break;
            case Rest:          sequence.printRest(str); break;
            case SlideProb:     sequence.printSlide(str); break;
            case Legato:        sequence.printLegatoProb(str); break;
            case Burst:         sequence.printBurst(str); break;
            case BurstCount:    str("%d", sequence.burstCount()); break;
            case BurstRate:     str("%d%%", sequence.burstRate()); break;
            case BurstPitch:    sequence.printBurstPitch(str); break;
            case Density:       sequence.printDensity(str); break;
            case Tilt:          sequence.printTilt(str); break;
            case Sleep:         sequence.printSleep(str); break;
            case Patience:      sequence.printPatience(str); break;
            case Mutate:        sequence.printMutate(str); break;
            case Jump:          sequence.printJump(str); break;
            case Size:          str("%d", sequence.size()); break;
            case First:         str("%d", sequence.first() + 1); break;
            case Last:          str("%d", sequence.last() + 1); break;
            case Rotate:        sequence.printRotate(str); break;
            case Lock:          _track->printLock(str); break;
            case Range:         str("%d Oct", sequence.range()); break;
            case MinDegree:     sequence.printMinDegree(str); break;
            case MaxDegree:     sequence.printMaxDegree(str); break;
            case LastItem:      break;
            }
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            auto &sequence = _track->sequence(_project->selectedPatternIndex());
            switch (Item(row)) {
            case RhythmMode:    sequence.setRhythmMode(ModelUtils::adjustedEnum(sequence.rhythmMode(), value)); break;
            case MelodyMode:    sequence.setMelodyMode(ModelUtils::adjustedEnum(sequence.melodyMode(), value)); break;
            case Complexity:    sequence.editComplexity(value, shift); break;
            case Contour:       sequence.editContour(value, shift); break;
            case Linearity:     sequence.editLinearity(value, shift); break;
            case Shape:         sequence.editMarblesMode(value, shift); break;
            case Spread:        sequence.editMarblesSpread(value, shift); break;
            case Bias:          sequence.editMarblesBias(value, shift); break;
            case Rate:          sequence.editRate(value, shift); break;
            case Variation:     sequence.editVariation(value, shift); break;
            case Rest:          sequence.editRest(value, shift); break;
            case SlideProb:     sequence.editSlide(value, shift); break;
            case Legato:        sequence.editLegatoProb(value, shift); break;
            case Burst:         sequence.editBurst(value, shift); break;
            case BurstCount:    sequence.setBurstCount(sequence.burstCount() + value); break;
            case BurstRate:     sequence.setBurstRate(sequence.burstRate() + value); break;
            case BurstPitch:    sequence.editBurstPitch(value, shift); break;
            case Density:       sequence.editDensity(value, shift); break;
            case Tilt:          sequence.editTilt(value, shift); break;
            case Sleep:         sequence.editSleep(value, shift); break;
            case Patience:      sequence.editPatience(value, shift); break;
            case Mutate:        sequence.editMutate(value, shift); break;
            case Jump:          sequence.editJump(value, shift); break;
            case Size:          sequence.setSize(sequence.size() + value); break;
            case First:         sequence.setFirst(sequence.first() + value); break;
            case Last:          sequence.setLast(sequence.last() + value); break;
            case Rotate:        sequence.editRotate(value, shift); break;
            case Lock:          _track->editLock(value, shift); break;
            case Range:         sequence.setRange(sequence.range() + value); break;
            case MinDegree:     sequence.editMinDegree(value, shift); break;
            case MaxDegree:     sequence.editMaxDegree(value, shift); break;
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
        case Complexity:    return Routing::Target::StochasticComplexity;
        default:            return Routing::Target::None;
        }
    }

private:
    StochasticTrack *_track = nullptr;
    Project *_project = nullptr;
};
