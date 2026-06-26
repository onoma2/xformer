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

// KD-12 branch transform op codes (canonical assignment + pool-bit order).
enum BranchOp {
    OpTranspose = 0, OpReverse, OpInverse, OpRetInverse,
    OpRotate, OpCompress, OpExpand, OpGateThin,
};
static constexpr int kNumBranchOps = 8;
static constexpr int kTransposeIntervals[] = { 5, 7, 12 };   // P4, P5, octave
static constexpr int kCompressFactorsX100[] = { 50, 67 };    // f < 1
static constexpr int kExpandFactorsX100[] = { 150, 200 };    // f > 1

// Deterministic 32-bit hash → float in [0,1). Ports the sim's hash01(seed,s,p):
// the seeded transform assignment + per-op params stay stable, no RNG content.
static float hash01(uint32_t seed, uint32_t s, uint32_t p) {
    uint32_t h = seed * 2654435761u + s * 40503u + p * 2246822519u;
    h ^= h >> 15; h *= 2246822519u;
    h ^= h >> 13; h *= 3266489917u;
    h ^= h >> 16;
    return float(h) / 4294967296.f;
}

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

#ifndef PLATFORM_STM32
    // KD-12 branch self-check (mirrors harness b1/b3). Save the sequence fields
    // we touch, run on a known ramp trunk, restore.
    {
        FractalSequence &seq = sequence();
        int sf = seq.loopFirst(), sl = seq.loopLast(), sr = seq.rotate();
        int sc = seq.branchCount(), ssd = seq.branchSeed(), sp = seq.branchPool();
        seq.setLoopFirst(0); seq.setLoopLast(7); seq.setRotate(0); seq.setBranchPool(0xff);
        for (int i = 0; i < 8; ++i) _trunk[i] = encodeCell(float(i), 8, true);

        float st; int gl; bool v;
        // b1: branchCount 0 → trunk segment equals raw trunk cells.
        seq.setBranchCount(0); rebuildBranchTransforms();
        for (int p = 0; p < 8; ++p) {
            segmentCell(0, p, st, gl, v);
            assert(std::abs(st - float(p)) < 0.06f && v && gl == 8);
        }
        // b3: a Reverse-transform branch reverses the previous segment.
        seq.setBranchCount(3);
        int seed = 1; bool found = false;
        for (; seed < 4000 && !found; ++seed) {
            seq.setBranchSeed(seed); rebuildBranchTransforms();
            if (_branches[0].kind == OpReverse) found = true;
        }
        assert(found);
        for (int p = 0; p < 8; ++p) {
            segmentCell(1, p, st, gl, v);
            assert(std::abs(st - float(7 - p)) < 0.06f);
        }

        seq.setLoopFirst(sf); seq.setLoopLast(sl); seq.setRotate(sr);
        seq.setBranchCount(sc); seq.setBranchSeed(ssd); seq.setBranchPool(sp);
    }
    reset();
#endif
}

void FractalTrackEngine::reset() {
    _sequence = &(_fractalTrack.sequence(pattern()));
    _relativeTick = 0;
    _recordPos = sequence().recordFirst();
    _readPos = sequence().loopFirst();
    _globalPos = 0;
    _branchHash = -1;
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
    _globalPos = 0;
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
        _globalPos = 0;
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

int FractalTrackEngine::loopLen() const {
    const auto &seq = sequence();
    int first = seq.loopFirst();
    int last = seq.loopLast();
    if (last < first) last = first;
    return last - first + 1;
}

// Forward+rotate lens: within-position 0..loopLen-1 → trunk read index (MVP order).
int FractalTrackEngine::trunkReadIndex(int within) const {
    const auto &seq = sequence();
    int first = seq.loopFirst();
    int len = loopLen();
    int s = ((within % len) + len) % len;
    if (seq.rotate() != 0) {
        s = (s + (seq.rotate() % len) + len) % len;
    }
    return clamp(first + s, 0, CONFIG_FRACTAL_MAX_CELLS - 1);
}

float FractalTrackEngine::inversionCenter() const {
    const auto &seq = sequence();
    int idx = clamp(seq.loopFirst(), 0, CONFIG_FRACTAL_MAX_CELLS - 1);
    if (cellValid(idx)) {
        float st; int gl; bool v;
        decodeCell(_trunk[idx], st, gl, v);
        return st;
    }
    return 0.f;   // empty trunk → root (rel-root 0)
}

// Derive one chained transform per branch from branchSeed, drawn only from the
// enabled branchPool (empty → all Transpose). Per-op params are seed-derived.
void FractalTrackEngine::rebuildBranchTransforms() {
    const auto &seq = sequence();
    uint32_t seed = uint32_t(seq.branchSeed());
    int pool = seq.branchPool();
    int len = loopLen();

    int eligible[kNumBranchOps]; int nEligible = 0;
    for (int k = 0; k < kNumBranchOps; ++k) {
        if ((pool >> k) & 1) eligible[nEligible++] = k;
    }
    if (nEligible == 0) { eligible[0] = OpTranspose; nEligible = 1; }

    int n = clamp(seq.branchCount(), 0, 7);
    for (int s = 1; s <= n; ++s) {
        BranchTransform &b = _branches[s - 1];
        b.kind = uint8_t(eligible[int(hash01(seed, s, 0) * nEligible) % nEligible]);
        b.t = 0; b.k = 0; b.fx100 = 100; b.n = 2;
        if (b.kind == OpTranspose) {
            int mag = kTransposeIntervals[int(hash01(seed, s, 1) * 3) % 3];
            b.t = int8_t((hash01(seed, s, 2) < 0.5f ? -1 : 1) * mag);
        } else if (b.kind == OpRotate) {
            b.k = uint8_t(len > 1 ? (1 + int(hash01(seed, s, 3) * (len - 1))) : 0);
        } else if (b.kind == OpCompress) {
            b.fx100 = uint16_t(kCompressFactorsX100[int(hash01(seed, s, 4) * 2) % 2]);
        } else if (b.kind == OpExpand) {
            b.fx100 = uint16_t(kExpandFactorsX100[int(hash01(seed, s, 4) * 2) % 2]);
        } else if (b.kind == OpGateThin) {
            b.n = uint8_t(2 + int(hash01(seed, s, 5) * 3));   // 2..4
        }
    }
    _branchHash = int((seed * 131u + uint32_t(pool) * 17u + uint32_t(len)) & 0x7fffffff);
    _branchCountCache = n;
}

void FractalTrackEngine::maybeRebuildBranchTransforms() {
    const auto &seq = sequence();
    int sig = int((uint32_t(seq.branchSeed()) * 131u
                   + uint32_t(seq.branchPool()) * 17u + uint32_t(loopLen())) & 0x7fffffff);
    if (sig != _branchHash || clamp(seq.branchCount(), 0, 7) != _branchCountCache) {
        rebuildBranchTransforms();
    }
}

// Resolve segment seg (0 = trunk) at within-position pos by applying the chained
// transforms onto the trunk cell. Recursive — depth ≤ branchCount ≤ 7. The trunk
// buffer is never mutated (branches transform on read).
void FractalTrackEngine::segmentCell(int seg, int pos, float &semitonesRelRoot, int &gateLen, bool &valid) const {
    int len = loopLen();
    pos = ((pos % len) + len) % len;
    if (seg == 0) {
        decodeCell(_trunk[trunkReadIndex(pos)], semitonesRelRoot, gateLen, valid);
        return;
    }
    const BranchTransform &tf = _branches[seg - 1];
    if (tf.kind == OpReverse) {
        segmentCell(seg - 1, len - 1 - pos, semitonesRelRoot, gateLen, valid);
        return;
    }
    if (tf.kind == OpRetInverse) {
        segmentCell(seg - 1, len - 1 - pos, semitonesRelRoot, gateLen, valid);
        semitonesRelRoot = 2.f * inversionCenter() - semitonesRelRoot;
        return;
    }
    if (tf.kind == OpRotate) {
        segmentCell(seg - 1, pos + tf.k, semitonesRelRoot, gateLen, valid);
        return;
    }
    segmentCell(seg - 1, pos, semitonesRelRoot, gateLen, valid);
    if (tf.kind == OpTranspose) {
        semitonesRelRoot += float(tf.t);
    } else if (tf.kind == OpInverse) {
        semitonesRelRoot = 2.f * inversionCenter() - semitonesRelRoot;
    } else if (tf.kind == OpCompress || tf.kind == OpExpand) {
        float center = inversionCenter();
        semitonesRelRoot = center + std::round((semitonesRelRoot - center) * float(tf.fx100) * 0.01f);
    } else if (tf.kind == OpGateThin) {
        if ((pos + 1) % tf.n == 0) gateLen = 0;   // periodic rest, CV untouched
    }
}

// KD-12 Path navigation order. Returns the segment-index play order into route[]:
// trunk first, then outward branches (bit 0) ascending, then held branches
// (bit 1) descending. Every segment appears exactly once.
static int routeOf(int path, int n, int routeIndex) {
    // outward (bit=0) ascending, then held (bit=1) descending; index 0 = trunk.
    if (routeIndex == 0) return 0;
    int seen = 0;
    for (int b = 1; b <= n; ++b) {            // outward ascending
        if (!((path >> (b - 1)) & 1)) { if (++seen == routeIndex) return b; }
    }
    for (int b = n; b >= 1; --b) {            // held descending
        if ((path >> (b - 1)) & 1) { if (++seen == routeIndex) return b; }
    }
    return 0;
}

void FractalTrackEngine::replaySection(uint32_t tick, uint32_t divisor) {
    const auto &seq = sequence();

    maybeRebuildBranchTransforms();

    int n = clamp(seq.branchCount(), 0, 7);
    int len = loopLen();
    int total = len * (1 + n);
    int gp = _globalPos % total;

    // KD-12: global walk position → (segment via Path route, within-position).
    int routeIndex = clamp(gp / len, 0, n);
    int within = gp % len;
    int segment = routeOf(seq.path(), n, routeIndex);

    float semitonesRelRoot; int gateLen; bool valid;
    segmentCell(segment, within, semitonesRelRoot, gateLen, valid);

    _readPos = uint8_t(trunkReadIndex(within));   // UI highlight = trunk index read

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

    _globalPos = uint16_t((gp + 1) % total);
}

void FractalTrackEngine::update(float dt) {
    (void)dt;
}
