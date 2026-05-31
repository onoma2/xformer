#include "UnitTest.h"

#include "apps/sequencer/model/Model.h"
#include "utils/MemoryFile.h"
#include "model/ProjectVersion.h"

using namespace fs;

// Linking removal keeps the _linkTrack byte as a written-zero / read-discarded
// reserved placeholder. The project format is positional, so any field written
// after the link byte must still deserialize correctly. These cases pin that
// alignment: drop the byte from read or write and the post-link field shifts.

UNIT_TEST("LinkingRemovalSerialization") {

CASE("note field after link byte survives round-trip") {
    Model model;
    auto &src = model.project().track(1);
    src.noteTrack().sequence(0).step(3).setNote(12);
    int expected = src.noteTrack().sequence(0).step(3).note();

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

    expectEqual(dst.noteTrack().sequence(0).step(3).note(), expected,
        "note written after the link byte must survive (stream stays aligned)");
}

CASE("curve field after link byte survives round-trip") {
    Model model;
    model.project().setTrackMode(1, Track::TrackMode::Curve);
    auto &src = model.project().track(1);
    src.curveTrack().setOffset(3);
    int expected = src.curveTrack().offset();

    MemoryFile file;
    VersionedSerializedWriter writer(
        [&file](const void *data, size_t len) { file.write(data, len); },
        ProjectVersion::Latest
    );
    src.write(writer);
    writer.writeHash();

    file.rewind();

    model.project().setTrackMode(2, Track::TrackMode::Curve);
    auto &dst = model.project().track(2);
    VersionedSerializedReader reader(
        [&file](void *data, size_t len) { file.read(data, len); },
        ProjectVersion::Latest
    );
    dst.read(reader);

    expectEqual(dst.curveTrack().offset(), expected,
        "curve offset written after the link byte must survive (stream stays aligned)");
}

} // UNIT_TEST
