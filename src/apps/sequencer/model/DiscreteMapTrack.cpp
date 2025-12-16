#include "DiscreteMapTrack.h"
#include "ProjectVersion.h"

void DiscreteMapTrack::gateOutputName(int index, StringBuilder &str) const {
    str("G%d", _trackIndex + 1);
}

void DiscreteMapTrack::cvOutputName(int index, StringBuilder &str) const {
    str("CV%d", _trackIndex + 1);
}

void DiscreteMapTrack::clear() {
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void DiscreteMapTrack::write(VersionedSerializedWriter &writer) const {
    writeArray(writer, _sequences);
}

void DiscreteMapTrack::read(VersionedSerializedReader &reader) {
    readArray(reader, _sequences);
}

void DiscreteMapTrack::writeRouted(Routing::Target target, int intValue, float floatValue) {
    // Currently no track-level routable parameters for DiscreteMap tracks.
    (void)target;
    (void)intValue;
    (void)floatValue;
}
