#include "PhaseFluxSequence.h"

#include "PhaseFluxMath.h"

#include <cmath>

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
    setPulseCount(0);
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
    setPulseAccumStep(0);
    setAccumulatorTrigger(AccumulatorTriggerType::Stage);
    setPulseAccumTrigger(AccumulatorTriggerType::Stage);
    setGateLength(50);
    setStageDivisor(StageDivisorSlot::Bar);
    setSkip(false);
    setStageLen(64);   // 64 = ×1 transparent default; sequence runs unchanged when stageLen is added
    setTemporalRepeat(RepeatType::x1);
    setPitchRepeat(RepeatType::x1);
    setTemporalWindow(WindowType::Off);
    setPitchWindow(WindowType::Off);
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

void PhaseFluxSequence::printTraversalPattern(StringBuilder &str) const {
    str(PhaseFluxMath::traversalPatternName(_traversalPattern));
}

void PhaseFluxSequence::snapToGrid(int beatTicks) {
    // 1. globalPhase snap to nearest 1/16 of cycle.
    const float step = 1.f / 16.f;
    _globalPhase = std::round(_globalPhase / step) * step;
    if (_globalPhase < 0.f) _globalPhase = 0.f;
    if (_globalPhase >= 1.f) _globalPhase -= 1.f;

    if (beatTicks <= 0) return;
    int seqDivisor = int(_divisor);
    if (seqDivisor <= 0) return;

    // 2. Per-stage stageLen → nearest whole project beat. Floor at 1 beat
    //    so cells never snap to silence. Silent cells (stageLen=0) preserved.
    for (auto &stage : _stages) {
        if (stage.skip()) continue;
        int currentLen = stage.stageLen();
        if (currentLen == 0) continue;
        int stageDivisorTicks = PhaseFluxMath::stageDivisorTicks(stage.stageDivisor());
        int cellTicksBase = (stageDivisorTicks * seqDivisor) / 12;
        if (cellTicksBase == 0) continue;
        int cellTicks = (cellTicksBase * currentLen) / 64;
        int targetBeats = (cellTicks + beatTicks / 2) / beatTicks;
        if (targetBeats < 1) targetBeats = 1;
        int targetTicks = targetBeats * beatTicks;
        int newStageLen = (targetTicks * 64) / cellTicksBase;
        if (newStageLen < 0) newStageLen = 0;
        if (newStageLen > 127) newStageLen = 127;
        stage.setStageLen(newStageLen);
    }
}

void PhaseFluxSequence::clear() {
    _scale = -1;
    _rootNote = -1;
    _resetMeasure = 0;
    _edited = 0;
    _pitchRate = uint8_t(defaultPitchRateIndex());
    _pitchMode = PitchMode::Cell;
    _cycleLength = CycleLengthMode::Fixed;
    _globalPhase = 0.f;
    _warpNudge = 0;
    _responseNudge = 0;
    _pulseNudge = 0;
    _lenNudge = 0;
    _cyclePhaseWarp = 0;
    _traversalPattern = 0;
    _divisor = 12;                       // 1/16 at PPQN 48
    _clockMultiplier = 100;
    _noteAccumConfig = AccumulatorConfig();
    _pulseAccumConfig = AccumulatorConfig();
    // Default to max span: note ±28, pulse ±16 (matching new pulseCount ceiling).
    _noteAccumConfig.setPosLim(28);
    _noteAccumConfig.setNegLim(28);
    _pulseAccumConfig.setPosLim(16);
    _pulseAccumConfig.setNegLim(16);
    for (size_t i = 0; i < _stages.size(); ++i) {
        _stages[i].clear();
        // Fresh sequence: only stage 0 active. Dial pulseCount on stage 0
        // up to 16 → matches NoteTrack 16-step 1/16 grid (since default
        // stageDivisor = Bar, seqDivisor = 12).
        if (i > 0) _stages[i].setSkip(true);
    }
}

void PhaseFluxSequence::write(VersionedSerializedWriter &writer) const {
    writer.write(_scale);
    writer.write(_rootNote);
    writer.write(_resetMeasure);
    // _pitchRate is 0..16 (5 bits). Pack cycleLength flag into bit 5 for
    // forward-compatible storage — old files read bit 5 = 0 = Adaptive,
    // preserving current playback exactly.
    {
        uint8_t packed = uint8_t(_pitchRate) & 0x1F;
        if (_cycleLength == CycleLengthMode::Fixed) packed |= 0x20;
        writer.write(packed);
    }
    writer.write(static_cast<uint8_t>(_pitchMode));
    writer.write(_globalPhase);
    writer.write(_warpNudge);
    writer.write(_responseNudge);
    writer.write(_pulseNudge);
    writer.write(_lenNudge);
    writer.write(_cyclePhaseWarp);
    writer.write(_traversalPattern);
    writer.write(_divisor);
    writer.write(_clockMultiplier);
    _noteAccumConfig.write(writer);
    _pulseAccumConfig.write(writer);
    for (const auto &stage : _stages) {
        stage.write(writer);
    }
}

void PhaseFluxSequence::read(VersionedSerializedReader &reader) {
    reader.read(_scale);
    reader.read(_rootNote);
    reader.read(_resetMeasure);
    // _pitchRate packs cycleLength in bit 5 (see write()). Old files have
    // bit 5 == 0 → Adaptive (preserves prior playback exactly).
    {
        uint8_t packed;
        reader.read(packed);
        _pitchRate = packed & 0x1F;
        _cycleLength = (packed & 0x20)
            ? CycleLengthMode::Fixed
            : CycleLengthMode::Adaptive;
    }
    uint8_t pitchMode;
    reader.read(pitchMode);
    _pitchMode = pitchMode < uint8_t(PitchMode::Last) ? static_cast<PitchMode>(pitchMode) : PitchMode::Cell;
    reader.read(_globalPhase);
    reader.read(_warpNudge);
    reader.read(_responseNudge);
    reader.read(_pulseNudge);
    reader.read(_lenNudge);
    reader.read(_cyclePhaseWarp);
    reader.read(_traversalPattern);
    reader.read(_divisor);
    reader.read(_clockMultiplier);
    _noteAccumConfig.read(reader);
    _pulseAccumConfig.read(reader);
    _pitchRate = clamp(int(_pitchRate), 0, kPitchRateCount - 1);
    _scale = clamp(int(_scale), -1, Scale::Count - 1);
    _rootNote = clamp(int(_rootNote), -1, 11);
    _resetMeasure = clamp(int(_resetMeasure), 0, 128);
    uint16_t d = _divisor;
    _divisor = ModelUtils::clampDivisor(d);
    _clockMultiplier = clamp(int(_clockMultiplier), 50, 150);
    for (auto &stage : _stages) {
        stage.read(reader);
    }
}
