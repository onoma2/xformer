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

// KD-13 ornament ids, grouped by intensity tier (graded complexity ramp).
// Ported from the sim's ORN_2STEP / ORN_4STEP / ORN_TRILL.
enum Ornament {
    OrnAnticipation = 0, OrnSuspension, OrnSyncopation, OrnOctaveUp, OrnFifthUp,
    OrnHalfTurnToward, OrnHalfTurnAway,                                  // 2-step
    OrnRunToward, OrnRunAway, OrnTurn, OrnArpToward, OrnArpAway,
    OrnMordentUp, OrnMordentDown,                                       // 4-step
    OrnTrill,                                                           // trill
};
static constexpr int kOrn2StepCount = 7;    // ids 0..6
static constexpr int kOrn4StepCount = 7;    // ids 7..13
static constexpr int kOrnTrillCount = 1;    // id 14

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
    _fractalTrack(track.fractalTrack()),
    _rng(0x6f7261 + track.trackIndex())
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

    // KD-13 ornament self-checks: rate 0 never fires; the eligible pool grows
    // by intensity tier; lock freezes the roll to the last-fired ornament.
    {
        for (int i = 0; i < 200; ++i) assert(rollOrnament(0, 100) == -1);

        int ids[kOrn2StepCount + kOrn4StepCount + kOrnTrillCount];
        assert(eligibleOrnaments(ids, 0) == kOrn2StepCount);
        assert(eligibleOrnaments(ids, 39) == kOrn2StepCount);
        assert(eligibleOrnaments(ids, 40) == kOrn2StepCount + kOrn4StepCount);
        assert(eligibleOrnaments(ids, 74) == kOrn2StepCount + kOrn4StepCount);
        assert(eligibleOrnaments(ids, 100) == kOrn2StepCount + kOrn4StepCount + kOrnTrillCount);

        // Lock freeze: prime _lastOrnament unlocked, then locked rolls hold it.
        _lastOrnament = OrnTrill;
        bool wasLocked = _fractalTrack.lock();
        _fractalTrack.setLock(true);
        for (int i = 0; i < 50; ++i) assert(rollOrnament(100, 100) == OrnTrill);
        _fractalTrack.setLock(wasLocked);
        _lastOrnament = -1;
    }
    reset();

    // KD-20 Pack: over 2N sections, recordSkip=1 writes half as many cells as
    // recordSkip=0. Replays captureSection's skip/write/reset counter rule.
    {
        auto writesOver = [](int skip, int sections) {
            int remaining = 0, writes = 0;
            for (int i = 0; i < sections; ++i) {
                if (remaining > 0) { remaining--; continue; }
                ++writes;
                remaining = skip;
            }
            return writes;
        };
        const int N = 32;
        assert(writesOver(0, 2 * N) == 2 * N);
        assert(writesOver(1, 2 * N) == N);
    }

    // KD-4 two-source mix: And gate is A&B, Sum cv is A+B; default A/A passes A.
    {
        auto sg = _fractalTrack.gateLogic();
        auto sc = _fractalTrack.cvLogic();
        _fractalTrack.setGateLogic(FractalTrack::GateLogic::A);
        _fractalTrack.setCvLogic(FractalTrack::CvLogic::A);
        assert(combineGate(true, false) == true);
        assert(std::abs(combineCv(2.f, 5.f, true, true) - 2.f) < 1e-4f);
        _fractalTrack.setGateLogic(FractalTrack::GateLogic::And);
        _fractalTrack.setCvLogic(FractalTrack::CvLogic::Sum);
        assert(combineGate(true, true) == true && combineGate(true, false) == false);
        assert(std::abs(combineCv(2.f, 5.f, true, true) - 7.f) < 1e-4f);
        _fractalTrack.setGateLogic(sg);
        _fractalTrack.setCvLogic(sc);
    }

    // KD-15 track-delay: delay 0 schedules the section's note immediately; delay 3
    // holds it for 3 sections, then surfaces it. Drives replaySection on a ramp
    // trunk with ornaments disabled (ornFirst > ornLast) so the plain note fires.
    {
        FractalSequence &seq = sequence();
        int sf = seq.loopFirst(), sl = seq.loopLast(), sr = seq.rotate();
        int sbc = seq.branchCount(), sof = seq.ornFirst(), sol = seq.ornLast();
        int std0 = _fractalTrack.trackDelay();
        seq.setLoopFirst(0); seq.setLoopLast(7); seq.setRotate(0);
        seq.setBranchCount(0); seq.setOrnFirst(1); seq.setOrnLast(0);
        for (int i = 0; i < 8; ++i) _trunk[i] = encodeCell(float(i + 1), 8, true);
        const uint32_t div = 12;

        // delay 0: each section schedules immediately (gate edges this call).
        _fractalTrack.setTrackDelay(0);
        _globalPos = 0; _orderState.reset(); clearSchedule(); clearDelayRing();
        replaySection(0, div);
        assert(_gateCount > 0 && _delayCount == 0);
        float cv0 = _cvEvents[0].cv;

        // delay 3: first 3 sections defer (ring fills, nothing scheduled); the
        // 4th surfaces section 0's note with the same playback CV.
        _fractalTrack.setTrackDelay(3);
        _globalPos = 0; _orderState.reset(); clearSchedule(); clearDelayRing();
        replaySection(0, div);   assert(_gateCount == 0 && _delayCount == 1);
        replaySection(0, div);   assert(_gateCount == 0 && _delayCount == 2);
        replaySection(0, div);   assert(_gateCount == 0 && _delayCount == 3);
        replaySection(0, div);
        assert(_gateCount > 0 && std::abs(_cvEvents[0].cv - cv0) < 1e-4f);

        seq.setLoopFirst(sf); seq.setLoopLast(sl); seq.setRotate(sr);
        seq.setBranchCount(sbc); seq.setOrnFirst(sof); seq.setOrnLast(sol);
        _fractalTrack.setTrackDelay(std0);
    }
    reset();
#endif
}

void FractalTrackEngine::reset() {
    _sequence = &(_fractalTrack.sequence(pattern()));
    _relativeTick = 0;
    _recordPos = sequence().recordFirst();
    _recordSkipRemaining = 0;
    _wasArmed = false;
    _readPos = sequence().loopFirst();
    _globalPos = 0;
    _orderState.reset();
    _branchHash = -1;
    _gate = false;
    _cv = 0.f;
    _activity = false;
    clearSchedule();
    clearDelayRing();
    _lastOrnament = -1;
    for (auto &cell : _trunk) cell = 0;
}

void FractalTrackEngine::restart() {
    _relativeTick = 0;
    _recordPos = sequence().recordFirst();
    _recordSkipRemaining = 0;
    _wasArmed = false;
    _readPos = sequence().loopFirst();
    _globalPos = 0;
    _orderState.reset();
    clearDelayRing();
}

void FractalTrackEngine::stop() {
    _gate = false;
    _activity = false;
    clearSchedule();
    clearDelayRing();
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
        _orderState.reset();
    }

    TickResult result = TickResult::NoUpdate;

    if (relativeTick % divisor == 0) {
        advanceSection(tick, divisor);
    }

    // Drain the sub-section CV/gate schedule at this tick. Each section fills a
    // main note plus any ornament flourish notes here, generated in time order.
    while (_cvHead < _cvCount && tick >= _cvEvents[_cvHead].tick) {
        _cv = _cvEvents[_cvHead].cv;
        ++_cvHead;
        result |= TickResult::CvUpdate;
    }
    while (_gateHead < _gateCount && tick >= _gateEvents[_gateHead].tick) {
        _gate = _gateEvents[_gateHead].gate;
        ++_gateHead;
        _activity = _gate;
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
    if (armed && !_wasArmed) _recordSkipRemaining = 0;   // fresh run starts on a write
    _wasArmed = armed;
    if (armed && !_fractalTrack.lock() && srcA >= 0) {
        captureSection();
    }

    replaySection(tick, divisor);
}

bool FractalTrackEngine::combineGate(bool a, bool b) const {
    switch (_fractalTrack.gateLogic()) {
    case FractalTrack::GateLogic::B:    return b;
    case FractalTrack::GateLogic::And:  return a && b;
    case FractalTrack::GateLogic::Or:   return a || b;
    case FractalTrack::GateLogic::Xor:  return a != b;
    case FractalTrack::GateLogic::Nand: return !(a && b);
    default:                            return a;   // A
    }
}

float FractalTrackEngine::combineCv(float aCv, float bCv, bool aGate, bool bGate) const {
    switch (_fractalTrack.cvLogic()) {
    case FractalTrack::CvLogic::B:     return bCv;
    case FractalTrack::CvLogic::Min:   return std::min(aCv, bCv);
    case FractalTrack::CvLogic::Max:   return std::max(aCv, bCv);
    case FractalTrack::CvLogic::Sum:   return aCv + bCv;
    case FractalTrack::CvLogic::Avg:   return (aCv + bCv) * 0.5f;
    case FractalTrack::CvLogic::Gated: return aGate ? aCv : (bGate ? bCv : aCv);
    default:                           return aCv;   // A
    }
}

void FractalTrackEngine::captureSection() {
    const auto &seq = sequence();
    int srcA = _fractalTrack.sourceA();

    // KD-20 Pack: skip recordSkip sections between writes; cells pack consecutively.
    if (_recordSkipRemaining > 0) {
        _recordSkipRemaining--;
        return;
    }

    const TrackEngine &parentA = _engine.trackEngine(srcA);
    bool aGate = parentA.gateOutput(0);
    float aCv = parentA.cvOutput(0);   // volts, 1V/oct, rel project root

    int srcB = _fractalTrack.sourceB();
    bool parentGate;
    float parentCv;
    if (srcB < 0) {
        parentGate = aGate;
        parentCv = aCv;
    } else {
        const TrackEngine &parentB = _engine.trackEngine(srcB);
        bool bGate = parentB.gateOutput(0);
        float bCv = parentB.cvOutput(0);
        parentGate = combineGate(aGate, bGate);
        parentCv = combineCv(aCv, bCv, aGate, bGate);
    }

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

    _recordSkipRemaining = uint8_t(seq.recordSkip());
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

    // KD-12: global walk position → segment via Path route. The within-loop read
    // order comes from the runMode-aware SequenceState (Forward stays byte-identical
    // to the old gp%len walk), not a hand-rolled forward+rotate.
    int routeIndex = clamp(gp / len, 0, n);
    int segment = routeOf(seq.path(), n, routeIndex);

    _orderState.advanceFree(seq.runMode(), 0, len - 1, _rng);
    int within = _orderState.step();
    // Lookahead within: a copy advanced one more section gives the next read order.
    SequenceState nextState = _orderState;
    nextState.advanceFree(seq.runMode(), 0, len - 1, _rng);
    int nextWithin = nextState.step();

    float semitonesRelRoot; int gateLen; bool valid;
    segmentCell(segment, within, semitonesRelRoot, gateLen, valid);

    _readPos = uint8_t(trunkReadIndex(within));   // UI highlight = trunk index read

    // Lookahead for direction-aware ornaments: the next sounding cell's raw note.
    float nextSemi; int nextGateLen; bool nextValid;
    segmentCell(segment, nextWithin, nextSemi, nextGateLen, nextValid);

    // Ornament eligibility (KD-13): sounding cell gated and trunk read index
    // inside the ornament zone. Rests and out-of-zone cells never ornament.
    int readIdx = trunkReadIndex(within);
    bool ornEligible = valid && gateLen > 0
        && readIdx >= seq.ornFirst() && readIdx <= seq.ornLast();

    int delay = _fractalTrack.trackDelay();
    if (delay <= 0) {
        // Immediate path (byte-identical to no-delay playback); ring stays empty.
        scheduleSection(tick, divisor, semitonesRelRoot, nextSemi, valid ? gateLen : 0, ornEligible);
    } else {
        // Surface notes due this section first, then defer this section's note.
        drainDelayRing(tick, divisor);
        pushDelay(semitonesRelRoot, valid ? gateLen : 0, ornEligible, delay);
    }

    _globalPos = uint16_t((gp + 1) % total);
}

// rel-root semitones → playback volts. octave = 1V; transpose semitones → volts.
// Trunk stays raw — captured CV is never quantized (KD-13). Ornament notes are
// already scale-snapped before reaching here; the main note is literal.
static float fractalPlaybackCv(float semitonesRelRoot, const FractalTrack &track) {
    float cv = semitonesRelRoot * (1.f / kSemitonesPerOctave);
    cv += float(track.octave());
    cv += float(track.transpose()) * (1.f / kSemitonesPerOctave);
    return cv;
}

// Nearest scale degree to a raw rel-root semitone (degree 0 = scale tonic).
// Ports the sim's nearestDegree by scanning degrees over ±1 octave.
int FractalTrackEngine::nearestDegree(float semitonesRelRoot) const {
    const auto scale = sequence().selectedScale(_model.project().scale(), _model.project().scaleRotate());
    int n = scale.notesPerOctave();
    float targetVolts = semitonesRelRoot * (1.f / kSemitonesPerOctave);
    int oct = int(std::floor(targetVolts));
    int base = oct * n;
    int best = base; float bestErr = 1e9f;
    for (int d = base - n; d <= base + 2 * n; ++d) {
        float err = std::abs(scale.noteToVolts(d) - targetVolts);
        if (err < bestErr) { bestErr = err; best = d; }
    }
    return best;
}

// Step `steps` scale degrees from the raw note's nearest degree → rel-root semitones.
float FractalTrackEngine::stepDegrees(float semitonesRelRoot, int steps) const {
    const auto scale = sequence().selectedScale(_model.project().scale(), _model.project().scaleRotate());
    return scale.noteToVolts(nearestDegree(semitonesRelRoot) + steps) * kSemitonesPerOctave;
}

// Fill ids[] with the ornament pool unlocked by intensity (0..100): 2-step
// always, +4-step at ≥40%, +trill at ≥75%. Returns the pool size.
int FractalTrackEngine::eligibleOrnaments(int ids[], int intensity) const {
    int k = 0;
    for (int i = 0; i < kOrn2StepCount; ++i) ids[k++] = OrnAnticipation + i;
    if (intensity >= 40) for (int i = 0; i < kOrn4StepCount; ++i) ids[k++] = OrnRunToward + i;
    if (intensity >= 75) for (int i = 0; i < kOrnTrillCount; ++i) ids[k++] = OrnTrill + i;
    return k;
}

// Roll an ornament. rate (0..100) = P(fire) per gated cell. Returns -1 (none) or
// an ornament id picked uniformly from the eligible tier. Lock freezes which one:
// Rate still gates whether a flourish fires, Lock holds the last-fired ornament.
int FractalTrackEngine::rollOrnament(int rate, int intensity) {
    if (rate <= 0) return -1;
    if (int(_rng.nextRange(100)) >= rate) return -1;
    if (_fractalTrack.lock()) {
        return _lastOrnament;   // frozen: repeat last-fired (may be -1 until first roll)
    }
    int ids[kOrn2StepCount + kOrn4StepCount + kOrnTrillCount];
    int pool = eligibleOrnaments(ids, intensity);
    int orn = ids[_rng.nextRange(uint32_t(pool))];
    _lastOrnament = orn;
    return orn;
}

// Push one mono note (gate-on at `at`, gate-off at at+dur, CV at `at`) onto the
// queues. Notes are emitted in time order so the queue stays a clean envelope.
void FractalTrackEngine::scheduleSection(uint32_t tick, uint32_t divisor, float mainSemi,
                                         float nextSemi, int gateLen, bool ornEligible) {
    clearSchedule();

    auto pushNote = [&](float semi, uint32_t at, uint32_t dur) {
        pushCv(at, fractalPlaybackCv(semi, _fractalTrack));
        pushGate(at, true);
        pushGate(at + std::max<uint32_t>(1u, dur), false);
    };

    if (gateLen <= 0) {
        // Rest: hold CV, gate stays low. Still set CV so glides/scope track it.
        pushCv(tick, fractalPlaybackCv(mainSemi, _fractalTrack));
        pushGate(tick, false);
        return;
    }

    // Proportional gate length: gateLen/15 of the section. Mirrors NoteTrack.
    uint32_t fullDur = std::max<uint32_t>(1u, (divisor * uint32_t(gateLen)) / 15u);
    uint32_t e = std::max<uint32_t>(1u, divisor / 8u);   // one eighth of the section

    int name = ornEligible ? rollOrnament(_sequence->ornamentRate(), _sequence->ornamentIntensity()) : -1;
    if (name < 0) {
        pushNote(mainSemi, tick, fullDur);
        return;
    }

    int dir = (nextSemi >= mainSemi) ? 1 : -1;   // toward the next cell

    // Grace: main held, then a one-eighth grace note at the tail.
    auto grace = [&](float semi) {
        pushNote(mainSemi, tick, fullDur - e);
        pushNote(semi, tick + fullDur - e, e);
    };

    switch (name) {
    case OrnAnticipation:
        pushNote(nextSemi, tick, e);
        pushNote(mainSemi, tick + e, fullDur - e);
        break;
    case OrnSuspension:                          // no prior-cell handle in the looper
        pushNote(mainSemi, tick, fullDur);
        break;
    case OrnSyncopation:                         // rest one eighth, then play
        pushNote(mainSemi, tick + e, fullDur - e);
        break;
    case OrnOctaveUp:    grace(mainSemi + 12.f); break;
    case OrnFifthUp:     grace(stepDegrees(mainSemi, 4)); break;
    case OrnHalfTurnToward: grace(stepDegrees(mainSemi, dir)); break;
    case OrnHalfTurnAway:   grace(stepDegrees(mainSemi, -dir)); break;
    case OrnTurn:
        pushNote(mainSemi, tick, e);
        pushNote(stepDegrees(mainSemi, 1), tick + e, e);
        pushNote(stepDegrees(mainSemi, -1), tick + 2 * e, e);
        pushNote(mainSemi, tick + 3 * e, fullDur - 3 * e);
        break;
    case OrnMordentUp:
    case OrnMordentDown: {
        int d = (name == OrnMordentUp) ? 1 : -1;
        pushNote(mainSemi, tick, e);
        pushNote(stepDegrees(mainSemi, d), tick + e, e);
        pushNote(mainSemi, tick + 2 * e, fullDur - 2 * e);
        break;
    }
    case OrnRunToward:
    case OrnRunAway: {
        int d = (name == OrnRunToward) ? dir : -dir;
        uint32_t s = std::max<uint32_t>(1u, divisor / 5u);
        pushNote(mainSemi, tick, s);
        for (int kk = 1; kk <= 4; ++kk) pushNote(stepDegrees(mainSemi, d * kk), tick + kk * s, s);
        break;
    }
    case OrnArpToward:
    case OrnArpAway: {
        int d = (name == OrnArpToward) ? dir : -dir;
        uint32_t s = std::max<uint32_t>(1u, divisor / 4u);
        int degs[3] = { 0, 2, 4 };
        for (int kk = 0; kk < 3; ++kk) pushNote(stepDegrees(mainSemi, d * degs[kk]), tick + kk * s, s);
        break;
    }
    case OrnTrill: {
        float up = stepDegrees(mainSemi, 1);
        uint32_t sp = std::max<uint32_t>(1u, divisor / 8u);
        for (int kk = 0; kk < 8; ++kk) pushNote((kk & 1) ? up : mainSemi, tick + kk * sp, sp);
        break;
    }
    default:
        pushNote(mainSemi, tick, fullDur);
        break;
    }
}

// KD-15: defer a resolved main note by `sections`. Stores the note only — raw
// rel-root semitone (×256 fixed-point) + gateLen + ornEligible bit; ornaments are
// re-rolled when the entry surfaces. Cap is the max delay, so never overflows.
void FractalTrackEngine::pushDelay(float mainSemi, int gateLen, bool ornEligible, int sections) {
    if (_delayCount >= kDelayRingSize) return;
    DelayEntry &e = _delayRing[_delayCount++];
    int q = clamp(int(std::lround(mainSemi * 256.f)), -32768, 32767);
    e.cv = int16_t(q);
    e.gatePacked = uint8_t((gateLen & 0xf) | (ornEligible ? 0x80 : 0));
    e.sectionsLeft = uint8_t(clamp(sections, 0, 255));
}

// Decrement every live entry; entries reaching 0 schedule their note now (with a
// fresh ornament roll). nextSemi has no cross-delay lookahead, so it equals the
// main note (ornament direction defaults). Compacts the ring in place.
void FractalTrackEngine::drainDelayRing(uint32_t tick, uint32_t divisor) {
    uint8_t w = 0;
    for (uint8_t r = 0; r < _delayCount; ++r) {
        DelayEntry &e = _delayRing[r];
        if (e.sectionsLeft > 0) e.sectionsLeft--;
        if (e.sectionsLeft == 0) {
            float semi = float(e.cv) * (1.f / 256.f);
            int gateLen = e.gatePacked & 0xf;
            bool ornEligible = (e.gatePacked & 0x80) != 0;
            scheduleSection(tick, divisor, semi, semi, gateLen, ornEligible);
        } else {
            _delayRing[w++] = e;
        }
    }
    _delayCount = w;
}

void FractalTrackEngine::update(float dt) {
    (void)dt;
}
