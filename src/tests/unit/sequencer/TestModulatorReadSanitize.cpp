#include "UnitTest.h"

#include "apps/sequencer/model/Modulator.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"
#include "model/ProjectVersion.h"

// A corrupt/old project file can carry _rate == 0, which the Tempo-domain engine
// path uses as a divisor (65536 / rate). read() reads fields raw, bypassing
// setRate's domain clamp, so read() must sanitize per domain after deserialization.

static void writeModulator(const Modulator &m, uint8_t *buf, size_t len) {
    MemoryWriter mw(buf, len);
    VersionedSerializedWriter writer([&mw](const void *d, size_t s) { mw.write(d, s); }, ProjectVersion::Latest);
    m.write(writer);
}

static void readModulator(Modulator &m, const uint8_t *buf, size_t len) {
    MemoryReader mr(buf, len);
    VersionedSerializedReader reader([&mr](void *d, size_t s) { mr.read(d, s); }, ProjectVersion::Latest);
    m.read(reader);
}

UNIT_TEST("ModulatorReadSanitize") {

CASE("zero_rate_is_clamped_on_read") {
    Modulator src;
    src.setRateDomain(Modulator::RateDomain::Tempo);
    src.setRate(6143); // distinctive 0x17FF; low byte 0xFF, high 0x17
    uint8_t buf[64] = {};
    writeModulator(src, buf, sizeof(buf));

    // Corrupt the rate field in the buffer to 0 (simulating bad file data).
    bool corrupted = false;
    for (size_t i = 0; i + 1 < sizeof(buf); ++i) {
        if (buf[i] == 0xFF && buf[i + 1] == 0x17) {
            buf[i] = 0;
            buf[i + 1] = 0;
            corrupted = true;
            break;
        }
    }
    expectTrue(corrupted, "located rate bytes to corrupt");

    Modulator dst;
    readModulator(dst, buf, sizeof(buf));
    expectTrue(dst.rate() >= 6, "rate sanitized to valid minimum after read");
}

}
