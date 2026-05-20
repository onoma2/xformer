#include "UnitTest.h"

#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/StochasticTrack.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"
#include "model/ProjectVersion.h"

static void writeSequence(const StochasticSequence &seq, uint8_t *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    seq.write(writer);
}

static void readSequence(StochasticSequence &seq, const uint8_t *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);
    seq.read(reader);
}

UNIT_TEST("StochasticSequenceSerialization") {

CASE("default_round_trip") {
    StochasticSequence seq;
    seq.clear();

    uint8_t buf[2048];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    StochasticSequence restored;
    restored.clear();
    readSequence(restored, buf, sizeof(buf));

    expectEqual(restored.density(), seq.density(), "density default");
    expectEqual(restored.level() == StochasticLevel::Core, true, "level default Core");
    for (int i = 0; i < 8; ++i) {
        expectEqual(restored.durationTicket(i), seq.durationTicket(i), "duration ticket default");
    }
    expectEqual(static_cast<int>(restored.marblesMode()), static_cast<int>(seq.marblesMode()), "marblesMode default");
}

CASE("density_persists") {
    StochasticSequence seq;
    seq.clear();
    seq.setDensity(33);

    uint8_t buf[2048];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    StochasticSequence restored;
    restored.clear();
    readSequence(restored, buf, sizeof(buf));

    expectEqual(restored.density(), 33, "density should be 33 after round trip");
}

CASE("duration_tickets_persist") {
    StochasticSequence seq;
    seq.clear();
    seq.setDurationTicket(0, 50);
    seq.setDurationTicket(1, 0);
    seq.setDurationTicket(2, 30);
    seq.setDurationTicket(3, 0);
    seq.setDurationTicket(4, 0);
    seq.setDurationTicket(5, 20);
    seq.setDurationTicket(6, 0);
    seq.setDurationTicket(7, 0);

    uint8_t buf[2048];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    StochasticSequence restored;
    restored.clear();
    readSequence(restored, buf, sizeof(buf));

    expectEqual(restored.durationTicket(0), 50, "duration ticket[0]");
    expectEqual(restored.durationTicket(2), 30, "duration ticket[2]");
    expectEqual(restored.durationTicket(5), 20, "duration ticket[5]");
    expectEqual(restored.durationTicket(1), 0, "duration ticket[1] zero");
}

CASE("level_persists") {
    StochasticSequence seq;
    seq.clear();
    seq.setLevel(StochasticLevel::Direct);

    uint8_t buf[2048];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    StochasticSequence restored;
    restored.clear();
    readSequence(restored, buf, sizeof(buf));

    expectEqual(static_cast<int>(restored.level()), static_cast<int>(StochasticLevel::Direct), "level should be Direct after round trip");
}

CASE("lock_does_not_persist") {
    StochasticTrack track;
    track.clear();
    track.setLock(true);
    expectEqual(track.lock(), true, "lock should be true before write");

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));

    MemoryWriter memoryWriter(buf, sizeof(buf));
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    track.write(writer);

    MemoryReader memoryReader(buf, sizeof(buf));
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);

    StochasticTrack restored;
    restored.clear();
    restored.read(reader);

    expectEqual(restored.lock(), false, "lock should NOT persist through serialization");
}

} // UNIT_TEST("StochasticSequenceSerialization")
