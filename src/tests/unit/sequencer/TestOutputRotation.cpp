#include "UnitTest.h"
#include "apps/sequencer/engine/Engine.h"
#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/model/Project.h"

UNIT_TEST("OutputRotation") {

    Model model;
    
    // Minimal Mock Classes - remove override if base methods are not virtual
    struct MockClockTimer : ClockTimer { void init() {} };
    struct MockAdc : Adc { void init() {} int channel(int) const { return 0; } };
    struct MockDac : Dac { 
        void init() {} 
        void setValue(int channel, int value) { values[channel] = value; }
        int values[CONFIG_CHANNEL_COUNT] = {0};
    };
    struct MockDio : Dio { void init() {} };
    struct MockGateOutput : GateOutput { 
        void init() {} 
        void setGate(int channel, bool value) { values[channel] = value; }
        bool values[CONFIG_CHANNEL_COUNT] = {false};
    };
    struct MockMidi : Midi { void init() {} bool recv(MidiMessage *) { return false; } bool send(const MidiMessage &) { return true; } };
    struct MockUsbMidi : UsbMidi { void init() {} bool recv(uint8_t *, MidiMessage *) { return false; } bool send(uint8_t, const MidiMessage &) { return true; } };

    MockClockTimer clockTimer;
    MockAdc adc;
    MockDac dac;
    MockDio dio;
    MockGateOutput gateOutput;
    MockMidi midi;
    MockUsbMidi usbMidi;

    Engine engine(model, clockTimer, adc, dac, dio, gateOutput, midi, usbMidi);
    engine.init();

    // Helper to set track output (voltage)
    auto setTrackVoltage = [&](int trackIndex, float voltage) {
        model.project().setTrackMode(trackIndex, Track::TrackMode::Curve);
        auto &track = model.project().track(trackIndex);
        // For CurveTrack, we can just set the offset to set a static voltage for testing
        // 1V = 100 int units in Offset
        track.curveTrack().setOffset(int(voltage * 100)); 
        // Ensure slide is 0 for instant update
        track.curveTrack().setSlideTime(0);
    };

    CASE("Selective_CV_Rotation") {
        // Setup: Tracks 0, 1, 2, 3
        // Track 0 -> 1V
        // Track 1 -> 2V
        // Track 2 -> 3V
        // Track 3 -> 4V
        setTrackVoltage(0, 1.0f);
        setTrackVoltage(1, 2.0f);
        setTrackVoltage(2, 3.0f);
        setTrackVoltage(3, 4.0f);

        // Layout: Map Tracks to CV Outputs
        // Out 0 <- Track 0
        // Out 1 <- Track 1
        // Out 2 <- Track 2
        // Out 3 <- Track 3
        auto &project = model.project();
        project.setCvOutputTrack(0, 0);
        project.setCvOutputTrack(1, 1);
        project.setCvOutputTrack(2, 2);
        project.setCvOutputTrack(3, 3);

        // Initial State: No Rotation
        project.track(0).setCvOutputRotate(0);
        project.track(1).setCvOutputRotate(0);
        project.track(2).setCvOutputRotate(0);
        project.track(3).setCvOutputRotate(0);

        // Run Engine Update (simulates one tick/frame)
        engine.update();

        // Verify Static Mapping
        // DAC values are raw integers. Model uses calibration.
        // Rough check: Output 0 should correspond to Track 0's voltage.
        // Since we can't easily predict DAC int values without calibration curve,
        // let's just check relative values or equality if we set voltages distinctly.
        // Actually, Engine writes to _cvOutput, which writes to Dac.
        // Let's assume identity for now or check ordering.
        
        // Better: We can spy on `_cvOutput.setChannel` by checking `dac.values`.
        // But `_cvOutput` applies calibration.
        
        // Let's define rotation logic:
        // Set Rotation on Tracks 0, 1, 2. Leave 3 static.
        project.track(0).setCvOutputRotate(1);
        project.track(1).setCvOutputRotate(1);
        project.track(2).setCvOutputRotate(1);
        // Track 3 rotate = 0

        // Run Engine
        engine.update();

        // Verify Logic
        // We expect Output 0 to have Track 2's value
        // We expect Output 1 to have Track 0's value
        // We expect Output 2 to have Track 1's value
        // We expect Output 3 to have Track 3's value

        // Note: DAC values depend on calibration. 
        // Track 0 = 1V, Track 1 = 2V, Track 2 = 3V, Track 3 = 4V.
        // Assuming linear positive calibration for simplicity in unit test environment (or checking relative order)
        
        int val0 = dac.values[0];
        int val1 = dac.values[1];
        int val2 = dac.values[2];
        int val3 = dac.values[3];

        // Debug output
        // printf("DAC: %d %d %d %d\n", val0, val1, val2, val3);

        // Since we can't assert exact DAC values without calibration mocks, 
        // we check that the values shifted as expected relative to a baseline run.
        // Ideally, we would mock the track engines to return specific "CV" float values directly,
        // but we are using the real engine logic which goes through calibration.

        // However, we know:
        // Track 2 (3V) > Track 1 (2V) > Track 0 (1V)
        
        // So for rotation:
        // Output 0 (Track 2, 3V) > Output 2 (Track 1, 2V) > Output 1 (Track 0, 1V)
        // Output 3 (Track 3, 4V) should be highest.

        // Check Output 3 is static and highest
        expectTrue(val3 > val0 && val3 > val1 && val3 > val2, "Output 3 should be Track 3 (4V)");

        // Check Rotation:
        // Output 0 should be Track 2 (3V)
        // Output 2 should be Track 1 (2V)
        // Output 1 should be Track 0 (1V)
        
        expectTrue(val0 > val2, "Output 0 (Trk2) > Output 2 (Trk1)");
        expectTrue(val2 > val1, "Output 2 (Trk1) > Output 1 (Trk0)");
    }
}
