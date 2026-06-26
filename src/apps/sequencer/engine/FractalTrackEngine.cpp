#include "FractalTrackEngine.h"
#include "Engine.h"

#include "model/FractalSequence.h"
#include "model/FractalTrack.h"
#include "model/Model.h"

#include "core/math/Math.h"

#include <cmath>
#include <algorithm>

#ifndef PLATFORM_STM32
#include <cassert>
#endif

// CV encoding constants (KD-2): signed 11-bit c in [-1024,1023] spans ±60
// semitones (±5 octaves) about the project root.
static constexpr float kSemitonesPerOctave = 12.f;
static constexpr float kCvRangeSemitones = 60.f;
static constexpr float kCvCodeScale = 1024.f;

uint16_t FractalTrackEngine::encodeCell(float semitonesRelRoot, int gateLen, bool valid) {
    int c = int(std::lround(semitonesRelRoot * kCvCodeScale / kCvRangeSemitones));
    c = clamp(c, -1024, 1023);
    gateLen = clamp(gateLen, 0, 15);
    uint16_t cell = uint16_t(c & 0x7ff);
    cell |= uint16_t((gateLen & 0xf) << 11);
    if (valid) cell |= 0x8000;
    return cell;
}

void FractalTrackEngine::decodeCell(uint16_t cell, float &semitonesRelRoot, int &gateLen, bool &valid) {
    int c = cell & 0x7ff;
    if (c & 0x400) c -= 0x800;   // sign-extend 11-bit
    semitonesRelRoot = float(c) * kCvRangeSemitones / kCvCodeScale;
    gateLen = (cell >> 11) & 0xf;
    valid = (cell & 0x8000) != 0;
}

FractalTrackEngine::FractalTrackEngine(Engine &engine, const Model &model, Track &track) :
    TrackEngine(engine, model, track),
    _fractalTrack(track.fractalTrack())
{
#ifndef PLATFORM_STM32
    // Codec round-trip self-check: gateLen exact, CV within one LSB.
    {
        float st; int gl; bool v;
        decodeCell(encodeCell(0.f, 8, true), st, gl, v);
        assert(gl == 8 && v && std::abs(st) < 0.06f);
        decodeCell(encodeCell(12.f, 15, true), st, gl, v);
        assert(gl == 15 && std::abs(st - 12.f) < 0.06f);
        decodeCell(encodeCell(-24.f, 1, true), st, gl, v);
        assert(gl == 1 && std::abs(st + 24.f) < 0.06f);
        decodeCell(encodeCell(0.f, 0, false), st, gl, v);
        assert(gl == 0 && !v);
        // Out-of-range clamps to ±5 octaves.
        decodeCell(encodeCell(100.f, 8, true), st, gl, v);
        assert(st > 59.f);
    }
#endif
    reset();
}

void FractalTrackEngine::reset() {
    _sequence = &(_fractalTrack.sequence(pattern()));
    _relativeTick = 0;
    _recordPos = sequence().recordFirst();
    _readPos = sequence().loopFirst();
    _gate = false;
    _cv = 0.f;
    _activity = false;
    _gateActive = false;
    _gateOffTick = 0;
    for (auto &cell : _trunk) cell = 0;
}

void FractalTrackEngine::restart() {
    _relativeTick = 0;
    _recordPos = sequence().recordFirst();
    _readPos = sequence().loopFirst();
}

void FractalTrackEngine::stop() {
    _gate = false;
    _gateActive = false;
    _activity = false;
}

uint32_t FractalTrackEngine::effectiveDivisor() const {
    const auto &seq = sequence();
    float clockMult = seq.clockMultiplier() * 0.01f;
    if (clockMult <= 0.f) clockMult = 1.f;
    uint32_t divisor = uint32_t(seq.divisor()) * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    return std::max<uint32_t>(1u, uint32_t(std::lround(divisor / clockMult)));
}

TrackEngine::TickResult FractalTrackEngine::tick(uint32_t tick) {
    ASSERT(_sequence != nullptr, "invalid sequence");
    const auto &seq = sequence();

    uint32_t divisor = effectiveDivisor();

    uint32_t resetDivisor = seq.resetMeasure() * _engine.measureDivisor();
    uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;
    if (resetDivisor > 0 && relativeTick == 0) {
        _recordPos = seq.recordFirst();
        _readPos = seq.loopFirst();
    }

    TickResult result = TickResult::NoUpdate;

    if (relativeTick % divisor == 0) {
        advanceSection(tick, divisor);
        result |= TickResult::GateUpdate | TickResult::CvUpdate;
    }

    // Drop the held gate when its proportional length elapses.
    if (_gateActive && tick >= _gateOffTick) {
        _gate = false;
        _gateActive = false;
        result |= TickResult::GateUpdate;
    }

    return result;
}

void FractalTrackEngine::advanceSection(uint32_t tick, uint32_t divisor) {
    const auto &seq = sequence();

    // Capture (KD-1). Read the parent's currently-emitted gate+CV at the
    // boundary (sample-and-hold — MVP fidelity). Note: this reads the
    // parent's output for THIS tick, so a parent at a higher track index is
    // effectively one section stale. Acceptable for MVP.
    bool armed = seq.recordTrigger() != 0;
    int srcA = _fractalTrack.sourceA();
    if (armed && !_fractalTrack.lock() && srcA >= 0) {
        captureSection();
    }

    replaySection(tick, divisor);
}

void FractalTrackEngine::captureSection() {
    const auto &seq = sequence();
    int srcA = _fractalTrack.sourceA();

    const TrackEngine &parent = _engine.trackEngine(srcA);
    bool parentGate = parent.gateOutput(0);
    float parentCv = parent.cvOutput(0);   // volts, 1V/oct, rel project root

    float semitonesRelRoot = parentCv * kSemitonesPerOctave;
    // gate high → fixed proportional gateLen (MVP); rest → gateLen 0.
    // TODO(fractal): observe-over-section gate length + onset (KD-1/14b).
    int gateLen = parentGate ? 8 : 0;
    _trunk[_recordPos] = encodeCell(semitonesRelRoot, gateLen, true);

    int first = seq.recordFirst();
    int last = seq.recordLast();
    if (last < first) last = first;
    if (_recordPos >= last) _recordPos = first;
    else _recordPos++;
}

void FractalTrackEngine::replaySection(uint32_t tick, uint32_t divisor) {
    const auto &seq = sequence();

    int first = seq.loopFirst();
    int last = seq.loopLast();
    if (last < first) last = first;
    int windowSize = last - first + 1;

    // Forward run with rotate() offset applied within the window.
    int pos = _readPos;
    if (seq.rotate() != 0 && windowSize > 0) {
        int offset = (_readPos - first + seq.rotate()) % windowSize;
        if (offset < 0) offset += windowSize;
        pos = first + offset;
    }
    pos = clamp(pos, 0, CONFIG_FRACTAL_MAX_CELLS - 1);

    float semitonesRelRoot; int gateLen; bool valid;
    decodeCell(_trunk[pos], semitonesRelRoot, gateLen, valid);

    // Octave/transpose applied at playback (octave = 1V; transpose semitones
    // → volts). Trunk stays raw — captured CV is never quantized (KD-13).
    float cv = semitonesRelRoot * (1.f / kSemitonesPerOctave);
    cv += float(_fractalTrack.octave());
    cv += float(_fractalTrack.transpose()) * (1.f / kSemitonesPerOctave);
    _cv = cv;

    if (gateLen > 0 && valid) {
        _gate = true;
        _gateActive = true;
        _activity = true;
        // Hold gate for gateLen/15 of the section, scheduled off the section
        // tick. Mirrors NoteTrack's proportional gate-length handling.
        uint32_t holdTicks = std::max<uint32_t>(1u, (divisor * uint32_t(gateLen)) / 15u);
        _gateOffTick = tick + holdTicks;
    } else {
        _gate = false;
        _gateActive = false;
        _activity = false;
    }

    // Advance Forward through loopFirst..loopLast.
    if (_readPos >= last) _readPos = first;
    else _readPos++;
}

void FractalTrackEngine::update(float dt) {
    (void)dt;
}
