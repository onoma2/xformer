#pragma once

#include "TrackEngine.h"
#include "model/FractalSequence.h"
#include "model/FractalTrack.h"

#include <cstdint>

// Looper engine: capture sourceA's emitted gate+CV per section into an inline
// trunk buffer, replay it through the loop window. Branches (KD-12) concatenate
// chained transforms of the trunk after it. Ornaments, sleep, two-source mix,
// track-delay and capture variants are deferred —
// see docs/superpowers/specs/2026-05-17-fractal-track-design.md.
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
    void replaySection(uint32_t tick, uint32_t divisor);

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
    uint8_t _readPos = 0;     // trunk read index of the sounding cell (UI highlight)
    uint16_t _globalPos = 0;  // KD-12 walk over the concatenated length 0..total-1

    // Outputs.
    bool _gate = false;
    float _cv = 0.f;
    bool _activity = false;
    bool _gateActive = false;     // gate currently held high within a section
    uint32_t _gateOffTick = 0;    // absolute tick at which the held gate falls
};

static_assert(sizeof(FractalTrackEngine) <= 912, "FractalTrackEngine too large");
