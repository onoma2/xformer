#pragma once

#include "model/NoteSequence.h"
#include "model/CurveSequence.h"

class SequenceBuilder {
public:
    virtual ~SequenceBuilder() = default;

    virtual void revert() = 0;
    virtual void apply() = 0;
    virtual void showOriginal() = 0;
    virtual void showPreview() = 0;
    virtual bool showingPreview() const = 0;
    virtual void updatePreview() = 0;

    // original sequence

    virtual int originalLength() const = 0;
    virtual float originalValue(int index) const = 0;

    // edit sequence

    virtual int length() const = 0;
    virtual void setLength(int length) = 0;

    virtual float value(int index) const = 0;
    virtual void setValue(int index, float value) = 0;

    virtual void clearSteps() = 0;
    virtual void copyStep(int fromIndex, int toIndex) = 0;

    virtual void clearLayer() = 0;
};

template<typename T>
class SequenceBuilderImpl : public SequenceBuilder {
public:
    SequenceBuilderImpl(T &sequence, typename T::Layer layer) :
        _edit(sequence),
        _original(sequence),
        _preview(nullptr),
        _layer(layer),
        _range(T::layerRange(layer)),
        _default(T::layerDefaultValue(layer)),
        _showingPreview(false)
    {}

    ~SequenceBuilderImpl() override {
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
            _preview = new (std::nothrow) T(_original);
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
            _preview = new (std::nothrow) T(_original);
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
        return _original.lastStep() - _original.firstStep() + 1;
    }

    float originalValue(int index) const override {
        int layerValue = _original.step(_original.firstStep() + index).layerValue(_layer);
        return float(layerValue - _range.min) / (_range.max - _range.min);
    }

    int length() const override {
        return _edit.lastStep() - _edit.firstStep() + 1;
    }

    void setLength(int length) override {
        _edit.setFirstStep(0);
        _edit.setLastStep(length - 1);
    }

    float value(int index) const override {
        int layerValue = _edit.step(_edit.firstStep() + index).layerValue(_layer);
        return float(layerValue - _range.min) / (_range.max - _range.min);
    }

    void setValue(int index, float value) override {
        int layerValue = std::round(value * (_range.max - _range.min) + _range.min);
        _edit.step(_edit.firstStep() + index).setLayerValue(_layer, layerValue);
    }

    void clearSteps() override {
        _edit.clearSteps();
    }

    void copyStep(int fromIndex, int toIndex) override {
        _edit.step(_edit.firstStep() + toIndex) = _original.step(_original.firstStep() + fromIndex);
    }

    void clearLayer() override {
        for (auto &step : _edit.steps()) {
            step.setLayerValue(_layer, _default);
        }
    }

public:
    T &editSequence() { return _edit; }
    const T &originalSequence() const { return _original; }

private:
    T &_edit;
    T _original;
    T *_preview;
    typename T::Layer _layer;
    Types::LayerRange _range;
    int _default;
    bool _showingPreview;
};

using NoteSequenceBuilder = SequenceBuilderImpl<NoteSequence>;
using CurveSequenceBuilder = SequenceBuilderImpl<CurveSequence>;
