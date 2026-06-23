#pragma once

#include "SequenceBuilder.h"

#include "model/IndexedSequence.h"
#include "model/Scale.h"
#include "model/RotatedScale.h"

#include <cmath>
#include <new>

// Bespoke SequenceBuilder for the Indexed track. The SequenceBuilderImpl<T> template
// cannot be reused: IndexedSequence has no per-step Layer enum and its firstStep() is a
// rotation offset, not an edit-range start. Preview/apply/revert mirror SequenceBuilderImpl
// exactly (IndexedSequence is value-copyable), so apply() commits to the live sequence.
class IndexedSequenceBuilder : public SequenceBuilder {
public:
    // Which step field the generic single-layer generators read/write.
    enum class Layer {
        NoteIndex,
        Duration,
        GateLength,
    };

    IndexedSequenceBuilder(IndexedSequence &sequence, Layer layer, const RotatedScaleView &scale, int rootNote) :
        _edit(sequence),
        _original(sequence),
        _preview(nullptr),
        _layer(layer),
        _scale(scale),
        _rootNote(rootNote),
        _showingPreview(false)
    {}

    ~IndexedSequenceBuilder() override {
        delete _preview;
    }

    void revert() override {
        _edit = _original;
        delete _preview;
        _preview = nullptr;
        _showingPreview = false;
    }

    void apply() override {
        if (_preview) {
            _edit = *_preview;
            _original = *_preview;
            _showingPreview = true;
        }
    }

    void showOriginal() override {
        _edit = _original;
        _showingPreview = false;
    }

    void showPreview() override {
        if (!_preview) {
            _preview = new (std::nothrow) IndexedSequence(_original);
            if (!_preview) {
                _showingPreview = false;
                return;
            }
            *_preview = _edit;
        }
        _edit = *_preview;
        _showingPreview = true;
    }

    void updatePreview() override {
        if (!_preview) {
            _preview = new (std::nothrow) IndexedSequence(_original);
            if (!_preview) {
                return;
            }
        }
        *_preview = _edit;
    }

    bool showingPreview() const override {
        return _showingPreview;
    }

    int originalLength() const override {
        return _original.activeLength();
    }

    float originalValue(int index) const override {
        return layerToFloat(_original.step(index));
    }

    int length() const override {
        return _edit.activeLength();
    }

    void setLength(int length) override {
        _edit.setActiveLength(length);
    }

    int capacity() const override {
        return IndexedSequence::MaxSteps;
    }

    float value(int index) const override {
        return layerToFloat(_edit.step(index));
    }

    void setValue(int index, float value) override {
        floatToLayer(_edit.step(index), value);
    }

    void clearSteps() override {
        for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
            _edit.step(i).clear();
        }
    }

    void copyStep(int fromIndex, int toIndex) override {
        _edit.step(toIndex) = _original.step(fromIndex);
    }

    void clearLayer() override {
        for (int i = 0; i < IndexedSequence::MaxSteps; ++i) {
            setLayerDefault(_edit.step(i));
        }
    }

    // whole-step writers (helical generator) + resolved musical context
    IndexedSequence &editSequence() { return _edit; }
    const Scale &scale() const { return _scale; }
    int rootNote() const { return _rootNote; }

private:
    // Generic value generators are scalar 0..1; map onto the chosen field.
    static constexpr int NoteIndexRange = 11;

    float layerToFloat(const IndexedSequence::Step &step) const {
        switch (_layer) {
        case Layer::NoteIndex:  return float(clamp(int(step.noteIndex()), 0, NoteIndexRange)) / NoteIndexRange;
        case Layer::Duration:   return float(step.duration()) / IndexedSequence::MaxDuration;
        case Layer::GateLength: return float(step.gateLength()) / IndexedSequence::GateLengthMax;
        }
        return 0.f;
    }

    void floatToLayer(IndexedSequence::Step &step, float value) const {
        switch (_layer) {
        case Layer::NoteIndex:  step.setNoteIndex(int8_t(std::lround(value * NoteIndexRange))); break;
        case Layer::Duration:   step.setDuration(uint16_t(std::lround(value * IndexedSequence::MaxDuration))); break;
        case Layer::GateLength: step.setGateLength(uint16_t(std::lround(value * IndexedSequence::GateLengthMax))); break;
        }
    }

    void setLayerDefault(IndexedSequence::Step &step) const {
        switch (_layer) {
        case Layer::NoteIndex:  step.setNoteIndex(0); break;
        case Layer::Duration:   step.setDuration(0); break;
        case Layer::GateLength: step.setGateLength(0); break;
        }
    }

    IndexedSequence &_edit;
    IndexedSequence _original;
    IndexedSequence *_preview;
    Layer _layer;
    RotatedScaleView _scale;
    int _rootNote;
    bool _showingPreview;
};
