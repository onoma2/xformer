#include "UnitTest.h"

#include "apps/sequencer/model/GeodeConfig.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"

#include <cstring>

UNIT_TEST("GeodeConfig") {

CASE("clear() defaults") {
    GeodeConfig g; g.clear();
    expectEqual(int(g.active()), 0, "active off");
    expectEqual(g.mode(), 0, "mode 0");
    for (int i = 0; i < GeodeConfig::VoiceCount; ++i) {
        expectEqual(g.divs(i), 1, "divs default 1");
        expectEqual(g.repeats(i), 0, "repeats default 0");
        expectEqual(g.tuneNumerator(i), i + 1, "tune num = i+1");
        expectEqual(g.tuneDenominator(i), 1, "tune den = 1");
    }
}

CASE("ranged setters clamp") {
    GeodeConfig g; g.clear();
    g.setDivs(0, 100); expectEqual(g.divs(0), 64, "divs clamps to 64");
    g.setDivs(0, 0);   expectEqual(g.divs(0), 1, "divs clamps to 1");
    g.setRepeats(0, 999); expectEqual(g.repeats(0), 255, "repeats clamps to 255");
    g.setRepeats(0, -9);  expectEqual(g.repeats(0), -1, "repeats clamps to -1");
    g.setMode(9); expectEqual(g.mode(), 2, "mode clamps to 2");
}

CASE("serialize round-trip preserves all fields incl dormant tune") {
    uint8_t buf[512];
    std::memset(buf, 0, sizeof(buf));

    GeodeConfig g; g.clear();
    g.setActive(true);
    g.setTime(1000); g.setIntone(2000); g.setRamp(3000); g.setCurve(4000);
    g.setMode(2);
    g.setDivs(2, 8); g.setRepeats(2, 4);
    g.setTune(3, 5, 4);

    MemoryWriter memoryWriter(buf, sizeof(buf));
    VersionedSerializedWriter writer([&memoryWriter] (const void *data, size_t len) {
        memoryWriter.write(data, len);
    }, 33);
    g.write(writer);

    MemoryReader memoryReader(buf, sizeof(buf));
    VersionedSerializedReader reader([&memoryReader] (void *data, size_t len) {
        memoryReader.read(data, len);
    }, 33);
    GeodeConfig r;
    r.read(reader);

    expectEqual(int(r.active()), 1, "active");
    expectEqual(r.time(), 1000, "time");
    expectEqual(r.intone(), 2000, "intone");
    expectEqual(r.ramp(), 3000, "ramp");
    expectEqual(r.curve(), 4000, "curve");
    expectEqual(r.mode(), 2, "mode");
    expectEqual(r.divs(2), 8, "divs[2]");
    expectEqual(r.repeats(2), 4, "repeats[2]");
    expectEqual(r.tuneNumerator(3), 5, "tune num[3]");
    expectEqual(r.tuneDenominator(3), 4, "tune den[3]");
    // untouched voices keep their defaults
    expectEqual(r.divs(0), 1, "divs[0] default preserved");
    expectEqual(r.tuneNumerator(5), 6, "tune num[5] default preserved");
}

}
