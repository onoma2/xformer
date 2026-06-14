#pragma once

#include "TrackEngine.h"

#include "TT2Runner.h"
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

class TT2TrackEngine final : public TrackEngine {
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
            runScript(uint8_t(_tt2Track.program().bootScriptIndex));
            return TickResult(CvUpdate | GateUpdate);
        }
        return NoUpdate;
    }

    // Real elapsed wall-time refresh (the 0.f recompose call is a no-op).
    // Drives the ms delay queue and the metro, accumulating sub-millisecond
    // remainder so slow refreshes still advance time accurately.
    virtual void update(float dt) override {
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

private:
    // Defined in TT2TrackEngine.cpp (needs the full Engine definition to read
    // cvInput/gateOutput/trackEngine).
    void updateInputTriggers();
    void sampleInputs();
    bool inputState(uint8_t index) const;
    float cvSourceVolts(TT2CvInputSource source) const;

    TT2Track &_tt2Track;
    TT2OutputState _output;
    uint32_t _tickCount = 0;
    float _msAccum = 0.f;
    int _metroAccumMs = 0;
    bool _firstTick = true;
    bool _prevInputState[TT2_TRIGGER_INPUT_COUNT] = {};
};

static_assert(sizeof(TT2TrackEngine) <= 944, "TT2TrackEngine too large (TeletypeTrackEngine at 944 is the container limit)");
