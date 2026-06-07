#include "NoteTrack.h"
#include "ProjectVersion.h"

void NoteTrack::clear() {
    setPlayMode(Types::PlayMode::Free);
    setFillMode(FillMode::Gates);
    setFillMuted(true);
    setCvUpdateMode(CvUpdateMode::Gate);
    setSlideTime(50);
    setOctave(0);
    setTranspose(0);
    setRotate(0);
    setGateProbabilityBias(0);
    setRetriggerProbabilityBias(0);
    setLengthBias(0);
    setNoteProbabilityBias(0);

    for (auto &sequence : _sequences) {
        sequence.clear();
    }
}

void NoteTrack::write(VersionedSerializedWriter &writer) const {
    writer.write(_playMode);
    writer.write(_fillMode);
    writer.write(_fillMuted);
    writer.write(_cvUpdateMode);
    writer.write(_slideTime);
    writer.write(_octave);
    writer.write(_transpose);
    writer.write(_rotate);
    writer.write(_gateProbabilityBias);
    writer.write(_retriggerProbabilityBias);
    writer.write(_lengthBias);
    writer.write(_noteProbabilityBias);
    writeArray(writer, _sequences);
}

void NoteTrack::read(VersionedSerializedReader &reader) {
    reader.backupHash();

    reader.read(_playMode);
    reader.read(_fillMode);
    reader.read(_fillMuted, ProjectVersion::Version26);
    reader.read(_cvUpdateMode, ProjectVersion::Version4);
    reader.read(_slideTime);
    reader.read(_octave);
    reader.read(_transpose);
    reader.read(_rotate);
    reader.read(_gateProbabilityBias);
    reader.read(_retriggerProbabilityBias);
    reader.read(_lengthBias);
    reader.read(_noteProbabilityBias);

    // There is a bug in previous firmware versions where writing the properties
    // of a note track did not update the hash value.
    if (reader.dataVersion() < ProjectVersion::Version23) {
        reader.restoreHash();
    }

    readArray(reader, _sequences);
}
