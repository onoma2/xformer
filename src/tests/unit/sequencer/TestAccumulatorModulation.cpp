#include "UnitTest.h"

#include "apps/sequencer/engine/NoteTrackEngine.h"
#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/NoteTrack.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/Config.h"
#include "apps/sequencer/engine/Engine.h"

// Minimal dependencies for Engine
#include "apps/sequencer/engine/EngineState.h"
#include "apps/sequencer/engine/MidiOutputEngine.h"
#include "drivers/ClockTimer.h"
#include "drivers/Adc.h"
#include "drivers/Dac.h"
#include "drivers/Dio.h"
#include "drivers/GateOutput.h"
#include "drivers/Midi.h"
#include "drivers/UsbMidi.h"

// Mock implementations for Engine dependencies (same as before)
class DummyShiftRegister : public ShiftRegister {
public:
    DummyShiftRegister() : ShiftRegister() {}
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

class DummyGateOutput : public GateOutput {
public:
    DummyGateOutput(ShiftRegister &shiftRegister) : GateOutput(shiftRegister) {}

    void init() {}
    void write(int channel, bool value) {}
    uint8_t gates() const { return 0; }
};

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

UNIT_TEST("NoteTrackEngine") {

CASE("accumulator_modulation") {
    Model model;

    // Instantiate dummy dependencies for Engine
    DummyShiftRegister shiftRegister;
    DummyClockTimer clockTimer;
    DummyAdc adc;
    DummyDac dac;
    DummyDio dio;
    DummyGateOutput gateOutput(shiftRegister);
    DummyMidi midi;
    DummyUsbMidi usbMidi;

    // Create a real Engine instance
    Engine engine(model, clockTimer, adc, dac, dio, gateOutput, midi, usbMidi);

    // Set track to Note mode
    model.project().setTrackMode(0, Track::TrackMode::Note);
    Track &track = model.project().track(0);
    NoteSequence &sequence = track.noteTrack().sequence(0);

    // Configure the accumulator
    auto &accumulator = const_cast<Accumulator&>(sequence.accumulator());
    accumulator.setEnabled(true);
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setMinValue(-10);
    accumulator.setMaxValue(10);
    accumulator.setStepValue(1);
    accumulator.setOrder(Accumulator::Order::Hold);  // Hold mode for predictable behavior
    accumulator.setPolarity(Accumulator::Polarity::Unipolar);

    // Set up a step that will trigger the accumulator
    sequence.step(0).setGate(true);
    sequence.step(0).setNote(0);  // Starting note: C-3
    sequence.step(0).setAccumulatorTrigger(true);
    
    sequence.step(1).setGate(true);
    sequence.step(1).setNote(0);  // Same base note: C-3
    sequence.step(1).setAccumulatorTrigger(false); // This step won't trigger the accumulator

    // Set up sequence parameters
    sequence.setFirstStep(0);
    sequence.setLastStep(1);

    NoteTrackEngine noteTrackEngine(engine, model, track, nullptr);
    noteTrackEngine.reset();

    // Check initial state
    expectEqual(static_cast<int>(sequence.accumulator().currentValue()), 0, "Initial accumulator value should be 0");

    // Process multiple ticks to advance sequence and trigger accumulator
    for (int i = 0; i < 10; ++i) {
        noteTrackEngine.tick(i * CONFIG_PPQN);
    }

    // The accumulator should now be at value 1 (increased by 1 due to step 0 trigger)
    expectEqual(static_cast<int>(sequence.accumulator().currentValue()), 1, "Accumulator value should be 1 after first trigger");

    // Reset and test again
    noteTrackEngine.restart();
    expectEqual(static_cast<int>(sequence.accumulator().currentValue()), 0, "Accumulator should reset to 0 after restart");
}

} // UNIT_TEST("NoteTrackEngine")