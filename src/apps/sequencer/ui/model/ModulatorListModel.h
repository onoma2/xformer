#pragma once

#include "ListModel.h"
#include "Config.h"
#include "model/Modulator.h"
#include "model/Project.h"

class ModulatorListModel : public ListModel {
public:
    enum Item {
        Shape,
        Mode,
        Rate,
        Depth,
        Offset,
        Phase,
        Smooth,
        RandomMode,
        GateTrack,
        CvOutput,
        Last
    };

    ModulatorListModel() : _project(nullptr), _modulatorIndex(0) {}

    void setProject(Project *project) {
        _project = project;
    }

    void setModulatorIndex(int index) {
        _modulatorIndex = clamp(index, 0, CONFIG_MODULATOR_COUNT - 1);
    }

    int modulatorIndex() const { return _modulatorIndex; }

    Modulator &modulator() {
        return _project->modulator(_modulatorIndex);
    }

    const Modulator &modulator() const {
        return _project->modulator(_modulatorIndex);
    }

    virtual int rows() const override {
        return _project ? Last : 0;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        if (!_project) return;

        if (column == 0) {
            formatName(Item(row), str);
        } else if (column == 1) {
            formatValue(Item(row), str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (!_project || column != 1) return;

        int count = indexedCount(row);
        if (count > 0) {
            int current = indexed(row);
            if (current >= 0) {
                int next = (current + value) % count;
                if (next < 0) next += count;
                setIndexed(row, next);
            }
        } else {
            editValue(Item(row), value, shift);
        }
    }

    virtual int indexedCount(int row) const override {
        if (!_project) return 0;

        switch (Item(row)) {
        case Shape:      return int(Modulator::Shape::Last);
        case Mode:       return int(Modulator::Mode::Last);
        case RandomMode: return int(Modulator::RandomMode::Last);
        default:         return 0;
        }
    }

    virtual int indexed(int row) const override {
        if (!_project) return -1;

        switch (Item(row)) {
        case Shape:      return static_cast<int>(modulator().shape());
        case Mode:       return static_cast<int>(modulator().mode());
        case RandomMode: return static_cast<int>(modulator().randomMode());
        default:         return -1;
        }
    }

    virtual void setIndexed(int row, int index) override {
        if (!_project || index < 0) return;
        if (index >= indexedCount(row)) return;

        switch (Item(row)) {
        case Shape:      modulator().setShape(static_cast<Modulator::Shape>(index)); break;
        case Mode:       modulator().setMode(static_cast<Modulator::Mode>(index)); break;
        case RandomMode: modulator().setRandomMode(static_cast<Modulator::RandomMode>(index)); break;
        default: break;
        }
    }

private:
    static const char *itemName(Item item) {
        switch (item) {
        case Shape:      return "SHAPE";
        case Mode:       return "MODE";
        case Rate:       return "RATE";
        case Depth:      return "DEPTH";
        case Offset:     return "OFFSET";
        case Phase:      return "PHASE";
        case Smooth:     return "SMOOTH";
        case RandomMode: return "R.MODE";
        case GateTrack:  return "G.TRACK";
        case CvOutput:   return "CV OUT";
        case Last:       break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        if (!_project) return;

        switch (item) {
        case Shape:      modulator().printShape(str); break;
        case Mode:       modulator().printMode(str); break;
        case Rate:       modulator().printRate(str); break;
        case Depth:      modulator().printDepth(str); break;
        case Offset:     modulator().printOffset(str); break;
        case Phase:      modulator().printPhase(str); break;
        case Smooth:     modulator().printSmooth(str); break;
        case RandomMode: modulator().printRandomMode(str); break;
        case GateTrack:  modulator().printGateTrack(str); break;
        case CvOutput:   formatCvOutput(str); break;
        case Last:       break;
        }
    }

    void formatCvOutput(StringBuilder &str) const {
        // Show which CV outputs this modulator drives
        bool first = true;
        for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
            if (_project->cvOutputModulator(i) == _modulatorIndex + 1) {
                if (!first) str(",");
                str("CV%d", i + 1);
                first = false;
            }
        }
        if (first) {
            str("--");
        }
    }

    void editValue(Item item, int value, bool shift) {
        if (!_project) return;

        switch (item) {
        case Rate:       modulator().editRate(value, shift); break;
        case Depth:      modulator().editDepth(value, shift); break;
        case Offset:     modulator().editOffset(value, shift); break;
        case Phase:      modulator().editPhase(value, shift); break;
        case Smooth:     modulator().editSmooth(value, shift); break;
        case GateTrack:  modulator().editGateTrack(value, shift); break;
        case CvOutput:
            // Cycle through CV output assignments for channel 1-8
            // Encoder increments through CV channels, assigning/unassigning modulator
            editCvOutput(value);
            break;
        default: break;
        }
    }

    void editCvOutput(int value) {
        // Find modulator's current first/last CV output assignment and cycle
        // Simple approach: rotate the assignment on the next CV channel
        static int lastChannel = 0;
        int ch = (lastChannel + value + CONFIG_CHANNEL_COUNT) % CONFIG_CHANNEL_COUNT;
        lastChannel = ch;
        int current = _project->cvOutputModulator(ch);
        if (current == _modulatorIndex + 1) {
            _project->setCvOutputModulator(ch, 0); // unassign
        } else {
            _project->setCvOutputModulator(ch, _modulatorIndex + 1);
        }
    }

    int clamp(int value, int min, int max) const {
        if (value < min) return min;
        if (value > max) return max;
        return value;
    }

    Project *_project;
    int _modulatorIndex;
};
