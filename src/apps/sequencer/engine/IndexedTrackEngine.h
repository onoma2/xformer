#pragma once

#include "TrackEngine.h"

#include "model/Track.h"
#include "model/IndexedSequence.h"
#include "model/Scale.h"

class IndexedTrackEngine : public TrackEngine {
public:
    IndexedTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _indexedTrack(track.indexedTrack())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Indexed; }

    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual void changePattern() override;

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return !mute() && _gateTimer > 0; }
    virtual float cvOutput(int index) const override { return _cvOutput; }
    virtual float sequenceProgress() const override;

    const IndexedSequence &sequence() const { return *_sequence; }
    bool isActiveSequence(const IndexedSequence &sequence) const { return &sequence == _sequence; }

    // For Testing/UI
    int currentStep() const { return _currentStepIndex; }

private:
    void advanceStep();
    void triggerStep();
    float noteIndexToVoltage(int8_t noteIndex) const;
    void applyModulation(float cv, const IndexedSequence::RouteConfig &cfg,
                        uint16_t &duration, uint16_t &gate, int8_t &note);

    IndexedTrack &_indexedTrack;
    IndexedSequence *_sequence = nullptr;

    // === Timing state ===
    uint32_t _stepTimer = 0;        // Counts up to step.duration
    uint32_t _gateTimer = 0;        // Counts down from step.gateLength
    int _currentStepIndex = 0;      // Current step (0 to activeLength-1)
    bool _running = true;

    // === Output ===
    float _cvOutput = 0.0f;
    bool _activity = false;
};
