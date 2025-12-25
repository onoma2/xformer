#include "CurveSequence.h"
#include "ProjectVersion.h"
#include "ModelUtils.h"
#include "Routing.h"

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
    setGateProbability(0);  // Initialize to 0 (Advanced Mode: OFF)
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
    case Routing::Target::WavefolderFold:
        setWavefolderFold(floatValue / 100.0f, true);  // Convert 0-100 to 0.0-1.0
        break;
    case Routing::Target::WavefolderGain:
        setWavefolderGain(floatValue / 100.0f, true);  // Convert 0-200 to 0.0-2.0
        break;
    case Routing::Target::DjFilter:
        setDjFilter(floatValue / 100.0f, true);  // Convert -100 to 100 to -1.0 to 1.0
        break;
    case Routing::Target::XFade:
        setXFade(floatValue / 100.0f, true);  // Convert 0-100 to 0.0-1.0
        break;
    case Routing::Target::ChaosAmount:
        setChaosAmount(intValue, true);
        break;
    case Routing::Target::ChaosRate:
        setChaosRate(intValue, true);
        break;
    case Routing::Target::ChaosParam1:
        setChaosParam1(intValue, true);
        break;
    case Routing::Target::ChaosParam2:
        setChaosParam2(intValue, true);
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

    setWavefolderFold(0.f);
    setWavefolderGain(0.f);
    setDjFilter(0.f);
    setXFade(1.f);
    
    setChaosAmount(0);
    setChaosAlgo(ChaosAlgorithm::Latoocarfian);
    setChaosRate(0);
    setChaosParam1(0);
    setChaosParam2(0);

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

    writer.write(_wavefolderFold.base);
    writer.write(_wavefolderGain.base);
    writer.write(_djFilter.base);
    writer.write(_xFade.base);
    writer.write(_chaosAmount.base);
    writer.write(_chaosRate.base);
    writer.write(_chaosParam1.base);
    writer.write(_chaosParam2.base);
    writer.write(_chaosAlgo);

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

    reader.read(_wavefolderFold.base);
    reader.read(_wavefolderGain.base);
    reader.read(_djFilter.base);
    reader.read(_xFade.base);
    reader.read(_chaosAmount.base);
    reader.read(_chaosRate.base);
    reader.read(_chaosParam1.base);
    reader.read(_chaosParam2.base);
    reader.read(_chaosAlgo);

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
    // Create a sine-like LFO pattern using Bell shape (1 cycle per step)
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        _steps[i].setShape(static_cast<int>(Curve::Bell));
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
    // Create a square LFO pattern using StepUp shape (1 cycle per step)
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        _steps[i].setShape(static_cast<int>(Curve::StepUp));
    }
}

void CurveSequence::populateWithRandomMinMax(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        _steps[i].setMin(std::rand() % (Min::Max + 1));
        _steps[i].setMax(std::rand() % (Max::Max + 1));
    }
}

void CurveSequence::populateWithMacroInit(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        _steps[i].setMin(0);
        _steps[i].setMax(255);
    }
}

void CurveSequence::populateWithMacroFm(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    float stepCount = float(maxStep - minStep + 1);
    
    const float freqMult = 8.0f; // Fixed multiplier for smooth acceleration

    for (int i = minStep; i <= maxStep; ++i) {
        auto eval = [&] (float t) {
            // Chirp signal using Triangle basis
            float phase = std::fmod(t * t * freqMult, 1.0f);
            return Curve::eval(Curve::Triangle, phase);
        };
        _steps[i].setMinNormalized(eval(float(i - minStep) / stepCount));
        _steps[i].setMaxNormalized(eval(float(i - minStep + 1) / stepCount));
        _steps[i].setShape(static_cast<int>(Curve::RampUp));
    }
}

void CurveSequence::populateWithMacroDamp(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    float stepCount = float(maxStep - minStep + 1);
    const float cycles = 4.0f;

    for (int i = minStep; i <= maxStep; ++i) {
        auto eval = [&] (float t) {
            return 0.5f + 0.5f * std::sin(t * 2.0f * 3.1415926536f * cycles) * (1.0f - t);
        };
        _steps[i].setMinNormalized(eval(float(i - minStep) / stepCount));
        _steps[i].setMaxNormalized(eval(float(i - minStep + 1) / stepCount));
        _steps[i].setShape(static_cast<int>(Curve::RampUp));
    }
}

void CurveSequence::populateWithMacroBounce(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    float stepCount = float(maxStep - minStep + 1);
    const float bounces = 4.0f;

    for (int i = minStep; i <= maxStep; ++i) {
        auto eval = [&] (float t) {
            return std::abs(std::sin(t * M_PI * bounces)) * (1.0f - t);
        };
        _steps[i].setMinNormalized(eval(float(i - minStep) / stepCount));
        _steps[i].setMaxNormalized(eval(float(i - minStep + 1) / stepCount));
        _steps[i].setShape(static_cast<int>(Curve::RampUp));
    }
}

void CurveSequence::populateWithRasterizedShape(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    float stepCount = float(maxStep - minStep + 1);

    if (stepCount < 2.f) {
        return;
    }

    // Source shape is taken from the first step
    const auto &sourceStep = _steps[minStep];
    Curve::Type sourceShape = static_cast<Curve::Type>(sourceStep.shape());
    float sourceMin = sourceStep.minNormalized();
    float sourceMax = sourceStep.maxNormalized();

    for (int i = minStep; i <= maxStep; ++i) {
        float phaseStart = float(i - minStep) / stepCount;
        float phaseEnd = float(i - minStep + 1) / stepCount;

        // Evaluate the source shape at the corresponding phase
        float valStartRaw = Curve::eval(sourceShape, phaseStart);
        float valEndRaw = Curve::eval(sourceShape, phaseEnd);

        // Scale to source step's Min/Max range
        // Since we support inversion (Min > Max), this logic holds:
        // val = min + raw * (max - min)
        float valStart = sourceMin + valStartRaw * (sourceMax - sourceMin);
        float valEnd = sourceMin + valEndRaw * (sourceMax - sourceMin);

        _steps[i].setMinNormalized(valStart);
        _steps[i].setMaxNormalized(valEnd);
        _steps[i].setShape(static_cast<int>(Curve::RampUp));
    }
}

void CurveSequence::transformInvert(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        int m = _steps[i].min();
        _steps[i].setMin(_steps[i].max());
        _steps[i].setMax(m);
    }
}

void CurveSequence::transformReverse(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int stepCount = maxStep - minStep + 1;

    if (stepCount < 2) {
        transformInvert(minStep, maxStep);
        return;
    }

    // Copy range
    Step temp[CONFIG_STEP_COUNT];
    for (int i = 0; i < stepCount; ++i) {
        temp[i] = _steps[minStep + i];
    }

    // Write back reversed
    for (int i = 0; i < stepCount; ++i) {
        auto &target = _steps[minStep + i];
        target = temp[stepCount - 1 - i];
        // Reverse internal direction too
        int m = target.min();
        target.setMin(target.max());
        target.setMax(m);
    }
}

void CurveSequence::transformHumanize(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        int jitterMin = (std::rand() % 11) - 5; // -5 to +5
        int jitterMax = (std::rand() % 11) - 5;
        _steps[i].setMin(clamp(_steps[i].min() + jitterMin, 0, 255));
        _steps[i].setMax(clamp(_steps[i].max() + jitterMax, 0, 255));
    }
}

void CurveSequence::transformAlign(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    // Starting from the second step in the range, align its Min to previous step's Max
    for (int i = minStep + 1; i <= maxStep; ++i) {
        _steps[i].setMin(_steps[i - 1].max());
    }
}

void CurveSequence::transformSmoothWalk(int firstStep, int lastStep) {
    int minStep = clamp(std::min(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);
    int maxStep = clamp(std::max(firstStep, lastStep), 0, CONFIG_STEP_COUNT - 1);

    for (int i = minStep; i <= maxStep; ++i) {
        int startVal = (i == minStep) ? _steps[i].min() : _steps[i - 1].max();
        int delta = (std::rand() % 121) - 60; // -60 to +60
        
        _steps[i].setMin(startVal);
        _steps[i].setMax(clamp(startVal + delta, 0, 255));
        _steps[i].setShape(static_cast<int>(Curve::SmoothUp));
    }
}
