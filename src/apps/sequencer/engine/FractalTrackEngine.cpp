#include "FractalTrackEngine.h"
#include "Engine.h"

#include "model/FractalSequence.h"
#include "model/FractalTrack.h"
#include "model/Model.h"

FractalTrackEngine::FractalTrackEngine(Engine &engine, const Model &model, Track &track) :
    TrackEngine(engine, model, track),
    _fractalTrack(track.fractalTrack())
{
    reset();
}

void FractalTrackEngine::reset() {
    _sequence = &(_fractalTrack.sequence(pattern()));
    _activity = false;
}

void FractalTrackEngine::restart() {
}

void FractalTrackEngine::stop() {
    _activity = false;
}

TrackEngine::TickResult FractalTrackEngine::tick(uint32_t tick) {
    return TickResult::NoUpdate;
}

void FractalTrackEngine::update(float dt) {
}
