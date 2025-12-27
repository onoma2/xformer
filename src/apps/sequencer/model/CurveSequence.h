#pragma once

#include "Config.h"
#include "Bitfield.h"
#include "Serialize.h"
#include "ModelUtils.h"
#include "Types.h"
#include "Curve.h"
#include "Routing.h"

#include "core/math/Math.h"
#include "core/utils/StringBuilder.h"

#include <array>
#include <bitset>
#include <cstdint>
#include <initializer_list>

class CurveSequence {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using Shape = UnsignedValue<6>;
    using ShapeVariationProbability = UnsignedValue<4>;
    using Min = UnsignedValue<8>;
    using Max = UnsignedValue<8>;
    using Gate = UnsignedValue<4>;
    using GateProbability = UnsignedValue<3>;

    enum class Layer {
        Shape,
        ShapeVariation,
        ShapeVariationProbability,
        Min,
        Max,
        Gate,
        GateProbability,
        Last
    };

    // New Gate Logic Definitions
    enum class AdvancedGateMode {
        Off = 0,
        RisingSlope = 1,
        FallingSlope = 2,
        AnySlope = 3,
        Compare25 = 4,
        Compare50 = 5,
        Compare75 = 6,
        Window = 7
    };

    enum EventGateBits {
        Peak = 1,
        Trough = 2,
        ZeroRise = 4,
        ZeroFall = 8
    };

    static const char *layerName(Layer layer) {
        switch (layer) {
        case Layer::Shape:                      return "SHAPE";
        case Layer::ShapeVariation:             return "SHAPE VAR";
        case Layer::ShapeVariationProbability:  return "SHAPE PROB";
        case Layer::Min:                        return "MIN";
        case Layer::Max:                        return "MAX";
        case Layer::Gate:                       return "GATE";
        case Layer::GateProbability:            return "GATE PROB";
        case Layer::Last:                       break;
        }
        return nullptr;
    }

    static Types::LayerRange layerRange(Layer layer);
    static int layerDefaultValue(Layer layer);

    class Step {
    public:
        //----------------------------------------
        // Properties
        //----------------------------------------

        // shape

        int shape() const { return _data0.shape; }
        void setShape(int shape) {
            _data0.shape = clamp(shape, 0, int(Curve::Last) - 1);
        }

        // shapeVariation

        int shapeVariation() const { return _data0.shapeVariation; }
        void setShapeVariation(int shapeVariation) {
            _data0.shapeVariation = clamp(shapeVariation, 0, int(Curve::Last) - 1);
        }

        // shapeVariationProbability

        int shapeVariationProbability() const { return _data0.shapeVariationProbability; }
        void setShapeVariationProbability(int shapeVariationProbability) {
            _data0.shapeVariationProbability = clamp(shapeVariationProbability, 0, 8);
        }

        // min

        int min() const { return _data0.min; }
        void setMin(int min) {
            _data0.min = Min::clamp(min);
        }

        float minNormalized() const { return float(min()) / Min::Max; }
        void setMinNormalized(float min) {
            setMin(int(std::round(min * Min::Max)));
        }

        // max

        int max() const { return _data0.max; }
        void setMax(int max) {
            _data0.max = Max::clamp(max);
        }

        float maxNormalized() const { return float(max()) / Max::Max; }
        void setMaxNormalized(float max) {
            setMax(int(std::round(max * Max::Max)));
        }

        // gateEventMask (compatible with old gate() getter/setter)

        int gate() const { return _data1.gateEventMask; }
        void setGate(int gate) {
            _data1.gateEventMask = clamp(gate, 0, 15);
        }

        int gateEventMask() const { return _data1.gateEventMask; }
        void setGateEventMask(int mask) {
            _data1.gateEventMask = clamp(mask, 0, 15);
        }

        // gateParameter (compatible with old gateProbability() getter/setter)

        int gateProbability() const { return _data1.gateParameter; }
        void setGateProbability(int gateProbability) {
            _data1.gateParameter = clamp(gateProbability, 0, 7);
        }

        int gateParameter() const { return _data1.gateParameter; }
        void setGateParameter(int param) {
            _data1.gateParameter = clamp(param, 0, 7);
        }

        // Helper: Get trigger length in ticks (exponential scale)
        uint32_t gateTriggerLength() const {
            // 0→4, 1→8, 2→16, 3→32, 4→64, 5→128, 6→256, 7→512 ticks
            return 4u << _data1.gateParameter;
        }

        // Helper: Get advanced mode
        AdvancedGateMode gateAdvancedMode() const {
            return AdvancedGateMode(int(_data1.gateParameter));
        }

        int layerValue(Layer layer) const;
        void setLayerValue(Layer layer, int value);

        //----------------------------------------
        // Methods
        //----------------------------------------

        Step() { clear(); }

        void clear();

        void write(VersionedSerializedWriter &writer) const;
        void read(VersionedSerializedReader &reader);

        bool operator==(const Step &other) const {
            return _data0.raw == other._data0.raw;
        }

        bool operator!=(const Step &other) const {
            return !(*this == other);
        }

    private:
        union {
            uint32_t raw;
            BitField<uint32_t, 0, Shape::Bits> shape;
            BitField<uint32_t, 6, Shape::Bits> shapeVariation;
            BitField<uint32_t, 12, ShapeVariationProbability::Bits> shapeVariationProbability;
            BitField<uint32_t, 16, Min::Bits> min;
            BitField<uint32_t, 24, Max::Bits> max;
        } _data0;
        union {
            uint16_t raw;
            BitField<uint16_t, 0, 4> gateEventMask;      // bits 0-3: Event enable flags
            BitField<uint16_t, 4, 3> gateParameter;      // bits 4-6: Trigger length or Advanced mode
            // 9 bits left (7-15)
        } _data1;
    };

    using StepArray = std::array<Step, CONFIG_STEP_COUNT>;

    //----------------------------------------
    // Properties
    //----------------------------------------

    // trackIndex

    int trackIndex() const { return _trackIndex; }

    // range

    Types::VoltageRange range() const { return _range; }
    void setRange(Types::VoltageRange range) {
        _range = ModelUtils::clampedEnum(range);
    }

    void editRange(int value, bool shift) {
        setRange(ModelUtils::adjustedEnum(range(), value));
    }

    void printRange(StringBuilder &str) const {
        str(Types::voltageRangeName(range()));
    }

    // divisor

    int divisor() const { return _divisor.get(isRouted(Routing::Target::Divisor)); }
    void setDivisor(int divisor, bool routed = false) {
        _divisor.set(ModelUtils::clampDivisor(divisor), routed);
    }

    int indexedDivisor() const { return ModelUtils::divisorToIndex(divisor()); }
    void setIndexedDivisor(int index) {
        int divisor = ModelUtils::indexToDivisor(index);
        if (divisor > 0) {
            setDivisor(divisor);
        }
    }

    void editDivisor(int value, bool shift) {
        if (!isRouted(Routing::Target::Divisor)) {
            setDivisor(ModelUtils::adjustedByDivisor(divisor(), value, shift));
        }
    }

    void printDivisor(StringBuilder &str) const {
        printRouted(str, Routing::Target::Divisor);
        ModelUtils::printDivisor(str, divisor());
    }

    // resetMeasure

    int resetMeasure() const { return _resetMeasure; }
    void setResetMeasure(int resetMeasure) {
        _resetMeasure = clamp(resetMeasure, 0, 128);
    }

    void editResetMeasure(int value, bool shift) {
        setResetMeasure(ModelUtils::adjustedByPowerOfTwo(resetMeasure(), value, shift));
    }

    void printResetMeasure(StringBuilder &str) const {
        if (resetMeasure() == 0) {
            str("off");
        } else {
            str("%d %s", resetMeasure(), resetMeasure() > 1 ? "bars" : "bar");
        }
    }

    // runMode

    Types::RunMode runMode() const { return _runMode.get(isRouted(Routing::Target::RunMode)); }
    void setRunMode(Types::RunMode runMode, bool routed = false) {
        _runMode.set(ModelUtils::clampedEnum(runMode), routed);
    }

    void editRunMode(int value, bool shift) {
        if (!isRouted(Routing::Target::RunMode)) {
            setRunMode(ModelUtils::adjustedEnum(runMode(), value));
        }
    }

    void printRunMode(StringBuilder &str) const {
        printRouted(str, Routing::Target::RunMode);
        str(Types::runModeName(runMode()));
    }

    // firstStep

    int firstStep() const {
        return _firstStep.get(isRouted(Routing::Target::FirstStep));
    }

    void setFirstStep(int firstStep, bool routed = false) {
        _firstStep.set(clamp(firstStep, 0, lastStep()), routed);
    }

    void editFirstStep(int value, bool shift) {
        if (shift) {
            offsetFirstAndLastStep(value);
        } else if (!isRouted(Routing::Target::FirstStep)) {
            setFirstStep(firstStep() + value);
        }
    }

    void printFirstStep(StringBuilder &str) const {
        printRouted(str, Routing::Target::FirstStep);
        str("%d", firstStep() + 1);
    }

    // lastStep

    int lastStep() const {
        // make sure last step is always >= first step even if stored value is invalid (due to routing changes)
        return std::max(firstStep(), int(_lastStep.get(isRouted(Routing::Target::LastStep))));
    }

    void setLastStep(int lastStep, bool routed = false) {
        _lastStep.set(clamp(lastStep, firstStep(), CONFIG_STEP_COUNT - 1), routed);
    }

    void editLastStep(int value, bool shift) {
        if (shift) {
            offsetFirstAndLastStep(value);
        } else if (!isRouted(Routing::Target::LastStep)) {
            setLastStep(lastStep() + value);
        }
    }

    void printLastStep(StringBuilder &str) const {
        printRouted(str, Routing::Target::LastStep);
        str("%d", lastStep() + 1);
    }

    // Chaos

    enum class ChaosAlgorithm : uint8_t {
        Latoocarfian,
        Lorenz,
        Last
    };

    static const char *chaosAlgorithmName(ChaosAlgorithm algo) {
        switch (algo) {
        case ChaosAlgorithm::Latoocarfian: return "Latoocarfian";
        case ChaosAlgorithm::Lorenz:       return "Lorenz";
        case ChaosAlgorithm::Last:         break;
        }
        return nullptr;
    }

    // wavefolderFold

    float wavefolderFold() const { return _wavefolderFold.get(isRouted(Routing::Target::WavefolderFold)); }
    void setWavefolderFold(float value, bool routed = false) {
        _wavefolderFold.set(clamp(value, 0.f, 1.f), routed);
    }
    void editWavefolderFold(int value, bool shift) {
        if (!isRouted(Routing::Target::WavefolderFold)) {
            setWavefolderFold(wavefolderFold() + value * (shift ? 0.1f : 0.01f));
        }
    }
    void printWavefolderFold(StringBuilder &str) const {
        printRouted(str, Routing::Target::WavefolderFold);
        str("%.2f", wavefolderFold());
    }

    // wavefolderGain

    float wavefolderGain() const { return _wavefolderGain.get(isRouted(Routing::Target::WavefolderGain)); }
    void setWavefolderGain(float value, bool routed = false) {
        _wavefolderGain.set(clamp(value, 0.f, 2.f), routed);
    }
    void editWavefolderGain(int value, bool shift) {
        if (!isRouted(Routing::Target::WavefolderGain)) {
            setWavefolderGain(wavefolderGain() + value * (shift ? 0.1f : 0.01f));
        }
    }
    void printWavefolderGain(StringBuilder &str) const {
        printRouted(str, Routing::Target::WavefolderGain);
        str("%.2f", wavefolderGain());
    }

    // djFilter

    float djFilter() const { return _djFilter.get(isRouted(Routing::Target::DjFilter)); }
    void setDjFilter(float value, bool routed = false) {
        _djFilter.set(clamp(value, -1.f, 1.f), routed);
    }
    void editDjFilter(int value, bool shift) {
        if (!isRouted(Routing::Target::DjFilter)) {
            setDjFilter(djFilter() + value * (shift ? 0.1f : 0.01f));
        }
    }
    void printDjFilter(StringBuilder &str) const {
        printRouted(str, Routing::Target::DjFilter);
        str("%+.2f", djFilter());
    }

    // xFade
    float xFade() const { return _xFade.get(isRouted(Routing::Target::XFade)); }
    void setXFade(float value, bool routed = false) {
        _xFade.set(clamp(value, 0.f, 1.f), routed);
    }
    void editXFade(int value, bool shift) {
        if (!isRouted(Routing::Target::XFade)) {
            setXFade(xFade() + value * (shift ? 0.1f : 0.01f));
        }
    }
    void printXFade(StringBuilder &str) const {
        printRouted(str, Routing::Target::XFade);
        str("%.2f", xFade());
    }

    // chaos

    int chaosAmount() const { return _chaosAmount.get(isRouted(Routing::Target::ChaosAmount)); }
    void setChaosAmount(int value, bool routed = false) {
        _chaosAmount.set(clamp(value, 0, 100), routed);
    }
    void editChaosAmount(int value, bool shift) {
        if (!isRouted(Routing::Target::ChaosAmount)) {
            setChaosAmount(chaosAmount() + value * (shift ? 5 : 1));
        }
    }
    void printChaosAmount(StringBuilder &str) const {
        printRouted(str, Routing::Target::ChaosAmount);
        str("%d%%", chaosAmount());
    }

    ChaosAlgorithm chaosAlgo() const { return _chaosAlgo; }
    void setChaosAlgo(ChaosAlgorithm algo) { _chaosAlgo = ModelUtils::clampedEnum(algo); }
    void editChaosAlgo(int value, bool shift) { setChaosAlgo(ModelUtils::adjustedEnum(chaosAlgo(), value)); }
    void printChaosAlgo(StringBuilder &str) const { str(chaosAlgorithmName(chaosAlgo())); }

    int chaosRate() const { return _chaosRate.get(isRouted(Routing::Target::ChaosRate)); }
    void setChaosRate(int value, bool routed = false) {
        _chaosRate.set(clamp(value, 0, 127), routed);
    }
    void editChaosRate(int value, bool shift) {
        if (!isRouted(Routing::Target::ChaosRate)) {
            setChaosRate(chaosRate() + value * (shift ? 5 : 1));
        }
    }
    float chaosHz() const {
        float normalized = chaosRate() / 127.f;
        if (normalized < 0.33f) {
            // First 1/3: 0.01 Hz to 0.1 Hz (Very Slow)
            // Range 0.33 maps to 0.09 Hz delta
            float t = normalized / 0.33f;
            return 0.01f + t * 0.09f;
        } else if (normalized < 0.66f) {
            // Middle 1/3: 0.1 Hz to 2.0 Hz (Musical LFO)
            // Range 0.33 maps to 1.9 Hz delta
            float t = (normalized - 0.33f) / 0.33f;
            // Use quadratic curve for musical feel
            return 0.1f + (t * t) * 1.9f;
        } else {
            // Last 1/3: 2.0 Hz to 50.0 Hz (Fast)
            float t = (normalized - 0.66f) / 0.34f;
            // Use cubic curve for fast acceleration
            return 2.0f + (t * t * t) * 48.0f;
        }
    }
    void printChaosRate(StringBuilder &str) const {
        printRouted(str, Routing::Target::ChaosRate);
        float rate = chaosHz();
        if (rate < 1.f) str("%.2fHz", rate);
        else if (rate < 10.f) str("%.1fHz", rate);
        else str("%.0fHz", rate);
    }

    int chaosParam1() const { return _chaosParam1.get(isRouted(Routing::Target::ChaosParam1)); }
    void setChaosParam1(int value, bool routed = false) {
        _chaosParam1.set(clamp(value, 0, 100), routed);
    }
    void editChaosParam1(int value, bool shift) {
        if (!isRouted(Routing::Target::ChaosParam1)) {
            setChaosParam1(chaosParam1() + value * (shift ? 5 : 1));
        }
    }
    void printChaosParam1(StringBuilder &str) const {
        printRouted(str, Routing::Target::ChaosParam1);
        str("%d", chaosParam1());
    }

    int chaosParam2() const { return _chaosParam2.get(isRouted(Routing::Target::ChaosParam2)); }
    void setChaosParam2(int value, bool routed = false) {
        _chaosParam2.set(clamp(value, 0, 100), routed);
    }
    void editChaosParam2(int value, bool shift) {
        if (!isRouted(Routing::Target::ChaosParam2)) {
            setChaosParam2(chaosParam2() + value * (shift ? 5 : 1));
        }
    }
    void printChaosParam2(StringBuilder &str) const {
        printRouted(str, Routing::Target::ChaosParam2);
        str("%d", chaosParam2());
    }

    // steps

    const StepArray &steps() const { return _steps; }
          StepArray &steps()       { return _steps; }

    const Step &step(int index) const { return _steps[index]; }
          Step &step(int index)       { return _steps[index]; }

    //----------------------------------------
    // Routing
    //----------------------------------------

    inline bool isRouted(Routing::Target target) const { return Routing::isRouted(target, _trackIndex); }
    inline void printRouted(StringBuilder &str, Routing::Target target) const { Routing::printRouted(str, target, _trackIndex); }
    void writeRouted(Routing::Target target, int intValue, float floatValue);

    //----------------------------------------
    // Methods
    //----------------------------------------

    CurveSequence() { clear(); }

    void clear();
    void clearSteps();

    bool isEdited() const;

    void setShapes(std::initializer_list<int> shapes);

    void shiftSteps(const std::bitset<CONFIG_STEP_COUNT> &selected, int direction);

    void duplicateSteps();

    // LFO-shape population functions
    void populateWithLfoShape(Curve::Type shape, int firstStep, int lastStep);
    void populateWithLfoPattern(Curve::Type shape, int firstStep, int lastStep);
    void populateWithLfoWaveform(Curve::Type upShape, Curve::Type downShape, int firstStep, int lastStep);

    // Advanced LFO waveform functions
    void populateWithSineWaveLfo(int firstStep, int lastStep);
    void populateWithTriangleWaveLfo(int firstStep, int lastStep);
    void populateWithSawtoothWaveLfo(int firstStep, int lastStep);
    void populateWithSquareWaveLfo(int firstStep, int lastStep);
    void populateWithRandomMinMax(int firstStep, int lastStep);

    // Macro Curve functions (multi-step rasterization)
    void populateWithMacroInit(int firstStep, int lastStep);
    void populateWithMacroFm(int firstStep, int lastStep);
    void populateWithMacroDamp(int firstStep, int lastStep);
    void populateWithMacroBounce(int firstStep, int lastStep);
    void populateWithRasterizedShape(int firstStep, int lastStep);

    // Transformation functions
    void transformInvert(int firstStep, int lastStep);
    void transformReverse(int firstStep, int lastStep);
    void transformMirror(int firstStep, int lastStep);
    void transformAlign(int firstStep, int lastStep);
    void transformSmoothWalk(int firstStep, int lastStep);

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int trackIndex) { _trackIndex = trackIndex; }

    void offsetFirstAndLastStep(int value) {
        value = clamp(value, -firstStep(), CONFIG_STEP_COUNT - 1 - lastStep());
        if (value > 0) {
            editLastStep(value, false);
            editFirstStep(value, false);
        } else {
            editFirstStep(value, false);
            editLastStep(value, false);
        }
    }

    int8_t _trackIndex = -1;
    Types::VoltageRange _range;
    Routable<uint16_t> _divisor;
    uint8_t _resetMeasure;
    Routable<Types::RunMode> _runMode;
    Routable<uint8_t> _firstStep;
    Routable<uint8_t> _lastStep;

    Routable<float> _wavefolderFold;
    Routable<float> _wavefolderGain;
    Routable<float> _djFilter;
    Routable<float> _xFade;

    Routable<int> _chaosAmount;
    ChaosAlgorithm _chaosAlgo;
    Routable<int> _chaosRate;
    Routable<int> _chaosParam1;
    Routable<int> _chaosParam2;

    StepArray _steps;

    friend class CurveTrack;
};
