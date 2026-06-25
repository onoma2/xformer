#pragma once

#include "Config.h"

#include "EngineState.h"
#include "MidiPort.h"

#include "model/Model.h"

#include "core/midi/MidiMessage.h"
#include "core/utils/EnumUtils.h"

#include <cstdint>

class Engine;

#if CONFIG_ENABLE_SANITIZE
# define SANITIZE_TRACK_MODE(_actual_, _expected_) ASSERT(_actual_ == _expected_, "invalid track mode");
#else // CONFIG_ENABLE_SANITIZE
# define SANITIZE_TRACK_MODE(_actual_, _expected_) {}
#endif // CONFIG_ENABLE_SANITIZE

class TrackEngine {
public:
    // Set of updates resulting from calling tick().
    enum TickResult {
        NoUpdate    = 0,
        CvUpdate    = (1<<0),
        GateUpdate  = (1<<1),
    };

    TrackEngine(Engine &engine, const Model &model, Track &track) :
        _engine(engine),
        _model(model),
        _track(track),
        _trackState(model.project().playState().trackState(track.trackIndex()))
    {
        changePattern();
    }

    template<typename T>
    const T &as() const {
        SANITIZE_TRACK_MODE(_track.trackMode(), trackMode());
        return *static_cast<const T *>(this);
    }

    template<typename T>
    T &as() {
        SANITIZE_TRACK_MODE(_track.trackMode(), trackMode());
        return *static_cast<T *>(this);
    }

    virtual Track::TrackMode trackMode() const = 0;

    // sequencer control

    virtual void reset() = 0;
    virtual void restart() = 0;
    virtual TickResult tick(uint32_t tick) = 0;
    virtual void update(float dt) = 0;
    // Optional hook to drop gates/cleanup when transport stops
    virtual void stop() {}

    virtual void changePattern() {}

    // UI-control hooks (Seam-2): Teletype engines override; default no-ops so the
    // script page can drive any engine through a TrackEngine& without a cast.
    virtual void triggerScript(int scriptIndex) {}
    virtual void toggleScriptMute(int scriptIndex) {}
    virtual void toggleMetroActive() {}

    virtual bool receiveMidi(MidiPort port, const MidiMessage &message) { return false; }
    virtual void monitorMidi(uint32_t tick, const MidiMessage &message) {}
    virtual void clearMidiMonitoring() {}

    // track output

    virtual bool activity() const = 0;
    virtual bool gateOutput(int index) const = 0;
    virtual float cvOutput(int index) const = 0;

    virtual float sequenceProgress() const { return -1.f; }

    // helpers

    bool isSelected() const { return _model.project().selectedTrackIndex() == _track.trackIndex(); }

    int swing() const { return _model.project().swing(); }

    int pattern() const { return _trackState.pattern(); }
    bool mute() const { return _trackState.mute(); }
    bool fill() const { return _trackState.fill(); }
    int fillAmount() const { return _trackState.fillAmount(); }

protected:
    Engine &_engine;
    const Model &_model;
    Track &_track;
    const PlayState::TrackState &_trackState;
};

ENUM_CLASS_OPERATORS(TrackEngine::TickResult)

#undef SANITIZE_TRACK_MODE
