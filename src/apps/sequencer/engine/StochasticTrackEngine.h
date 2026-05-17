#pragma once

#include "TrackEngine.h"
#include "model/StochasticTrack.h"

class StochasticTrackEngine : public TrackEngine {
public:
    StochasticTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine)
        : TrackEngine(engine, model, track, linkedTrackEngine) {}

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Stochastic; }

    virtual void reset() override {}
    virtual void restart() override {}
    virtual TickResult tick(uint32_t tick) override { return TickResult::NoUpdate; }
    virtual void update(float dt) override {}

    virtual bool activity() const override { return false; }
    virtual bool gateOutput(int index) const override { return false; }
    virtual float cvOutput(int index) const override { return 0.f; }
};
