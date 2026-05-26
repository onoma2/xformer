#include "PhaseFluxSequence.h"

namespace {
    struct PitchRatio { int num; int den; };
    // P:T ratios. Every non-1:1 entry drifts across full 16-cell cycles —
    // denominator has at least one odd factor, so 16 × num/den is non-integer
    // and cell-revisits land on a different pitch curve position each cycle.
    // Drift period (cycles to fully realign) noted alongside.
    constexpr PitchRatio kPitchRatios[PhaseFluxSequence::kPitchRateCount] = {
        { 1,  7},   //  0 — 0.143,  7-cycle drift
        { 1,  5},   //  1 — 0.200,  5-cycle drift
        { 1,  3},   //  2 — 0.333,  3-cycle drift
        { 2,  5},   //  3 — 0.400,  5-cycle drift
        { 3,  5},   //  4 — 0.600,  5-cycle drift
        { 5,  7},   //  5 — 0.714,  7-cycle drift
        { 7,  9},   //  6 — 0.778,  9-cycle drift
        {11, 12},   //  7 — 0.917,  3-cycle drift
        {16, 17},   //  8 — 0.941, 17-cycle drift (very slow evolve near 1:1)
        { 1,  1},   //  9 — locked baseline (DEFAULT)
        {18, 17},   // 10 — 1.059, 17-cycle drift (very slow evolve near 1:1)
        {13, 12},   // 11 — 1.083,  3-cycle drift
        {11,  9},   // 12 — 1.222,  9-cycle drift
        { 9,  7},   // 13 — 1.286,  7-cycle drift
        { 7,  5},   // 14 — 1.400,  5-cycle drift
        { 5,  3},   // 15 — 1.667,  3-cycle drift
        { 9,  5},   // 16 — 1.800,  5-cycle drift
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
