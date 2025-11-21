#pragma once

#include "TrackEngine.h"

#include "model/Track.h"

#include "core/utils/Random.h"

class TuesdayTrackEngine : public TrackEngine {
public:
    TuesdayTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _tuesdayTrack(track.tuesdayTrack())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Tuesday; }

    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return _gateOutput; }
    virtual float cvOutput(int index) const override { return _cvOutput; }

private:
    void initAlgorithm();

    const TuesdayTrack &_tuesdayTrack;

    // Dual RNG system (matches original Tuesday)
    // _rng is seeded from seed1 (Flow) or seed2 (Ornament) depending on algorithm
    // _extraRng is seeded from the other parameter
    Random _rng;
    Random _extraRng;

    // Cached parameter seeds for re-initialization on loop
    uint8_t _cachedFlow = 0;
    uint8_t _cachedOrnament = 0;

    // Algorithm state
    uint32_t _stepIndex = 0;
    uint32_t _gateLength = 0;
    uint32_t _gateTicks = 0;

    // TEST algorithm state
    uint8_t _testMode = 0;      // 0=OCTSWEEPS, 1=SCALEWALKER
    uint8_t _testSweepSpeed = 0;
    uint8_t _testAccent = 0;
    uint8_t _testVelocity = 0;
    int16_t _testNote = 0;

    // TRITRANCE algorithm state
    int16_t _triB1 = 0;  // High note
    int16_t _triB2 = 0;  // Phase offset
    int16_t _triB3 = 0;  // Note offset

    // STOMPER algorithm state
    uint8_t _stomperMode = 0;
    uint8_t _stomperCountDown = 0;
    uint8_t _stomperLowNote = 0;
    uint8_t _stomperHighNote[2] = {0, 0};
    int16_t _stomperLastNote = 0;
    uint8_t _stomperLastOctave = 0;

    // MARKOV algorithm state
    int16_t _markovHistory1 = 0;
    int16_t _markovHistory3 = 0;
    uint8_t _markovMatrix[8][8][2];

    // Output state
    bool _activity = false;
    bool _gateOutput = false;
    float _cvOutput = 0.f;
};
