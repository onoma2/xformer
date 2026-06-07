#include "IndexedTrack.h"
#include "ProjectVersion.h"

void IndexedTrack::gateOutputName(int index, StringBuilder &str) const {
    str("G%d", _trackIndex + 1);
}

void IndexedTrack::cvOutputName(int index, StringBuilder &str) const {
    str("CV%d", _trackIndex + 1);
}

void IndexedTrack::clear() {
    _cvUpdateMode = CvUpdateMode::Gate;
    setPlayMode(Types::PlayMode::Free);
    _routedSync = 0.f;
    setOctave(0);
    setTranspose(0);
    setSlideTime(25);
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void IndexedTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_cvUpdateMode);
    writer.write(_playMode);
    writer.write(_octave);
    writer.write(_transpose);
    writer.write(_slideTime);
    writeArray(writer, _sequences);
}

void IndexedTrack::read(VersionedSerializedReader &reader) {
    reader.read(_cvUpdateMode);
    reader.read(_playMode);
    reader.read(_octave);
    reader.read(_transpose);
    reader.read(_slideTime);
    readArray(reader, _sequences);
}
