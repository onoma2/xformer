#include "IndexedTrack.h"
#include "ProjectVersion.h"

void IndexedTrack::gateOutputName(int index, StringBuilder &str) const {
    str("G%d", _trackIndex + 1);
}

void IndexedTrack::cvOutputName(int index, StringBuilder &str) const {
    str("CV%d", _trackIndex + 1);
}

void IndexedTrack::clear() {
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void IndexedTrack::write(VersionedSerializedWriter &writer) const {
    writeArray(writer, _sequences);
}

void IndexedTrack::read(VersionedSerializedReader &reader) {
    readArray(reader, _sequences);
}

void IndexedTrack::writeRouted(Routing::Target target, int intValue, float floatValue) {
    // For now, forward sequence-level routable params to all patterns
    // In the future, we might add track-level routable params here
    for (auto &sequence : _sequences) {
        sequence.writeRouted(target, intValue, floatValue);
    }
}
