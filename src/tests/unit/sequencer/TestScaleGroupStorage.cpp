#include "UnitTest.h"

#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Track.h"
#include "apps/sequencer/model/NoteSequence.h"
#include "apps/sequencer/model/StochasticSequence.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"
#include "apps/sequencer/model/DiscreteMapSequence.h"
#include "apps/sequencer/model/IndexedSequence.h"
#include "apps/sequencer/model/TuesdaySequence.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"
#include "model/ProjectVersion.h"

#include <cstring>

// Step-0 baseline (measured before the bias-pack), asserted with == so any
// alignment growth from the uint16 scale group HALTS the build.
static constexpr int kSizeProject             = 78264;
static constexpr int kSizeNoteSequence        = 556;
static constexpr int kSizeStochasticSequence  = 476;
static constexpr int kSizePhaseFluxSequence   = 288;
static constexpr int kSizeDiscreteMapSequence = 128;
static constexpr int kSizeIndexedSequence     = 436;
static constexpr int kSizeTuesdaySequence     = 26;
static constexpr int kSizeTrack               = 9544;

template <typename Seq>
static void roundTripSequenceWithSentinel(const char *name) {
    Seq seq;
    seq.clear();

    // scaleRotate round-trip incl. the -1 inherit sentinel.
    seq.setScaleRotate(-1);
    expectEqual(seq.scaleRotate(), -1, name);
    seq.setScaleRotate(0);
    expectEqual(seq.scaleRotate(), 0, name);
    seq.setScaleRotate(31);
    expectEqual(seq.scaleRotate(), 31, name);
    seq.setScaleRotate(32);  // clamp to 31 (fixed range, NOT notesPerOctave)
    expectEqual(seq.scaleRotate(), 31, name);
    seq.setScaleRotate(-2);  // clamp to -1
    expectEqual(seq.scaleRotate(), -1, name);

    // scale / rootNote still round-trip, packing does not bleed between fields.
    seq.setScale(-1);
    seq.setRootNote(7);
    seq.setScaleRotate(5);
    expectEqual(seq.scale(), -1, name);
    expectEqual(seq.rootNote(), 7, name);
    expectEqual(seq.scaleRotate(), 5, name);

    seq.setScale(12);
    expectEqual(seq.scale(), 12, name);
    expectEqual(seq.rootNote(), 7, name);     // unchanged by scale write
    expectEqual(seq.scaleRotate(), 5, name);  // unchanged
}

template <typename T>
static void writeSerializable(const T &obj, uint8_t *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    obj.write(writer);
}

template <typename T>
static void readSerializable(T &obj, const uint8_t *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);
    obj.read(reader);
}

// Serialize -> deserialize a sequence owner, asserting scale/rootNote/scaleRotate
// all survive the wire round-trip.
template <typename Seq>
static void serializeRoundTripSequence(const char *name, int scaleRot) {
    Seq seq;
    seq.clear();
    seq.setScale(3);
    seq.setRootNote(7);
    seq.setScaleRotate(scaleRot);

    uint8_t buf[65536];
    std::memset(buf, 0, sizeof(buf));
    writeSerializable(seq, buf, sizeof(buf));

    Seq restored;
    restored.clear();
    readSerializable(restored, buf, sizeof(buf));

    expectEqual(restored.scale(), 3, name);
    expectEqual(restored.rootNote(), 7, name);
    expectEqual(restored.scaleRotate(), scaleRot, name);
}

UNIT_TEST("ScaleGroupStorage") {

CASE("note_round_trip") {
    roundTripSequenceWithSentinel<NoteSequence>("NoteSequence");
}

CASE("stochastic_round_trip") {
    roundTripSequenceWithSentinel<StochasticSequence>("StochasticSequence");
}

CASE("phaseflux_round_trip") {
    roundTripSequenceWithSentinel<PhaseFluxSequence>("PhaseFluxSequence");
}

CASE("indexed_round_trip") {
    roundTripSequenceWithSentinel<IndexedSequence>("IndexedSequence");
}

CASE("tuesday_round_trip") {
    roundTripSequenceWithSentinel<TuesdaySequence>("TuesdaySequence");
}

CASE("discretemap_round_trip") {
    // DiscreteMap clamps rootNote to 0..11 (no -1), but scale + scaleRotate
    // carry the -1 sentinel.
    DiscreteMapSequence seq;
    seq.clear();
    seq.setScaleRotate(-1);
    expectEqual(seq.scaleRotate(), -1, "DiscreteMap scaleRotate -1");
    seq.setScaleRotate(31);
    expectEqual(seq.scaleRotate(), 31, "DiscreteMap scaleRotate 31");
    seq.setScaleRotate(32);
    expectEqual(seq.scaleRotate(), 31, "DiscreteMap scaleRotate clamp 31");
    seq.setScale(-1);
    seq.setRootNote(9);
    seq.setScaleRotate(4);
    expectEqual(seq.scale(), -1, "DiscreteMap scale -1");
    expectEqual(seq.rootNote(), 9, "DiscreteMap rootNote 9");
    expectEqual(seq.scaleRotate(), 4, "DiscreteMap scaleRotate 4");
}

CASE("project_no_sentinel") {
    Project project;
    project.clear();
    // Project scaleRotate is 0..31, no inherit sentinel.
    project.setScaleRotate(0);
    expectEqual(project.scaleRotate(), 0, "Project scaleRotate 0");
    project.setScaleRotate(31);
    expectEqual(project.scaleRotate(), 31, "Project scaleRotate 31");
    project.setScaleRotate(40);
    expectEqual(project.scaleRotate(), 31, "Project scaleRotate clamp 31");

    project.setScale(5);
    project.setRootNote(3);
    project.setScaleRotate(7);
    expectEqual(project.scale(), 5, "Project scale 5");
    expectEqual(project.rootNote(), 3, "Project rootNote 3");
    expectEqual(project.scaleRotate(), 7, "Project scaleRotate 7");

    project.setScale(2);
    expectEqual(project.scale(), 2, "Project scale 2");
    expectEqual(project.rootNote(), 3, "Project rootNote unchanged");
    expectEqual(project.scaleRotate(), 7, "Project scaleRotate unchanged");
}

CASE("sizeof_gate_zero_growth") {
    expectEqual(int(sizeof(Project)), kSizeProject, "Project size must equal baseline");
    expectEqual(int(sizeof(NoteSequence)), kSizeNoteSequence, "NoteSequence size must equal baseline");
    expectEqual(int(sizeof(StochasticSequence)), kSizeStochasticSequence, "StochasticSequence size must equal baseline");
    expectEqual(int(sizeof(PhaseFluxSequence)), kSizePhaseFluxSequence, "PhaseFluxSequence size must equal baseline");
    expectEqual(int(sizeof(DiscreteMapSequence)), kSizeDiscreteMapSequence, "DiscreteMapSequence size must equal baseline");
    expectEqual(int(sizeof(IndexedSequence)), kSizeIndexedSequence, "IndexedSequence size must equal baseline");
    expectEqual(int(sizeof(TuesdaySequence)), kSizeTuesdaySequence, "TuesdaySequence size must equal baseline");
    expectEqual(int(sizeof(Track)), kSizeTrack, "Track variant size must equal baseline");
}

CASE("note_serialize_round_trip") {
    serializeRoundTripSequence<NoteSequence>("NoteSequence serialize", 5);
}

CASE("stochastic_serialize_round_trip") {
    serializeRoundTripSequence<StochasticSequence>("StochasticSequence serialize", 5);
}

CASE("phaseflux_serialize_round_trip") {
    serializeRoundTripSequence<PhaseFluxSequence>("PhaseFluxSequence serialize", 5);
}

CASE("discretemap_serialize_round_trip") {
    serializeRoundTripSequence<DiscreteMapSequence>("DiscreteMapSequence serialize", 5);
}

CASE("indexed_serialize_round_trip") {
    serializeRoundTripSequence<IndexedSequence>("IndexedSequence serialize", 5);
}

CASE("tuesday_serialize_round_trip") {
    serializeRoundTripSequence<TuesdaySequence>("TuesdaySequence serialize", 5);
}

CASE("note_serialize_scale_rotate_sentinel") {
    // -1 inherit sentinel must survive the wire too.
    serializeRoundTripSequence<NoteSequence>("NoteSequence serialize -1", -1);
}

CASE("project_serialize_round_trip") {
    Project project;
    project.clear();
    project.setScale(3);
    project.setRootNote(7);
    project.setScaleRotate(5);

    uint8_t buf[131072];
    std::memset(buf, 0, sizeof(buf));
    writeSerializable(project, buf, sizeof(buf));

    Project restored;
    restored.clear();
    readSerializable(restored, buf, sizeof(buf));

    expectEqual(restored.scale(), 3, "Project scale serialize");
    expectEqual(restored.rootNote(), 7, "Project rootNote serialize");
    expectEqual(restored.scaleRotate(), 5, "Project scaleRotate serialize");
}

}
