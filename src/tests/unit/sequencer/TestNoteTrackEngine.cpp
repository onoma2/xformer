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
#include "drivers/ShiftRegister.h"
#include "drivers/GateOutput.h"
#include "drivers/Midi.h"
#include "drivers/UsbMidi.h"

// Minimal dummy implementations for Engine dependencies
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
    // Callback is a private type alias, so we can't implement setCallback directly.
    // We will ignore it for now, as it's not used by NoteTrackEngine in this test.
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
    // RxCallback is a private type alias, so we can't implement setRxCallback directly.
    // We will ignore it for now, as it's not used by NoteTrackEngine in this test.
    void tx(uint8_t port, uint8_t data) {}
    void tx(uint8_t port, const MidiMessage &message) {}
    bool txReady(uint8_t port) { return true; }
    void process() {}
};

class DummyUsbMidi : public UsbMidi {
public:
    void init() {}
    // RxCallback, ConnectCallback, DisconnectCallback are private type aliases.
    // We will ignore them for now.
    void tx(uint8_t cable, uint8_t *data) {}
    void tx(uint8_t cable, const MidiMessage &message) {}
    bool txReady(uint8_t cable) { return true; }
    void process() {}
};

// Mock MidiOutputEngine - methods are not virtual in base class
class MockMidiOutputEngine : public MidiOutputEngine {
public:
    // Need a non-mock Engine instance for this constructor
    MockMidiOutputEngine(Engine &engine, Model &model) : MidiOutputEngine(engine, model) {}
    void sendGate(int trackIndex, bool gate) {}
    void sendCv(int trackIndex, float cv) {}
    void sendSlide(int trackIndex, bool slide) {}
};


UNIT_TEST("NoteTrackEngine") {

CASE("accumulator_integration") {
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

    // Set track mode through the project interface (the proper way now that setTrackMode is private again)
    model.project().setTrackMode(0, Track::TrackMode::Note);
    Track &track = model.project().track(0); // Use track from model

    NoteTrackEngine noteTrackEngine(engine, model, track, nullptr);

    NoteSequence &sequence = track.noteTrack().sequence(0); // Access sequence via track.noteTrack()
    sequence.step(0).setGate(true);
    sequence.step(0).setAccumulatorTrigger(true);
    sequence.step(0).setRetrigger(0); // No ratchets for now

    Accumulator &accumulator = sequence.accumulator();
    accumulator.setEnabled(true);
    accumulator.setDirection(Accumulator::Direction::Up);
    accumulator.setMinValue(0);
    accumulator.setMaxValue(10);
    accumulator.setStepValue(1);
    accumulator.setOrder(Accumulator::Order::Wrap);

    noteTrackEngine.reset();

    // Simulate a few ticks
    for (int i = 0; i < 5; ++i) {
        noteTrackEngine.tick(i * CONFIG_PPQN); // Advance by one quarter note
    }

    // Expect accumulator to have ticked 5 times
    expectEqual(static_cast<int>(accumulator.currentValue()), 5, "accumulator currentValue should be 5");
}

} // UNIT_TEST("NoteTrackEngine")