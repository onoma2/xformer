#include "UnitTest.h"

#include "apps/sequencer/model/CurveTrack.h"
#include "utils/MemoryFile.h"
#include "model/ProjectVersion.h"

using namespace fs;

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

} // UNIT_TEST