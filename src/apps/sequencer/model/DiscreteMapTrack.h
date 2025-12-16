#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "DiscreteMapSequence.h"

#include <array>

class DiscreteMapTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using DiscreteMapSequenceArray = std::array<DiscreteMapSequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    //----------------------------------------
    // Properties
    //----------------------------------------

    // sequences

    const DiscreteMapSequenceArray &sequences() const { return _sequences; }
          DiscreteMapSequenceArray &sequences()       { return _sequences; }

    const DiscreteMapSequence &sequence(int index) const { return _sequences[index]; }
          DiscreteMapSequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Methods
    //----------------------------------------

    DiscreteMapTrack() { clear(); }

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
    DiscreteMapSequenceArray _sequences;

    friend class Track;
};
