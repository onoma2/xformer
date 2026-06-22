#pragma once

#include "Config.h"

#include "SequenceBuilder.h"

#include "core/utils/StringBuilder.h"

#include <array>

using GeneratorPattern = std::array<uint8_t, CONFIG_STEP_COUNT>;

class Generator {
public:
    enum class Mode {
        InitLayer,
        InitSteps,
        Euclidean,
        Random,
        Algo,
        Last
    };

    static const char *modeName(Mode mode) {
        switch (mode) {
        case Mode::InitLayer:   return "Init Layer";
        case Mode::InitSteps:   return "Init Steps";
        case Mode::Euclidean:   return "Euclidean";
        case Mode::Random:      return "Random";
        case Mode::Algo:        return "Algo";
        case Mode::Last:        break;
        }
        return nullptr;
    }

    Generator(SequenceBuilder &builder) :
        _builder(builder)
    {}

    virtual Mode mode() const = 0;
    const char *name() const { return modeName(mode()); }

    // physical step capacity of the target sequence (drives the page grid/bank nav)
    int capacity() const { return _builder.capacity(); }

    // parameters

    virtual int paramCount() const = 0;
    virtual const char *paramName(int index) const = 0;
    virtual void editParam(int index, int value, bool shift) = 0;
    virtual void printParam(int index, StringBuilder &str) const = 0;

    virtual void init() {}
    virtual void randomizeParams() {}

    virtual void revert() {
        _builder.revert();
    }

    virtual void apply() {
        _builder.apply();
    }

    virtual void showOriginal() {
        _builder.showOriginal();
    }

    virtual void showPreview() {
        _builder.showPreview();
    }

    void updatePreview() {
        _builder.updatePreview();
    }

    bool showingPreview() const {
        return _builder.showingPreview();
    }

    virtual void update() = 0;
    virtual int displayValue(int index) const { (void)index; return 0; }

    static Generator *execute(Generator::Mode mode, SequenceBuilder &builder);

protected:
    SequenceBuilder &_builder;
};
