#include "TT2TrackEngine.h"

#include "Engine.h"
#include "CvInput.h"
#include "NoteTrackEngine.h"
#include "MidiUtils.h"

#include "model/NoteSequence.h"

#include <cmath>

// Active host for native ops (TT2Host.h). Set/cleared via ScopedHost around
// script execution; null outside it.
namespace { TT2Host *g_tt2ActiveHost = nullptr; }
TT2Host *tt2ActiveHost() { return g_tt2ActiveHost; }
void tt2SetActiveHost(TT2Host *host) { g_tt2ActiveHost = host; }

// Resolve a Note track's active sequence for cross-track W* ops.
static NoteSequence *tt2NoteSequence(Model &model, int trackIndex) {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) return nullptr;
    Track &track = model.project().track(trackIndex);
    if (track.trackMode() != Track::TrackMode::Note) return nullptr;
    int patternIndex = model.project().playState().trackState(trackIndex).pattern();
    if (patternIndex < 0 || patternIndex >= CONFIG_PATTERN_COUNT) return nullptr;
    return &track.noteTrack().sequence(patternIndex);
}

// Resolve one trigger input's configured source to a boolean gate level.
// Mirrors TeletypeTrackEngine::inputState: CvIn (threshold > 1V), GateOut
// (read back the engine's gate output), LogicalGate (another track's gate).
bool TT2TrackEngine::inputState(uint8_t index) const {
    if (index >= TT2_TRIGGER_INPUT_COUNT) {
        return false;
    }
    TT2TriggerSource source = _tt2Track.program().triggerSource[index];

    if (source >= TT2TriggerSource::CvIn1 && source <= TT2TriggerSource::CvIn4) {
        int ch = int(source) - int(TT2TriggerSource::CvIn1);
        if (ch < CvInput::Channels) {
            return _engine.cvInput().channel(ch) > 1.0f;  // gate threshold at 1V
        }
        return false;
    }

    if (source >= TT2TriggerSource::GateOut1 && source <= TT2TriggerSource::GateOut8) {
        int g = int(source) - int(TT2TriggerSource::GateOut1);
        return (_engine.gateOutput() >> g) & 0x1;
    }

    if (source >= TT2TriggerSource::LogicalGate1 && source <= TT2TriggerSource::LogicalGate8) {
        int t = int(source) - int(TT2TriggerSource::LogicalGate1);
        if (t >= 0 && t < CONFIG_TRACK_COUNT) {
            return _engine.trackEngine(t).gateOutput(0);
        }
        return false;
    }

    return false;  // None
}

// Sample the trigger inputs and fire the matching script on each rising edge.
// TI i -> script i (0-3); the metro (4) and init (5) scripts are reserved and
// never trigger-driven. A muted input (variables.mutes bit) still tracks its
// edge (no backlog on unmute) but does not fire. Latches the live level for STATE.
void TT2TrackEngine::updateInputTriggers() {
    auto &runtime = _tt2Track.runtime();
    bool now[TT2_TRIGGER_INPUT_COUNT];
    for (int i = 0; i < TT2_TRIGGER_INPUT_COUNT; ++i) {
        now[i] = inputState(uint8_t(i));
        runtime.inputLevel[i] = now[i] ? 1 : 0;
    }
    uint8_t fired = tt2RisingEdges(now, _prevInputState, TT2_TRIGGER_INPUT_COUNT);
    uint8_t mutes = uint8_t(runtime.variables.mutes);
    for (int i = 0; i < TT2_TRIGGER_INPUT_COUNT; ++i) {
        if ((fired & (1 << i)) && !((mutes >> i) & 1)) {
            runScript(uint8_t(i));
        }
    }
}

// Resolve a CV input source to Performer volts. Mirrors the legacy
// TeletypeTrackEngine readCvSource: physical CV in, CV out read-back, CV router
// lane, or another track's CV. None -> 0.
float TT2TrackEngine::cvSourceVolts(TT2CvInputSource source) const {
    if (source >= TT2CvInputSource::CvIn1 && source <= TT2CvInputSource::CvIn4) {
        int ch = int(source) - int(TT2CvInputSource::CvIn1);
        return ch < CvInput::Channels ? _engine.cvInput().channel(ch) : 0.f;
    }
    if (source >= TT2CvInputSource::CvOut1 && source <= TT2CvInputSource::CvOut8) {
        int ch = int(source) - int(TT2CvInputSource::CvOut1);
        return _engine.cvOutput().channel(ch);
    }
    if (source >= TT2CvInputSource::CvRoute1 && source <= TT2CvInputSource::CvRoute4) {
        int lane = int(source) - int(TT2CvInputSource::CvRoute1);
        return _engine.cvRouteOutput(lane);
    }
    if (source >= TT2CvInputSource::LogicalCv1 && source <= TT2CvInputSource::LogicalCv8) {
        int t = int(source) - int(TT2CvInputSource::LogicalCv1);
        if (t >= 0 && t < CONFIG_TRACK_COUNT) {
            return _engine.trackEngine(t).cvOutput(0);
        }
    }
    return 0.f;  // None / out of range
}

// Sample the six CV-mapped engine inputs each refresh. IN/PARAM are always
// written (0 if unmapped); X/Y/Z/T only when mapped, so a script can still set
// them by hand when their source is None.
void TT2TrackEngine::sampleInputs() {
    auto &program = _tt2Track.program();
    auto &v = _tt2Track.runtime().variables;
    v.in    = voltsToRaw(cvSourceVolts(program.cvInputSource[int(TT2CvInput::In)]));
    v.param = voltsToRaw(cvSourceVolts(program.cvInputSource[int(TT2CvInput::Param)]));
    int16_t *target[4] = { &v.x, &v.y, &v.z, &v.t };
    for (int k = 0; k < 4; ++k) {
        TT2CvInputSource src = program.cvInputSource[int(TT2CvInput::X) + k];
        if (src != TT2CvInputSource::None) {
            *target[k] = voltsToRaw(cvSourceVolts(src));
        }
    }
}

// --- UI live-exec / trigger (host-scoped + engine-locked) ---

TT2EvalResult TT2TrackEngine::runLiveCommand(const TT2Command &cmd) {
    _engine.lock();
    TT2EvalResult result;
    {
        ScopedHost host(this);
        result = evaluateCommand(cmd, _tt2Track.runtime(), _output, &_tt2Track.program());
    }
    _engine.unlock();
    return result;
}

void TT2TrackEngine::triggerScript(int scriptIndex) {
    if (scriptIndex < 0 || scriptIndex >= TT2_SCRIPT_COUNT) {
        return;
    }
    _engine.lock();
    {
        ScopedHost host(this);
        runScript(uint8_t(scriptIndex));
    }
    _engine.unlock();
}

void TT2TrackEngine::toggleScriptMute(int scriptIndex) {
    if (scriptIndex < 0 || scriptIndex >= TT2_TRIGGER_INPUTS) {
        return;
    }
    _engine.lock();
    _tt2Track.runtime().variables.mutes ^= uint8_t(1 << scriptIndex);
    _engine.unlock();
}

void TT2TrackEngine::toggleMetroActive() {
    _engine.lock();
    auto &mAct = _tt2Track.runtime().variables.m_act;
    mAct = mAct ? 0 : 1;
    _engine.unlock();
}

// --- TT2Host: live engine / cross-track access for W*/BUS/RT ops ---

int16_t TT2TrackEngine::hostTempo() {
    return int16_t(std::lround(_engine.tempo()));
}
void TT2TrackEngine::hostSetTempo(int16_t bpm) {
    int b = bpm; if (b < 1) b = 1; if (b > 1000) b = 1000;
    _engine.setTempo(float(b));
}
int16_t TT2TrackEngine::hostTransportRunning() {
    return _engine.clockRunning() ? 1 : 0;
}
void TT2TrackEngine::hostSetTransportRunning(int16_t state) {
    if (state == 1) { if (!_engine.clockRunning()) _engine.clockStart(); }
    else if (state == 0) { if (_engine.clockRunning()) _engine.clockReset(); }
    else { if (_engine.clockRunning()) _engine.clockStop(); }
}
int16_t TT2TrackEngine::hostBarFraction(uint8_t bars) {
    float frac;
    if (bars <= 1) {
        frac = _engine.measureFraction();
    } else {
        uint32_t span = _engine.measureDivisor() * uint32_t(bars);
        frac = span ? float(_engine.tick() % span) / float(span) : 0.f;
    }
    int r = int(frac * 16383.f);
    if (r < 0) r = 0; if (r > 16383) r = 16383;
    return int16_t(r);
}
int16_t TT2TrackEngine::hostWms(uint8_t mult) {
    float bpm = _engine.tempo();
    if (bpm <= 0.f) return 0;
    double ms = (60000.0 / bpm / 4.0) * mult;
    long r = std::lround(ms); if (r < 1) r = 1; if (r > 32767) r = 32767;
    return int16_t(r);
}
int16_t TT2TrackEngine::hostWtu(uint8_t div, uint8_t mult) {
    float bpm = _engine.tempo();
    if (bpm <= 0.f) return 0;
    if (div == 0) div = 1;
    double ms = (60000.0 / bpm / double(div)) * mult;
    long r = std::lround(ms); if (r < 1) r = 1; if (r > 32767) r = 32767;
    return int16_t(r);
}
int16_t TT2TrackEngine::hostTrackPattern(uint8_t track) {
    if (track >= CONFIG_TRACK_COUNT) return 0;
    return int16_t(_engine.trackEngine(track).pattern());
}
void TT2TrackEngine::hostSetTrackPattern(uint8_t track, uint8_t pattern) {
    if (track >= CONFIG_TRACK_COUNT || pattern >= CONFIG_PATTERN_COUNT) return;
    _engine.selectTrackPattern(track, pattern);
}
int16_t TT2TrackEngine::hostNoteGateGet(uint8_t track, uint8_t step) {
    const NoteSequence *seq = tt2NoteSequence(_engine.model(), track);
    if (!seq || step >= CONFIG_STEP_COUNT) return 0;
    return seq->step(step).gate() ? 1 : 0;
}
void TT2TrackEngine::hostNoteGateSet(uint8_t track, uint8_t step, int16_t v) {
    NoteSequence *seq = tt2NoteSequence(_engine.model(), track);
    if (!seq || step >= CONFIG_STEP_COUNT) return;
    seq->step(step).setGate(v != 0);
}
int16_t TT2TrackEngine::hostNoteNoteGet(uint8_t track, uint8_t step) {
    const NoteSequence *seq = tt2NoteSequence(_engine.model(), track);
    if (!seq || step >= CONFIG_STEP_COUNT) return 0;
    return int16_t(seq->step(step).note() - NoteSequence::Note::Min);
}
void TT2TrackEngine::hostNoteNoteSet(uint8_t track, uint8_t step, int16_t v) {
    NoteSequence *seq = tt2NoteSequence(_engine.model(), track);
    if (!seq || step >= CONFIG_STEP_COUNT) return;
    seq->step(step).setNote(v + NoteSequence::Note::Min);
}
int16_t TT2TrackEngine::hostNoteGateHere(uint8_t track) {
    if (track >= CONFIG_TRACK_COUNT) return 0;
    const TrackEngine &te = _engine.trackEngine(track);
    if (te.trackMode() != Track::TrackMode::Note) return 0;
    int step = te.as<NoteTrackEngine>().currentStep();
    return hostNoteGateGet(track, uint8_t(step));
}
int16_t TT2TrackEngine::hostNoteNoteHere(uint8_t track) {
    if (track >= CONFIG_TRACK_COUNT) return 0;
    const TrackEngine &te = _engine.trackEngine(track);
    if (te.trackMode() != Track::TrackMode::Note) return 0;
    int step = te.as<NoteTrackEngine>().currentStep();
    return hostNoteNoteGet(track, uint8_t(step));
}
int16_t TT2TrackEngine::hostRoutingSource(uint8_t index) {
    int r = int(_engine.routingEngine().routeSource(index) * 16383.f);
    if (r < 0) r = 0; if (r > 16383) r = 16383;
    return int16_t(r);
}
int16_t TT2TrackEngine::hostBusCv(uint8_t index) {
    return voltsToRaw(_engine.busCv(index));
}
void TT2TrackEngine::hostSetBusCv(uint8_t index, int16_t raw) {
    _engine.setBusCv(index, rawToVolts(raw), Engine::BusWriterTeletype);
}

// --- Modulator slots (MO.*) ---

Modulator &TT2TrackEngine::hostModulator(uint8_t idx) {
    if (idx >= CONFIG_MODULATOR_COUNT) idx = CONFIG_MODULATOR_COUNT - 1;
    return _engine.model().project().modulator(idx);
}
int16_t TT2TrackEngine::hostModulatorOutput(uint8_t idx) {
    return int16_t(_engine.modulatorEngine().currentValue(idx));
}
void TT2TrackEngine::hostModulatorTrigger(uint8_t idx) {
    _engine.modulatorEngine().triggerManual(idx);
}

// --- Geode engine (G.*) ---

GeodeConfig &TT2TrackEngine::hostGeodeConfig() {
    return _engine.model().project().geode();
}
int16_t TT2TrackEngine::hostGeodeMix() {
    int v = int(_engine.geodeEngine().mixLevel() * 16383.f + 0.5f);
    if (v < 0) v = 0; if (v > 16383) v = 16383;
    return int16_t(v);
}

// Mirror the live gate-fire sequence (Engine.cpp Geode driver): trigger sets up
// the burst, then setVoicePhase+markVoiceTriggered+triggerImmediate fire the
// first envelope now (update() alone won't, since the gate resets phase).
void TT2TrackEngine::hostGeodeTriggerVoice(uint8_t v, int16_t divs, int16_t repeats) {
    if (v >= GeodeEngine::VoiceCount) return;
    auto &geode = _engine.model().project().geode();
    auto &ge = _engine.geodeEngine();
    float run = (_engine.modulatorEngine().currentValue(1) - 64) / 64.f;
    ge.triggerVoice(v, divs, repeats);
    ge.setVoicePhase(v, 0.f);
    ge.markVoiceTriggered(v);
    ge.triggerImmediate(v, geode.timeNorm(), (geode.intone() - 8192) / 8192.f,
                        run, uint8_t(geode.mode()));
}
void TT2TrackEngine::hostGeodeTriggerAll(int16_t divs, int16_t repeats) {
    auto &geode = _engine.model().project().geode();
    auto &ge = _engine.geodeEngine();
    float run = (_engine.modulatorEngine().currentValue(1) - 64) / 64.f;
    ge.triggerAllVoices(divs, repeats);
    for (int i = 0; i < GeodeEngine::VoiceCount; ++i) {
        ge.setVoicePhase(i, 0.f);
        ge.markVoiceTriggered(i);
    }
    ge.triggerImmediateAll(geode.timeNorm(), (geode.intone() - 8192) / 8192.f,
                           run, uint8_t(geode.mode()));
}

// Run macro 0..16383 (8192 = noon) maps to M2's offset (-64..+63 -> output bias).
// Geode-enable parks M2 at depth 0, so this sets a fixed run by default; M2 depth
// (if set) sweeps run around this center.
int16_t TT2TrackEngine::hostGeodeRun() {
    int macro = (_engine.model().project().modulator(1).offset() + 64) * 128;
    if (macro < 0) macro = 0; if (macro > 16383) macro = 16383;
    return int16_t(macro);
}
void TT2TrackEngine::hostGeodeSetRun(int16_t macro) {
    if (macro < 0) macro = 0; if (macro > 16383) macro = 16383;
    _engine.model().project().modulator(1).setOffset(macro / 128 - 64);
}

// --- MIDI: filter by the track source, populate the buffer, fire the script ---

bool TT2TrackEngine::receiveMidi(MidiPort port, const MidiMessage &message) {
    if (!MidiUtils::matchSource(port, message, _tt2Track.program().midiSource)) {
        return false;  // not our source — let other tracks process it
    }
    processMidiMessage(message);
    return true;  // consume on match
}

void TT2TrackEngine::processMidiMessage(const MidiMessage &message) {
    auto &runtime = _tt2Track.runtime();
    TT2Midi &m = runtime.midi;
    tt2MidiClear(m);

    int fireScript = -1;
    if (message.isNoteOn() && message.velocity() > 0) {
        tt2MidiNoteOn(m, message.channel(), message.note(), message.velocity());
        fireScript = m.on_script;
    } else if (message.isNoteOff() || (message.isNoteOn() && message.velocity() == 0)) {
        tt2MidiNoteOff(m, message.channel(), message.note());
        fireScript = m.off_script;
    } else if (message.isControlChange()) {
        tt2MidiCc(m, message.channel(), message.controlNumber(), message.controlValue());
        fireScript = m.cc_script;
    } else {
        return;  // not a tracked event type
    }

    if (fireScript >= 0 && fireScript < TT2_SCRIPT_COUNT) {
        ScopedHost host(this);
        runScript(uint8_t(fireScript));
    }
}
