#include "Project.h"
#include "ProjectVersion.h"
#include "StochasticTypes.h"

#if PLATFORM_SIM
#include "engine/TT2ScriptLoader.h"
namespace {
// Seed a TeletypeV2 track with demo scripts spanning the op surface (incl. the
// templated re-entrant/delay/pattern paths) so the sim boots an auditable patch.
// Trigger scripts S1-S8 = indices 0-7; metro = 8, init = 9.
void seedTeletypeV2Demo(TeletypeProgram &p) {
    // Deterministic by design: no RND, so every op's effect is self-evident on
    // the HUD / modulator page. The metro drives Mod 1 so it animates live.
    loadScriptText(p, 0,                  // S1: classic ops (fixed chord + IF/EVERY)
        "CV 2 N 60\n"
        "CV 3 N ADD 4 0\n"
        "CV 4 N MUL 2 6\n"
        "IF EQ 1 1 : TR.P 2\n"
        "EVERY 2 : TR.P 3\n");
    loadScriptText(p, 1,                  // S2: Performer window + BUS (cross-track)
        "WBPM 120\n"
        "WR 1\n"
        "BUS 1 V 2\n"
        "CV 5 BUS 1\n"
        "WNG 2 0 1\n"
        "WNN 2 0 7\n");
    loadScriptText(p, 2,                  // S3: additive routing + fans out to the templated-path demos (metro-fired)
        "MO.DEPTH 1 MUL X 12\n"           // relocated from metro (keeps Mod 1 depth live)
        "CV 8 N MUL X 2\n"                // TT2 CV sums onto physical CV jack 8
        "TR.P 8\n"                        // TT2 gate ORs onto physical gate jack 8
        "SCRIPT 5\n"                      // -> S5 pattern walk (re-entry depth 3)
        "SCRIPT 6\n"                      // -> S6 command stack (S.ALL re-enters, depth 4)
        "SCRIPT 7\n");                    // -> S7 delay queue
    loadScriptText(p, 3,                  // S4: Geode
        "G.MODE 1\n"
        "G.TUNE 1 2\n"
        "G.V 1 4 2\n"
        "G.V 2 4 2\n"
        "G.RUN 64\n");
    loadScriptText(p, 4,                  // S5: pattern family — P.N/P.L/PN write + P.NEXT walk -> CV 6 cycles 0,7,12
        "P.N 0\n"                         // select pattern bank 0
        "P.L 3\n"                         // length 3 (cursor wraps every 3 steps)
        "PN 0 0 0\n"                      // bank0[0]=0
        "PN 0 1 7\n"                      // bank0[1]=7
        "PN 0 2 12\n"                     // bank0[2]=12
        "CV 6 N P.NEXT\n");               // advance cursor, quantize, drive CV 6 (exercises pattern* family)
    loadScriptText(p, 5,                  // S6: command stack — push a body then run it; exercises runStoredCommand re-entry
        "S : TR.P 6\n"                    // push "TR.P 6" onto the command stack
        "S.ALL\n"                         // run + clear the stack -> gate 6 pulses
        "SCRIPT 8\n");                    // -> S8 pattern transforms (re-entry depth 4)
    loadScriptText(p, 6,                  // S7: delay queue — gate 7 pulses 250ms after each metro tick
        "DEL 250: TR.P 7\n");             // exercises tt2AdvanceDelays + the delay queue
    loadScriptText(p, 7,                  // S8: pattern transforms — P.PUSH build + P.REV on bank 1 -> CV 7 walks the reversed seq
        "P.N 1\n"                         // bank 1 (separate from S5's bank 0)
        "P.L 0\n"                         // reset empty each fire (idempotent rebuild)
        "P.PUSH 0\n"                      // append 0  (patternPush)
        "P.PUSH 5\n"                      // append 5 -> {0,5}
        "P.REV\n"                         // reverse -> {5,0}  (patternReverse)
        "CV 7 N P.NEXT\n");               // walk reversed -> CV 7 cycles 5,0 (contrast S5's forward)
    loadScriptText(p, TT2_METRO_SCRIPT,   // M: counter drives Mod 1 + jack 1, fires S3 for jack 8
        "X ADD X 1\n"
        "X MOD X 8\n"
        "MO.RATE 1 MUL X 16\n"
        "CV 1 N MUL X 2\n"
        "TR.P 1\n"
        "SCRIPT 3\n");
    loadScriptText(p, TT2_INIT_SCRIPT,    // I: boot config (runs once on start)
        "X 0\n"
        "MO.SHAPE 1 1\n"
        "MO.RATE 1 30\n"
        "MO.DEPTH 1 50\n"
        "M 500\n"
        "M.ACT 1\n");
}

// Seed a TeletypeMini track: 4 scenes switched seamlessly by the w-pattern
// selector (scene = pattern % 4). Per-scene tempo+pitch live in the metro
// script (runs each tick on the active scene); script0 boots the metro once.
void seedTeletypeMiniDemo(TT2MiniTrack &mt) {
    static const char *metro[TT2ConfigMini::SceneCount] = {
        "M 500\nCV 2 N 60\nTR.P 4\n",    // scene 0: medium, root (note 60 = 0V)
        "M 250\nCV 2 N 67\nTR.P 4\n",    // scene 1: faster, fifth
        "M 1000\nCV 2 N 72\nTR.P 4\n",   // scene 2: slow, octave
        "M 125\nCV 2 N 63\nTR.P 4\n",    // scene 3: fastest, minor third
    };
    for (int s = 0; s < TT2ConfigMini::SceneCount; ++s) {
        auto &p = mt.program(s);
        loadScriptText(p, 0, "M 500\nM.ACT 1\n");                // script0/trigger1: boot the metro
        loadScriptText(p, 1, "CV 2 N 69\nTR.P 4\n");              // script1/trigger2: manual accent
        loadScriptText(p, TT2ConfigMini::MetroScript, metro[s]); // metro: per-scene tempo/pitch on CV2/TR4
    }
}

// Cross-track demo: Full track 0 conducts Mini track 1. The Full metro cycles the
// Mini's scene (WP = switch w-pattern → scene) and fires its accent (WS = trigger
// script); each Mini scene has its own tempo/pitch, so the cycle is audible. The
// two new cross-track ops, end to end. Mini metro is booted once (X==0) so the
// per-tick WS accent doesn't reset its tempo.
void seedTeletypeCrossTrackDemo(TeletypeProgram &full, TT2MiniTrack &mini) {
    loadScriptText(full, 0, "WP.SET 2 MOD ADD WP 2 1 4\n");   // S1: manual next-scene on Mini
    loadScriptText(full, 1, "WS 2 2\n");                  // S2: manual fire Mini accent
    loadScriptText(full, TT2_METRO_SCRIPT,
        "IF EQ X 0 : WS 2 1\n"   // boot Mini metro once
        "X ADD X 1\n"
        "WP.SET 2 MOD X 4\n"     // cycle Mini scene 0..3
        "WS 2 2\n"               // fire Mini accent (Mini script 1)
        "CV 1 N MOD X 7\n"       // own note on CV1
        "TR.P 1\n");             // own gate on TR1
    loadScriptText(full, TT2_INIT_SCRIPT,
        "X 0\n"
        "M 1500\n"               // 1.5s conductor tick
        "M.ACT 1\n");

    static const char *sceneMetro[TT2ConfigMini::SceneCount] = {
        "M 500\nCV 2 N 60\nTR.P 4\n",    // scene 0: medium, root (note 60 = 0V)
        "M 250\nCV 2 N 67\nTR.P 4\n",    // scene 1: faster, fifth
        "M 1000\nCV 2 N 72\nTR.P 4\n",   // scene 2: slow, octave
        "M 125\nCV 2 N 63\nTR.P 4\n",    // scene 3: fastest, minor third
    };
    static const char *sceneAccent[TT2ConfigMini::SceneCount] = {
        "CV 2 N 64\nTR.P 4\n",            // per-scene accent fired by Full's WS 2 2
        "CV 2 N 11\nTR.P 4\n",
        "CV 2 N 16\nTR.P 4\n",
        "CV 2 N 67\nTR.P 4\n",
    };
    for (int s = 0; s < TT2ConfigMini::SceneCount; ++s) {
        auto &p = mini.program(s);
        loadScriptText(p, 0, "M 500\nM.ACT 1\n");                       // boot (WS 2 1, once)
        loadScriptText(p, 1, sceneAccent[s]);                          // accent (WS 2 2, per tick)
        loadScriptText(p, TT2ConfigMini::MetroScript, sceneMetro[s]);  // per-scene tempo/pitch
    }
}
} // namespace
#endif

Project::Project() :
    _playState(*this),
    _routing(*this)
{
    for (size_t i = 0; i < _tracks.size(); ++i) {
        _tracks[i].setTrackIndex(i);
    }

    clear();
}

void Project::writeRouted(Routing::Target target, int intValue, float floatValue) {
    switch (target) {
    case Routing::Target::CvRouteScan:
        setCvRouteScan(intValue, true);
        break;
    case Routing::Target::CvRouteRoute:
        setCvRouteRoute(intValue, true);
        break;
    default:
        break;
    }
}

void Project::clear() {
    _slot = uint8_t(-1);
    StringUtils::copy(_name, "INIT", sizeof(_name));
    setAutoLoaded(false);
    setTempo(120.f);
    setSwing(50);
    setTimeSignature(TimeSignature());
    setSyncMeasure(1);
    setAlwaysSyncPatterns(false);
    _scaleGroup.raw = 0;
    setScale(0);
    setRootNote(0);
    setScaleRotate(0);
    setMonitorMode(Types::MonitorMode::Always);
    setRecordMode(Types::RecordMode::Overdub);
    setMidiInputMode(Types::MidiInputMode::All);
    setMidiIntegrationMode(Types::MidiIntegrationMode::None);
    setMidiProgramOffset(0);
    setCvGateInput(Types::CvGateInput::Off);
    setCurveCvInput(Types::CurveCvInput::Off);
    setBusSafety(true);

    _clockSetup.clear();

    for (auto &track : _tracks) {
        track.clear();
    }

    for (int i = 0; i < CONFIG_CHANNEL_COUNT; ++i) {
        _cvOutputTracks[i] = i;
        _gateOutputTracks[i] = i;
    }

    _song.clear();
    _playState.clear();
    _routing.clear();
    _midiOutput.clear();
    _cvRoute.clear();

    for (auto &modulator : _modulators) {
        modulator.clear();
    }
    _cvOutputModulators.fill(0);
    _geode.clear();

    for (auto &userScale : UserScale::userScales) {
        userScale.clear();
    }

    setSelectedTrackIndex(0);
    setSelectedPatternIndex(0);

    // load demo project on simulator
    //
    // STRESS_TIMING_DEMO: temporary -Os timing-probe patch. When 1, a fresh
    // project (New Project / factory init) boots a heavy 8-track patch on BOTH
    // hardware and sim, for reading the MonitorPage "ENG WORST" probe under load.
    // Set back to 0 when the timing experiment is done. Non-destructive — loading
    // a saved project still overwrites clear().
#define STRESS_TIMING_DEMO 0
    // TT_CROSSTRACK_DEMO: when 1, the sim boots a TT2 cross-track patch (Full
    // track 0 conducts Mini track 1 — WP scene-switch + WS script-trigger)
    // instead of the 8-track showcase default. Set to 0 for the showcase.
#define TT_CROSSTRACK_DEMO 0
#if STRESS_TIMING_DEMO
    // 8 active tracks across the heaviest engines at fast tempo. RAM-safe: only
    // ONE Stochastic track (two have hardfaulted on boot — see PROJECT.md).
    setTempo(240.f);
    setScale(2);

    track(0).setTrackMode(Track::TrackMode::Stochastic);

    track(1).setTrackMode(Track::TrackMode::PhaseFlux);
    track(1).phaseFluxTrack().setOctave(+1);
    track(2).setTrackMode(Track::TrackMode::PhaseFlux);

    track(3).setTrackMode(Track::TrackMode::Curve);
    {
        auto &c = curveSequence(3, 0);
        c.setDivisor(12);   // fast sweep — CV churn every few ticks
        c.setFirstStep(0);
        c.setLastStep(15);
        for (int i = 0; i < 16; ++i) {
            auto &s = c.step(i);
            s.setShape(3);
            s.setMin(0);
            s.setMax(255);
        }
    }

    track(4).setTrackMode(Track::TrackMode::Tuesday);

    for (int t = 5; t <= 7; ++t) {
        track(t).setTrackMode(Track::TrackMode::Note);
        noteSequence(t, 0).setLastStep(15);
        noteSequence(t, 0).setGates({ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 });
    }
#elif PLATFORM_SIM
#if TT_CROSSTRACK_DEMO
    track(0).setTrackMode(Track::TrackMode::TeletypeV2);
    track(1).setTrackMode(Track::TrackMode::TeletypeMini);
    seedTeletypeCrossTrackDemo(track(0).tt2Track().program(), track(1).tt2MiniTrack());
    setTempo(120.f);
    setScale(2); // 2 corresponds to H.Minor scale
#else
    // Sim default: 8-track sparse ambient showcase, one engine per slot. Slow
    // tempo + long, mismatched divisors so the tracks drift in and out of phase.
    setTempo(58.f);
    setScale(2); // H.Minor

    // Track 0 — TeletypeV2: sparse script. 2s metro fires root then fifth, so a
    // single gate every ~4s on each, scripts driving CV1/gate1.
    track(0).setTrackMode(Track::TrackMode::TeletypeV2);
    {
        auto &p = track(0).tt2Track().program();
        loadScriptText(p, TT2_INIT_SCRIPT, "X 0\nM 2000\nM.ACT 1\n");
        loadScriptText(p, TT2_METRO_SCRIPT,
            "X ADD X 1\n"
            "X MOD X 4\n"
            "IF EQ X 0 : CV 1 N 60\n"     // note 60 = 0V (raw 8192); N 0 would be -5V
            "IF EQ X 0 : TR.P 1\n"
            "IF EQ X 2 : CV 1 N 67\n"     // fifth above the root
            "IF EQ X 2 : TR.P 1\n");
    }

    // Track 1 — TeletypeMini: scene-based mini scripts (per-scene tempo/pitch).
    track(1).setTrackMode(Track::TrackMode::TeletypeMini);
    seedTeletypeMiniDemo(track(1).tt2MiniTrack());

    // Track 2 — Indexed: route-driven note index. A slow Mod1 LFO routed to
    // IndexedA transposes a short 5-note motif up/down ~1 octave over time.
    track(2).setTrackMode(Track::TrackMode::Indexed);
    track(2).indexedTrack().setOctave(-1);
    {
        auto &seq = indexedSequence(2, 0);
        seq.setDivisor(144);
        seq.setScale(2);
        seq.setActiveLength(5);
        static const int8_t motif[5] = { 0, 3, 2, 5, 4 };
        for (int i = 0; i < 5; ++i) {
            auto &s = seq.step(i);
            s.setNoteIndex(motif[i]);
            s.setDuration(192);   // quarter-length step
            s.setGateLength(140); // long, breathing gate
        }
        IndexedSequence::RouteConfig rc;
        rc.targetParam = IndexedSequence::ModTarget::NoteIndex;
        rc.source = IndexedSequence::RouteSource::A;
        rc.amount = 100.f;        // ±1 octave swing
        seq.setRouteA(rc);
    }
    {
        auto &mod = modulator(0);
        mod.setShape(Modulator::Shape::Sine);
        mod.setRateDomain(Modulator::RateDomain::Free);
        mod.setRate(30);          // 0.30 Hz
        mod.setDepth(110);
        auto &rt = routing().route(routing().findEmptyRoute());
        rt.setTarget(Routing::Target::IndexedA);
        rt.setSource(Routing::Source::Mod1);
        rt.setTracks(1 << 2);
        rt.setMin(0.f);
        rt.setMax(1.f);
    }

    // Track 3 — DiscreteMap: internal sawtooth ramp (no live CV needed) sweeps 5
    // enabled stages once per ~2-bar period — a slow ascending arpeggio.
    track(3).setTrackMode(Track::TrackMode::DiscreteMap);
    {
        auto &seq = discreteMapSequence(3, 0);
        seq.setClockSource(DiscreteMapSequence::ClockSource::Internal);
        seq.setDivisor(384);
        seq.setScale(2);
        seq.setOctave(-1);
        seq.setGateLength(60);
        static const int8_t thr[5]  = { -80, -40, 0, 40, 80 };
        static const int8_t note[5] = { 0, 2, 3, 5, 7 };
        for (int i = 0; i < 5; ++i) {
            auto &st = seq.stage(i);
            st.setThreshold(thr[i]);
            st.setDirection(DiscreteMapSequence::Stage::TriggerDir::Rise);
            st.setNoteIndex(note[i]);
        }
    }

    // Track 4 — Stochastic: sparse generative loop. High rest, low complexity,
    // narrow pitch field, full-length notes — slow drifting melody.
    track(4).setTrackMode(Track::TrackMode::Stochastic);
    track(4).stochasticTrack().setOctave(-1);
    {
        auto &seq = track(4).stochasticTrack().sequence(0);
        seq.setScale(2);
        seq.setDivisor(48);
        seq.setClockMultiplier(60);
        seq.setRest(80);
        seq.setComplexity(15);
        seq.setVariation(5);
        seq.setRange(15);
        seq.setMaskMelody(20);
        seq.setNoteDuration(5);
    }

    // Track 5 — PhaseFlux: sparse stages, slow divisor. Few pulses per stage,
    // long stage lengths, 1-in-4 mask — phase movement that breathes.
    track(5).setTrackMode(Track::TrackMode::PhaseFlux);
    track(5).phaseFluxTrack().setOctave(-2);
    {
        auto &seq = phaseFluxSequence(5, 0);
        seq.setDivisor(288);
        seq.setScale(2);
        seq.setFirstStage(0);
        seq.setLastStage(5);
        static const int pulse[6] = { 1, 0, 1, 0, 2, 0 };
        static const int pitch[6] = { 0, 2, 3, 5, 4, 7 };
        for (int i = 0; i < 6; ++i) {
            auto &st = seq.stage(i);
            st.setPulseCount(pulse[i]);
            st.setBasePitch(pitch[i]);
            st.setGateLength(30);
            st.setLength(12);
            st.setMask(PhaseFluxSequence::MaskType::OneInFour);
        }
    }

    // Track 6 — Tuesday: Turing machine (algorithm 15). Low flip power for a
    // slowly-mutating drone, tight ornament, sparse mask, slow divisor.
    track(6).setTrackMode(Track::TrackMode::Tuesday);
    {
        auto &seq = tuesdaySequence(6, 0);
        seq.setAlgorithm(15); // Turing
        seq.setDivisor(96);
        seq.setScale(2);
        seq.setOctave(-1);
        seq.setFlow(8);
        seq.setPower(2);
        seq.setOrnament(2);
        seq.setMaskParameter(4);
        seq.setGateLength(30);
    }

    // Track 7 — Fractal: mirrors Stochastic (track 4) + Tuesday (track 6),
    // gate-OR / gated-CV, armed on every pattern so it captures on play.
    track(7).setTrackMode(Track::TrackMode::Fractal);
    {
        auto &ft = track(7).fractalTrack();
        ft.setSourceA(4);
        ft.setSourceB(6);
        ft.setGateLogic(FractalTrack::GateLogic::Or);
        ft.setCvLogic(FractalTrack::CvLogic::Gated);
        for (int p = 0; p < CONFIG_PATTERN_COUNT; ++p) {
            ft.sequence(p).setRecordMode(FractalSequence::RecordMode::Replace);
            ft.sequence(p).setRecordTrigger(1); // armed
        }
    }
#endif // TT_CROSSTRACK_DEMO
#endif

    _observable.notify(ProjectCleared);
}

void Project::clearPattern(int patternIndex) {
    for (auto &track : _tracks) {
        track.clearPattern(patternIndex);
    }
}

void Project::setTrackMode(int trackIndex, Track::TrackMode trackMode) {
    // TODO make sure engine is synced to this before updating UI
    _playState.revertSnapshot();
    _tracks[trackIndex].setTrackMode(trackMode);
    _observable.notify(TrackModeChanged);
}

void Project::write(VersionedSerializedWriter &writer) const {
    writer.write(_name, NameLength + 1);
    writer.write(_tempo.base);
    writer.write(_swing.base);
    _timeSignature.write(writer);
    writer.write(_syncMeasure);
    writer.write(_alwaysSyncPatterns);
    uint8_t scaleField = scale();
    uint8_t rootNoteField = rootNote();
    uint8_t scaleRotateField = uint8_t(scaleRotate());
    writer.write(scaleField);
    writer.write(rootNoteField);
    writer.write(scaleRotateField);
    writer.write(_monitorMode);
    writer.write(_recordMode);
    writer.write(_midiInputMode);
    writer.write(_midiIntegrationMode);
    writer.write(_midiProgramOffset);
    _midiInputSource.write(writer);
    writer.write(_cvGateInput);
    writer.write(_curveCvInput);
    writer.write(_busSafety);

    _clockSetup.write(writer);

    writeArray(writer, _tracks);
    writeArray(writer, _cvOutputTracks);
    writeArray(writer, _gateOutputTracks);

    writeArray(writer, _modulators);
    writeArray(writer, _cvOutputModulators);
    _geode.write(writer);

    writer.write(_selectedModulatorIndex);

    _song.write(writer);
    _playState.write(writer);
    _routing.write(writer);
    _midiOutput.write(writer);
    _cvRoute.write(writer);

    writeArray(writer, UserScale::userScales);

    writer.write(_selectedTrackIndex);
    writer.write(_selectedPatternIndex);

    writer.writeHash();

    _autoLoaded = false;
}

bool Project::read(VersionedSerializedReader &reader) {
    clear();

    reader.read(_name, NameLength + 1, ProjectVersion::Version5);
    reader.read(_tempo.base);
    reader.read(_swing.base);
    if (reader.dataVersion() >= ProjectVersion::Version18) {
        _timeSignature.read(reader);
    }
    reader.read(_syncMeasure);
    if (reader.dataVersion() >= ProjectVersion::Version32) {
        reader.read(_alwaysSyncPatterns);
    }
    uint8_t scaleField = 0;
    uint8_t rootNoteField = 0;
    uint8_t scaleRotateField = 0;
    reader.read(scaleField);
    reader.read(rootNoteField);
    reader.read(scaleRotateField);
    setScale(scaleField);
    setRootNote(rootNoteField);
    setScaleRotate(scaleRotateField);
    reader.read(_monitorMode, ProjectVersion::Version30);
    reader.read(_recordMode);
    if (reader.dataVersion() >= ProjectVersion::Version29) {
        reader.read(_midiInputMode);
        _midiInputSource.read(reader);
    }
    if (reader.dataVersion() >= ProjectVersion::Version32) {
        reader.read(_midiIntegrationMode);
        reader.read(_midiProgramOffset);
    }
    reader.read(_cvGateInput, ProjectVersion::Version6);
    reader.read(_curveCvInput, ProjectVersion::Version11);
    reader.read(_busSafety);

    _clockSetup.read(reader);

    readArray(reader, _tracks);
    readArray(reader, _cvOutputTracks);
    readArray(reader, _gateOutputTracks);

    readArray(reader, _modulators);
    readArray(reader, _cvOutputModulators);
    _geode.read(reader);

    reader.read(_selectedModulatorIndex);

    _song.read(reader);
    _playState.read(reader);
    _routing.read(reader);
    _midiOutput.read(reader);
    _cvRoute.read(reader);

    if (reader.dataVersion() >= ProjectVersion::Version5) {
        readArray(reader, UserScale::userScales);
    }

    reader.read(_selectedTrackIndex);
    reader.read(_selectedPatternIndex);

    bool success = reader.checkHash();
    if (success) {
        _observable.notify(ProjectRead);
    } else {
        clear();
    }

    return success;
}
