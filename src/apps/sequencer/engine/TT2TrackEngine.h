#pragma once

#include "TrackEngine.h"

#include "TT2Runner.h"
#include "TT2Host.h"
#include "TeletypeOutputState.h"

#include "model/Track.h"
#include "model/TT2Track.h"

#include <cstdint>

// Rising-edge detector over the trigger inputs. Returns a bitmask of inputs
// that went low->high this pass; updates prev[] in place. Pure (no Engine) so
// the firing logic is unit-testable without an Engine context.
inline uint8_t tt2RisingEdges(const bool *now, bool *prev, int count) {
    uint8_t fired = 0;
    for (int i = 0; i < count; ++i) {
        if (now[i] && !prev[i]) fired |= uint8_t(1 << i);
        prev[i] = now[i];
    }
    return fired;
}

class TT2TrackEngine final : public TrackEngine, public TT2Host {
public:
    TT2TrackEngine(Engine &engine, const Model &model, Track &track) :
        TrackEngine(engine, model, track),
        _tt2Track(track.tt2Track())
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::TeletypeV2; }

    virtual void reset() override {
        init(_output);
        tt2DelayClear(_tt2Track.runtime());
        _tickCount = 0;
        _msAccum = 0.f;
        _metroAccumMs = 0;
        _firstTick = true;
    }
    virtual void restart() override {
        _firstTick = true;
        _metroAccumMs = 0;
    }

    // Run the boot/init script once on (re)start. Periodic work (metro) and
    // time-based work (delays) run in update(dt) on the real ms clock.
    virtual TickResult tick(uint32_t tick) override {
        (void)tick;
        if (_firstTick) {
            _firstTick = false;
            ScopedHost host(this);
            runScript(uint8_t(_tt2Track.program().bootScriptIndex));
            return TickResult(CvUpdate | GateUpdate);
        }
        return NoUpdate;
    }

    // Real elapsed wall-time refresh (the 0.f recompose call is a no-op).
    // Drives the ms delay queue and the metro, accumulating sub-millisecond
    // remainder so slow refreshes still advance time accurately.
    virtual void update(float dt) override {
        ScopedHost host(this);
        // Sample trigger inputs + CV-mapped inputs every refresh (async, not clocked).
        updateInputTriggers();
        sampleInputs();
        if (dt <= 0.f) {
            return;
        }
        _msAccum += dt * 1000.f;
        int whole = int(_msAccum);
        if (whole > 0) {
            _msAccum -= float(whole);
            // Shape outputs first so anything seeded this pass (trigger scripts,
            // delays/metro firing below) starts its ramp/pulse next pass at full
            // length rather than losing `whole` ms on arming.
            for (int i = 0; i < TT2_OUTPUT_CV_COUNT; ++i) {
                tt2AdvanceCvSlew(_output.cv[i], whole);
            }
            for (int i = 0; i < TT2_OUTPUT_TR_COUNT; ++i) {
                tt2AdvanceTrPulse(_output.tr[i], whole);
            }
            auto &program = _tt2Track.program();
            auto &runtime = _tt2Track.runtime();
            runtime.clockMs += uint32_t(whole);  // drives TIME / LAST
            if (runtime.metroResetReq) { _metroAccumMs = 0; runtime.metroResetReq = 0; }  // M.RESET
            tt2AdvanceDelays(program, runtime, _output, whole);
            tt2AdvanceMetro(program, runtime, _output, whole, _metroAccumMs);
        }
    }

    virtual bool activity() const override { return _output.cvDirty != 0 || _output.trDirty != 0; }

    // Incoming MIDI: filter by the track's MidiSourceConfig, populate the
    // runtime MIDI buffer (MI.* reads), and fire the configured script.
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
        return rawToVolts(int16_t(_output.cv[index].currentQ >> 8));
    }

    // Execute one script by index against the bound track's program/runtime.
    TT2EvalResult runScript(uint8_t scriptIndex) {
        return ::runScript(_tt2Track.program(), _tt2Track.runtime(), _output, scriptIndex);
    }

    // UI live-exec / trigger entry points. Each installs this engine as the
    // scoped host (so W*/BUS/MO/G resolve) AND holds Engine::lock() for the
    // duration — the UI thread mutates runtime/output that engine update() also
    // touches. Synchronous on the UI thread, mirroring the legacy live path.
    // Defined in the .cpp (need the full Engine definition to lock).
    TT2EvalResult runLiveCommand(const TT2Command &cmd);
    void triggerScript(int scriptIndex);

    TT2OutputState &output() { return _output; }
    const TT2OutputState &output() const { return _output; }

    // Convert a raw 0..16383 CV value to Performer float volts (-5V .. +5V).
    static float rawToVolts(int16_t raw) {
        if (raw < 0) raw = 0;
        if (raw > 16383) raw = 16383;
        return raw / 16383.f * 10.f - 5.f;
    }

    // Inverse: clamp -5V..+5V into raw 0..16383.
    static int16_t voltsToRaw(float volts) {
        if (volts < -5.f) volts = -5.f;
        if (volts > 5.f) volts = 5.f;
        return int16_t((volts + 5.f) / 10.f * 16383.f + 0.5f);
    }

    // TT2Host — live engine / cross-track access for W*/BUS/RT ops (TT2Host.h).
    // All defined in TT2TrackEngine.cpp (need the full Engine definition).
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
    // Sets this engine as the active TT2Host for the duration of script
    // execution, restoring the previous host on scope exit.
    struct ScopedHost {
        TT2Host *prev;
        explicit ScopedHost(TT2Host *h) : prev(tt2ActiveHost()) { tt2SetActiveHost(h); }
        ~ScopedHost() { tt2SetActiveHost(prev); }
    };

    // Defined in TT2TrackEngine.cpp (needs the full Engine definition to read
    // cvInput/gateOutput/trackEngine).
    void updateInputTriggers();
    void sampleInputs();
    bool inputState(uint8_t index) const;
    float cvSourceVolts(TT2CvInputSource source) const;
    void processMidiMessage(const MidiMessage &message);

    TT2Track &_tt2Track;
    TT2OutputState _output;
    uint32_t _tickCount = 0;
    float _msAccum = 0.f;
    int _metroAccumMs = 0;
    bool _firstTick = true;
    bool _prevInputState[TT2_TRIGGER_INPUT_COUNT] = {};
};

static_assert(sizeof(TT2TrackEngine) <= 944, "TT2TrackEngine too large (TeletypeTrackEngine at 944 is the container limit)");
