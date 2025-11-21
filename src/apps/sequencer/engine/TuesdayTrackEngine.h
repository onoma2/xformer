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
    const TuesdayTrack &_tuesdayTrack;

    // Algorithm state
    uint32_t _stepIndex = 0;
    uint32_t _gateLength = 0;
    uint32_t _gateTicks = 0;
    Random _rng;

    // Output state
    bool _activity = false;
    bool _gateOutput = false;
    float _cvOutput = 0.f;
};
