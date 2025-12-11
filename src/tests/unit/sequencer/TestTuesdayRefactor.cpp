#include "UnitTest.h"
#include "apps/sequencer/engine/TuesdayTrackEngine.h"
#include "apps/sequencer/model/TuesdaySequence.h"
#include "apps/sequencer/model/TuesdayTrack.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/engine/Engine.h"

#include "apps/sequencer/engine/EngineState.h"
#include "apps/sequencer/engine/MidiOutputEngine.h"
#include "drivers/ClockTimer.h"
#include "drivers/Adc.h"
#include "drivers/Dac.h"
#include "drivers/Dio.h"
#include "drivers/Midi.h"
#include "drivers/UsbMidi.h"

// Define our own MockGateOutput that does NOT inherit from the real GateOutput
// The real GateOutput depends on sim::Simulator which we are linking against,
// causing duplicate symbols if we try to define Simulator mocks.
// But Engine expects reference to GateOutput type.
// We can't easily swap the type in Engine constructor signature.
// Engine takes `GateOutput &gateOutput`.
// GateOutput is defined in drivers/GateOutput.h.
// In Sim, that includes Simulator.h.
// We are linking against `core` which contains `Simulator.cpp`.
// So we should NOT redefine `sim::Simulator::instance`.
// But we need `drivers/GateOutput.h` to define the class.
// And that class uses `sim::Simulator::instance()` in its constructor.
// So we cannot instantiate a real `GateOutput` without a real `Simulator` existing.
// `core` library has `Simulator::instance()` implemented in `Simulator.cpp`.
// So we just need to ensure `sim::Simulator` is initialized?
// Or maybe `core` library is NOT linked correctly?
// The error says "duplicate symbol ... in libcore.a(Simulator.cpp.o)".
// This means we are linking `libcore.a` AND defining the symbols locally.
// Solution: Do NOT define `sim::Simulator::instance` locally.
// Just include the header.

#include "drivers/GateOutput.h"

// Dummy Drivers
class DummyShiftRegister {
public:
    void init() {}
    void setOutput(int index, bool value) {}
    bool update() { return false; }
    void write(uint8_t channel, uint32_t data) {}
    bool hasChanged() { return false; }
    uint32_t read() { return 0; }
};

class DummyClockTimer : public ClockTimer {
public:
    void init() {}
    void start() {}
    void stop() {}
    void setPeriod(uint32_t period) {}
};

class DummyAdc : public Adc {
public:
    void init() {}
    void start() {}
    void stop() {}
    uint16_t read(int channel) { return 0; }
};

class DummyDac : public Dac {
public:
    void init() {}
    void write(int channel, uint16_t value) {}
};

class DummyDio : public Dio {
public:
    void init() {}
    void write(int pin, bool value) {}
    bool read(int pin) { return false; }
};

// Use the real GateOutput class, but we need to handle its dependency on Simulator
// In the test environment, we might need to initialize Simulator?
// Or just let it crash if it tries to access hardware?
// The dummy drivers above are fine because they inherit virtual interfaces.
// GateOutput is NOT virtual in the codebase?
// drivers/GateOutput.h: class GateOutput { ... }
// It is a concrete class.
// Engine takes `GateOutput&`.
// So we must pass an instance of `GateOutput`.
// `GateOutput` constructor calls `sim::Simulator::instance()`.
// If we construct it, it will call that.
// That symbol exists in libcore.a.
// So it should link fine if we don't redefine it.

class DummyMidi : public Midi {
public:
    void init() {}
    void tx(uint8_t port, uint8_t data) {}
    void tx(uint8_t port, const MidiMessage &message) {}
    bool txReady(uint8_t port) { return true; }
    void process() {}
};

class DummyUsbMidi : public UsbMidi {
public:
    void init() {}
    void tx(uint8_t cable, uint8_t *data) {}
    void tx(uint8_t cable, const MidiMessage &message) {}
    bool txReady(uint8_t cable) { return true; }
    void process() {}
};

UNIT_TEST("TuesdayTrackEngineRefactor") {

    // Setup Environment
    Model model;
    // DummyShiftRegister shiftRegister; // Not used
    DummyClockTimer clockTimer;
    DummyAdc adc;
    DummyDac dac;
    DummyDio dio;
    GateOutput gateOutput; 
    DummyMidi midi;
    DummyUsbMidi usbMidi;
    Engine engine(model, clockTimer, adc, dac, dio, gateOutput, midi, usbMidi);

    model.project().setTrackMode(0, Track::TrackMode::Tuesday);
    Track &track = model.project().track(0);
    TuesdayTrackEngine tuesdayEngine(engine, model, track, nullptr);

    CASE("scale_quantization_pipeline") {
        // Configure Sequence
        TuesdaySequence &seq = track.tuesdayTrack().sequence(0);
        seq.setAlgorithm(0); // TEST Mode (OctSweeps)
        seq.setFlow(1);      // Mode 0
        seq.setOrnament(1);
        seq.setUseScale(true);
        
        // Set Project Scale to Major (C, D, E, F, G, A, B)
        model.project().setScale(0); // Major
        model.project().setRootNote(0);      // C
        
        tuesdayEngine.tick(0);
        float cv0 = tuesdayEngine.cvOutput(0);
        
        tuesdayEngine.tick(192); // 1 PPQN step
        float cv1 = tuesdayEngine.cvOutput(0);
        
        expectEqual(cv0, 0.0f, "Tick 0 should be 0V (C0)");
        expectEqual(cv1, 1.0f, "Tick 1 should be 1V (C1)");
        
        // Switch to Mode 1 (ScaleWalker)
        seq.setFlow(9); 
        tuesdayEngine.tick(0); // Reset/Re-init
        
        model.project().setScale(0); // Major
        tuesdayEngine.reset();
        tuesdayEngine.tick(0); // Note 0
        tuesdayEngine.tick(192); // Note 1
        float cvMajor = tuesdayEngine.cvOutput(0);
        
        model.project().setScale(1); // Pentatonic Minor
        tuesdayEngine.reset();
        tuesdayEngine.tick(0);
        tuesdayEngine.tick(192); // Note 1
        float cvPenta = tuesdayEngine.cvOutput(0);
        
        // 2/12 = 0.1666, 3/12 = 0.25
        expectTrue(std::abs(cvMajor - 0.1666f) < 0.01f, "Major 2nd (D)");
        expectTrue(std::abs(cvPenta - 0.25f) < 0.01f, "Minor 3rd (Eb)");
    }
    
    CASE("velocity_density_gating") {
         TuesdaySequence &seq = track.tuesdayTrack().sequence(0);
         seq.setAlgorithm(0);
         seq.setPower(1); // Low Power
         
         seq.setOrnament(1); // Low Velocity
         tuesdayEngine.reset();
         tuesdayEngine.tick(0);
         expectFalse(tuesdayEngine.gateOutput(0), "Low Power + Low Vel = No Gate");
         
         seq.setPower(8); // Mid Power
         seq.setOrnament(16); // Max Velocity
         tuesdayEngine.reset();
         tuesdayEngine.tick(0);
         expectTrue(tuesdayEngine.gateOutput(0), "Mid Power + High Vel = Gate");
    }

}