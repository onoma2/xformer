#pragma once

#include "Config.h"

#include "ListModel.h"

#include "model/Routing.h"

class RouteListModel : public ListModel {
public:
    enum Item {
        Target,
        Min,
        Max,
        Tracks,
        Source,
        FirstSource,
        CvRange = FirstSource,
        MidiSource = FirstSource,
        MidiEvent,
        FirstMidiEventConfig,
        MidiControlNumber = FirstMidiEventConfig,
        MidiNote = FirstMidiEventConfig,
        MidiNoteRange,
        BusBias,
        BusDepth,
        BusShaper,
        RotateMode,
        Last
    };

    RouteListModel(Routing::Route &route) :
        _route(route)
    {}

    virtual int rows() const override {
        bool isEmpty = _route.target() == Routing::Target::None;
        bool isCvSource = Routing::isCvSource(_route.source());
        bool isMidiSource = Routing::isMidiSource(_route.source());
        bool isBusTarget = Routing::isBusTarget(_route.target());
        bool showRotateMode = _route.target() == Routing::Target::CvOutputRotate;
        bool hasNoteOrController = _route.midiSource().event() != Routing::MidiSource::Event::PitchBend;
        bool hasNoteRange = _route.midiSource().event() == Routing::MidiSource::Event::NoteRange;
        const int baseLast = int(BusShaper) + 1;
        int baseRows = 1;
        if (isEmpty) {
            baseRows = 1;
        } else if (isCvSource) {
            baseRows = FirstSource + 1;
        } else if (isMidiSource) {
            baseRows = hasNoteOrController ? (hasNoteRange ? baseLast : baseLast - 1) : baseLast - 2;
        } else {
            baseRows = FirstSource;
        }
        if (isBusTarget && !isEmpty) {
            baseRows += 3;
        }
        if (showRotateMode && !isEmpty) {
            baseRows += 1;
        }
        return baseRows;
    }

    virtual int columns() const override {
        return 2;
    }

    virtual void cell(int row, int column, StringBuilder &str) const override {
        Item item = itemForRow(row);
        if (column == 0) {
            formatName(item, str);
        } else if (column == 1) {
            formatValue(item, str);
        }
    }

    virtual void edit(int row, int column, int value, bool shift) override {
        if (column == 1) {
            editValue(itemForRow(row), value, shift);
        }
    }

private:
    Item itemForRow(int row) const {
        bool isEmpty = _route.target() == Routing::Target::None;
        bool isCvSource = Routing::isCvSource(_route.source());
        bool isMidiSource = Routing::isMidiSource(_route.source());
        bool isBusTarget = Routing::isBusTarget(_route.target());
        bool showRotateMode = _route.target() == Routing::Target::CvOutputRotate;
        bool hasNoteOrController = _route.midiSource().event() != Routing::MidiSource::Event::PitchBend;
        bool hasNoteRange = _route.midiSource().event() == Routing::MidiSource::Event::NoteRange;
        const int baseLast = int(BusShaper) + 1;
        int baseRows = 1;
        if (isEmpty) {
            baseRows = 1;
        } else if (isCvSource) {
            baseRows = FirstSource + 1;
        } else if (isMidiSource) {
            baseRows = hasNoteOrController ? (hasNoteRange ? baseLast : baseLast - 1) : baseLast - 2;
        } else {
            baseRows = FirstSource;
        }
        if (isBusTarget && !isEmpty && row >= baseRows) {
            int offset = row - baseRows;
            if (offset == 0) return BusBias;
            if (offset == 1) return BusDepth;
            if (offset == 2) return BusShaper;
        }
        int baseRowsNoRotate = baseRows;
        if (isBusTarget && !isEmpty) {
            baseRowsNoRotate += 3;
        }
        if (showRotateMode && !isEmpty && row == baseRowsNoRotate) {
            return RotateMode;
        }
        return Item(row);
    }

    const char *itemName(Item item) const {
        switch (item) {
        case Target:        return "Target";
        case Min:           return "Min";
        case Max:           return "Max";
        case RotateMode:    return "Mode";
        case Tracks:        return "Tracks";
        case Source:        return "Source";
        // case CvRange:
        case MidiSource:    return Routing::isCvSource(_route.source()) ? "Range" : "MIDI Source";
        case MidiEvent:     return "MIDI Event";
        // case MidiControlNumber:
        case MidiNote:
                            return _route.midiSource().isControlEvent() ? "CC Number" : "Note";
        case MidiNoteRange: return "Note Range";
        case BusBias:       return "Bias";
        case BusDepth:      return "Depth";
        case BusShaper:     return "Shaper";
        case Last:          break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case Target:
            _route.printTarget(str);
            break;
        case Min:
            _route.printMin(str);
            break;
        case Max:
            _route.printMax(str);
            break;
        case RotateMode:
            str(_route.cvRotateInterpolate() ? "Interp" : "Step");
            break;
        case Tracks:
            _route.printTracks(str);
            break;
        case Source:
            _route.printSource(str);
            break;
        // case CvRange:
        case MidiSource:
            if (Routing::isCvSource(_route.source())) {
                _route.cvSource().printRange(str);
            } else {
                _route.midiSource().source().print(str);
            }
            break;
        case MidiEvent:
            _route.midiSource().printEvent(str);
            break;
        // case MidiControlNumber:
        case MidiNote:
            if (_route.midiSource().isControlEvent()) {
                _route.midiSource().printControlNumber(str);
            } else {
                _route.midiSource().printNote(str);
            }
            break;
        case MidiNoteRange:
            _route.midiSource().printNoteRange(str);
            break;
        case BusBias:
            str("%d%%", _route.biasPct(0));
            break;
        case BusDepth:
            str("%d%%", _route.depthPct(0));
            break;
        case BusShaper:
            str(shaperShort(_route.shaper(0)));
            break;
        case Last:
            break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case Target:
            _route.editTarget(value, shift);
            break;
        case Min:
            _route.editMin(value, shift);
            break;
        case Max:
            _route.editMax(value, shift);
            break;
        case RotateMode:
            if (value != 0) {
                _route.setCvRotateInterpolate(!_route.cvRotateInterpolate());
            }
            break;
        case Tracks:
            // handled in RoutePage
            break;
        case Source:
            _route.editSource(value, shift);
            break;
        // case CvRange:
        case MidiSource:
            if (Routing::isCvSource(_route.source())) {
                _route.cvSource().editRange(value, shift);
            } else {
                _route.midiSource().source().edit(value, shift);
            }
            break;
        case MidiEvent:
            _route.midiSource().editEvent(value, shift);
            break;
        // case MidiControlNumber:
        case MidiNote:
            if (_route.midiSource().isControlEvent()) {
                _route.midiSource().editControlNumber(value, shift);
            } else {
                _route.midiSource().editNote(value, shift);
            }
            break;
        case MidiNoteRange:
            _route.midiSource().editNoteRange(value, shift);
            break;
        case BusBias:
            _route.setBiasPct(0, _route.biasPct(0) + value * (shift ? 10 : 1));
            break;
        case BusDepth:
            _route.setDepthPct(0, _route.depthPct(0) + value * (shift ? 10 : 1));
            break;
        case BusShaper: {
            int next = int(_route.shaper(0)) + value;
            if (next < 0) next = int(Routing::Shaper::Last) - 1;
            if (next >= int(Routing::Shaper::Last)) next = 0;
            auto shaper = Routing::Shaper(next);
            _route.setShaper(0, shaper);
            _route.setCreaseEnabled(0, shaper == Routing::Shaper::Crease);
            break;
        }
        case Last:
            break;
        }
    }

    const char *shaperShort(Routing::Shaper shaper) const {
        switch (shaper) {
        case Routing::Shaper::None:               return "NO";
        case Routing::Shaper::Crease:             return "CR";
        case Routing::Shaper::Location:           return "LO";
        case Routing::Shaper::Envelope:           return "EN";
        case Routing::Shaper::TriangleFold:       return "TF";
        case Routing::Shaper::FrequencyFollower:  return "FF";
        case Routing::Shaper::Activity:           return "AC";
        case Routing::Shaper::ProgressiveDivider: return "PD";
        case Routing::Shaper::VcaNext:            return "VC";
        case Routing::Shaper::Last:               break;
        }
        return "NO";
    }

    Routing::Route &_route;
};
