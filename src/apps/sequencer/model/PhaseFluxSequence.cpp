#include "PhaseFluxSequence.h"

namespace {
    struct PitchRatio { int num; int den; };
    // Ordered from slowest drift (long repeat) to fastest. 1:1 = locked.
    constexpr PitchRatio kPitchRatios[PhaseFluxSequence::kPitchRateCount] = {
        {1,16}, {1,8}, {1,4}, {1,3}, {3,8}, {1,2}, {2,3}, {3,4}, {7,8},
        {1,1},
        {8,7}, {4,3}, {3,2}, {2,1}, {3,1}, {4,1}, {8,1},
    };
    constexpr int kDefaultPitchRateIndex = 9;  // 1:1
}

int PhaseFluxSequence::pitchRateNum(int index) {
    return kPitchRatios[clamp(index, 0, kPitchRateCount - 1)].num;
}
int PhaseFluxSequence::pitchRateDen(int index) {
    return kPitchRatios[clamp(index, 0, kPitchRateCount - 1)].den;
}
int PhaseFluxSequence::defaultPitchRateIndex() {
    return kDefaultPitchRateIndex;
}

void PhaseFluxSequence::Stage::clear() {
    _data0.raw = 0;
    _data1.raw = 0;
    _data2.raw = 0;
    _data3.raw = 0;
    setPulseCount(1);
    setBasePitch(0);
    setPitchRange(PitchRangeType::One);
    setPitchDirection(PitchDirectionType::Up);
    setTemporalCurve(TemporalCurveType::Linear);
    setTemporalFlipV(false);
    setTemporalFlipH(false);
    setTemporalWarp(0);
    setTemporalResponse(0);
    setPitchCurve(PitchCurveType::Ramp);
    setPitchFlipV(false);
    setPitchFlipH(false);
    setPitchWarp(0);
    setPitchResponse(0);
    setMaskMelody(100);
    setTiltMelody(0);
    setPhaseShift(0);
    setMask(MaskType::Off);
    setMaskShift(0);
    setAccumulatorStep(0);
    setAccumulatorLength(1);
    setGateLength(50);
    setStageDivisor(StageDivisorSlot::Div1_16);
    setSkip(false);
    setStageLen(64);   // 64 = ×1 transparent default; sequence runs unchanged when stageLen is added
}

void PhaseFluxSequence::Stage::write(VersionedSerializedWriter &writer) const {
    writer.write(_data0.raw);
    writer.write(_data1.raw);
    writer.write(_data2.raw);
    writer.write(_data3.raw);
}

void PhaseFluxSequence::Stage::read(VersionedSerializedReader &reader) {
    reader.read(_data0.raw);
    reader.read(_data1.raw);
    reader.read(_data2.raw);
    reader.read(_data3.raw);
}

void PhaseFluxSequence::clear() {
    _scale = -1;
    _rootNote = -1;
    _resetMeasure = 0;
    _edited = 0;
    _pitchRate = uint8_t(defaultPitchRateIndex());
    _pitchMode = PitchMode::Cell;
    _globalPhase = 0.f;
    _divisor.setBase(12);                // 1/16 at PPQN 48
    _clockMultiplier.setBase(100);
    for (auto &stage : _stages) {
        stage.clear();
    }
}

void PhaseFluxSequence::write(VersionedSerializedWriter &writer) const {
    writer.write(_scale);
    writer.write(_rootNote);
    writer.write(_resetMeasure);
    writer.write(_pitchRate);
    writer.write(static_cast<uint8_t>(_pitchMode));
    writer.write(_globalPhase);
    _divisor.write(writer);
    _clockMultiplier.write(writer);
    for (const auto &stage : _stages) {
        stage.write(writer);
    }
}

void PhaseFluxSequence::read(VersionedSerializedReader &reader) {
    reader.read(_scale);
    reader.read(_rootNote);
    reader.read(_resetMeasure);
    reader.read(_pitchRate);
    uint8_t pitchMode;
    reader.read(pitchMode);
    _pitchMode = pitchMode < uint8_t(PitchMode::Last) ? static_cast<PitchMode>(pitchMode) : PitchMode::Cell;
    reader.read(_globalPhase);
    _divisor.read(reader);
    _clockMultiplier.read(reader);
    _pitchRate = clamp(int(_pitchRate), 0, kPitchRateCount - 1);
    _scale = clamp(int(_scale), -1, Scale::Count - 1);
    _rootNote = clamp(int(_rootNote), -1, 11);
    _resetMeasure = clamp(int(_resetMeasure), 0, 128);
    uint16_t d = _divisor.base;
    _divisor.setBase(ModelUtils::clampDivisor(d));
    _clockMultiplier.setBase(clamp(int(_clockMultiplier.base), 50, 150));
    for (auto &stage : _stages) {
        stage.read(reader);
    }
}
