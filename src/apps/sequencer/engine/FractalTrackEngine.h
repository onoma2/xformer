#pragma once

#include "TrackEngine.h"
#include "model/FractalSequence.h"
#include "model/FractalTrack.h"

// Skeleton engine — registration + valid TrackEngine overrides only. Capture /
// replay / branch / ornament behavior lands in a later phase. See
// docs/superpowers/specs/2026-05-17-fractal-track-design.md.
class FractalTrackEngine final : public TrackEngine {
public:
    FractalTrackEngine(Engine &engine, const Model &model, Track &track);

    virtual void reset() override;
    virtual void restart() override;
    virtual void stop() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Fractal; }

    virtual bool gateOutput(int index) const override { return false; }
    virtual float cvOutput(int index) const override { return 0.f; }

    virtual void changePattern() override {
        _sequence = &(_fractalTrack.sequence(pattern()));
    }

    bool activity() const override { return _activity; }

private:
    FractalTrack &_fractalTrack;
    FractalSequence *_sequence = nullptr;
    bool _activity = false;
};
