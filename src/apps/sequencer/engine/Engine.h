#pragma once

#include "EngineState.h"
#include "Clock.h"
#include "WallClock.h"
#include "TapTempo.h"
#include "NudgeTempo.h"
#include "TrackEngine.h"
#include "NoteTrackEngine.h"
#include "CurveTrackEngine.h"
#include "MidiCvTrackEngine.h"
#include "TuesdayTrackEngine.h"
#include "DiscreteMapTrackEngine.h"
#include "IndexedTrackEngine.h"
#include "StochasticTrackEngine.h"
#include "FractalTrackEngine.h"
#include "PhaseFluxTrackEngine.h"
#include "TT2TrackEngine.h"
#include "TT2MiniTrackEngine.h"
#include "CvInput.h"
#include "CvOutput.h"
#include "RoutingEngine.h"
#include "MidiOutputEngine.h"
#include "ModulatorEngine.h"
#include "GeodeEngine.h"
#include "MidiPort.h"
#include "MidiLearn.h"
#include "CvGateToMidiConverter.h"
#include "UpdateReducer.h"

#include "model/Model.h"

#include "drivers/ClockTimer.h"
#include "drivers/Adc.h"
#include "drivers/Dac.h"
#include "drivers/Dio.h"
#include "drivers/GateOutput.h"
#include "drivers/Midi.h"
#include "drivers/UsbMidi.h"
#include "drivers/UsbH.h"

#include <array>

#include <cstdint>

class Engine : private Clock::Listener {
public:
    using TrackEngineContainer = Container<NoteTrackEngine, CurveTrackEngine, MidiCvTrackEngine, TuesdayTrackEngine, DiscreteMapTrackEngine, IndexedTrackEngine, StochasticTrackEngine, FractalTrackEngine, PhaseFluxTrackEngine, TT2TrackEngine, TT2MiniTrackEngine>;
    using TrackEngineContainerArray = std::array<TrackEngineContainer, CONFIG_TRACK_COUNT>;
    using TrackEngineArray = std::array<TrackEngine *, CONFIG_TRACK_COUNT>;

    using MidiReceiveHandler = std::function<bool(MidiPort port, uint8_t cable, const MidiMessage &message)>;

    using UsbMidiConnectHandler = std::function<void(uint16_t vendorId, uint16_t productId)>;
    using UsbMidiDisconnectHandler = std::function<void()>;

    using KeyboardReceiveHandler = std::function<void(uint8_t keycode, uint8_t modifiers, uint8_t pressed)>;
    using HidConnectHandler = std::function<void(uint8_t device_id, int type)>;
    using HidDisconnectHandler = std::function<void(uint8_t device_id)>;

    using MessageHandler = std::function<void(const char *text, uint32_t duration)>;

    enum ClockSource {
        ClockSourceExternal,
        ClockSourceMidi,
        ClockSourceUsbMidi,
    };

    struct Stats {
        uint32_t uptime;
        uint32_t midiRxOverflow;
        uint32_t usbMidiRxOverflow;
    };

    Engine(Model &model, ClockTimer &clockTimer, Adc &adc, Dac &dac, Dio &dio, GateOutput &gateOutput, Midi &midi, UsbMidi &usbMidi, UsbH &usbH);

    void init();
    void update();

    // Engine::update() timing probe (µs, wall clock). Temporary instrumentation
    // for the -Os timing check — worst-case main-loop processing vs tick budget.
    uint32_t engineUpdateLastUs() const { return _updateLastUs; }
    uint32_t engineUpdateMaxUs() const { return _updateMaxUs; }
    uint32_t engineUpdateMaxTicks() const { return _updateMaxTicks; }
    void resetEngineUpdateStats() { _updateLastUs = 0; _updateMaxUs = 0; _updateMaxTicks = 0; }

    // locking temporarily puts the engine in a state where completely skips all updates
    // lock should only be hold for very short amounts of time
    void lock();
    void unlock();
    bool isLocked() const { return _locked; }

    // suspending temporarily puts the engine in a state where it only processes basic events but skips all updates
    // suspending can be used during longer periods of time (e.g. file operations)
    void suspend();
    void resume();
    bool isSuspended() const { return _suspended; }

    // clock control
    void togglePlay(bool shift = false);
    void clockStart();
    void clockStop();
    void clockContinue();
    void clockReset();
    bool clockRunning() const;

    // recording
    void toggleRecording();
    void setRecording(bool recording);
    bool recording() const;

    // tempo
    float tempo() const { return _clock.bpm(); }
    void setTempo(float bpm);

    // tap tempo
    void tapTempoReset();
    void tapTempoTap();

    // nudge tempo
    void nudgeTempoSetDirection(int direction);
    float nudgeTempoStrength() const;

    // time base
    uint32_t tick() const { return _tick; }
    uint32_t noteDivisor() const;
    uint32_t measureDivisor() const;
    float measureFraction() const;
    uint32_t syncDivisor() const;
    float syncFraction() const;

    const CvInput &cvInput() const { return _cvInput; }
    const CvOutput &cvOutput() const { return _cvOutput; }
    const uint8_t gateOutput() const { return _gateOutput.gates(); }

    static constexpr int BusCvCount = 4;
    // Per-lane writer-domain bits, set live at write, reset each frame. The bus page
    // reads busWriters() to light R/C/T glyphs as routes/CV-router/Teletype drive a lane.
    static constexpr uint8_t BusWriterRouting = 1 << 0;
    static constexpr uint8_t BusWriterCvRouter = 1 << 1;
    static constexpr uint8_t BusWriterTeletype = 1 << 2;
    float busCv(int index) const {
        if (index < 0 || index >= BusCvCount) {
            return 0.f;
        }
        return clamp(_busCv[index], -5.f, 5.f);
    }
    uint8_t busWriters(int index) const {
        if (index < 0 || index >= BusCvCount) {
            return 0;
        }
        return _busCvWriters[index];
    }
    float cvRouteOutput(int lane) const;
    void setBusCv(int index, float volts, uint8_t writer = 0) {
        if (index < 0 || index >= BusCvCount) {
            return;
        }
        _busCvWriters[index] |= writer;
        // Sum+clamp law: per-frame writers (routing/CV-router/Teletype) accumulate
        // UNCLAMPED onto the lane (first seeds/drops last frame, rest add); busCv()
        // clamps once at read, so the value is the order-free sum-then-clamp, not an
        // order-biased running clamp. _busCvWritten resets each frame in both paths.
        if (!_busCvWritten[index]) {
            _busCvWritten[index] = true;
            _busCv[index] = volts;
        } else {
            _busCv[index] += volts;
        }
    }

    static constexpr int TvCount = 16;
    int16_t tvGet(int i) const { return (i >= 0 && i < TvCount) ? _tv[i] : 0; }
    void tvSet(int i, int16_t v) { if (i >= 0 && i < TvCount) _tv[i] = v; }

    // gate overrides
    bool gateOutputOverride() const { return _gateOutputOverride; }
    void setGateOutputOverride(bool enabled) { _gateOutputOverride = enabled; }
    void setGateOutput(uint8_t gates) { _gateOutputOverrideValue = gates; }

    // cv overrides
    bool cvOutputOverride() const { return _cvOutputOverride; }
    void setCvOutputOverride(bool enabled) { _cvOutputOverride = enabled; }
    void setCvOutput(int channel, float value) { _cvOutputOverrideValues[channel] = value; }

    void selectTrackPattern(int trackIndex, int patternIndex);

    const Clock &clock() const { return _clock; }
          Clock &clock()       { return _clock; }

    void updateBusSafetyMode();
    void applyBusSafety();

    const EngineState &state() const { return _state; }

    const TrackEngineArray &trackEngines() const { return _trackEngines; }
          TrackEngineArray &trackEngines()       { return _trackEngines; }

    const TrackEngine &trackEngine(int index) const { return *_trackEngines[index]; }
          TrackEngine &trackEngine(int index)       { return *_trackEngines[index]; }

    const TrackEngine &selectedTrackEngine() const { return *_trackEngines[_model.project().selectedTrackIndex()]; }
          TrackEngine &selectedTrackEngine()       { return *_trackEngines[_model.project().selectedTrackIndex()]; }

    const RoutingEngine &routingEngine() const { return _routingEngine; }
          RoutingEngine &routingEngine()       { return _routingEngine; }

    const ModulatorEngine &modulatorEngine() const { return _modulatorEngine; }
          ModulatorEngine &modulatorEngine()       { return _modulatorEngine; }

    const GeodeEngine &geodeEngine() const { return _geodeEngine; }
          GeodeEngine &geodeEngine()       { return _geodeEngine; }

    const MidiOutputEngine &midiOutputEngine() const { return _midiOutputEngine; }
          MidiOutputEngine &midiOutputEngine()       { return _midiOutputEngine; }

    const MidiLearn &midiLearn() const { return _midiLearn; }
          MidiLearn &midiLearn()       { return _midiLearn; }

    const Model &model() const { return _model; }
          Model &model()       { return _model; }

    bool trackEnginesConsistent() const;
    bool trackPatternsConsistent() const;

    bool sendMidi(MidiPort port, uint8_t cable, const MidiMessage &message);
    void setMidiReceiveHandler(MidiReceiveHandler handler) { _midiReceiveHandler = handler; }
    void setUsbMidiConnectHandler(UsbMidiConnectHandler handler) { _usbMidiConnectHandler = handler; }
    void setUsbMidiDisconnectHandler(UsbMidiDisconnectHandler handler) { _usbMidiDisconnectHandler = handler; }

    void setKeyboardReceiveHandler(KeyboardReceiveHandler handler) { _keyboardReceiveHandler = handler; }
    void setHidConnectHandler(HidConnectHandler handler) { _hidConnectHandler = handler; }
    void setHidDisconnectHandler(HidDisconnectHandler handler) { _hidDisconnectHandler = handler; }
    bool midiProgramChangesEnabled();
    void sendMidiProgramChange(int programNumber);
    void sendMidiProgramSave(int programNumber);

    // message handling
    void showMessage(const char *text, uint32_t duration = 1000);
    void setMessageHandler(MessageHandler handler);

    Stats stats() const;

private:
    // Clock::Listener
    virtual void onClockOutput(const Clock::OutputState &state) override;
    virtual void onClockMidi(uint8_t data) override;

    void updateTrackSetups();
    void updateTrackOutputs();
    void updateCvRouteOutputs();
    void reset();
    void updatePlayState(bool ticked);

    float applyModulatorOffset(int channel, float cvValue) const;
    void updateOverrides();

    void usbMidiConnect(uint16_t vendorId, uint16_t productId);
    void usbMidiDisconnect();
    void hidConnect(uint8_t device_id, int type);
    void hidDisconnect(uint8_t device_id);

    void receiveMidi();
    void receiveMidi(MidiPort port, uint8_t cable, const MidiMessage &message);
    void receiveKeyboard();
    void monitorMidi(const MidiMessage &message);

    void initClock();
    void updateClockSetup();

    Model &_model;
    Project &_project;
    Dio &_dio;
    GateOutput &_gateOutput;
    Midi &_midi;
    UsbMidi &_usbMidi;
    UsbH &_usbH;

    EngineState _state;

    CvInput _cvInput;
    CvOutput _cvOutput;

    Clock _clock;
    TapTempo _tapTempo;
    NudgeTempo _nudgeTempo;

    TrackEngineContainerArray _trackEngineContainers;
    TrackEngineArray _trackEngines;
    UpdateReducer<os::time::ms(25)> _updateReducer; // rate-limit cap on the per-tick global recompute

    MidiOutputEngine _midiOutputEngine;

    RoutingEngine _routingEngine;
    ModulatorEngine _modulatorEngine;
    GeodeEngine _geodeEngine;   // modulator-driven Geode (Just Friends) voices, M3-M8
    bool _modulatorGateState[CONFIG_MODULATOR_COUNT] = {}; // hysteresis latch per modulator gate
    MidiLearn _midiLearn;
    MidiReceiveHandler _midiReceiveHandler;
    UsbMidiConnectHandler _usbMidiConnectHandler;
    UsbMidiDisconnectHandler _usbMidiDisconnectHandler;
    KeyboardReceiveHandler _keyboardReceiveHandler;
    HidConnectHandler _hidConnectHandler;
    HidDisconnectHandler _hidDisconnectHandler;

    CvGateToMidiConverter _cvGateToMidiConverter;

    // locking
    volatile uint32_t _requestLock = 0;
    volatile uint32_t _locked = 0;

    // suspending
    volatile uint32_t _requestSuspend = 0;
    volatile uint32_t _suspended = 0;

    uint32_t _tick = 0;

    WallClock _wallClock;
    uint32_t _lastWallUs = 0;

    void updateImpl();
    uint32_t _updateLastUs = 0;
    uint32_t _updateMaxUs = 0;
    uint32_t _updateTicks = 0;
    uint32_t _updateMaxTicks = 0;

    // midi monitoring
    struct {
        Types::MidiInputMode lastMidiInputMode;
        MidiSourceConfig lastMidiInputSource;
        Types::CvGateInput lastCvGateInput;

        bool inputChanged(const Project &project) {
            bool changed =
                project.midiInputMode() != lastMidiInputMode ||
                project.midiInputSource() != lastMidiInputSource ||
                project.cvGateInput() != lastCvGateInput;
            if (changed) {
                lastMidiInputMode = project.midiInputMode();
                lastMidiInputSource = project.midiInputSource();
                lastCvGateInput = project.cvGateInput();
            }
            return changed;
        }

        int8_t lastNote = -1;
        int8_t lastTrack = -1;
    } _midiMonitoring;

    // TODO Could be a setting if needed
    static const bool _preSendMidiPgmChange = true;
    bool _midiHasSentInitialPgmChange = false;
    int _midiLastInitialProgramOffset = -1;
    // State machine for when to pre-handle (midi) events
    // Allows us to handle pre-handle events even if they are submitted after the pre-handle tick
    // (then we process them immediately)
    enum PreHandle {
        PreHandleNone,
        PreHandlePending,
        PreHandleComplete,
    };
    PreHandle _pendingPreHandle = PreHandleNone;

    // gate output overrides
    bool _gateOutputOverride = false;
    uint8_t _gateOutputOverrideValue = 0;

    // cv output overrides
    bool _cvOutputOverride = false;
    std::array<float, CvOutput::Channels> _cvOutputOverrideValues;
    std::array<float, BusCvCount> _busCv{};
    std::array<bool, BusCvCount> _busCvWritten{};
    std::array<uint8_t, BusCvCount> _busCvWriters{};
    int16_t _tv[TvCount] = {};
    bool _busCvSafeMode = true;
    static constexpr float BusCvDecay = 0.9995f;
    std::array<float, CvRoute::LaneCount> _cvRouteOutputs{};

    MessageHandler _messageHandler;
};
