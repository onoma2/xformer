#include "Project.h"
#include "ProjectVersion.h"
#include "StochasticTypes.h"

#if PLATFORM_SIM
#include "engine/TT2ScriptLoader.h"
namespace {
// Seed a TeletypeV2 track with demo scripts spanning the op surface so the
// simulator boots an auditable TT2 patch without hand-typing on hardware.
// Trigger scripts S1-S4 = indices 0-3, metro = 4, init = 5.
void seedTeletypeV2Demo(TeletypeProgram &p) {
    // Deterministic by design: no RND, so every op's effect is self-evident on
    // the HUD / modulator page. The metro drives Mod 1 so it animates live.
    loadScriptText(p, 0,                  // S1: classic ops (fixed chord + IF/EVERY)
        "CV 2 N 0\n"
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
    loadScriptText(p, 2,                  // S3: modulator one-shot (manual trigger)
        "MO.SHAPE 2 0\n"
        "MO.RATE 2 80\n"
        "MO.DEPTH 2 127\n"
        "MO.TRIG 2\n");
    loadScriptText(p, 3,                  // S4: Geode
        "G.MODE 1\n"
        "G.TUNE 1 2\n"
        "G.V 1 4 2\n"
        "G.V 2 4 2\n"
        "G.RUN 64\n");
    loadScriptText(p, TT2_METRO_SCRIPT,   // M: deterministic counter drives Mod 1
        "X ADD X 1\n"
        "X MOD X 8\n"
        "MO.RATE 1 MUL X 16\n"
        "MO.DEPTH 1 MUL X 12\n"
        "CV 1 N MUL X 2\n"
        "TR.P 1\n");
    loadScriptText(p, TT2_INIT_SCRIPT,    // I: boot config (runs once on start)
        "X 0\n"
        "MO.SHAPE 1 1\n"
        "MO.RATE 1 30\n"
        "MO.DEPTH 1 50\n"
        "M 500\n"
        "M.ACT 1\n");
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
    setScale(0);
    setRootNote(0);
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
    // Track 1: TeletypeV2 showcase - 6 scripts spanning the op surface so the sim
    // boots an auditable TT2 patch (classic / Performer W. + BUS / MO / Geode).
    track(0).setTrackMode(Track::TrackMode::TeletypeV2);
    seedTeletypeV2Demo(track(0).tt2Track().program());

    noteSequence(1, 0).setLastStep(15);
    noteSequence(1, 0).setGates({ 0,0,0,0,1,0,0,0,0,0,0,0,1,0,0,0 });
    noteSequence(2, 0).setLastStep(15);
    noteSequence(2, 0).setGates({ 0,1,0,0,1,0,0,1,0,0,1,0,0,1,0,0 });
    noteSequence(3, 0).setLastStep(15);
    noteSequence(3, 0).setGates({ 0,0,0,0,1,0,0,0,0,0,0,1,0,0,0,0 });
    noteSequence(4, 0).setLastStep(15);
    noteSequence(4, 0).setGates({ 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1 });
    noteSequence(5, 0).setLastStep(15);
    noteSequence(5, 0).setGates({ 0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0 });

    // Track 7: Note with empty sequence (gates all off — NoteSequence::clear default).

    // Track 8: PhaseFlux — boots to a NoteTrack-equivalent metronome straight
    // from Sequence::clear(): stages 0..3 active (4 pulses each) over a 1-bar
    // cycle at the 1/16 divisor = 16 sixteenths across the bar, in sync with
    // the NoteTracks above. No per-stage override needed.
    track(7).setTrackMode(Track::TrackMode::PhaseFlux);
    track(7).phaseFluxTrack().setOctave(+1);

    setTempo(80.f);
    setScale(2); // 2 corresponds to Minor scale
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
    if (trackMode == Track::TrackMode::Teletype) {
        _tracks[trackIndex].teletypeTrack().seedOutputDestsFromTrackIndex(trackIndex);
    }
    _observable.notify(TrackModeChanged);
}

void Project::write(VersionedSerializedWriter &writer) const {
    writer.write(_name, NameLength + 1);
    writer.write(_tempo.base);
    writer.write(_swing.base);
    _timeSignature.write(writer);
    writer.write(_syncMeasure);
    writer.write(_alwaysSyncPatterns);
    writer.write(_scale);
    writer.write(_rootNote);
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
    reader.read(_scale);
    reader.read(_rootNote);
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
