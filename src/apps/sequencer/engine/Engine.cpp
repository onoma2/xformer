#include "Engine.h"

#include "Config.h"
#include "MidiUtils.h"
#include "GateRotation.h"
#include "Tt2OutputMix.h"

#include "core/Debug.h"
#include "core/midi/MidiMessage.h"

#include "os/os.h"

#include <cmath>

Engine::Engine(Model &model, ClockTimer &clockTimer, Adc &adc, Dac &dac, Dio &dio, GateOutput &gateOutput, Midi &midi, UsbMidi &usbMidi, UsbH &usbH) :
    _model(model),
    _project(model.project()),
    _dio(dio),
    _gateOutput(gateOutput),
    _midi(midi),
    _usbMidi(usbMidi),
    _usbH(usbH),
    _cvInput(adc),
    _cvOutput(dac, model.settings().calibration()),
    _clock(clockTimer),
    _midiOutputEngine(*this, model),
    _routingEngine(*this, model)
{
    _cvOutputOverrideValues.fill(0.f);
    _cvRouteOutputs.fill(0.f);
    _trackEngines.fill(nullptr);
    _busCv.fill(0.f);

    _usbMidi.setConnectHandler([this] (uint16_t vendorId, uint16_t productId) { usbMidiConnect(vendorId, productId); });
    _usbMidi.setDisconnectHandler([this] () { usbMidiDisconnect(); });

    _usbH.setHidCallbacks(
        [](uint8_t device_id, HID_TYPE type, void *ctx) {
            auto *engine = static_cast<Engine *>(ctx);
            engine->hidConnect(device_id, static_cast<int>(type));
        },
        [](uint8_t device_id, void *ctx) {
            auto *engine = static_cast<Engine *>(ctx);
            engine->hidDisconnect(device_id);
        },
        this
    );

    _midiMonitoring.inputChanged(_project);
}

void Engine::init() {
    _cvInput.init();
    _cvOutput.init();
    _clock.init();

    initClock();
    updateClockSetup();

    // setup track engines
    updateTrackSetups();
    reset();

    _lastWallUs = _wallClock.now();
}

void Engine::update() {
    _updateTicks = 0;
    uint32_t start = _wallClock.now();
    updateImpl();
    uint32_t elapsed = _wallClock.now() - start;
    _updateLastUs = elapsed;
    if (elapsed > _updateMaxUs) {
        _updateMaxUs = elapsed;
        _updateMaxTicks = _updateTicks;
    }
}

void Engine::updateImpl() {
    // locking
    _locked = _requestLock;
    if (_locked) {
        return;
    }

    uint32_t nowUs = _wallClock.now();
    float dt = (nowUs - _lastWallUs) * 1e-6f; // seconds of real wall time (wrap-safe delta)
    _lastWallUs = nowUs;

    // suspending
    if (_requestSuspend != _suspended) {
        if (_requestSuspend) {
            _clock.masterStop();
        }
        _suspended = _requestSuspend;
    }

    if (_suspended) {
        // consume ticks
        uint32_t tick;
        while (_clock.checkTick(&tick)) {}

        // consume midi events
        uint8_t cable;
        MidiMessage message;
        while (_midi.recv(&message)) {}
        while (_usbMidi.recv(&cable, &message)) {}

        _cvInput.update();
        _busCvWritten.fill(false);
        _busCvWriters.fill(0);   // re-seed bus sum each frame (CV-router writes below)
        _busCvRouting.fill(0.f); // suspended: no routing compose, so drop its slot
        updateOverrides();       // clears + refills the CV-router slot
        _cvOutput.update();
        _gateOutput.update();
        return;
    }

    // rebuild engine containers for any tracks whose mode changed
    // (must precede clock-event processing — Reset/Start call Engine::reset()
    //  which iterates _trackEngines[] and would deref stale pointers if the
    //  model union was swapped by Project::clear() while engine was suspended)
    updateTrackSetups();

    updateBusSafetyMode();
    _busCvWritten.fill(false);
    _busCvWriters.fill(0);
    // Clear the CV-router slot at frame start too: the first routing compose
    // (below) can read busCv() for a BusCv source before updateOverrides()
    // refills it, so leaving last frame's value here is a stale cross-frame read.
    _busCvCvRouter.fill(0.f);

    // process clock events
    while (Clock::Event event = _clock.checkEvent()) {
        switch (event) {
        case Clock::Start:
            // DBG("START");
            reset();
            _state.setRunning(true);
            break;
        case Clock::Stop:
            // DBG("STOP");
            for (auto trackEngine : _trackEngines) {
                trackEngine->stop();   // drop pending gates so none stick HIGH
            }
            _state.setRunning(false);
            break;
        case Clock::Continue:
            // DBG("CONTINUE");
            _state.setRunning(true);
            break;
        case Clock::Reset:
            // DBG("RESET");
            reset();
            _state.setRunning(false);
            break;
        }
    }

    // update tempo
    _nudgeTempo.update(dt);
    _clock.setMasterBpm(_project.tempo() * (1.f + _nudgeTempo.strength() * 0.1f));

    // update clock setup
    updateClockSetup();

    // update play state
    updatePlayState(false);

    // update cv inputs
    _cvInput.update();

    // receive midi events
    receiveMidi();

    // receive keyboard events
    receiveKeyboard();

    // update routings
    _busCvRouting.fill(0.f);   // re-seed routing's bus contribution each compose
    _routingEngine.update();

    uint32_t tick;
    while (_clock.checkTick(&tick)) {
        _tick = tick;
        ++_updateTicks;

        // update play state
        updatePlayState(true);

        // tick track engines; collect whether any track updated its CV output
        bool cvUpdated = false;
        for (size_t trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            auto &track = _model.project().track(trackIndex);
            auto &trackEngine = *_trackEngines[trackIndex];

            TrackEngine::TickResult result = TrackEngine::TickResult::NoUpdate;
            if (track.runGate()) {
                result = trackEngine.tick(tick);
            }
            if (result & TrackEngine::TickResult::CvUpdate) {
                cvUpdated = true;
            }
        }

        // update midi outputs, force sending CC on first tick
        if (tick == 0) {
            _midiOutputEngine.update(true);
        }

        // tick modulators before the global recompute so a modulator used as a
        // routing/CV source reflects the current tick
        const auto &geode = _project.geode();
        const bool geodeActive = geode.active();
        if (geodeActive) {
            // push per-voice tune ratios into the engine (only on change, keeps the cache warm)
            for (int v = 0; v < GeodeEngine::VoiceCount; ++v) {
                if (_geodeEngine.voiceTuneNumerator(v) != geode.tuneNumerator(v) ||
                    _geodeEngine.voiceTuneDenominator(v) != geode.tuneDenominator(v)) {
                    _geodeEngine.setVoiceTune(v, geode.tuneNumerator(v), geode.tuneDenominator(v));
                }
            }
        }
        const bool justf = !geodeActive && _modulatorEngine.justfActive();
        const float justfMasterHz = justf ? _project.modulator(0).rateHz() : 0.f;
        const float justfIntone = _modulatorEngine.intone();
        for (int modulatorIndex = 0; modulatorIndex < CONFIG_MODULATOR_COUNT; ++modulatorIndex) {
            const auto &modulator = _project.modulator(modulatorIndex);
            Routing::Source gs = modulator.gateSource();
            // self-gating is protected: a modulator never gates from its own output
            float level = (Routing::isModulatorSource(gs) &&
                           Routing::modulatorSourceIndex(gs) == modulatorIndex)
                ? 0.f : _routingEngine.resolveSourceLevel(gs);
            bool prevGate = _modulatorGateState[modulatorIndex];
            // manual audition pulse (F1 re-press) ORs into the gate, then decays
            bool gate = ModulatorEngine::gateFromLevel(level, prevGate) ||
                        _modulatorEngine.manualGateHigh(modulatorIndex);
            _modulatorEngine.decayManualGate(modulatorIndex, dt);
            _modulatorGateState[modulatorIndex] = gate;

            // Geode: M3-M8 are GeodeEngine voices (output set after the update below). A gate
            // rising edge fires that voice's burst. M1 (clock) and M2 (run) tick as normal
            // modulators so their phase/output can be tapped.
            int voice = modulatorIndex - 2;
            if (geodeActive && voice >= 0 && voice < GeodeEngine::VoiceCount) {
                if (gate && !prevGate) {
                    // Mirror the Teletype JF.VOX sequence: set up the burst AND fire the
                    // first envelope now (update() alone won't, since the gate resets phase).
                    float run = (_modulatorEngine.currentValue(1) - 64) / 64.f;
                    _geodeEngine.triggerVoice(voice, geode.divs(voice), geode.repeats(voice));
                    _geodeEngine.setVoicePhase(voice, 0.f);
                    _geodeEngine.markVoiceTriggered(voice);
                    _geodeEngine.triggerImmediate(voice, geode.timeNorm(),
                        (geode.intone() - 8192) / 8192.f, run, uint8_t(geode.mode()));
                }
                continue;
            }

            // JustF: Free-domain modulators run at M1 x INTONE-ratio (B-clamped); others as-is.
            float rateOverride = -1.f;
            const Modulator *envBase = nullptr;
            if (justf && modulator.rateDomain() == Modulator::RateDomain::Free) {
                rateOverride = ModulatorEngine::justfEffectiveHz(justfMasterHz, justfIntone, modulatorIndex + 1);
                envBase = &_project.modulator(0);   // ADSR inherits M1's envelope, spread by index
            }
            _modulatorEngine.tick(tick, dt, modulator, modulatorIndex, gate, rateOverride, envBase);
            _midiOutputEngine.sendModulator(modulatorIndex, _modulatorEngine.currentValue(modulatorIndex));
        }

        // Geode driver: M1's phase clocks the engine, M2's output is RUN, globals come from
        // GeodeConfig. Voice levels become M3-M8's output (through the floor/invert transform).
        if (geodeActive) {
            float mf = _modulatorEngine.currentPhase(0) / 65536.f;       // M1 = clock
            float run = (_modulatorEngine.currentValue(1) - 64) / 64.f;  // M2 = run, bipolar
            float time = geode.timeNorm();
            float intone = (geode.intone() - 8192) / 8192.f;
            float ramp = geode.ramp() / 16383.f;
            float curve = (geode.curve() - 8192) / 8192.f;
            _geodeEngine.update(dt, mf, time, intone, ramp, curve, run, uint8_t(geode.mode()));
            for (int v = 0; v < GeodeEngine::VoiceCount; ++v) {
                int mi = v + 2;
                _modulatorEngine.setVoiceOutput(mi, _geodeEngine.voiceLevel(v), _project.modulator(mi));
                _midiOutputEngine.sendModulator(mi, _modulatorEngine.currentValue(mi));
            }
        }

        // single per-tick global recompute (was run once per firing track, T×
        // redundant). The reducer remains a rate-limit safety cap.
        // Compose physical outputs first so routing sources that read CvOut/GateOut
        // see the just-ticked track values; then route; then fill CV-route outputs
        // and recompose so CV-route output channels are current-tick (not one
        // update stale). The double compose breaks the routing<->CV-route cycle.
        if (cvUpdated && _updateReducer.update()) {
            for (auto trackEngine : _trackEngines) {
                trackEngine->update(0.f);
            }
            updateTrackOutputs();
            _busCvRouting.fill(0.f);
            _routingEngine.update();
            updateOverrides();
            updateTrackOutputs();
        }
    }

    for (auto trackEngine : _trackEngines) {
        trackEngine->update(dt);
    }

    _midiOutputEngine.update();

    // final compose: overrides fill the CV-route outputs before track outputs read them
    updateOverrides();
    updateTrackOutputs();
    applyBusSafety();

    // update cv/gate outputs
    _cvOutput.update();
    _gateOutput.update();
}

void Engine::updateBusSafetyMode() {
    _busCvSafeMode = _model.project().busSafety();
}

void Engine::applyBusSafety() {
    if (!_busCvSafeMode) {
        return;
    }
    for (int i = 0; i < BusCvCount; ++i) {
        if (!_busCvWritten[i]) {
            _busCv[i] *= BusCvDecay;
        }
    }
}

void Engine::lock() {
    while (!isLocked()) {
        _requestLock = 1;
#ifdef PLATFORM_SIM
        update();
#endif
    }
}

void Engine::unlock() {
    while (isLocked()) {
        _requestLock = 0;
#ifdef PLATFORM_SIM
        update();
#endif
    }
}

void Engine::suspend() {
    // TODO make re-entrant
    while (!isSuspended()) {
        _requestSuspend = 1;
#ifdef PLATFORM_SIM
        update();
#endif
    }
}

void Engine::resume() {
    while (isSuspended()) {
        _requestSuspend = 0;
#ifdef PLATFORM_SIM
        update();
#endif
    }
}

void Engine::togglePlay(bool shift) {
    if (shift) {
        switch (_project.clockSetup().shiftMode()) {
        case ClockSetup::ShiftMode::Restart:
            // restart
            clockStart();
            break;
        case ClockSetup::ShiftMode::Pause:
            // stop/continue
            if (clockRunning()) {
                clockStop();
            } else {
                clockContinue();
            }
            break;
        case ClockSetup::ShiftMode::Last:
            break;
        }
    } else {
        // start/stop
        if (clockRunning()) {
            clockReset();
        } else {
            clockStart();
        }
    }
}

void Engine::clockStart() {
    _clock.masterStart();
}

void Engine::clockStop() {
    _clock.masterStop();
}

void Engine::clockContinue() {
    _clock.masterContinue();
}

void Engine::clockReset() {
    _clock.masterReset();
}

bool Engine::clockRunning() const {
    return _state.running();
}

void Engine::toggleRecording() {
    _state.setRecording(!_state.recording());
}

void Engine::setRecording(bool recording) {
    _state.setRecording(recording);
}

bool Engine::recording() const {
    return _state.recording();
}

void Engine::tapTempoReset() {
    _tapTempo.reset();
}

void Engine::tapTempoTap() {
    float bpm = _project.tempoBase();   // tap the anchor, not the modulated value
    bpm = _tapTempo.tap(bpm);
    _project.setTempo(bpm);
}

void Engine::nudgeTempoSetDirection(int direction) {
    _nudgeTempo.setDirection(direction);
}

float Engine::nudgeTempoStrength() const {
    return _nudgeTempo.strength();
}

void Engine::setTempo(float bpm) {
    _model.project().setTempo(bpm);
}

void Engine::selectTrackPattern(int trackIndex, int patternIndex) {
    _model.project().playState().selectTrackPattern(trackIndex, patternIndex, PlayState::ExecuteType::Immediate);
}


uint32_t Engine::noteDivisor() const {
    return _project.timeSignature().noteDivisor();
}

uint32_t Engine::measureDivisor() const {
    return _project.timeSignature().measureDivisor();
}

float Engine::measureFraction() const {
    uint32_t divisor = measureDivisor();
    return float(_tick % divisor) / divisor;
}

uint32_t Engine::syncDivisor() const {
    return _project.syncMeasure() * measureDivisor();
}

float Engine::syncFraction() const {
    uint32_t divisor = syncDivisor();
    return float(_tick % divisor) / divisor;
}

bool Engine::trackEnginesConsistent() const {
    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
        if (trackEngine(trackIndex).trackMode() != _project.track(trackIndex).trackMode()) {
            return false;
        }
    }
    return true;
}

bool Engine::trackPatternsConsistent() const {
    auto playState = _project.playState();
    auto firstTrackState = playState.trackState(0);

    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
        auto trackState = playState.trackState(trackIndex);

        if (trackState.pattern() != firstTrackState.pattern()
            || trackState.requestedPattern() != firstTrackState.requestedPattern()) {
            return false;
        }
    }
    return true;
}

bool Engine::sendMidi(MidiPort port, uint8_t cable, const MidiMessage &message) {
    switch (port) {
    case MidiPort::Midi:
        return _midi.send(message);
    case MidiPort::UsbMidi:
        return _usbMidi.send(cable, message);
    case MidiPort::CvGate:
        // input only
        break;
    }
    return false;
}

bool Engine::midiProgramChangesEnabled() {
    return _project.midiIntegrationProgramChangesEnabled()
        && trackPatternsConsistent()
        && !_project.playState().snapshotActive();
}

void Engine::sendMidiProgramChange(int programNumber) {
    if (_project.midiIntegrationMalekkoEnabled()) {
        _midiOutputEngine.sendMalekkoSelectHandshake(0);
    }
    if (_project.midiIntegrationProgramChangesEnabled()) {
        _midiOutputEngine.sendProgramChange(0, _project.midiProgramOffset() + programNumber);
    }
    if (_project.midiIntegrationMalekkoEnabled()) {
        _midiOutputEngine.sendMalekkoSelectReleaseHandshake(0);
    }
}

void Engine::sendMidiProgramSave(int programNumber) {
    if (_project.midiIntegrationMalekkoEnabled()) {
        _midiOutputEngine.sendMalekkoSaveHandshake(0);
    }
    if (_project.midiIntegrationProgramChangesEnabled()) {
        _midiOutputEngine.sendProgramChange(0, _project.midiProgramOffset() + programNumber);
    }
}

void Engine::showMessage(const char *text, uint32_t duration) {
    if (_messageHandler) {
        _messageHandler(text, duration);
    }
}

void Engine::setMessageHandler(MessageHandler handler) {
    _messageHandler = handler;
}

Engine::Stats Engine::stats() const {
    return {
        .uptime = os::ticks() / os::time::ms(1000),
        .midiRxOverflow = _midi.rxOverflow(),
        .usbMidiRxOverflow = _usbMidi.rxOverflow()
    };
}

void Engine::onClockOutput(const Clock::OutputState &state) {
    _dio.clockOutput.set(state.clock);
    switch (_project.clockSetup().clockOutputMode()) {
    case ClockSetup::ClockOutputMode::Reset:
        _dio.resetOutput.set(state.reset);
        break;
    case ClockSetup::ClockOutputMode::Run:
        _dio.resetOutput.set(state.run);
        break;
    case ClockSetup::ClockOutputMode::Last:
        break;
    }
}

void Engine::onClockMidi(uint8_t data) {
    // TODO we should send a single byte with priority
    const auto &clockSetup = _project.clockSetup();
    if (clockSetup.midiTx()) {
        _midi.send(MidiMessage(data));
    }
    if (clockSetup.usbTx()) {
        // always send clock on cable 0
        _usbMidi.send(0, MidiMessage(data));
    }
}

void Engine::updateTrackSetups() {
    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
        auto &track = _project.track(trackIndex);

        if (!_trackEngines[trackIndex] || _trackEngines[trackIndex]->trackMode() != track.trackMode()) {
            auto &trackEngine = _trackEngines[trackIndex];
            auto &trackContainer = _trackEngineContainers[trackIndex];

            switch (track.trackMode()) {
            case Track::TrackMode::Note:
                trackEngine = trackContainer.create<NoteTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::Curve:
                trackEngine = trackContainer.create<CurveTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::MidiCv:
                trackEngine = trackContainer.create<MidiCvTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::Tuesday:
                trackEngine = trackContainer.create<TuesdayTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::DiscreteMap:
                trackEngine = trackContainer.create<DiscreteMapTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::Indexed:
                trackEngine = trackContainer.create<IndexedTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::Stochastic:
                trackEngine = trackContainer.create<StochasticTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::Fractal:
                trackEngine = trackContainer.create<FractalTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::PhaseFlux:
                trackEngine = trackContainer.create<PhaseFluxTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::TeletypeV2:
                trackEngine = trackContainer.create<TT2TrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::TeletypeMini:
                trackEngine = trackContainer.create<TT2MiniTrackEngine>(*this, _model, track);
                break;
            case Track::TrackMode::Last:
                break;
            }
        }
    }
}

void Engine::updateTrackOutputs() {
    const auto &gateOutputTracks = _project.gateOutputTracks();
    const auto &cvOutputTracks = _project.cvOutputTracks();

    // TT2 tracks emit to jacks only via the auto-index layer below; they are
    // excluded from the legacy Layout source path and the rotation pools.
    bool isTt2[CONFIG_TRACK_COUNT];
    bool anyTt2 = false;
    for (int t = 0; t < CONFIG_TRACK_COUNT; ++t) {
        auto mode = _project.track(t).trackMode();
        isTt2[t] = (mode == Track::TrackMode::TeletypeV2 || mode == Track::TrackMode::TeletypeMini);
        anyTt2 = anyTt2 || isTt2[t];
    }

    int trackGateIndex[CONFIG_TRACK_COUNT];
    int trackCvIndex[CONFIG_TRACK_COUNT];
    int cvOutputTrackIndex[CONFIG_CHANNEL_COUNT];
    int cvOutputTrackSlot[CONFIG_CHANNEL_COUNT];
    int cvOutputCvRouteLane[CONFIG_CHANNEL_COUNT];

    for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
        trackGateIndex[trackIndex] = 0;
        trackCvIndex[trackIndex] = 0;
    }
    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        cvOutputTrackIndex[i] = -1;
        cvOutputTrackSlot[i] = -1;
        cvOutputCvRouteLane[i] = -1;
    }

    // --- Gate Rotation Logic ---
    int gatePool[CONFIG_CHANNEL_COUNT];
    int gatePoolSize = 0;

    // Identify Gate Pool: jacks whose track is in the rotation group (spec 018 — the
    // GateOutputRotate route's mask), not the legacy per-track isGateOutputRotated().
    uint8_t gateRotateMask = _routingEngine.gateRotateMask();
    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        int trackIndex = gateOutputTracks[i];
        if (trackIndex < CONFIG_TRACK_COUNT &&
            !isTt2[trackIndex] &&
            (gateRotateMask & (1 << trackIndex))) {
            gatePool[gatePoolSize++] = i;
        }
    }

    // --- CV Rotation Logic ---
    int cvPool[CONFIG_CHANNEL_COUNT];
    int cvPoolSize = 0;

    // Identify CV Pool: jacks whose track is in the CV rotation group (spec 019 — the
    // CvOutputRotate route's mask).
    uint8_t cvRotateMask = _routingEngine.cvRotateMask();
    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        int trackIndex = cvOutputTracks[i];
        if (trackIndex < CONFIG_TRACK_COUNT &&
            !isTt2[trackIndex] &&
            (cvRotateMask & (1 << trackIndex))) {
            cvPool[cvPoolSize++] = i;
        }
    }

    // Precompute CV output track slots and CV route lanes
    int cvRouteLane = 0;
    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        int trackIndex = cvOutputTracks[i];
        if (trackIndex < CONFIG_TRACK_COUNT) {
            cvOutputTrackIndex[i] = trackIndex;
            cvOutputTrackSlot[i] = trackCvIndex[trackIndex]++;
        } else if (trackIndex == CONFIG_TRACK_COUNT) {
            cvOutputCvRouteLane[i] = cvRouteLane++;
        }
    }

    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        // Gate Output
        int gateSourceOutputIndex = i;
        // Check if this output is in the pool
        int gatePoolIndex = -1;
        for (int k = 0; k < gatePoolSize; ++k) { if (gatePool[k] == i) { gatePoolIndex = k; break; } }

        if (gatePoolIndex != -1) {
            // One group amount rotates the whole pool (legacy (p - N) mod K cycle).
            int rotatedIndex = rotatedGroupMember(gatePoolIndex, _routingEngine.gateRotateAmount(), gatePoolSize);
            gateSourceOutputIndex = gatePool[rotatedIndex];
        }

        int gateOutputTrack = gateOutputTracks[gateSourceOutputIndex];
        if (!_gateOutputOverride && gateOutputTrack < CONFIG_TRACK_COUNT) {
            // Legacy gate skips a TT2 source (resolves false); TT2 reaches the jack
            // only via the OR layer below, fixed to physical gate jack index.
            bool g = isTt2[gateOutputTrack] ? false
                : _trackEngines[gateOutputTrack]->gateOutput(trackGateIndex[gateOutputTrack]++);
            bool gateAtJack[CONFIG_TRACK_COUNT];
            for (int t = 0; t < CONFIG_TRACK_COUNT; ++t)
                gateAtJack[t] = isTt2[t] ? _trackEngines[t]->gateOutput(i) : false;
            g = g || Tt2OutputMix::anyGate(gateAtJack, isTt2, CONFIG_TRACK_COUNT);
            _gateOutput.setGate(i, g);
        }

        // CV Output
        int cvSourceOutputIndex = i;
        // Check if this output is in the pool
        int cvPoolIndex = -1;
        for (int k = 0; k < cvPoolSize; ++k) { if (cvPool[k] == i) { cvPoolIndex = k; break; } }

        if (cvPoolIndex != -1) {
            // One group amount rotates the whole pool (discrete, mirror of gate; spec 019).
            int rotatedIndex = rotatedGroupMember(cvPoolIndex, _routingEngine.cvRotateAmount(), cvPoolSize);
            cvSourceOutputIndex = cvPool[rotatedIndex];
        }

        int cvOutputTrack = cvOutputTracks[cvSourceOutputIndex];
        if (_cvOutputOverride) {
            continue;
        }
        // Legacy base (modulator-offset applied): a TT2-assigned jack resolves to 0
        // here, then receives its TT2 value through the auto-index layer below.
        float cvBase = 0.f;
        bool writeJack = anyTt2;
        if (cvOutputTrack < CONFIG_TRACK_COUNT) {
            if (!isTt2[cvOutputTrack]) {
                int cvSlot = cvOutputTrackSlot[cvSourceOutputIndex];
                cvBase = _trackEngines[cvOutputTrack]->cvOutput(cvSlot);
            }
            writeJack = true;
        } else if (cvOutputTrack == CONFIG_TRACK_COUNT) {
            int lane = cvOutputCvRouteLane[cvSourceOutputIndex];
            if (lane >= 0 && lane < int(_cvRouteOutputs.size())) {
                cvBase = _cvRouteOutputs[lane];
                writeJack = true;
            }
        }
        if (writeJack) {
            float v = applyModulatorOffset(i, cvBase);
            float cvAtJack[CONFIG_TRACK_COUNT];
            for (int t = 0; t < CONFIG_TRACK_COUNT; ++t)
                cvAtJack[t] = isTt2[t] ? _trackEngines[t]->cvOutput(i) : 0.f;
            v = clamp(v + Tt2OutputMix::sumCv(cvAtJack, isTt2, CONFIG_TRACK_COUNT), -5.f, 5.f);
            _cvOutput.setChannel(i, v);
        }
    }
}

float Engine::applyModulatorOffset(int channel, float cvValue) const {
    int modulatorIndex = _project.cvOutputModulator(channel);
    if (modulatorIndex > 0 && modulatorIndex <= CONFIG_MODULATOR_COUNT) {
        int modValue = _modulatorEngine.currentValue(modulatorIndex - 1);
        float modOffset = (modValue - 64) / 12.8f;  // 0..127 → ±5.0V
        cvValue += modOffset;
        cvValue = clamp(cvValue, -5.f, 5.f);
    }
    return cvValue;
}

void Engine::updateCvRouteOutputs() {
    const auto &cvRoute = _project.cvRoute();
    float inputs[CvRoute::LaneCount];

    for (int lane = 0; lane < CvRoute::LaneCount; ++lane) {
        switch (cvRoute.inputSource(lane)) {
        case CvRoute::InputSource::CvIn:
            inputs[lane] = _cvInput.channel(lane);
            break;
        case CvRoute::InputSource::Bus:
            inputs[lane] = busCv(lane);
            break;
        case CvRoute::InputSource::Mod1:
            inputs[lane] = (_modulatorEngine.currentValue(0) / 127.f) * 10.f - 5.f;
            break;
        case CvRoute::InputSource::Mod2:
            inputs[lane] = (_modulatorEngine.currentValue(1) / 127.f) * 10.f - 5.f;
            break;
        case CvRoute::InputSource::Mod3:
            inputs[lane] = (_modulatorEngine.currentValue(2) / 127.f) * 10.f - 5.f;
            break;
        case CvRoute::InputSource::Mod4:
            inputs[lane] = (_modulatorEngine.currentValue(3) / 127.f) * 10.f - 5.f;
            break;
        case CvRoute::InputSource::Mod5:
            inputs[lane] = (_modulatorEngine.currentValue(4) / 127.f) * 10.f - 5.f;
            break;
        case CvRoute::InputSource::Mod6:
            inputs[lane] = (_modulatorEngine.currentValue(5) / 127.f) * 10.f - 5.f;
            break;
        case CvRoute::InputSource::Mod7:
            inputs[lane] = (_modulatorEngine.currentValue(6) / 127.f) * 10.f - 5.f;
            break;
        case CvRoute::InputSource::Mod8:
            inputs[lane] = (_modulatorEngine.currentValue(7) / 127.f) * 10.f - 5.f;
            break;
        case CvRoute::InputSource::Off:
            inputs[lane] = 0.f;
            break;
        case CvRoute::InputSource::Last:
            inputs[lane] = 0.f;
            break;
        }
    }

    float scanNorm = clamp(cvRoute.scan(), 0, 100) * 0.01f;
    float scanPos = scanNorm * float(CvRoute::LaneCount - 1);
    int scanLow = clamp(int(std::floor(scanPos)), 0, CvRoute::LaneCount - 1);
    int scanHigh = clamp(int(std::ceil(scanPos)), 0, CvRoute::LaneCount - 1);
    float scanT = scanPos - scanLow;
    float signal = inputs[scanLow] * (1.f - scanT) + inputs[scanHigh] * scanT;

    float routeNorm = clamp(cvRoute.route(), 0, 100) * 0.01f;
    float routePos = routeNorm * float(CvRoute::LaneCount - 1);
    int routeLow = clamp(int(std::floor(routePos)), 0, CvRoute::LaneCount - 1);
    int routeHigh = clamp(int(std::ceil(routePos)), 0, CvRoute::LaneCount - 1);
    float routeT = routePos - routeLow;

    float weights[CvRoute::LaneCount] = {};
    if (routeLow == routeHigh) {
        weights[routeLow] = 1.f;
    } else {
        weights[routeLow] = 1.f - routeT;
        weights[routeHigh] = routeT;
    }

    for (int lane = 0; lane < CvRoute::LaneCount; ++lane) {
        float out = signal * weights[lane];
        switch (cvRoute.outputDest(lane)) {
        case CvRoute::OutputDest::CvOut:
            _cvRouteOutputs[lane] = clamp(out, -5.f, 5.f);
            break;
        case CvRoute::OutputDest::Bus:
            setBusCv(lane, out, BusWriterCvRouter);
            _cvRouteOutputs[lane] = 0.f;
            break;
        case CvRoute::OutputDest::None:
        case CvRoute::OutputDest::Last:
            _cvRouteOutputs[lane] = 0.f;
            break;
        }
    }
}

float Engine::cvRouteOutput(int lane) const {
    if (lane < 0 || lane >= int(_cvRouteOutputs.size())) {
        return 0.f;
    }
    return _cvRouteOutputs[lane];
}

void Engine::reset() {
    for (auto trackEngine : _trackEngines) {
        trackEngine->reset();
    }

    _midiOutputEngine.reset();
    _modulatorEngine.reset();
    _geodeEngine.reset();
}

void Engine::updatePlayState(bool ticked) {
    auto &playState = _project.playState();
    auto &songState = playState.songState();
    const auto &song = _project.song();

    bool hasImmediateRequests = playState.hasImmediateRequests();
    bool hasSyncedRequests = playState.hasSyncedRequests();
    bool handleLatchedRequests = playState.executeLatchedRequests();
    bool hasRequests = hasImmediateRequests || hasSyncedRequests || handleLatchedRequests;

    bool handleSyncedRequests = _tick % syncDivisor() == 0;
    bool handleSongAdvance = ticked && _tick > 0 && _tick % measureDivisor() == 0;
    bool withinPreHandleRange = (_tick + 192) % syncDivisor() < 192;
    if (withinPreHandleRange && _pendingPreHandle == PreHandleNone) {
        _pendingPreHandle = PreHandlePending;
    } else if (!withinPreHandleRange && _pendingPreHandle != PreHandleNone) {
        _pendingPreHandle = PreHandleNone;
    }

    // send initial program change if we haven't sent it already
    // means that when the sequencer initially starts, it will sync connected devices to the same pattern
    // only works when all patterns are equal
    // we also send a program change if the midi program offset setting is updated
    if (midiProgramChangesEnabled() && (!_midiHasSentInitialPgmChange || _project.midiProgramOffset() != _midiLastInitialProgramOffset)) {
        sendMidiProgramChange(playState.trackState(0).pattern());
        _midiHasSentInitialPgmChange = true;
        _midiLastInitialProgramOffset = _project.midiProgramOffset();
    }

    // handle mute & pattern requests

    bool changedPatterns = false;

    if (hasRequests) {
        int muteRequests = PlayState::TrackState::ImmediateMuteRequest |
            (handleSyncedRequests ? PlayState::TrackState::SyncedMuteRequest : 0) |
            (handleLatchedRequests ? PlayState::TrackState::LatchedMuteRequest : 0);

        int patternRequests = PlayState::TrackState::ImmediatePatternRequest |
            (handleSyncedRequests ? PlayState::TrackState::SyncedPatternRequest : 0) |
            (handleLatchedRequests ? PlayState::TrackState::LatchedPatternRequest : 0);

        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            auto &trackState = playState.trackState(trackIndex);

            // handle mute requests
            if (trackState.hasRequests(muteRequests)) {
                trackState.setMute(trackState.requestedMute());
            }

            // handle pattern requests
            if (trackState.hasRequests(patternRequests)) {
                trackState.setPattern(trackState.requestedPattern());
                changedPatterns = true;
            }

            // clear requests
            trackState.clearRequests(muteRequests | patternRequests);
        }

        bool shouldSendPgmChange = !_preSendMidiPgmChange && changedPatterns;
        bool shouldPreSendPgmChange = _preSendMidiPgmChange && ((changedPatterns && !playState.hasSyncedRequests())
                                                                || (_pendingPreHandle == PreHandlePending && playState.hasSyncedRequests()));

        if (midiProgramChangesEnabled() && (shouldSendPgmChange || shouldPreSendPgmChange)) {
            sendMidiProgramChange(playState.trackState(0).requestedPattern());
            _pendingPreHandle = PreHandleComplete;
        }
    }

    // handle song requests

    auto activateSongSlot = [&] (const Song::Slot &slot) {
        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            playState.trackState(trackIndex).setPattern(slot.pattern(trackIndex));
            // only set mutes if track in song contains any mutes at all
            if (song.trackHasMutes(trackIndex)) {
                playState.trackState(trackIndex).setMute(slot.mute(trackIndex));
            }
        }
    };

    if (hasRequests) {
        int playRequests = PlayState::SongState::ImmediatePlayRequest |
            (handleSyncedRequests ? PlayState::SongState::SyncedPlayRequest : 0) |
            (handleLatchedRequests ? PlayState::SongState::LatchedPlayRequest : 0);

        int stopRequests = PlayState::SongState::ImmediateStopRequest |
            (handleSyncedRequests ? PlayState::SongState::SyncedStopRequest : 0) |
            (handleLatchedRequests ? PlayState::SongState::LatchedStopRequest : 0);

        if (songState.hasRequests(playRequests)) {
            int requestedSlot = songState.requestedSlot();
            if (requestedSlot >= 0 && requestedSlot < song.slotCount()) {
                activateSongSlot(song.slot(requestedSlot));
                songState.setCurrentSlot(requestedSlot);
                songState.setCurrentRepeat(0);
                songState.setPlaying(true);
                handleSongAdvance = false;

                // start clock if not running
                if (!clockRunning()) {
                    clockStart();
                }
            }
        }

        if (changedPatterns || songState.hasRequests(stopRequests)) {
            songState.setPlaying(false);
        }

        songState.clearRequests(playRequests | stopRequests);
    }

    // clear pending requests

    if (hasRequests) {
        playState.clearImmediateRequests();
        if (handleSyncedRequests) {
            playState.clearSyncedRequests();
        }
        if (handleLatchedRequests) {
            playState.clearLatchedRequests();
        }
    }

    // handle song slot change
    if (songState.playing()) {
        const auto &slot = song.slot(songState.currentSlot());
        int currentSlot = songState.currentSlot();
        int currentRepeat = songState.currentRepeat();

        // send program changes when advancing pattern in song mode
        if (ticked && ((_preSendMidiPgmChange && _pendingPreHandle == PreHandlePending) || (!_preSendMidiPgmChange && handleSongAdvance))) {
            if (currentRepeat + 1 >= slot.repeats()) {
                auto nextSlot = song.slot(currentSlot + 1 < song.slotCount() ? currentSlot + 1 : 0);
                bool nextSlotPatternsEqual = true;
                int firstPattern = nextSlot.pattern(0);

                for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                    if (nextSlot.pattern(trackIndex) != firstPattern) {
                        nextSlotPatternsEqual = false;
                        break;
                    }
                }

                if (midiProgramChangesEnabled() && nextSlotPatternsEqual) {
                    sendMidiProgramChange(firstPattern);
                }
            }

            // TODO There is a possible race condition with synced patterns here
            // If song mode is active, we pre-send the program change,
            // and a user syncs a pattern change after it was pre-sent
            // we will not send the program change for the synced pattern change
            _pendingPreHandle = PreHandleComplete;
        }

        if (handleSongAdvance) {
            if (currentRepeat + 1 < slot.repeats()) {
                // next repeat
                songState.setCurrentRepeat(currentRepeat + 1);
            } else {
                // next slot
                songState.setCurrentRepeat(0);
                if (currentSlot + 1 < song.slotCount()) {
                    songState.setCurrentSlot(currentSlot + 1);
                } else {
                    songState.setCurrentSlot(0);
                }

                // update patterns
                activateSongSlot(song.slot(songState.currentSlot()));
                for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                    _trackEngines[trackIndex]->restart();
                }
            }
        }
    }

    // abort song mode if slot becomes invalid

    if ((songState.playing() && songState.currentSlot() >= song.slotCount()) ||
        (songState.currentRepeat() >= song.slot(songState.currentSlot()).repeats())) {
        playState.stopSong();
    }

    if (hasRequests | handleSongAdvance) {
        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            _trackEngines[trackIndex]->changePattern();
        }
    }
}

void Engine::updateOverrides() {
    // Re-seed the CV-router's bus contribution each compose. Cleared here (not
    // in updateCvRouteOutputs) so it also drops to 0 when the CV-output override
    // skips the CV-router below — otherwise busCv() would sum a stale value.
    _busCvCvRouter.fill(0.f);
    // overrides
    if (_gateOutputOverride) {
        _gateOutput.setGates(_gateOutputOverrideValue);
    }
    if (_cvOutputOverride) {
        for (size_t i = 0; i < _cvOutputOverrideValues.size(); ++i) {
            _cvOutput.setChannel(i, _cvOutputOverrideValues[i]);
        }
    } else {
        updateCvRouteOutputs();
    }
}

void Engine::usbMidiConnect(uint16_t vendorId, uint16_t productId) {
    if (_usbMidiConnectHandler) {
        _usbMidiConnectHandler(vendorId, productId);
    }
}

void Engine::usbMidiDisconnect() {
    if (_usbMidiDisconnectHandler) {
        _usbMidiDisconnectHandler();
    }
}

void Engine::hidConnect(uint8_t device_id, int type) {
    if (_hidConnectHandler) {
        _hidConnectHandler(device_id, type);
    }
}

void Engine::hidDisconnect(uint8_t device_id) {
    if (_hidDisconnectHandler) {
        _hidDisconnectHandler(device_id);
    }
}

void Engine::receiveKeyboard() {
    HidKeyEvent keyEvent;
    while (_usbH.recvKey(&keyEvent)) {
        if (_keyboardReceiveHandler) {
            _keyboardReceiveHandler(keyEvent.keycode, keyEvent.modifiers, keyEvent.pressed);
        }
    }
}

void Engine::receiveMidi() {
    // reset MIDI monitoring if monitoring config has changed
    if (_midiMonitoring.inputChanged(_project)) {
        for (auto trackEngine : _trackEngines) {
            trackEngine->clearMidiMonitoring();
        }
    }

    // receive MIDI messages from ports
    MidiMessage message;
    while (_midi.recv(&message)) {
        message.fixFakeNoteOff();
        receiveMidi(MidiPort::Midi, 0, message);
    }
    uint8_t cable;
    while (_usbMidi.recv(&cable, &message)) {
        message.fixFakeNoteOff();
        receiveMidi(MidiPort::UsbMidi, cable, message);
    }

    // derive MIDI messages from CV/Gate input
    switch (_project.cvGateInput()) {
    case Types::CvGateInput::Off:
        _cvGateToMidiConverter.reset();
        break;
    case Types::CvGateInput::Cv1Cv2:
        _cvGateToMidiConverter.convert(_cvInput.channel(0), _cvInput.channel(1), 0, [this] (const MidiMessage &message) {
            receiveMidi(MidiPort::CvGate, 0, message);
        });
        break;
    case Types::CvGateInput::Cv3Cv4:
        _cvGateToMidiConverter.convert(_cvInput.channel(2), _cvInput.channel(3), 1, [this] (const MidiMessage &message) {
            receiveMidi(MidiPort::CvGate, 0, message);
        });
        break;
    case Types::CvGateInput::Last:
        break;
    }
}

void Engine::receiveMidi(MidiPort port, uint8_t cable, const MidiMessage &message) {
    // filter out real-time and system messages
    if (message.isRealTimeMessage() || message.isSystemMessage()) {
        return;
    }

    // let receive handler consume messages (controllers in UI task)
    if (_midiReceiveHandler) {
        if (_midiReceiveHandler(port, cable, message)) {
            return;
        }
    }

    // discard all messages not from cable 0
    if (cable != 0) {
        return;
    }

    // handle program changes
    if (message.isProgramChange() && _project.midiIntegrationProgramChangesEnabled()) {
        auto &playState = _project.playState();
        // if requested pattern > 16, we wrap around and start from the beginning
        // this allows other gear that may send fixed program changes based on the pattern
        // to still select some sequence on the performer on patterns > 16
        int requestedPattern = message.programNumber() % CONFIG_PATTERN_COUNT;

        for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
            playState.selectTrackPattern(trackIndex, requestedPattern, PlayState::ExecuteType::Immediate);
        }

        // Forward program changes to output
        _midiOutputEngine.sendProgramChange(0, requestedPattern);
    }

    // let midi learn inspect messages (except from virtual CV/Gate messages)
    if (port != MidiPort::CvGate) {
        _midiLearn.receiveMidi(port, message);
    }

    // let routing engine consume messages
    if (_routingEngine.receiveMidi(port, message)) {
        return;
    }

    // let track engines consume messages (only MIDI/CV tracks)
    // allow all tracks to receive messages even if one of them consumes it
    bool consumed = false;
    for (auto trackEngine : _trackEngines) {
        consumed |= trackEngine->receiveMidi(port, message);
    }
    if (consumed) {
        return;
    }

    // midi monitoring (and recording)
    if (port != MidiPort::CvGate) {
        if (_project.midiInputMode() == Types::MidiInputMode::Off) {
            return;
        }
        if (_project.midiInputMode() == Types::MidiInputMode::Source && !MidiUtils::matchSource(port, message, _project.midiInputSource())) {
            return;
        }
    }
    monitorMidi(message);
}

void Engine::monitorMidi(const MidiMessage &message) {
    // helper to send monitor message to a track engine
    auto sendMidi = [this] (int trackIndex, const MidiMessage &message) {
        _trackEngines[trackIndex]->monitorMidi(_tick, message);
    };

    auto currentTrack = _project.selectedTrackIndex();

    // detect when selected track has changed and a note is still active -> send note off
    if (int(_midiMonitoring.lastNote) != -1 && int(_midiMonitoring.lastTrack) != -1 && currentTrack != _midiMonitoring.lastTrack) {
        sendMidi(_midiMonitoring.lastTrack, MidiMessage::makeNoteOff(0, _midiMonitoring.lastNote));
    }

    if (message.isNoteOn()) {
        // detect if a different note is still active => send note off
        if (_midiMonitoring.lastNote != -1 && _midiMonitoring.lastNote != message.note()) {
            sendMidi(currentTrack, MidiMessage::makeNoteOff(0, _midiMonitoring.lastNote));
        }
        // send note on
        sendMidi(currentTrack, MidiMessage::makeNoteOn(0, message.note(), message.velocity()));
        _midiMonitoring.lastNote = message.note();
        _midiMonitoring.lastTrack = currentTrack;
    } else if (message.isNoteOff()) {
        // send note off
        sendMidi(currentTrack, MidiMessage::makeNoteOff(0, message.note()));
        _midiMonitoring.lastNote = -1;
        _midiMonitoring.lastTrack = currentTrack;
    } else {
        sendMidi(currentTrack, message);
    }
}

void Engine::initClock() {
    _clock.setListener(this);

    const auto &clockSetup = _project.clockSetup();

    // Forward external clock signals to clock
    _dio.clockInput.setHandler([&] (bool value) {
        // interrupt context

        // start clock on first clock pulse if reset is not hold and clock is not running
        if (clockSetup.clockInputMode() == ClockSetup::ClockInputMode::Reset && !_clock.isRunning() && !_dio.resetInput.get()) {
            _clock.slaveStart(ClockSourceExternal);
        }
        if (value) {
            _clock.slaveTick(ClockSourceExternal);
        }
    });

    // Handle reset or start/stop input
    _dio.resetInput.setHandler([&] (bool value) {
        // interrupt context
        switch (clockSetup.clockInputMode()) {
        case ClockSetup::ClockInputMode::Reset:
            if (value) {
                _clock.slaveReset(ClockSourceExternal);
            } else {
                _clock.slaveStart(ClockSourceExternal);
            }
            break;
        case ClockSetup::ClockInputMode::Run:
            if (value) {
                _clock.slaveContinue(ClockSourceExternal);
            } else {
                _clock.slaveStop(ClockSourceExternal);
            }
            break;
        case ClockSetup::ClockInputMode::StartStop:
            if (value) {
                _clock.slaveStart(ClockSourceExternal);
            } else {
                _clock.slaveStop(ClockSourceExternal);
                _clock.slaveReset(ClockSourceExternal);
            }
            break;
        case ClockSetup::ClockInputMode::Last:
            break;
        }
    });

    // Forward MIDI clock messages to clock
    _midi.setRecvFilter([this] (uint8_t data) {
        if (MidiMessage::isClockMessage(data)) {
            _clock.slaveHandleMidi(ClockSourceMidi, data);
            return true;
        }
        return false;
    });

    _usbMidi.setRecvFilter([this] (uint8_t data) {
        if (MidiMessage::isClockMessage(data)) {
            _clock.slaveHandleMidi(ClockSourceUsbMidi, data);
            return true;
        }
        return false;
    });
}

void Engine::updateClockSetup() {
    auto &clockSetup = _project.clockSetup();

    // Update clock swing
    _clock.outputConfigureSwing(clockSetup.clockOutputSwing() ? _project.swing() : 0);

    if (!clockSetup.isDirty()) {
        return;
    }

    // Configure clock mode
    switch (clockSetup.mode()) {
    case ClockSetup::Mode::Auto:
        _clock.setMode(Clock::Mode::Auto);
        break;
    case ClockSetup::Mode::Master:
        _clock.setMode(Clock::Mode::Master);
        break;
    case ClockSetup::Mode::Slave:
        _clock.setMode(Clock::Mode::Slave);
        break;
    case ClockSetup::Mode::Last:
        break;
    }

    // Configure clock slaves
    uint32_t ticksPerPulse = clockSetup.clockInputDivisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    if (clockSetup.clockInputMultiplier() > 0) {
        ticksPerPulse *= clockSetup.clockInputMultiplier();
    } else {
        ticksPerPulse /= -clockSetup.clockInputMultiplier();
    }

    _clock.slaveConfigure(ClockSourceExternal, ticksPerPulse, true);
    _clock.slaveConfigure(ClockSourceMidi, CONFIG_PPQN / 24, clockSetup.midiRx());
    _clock.slaveConfigure(ClockSourceUsbMidi, CONFIG_PPQN / 24, clockSetup.usbRx());

    // Update from clock input signal
    bool resetInput = _dio.resetInput.get();
    bool running = _clock.isRunning();

    switch (clockSetup.clockInputMode()) {
    case ClockSetup::ClockInputMode::Reset:
        if (resetInput && running) {
            _clock.slaveReset(ClockSourceExternal);
        }
        break;
    case ClockSetup::ClockInputMode::Run:
        if (resetInput && !running) {
            _clock.slaveContinue(ClockSourceExternal);
        } else if (!resetInput && running) {
            _clock.slaveStop(ClockSourceExternal);
        }
        break;
    case ClockSetup::ClockInputMode::StartStop:
        if (resetInput && !running) {
            _clock.slaveStart(ClockSourceExternal);
        } else if (!resetInput && running) {
            _clock.slaveReset(ClockSourceExternal);
        }
        break;
    case ClockSetup::ClockInputMode::Last:
        break;
    }

    // Configure clock outputs
    _clock.outputConfigure(clockSetup.clockOutputDivisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN), clockSetup.clockOutputPulse());

    // Update clock outputs
    onClockOutput(_clock.outputState());

    clockSetup.clearDirty();
}
