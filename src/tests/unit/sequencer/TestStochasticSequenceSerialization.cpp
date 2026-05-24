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

    expectEqual(restored.gateLength(), seq.gateLength(), "gate length default");
    for (int i = 0; i < 8; ++i) {
        expectEqual(restored.durationTicket(i), seq.durationTicket(i), "duration ticket default");
    }
    expectEqual(static_cast<int>(restored.marblesMode()), static_cast<int>(seq.marblesMode()), "marblesMode default");
}

CASE("gate_length_persists") {
    StochasticSequence seq;
    seq.clear();
    seq.setGateLength(33);

    uint8_t buf[2048];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    StochasticSequence restored;
    restored.clear();
    readSequence(restored, buf, sizeof(buf));

    expectEqual(restored.gateLength(), 33, "gate length should be 33 after round trip");
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

CASE("content_snapshot_roundtrip") {
    // Tier 4 snapshot: capture/restore preserves all content fields except
    // _steps[], which are intentionally NOT touched (caller regenerates from
    // seeds via generateRhythm / generateMelody).
    StochasticSequence src;
    src.clear();
    src.setRhythmSeed(0xCAFEF00Du);
    src.setMelodySeed(0xDEADBEEFu);
    src.setMaskMelody(73);
    src.setTiltMelody(42);
    src.setMaskRhythm(88);
    src.setTiltRhythm(55);
    src.setComplexity(33);
    src.setContour(-44);
    src.setBurst(77);
    src.setBurstCount(60);
    src.setBurstRate(80);
    src.setRange(35);
    src.setRest(22);
    src.setSize(24);
    src.setFirst(5);
    src.setDegreeTicket(0, 90);
    src.setDegreeTicket(5, -1);
    src.setDegreeTicket(7, 33);
    src.setDurationTicket(2, 50);
    src.setDurationTicket(4, -1);
    src.setRhythmMode(StochasticSourceMode::Loop);
    src.setBurstHold(StochasticBurstHold::RollFit);
    // Touch a stored step so we can verify steps[] is preserved (not wiped
    // by the snapshot restore).
    src.steps()[3].setDegree(11);
    src.steps()[3].setOctave(2);
    src.steps()[3].setMelodyValid(true);

    StochasticSequence::ContentSnapshot snap;
    src.captureContentTo(snap);

    StochasticSequence dst;
    dst.clear();
    // Pre-populate dst's steps[3] with different content — the restore must
    // leave it alone (Tier 4 contract: events come from a separate regen pass).
    dst.steps()[3].setDegree(99);
    dst.steps()[3].setOctave(0);
    dst.restoreContentFrom(snap);

    expectEqual(int(dst.rhythmSeed()), int(0xCAFEF00Du));
    expectEqual(int(dst.melodySeed()), int(0xDEADBEEFu));
    expectEqual(dst.maskMelody(), 73);
    expectEqual(dst.tiltMelody(), 42);
    expectEqual(dst.maskRhythm(), 88);
    expectEqual(dst.tiltRhythm(), 55);
    expectEqual(dst.complexity(), 33);
    expectEqual(dst.contour(), -44);
    expectEqual(dst.burst(), 77);
    expectEqual(int(dst.burstCount()), 60);
    expectEqual(int(dst.burstRate()), 80);
    expectEqual(dst.range(), 35);
    expectEqual(dst.rest(), 22);
    expectEqual(int(dst.size()), 24);
    expectEqual(int(dst.first()), 5);
    expectEqual(dst.degreeTicket(0), 90);
    expectEqual(dst.degreeTicket(5), -1);
    expectEqual(dst.degreeTicket(7), 33);
    expectEqual(dst.durationTicket(2), 50);
    expectEqual(dst.durationTicket(4), -1);
    expectTrue(dst.rhythmMode() == StochasticSourceMode::Loop, "rhythmMode roundtrip");
    expectTrue(dst.burstHold() == StochasticBurstHold::RollFit, "burstHold roundtrip");
    // steps[3] is the dst original (99/0) — proves snapshot did not overwrite events.
    expectEqual(int(dst.steps()[3].degree()), 99, "snapshot must not overwrite stored events");
}

} // UNIT_TEST("StochasticSequenceSerialization")
