#pragma once

#include "Config.h"

#include "RoutableListModel.h"

#include "model/FractalSequence.h"

class FractalSequenceListModel : public RoutableListModel {
public:
    enum Item {
        LoopFirst,
        LoopLast,
        RecordFirst,
        RecordLast,
        OrnFirst,
        OrnLast,
        OrderMode,
        OrnamentRate,
        OrnamentIntensity,
        RecordSkip,
        RecordMode,
        CaptureCadence,
        CaptureFidelity,
        Last
    };

    FractalSequenceListModel()
    {}

    void setSequence(FractalSequence *sequence) {
        _sequence = sequence;
    }

    virtual int rows() const override {
        return _sequence ? int(Last) : 0;
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

    virtual int indexedCount(int row) const override {
        return indexedCountValue(Item(row));
    }

    virtual int indexed(int row) const override {
        return indexedValue(Item(row));
    }

    virtual void setIndexed(int row, int index) override {
        if (index >= 0 && index < indexedCount(row)) {
            setIndexedValue(Item(row), index);
        }
    }

    virtual Routing::Target routingTarget(int row) const override {
        switch (Item(row)) {
        case OrnamentRate:
            return Routing::Target::FractalOrnamentRate;
        case OrnamentIntensity:
            return Routing::Target::FractalOrnamentIntensity;
        default:
            return Routing::Target::None;
        }
    }

    int rowForItem(Item item) const {
        return int(item);
    }

private:
    static const char *itemName(Item item) {
        switch (item) {
        case LoopFirst:         return "Loop First";
        case LoopLast:          return "Loop Last";
        case RecordFirst:       return "Rec First";
        case RecordLast:        return "Rec Last";
        case OrnFirst:          return "Orn First";
        case OrnLast:           return "Orn Last";
        case OrderMode:         return "Order Mode";
        case OrnamentRate:      return "Orn Rate";
        case OrnamentIntensity: return "Orn Intens";
        case RecordSkip:        return "Rec Skip";
        case RecordMode:        return "Rec Mode";
        case CaptureCadence:    return "Capture";
        case CaptureFidelity:   return "Fidelity";
        case Last:              break;
        }
        return nullptr;
    }

    void formatName(Item item, StringBuilder &str) const {
        str(itemName(item));
    }

    void formatValue(Item item, StringBuilder &str) const {
        switch (item) {
        case LoopFirst:         _sequence->printLoopFirst(str); break;
        case LoopLast:          _sequence->printLoopLast(str); break;
        case RecordFirst:       _sequence->printRecordFirst(str); break;
        case RecordLast:        _sequence->printRecordLast(str); break;
        case OrnFirst:          str("%d", _sequence->ornFirst() + 1); break;
        case OrnLast:           str("%d", _sequence->ornLast() + 1); break;
        case OrderMode:         _sequence->printOrderMode(str); break;
        case OrnamentRate:      str("%d%%", _sequence->ornamentRate()); break;
        case OrnamentIntensity: str("%d%%", _sequence->ornamentIntensity()); break;
        case RecordSkip:        _sequence->printRecordSkip(str); break;
        case RecordMode:        _sequence->printRecordMode(str); break;
        case CaptureCadence:    _sequence->printCaptureCadence(str); break;
        case CaptureFidelity:   _sequence->printCaptureFidelity(str); break;
        case Last:              break;
        }
    }

    void editValue(Item item, int value, bool shift) {
        switch (item) {
        case LoopFirst:         _sequence->editLoopFirst(value, shift); break;
        case LoopLast:          _sequence->editLoopLast(value, shift); break;
        case RecordFirst:       _sequence->editRecordFirst(value, shift); break;
        case RecordLast:        _sequence->editRecordLast(value, shift); break;
        case OrnFirst:          _sequence->setOrnFirst(_sequence->ornFirst() + value); break;
        case OrnLast:           _sequence->setOrnLast(_sequence->ornLast() + value); break;
        case OrderMode:         _sequence->editOrderMode(value, shift); break;
        case OrnamentRate:      _sequence->setOrnamentRate(_sequence->ornamentRate() + value * (shift ? 10 : 1)); break;
        case OrnamentIntensity: _sequence->setOrnamentIntensity(_sequence->ornamentIntensity() + value * (shift ? 10 : 1)); break;
        case RecordSkip:        _sequence->editRecordSkip(value, shift); break;
        case RecordMode:        _sequence->editRecordMode(value, shift); break;
        case CaptureCadence:    _sequence->editCaptureCadence(value, shift); break;
        case CaptureFidelity:   _sequence->editCaptureFidelity(value, shift); break;
        case Last:              break;
        }
    }

    int indexedCountValue(Item item) const {
        switch (item) {
        case LoopFirst:
        case LoopLast:
        case RecordFirst:
        case RecordLast:
        case OrnFirst:
        case OrnLast:
            return CONFIG_FRACTAL_MAX_CELLS;
        case OrderMode:
            return int(FractalSequence::OrderMode::Last);
        case OrnamentRate:
        case OrnamentIntensity:
            return 101;
        case RecordSkip:
            return 16;
        case RecordMode:
            return int(FractalSequence::RecordMode::Last);
        case CaptureCadence:
            return int(FractalSequence::CaptureCadence::Last);
        case CaptureFidelity:
            return int(FractalSequence::CaptureFidelity::Last);
        case Last:
            break;
        }
        return -1;
    }

    int indexedValue(Item item) const {
        switch (item) {
        case LoopFirst:         return _sequence->loopFirst();
        case LoopLast:          return _sequence->loopLast();
        case RecordFirst:       return _sequence->recordFirst();
        case RecordLast:        return _sequence->recordLast();
        case OrnFirst:          return _sequence->ornFirst();
        case OrnLast:           return _sequence->ornLast();
        case OrderMode:         return int(_sequence->orderMode());
        case OrnamentRate:      return _sequence->ornamentRate();
        case OrnamentIntensity: return _sequence->ornamentIntensity();
        case RecordSkip:        return _sequence->recordSkip();
        case RecordMode:        return int(_sequence->recordMode());
        case CaptureCadence:    return int(_sequence->captureCadence());
        case CaptureFidelity:   return int(_sequence->captureFidelity());
        case Last:              break;
        }
        return -1;
    }

    void setIndexedValue(Item item, int index) {
        switch (item) {
        case LoopFirst:         return _sequence->setLoopFirst(index);
        case LoopLast:          return _sequence->setLoopLast(index);
        case RecordFirst:       return _sequence->setRecordFirst(index);
        case RecordLast:        return _sequence->setRecordLast(index);
        case OrnFirst:          return _sequence->setOrnFirst(index);
        case OrnLast:           return _sequence->setOrnLast(index);
        case OrderMode:         return _sequence->setOrderMode(FractalSequence::OrderMode(index));
        case OrnamentRate:      return _sequence->setOrnamentRate(index);
        case OrnamentIntensity: return _sequence->setOrnamentIntensity(index);
        case RecordSkip:        return _sequence->setRecordSkip(index);
        case RecordMode:        return _sequence->setRecordMode(FractalSequence::RecordMode(index));
        case CaptureCadence:    return _sequence->setCaptureCadence(FractalSequence::CaptureCadence(index));
        case CaptureFidelity:   return _sequence->setCaptureFidelity(FractalSequence::CaptureFidelity(index));
        case Last:              break;
        }
    }

    FractalSequence *_sequence = nullptr;
};
