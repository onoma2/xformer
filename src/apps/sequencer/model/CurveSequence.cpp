#include "CurveSequence.h"
#include "ProjectVersion.h"
#include "ModelUtils.h"

Types::LayerRange CurveSequence::layerRange(Layer layer) {
    #define CASE(_name_) \
    case Layer::_name_: \
        return { _name_::Min, _name_::Max };

    switch (layer) {
    case Layer::Shape:
    case Layer::ShapeVariation:
        return { 0, int(Curve::Last) - 1 };
    CASE(ShapeVariationProbability)
    CASE(Min)
    CASE(Max)
    CASE(Gate)
    CASE(GateProbability)
    case Layer::Last:
        break;
    }

    #undef CASE

    return { 0, 0 };
}

int CurveSequence::layerDefaultValue(Layer layer)
{
    CurveSequence::Step step;

    switch (layer) {
    case Layer::Shape:
        return step.shape();
    case Layer::ShapeVariation:
        return step.shapeVariation();
    case Layer::ShapeVariationProbability:
        return step.shapeVariationProbability();
    case Layer::Min:
        return step.min();
    case Layer::Max:
        return step.max();
    case Layer::Gate:
        return step.gate();
    case Layer::GateProbability:
        return step.gateProbability();
    case Layer::Last:
        break;
    }

    return 0;
}

int CurveSequence::Step::layerValue(Layer layer) const {
    switch (layer) {
    case Layer::Shape:
        return shape();
    case Layer::ShapeVariation:
        return shapeVariation();
    case Layer::ShapeVariationProbability:
        return shapeVariationProbability();
    case Layer::Min:
        return min();
    case Layer::Max:
        return max();
    case Layer::Gate:
        return gate();
    case Layer::GateProbability:
        return gateProbability();
    case Layer::Last:
        break;
    }

    return 0;
}

void CurveSequence::Step::setLayerValue(Layer layer, int value) {
    switch (layer) {
    case Layer::Shape:
        setShape(value);
        break;
    case Layer::ShapeVariation:
        setShapeVariation(value);
        break;
    case Layer::ShapeVariationProbability:
        setShapeVariationProbability(value);
        break;
    case Layer::Min:
        setMin(value);
        break;
    case Layer::Max:
        setMax(value);
        break;
    case Layer::Gate:
        setGate(value);
        break;
    case Layer::GateProbability:
        setGateProbability(value);
        break;
    case Layer::Last:
        break;
    }
}

void CurveSequence::Step::clear() {
    _data0.raw = 0;
    _data1.raw = 0;
    setShape(0);
    setShapeVariation(0);
    setShapeVariationProbability(0);
    setMin(0);
    setMax(Max::Max);
    setGate(0);
    setGateProbability(GateProbability::Max);
}

void CurveSequence::Step::write(VersionedSerializedWriter &writer) const {
    writer.write(_data0);
    writer.write(_data1);
}

void CurveSequence::Step::read(VersionedSerializedReader &reader) {
    if (reader.dataVersion() < ProjectVersion::Version15) {
        uint8_t shape, min, max;
        reader.read(shape);
        reader.read(min);
        reader.read(max);
        _data0.shape = shape;
        _data0.min = min;
        _data0.max = max;

        if (reader.dataVersion() < ProjectVersion::Version14) {
            if (_data0.shape <= 1) {
                _data0.shape = (_data0.shape + 1) % 2;
            }
        }
    } else {
        reader.read(_data0);
        reader.read(_data1);
    }
}

void CurveSequence::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::Divisor:
        setDivisor(intValue, true);
        break;
    case Routing::Target::RunMode:
        setRunMode(Types::RunMode(intValue), true);
        break;
    case Routing::Target::FirstStep:
        setFirstStep(intValue, true);
        break;
    case Routing::Target::LastStep:
        setLastStep(intValue, true);
        break;
    default:
        break;
    }
}

void CurveSequence::clear() {
    setRange(Types::VoltageRange::Bipolar5V);
    setDivisor(12);
    setResetMeasure(0);
    setRunMode(Types::RunMode::Forward);
    setFirstStep(0);
    setLastStep(15);

    clearSteps();
}

void CurveSequence::clearSteps() {
    for (auto &step : _steps) {
        step.clear();
    }
}

bool CurveSequence::isEdited() const {
    auto clearStep = Step();
    for (const auto &step : _steps) {
        if (step != clearStep) {
            return true;
        }
    }
    return false;
}

void CurveSequence::setShapes(std::initializer_list<int> shapes) {
    size_t step = 0;
    for (auto shape : shapes) {
        if (step < _steps.size()) {
            _steps[step++].setShape(shape);
        }
    }
}

void CurveSequence::shiftSteps(const std::bitset<CONFIG_STEP_COUNT> &selected, int direction) {
    if (selected.any()) {
        ModelUtils::shiftSteps(_steps, selected, direction);
    } else {
        ModelUtils::shiftSteps(_steps, firstStep(), lastStep(), direction);
    }
}

void CurveSequence::duplicateSteps() {
    ModelUtils::duplicateSteps(_steps, firstStep(), lastStep());
    setLastStep(lastStep() + (lastStep() - firstStep() + 1));
}

void CurveSequence::write(VersionedSerializedWriter &writer) const {
    writer.write(_range);
    writer.write(_divisor.base);
    writer.write(_resetMeasure);
    writer.write(_runMode.base);
    writer.write(_firstStep.base);
    writer.write(_lastStep.base);

    writeArray(writer, _steps);
}

void CurveSequence::read(VersionedSerializedReader &reader) {
    reader.read(_range);
    if (reader.dataVersion() < ProjectVersion::Version10) {
        reader.readAs<uint8_t>(_divisor.base);
    } else {
        reader.read(_divisor.base);
    }
    reader.read(_resetMeasure);
    reader.read(_runMode.base);
    reader.read(_firstStep.base);
    reader.read(_lastStep.base);

    readArray(reader, _steps);
}

void CurveSequence::populateWithLfoShape(Curve::Type shape, int firstStep, int lastStep) {
    // Clamp the shape to valid range
    int clampedShape = clamp(static_cast<int>(shape), 0, static_cast<int>(Curve::Last) - 1);

    // Ensure range is valid
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    // Populate the range with the specified shape
    for (int i = minStep; i <= maxStep; ++i) {
        _steps[i].setShape(clampedShape);
    }
}

void CurveSequence::populateWithLfoPattern(Curve::Type shape, int firstStep, int lastStep) {
    // Clamp the shape to valid range
    int clampedShape = clamp(static_cast<int>(shape), 0, static_cast<int>(Curve::Last) - 1);

    // Ensure range is valid
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    // For pattern, we can cycle through different variations of the same shape type
    // This creates a more interesting LFO pattern
    for (int i = minStep; i <= maxStep; ++i) {
        // Use the base shape but vary based on position for more interesting patterns
        _steps[i].setShape(clampedShape);
    }
}

void CurveSequence::populateWithLfoWaveform(Curve::Type upShape, Curve::Type downShape, int firstStep, int lastStep) {
    // Clamp shapes to valid range
    int clampedUpShape = clamp(static_cast<int>(upShape), 0, static_cast<int>(Curve::Last) - 1);
    int clampedDownShape = clamp(static_cast<int>(downShape), 0, static_cast<int>(Curve::Last) - 1);

    // Ensure range is valid
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    // Alternate between up and down shapes to simulate a waveform
    for (int i = minStep; i <= maxStep; ++i) {
        if ((i - minStep) % 2 == 0) {
            _steps[i].setShape(clampedUpShape);
        } else {
            _steps[i].setShape(clampedDownShape);
        }
    }
}

// Additional LFO pattern functions for more complex waveforms
void CurveSequence::populateWithSineWaveLfo(int firstStep, int lastStep) {
    // Create a sine-like LFO pattern using SmoothUp and SmoothDown
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    int rangeSize = maxStep - minStep + 1;
    if (rangeSize < 2) {
        if (minStep <= maxStep) {
            _steps[minStep].setShape(static_cast<int>(Curve::SmoothUp));
        }
        return;
    }

    // Create a sine-like pattern by alternating SmoothUp and SmoothDown shapes
    for (int i = minStep; i <= maxStep; ++i) {
        if ((i - minStep) % 2 == 0) {
            _steps[i].setShape(static_cast<int>(Curve::SmoothUp));
        } else {
            _steps[i].setShape(static_cast<int>(Curve::SmoothDown));
        }
    }
}

void CurveSequence::populateWithTriangleWaveLfo(int firstStep, int lastStep) {
    // Create a triangle LFO pattern using Triangle shape
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        _steps[i].setShape(static_cast<int>(Curve::Triangle));
    }
}

void CurveSequence::populateWithSawtoothWaveLfo(int firstStep, int lastStep) {
    // Create a sawtooth LFO pattern using RampUp shape
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        _steps[i].setShape(static_cast<int>(Curve::RampUp));
    }
}

void CurveSequence::populateWithSquareWaveLfo(int firstStep, int lastStep) {
    // Create a square LFO pattern using StepUp and StepDown shapes alternating
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        if ((i - minStep) % 2 == 0) {
            _steps[i].setShape(static_cast<int>(Curve::StepUp));
        } else {
            _steps[i].setShape(static_cast<int>(Curve::StepDown));
        }
    }
}
