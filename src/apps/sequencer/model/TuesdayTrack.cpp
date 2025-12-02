#include "TuesdayTrack.h"
#include "ProjectVersion.h"

void TuesdayTrack::gateOutputName(int index, StringBuilder &str) const {
    str("G%d", _trackIndex + 1);
}

void TuesdayTrack::cvOutputName(int index, StringBuilder &str) const {
    str("CV%d", _trackIndex + 1);
}

void TuesdayTrack::clear() {
    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void TuesdayTrack::write(VersionedSerializedWriter &writer) const {
    writeArray(writer, _sequences);
}

void TuesdayTrack::read(VersionedSerializedReader &reader) {
    if (reader.dataVersion() < ProjectVersion::Version50) {
        // Migration from track-level to sequence-level
        // Read old data into a temp sequence, then copy to all sequences
        TuesdaySequence tempSequence;
        
        reader.read(tempSequence._algorithm.base, ProjectVersion::Version35);
        reader.read(tempSequence._flow.base, ProjectVersion::Version35);
        reader.read(tempSequence._ornament.base, ProjectVersion::Version35);
        reader.read(tempSequence._power.base, ProjectVersion::Version35);
        reader.read(tempSequence._loopLength, ProjectVersion::Version35);
        reader.read(tempSequence._glide.base, ProjectVersion::Version35);
        reader.read(tempSequence._trill.base, ProjectVersion::Version41);
        reader.read(tempSequence._useScale, ProjectVersion::Version35);
        reader.read(tempSequence._skew, ProjectVersion::Version35);
        
        uint8_t cvUpdateMode;
        reader.read(cvUpdateMode, ProjectVersion::Version35);
        tempSequence._cvUpdateMode = cvUpdateMode;

        reader.read(tempSequence._octave.base, ProjectVersion::Version35);
        reader.read(tempSequence._transpose.base, ProjectVersion::Version35);
        
        // Divisor migration
        if (reader.dataVersion() < ProjectVersion::Version10) {
            uint8_t div;
            reader.read(div);
            tempSequence._divisor.base = div;
        } else {
            reader.read(tempSequence._divisor.base, ProjectVersion::Version35);
        }

        reader.read(tempSequence._resetMeasure, ProjectVersion::Version35);
        reader.read(tempSequence._scale, ProjectVersion::Version35);
        reader.read(tempSequence._rootNote, ProjectVersion::Version35);
        reader.read(tempSequence._scan.base, ProjectVersion::Version35);
        reader.read(tempSequence._rotate.base, ProjectVersion::Version35);
        reader.read(tempSequence._gateOffset.base, ProjectVersion::Version40);

        // Copy to all sequences
        for (auto &sequence : _sequences) {
            sequence = tempSequence;
        }
    } else {
        readArray(reader, _sequences);
    }
}
