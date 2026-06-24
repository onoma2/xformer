#include "UnitTest.h"

#include "apps/sequencer/model/MidiOutput.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"
#include "model/ProjectVersion.h"

static void writeOutput(const MidiOutput::Output &out, uint8_t *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    out.write(writer);
}

static void readOutput(MidiOutput::Output &out, const uint8_t *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);
    out.read(reader);
}

UNIT_TEST("MidiOutput") {

CASE("setEvent(Note) defaults: microtune off, bendRange 2 (no /0)") {
    MidiOutput::Output o;
    o.setEvent(MidiOutput::Output::Event::Note, true);
    expectTrue(!o.microtune(), "microtune off");
    expectEqual(int(o.bendRange()), 2, "bendRange 2");
}

CASE("microtune + bendRange round-trip via Output::write/read") {
    MidiOutput::Output a;
    a.setEvent(MidiOutput::Output::Event::Note, true);
    a.setMicrotune(true);
    a.setBendRange(12);

    uint8_t buf[256];
    writeOutput(a, buf, sizeof(buf));

    MidiOutput::Output b;
    readOutput(b, buf, sizeof(buf));

    expectTrue(b.microtune(), "microtune survives");
    expectEqual(int(b.bendRange()), 12, "bendRange survives");
}

}
