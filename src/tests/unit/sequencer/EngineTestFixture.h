#pragma once

#include "sim/Simulator.h"

#include "drivers/ClockTimer.h"
#include "drivers/Adc.h"
#include "drivers/Dac.h"
#include "drivers/Dio.h"
#include "drivers/GateOutput.h"
#include "drivers/Midi.h"
#include "drivers/UsbMidi.h"
#include "drivers/UsbH.h"

#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/engine/Engine.h"

// Headless engine harness: real sim drivers + the Simulator singleton + Model +
// Engine, driven by Simulator::wait() (deterministic fake-tick time, no GUI).
// The Simulator must be constructed before any driver — each driver touches
// Simulator::instance() in its ctor. Ticks advance only when Simulator::step()
// runs (via wait()); slaveTick() alone queues nothing usable. See
// .tasks/review-fixes-tdd/PLAN.md Phase 0.
class EngineTestFixture {
public:
    EngineTestFixture() {
        _model.init();
        _engine.init();
        // The Clock boots with a pending Reset event (Clock.h: _requestedEvents
        // = Reset). On real hardware the engine runs update() from boot, so that
        // Reset drains before play is pressed. Drain it here — otherwise a Reset
        // queued alongside the first Start would clobber the running state.
        _engine.update();
        _simulator.addUpdateCallback([this] { _engine.update(); });
    }

    Model &model() { return _model; }
    Project &project() { return _model.project(); }
    Engine &engine() { return _engine; }

    void start() { _engine.clockStart(); }
    void stop() { _engine.clockStop(); }

    // Advance N simulated milliseconds; each is one Simulator::step().
    void advance(int ms) { _simulator.wait(ms); }

    // Drive a CV input to a fixed voltage (for deterministic routing sources).
    void setCvIn(int channel, float volts) { _simulator.setAdc(channel, volts); }

private:
    sim::Simulator _simulator{ sim::Target{ []{}, []{}, []{} } };
    ClockTimer _clockTimer;
    Adc _adc;
    Dac _dac;
    Dio _dio;
    GateOutput _gateOutput;
    Midi _midi;
    UsbMidi _usbMidi;
    UsbH _usbH;
    Model _model;
    Engine _engine{ _model, _clockTimer, _adc, _dac, _dio, _gateOutput, _midi, _usbMidi, _usbH };
};
