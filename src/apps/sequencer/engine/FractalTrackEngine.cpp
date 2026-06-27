#include "FractalTrackEngine.h"
#include "Engine.h"
#include "TT2OutputShaping.h"

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
    _rng(0x6f7261 + track.trackIndex()),
    _branchRng(0)
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

    // KD-13 retrigger gap: consecutive ornament notes leave the gate low for a tick
    // between them (each note-off precedes the next note-on) so they re-articulate
    // instead of fusing into one held gate.
    {
        int sr = _sequence->ornamentRate(), si = _sequence->ornamentIntensity();
        _sequence->setOrnamentRate(100); _sequence->setOrnamentIntensity(100);
        bool sawMulti = false;
        for (int t = 0; t < 200 && !sawMulti; ++t) {
            scheduleSection(0, 96, 0.f, 12.f, 8, true);
            if (_gateCount >= 4) {            // ≥2 notes scheduled
                sawMulti = true;
                for (int i = 1; i + 1 < _gateCount; i += 2)   // off[i] < on[i+1]
                    assert(_gateEvents[i].tick < _gateEvents[i + 1].tick);
            }
        }
        assert(sawMulti);
        _sequence->setOrnamentRate(sr); _sequence->setOrnamentIntensity(si);
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
        assert(_gateCount > 0 && _delayRing.readable() == 0);
        float cv0 = _cvEvents[0].cv;

        // delay 3: first 3 sections defer (ring fills, nothing scheduled); the
        // 4th surfaces section 0's note with the same playback CV.
        _fractalTrack.setTrackDelay(3);
        _globalPos = 0; _orderState.reset(); clearSchedule(); clearDelayRing();
        replaySection(0, div);   assert(_gateCount == 0 && _delayRing.readable() == 1);
        replaySection(0, div);   assert(_gateCount == 0 && _delayRing.readable() == 2);
        replaySection(0, div);   assert(_gateCount == 0 && _delayRing.readable() == 3);
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
    for (auto &o : _onset) o = 0;
    resetObservation();
    _eventHasRef = false;
    _eventRefSemi = 0.f;
    _eventInhibit = 0;
}

void FractalTrackEngine::clearTrunk() {
    for (auto &cell : _trunk) cell = 0;
    for (auto &o : _onset) o = 0;
    _recordPos = sequence().recordFirst();
    _recordSkipRemaining = 0;
    _wasArmed = false;
    _readPos = sequence().loopFirst();
    _globalPos = 0;
    _orderState.reset();
    _gate = false;
    _activity = false;
    clearSchedule();
    clearDelayRing();
    _lastOrnament = -1;
    resetObservation();
    _eventHasRef = false;
    _eventInhibit = 0;
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

    // KD-1/14b: observe the parent every tick across the section so capture can
    // summarize gate length + first-edge onset. The boundary tick (handled in
    // advanceSection above) reset the window; this samples it into the fresh one.
    {
        const auto &seq2 = sequence();
        bool armed = seq2.recordTrigger() != 0;
        if (armed && !_fractalTrack.lock() && _fractalTrack.hasSource()) {
            observeParent(divisor);
        }
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

    // KD-1/14b: at the boundary, commit the section that just ended from the
    // observe-over-section accumulators (Section cadence only — Event cadence
    // commits per parent note-change inside observeParent). Then reset the window
    // for the section now beginning. Lock/disarm suspend capture entirely.
    bool armed = seq.recordTrigger() != 0;
    if (armed && !_wasArmed) { _recordSkipRemaining = 0; resetObservation(); }
    _wasArmed = armed;
    bool capturing = armed && !_fractalTrack.lock() && _fractalTrack.hasSource();
    bool sectionCadence = seq.captureCadence() == FractalSequence::CaptureCadence::Section;
    if (capturing && sectionCadence) {
        captureSection();
    }
    resetObservation();

    replaySection(tick, divisor);
}

void FractalTrackEngine::resetObservation() {
    _obsTickCount = 0;
    _obsGateHighTicks = 0;
    _obsOnsetTick = -1;
    _obsEdgeCv = 0.f;
    _obsAnyGate = false;
    _obsPrevGate = false;
}

// KD-1/14b: ÷ the section into 16 onset buckets; first-edge tick → 4-bit phase.
int FractalTrackEngine::onsetNibbleFromObservation(int onsetTick, int sectionTicks) {
    if (onsetTick < 0 || sectionTicks <= 0) return 0;
    int n = (onsetTick * 16) / sectionTicks;
    return clamp(n, 0, 15);
}

// KD-1: gate-high tick fraction → proportional 4-bit gateLen. Mirrors the sim's
// frac→gateLen buckets: full ≥0.97 → 15, very short ≤0.18 → 1, else 2..14.
int FractalTrackEngine::gateLenFromObservation(int gateHighTicks, int sectionTicks) {
    if (gateHighTicks <= 0 || sectionTicks <= 0) return 0;
    float frac = float(gateHighTicks) / float(sectionTicks);
    if (frac >= 0.97f) return 15;
    if (frac <= 0.18f) return 1;
    return clamp(int(std::lround(frac * 14.f)), 2, 14);
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

// Raw volts of a single routing channel — same source locations as
// RoutingEngine::resolveSourceValue, but unnormalized (1V/oct for the Fractal).
// Mod 0..127 → ±5V via Engine's modulator-offset mapping (applyModulatorOffset).
float FractalTrackEngine::readChannelVolts(Routing::Source src) const {
    using S = Routing::Source;
    if (src >= S::CvIn1 && src <= S::CvIn4) {
        return _engine.cvInput().channel(int(src) - int(S::CvIn1));
    }
    if (src >= S::CvOut1 && src <= S::CvOut8) {
        return _engine.cvOutput().channel(int(src) - int(S::CvOut1));
    }
    if (src >= S::BusCv1 && src <= S::BusCv4) {
        return _engine.busCv(int(src) - int(S::BusCv1));
    }
    if (src >= S::GateOut1 && src <= S::GateOut8) {
        return (_engine.gateOutput() & (1 << (int(src) - int(S::GateOut1)))) ? 5.f : 0.f;
    }
    if (src >= S::Mod1 && src <= S::Mod8) {
        int v = _engine.modulatorEngine().currentValue(int(src) - int(S::Mod1));
        return (v - 64) / 12.8f;   // 0..127 → ±5V
    }
    return 0.f;
}

// Channel-as-gate: a GateOut bit is on when the raw read is non-zero; any other
// channel (a CV) gates when its value is non-zero.
bool FractalTrackEngine::channelAsGate(Routing::Source src) const {
    return readChannelVolts(src) != 0.f;
}

// KD-4 mix resolve. Both-track-or-None slots take the logic-mix path (byte-
// identical to before). If either slot is a single routing channel, the tailored
// path applies: final CV from slot A, final gate from slot B, no logic mix.
// recordMuted (KD-21 ghost) taps the pre-mute gate. Returns false if no source.
bool FractalTrackEngine::resolveParentMix(bool &gate, float &cv) const {
    int srcA = _fractalTrack.sourceA();
    int srcB = _fractalTrack.sourceB();
    using SK = FractalTrack::SourceKind;
    bool channelInvolved =
        FractalTrack::sourceKind(srcA) == SK::Channel ||
        FractalTrack::sourceKind(srcB) == SK::Channel;
    bool recordMuted = _fractalTrack.recordMuted();

    if (!channelInvolved) {
        if (srcA < 0) { gate = false; cv = 0.f; return false; }
        const TrackEngine &parentA = _engine.trackEngine(srcA);
        bool aGate = recordMuted ? parentA.recordGate() : parentA.gateOutput(0);
        float aCv = parentA.cvOutput(0);

        if (srcB < 0) { gate = aGate; cv = aCv; return true; }
        const TrackEngine &parentB = _engine.trackEngine(srcB);
        bool bGate = recordMuted ? parentB.recordGate() : parentB.gateOutput(0);
        float bCv = parentB.cvOutput(0);
        gate = combineGate(aGate, bGate);
        cv = combineCv(aCv, bCv, aGate, bGate);
        return true;
    }

    if (srcA < 0 && srcB < 0) { gate = false; cv = 0.f; return false; }

    // Slot A → CV (+ its own gate, used as the fallback when B is None).
    float aCv = 0.f; bool aGate = false;
    switch (FractalTrack::sourceKind(srcA)) {
    case SK::Channel: {
        Routing::Source ch = FractalTrack::sourceChannelOf(srcA);
        aCv = readChannelVolts(ch);
        aGate = channelAsGate(ch);
        break;
    }
    case SK::Track: {
        const TrackEngine &parentA = _engine.trackEngine(srcA);
        aCv = parentA.cvOutput(0);
        aGate = recordMuted ? parentA.recordGate() : parentA.gateOutput(0);
        break;
    }
    case SK::None: break;
    }

    // Slot B → gate. None → fall back to slot A's gate so a lone CV-in-A gates.
    switch (FractalTrack::sourceKind(srcB)) {
    case SK::Channel:
        gate = channelAsGate(FractalTrack::sourceChannelOf(srcB));
        break;
    case SK::Track: {
        const TrackEngine &parentB = _engine.trackEngine(srcB);
        gate = recordMuted ? parentB.recordGate() : parentB.gateOutput(0);
        break;
    }
    case SK::None:
        gate = aGate;
        break;
    }
    cv = aCv;
    return true;
}

// KD-1/14b: one tick of observation. Accumulate gate-high ticks, the first
// rising-edge phase + its CV (Feel), and S&H the latest CV. Event cadence fires
// a commit here on a hysteretic note-change (Rings cv_scaler port).
void FractalTrackEngine::observeParent(uint32_t divisor) {
    const auto &seq = sequence();
    bool gate; float cv;
    if (!resolveParentMix(gate, cv)) return;
    float semi = cv * kSemitonesPerOctave;

    _obsLastCv = cv;
    if (gate) {
        _obsGateHighTicks++;
        _obsAnyGate = true;
        if (!_obsPrevGate && _obsOnsetTick < 0) {
            _obsOnsetTick = int16_t(_obsTickCount);
            _obsEdgeCv = cv;
        }
    }
    if (_eventInhibit > 0) _eventInhibit--;

    // KD-14b Event cadence: commit on a hysteretic semitone change outside the
    // inhibit window. recordPos advances per parent note event → compact note-list.
    if (seq.captureCadence() == FractalSequence::CaptureCadence::Event) {
        if (!_eventHasRef) {
            if (gate) { _eventRefSemi = std::round(semi); _eventHasRef = true; }
        } else if (std::abs(semi - _eventRefSemi) >= (1.f - 1.f / 3.f)
                   && _eventInhibit == 0) {
            _eventRefSemi = std::round(semi);
            _eventInhibit = int16_t(std::max<uint32_t>(1u, divisor / 8u));   // ~⅛ section
            int gateLen = gate ? 8 : 0;
            commitCell(semi, gateLen, 0, true);   // Event lands on the cell, onset 0
            advanceRecordPos();
        }
    }

    _obsPrevGate = gate;
    _obsTickCount++;
}

void FractalTrackEngine::captureSection() {
    const auto &seq = sequence();

    // KD-20 Pack: skip recordSkip sections between writes; cells pack consecutively.
    if (_recordSkipRemaining > 0) {
        _recordSkipRemaining--;
        return;
    }

    bool feel = seq.captureFidelity() == FractalSequence::CaptureFidelity::Feel;
    int sectionTicks = std::max(1, int(_obsTickCount));

    // KD-1: gateLen from accumulated gate-high duration; rest if never high.
    int gateLen = gateLenFromObservation(_obsGateHighTicks, sectionTicks);
    // CV: Feel S&Hs at the first rising edge; Quantized holds the last sample.
    float cv = (feel && _obsAnyGate) ? _obsEdgeCv : _obsLastCv;
    float semi = cv * kSemitonesPerOctave;
    int onset = (feel && _obsOnsetTick >= 0)
        ? onsetNibbleFromObservation(_obsOnsetTick, sectionTicks) : 0;

    // KD-14 Latch: only write when the section gated — silent sections keep their
    // prior content (overdub). Replace (default) overwrites every section.
    bool latch = seq.recordMode() == FractalSequence::RecordMode::Latch;
    if (!(latch && gateLen == 0)) {
        commitCell(semi, gateLen, onset, true);
    }

    advanceRecordPos();
    _recordSkipRemaining = uint8_t(seq.recordSkip());
}

void FractalTrackEngine::commitCell(float semitonesRelRoot, int gateLen, int onsetNibbleVal, bool valid) {
    _trunk[_recordPos] = encodeCell(semitonesRelRoot, gateLen, valid);
    setOnsetNibble(_recordPos, onsetNibbleVal);
}

void FractalTrackEngine::advanceRecordPos() {
    const auto &seq = sequence();
    int first = seq.recordFirst();
    int last = seq.recordLast();
    if (last < first) last = first;
    if (_recordPos < first || _recordPos > last) { _recordPos = uint8_t(first); return; }
    if (_recordPos >= last) _recordPos = uint8_t(first);
    else _recordPos++;
}

// orderMode → SequenceState runMode for the four standard traversals. Converge/
// Diverge/Page advance the logical step linearly (Forward) and remap it in
// orderMapWithin, so they resolve to Forward here.
static Types::RunMode orderRunMode(FractalSequence::OrderMode mode) {
    switch (mode) {
    case FractalSequence::OrderMode::Reverse:  return Types::RunMode::Backward;
    case FractalSequence::OrderMode::Pendulum: return Types::RunMode::Pendulum;
    case FractalSequence::OrderMode::Random:   return Types::RunMode::Random;
    default:                                   return Types::RunMode::Forward;
    }
}

// Map a linear logical step (0..len-1) to a within-loop read position for the
// Converge/Diverge/Page orders. Ported from the sim's mapLogicalToRead
// (.scratch/fractal-sim.html, order codes 4/5/6). Standard orders pass through.
static int orderMapWithin(FractalSequence::OrderMode mode, int step, int len) {
    if (len <= 0) return 0;
    int s = ((step % len) + len) % len;
    switch (mode) {
    case FractalSequence::OrderMode::Converge: {   // 0,last,1,last-1,...
        int half = s / 2;
        return (s % 2 == 0) ? half : (len - 1 - half);
    }
    case FractalSequence::OrderMode::Diverge: {     // centre outward
        int c = (len - 1) / 2;
        int k = (s + 1) / 2;
        return (s % 2 == 0) ? clamp(c - k, 0, len - 1) : clamp(c + k, 0, len - 1);
    }
    case FractalSequence::OrderMode::Page: {        // 4-cell pages
        int pages = (len + 3) / 4;
        int page = s % pages;
        int within = s / pages;
        return clamp(page * 4 + within, 0, len - 1);
    }
    default:
        return s;
    }
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

// Cache signature: rebuild only when {branchSeed, branchPool, loopLen} changes.
int FractalTrackEngine::branchSignature() const {
    const auto &seq = sequence();
    return int((uint32_t(seq.branchSeed()) * 131u
                + uint32_t(seq.branchPool()) * 17u + uint32_t(loopLen())) & 0x7fffffff);
}

// Derive one chained transform per branch from branchSeed, drawn only from the
// enabled branchPool (empty → all Transpose). The branch RNG is reseeded from
// branchSeed each rebuild, so the same seed always yields the same chain.
void FractalTrackEngine::rebuildBranchTransforms() {
    const auto &seq = sequence();
    int pool = seq.branchPool();
    int len = loopLen();

    int eligible[kNumBranchOps]; int nEligible = 0;
    for (int k = 0; k < kNumBranchOps; ++k) {
        if ((pool >> k) & 1) eligible[nEligible++] = k;
    }
    if (nEligible == 0) { eligible[0] = OpTranspose; nEligible = 1; }

    _branchRng = Random(uint32_t(seq.branchSeed()));
    int n = clamp(seq.branchCount(), 0, 7);
    for (int s = 1; s <= n; ++s) {
        BranchTransform &b = _branches[s - 1];
        b.kind = uint8_t(eligible[_branchRng.nextRange(uint32_t(nEligible))]);
        b.t = 0; b.k = 0; b.fx100 = 100; b.n = 2;
        if (b.kind == OpTranspose) {
            int mag = kTransposeIntervals[_branchRng.nextRange(3)];
            b.t = int8_t((_branchRng.nextBinary() ? -1 : 1) * mag);
        } else if (b.kind == OpRotate) {
            b.k = uint8_t(len > 1 ? (1 + int(_branchRng.nextRange(uint32_t(len - 1)))) : 0);
        } else if (b.kind == OpCompress) {
            b.fx100 = uint16_t(kCompressFactorsX100[_branchRng.nextRange(2)]);
        } else if (b.kind == OpExpand) {
            b.fx100 = uint16_t(kExpandFactorsX100[_branchRng.nextRange(2)]);
        } else if (b.kind == OpGateThin) {
            b.n = uint8_t(2 + _branchRng.nextRange(3));   // 2..4
        }
    }
    _branchHash = branchSignature();
    _branchCountCache = n;
}

int FractalTrackEngine::branchParam(int seg) const {
    const BranchTransform &b = _branches[seg - 1];
    switch (b.kind) {
    case OpTranspose:            return b.t;
    case OpRotate:               return b.k;
    case OpCompress: case OpExpand: return b.fx100;
    default:                     return 0;
    }
}

void FractalTrackEngine::maybeRebuildBranchTransforms() {
    if (branchSignature() != _branchHash
            || clamp(sequence().branchCount(), 0, 7) != _branchCountCache) {
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

int FractalTrackEngine::routeOf(int path, int n, int routeIndex) {
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

const char *FractalTrackEngine::ornamentName(int id) {
    static const char *kNames[] = {
        "Anticipation", "Suspension", "Syncopation", "Octave-Up", "Fifth-Up",
        "Half-Turn Toward", "Half-Turn Away",
        "Run Toward", "Run Away", "Turn", "Arp Toward", "Arp Away",
        "Mordent Up", "Mordent Down",
        "Trill",
    };
    if (id < 0 || id >= int(sizeof(kNames) / sizeof(kNames[0]))) return "-";
    return kNames[id];
}

void FractalTrackEngine::replaySection(uint32_t tick, uint32_t divisor) {
    const auto &seq = sequence();

    maybeRebuildBranchTransforms();

    int n = clamp(seq.branchCount(), 0, 7);
    int len = loopLen();
    int total = len * (1 + n);
    int gp = _globalPos % total;

    // KD-12: global walk position → segment via Path route. The within-loop read
    // order comes from orderMode: the four standard orders drive SequenceState
    // (Forward stays byte-identical to the old gp%len walk); Converge/Diverge/Page
    // advance the logical step linearly and remap it via orderMapWithin.
    int routeIndex = clamp(gp / len, 0, n);
    int segment = routeOf(seq.path(), n, routeIndex);

    FractalSequence::OrderMode order = seq.orderMode();
    Types::RunMode runMode = orderRunMode(order);
    _orderState.advanceFree(runMode, 0, len - 1, _rng);
    int within = orderMapWithin(order, _orderState.step(), len);
    // Lookahead within: a copy advanced one more section gives the next read order.
    SequenceState nextState = _orderState;
    nextState.advanceFree(runMode, 0, len - 1, _rng);
    int nextWithin = orderMapWithin(order, nextState.step(), len);

    float semitonesRelRoot; int gateLen; bool valid;
    segmentCell(segment, within, semitonesRelRoot, gateLen, valid);

    _readPos = uint8_t(trunkReadIndex(within));   // UI highlight = trunk index read
    _currentSegment = uint8_t(segment);           // UI highlight = sounding branch block

    // Lookahead for direction-aware ornaments: the next sounding cell's raw note.
    float nextSemi; int nextGateLen; bool nextValid;
    segmentCell(segment, nextWithin, nextSemi, nextGateLen, nextValid);

    // Ornament eligibility (KD-13): sounding cell gated and trunk read index
    // inside the ornament zone. Rests and out-of-zone cells never ornament.
    int readIdx = trunkReadIndex(within);
    bool ornEligible = valid && gateLen > 0
        && readIdx >= seq.ornFirst() && readIdx <= seq.ornLast();

    // KD-14b Feel replay: delay the cell's gate by its stored onset phase
    // (onset/16 of the section) → swing/microtiming. Quantized onset is 0 → no
    // delay (the array stays 0 there). Onset is keyed to the trunk index read.
    uint32_t onsetTicks = 0;
    if (seq.captureFidelity() == FractalSequence::CaptureFidelity::Feel) {
        onsetTicks = (uint32_t(onsetNibble(readIdx)) * divisor) / 16u;
    }

    int delay = _fractalTrack.trackDelay();
    if (delay <= 0) {
        // Immediate path (byte-identical to no-delay playback when onset 0).
        scheduleSection(tick + onsetTicks, divisor, semitonesRelRoot, nextSemi, valid ? gateLen : 0, ornEligible);
    } else {
        // Surface notes due this section first, then defer this section's note.
        drainDelayRing(tick, divisor);
        pushDelay(semitonesRelRoot, valid ? gateLen : 0, ornEligible, delay);
    }

    _globalPos = uint16_t((gp + 1) % total);
}

// Opt-in playback quantize: snap rel-root semitones to the track's quantize scale
// via Tt2OutputShaping::shapeCv's quantize block (Bipolar5V range, no offset, so
// only the quantize+clamp engages). quantize < 0 → raw pass-through, untouched.
float FractalTrackEngine::quantizeMainSemi(float semitonesRelRoot) const {
    int q = _fractalTrack.quantize();
    if (q < 0) return semitonesRelRoot;
    float volts = semitonesRelRoot * (1.f / kSemitonesPerOctave);
    volts = Tt2OutputShaping::shapeCv(volts, Types::VoltageRange::Bipolar5V, 0,
                                      q, -1, _model.project().rootNote());
    return volts * kSemitonesPerOctave;
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

// Scale degree for a raw rel-root semitone (degree 0 = scale tonic) via the
// shared Scale::noteFromVolts snap. Floor-style: snaps to the degree at or
// below the target (the prior hand-scan rounded to nearest — see test note).
int FractalTrackEngine::nearestDegree(float semitonesRelRoot) const {
    const auto scale = sequence().selectedScale(_model.project().scale(), _model.project().scaleRotate());
    return scale.noteFromVolts(semitonesRelRoot * (1.f / kSemitonesPerOctave));
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

    // Post-record quantize (opt-in): snap the played main note to the track's
    // quantize scale. Off (-1) → skipped, byte-identical. Trunk stays raw; the
    // derived ornament flourish notes already snap to their own scale.
    mainSemi = quantizeMainSemi(mainSemi);
    nextSemi = quantizeMainSemi(nextSemi);

    // gap = retrigger silence held before the next note: ornament notes pass 1 so
    // the gate drops for a tick and re-articulates; the plain/main note passes 0 to
    // stay byte-identical to un-ornamented playback.
    auto pushNote = [&](float semi, uint32_t at, uint32_t dur, uint32_t gap = 1u) {
        pushCv(at, fractalPlaybackCv(semi, _fractalTrack));
        pushGate(at, true);
        pushGate(at + (dur > gap ? dur - gap : 1u), false);
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
        pushNote(mainSemi, tick, fullDur, 0u);   // plain note: no retrigger gap (byte-identical)
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
        pushNote(mainSemi, tick, fullDur, 0u);
        break;
    }
}

// KD-15: defer a resolved main note by `sections`. Stores the note only — raw
// rel-root semitone (×256 fixed-point) + gateLen + ornEligible bit; ornaments are
// re-rolled when the entry surfaces. Cap is the max delay, so never overflows.
void FractalTrackEngine::pushDelay(float mainSemi, int gateLen, bool ornEligible, int sections) {
    if (_delayRing.full()) return;
    DelayEntry e;
    e.cv = int16_t(clamp(int(std::lround(mainSemi * 256.f)), -32768, 32767));
    e.gatePacked = uint8_t((gateLen & 0xf) | (ornEligible ? 0x80 : 0));
    e.sectionsLeft = uint8_t(clamp(sections, 0, 255));
    _delayRing.write(e);
}

// Decrement every live entry; entries reaching 0 schedule their note now (with a
// fresh ornament roll). nextSemi has no cross-delay lookahead, so it equals the
// main note (ornament direction defaults). Survivors are re-queued in order.
void FractalTrackEngine::drainDelayRing(uint32_t tick, uint32_t divisor) {
    size_t pending = _delayRing.readable();
    for (size_t i = 0; i < pending; ++i) {
        DelayEntry e = _delayRing.read();
        if (e.sectionsLeft > 0) e.sectionsLeft--;
        if (e.sectionsLeft == 0) {
            float semi = float(e.cv) * (1.f / 256.f);
            int gateLen = e.gatePacked & 0xf;
            bool ornEligible = (e.gatePacked & 0x80) != 0;
            scheduleSection(tick, divisor, semi, semi, gateLen, ornEligible);
        } else {
            _delayRing.write(e);
        }
    }
}

void FractalTrackEngine::update(float dt) {
    (void)dt;
}
