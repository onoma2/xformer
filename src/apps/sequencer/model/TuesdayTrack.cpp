#include "TuesdayTrack.h"
#include "ProjectVersion.h"

void TuesdayTrack::gateOutputName(int index, StringBuilder &str) const {
    str("G%d", _trackIndex + 1);
}

void TuesdayTrack::cvOutputName(int index, StringBuilder &str) const {
    str("CV%d", _trackIndex + 1);
}

void TuesdayTrack::clear() {
    setPlayMode(Types::PlayMode::Aligned);
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void TuesdayTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_playMode);
    writeArray(writer, _sequences);
}

void TuesdayTrack::read(VersionedSerializedReader &reader) {
    reader.read(_playMode);
    readArray(reader, _sequences);
}
