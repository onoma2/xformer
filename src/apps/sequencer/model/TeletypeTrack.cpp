#include "TeletypeTrack.h"

void TeletypeTrack::clear() {
    ss_init(&_state);
}

void TeletypeTrack::gateOutputName(int index, StringBuilder &str) const {
    str("TT G%d", (index % 4) + 1);
}

void TeletypeTrack::cvOutputName(int index, StringBuilder &str) const {
    str("TT CV%d", (index % 4) + 1);
}

void TeletypeTrack::write(VersionedSerializedWriter &writer) const {
    (void)writer;
}

void TeletypeTrack::read(VersionedSerializedReader &reader) {
    (void)reader;
    clear();
}
