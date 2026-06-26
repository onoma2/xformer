#include "UnitTest.h"

#include "apps/sequencer/model/Model.h"
#include "engine/TT2ScriptLoader.h"
#include "utils/MemoryFile.h"
#include "model/ProjectVersion.h"

using namespace fs;

// Round-trips a Mini Track through Track::write/read (mode dispatch), not the
// bare TT2MiniTrack model. Marker per scene: script0 holds N "M k" lines so
// scripts[0].length is distinct (1..4) and survives the program blob write.

UNIT_TEST("TT2MiniSerialize") {

CASE("track mode + 4 distinct scenes survive Track-level round-trip") {
    Model model;
    model.project().setTrackMode(1, Track::TrackMode::TeletypeMini);
    auto &src = model.project().track(1);

    static const char *scripts[4] = {
        "M 10\n",
        "M 20\nM 21\n",
        "M 30\nM 31\nM 32\n",
        "M 40\nM 41\nM 42\nM 43\n",
    };
    for (int n = 0; n < 4; ++n) {
        expectTrue(loadScriptText(src.tt2MiniTrack().program(n), 0, scripts[n]) >= 0, "script loads");
    }

    MemoryFile file;
    VersionedSerializedWriter writer(
        [&file](const void *data, size_t len) { file.write(data, len); },
        ProjectVersion::Latest
    );
    src.write(writer);
    writer.writeHash();

    file.rewind();

    auto &dst = model.project().track(2);
    VersionedSerializedReader reader(
        [&file](void *data, size_t len) { file.read(data, len); },
        ProjectVersion::Latest
    );
    dst.read(reader);

    expectTrue(dst.trackMode() == Track::TrackMode::TeletypeMini,
        "mode reads back as TeletypeMini (not Note=0)");

    for (int n = 0; n < 4; ++n) {
        expectEqual(int(dst.tt2MiniTrack().program(n).scripts[0].length), n + 1,
            "scene preserves its own script length");
    }
    // Sanity: the four are distinct, so the loop is not aliasing one scene.
    expectTrue(dst.tt2MiniTrack().program(0).scripts[0].length
            != dst.tt2MiniTrack().program(3).scripts[0].length,
        "scenes 0 and 3 differ");
}

} // UNIT_TEST
