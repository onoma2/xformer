#include "AlgoGenerator.h"

#include "SequenceBuilder.h"

#include "core/math/Math.h"

#include <ctime>

static const char *kAlgorithmNames[] = {
    "TEST", "TRITRANCE", "STOMPER", "MARKOV", "CHIP1",
    "CHIP2", "WOBBLE", "SCALEWLK", "WINDOW", "MINIMAL",
    "GANZ", "BLAKE", "APHEX", "AUTECHRE", "STEPWAVE"
};

AlgoGenerator::AlgoGenerator(SequenceBuilder &builder, Params &params) :
    Generator(builder),
    _params(params)
{
    _pattern.fill(0);
    update();
}

const char *AlgoGenerator::paramName(int index) const {
    switch (Param(index)) {
    case Param::Seed:      return "Seed";
    case Param::Algorithm: return "Algo";
    case Param::Flow:      return "Flow";
    case Param::Ornament:  return "Ornmt";
    case Param::Power:     return "Power";
    case Param::Last:      break;
    }
    return nullptr;
}

void AlgoGenerator::editParam(int index, int value, bool shift) {
    (void)shift;
    switch (Param(index)) {
    case Param::Seed:
        if (value != 0) {
            randomizeSeed();
        }
        break;
    case Param::Algorithm:
        setAlgorithm(algorithm() + value);
        break;
    case Param::Flow:
        setFlow(flow() + value);
        break;
    case Param::Ornament:
        setOrnament(ornament() + value);
        break;
    case Param::Power:
        setPower(power() + value);
        break;
    case Param::Last:
        break;
    }
}

void AlgoGenerator::printParam(int index, StringBuilder &str) const {
    switch (Param(index)) {
    case Param::Seed:
        str("%08X", _params.seed);
        break;
    case Param::Algorithm:
        if (_params.algorithm < 15) {
            str(kAlgorithmNames[_params.algorithm]);
        } else {
            str("%d", _params.algorithm);
        }
        break;
    case Param::Flow:
        str("%d", flow());
        break;
    case Param::Ornament:
        str("%d", ornament());
        break;
    case Param::Power:
        str("%d", power());
        break;
    case Param::Last:
        break;
    }
}

void AlgoGenerator::init() {
    _params.seed = 0;
    _params.algorithm = 0;
    _params.flow = 8;
    _params.ornament = 8;
    _params.power = 8;
    _params.variation = 100;
    update();
}

void AlgoGenerator::randomizeParams() {
    randomizeSeed();
    Random rng(_params.seed ^ 0xB5297A4Du);
    _params.flow = int(rng.nextRange(17));
    _params.ornament = int(rng.nextRange(17));
    _params.power = int(rng.nextRange(17));
    _params.algorithm = int(rng.nextRange(15));
    _params.variation = 100;
}

void AlgoGenerator::randomizeSeed() {
    static uint32_t entropy = 0;
    if (entropy == 0) {
        entropy = uint32_t(time(NULL)) ^ uint32_t(clock()) ^ 0xA341316Cu;
    }
    entropy = entropy * 1664525u + 1013904223u + uint32_t(clock());
    Random rng(entropy);
    _params.seed = rng.next();
    entropy = rng.next();
}

int AlgoGenerator::ornamentToSubdivisions(int ornament) const {
    int subdivisions = 4;
    if (ornament >= 5 && ornament <= 8) subdivisions = 3;
    else if (ornament >= 9 && ornament <= 12) subdivisions = 5;
    else if (ornament >= 13) subdivisions = 7;
    return subdivisions;
}

void AlgoGenerator::update() {
    AlgoParams algoParams;
    algoParams.algorithm = _params.algorithm;
    algoParams.flow = _params.flow;
    algoParams.ornament = _params.ornament;
    algoParams.glide = _params.variation / 2;
    algoParams.power = _params.power;
    algoParams.stepTrill = 0;
    algoParams.gateLength = 50;
    algoParams.seed = _params.seed;

    _algoCore.init(algoParams);

    Random blendRng(_params.seed ^ 0xFEEDC0DE);
    auto &seq = static_cast<SequenceBuilderImpl<NoteSequence> &>(_builder).editSequence();
    int stepCount = _builder.length();
    int firstStep = seq.firstStep();

    int coolDown = 0;
    int coolDownMax = 16 - _params.power;

    for (int i = 0; i < stepCount; ++i) {
        AlgoContext ctx;
        ctx.divisor = 1;
        ctx.tpb = 1;
        ctx.loopLength = stepCount;
        ctx.effectiveLoopLength = stepCount;
        ctx.rotatedStep = i;
        ctx.stepIndex = i;
        ctx.ornament = _params.ornament;
        ctx.subdivisions = ornamentToSubdivisions(_params.ornament);
        ctx.stepsPerBeat = 4;
        ctx.isBeatStart = (i % ctx.stepsPerBeat == 0);

        AlgoResult result = _algoCore.generate(_params.algorithm, ctx);

        auto &step = seq.step(firstStep + i);

        // Per-layer variation roll
        bool useGate  = int(blendRng.nextRange(100)) < _params.variation;
        bool useNote  = int(blendRng.nextRange(100)) < _params.variation;
        bool useSlide = int(blendRng.nextRange(100)) < _params.variation;
        bool useGateOffset = int(blendRng.nextRange(100)) < _params.variation;
        bool useLength = int(blendRng.nextRange(100)) < _params.variation;
        bool useRetrigger = int(blendRng.nextRange(100)) < _params.variation;

        // Gate decision: velocity > 0 && gateRatio > 0 && cooldown allows
        bool shouldGate = result.velocity > 0 && result.gateRatio > 0;
        if (shouldGate && coolDownMax > 0) {
            shouldGate = (--coolDown <= 0);
            if (shouldGate) {
                coolDown = std::max(1, int(blendRng.nextRange(coolDownMax + 1)));
            }
        } else if (shouldGate && coolDownMax == 0) {
            // power=16: all gates pass
        } else {
            shouldGate = false;
        }

        if (useGate) {
            step.setGate(shouldGate);
        }
        if (useNote && shouldGate) {
            step.setNote(result.note % 12);
        }
        if (useSlide && shouldGate) {
            step.setSlide(result.slide);
        }
        if (useGateOffset) {
            const auto &range = NoteSequence::layerRange(NoteSequence::Layer::GateOffset);
            step.setLayerValue(NoteSequence::Layer::GateOffset,
                range.min + result.gateOffset * (range.max - range.min) / 100);
        }
        if (useLength) {
            const auto &range = NoteSequence::layerRange(NoteSequence::Layer::Length);
            step.setLayerValue(NoteSequence::Layer::Length,
                range.min + result.gateRatio * (range.max - range.min) / 200);
        }
        if (useRetrigger) {
            const auto &range = NoteSequence::layerRange(NoteSequence::Layer::Retrigger);
            step.setLayerValue(NoteSequence::Layer::Retrigger,
                range.min + (result.trillCount - 1) * (range.max - range.min) / 3);
        }

        // Cache display value
        _pattern[i] = (result.note + result.octave * 12) * 255 / 127;
        if (_pattern[i] > 255) _pattern[i] = 255;
    }
}

int AlgoGenerator::displayValue(int index) const {
    if (!showingPreview()) {
        return clamp(int(_builder.originalValue(index) * 255.f + 0.5f), 0, 255);
    }
    return _pattern[index];
}
