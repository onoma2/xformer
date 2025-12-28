#include "Project.h"
#include "ProjectVersion.h"

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

    // Track 8: Indexed
    track(7).setTrackMode(Track::TrackMode::Indexed);
    auto &indexed = indexedSequence(7, 0);
    indexed.setActiveLength(5);
    for (int i = 0; i < 5; ++i) {
        indexed.step(i).setNoteIndex(i * 2);
        indexed.step(i).setGateLength(5);
    }

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

    _clockSetup.write(writer);

    writeArray(writer, _tracks);
    writeArray(writer, _cvOutputTracks);
    writeArray(writer, _gateOutputTracks);

    _song.write(writer);
    _playState.write(writer);
    _routing.write(writer);
    _midiOutput.write(writer);

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

    _clockSetup.read(reader);

    readArray(reader, _tracks);
    readArray(reader, _cvOutputTracks);
    readArray(reader, _gateOutputTracks);

    _song.read(reader);
    _playState.read(reader);
    _routing.read(reader);
    _midiOutput.read(reader);

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
