#pragma once

#include "TrackEngine.h"

#include "TT2Runner.h"
#include "TeletypeOutputState.h"

#include "model/Track.h"
#include "model/TT2Track.h"

#include <cstdint>

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
        _tickCount = 0;
        _firstTick = true;
    }
    virtual void restart() override { _firstTick = true; }

    // Minimal tick model: run the boot/init script once on (re)start, then the
    // metro script every clockDivisor master ticks. No real metro/trigger
    // scheduler yet — this is the first live slice. Empty scripts are no-ops.
    virtual TickResult tick(uint32_t tick) override {
        (void)tick;
        bool ran = false;
        if (_firstTick) {
            _firstTick = false;
            runScript(uint8_t(_tt2Track.program().bootScriptIndex));
            ran = true;
        }
        int div = _tt2Track.program().clockDivisor;
        if (div < 1) div = 1;
        if ((_tickCount % uint32_t(div)) == 0) {
            runScript(uint8_t(TT2_METRO_SCRIPT));
            ran = true;
        }
        ++_tickCount;
        return ran ? TickResult(CvUpdate | GateUpdate) : NoUpdate;
    }

    // Real elapsed wall-time refresh (the 0.f recompose call is a no-op).
    // Drains the ms delay queue, accumulating sub-millisecond remainder so
    // slow refreshes still advance delays accurately.
    virtual void update(float dt) override {
        if (dt <= 0.f) {
            return;
        }
        _msAccum += dt * 1000.f;
        int whole = int(_msAccum);
        if (whole > 0) {
            _msAccum -= float(whole);
            tt2AdvanceDelays(_tt2Track.program(), _tt2Track.runtime(), _output, whole);
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
        return rawToVolts(_output.cv[index].targetRaw);
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

private:
    TT2Track &_tt2Track;
    TT2OutputState _output;
    uint32_t _tickCount = 0;
    float _msAccum = 0.f;
    bool _firstTick = true;
};

static_assert(sizeof(TT2TrackEngine) <= 944, "TT2TrackEngine too large (TeletypeTrackEngine at 944 is the container limit)");
