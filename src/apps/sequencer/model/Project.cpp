#include "Project.h"
#include "ProjectVersion.h"
#include "StochasticTypes.h"

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
    case Routing::Target::Tempo:
        setTempo(floatValue, true);
        break;
    case Routing::Target::Swing:
        setSwing(intValue, true);
        break;
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

    for (auto &userScale : UserScale::userScales) {
        userScale.clear();
    }

    setSelectedTrackIndex(0);
    setSelectedPatternIndex(0);

    // load demo project on simulator
#if PLATFORM_SIM
    // Track 1: Curve track as CV source for DiscreteMap
    track(0).setTrackMode(Track::TrackMode::Curve);
    auto &curve = curveSequence(0, 0);
    curve.setDivisor(192);  // Slow sweep
    curve.setFirstStep(0);
    curve.setLastStep(0);  // Single step for simple ramp

    // Single curve step sweeping full range
    auto &cv = curve.step(0);
    cv.setShape(3);  // Linear
    cv.setMin(0);    // -5V
    cv.setMax(255);  // +5V

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

    // Track 8: PhaseFlux — each of 16 stages showcases a distinct curve / warp
    // / response combination on either the temporal or pitch axis. Sequence
    // divisor = 96 (1/2 note at PPQN-48) so every stage is half a beat long
    // and pulseCount=4 produces an audible 3-pulse ratchet (the t=0 candidate
    // is dropped by the §6.4 collision clamp; the other 3 land cleanly).
    track(7).setTrackMode(Track::TrackMode::PhaseFlux);
    auto &pfTrack = track(7).phaseFluxTrack();
    auto &pfSeq = pfTrack.sequence(0);

    pfSeq.setDivisor(96);

    using TempCurve = PhaseFluxSequence::TemporalCurveType;
    using PitchCurve = PhaseFluxSequence::PitchCurveType;

    for (int i = 0; i < 16; ++i) {
        auto &stage = pfSeq.stage(i);
        stage.setPulseCount(4);
        stage.setBasePitch(0);
        stage.setGateLength(40);
        // stageLen default = 64 = 1× transparent; no override needed.
    }

    // 0..3 — Temporal Linear baseline + warp/response identity demo.
    // With Linear curve, warp and response compose into one powerBend → stage
    // 1 and stage 2 should sound identical, confirming the redundancy.
    pfSeq.stage(0).setTemporalCurve(TempCurve::Linear);
    pfSeq.stage(1).setTemporalCurve(TempCurve::Linear);
    pfSeq.stage(1).setTemporalWarp(50);
    pfSeq.stage(2).setTemporalCurve(TempCurve::Linear);
    pfSeq.stage(2).setTemporalResponse(50);
    pfSeq.stage(3).setTemporalCurve(TempCurve::Bell);

    // 4..7 — Temporal Bell + Bounce. With Bell, warp and response decouple:
    // 4 = peak shifted in input axis, 5 = peak stretched in output amplitude,
    // 6 = both (combination only reachable via two knobs), 7 = Bounce decay.
    pfSeq.stage(4).setTemporalCurve(TempCurve::Bell);
    pfSeq.stage(4).setTemporalWarp(40);
    pfSeq.stage(5).setTemporalCurve(TempCurve::Bell);
    pfSeq.stage(5).setTemporalResponse(40);
    pfSeq.stage(6).setTemporalCurve(TempCurve::Bell);
    pfSeq.stage(6).setTemporalWarp(40);
    pfSeq.stage(6).setTemporalResponse(40);
    pfSeq.stage(7).setTemporalCurve(TempCurve::Bounce);

    // 8..11 — Pitch Ramp baseline + warp/response. Same Linear-equivalence
    // story (9 ≈ 10), then Bell pitch (arc up-and-back).
    pfSeq.stage(8).setPitchCurve(PitchCurve::Ramp);
    pfSeq.stage(9).setPitchCurve(PitchCurve::Ramp);
    pfSeq.stage(9).setPitchWarp(50);
    pfSeq.stage(10).setPitchCurve(PitchCurve::Ramp);
    pfSeq.stage(10).setPitchResponse(50);
    pfSeq.stage(11).setPitchCurve(PitchCurve::Bell);

    // 12..15 — Pitch Bell warp/response decoupling + Triangle + reversed skew.
    pfSeq.stage(12).setPitchCurve(PitchCurve::Bell);
    pfSeq.stage(12).setPitchWarp(40);
    pfSeq.stage(13).setPitchCurve(PitchCurve::Bell);
    pfSeq.stage(13).setPitchResponse(40);
    pfSeq.stage(14).setPitchCurve(PitchCurve::Triangle);
    pfSeq.stage(15).setPitchCurve(PitchCurve::Triangle);
    pfSeq.stage(15).setPitchWarp(-40);

    pfTrack.setOctave(+1);

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
