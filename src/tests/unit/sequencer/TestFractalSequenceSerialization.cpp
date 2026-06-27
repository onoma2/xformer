#include "UnitTest.h"

#include "apps/sequencer/model/FractalSequence.h"
#include "apps/sequencer/model/FractalTrack.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"
#include "model/ProjectVersion.h"

#include <cstring>

static void writeSequence(const FractalSequence &seq, uint8_t *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    seq.write(writer);
}

static void readSequence(FractalSequence &seq, const uint8_t *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);
    seq.read(reader);
}

static void writeTrack(const FractalTrack &track, uint8_t *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    track.write(writer);
}

static void readTrack(FractalTrack &track, const uint8_t *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);
    track.read(reader);
}

UNIT_TEST("FractalSequenceSerialization") {

CASE("all_fields_round_trip") {
    FractalSequence seq;
    seq.clear();

    // chassis
    seq.setScale(3);
    seq.setRootNote(5);
    seq.setScaleRotate(7);
    seq.setDivisor(24);
    seq.setClockMultiplier(133);
    seq.setResetMeasure(8);
    seq.setLoopFirst(2);
    seq.setLoopLast(9);
    seq.setRotate(-13);
    seq.setRecordSkip(3);
    seq.setRecordFirst(1);
    seq.setRecordLast(6);
    seq.setOrnFirst(2);
    seq.setOrnLast(5);
    seq.setRecordTrigger(1);
    seq.setBranchCount(4);
    seq.setPath(170);
    seq.setBranchSeed(40000);
    seq.setBranchPool(0x55);
    seq.setOrnamentRate(60);
    seq.setOrnamentIntensity(40);

    // typed modes — non-default values
    seq.setOrderMode(FractalSequence::OrderMode::Page);
    seq.setLoopMode(FractalSequence::LoopMode::Once);
    seq.setRecordMode(FractalSequence::RecordMode::Latch);
    seq.setCaptureCadence(FractalSequence::CaptureCadence::Event);
    seq.setCaptureFidelity(FractalSequence::CaptureFidelity::Feel);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    FractalSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    expectEqual(r.scale(), 3, "scale persists");
    expectEqual(r.rootNote(), 5, "rootNote persists");
    expectEqual(r.scaleRotate(), 7, "scaleRotate persists");
    expectEqual(r.divisor(), 24, "divisor persists");
    expectEqual(r.clockMultiplier(), 133, "clockMultiplier persists");
    expectEqual(r.resetMeasure(), 8, "resetMeasure persists");
    expectEqual(r.loopFirst(), 2, "loopFirst persists");
    expectEqual(r.loopLast(), 9, "loopLast persists");
    expectEqual(r.rotate(), -13, "rotate persists");
    expectEqual(r.recordSkip(), 3, "recordSkip persists");
    expectEqual(r.recordFirst(), 1, "recordFirst persists");
    expectEqual(r.recordLast(), 6, "recordLast persists");
    expectEqual(r.ornFirst(), 2, "ornFirst persists");
    expectEqual(r.ornLast(), 5, "ornLast persists");
    expectEqual(r.recordTrigger(), 1, "recordTrigger persists");
    expectEqual(r.branchCount(), 4, "branchCount persists");
    expectEqual(r.path(), 170, "path persists");
    expectEqual(r.branchSeed(), 40000, "branchSeed persists");
    expectEqual(r.branchPool(), 0x55, "branchPool persists");
    expectEqual(r.ornamentRate(), 60, "ornamentRate persists");
    expectEqual(r.ornamentIntensity(), 40, "ornamentIntensity persists");

    expectTrue(r.orderMode() == FractalSequence::OrderMode::Page, "orderMode persists");
    expectTrue(r.loopMode() == FractalSequence::LoopMode::Once, "loopMode persists");
    expectTrue(r.recordMode() == FractalSequence::RecordMode::Latch, "recordMode persists");
    expectTrue(r.captureCadence() == FractalSequence::CaptureCadence::Event, "captureCadence persists");
    expectTrue(r.captureFidelity() == FractalSequence::CaptureFidelity::Feel, "captureFidelity persists");
}

CASE("typed_modes_byte_identical") {
    // Prove the enum typing kept the underlying serialized byte unchanged:
    // a sequence with every typed mode at its highest valid value must produce
    // exactly the same bytes whether the raw int or the enum is set.
    FractalSequence seq;
    seq.clear();
    seq.setOrderMode(FractalSequence::OrderMode::Page);    // 6
    seq.setLoopMode(FractalSequence::LoopMode::Once);      // 1
    seq.setRecordMode(FractalSequence::RecordMode::Latch); // 1
    seq.setCaptureCadence(FractalSequence::CaptureCadence::Event);     // 1
    seq.setCaptureFidelity(FractalSequence::CaptureFidelity::Feel);    // 1

    expectEqual(int(seq.orderMode()), 6, "orderMode underlying byte == 6 (Page)");
    expectEqual(int(seq.loopMode()), 1, "loopMode underlying byte == 1 (Once)");
    expectEqual(int(seq.recordMode()), 1, "recordMode underlying byte == 1 (Latch)");
    expectEqual(int(seq.captureCadence()), 1, "captureCadence underlying byte == 1 (Event)");
    expectEqual(int(seq.captureFidelity()), 1, "captureFidelity underlying byte == 1 (Feel)");
}

CASE("clamped_enum_rejects_out_of_range") {
    FractalSequence seq;
    seq.clear();

    // adjustedEnum / clampedEnum must pin at the last valid member (Last - 1).
    seq.setOrderMode(FractalSequence::OrderMode(99));
    expectEqual(int(seq.orderMode()), int(FractalSequence::OrderMode::Last) - 1, "orderMode clamps to last valid");

    seq.setLoopMode(FractalSequence::LoopMode(99));
    expectEqual(int(seq.loopMode()), int(FractalSequence::LoopMode::Last) - 1, "loopMode clamps to last valid");

    seq.setRecordMode(FractalSequence::RecordMode(99));
    expectEqual(int(seq.recordMode()), int(FractalSequence::RecordMode::Last) - 1, "recordMode clamps to last valid");

    seq.setCaptureCadence(FractalSequence::CaptureCadence(99));
    expectEqual(int(seq.captureCadence()), int(FractalSequence::CaptureCadence::Last) - 1, "captureCadence clamps to last valid");

    seq.setCaptureFidelity(FractalSequence::CaptureFidelity(99));
    expectEqual(int(seq.captureFidelity()), int(FractalSequence::CaptureFidelity::Last) - 1, "captureFidelity clamps to last valid");
}

CASE("track_record_muted_round_trip") {
    FractalTrack track;
    track.clear();
    expectTrue(!track.recordMuted(), "recordMuted defaults false");

    track.setRecordMuted(true);

    uint8_t buf[8192];
    std::memset(buf, 0, sizeof(buf));
    writeTrack(track, buf, sizeof(buf));

    FractalTrack r;
    r.clear();
    readTrack(r, buf, sizeof(buf));

    expectTrue(r.recordMuted(), "recordMuted persists");
}

CASE("track_quantize_round_trip") {
    FractalTrack track;
    track.clear();
    expectEqual(track.quantize(), -1, "quantize defaults to raw (-1)");

    track.setQuantize(5);

    uint8_t buf[8192];
    std::memset(buf, 0, sizeof(buf));
    writeTrack(track, buf, sizeof(buf));

    FractalTrack r;
    r.clear();
    readTrack(r, buf, sizeof(buf));

    expectEqual(r.quantize(), 5, "quantize persists");
}

CASE("track_channel_source_round_trip") {
    // Extended source slots store track indices (current) and single channels
    // (above the track range). A channel value must round-trip and classify.
    FractalTrack track;
    track.clear();

    int chanA = FractalTrack::sourceMax();   // last channel (Mod8)
    track.setSourceA(chanA);
    track.setSourceB(3);                      // still a parent track

    uint8_t buf[8192];
    std::memset(buf, 0, sizeof(buf));
    writeTrack(track, buf, sizeof(buf));

    FractalTrack r;
    r.clear();
    readTrack(r, buf, sizeof(buf));

    expectEqual(r.sourceA(), chanA, "channel source A persists");
    expectEqual(r.sourceB(), 3, "track source B persists");
    expectTrue(FractalTrack::sourceKind(r.sourceA()) == FractalTrack::SourceKind::Channel,
               "source A classifies as channel");
    expectTrue(FractalTrack::sourceKind(r.sourceB()) == FractalTrack::SourceKind::Track,
               "source B classifies as track");
    expectTrue(FractalTrack::sourceChannelOf(r.sourceA()) == Routing::Source::Mod8,
               "last channel maps to Mod8");
}

} // UNIT_TEST("FractalSequenceSerialization")
