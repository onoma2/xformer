#include "PhaseFluxSequence.h"

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
    reader.read(_globalPhase);
    _divisor.read(reader);
    _clockMultiplier.read(reader);
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
