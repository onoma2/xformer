#pragma once

#include "TrackEngine.h"
#include "model/FractalSequence.h"
#include "model/FractalTrack.h"

#include "core/utils/Random.h"

#include <cstdint>

// Looper engine: capture the source tracks' emitted gate+CV per section into an
// inline trunk buffer, replay it through the loop window. With sourceB set, the
// two parents combine via gateLogic × cvLogic (KD-4). Branches (KD-12) concatenate
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

    // Cursors for the UI hero pages.
    int readPos() const { return _readPos; }
    int recordPos() const { return _recordPos; }

private:
    FractalSequence &sequence()             { return *_sequence; }
    const FractalSequence &sequence() const { return *_sequence; }

    // Sequence divisor scaled by clockMultiplier + PPQN conversion. Mirrors
    // StochasticTrackEngine::effectiveDivisor().
    uint32_t effectiveDivisor() const;

    void advanceSection(uint32_t tick, uint32_t divisor);
    void captureSection();

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
    static constexpr int kDelayRingSize = 16;
    DelayEntry _delayRing[kDelayRingSize];
    uint8_t _delayCount = 0;
    void clearDelayRing() { _delayCount = 0; }
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

    // Section timing.
    uint32_t _relativeTick = 0;
    uint8_t _recordPos = 0;   // capture cursor (recordFirst..recordLast)
    uint8_t _recordSkipRemaining = 0;   // KD-20 Pack: sections to skip before next write
    bool _wasArmed = false;   // arm rising-edge detect → fresh run starts on a write
    uint8_t _readPos = 0;     // trunk read index of the sounding cell (UI highlight)
    uint16_t _globalPos = 0;  // KD-12 walk over the concatenated length 0..total-1

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
    // freezes _lastOrnament: Rate still gates whether a flourish fires, Lock
    // fixes which one. -1 = none rolled yet.
    Random _rng;
    int _lastOrnament = -1;
};

static_assert(sizeof(FractalTrackEngine) <= 912, "FractalTrackEngine too large");
