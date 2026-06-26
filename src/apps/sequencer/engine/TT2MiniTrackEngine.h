#pragma once

#include "TrackEngine.h"

#include "TT2Runner.h"
#include "TT2Host.h"
#include "TeletypeOutputState.h"
#include "TT2OutputShaping.h"

#include "model/Track.h"
#include "model/TT2MiniTrack.h"
#include "model/TT2Config.h"

#include <cstdint>

// Rising-edge detector shared with TT2TrackEngine (tt2RisingEdges) lives in
// TT2TrackEngine.h. Mini mirrors that engine retemplated on TT2ConfigMini.
#include "TT2TrackEngine.h"

inline int tt2SceneIndex(int pattern, int sceneCount) { return pattern % sceneCount; }

class TT2MiniTrackEngine final : public TrackEngine, public TT2Host {
public:
    TT2MiniTrackEngine(Engine &engine, const Model &model, Track &track) :
        TrackEngine(engine, model, track),
        _miniTrack(track.tt2MiniTrack())
    {
        changePattern();
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::TeletypeMini; }

    virtual void reset() override {
        init(_output);
        tt2DelayClear(_miniTrack.runtime());
        _tickCount = 0;
        _msAccum = 0.f;
        _metroAccumMs = 0;
        _firstTick = true;
    }
    virtual void restart() override {
        _firstTick = true;
        _metroAccumMs = 0;
    }

    virtual void changePattern() override {
        _activeScene = tt2SceneIndex(pattern(), TT2ConfigMini::SceneCount);
        // seamless: runtime carries, no reset, no re-boot
    }

    virtual TickResult tick(uint32_t tick) override {
        (void)tick;
        return NoUpdate;
    }

    // Boot script runs once on the first refresh (transport-independent), so the
    // free-running metro starts with init's settings applied. Mirrors
    // TT2TrackEngine::update retemplated on TT2ConfigMini.
    virtual void update(float dt) override {
        ScopedHost host(this);
        if (_firstTick) {
            _firstTick = false;
            runScript(uint8_t(_miniTrack.program(_activeScene).bootScriptIndex));
        }
        updateInputTriggers();
        sampleInputs();
        if (dt <= 0.f) {
            return;
        }
        _msAccum += dt * 1000.f;
        int whole = int(_msAccum);
        if (whole > 0) {
            _msAccum -= float(whole);
            for (int i = 0; i < TT2_OUTPUT_CV_COUNT; ++i) {
                tt2AdvanceCvSlew(_output.cv[i], whole);
            }
            for (int i = 0; i < TT2_OUTPUT_TR_COUNT; ++i) {
                tt2AdvanceTrPulse(_output.tr[i], whole);
            }
            auto &program = _miniTrack.program(_activeScene);
            auto &runtime = _miniTrack.runtime();
            runtime.clockMs += uint32_t(whole);
            if (runtime.metroResetReq) { _metroAccumMs = 0; runtime.metroResetReq = 0; }
            tt2AdvanceDelays(program, runtime, _output, whole);
            tt2AdvanceMetro(program, runtime, _output, whole, _metroAccumMs, metroTempo());
        }
    }

    virtual bool activity() const override { return _output.cvDirty != 0 || _output.trDirty != 0; }

    virtual bool receiveMidi(MidiPort port, const MidiMessage &message) override;

    virtual bool gateOutput(int index) const override {
        if (mute() || index < 0 || index >= TT2_OUTPUT_TR_COUNT) {
            return false;
        }
        return _output.tr[index].level != 0;
    }
    virtual float cvOutput(int index) const override {
        if (index < 0 || index >= TT2_OUTPUT_CV_COUNT) {
            return 0.f;
        }
        if (!(_output.cvDirty & (1 << index))) {
            return 0.f;
        }
        float v = TT2TrackEngine::rawToVolts(int16_t(_output.cv[index].currentQ >> 8));
        const auto &p = _miniTrack.program(_activeScene);
        return Tt2OutputShaping::shapeCv(v, p.cvOutputRange[index], p.cvOutputOffset[index],
            p.cvOutputQuantizeScale[index], p.cvOutputRootNote[index],
            _model.project().rootNote());
    }

    // Execute one script by index against the active scene's program/runtime.
    TT2EvalResult runScript(uint8_t scriptIndex) {
        return ::runScript(_miniTrack.program(_activeScene), _miniTrack.runtime(), _output, scriptIndex);
    }

    void triggerScript(int scriptIndex) override;
    void toggleScriptMute(int scriptIndex) override;
    void toggleMetroActive() override;

    TT2OutputState &output() { return _output; }
    const TT2OutputState &output() const { return _output; }

    bool inputState(uint8_t index) const;

    // TT2Host — live engine / cross-track access for W*/BUS/RT ops (TT2Host.h).
    int16_t hostTempo() override;
    void hostSetTempo(int16_t bpm) override;
    int16_t hostTransportRunning() override;
    void hostSetTransportRunning(int16_t state) override;
    int16_t hostBarFraction(uint8_t bars) override;
    int16_t hostWms(uint8_t mult) override;
    int16_t hostWtu(uint8_t div, uint8_t mult) override;
    int16_t hostTrackPattern(uint8_t track) override;
    void hostSetTrackPattern(uint8_t track, uint8_t pattern) override;
    int16_t hostNoteGateGet(uint8_t track, uint8_t step) override;
    void hostNoteGateSet(uint8_t track, uint8_t step, int16_t v) override;
    int16_t hostNoteNoteGet(uint8_t track, uint8_t step) override;
    void hostNoteNoteSet(uint8_t track, uint8_t step, int16_t v) override;
    int16_t hostNoteGateHere(uint8_t track) override;
    int16_t hostNoteNoteHere(uint8_t track) override;
    int16_t hostRoutingSource(uint8_t index) override;
    int16_t hostBusCv(uint8_t index) override;
    void hostSetBusCv(uint8_t index, int16_t raw) override;
    Modulator &hostModulator(uint8_t idx) override;
    int16_t hostModulatorOutput(uint8_t idx) override;
    void hostModulatorTrigger(uint8_t idx) override;
    GeodeConfig &hostGeodeConfig() override;
    int16_t hostGeodeMix() override;
    void hostGeodeTriggerVoice(uint8_t v, int16_t divs, int16_t repeats) override;
    void hostGeodeTriggerAll(int16_t divs, int16_t repeats) override;
    int16_t hostGeodeRun() override;
    void hostGeodeSetRun(int16_t macro) override;

private:
    struct ScopedHost {
        TT2Host *prev;
        explicit ScopedHost(TT2Host *h) : prev(tt2ActiveHost()) { tt2SetActiveHost(h); }
        ~ScopedHost() { tt2SetActiveHost(prev); }
    };

    void updateInputTriggers();
    void sampleInputs();
    float cvSourceVolts(TT2CvInputSource source) const;
    void processMidiMessage(const MidiMessage &message);
    float metroTempo() const;

    TT2MiniTrack &_miniTrack;
    int _activeScene = -1;
    TT2OutputState _output;
    uint32_t _tickCount = 0;
    float _msAccum = 0.f;
    int _metroAccumMs = 0;
    bool _firstTick = true;
    bool _prevInputState[TT2ConfigMini::TriggerInputCount] = {};
};

static_assert(sizeof(TT2MiniTrackEngine) <= 944, "");
