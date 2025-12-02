#pragma once

#include "Config.h"
#include "Types.h"
#include "Serialize.h"
#include "TuesdaySequence.h"

class TuesdayTrack {
public:
    //----------------------------------------
    // Types
    //----------------------------------------

    using TuesdaySequenceArray = std::array<TuesdaySequence, CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;

    //----------------------------------------
    // Properties
    //----------------------------------------

    // sequences

    const TuesdaySequenceArray &sequences() const { return _sequences; }
          TuesdaySequenceArray &sequences()       { return _sequences; }

    const TuesdaySequence &sequence(int index) const { return _sequences[index]; }
          TuesdaySequence &sequence(int index)       { return _sequences[index]; }

    //----------------------------------------
    // Methods
    //----------------------------------------

    TuesdayTrack() { clear(); }

    void clear();

    void gateOutputName(int index, StringBuilder &str) const;
    void cvOutputName(int index, StringBuilder &str) const;

    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);

private:
    void setTrackIndex(int trackIndex) {
        _trackIndex = trackIndex;
        for (auto &sequence : _sequences) {
            sequence.setTrackIndex(trackIndex);
        }
    }

    int8_t _trackIndex = -1;
    TuesdaySequenceArray _sequences;

    friend class Track;
};
