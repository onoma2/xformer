#include "UnitTest.h"

#include "apps/sequencer/model/CurveTrack.h"
#include "utils/MemoryFile.h"
#include "model/ProjectVersion.h"

using namespace fs;

// This is a mock of the old CurveTrack::write method as it was for data version <= 41
void write_v41(const CurveTrack &track, VersionedSerializedWriter &writer, uint8_t phaseOffset) {
    writer.write(track.playMode());
    writer.write(track.fillMode());
    writer.write(track.muteMode());
    writer.write(track.slideTime());
    writer.write(track.offset());
    writer.write(track.rotate());
    writer.write(track.shapeProbabilityBias());
    writer.write(track.gateProbabilityBias());
    writer.write(phaseOffset);
    writeArray(writer, track.sequences());
}


UNIT_TEST("CurveTrackSerialization") {

CASE("globalPhase") {
    CurveTrack track;
    track.setGlobalPhase(0.75f);

    MemoryFile file;
    VersionedSerializedWriter writer(
        [&file](const void *data, size_t len) { file.write(data, len); },
        ProjectVersion::Latest
    );
    track.write(writer);
    writer.writeHash();

    file.rewind();

    CurveTrack loadedTrack;
    VersionedSerializedReader reader(
        [&file](void *data, size_t len) { file.read(data, len); },
        ProjectVersion::Latest
    );
    loadedTrack.read(reader);

    expectEqual(loadedTrack.globalPhase(), 0.75f, "globalPhase should persist");
}

CASE("migration from phaseOffset") {
    CurveTrack track; // A track with default values for other properties

    MemoryFile file;
    VersionedSerializedWriter writer(
        [&file](const void *data, size_t len) { file.write(data, len); },
        ProjectVersion::Version41
    );
    write_v41(track, writer, 75); // write phaseOffset of 75
    writer.writeHash();

    file.rewind();

    CurveTrack loadedTrack;
    VersionedSerializedReader reader(
        [&file](void *data, size_t len) { file.read(data, len); },
        ProjectVersion::Latest
    );
    loadedTrack.read(reader);

    // The old value was an int 0-100, new one is float 0.0-1.0
    // 75 should become 0.75
    expectEqual(loadedTrack.globalPhase(), 0.75f, "globalPhase should be migrated from old phaseOffset");
}


} // UNIT_TEST