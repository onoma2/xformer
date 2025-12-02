#include "Track.h"
#include "Project.h"
#include "ProjectVersion.h"

void Track::clear() {
    _trackMode = TrackMode::Default;
    _linkTrack = -1;
    setCvOutputRotate(0);
    setGateOutputRotate(0);

    initContainer();
}

void Track::clearPattern(int patternIndex) {
    switch (_trackMode) {
    case TrackMode::Note:
        _track.note->sequence(patternIndex).clear();
        break;
    case TrackMode::Curve:
        _track.curve->sequence(patternIndex).clear();
        break;
    case TrackMode::MidiCv:
        break;
    case TrackMode::Tuesday:
        _track.tuesday->sequence(patternIndex).clear();
        break;
    case TrackMode::Last:
        break;
    }
}

void Track::copyPattern(int src, int dst) {
    switch (_trackMode) {
    case TrackMode::Note:
        _track.note->sequence(dst) = _track.note->sequence(src);
        break;
    case TrackMode::Curve:
        _track.curve->sequence(dst) = _track.curve->sequence(src);
        break;
    case TrackMode::MidiCv:
        break;
    case TrackMode::Tuesday:
        _track.tuesday->sequence(dst) = _track.tuesday->sequence(src);
        break;
    case TrackMode::Last:
        break;
    }
}

bool Track::duplicatePattern(int patternIndex) {
    if (patternIndex < CONFIG_PATTERN_COUNT - 1) {
        copyPattern(patternIndex, patternIndex + 1);
        return true;
    }
    return false;
}

void Track::gateOutputName(int index, StringBuilder &str) const {
    switch (_trackMode) {
    case TrackMode::Note:
    case TrackMode::Curve:
        str("Gate");
        break;
    case TrackMode::MidiCv:
        _track.midiCv->gateOutputName(index, str);
        break;
    case TrackMode::Tuesday:
        _track.tuesday->gateOutputName(index, str);
        break;
    case TrackMode::Last:
        break;
    }
}

void Track::cvOutputName(int index, StringBuilder &str) const {
    switch (_trackMode) {
    case TrackMode::Note:
    case TrackMode::Curve:
        str("CV");
        break;
    case TrackMode::MidiCv:
        _track.midiCv->cvOutputName(index, str);
        break;
    case TrackMode::Tuesday:
        _track.tuesday->cvOutputName(index, str);
        break;
    case TrackMode::Last:
        break;
    }
}

void Track::write(VersionedSerializedWriter &writer) const {
    writer.writeEnum(_trackMode, trackModeSerialize);
    writer.write(_linkTrack);
    writer.write(_cvOutputRotate.base);
    writer.write(_gateOutputRotate.base);

    switch (_trackMode) {
    case TrackMode::Note:
        _track.note->write(writer);
        break;
    case TrackMode::Curve:
        _track.curve->write(writer);
        break;
    case TrackMode::MidiCv:
        _track.midiCv->write(writer);
        break;
    case TrackMode::Tuesday:
        _track.tuesday->write(writer);
        break;
    case TrackMode::Last:
        break;
    }
}

void Track::read(VersionedSerializedReader &reader) {
    reader.readEnum(_trackMode, trackModeSerialize);
    reader.read(_linkTrack);
    
    if (reader.dataVersion() >= ProjectVersion::Version47) { // Assuming Version47 is next available or using dummy check
         reader.read(_cvOutputRotate.base);
         reader.read(_gateOutputRotate.base);
    } else {
         setCvOutputRotate(0);
         setGateOutputRotate(0);
    }

    initContainer();

    switch (_trackMode) {
    case TrackMode::Note:
        _track.note->read(reader);
        break;
    case TrackMode::Curve:
        _track.curve->read(reader);
        break;
    case TrackMode::MidiCv:
        _track.midiCv->read(reader);
        break;
    case TrackMode::Tuesday:
        _track.tuesday->read(reader);
        break;
    case TrackMode::Last:
        break;
    }
}


