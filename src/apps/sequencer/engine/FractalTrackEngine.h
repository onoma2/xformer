#pragma once

#include "TrackEngine.h"
#include "SequenceState.h"
#include "model/FractalSequence.h"
#include "model/FractalTrack.h"

#include "core/utils/Random.h"
#include "core/utils/RingBuffer.h"

#include <cstdint>

// Looper engine: observe the source tracks' emitted gate+CV every tick across a
// section (KD-1) and commit one summarized cell — proportional gate length, plus
// (Feel, KD-14b) the first rising-edge onset phase — into an inline trunk buffer,
// then replay it through the loop window. Cadence picks when a cell is written
// (Section boundary vs Event note-change); Fidelity picks whether onset is kept.
// With sourceB set, the two parents combine via gateLogic × cvLogic (KD-4). Branches (KD-12) concatenate
// chained transforms of the trunk after it. Ornaments (KD-13) inject scale-snapped
// flourish notes inside a section via a sub-section gate/CV event queue.
// Track-delay (KD-15) shifts the resolved output trackDelay sections later via a
// fixed ring — see docs/superpowers/specs/2026-05-17-fractal-track-design.md.
class FractalTrackEngine final : public TrackEngine {
public:
    FractalTrackEngine(Engine &engine, const Model &model, Track &track);

    virtual void reset() override;
    virtual void restart() override;
    virtual void stop() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Fractal; }

    virtual bool gateOutput(int index) const override { return _gate; }
    virtual float cvOutput(int index) const override { return _cv; }

    virtual void changePattern() override {
        _sequence = &(_fractalTrack.sequence(pattern()));
    }

    bool activity() const override { return _activity; }

    // Trunk cell codec (KD-2). One uint16 per section:
    //   bit 15      valid
    //   bits 11..14 gateLen (0=rest, 1=trig, 2..14=proportional, 15=tie)
    //   bits 0..10  signed 11-bit CV rel root, c in [-1024,1023];
    //               semitones = c * 60 / 1024 (≈ ±5 octaves)
    static uint16_t encodeCell(float semitonesRelRoot, int gateLen, bool valid);
    static void decodeCell(uint16_t cell, float &semitonesRelRoot, int &gateLen, bool &valid);
    bool cellValid(int i) const { return (_trunk[i] & 0x8000) != 0; }

    // Trunk accessor for the UI tape page (later phase).
    const uint16_t *trunk() const { return _trunk; }

    // Erase the recorded loop: zero the trunk, reset read/record cursors, and
    // clear the sub-section schedule + delay ring so playback restarts clean.
    void clearTrunk();

    // Cursors for the UI hero pages.
    int readPos() const { return _readPos; }
    int recordPos() const { return _recordPos; }
    int currentSegment() const { return _currentSegment; }   // 0=trunk, 1..N=branch

    // KD-12 Path navigation order: trunk first, then outward branches (path bit 0)
    // ascending, then held branches (bit 1) descending. routeIndex 0 = trunk.
    static int routeOf(int path, int branchCount, int routeIndex);

    // Last-fired ornament id (-1 = none), and its display name (flash strings).
    int lastOrnament() const { return _lastOrnament; }
    static const char *ornamentName(int id);

    // Hero step-grid queues. queueSegment defers a playback jump to the next
    // section boundary; queueOrnament forces an ornament on the next gated cell.
    void queueSegment(int seg) { _queuedSegment = int8_t(clamp(seg, 0, 7)); }
    void queueOrnament(int id) { _queuedOrnament = int8_t(clamp(id, 0, 14)); }
    bool hasQueuedOrnament() const { return _queuedOrnament >= 0; }

    // Branch transform readout for the UI Branch page. seg 1..branchCount.
    // branchKind → op code 0..7; branchParam → kind-relevant resolved value:
    // Transpose semitones, Rotate offset, Compress/Expand factor ×100, else 0.
    int branchKind(int seg) const { return _branches[seg - 1].kind; }
    int branchParam(int seg) const;

    // KD-1 observe-over-section summary: gate-high tick count over the section's
    // total ticks → proportional 4-bit gateLen (0 = rest). Pure, side-effect free,
    // shared by the live capture path and unit tests.
    static int gateLenFromObservation(int gateHighTicks, int sectionTicks);
    // KD-14b: first rising-edge tick over the section → 4-bit onset phase (0..15).
    static int onsetNibbleFromObservation(int onsetTick, int sectionTicks);

#ifndef PLATFORM_STM32
    // Test seam: run one section's traversal (capture-free, no Engine deref) so
    // unit tests can read the orderMode-ordered trunk index sequence.
    void replaySectionForTest(uint32_t tick, uint32_t divisor) { replaySection(tick, divisor); }

    // Test seams for the Random/RingBuffer/Scale-snap refactor characterization.
    void setTrunkCellForTest(int i, uint16_t cell) { _trunk[i] = cell; }
    void rebuildBranchTransformsForTest() { rebuildBranchTransforms(); }
    float segmentSemiForTest(int seg, int pos) const {
        float st; int gl; bool v; segmentCell(seg, pos, st, gl, v); return st;
    }
    uint8_t branchKindForTest(int seg) const { return _branches[seg - 1].kind; }
    size_t delayPendingForTest() const { return _delayRing.readable(); }
    int cvEventCountForTest() const { return _cvCount; }
    float cvEventForTest(int i) const { return _cvEvents[i].cv; }
    uint32_t gateEventTickForTest(int i) const { return _gateEvents[i].tick; }
    int nearestDegreeForTest(float semi) const { return nearestDegree(semi); }
    int onsetNibbleForTest(int cell) const { return onsetNibble(cell); }
    void setOnsetNibbleForTest(int cell, int v) { setOnsetNibble(cell, v); }
    uint16_t trunkCellForTest(int cell) const { return _trunk[cell]; }
    int rollOrnamentForTest(int rate, int intensity) { return rollOrnament(rate, intensity); }
#endif

private:
    FractalSequence &sequence()             { return *_sequence; }
    const FractalSequence &sequence() const { return *_sequence; }

    // Sequence divisor scaled by clockMultiplier + PPQN conversion. Mirrors
    // StochasticTrackEngine::effectiveDivisor().
    uint32_t effectiveDivisor() const;

    void advanceSection(uint32_t tick, uint32_t divisor);
    void captureSection();

    // KD-1/14b observe-over-section. Sample the mixed parent every tick while
    // armed: accumulate gate-high ticks → proportional gateLen, first rising-edge
    // phase → onset, S&H the CV at the first edge (Feel) / boundary (Quantized).
    // Event cadence commits on the parent's hysteretic note-change here instead.
    bool resolveParentMix(bool &gate, float &cv) const;
    // Single routing-channel reads for source slots (raw volts / channel-as-gate).
    float readChannelVolts(Routing::Source src) const;
    bool channelAsGate(Routing::Source src) const;
    void observeParent(uint32_t divisor);
    void resetObservation();
    void commitCell(float semitonesRelRoot, int gateLen, int onsetNibble, bool valid);
    void advanceRecordPos();

    // KD-4 two-source mix. gateLogic/cvLogic combine the two parents'
    // gate+CV (volts). cvLogic Gated picks whichever just fired, A-priority.
    bool combineGate(bool a, bool b) const;
    float combineCv(float aCv, float bCv, bool aGate, bool bGate) const;
    void replaySection(uint32_t tick, uint32_t divisor);

    // KD-13 ornaments. Scale snapping mirrors the sim's nearestDegree/stepDegrees
    // — degrees are rel-root (scale tonic = 0), trunk stays raw. The ornament
    // generator emits a mono note list scheduled onto the gate/CV queues.
    int nearestDegree(float semitonesRelRoot) const;
    float stepDegrees(float semitonesRelRoot, int steps) const;
    // Opt-in playback quantize for the literal main note (raw when quantize < 0).
    float quantizeMainSemi(float semitonesRelRoot) const;
    int eligibleOrnaments(int ids[], int intensity) const;
    int rollOrnament(int rate, int intensity);
    void scheduleSection(uint32_t tick, uint32_t divisor, float mainSemi,
                         float nextSemi, int gateLen, bool ornEligible);

    // KD-15 track-delay output ring. Pushing a section's resolved main note here
    // shifts its audible output trackDelay() sections later (canon/echo). Note
    // only — ornaments are re-rolled when the entry surfaces, not stored. Cap 16
    // = max delay, so one entry per pending section can never overflow.
    struct DelayEntry {
        int16_t cv;          // raw rel-root semitones ×256 fixed-point
        uint8_t gatePacked;  // bits 0..3 gateLen; bit 7 ornEligible; onset nibble 0
        uint8_t sectionsLeft;
    };
    // RingBuffer's modular cursor math only holds for power-of-2 sizes; 32 gives
    // capacity 31, comfortably above the 16-section max delay (one entry/section).
    static constexpr int kDelayRingSize = 32;
    RingBuffer<DelayEntry, kDelayRingSize> _delayRing;
    void clearDelayRing() { while (!_delayRing.empty()) _delayRing.read(); }
    void pushDelay(float mainSemi, int gateLen, bool ornEligible, int sections);
    void drainDelayRing(uint32_t tick, uint32_t divisor);

    // KD-12 branches — one chained transform per branch, derived from branchSeed
    // + the branchPool mask. Recomputed when seed/pool/loop-window changes.
    struct BranchTransform {
        uint8_t kind;   // op code 0..7 (Transpose..Gate-thin)
        int8_t t;       // Transpose semitones
        uint8_t k;      // Rotate offset
        uint16_t fx100; // Compress/Expand factor ×100
        uint8_t n;      // Gate-thin period
    };
    BranchTransform _branches[7] = {};
    int _branchHash = -1;          // signature of the cached assignment
    int _branchCountCache = 0;     // branchCount the cache was built for

    int branchSignature() const;   // {seed,pool,len} → cache signature
    void rebuildBranchTransforms();
    void maybeRebuildBranchTransforms();
    // Resolve segment seg (0 = trunk) at within-position pos → trunk cell fields.
    void segmentCell(int seg, int pos, float &semitonesRelRoot, int &gateLen, bool &valid) const;
    float inversionCenter() const;
    int trunkReadIndex(int within) const;   // Forward+rotate lens (MVP order)
    int loopLen() const;

    FractalTrack &_fractalTrack;
    FractalSequence *_sequence = nullptr;

    // Inline trunk (KD-2/KD-8/9): 128 cells × 2 B = 256 B, CCMRAM.
    uint16_t _trunk[CONFIG_FRACTAL_MAX_CELLS] = {};

    // KD-14b Feel onset side-array: a 4-bit onset phase per cell (when in the
    // section the gate first fired), packed two cells per byte → 64 B (CCMRAM).
    // Always 0 in Quantized (replay no delay); the +sizeof grower budgeted by KD-9.
    uint8_t _onset[CONFIG_FRACTAL_MAX_CELLS / 2] = {};
    int onsetNibble(int cell) const {
        uint8_t b = _onset[cell >> 1];
        return (cell & 1) ? (b >> 4) : (b & 0xf);
    }
    void setOnsetNibble(int cell, int v) {
        uint8_t &b = _onset[cell >> 1];
        v &= 0xf;
        b = (cell & 1) ? uint8_t((b & 0x0f) | (v << 4)) : uint8_t((b & 0xf0) | v);
    }

    // Section timing.
    uint32_t _relativeTick = 0;
    uint8_t _recordPos = 0;   // capture cursor (recordFirst..recordLast)
    uint8_t _recordSkipRemaining = 0;   // KD-20 Pack: sections to skip before next write
    bool _wasArmed = false;   // arm rising-edge detect → fresh run starts on a write

    // KD-1/14b observe-over-section accumulators. Reset each section boundary,
    // updated every armed tick by observeParent().
    uint16_t _obsTickCount = 0;    // ticks elapsed in this section so far
    uint16_t _obsGateHighTicks = 0; // count of ticks the mixed gate was high
    int16_t _obsOnsetTick = -1;    // tick-within-section of the first rising edge (-1 none)
    float _obsEdgeCv = 0.f;        // mixed CV sampled at the first rising edge
    float _obsLastCv = 0.f;        // most recent mixed CV (S&H for Quantized boundary)
    bool _obsPrevGate = false;     // previous tick's mixed gate (edge detect)
    bool _obsAnyGate = false;      // any gate-high seen this section
    // KD-14b Event cadence hysteresis state (Rings cv_scaler port).
    float _eventRefSemi = 0.f;     // last fired quantized semitone reference
    bool _eventHasRef = false;     // reference armed (first note seen)
    int16_t _eventInhibit = 0;     // inhibit-window ticks remaining
    uint8_t _readPos = 0;     // trunk read index of the sounding cell (UI highlight)
    uint8_t _currentSegment = 0;  // sounding segment (0=trunk, 1..N=branch) for UI highlight
    uint16_t _globalPos = 0;  // KD-12 walk over the concatenated length 0..total-1

    // Within-loop read order, orderMode-aware. Advanced once per section; its step()
    // is remapped by orderMapWithin into the loop-relative position fed to
    // segmentCell/trunkReadIndex.
    SequenceState _orderState;

    // Outputs.
    bool _gate = false;
    float _cv = 0.f;
    bool _activity = false;

    // Sub-section schedule (KD-13). The flourish for a section is generated in
    // time order when the section starts, so a plain fixed array drained by a
    // head cursor replaces the sorted queues. Worst case is Trill: 8 notes →
    // 16 gate edges + 8 CV updates. Cleared and refilled per section.
    struct GateEvent { uint32_t tick; bool gate; };
    struct CvEvent { uint32_t tick; float cv; };
    static constexpr int kMaxGateEvents = 18;
    static constexpr int kMaxCvEvents = 10;
    GateEvent _gateEvents[kMaxGateEvents];
    CvEvent _cvEvents[kMaxCvEvents];
    uint8_t _gateCount = 0, _gateHead = 0;
    uint8_t _cvCount = 0, _cvHead = 0;

    void clearSchedule() { _gateCount = _gateHead = _cvCount = _cvHead = 0; }
    void pushGate(uint32_t tick, bool gate) {
        if (_gateCount < kMaxGateEvents) _gateEvents[_gateCount++] = { tick, gate };
    }
    void pushCv(uint32_t tick, float cv) {
        if (_cvCount < kMaxCvEvents) _cvEvents[_cvCount++] = { tick, cv };
    }

    // KD-13 ornament state. PRNG is live/probabilistic (free-running). Lock
    // freezes _lastOrnament (which flourish) AND _lastDir (its realized shape —
    // the toward/away direction + lookahead), so the locked flourish is identical
    // every firing instead of re-adapting to each cell. -1 = none rolled yet.
    Random _rng;
    Random _branchRng;        // seeded from branchSeed per rebuild — deterministic
    int _lastOrnament = -1;
    int8_t _lastDir = 1;

    // Hero step-grid live queues. _queuedSegment: pending Branch-page playback
    // jump, applied at the next section boundary. _queuedOrnament: pending forced
    // ornament punch-in, consumed by the next gated cell. -1 = idle.
    int8_t _queuedSegment = -1;
    int8_t _queuedOrnament = -1;
};

static_assert(sizeof(FractalTrackEngine) <= 912, "FractalTrackEngine too large");
