#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "IndexedSequence.h"

#include <array>

class IndexedTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using IndexedSequenceArray = std::array<IndexedSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    //----------------------------------------
    // Properties
    //----------------------------------------

    // sequences

    const IndexedSequenceArray &sequences() const { return _sequences; }
          IndexedSequenceArray &sequences()       { return _sequences; }

    const IndexedSequence &sequence(int index) const { return _sequences[index]; }
          IndexedSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Methods
    //----------------------------------------

    IndexedTrack() { clear(); }

    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);
    void writeRouted(Routing::Target target, int intValue, float floatValue);

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;
    IndexedSequenceArray _sequences;

    friend class Track;
};
