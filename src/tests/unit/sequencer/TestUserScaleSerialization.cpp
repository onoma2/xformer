#include "UnitTest.h"

#include "apps/sequencer/model/UserScale.h"
#include "apps/sequencer/Config.h"
#include "utils/MemoryFile.h"
#include "model/ProjectVersion.h"

using namespace fs;

// A corrupt/oversized size byte in a UserScale file must be rejected on read,
// not looped over — otherwise read() writes past _items (CONFIG_USER_SCALE_SIZE).
UNIT_TEST("UserScaleSerialization") {

CASE("read rejects an oversized size byte without overrunning items") {
    // Craft a buffer with size = 40 (> CONFIG_USER_SCALE_SIZE = 32), bypassing
    // write()'s clamp, with a matching hash so only the size guard can reject it.
    MemoryFile file;
    VersionedSerializedWriter writer(
        [&file](const void *data, size_t len) { file.write(data, len); },
        ProjectVersion::Latest);

    char name[UserScale::NameLength + 1] = "evil";
    writer.write(name, UserScale::NameLength + 1);
    UserScale::Mode mode = UserScale::Mode::Chromatic;
    writer.write(mode);
    uint8_t badSize = 40;
    writer.write(badSize);
    for (int i = 0; i < badSize; ++i) {
        int16_t v = 0;
        writer.write(v);
    }
    writer.writeHash();
    file.rewind();

    UserScale scale;
    VersionedSerializedReader reader(
        [&file](void *data, size_t len) { file.read(data, len); },
        ProjectVersion::Latest);
    bool ok = scale.read(reader);

    expect(!ok, "oversized size byte rejected");
    expect(scale.size() <= CONFIG_USER_SCALE_SIZE, "size never exceeds the items array");
}

CASE("valid round-trip survives") {
    UserScale scale;
    scale.setMode(UserScale::Mode::Chromatic);
    scale.setName("mine");
    scale.setSize(12);
    for (int i = 0; i < 12; ++i) {
        scale.setItem(i, i);
    }

    MemoryFile file;
    VersionedSerializedWriter writer(
        [&file](const void *data, size_t len) { file.write(data, len); },
        ProjectVersion::Latest);
    scale.write(writer);
    file.rewind();

    UserScale loaded;
    VersionedSerializedReader reader(
        [&file](void *data, size_t len) { file.read(data, len); },
        ProjectVersion::Latest);
    bool ok = loaded.read(reader);

    expect(ok, "valid scale reads back");
    expectEqual(loaded.size(), 12, "size preserved");
    for (int i = 0; i < 12; ++i) {
        expectEqual(loaded.item(i), i, "item preserved");
    }
}

}
