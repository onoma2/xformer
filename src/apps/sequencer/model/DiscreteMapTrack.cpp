#include "DiscreteMapTrack.h"
#include "ProjectVersion.h"

void DiscreteMapTrack::gateOutputName(int index, StringBuilder &str) const {
    str("G%d", _trackIndex + 1);
}

void DiscreteMapTrack::cvOutputName(int index, StringBuilder &str) const {
    str("CV%d", _trackIndex + 1);
}

void DiscreteMapTrack::clear() {
    _routedInput = 0.f;
    _routedScanner = 0.f;
    setCvUpdateMode(CvUpdateMode::Gate);  // Initialize default value
    setPlayMode(Types::PlayMode::Free);
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void DiscreteMapTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_cvUpdateMode);
    writer.write(_playMode);
    writeArray(writer, _sequences);
}

void DiscreteMapTrack::read(VersionedSerializedReader &reader) {
    reader.read(_cvUpdateMode);
    reader.read(_playMode);
    readArray(reader, _sequences);
}

