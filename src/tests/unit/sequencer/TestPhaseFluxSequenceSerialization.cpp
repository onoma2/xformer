#include "UnitTest.h"

#include "apps/sequencer/model/AccumulatorConfig.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"
#include "apps/sequencer/model/PhaseFluxTrack.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Track.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "../core/io/MemoryReaderWriter.h"
#include "model/ProjectVersion.h"

#include <cstring>

static void writeSequence(const PhaseFluxSequence &seq, uint8_t *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    seq.write(writer);
}

static void readSequence(PhaseFluxSequence &seq, const uint8_t *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);
    seq.read(reader);
}

static void writeTrack(const PhaseFluxTrack &track, uint8_t *buf, size_t len) {
    MemoryWriter memoryWriter(buf, len);
    VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t sz) {
        memoryWriter.write(data, sz);
    }, ProjectVersion::Latest);
    track.write(writer);
}

static void readTrack(PhaseFluxTrack &track, const uint8_t *buf, size_t len) {
    MemoryReader memoryReader(buf, len);
    VersionedSerializedReader reader([&memoryReader](void *data, size_t sz) {
        memoryReader.read(data, sz);
    }, ProjectVersion::Latest);
    track.read(reader);
}

// Build a default Stage and compare all Stage fields against a probe Stage —
// used by bitpack_no_overlap to confirm only the field-under-test moved.
static void expectStageEqualExceptOne(const PhaseFluxSequence::Stage &probe,
                                      const PhaseFluxSequence::Stage &defaults,
                                      const char *which, int excludeIndex) {
    int i = 0;
    if (excludeIndex != i++) expectEqual(probe.pulseCount(), defaults.pulseCount(), which);
    if (excludeIndex != i++) expectEqual(probe.basePitch(), defaults.basePitch(), which);
    if (excludeIndex != i++) expectEqual(int(probe.pitchRange()), int(defaults.pitchRange()), which);
    if (excludeIndex != i++) expectEqual(int(probe.pitchDirection()), int(defaults.pitchDirection()), which);
    if (excludeIndex != i++) expectEqual(int(probe.temporalCurve()), int(defaults.temporalCurve()), which);
    if (excludeIndex != i++) expectEqual(probe.temporalFlipV(), defaults.temporalFlipV(), which);
    if (excludeIndex != i++) expectEqual(probe.temporalFlipH(), defaults.temporalFlipH(), which);
    if (excludeIndex != i++) expectEqual(probe.temporalWarp(), defaults.temporalWarp(), which);
    if (excludeIndex != i++) expectEqual(probe.temporalResponse(), defaults.temporalResponse(), which);
    if (excludeIndex != i++) expectEqual(int(probe.pitchCurve()), int(defaults.pitchCurve()), which);
    if (excludeIndex != i++) expectEqual(probe.pitchFlipV(), defaults.pitchFlipV(), which);
    if (excludeIndex != i++) expectEqual(probe.pitchFlipH(), defaults.pitchFlipH(), which);
    if (excludeIndex != i++) expectEqual(probe.pitchWarp(), defaults.pitchWarp(), which);
    if (excludeIndex != i++) expectEqual(probe.pitchResponse(), defaults.pitchResponse(), which);
    if (excludeIndex != i++) expectEqual(probe.maskMelody(), defaults.maskMelody(), which);
    if (excludeIndex != i++) expectEqual(probe.tiltMelody(), defaults.tiltMelody(), which);
    if (excludeIndex != i++) expectEqual(probe.phaseShift(), defaults.phaseShift(), which);
    if (excludeIndex != i++) expectEqual(int(probe.mask()), int(defaults.mask()), which);
    if (excludeIndex != i++) expectEqual(probe.maskShift(), defaults.maskShift(), which);
    if (excludeIndex != i++) expectEqual(probe.accumulatorStep(), defaults.accumulatorStep(), which);
    if (excludeIndex != i++) expectEqual(probe.pulseAccumStep(), defaults.pulseAccumStep(), which);
    if (excludeIndex != i++) expectEqual(probe.gateLength(), defaults.gateLength(), which);
    if (excludeIndex != i++) expectEqual(probe.skip(), defaults.skip(), which);
    if (excludeIndex != i++) expectEqual(int(probe.accumulatorTrigger()), int(defaults.accumulatorTrigger()), which);
    if (excludeIndex != i++) expectEqual(int(probe.pulseAccumTrigger()), int(defaults.pulseAccumTrigger()), which);
    if (excludeIndex != i++) expectEqual(probe.length(), defaults.length(), which);
}

UNIT_TEST("PhaseFluxSequenceSerialization") {

CASE("default_round_trip") {
    PhaseFluxSequence seq;
    seq.clear();

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence restored;
    restored.clear();
    readSequence(restored, buf, sizeof(buf));

    expectEqual(restored.scale(), -1, "scale default");
    expectEqual(restored.rootNote(), -1, "rootNote default");
    expectEqual(restored.divisor(), 12, "divisor default");
    expectEqual(restored.clockMultiplier(), 100, "clockMultiplier default");
    expectEqual(restored.resetMeasure(), 0, "resetMeasure default");
    expectEqual(restored.firstStage(), 0, "firstStage default");
    expectEqual(restored.lastStage(), PhaseFluxSequence::StageCount - 1, "lastStage default");

    for (int i = 0; i < PhaseFluxSequence::StageCount; ++i) {
        const auto &s = restored.stage(i);
        // Fresh sequence = default NoteTrack: stages 0..3 are active Quarter
        // beats (4 pulses each), 4..15 are skipped (pulseCount 0).
        expectEqual(s.pulseCount(), i < 4 ? 4 : 0, "default pulseCount");
        expectEqual(s.basePitch(), 0, "default basePitch");
        expectEqual(int(s.pitchRange()), int(PhaseFluxSequence::PitchRangeType::One), "default pitchRange");
        expectEqual(int(s.pitchDirection()), int(PhaseFluxSequence::PitchDirectionType::Up), "default pitchDirection");
        expectEqual(int(s.temporalCurve()), int(PhaseFluxSequence::TemporalCurveType::Linear), "default temporalCurve");
        expectEqual(s.temporalFlipV(), false, "default temporalFlipV");
        expectEqual(s.temporalFlipH(), false, "default temporalFlipH");
        expectEqual(s.temporalWarp(), 0, "default temporalWarp");
        expectEqual(s.temporalResponse(), 0, "default temporalResponse");
        expectEqual(int(s.pitchCurve()), int(PhaseFluxSequence::PitchCurveType::Ramp), "default pitchCurve");
        expectEqual(s.pitchFlipV(), false, "default pitchFlipV");
        expectEqual(s.pitchFlipH(), false, "default pitchFlipH");
        expectEqual(s.pitchWarp(), 0, "default pitchWarp");
        expectEqual(s.pitchResponse(), 0, "default pitchResponse");
        expectEqual(s.maskMelody(), 100, "default maskMelody");
        expectEqual(s.tiltMelody(), 0, "default tiltMelody");
        expectEqual(s.phaseShift(), 0, "default phaseShift");
        expectEqual(int(s.mask()), int(PhaseFluxSequence::MaskType::Off), "default mask");
        expectEqual(s.maskShift(), 0, "default maskShift");
        expectEqual(s.accumulatorStep(), 0, "default accumulatorStep");
        expectEqual(s.pulseAccumStep(), 0, "default pulseAccumStep");
        expectEqual(s.gateLength(), 50, "default gateLength");
        // Fresh sequence: stages 0..3 active beats, 4..15 skipped.
        expectEqual(s.skip(), i >= 4, "default skip (stages 0..3 active)");
        expectEqual(int(s.accumulatorTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Stage), "default accumulatorTrigger");
        expectEqual(int(s.pulseAccumTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Stage), "default pulseAccumTrigger");
        expectEqual(s.length(), 4, "default length (FLUX LENG = one beat at 1/16)");
        expectEqual(int(s.temporalRepeat()), int(PhaseFluxSequence::RepeatType::x1), "default temporalRepeat");
        expectEqual(int(s.pitchRepeat()),    int(PhaseFluxSequence::RepeatType::x1), "default pitchRepeat");
        expectEqual(int(s.temporalWindow()), int(PhaseFluxSequence::WindowType::Off), "default temporalWindow");
        expectEqual(int(s.pitchWindow()),    int(PhaseFluxSequence::WindowType::Off), "default pitchWindow");
    }
}

CASE("chassis_fields_persist") {
    PhaseFluxSequence seq;
    seq.clear();
    seq.setScale(3);
    seq.setRootNote(5);
    seq.setDivisor(24);
    seq.setClockMultiplier(133);
    seq.setResetMeasure(8);
    seq.setLastStage(11);
    seq.setFirstStage(3);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence restored;
    restored.clear();
    readSequence(restored, buf, sizeof(buf));

    expectEqual(restored.scale(), 3, "scale persists");
    expectEqual(restored.rootNote(), 5, "rootNote persists");
    expectEqual(restored.divisor(), 24, "divisor persists");
    expectEqual(restored.clockMultiplier(), 133, "clockMultiplier persists");
    expectEqual(restored.resetMeasure(), 8, "resetMeasure persists");
    expectEqual(restored.firstStage(), 3, "firstStage persists");
    expectEqual(restored.lastStage(), 11, "lastStage persists");

    // Track-level chassis
    PhaseFluxTrack track;
    track.clear();
    track.setSlideTime(77);
    track.setOctave(-3);
    track.setTranspose(42);
    track.setPlayMode(Types::PlayMode::Free);
    track.setFillMode(PhaseFluxTrack::FillMode::NextPattern);
    track.setCvUpdateMode(PhaseFluxTrack::CvUpdateMode::Always);

    uint8_t tbuf[65536];
    std::memset(tbuf, 0, sizeof(tbuf));
    writeTrack(track, tbuf, sizeof(tbuf));

    PhaseFluxTrack rtrack;
    rtrack.clear();
    readTrack(rtrack, tbuf, sizeof(tbuf));

    expectEqual(rtrack.slideTime(), 77, "slideTime persists");
    expectEqual(rtrack.octave(), -3, "octave persists");
    expectEqual(rtrack.transpose(), 42, "transpose persists");
    expectTrue(rtrack.playMode() == Types::PlayMode::Free, "playMode persists");
    expectTrue(rtrack.fillMode() == PhaseFluxTrack::FillMode::NextPattern, "fillMode persists");
    expectTrue(rtrack.cvUpdateMode() == PhaseFluxTrack::CvUpdateMode::Always, "cvUpdateMode persists");
}

CASE("stage_fields_persist") {
    // Distribute 24 field touches across the 16 stages. Each stage gets a
    // distinctive set of values; round-trip must restore them all.
    PhaseFluxSequence seq;
    seq.clear();

    auto &s0  = seq.stage(0);
    auto &s1  = seq.stage(1);
    auto &s2  = seq.stage(2);
    auto &s3  = seq.stage(3);
    auto &s4  = seq.stage(4);
    auto &s5  = seq.stage(5);
    auto &s6  = seq.stage(6);
    auto &s7  = seq.stage(7);

    s0.setPulseCount(5);
    s0.setBasePitch(11);
    s0.setPitchRange(PhaseFluxSequence::PitchRangeType::Two);
    s0.setPitchDirection(PhaseFluxSequence::PitchDirectionType::Bipolar);

    s1.setTemporalCurve(PhaseFluxSequence::TemporalCurveType::Bell);
    s1.setTemporalFlipV(true);
    s1.setTemporalFlipH(true);
    s1.setTemporalWarp(31);
    s1.setTemporalResponse(-17);

    s2.setPitchCurve(PhaseFluxSequence::PitchCurveType::Triangle);
    s2.setPitchFlipV(true);
    s2.setPitchFlipH(true);
    s2.setPitchWarp(-29);
    s2.setPitchResponse(23);

    s3.setMaskMelody(73);
    s3.setTiltMelody(42);
    s3.setPhaseShift(5);

    s4.setMask(PhaseFluxSequence::MaskType::TwoInFour);
    s4.setMaskShift(3);

    s5.setAccumulatorStep(7);
    s5.setPulseAccumStep(-3);
    s5.setAccumulatorTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse);
    s5.setPulseAccumTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse);

    s6.setGateLength(88);
    s6.setLength(9);

    s7.setSkip(true);
    s7.setLength(42);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    expectEqual(r.stage(0).pulseCount(), 5, "s0 pulseCount");
    expectEqual(r.stage(0).basePitch(), 11, "s0 basePitch");
    expectEqual(int(r.stage(0).pitchRange()), int(PhaseFluxSequence::PitchRangeType::Two), "s0 pitchRange");
    expectEqual(int(r.stage(0).pitchDirection()), int(PhaseFluxSequence::PitchDirectionType::Bipolar), "s0 pitchDirection");

    expectEqual(int(r.stage(1).temporalCurve()), int(PhaseFluxSequence::TemporalCurveType::Bell), "s1 temporalCurve");
    expectEqual(r.stage(1).temporalFlipV(), true, "s1 temporalFlipV");
    expectEqual(r.stage(1).temporalFlipH(), true, "s1 temporalFlipH");
    expectEqual(r.stage(1).temporalWarp(), 31, "s1 temporalWarp");
    expectEqual(r.stage(1).temporalResponse(), -17, "s1 temporalResponse");

    expectEqual(int(r.stage(2).pitchCurve()), int(PhaseFluxSequence::PitchCurveType::Triangle), "s2 pitchCurve");
    expectEqual(r.stage(2).pitchFlipV(), true, "s2 pitchFlipV");
    expectEqual(r.stage(2).pitchFlipH(), true, "s2 pitchFlipH");
    expectEqual(r.stage(2).pitchWarp(), -29, "s2 pitchWarp");
    expectEqual(r.stage(2).pitchResponse(), 23, "s2 pitchResponse");

    expectEqual(r.stage(3).maskMelody(), 73, "s3 maskMelody");
    expectEqual(r.stage(3).tiltMelody(), 42, "s3 tiltMelody");
    expectEqual(r.stage(3).phaseShift(), 5, "s3 phaseShift");

    expectEqual(int(r.stage(4).mask()), int(PhaseFluxSequence::MaskType::TwoInFour), "s4 mask");
    expectEqual(r.stage(4).maskShift(), 3, "s4 maskShift");

    expectEqual(r.stage(5).accumulatorStep(), 7, "s5 accumulatorStep");
    expectEqual(r.stage(5).pulseAccumStep(), -3, "s5 pulseAccumStep");
    expectEqual(int(r.stage(5).accumulatorTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Pulse), "s5 accumulatorTrigger");
    expectEqual(int(r.stage(5).pulseAccumTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Pulse), "s5 pulseAccumTrigger");

    expectEqual(r.stage(6).gateLength(), 88, "s6 gateLength");
    expectEqual(r.stage(6).length(), 9, "s6 length");

    expectEqual(r.stage(7).skip(), true, "s7 skip");
    expectEqual(r.stage(7).length(), 42, "s7 length");
}

CASE("repeat_and_window_roundtrip") {
    // §14.2 — per-axis Repeat (x1/x2/x3/x5) + Window (Off/F70/F50/P70/P50).
    PhaseFluxSequence seq;
    seq.clear();

    seq.stage(0).setTemporalRepeat(PhaseFluxSequence::RepeatType::x1);
    seq.stage(0).setPitchRepeat(PhaseFluxSequence::RepeatType::x5);
    seq.stage(0).setTemporalWindow(PhaseFluxSequence::WindowType::Off);
    seq.stage(0).setPitchWindow(PhaseFluxSequence::WindowType::Polarize50);

    seq.stage(1).setTemporalRepeat(PhaseFluxSequence::RepeatType::x2);
    seq.stage(1).setPitchRepeat(PhaseFluxSequence::RepeatType::x3);
    seq.stage(1).setTemporalWindow(PhaseFluxSequence::WindowType::Focus70);
    seq.stage(1).setPitchWindow(PhaseFluxSequence::WindowType::Focus50);

    seq.stage(2).setTemporalRepeat(PhaseFluxSequence::RepeatType::x3);
    seq.stage(2).setPitchRepeat(PhaseFluxSequence::RepeatType::x2);
    seq.stage(2).setTemporalWindow(PhaseFluxSequence::WindowType::Polarize70);
    seq.stage(2).setPitchWindow(PhaseFluxSequence::WindowType::Off);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence restored;
    restored.clear();
    readSequence(restored, buf, sizeof(buf));

    expectEqual(int(restored.stage(0).temporalRepeat()), int(PhaseFluxSequence::RepeatType::x1), "s0 temporalRepeat");
    expectEqual(int(restored.stage(0).pitchRepeat()),    int(PhaseFluxSequence::RepeatType::x5), "s0 pitchRepeat");
    expectEqual(int(restored.stage(0).temporalWindow()), int(PhaseFluxSequence::WindowType::Off),       "s0 temporalWindow");
    expectEqual(int(restored.stage(0).pitchWindow()),    int(PhaseFluxSequence::WindowType::Polarize50),"s0 pitchWindow");

    expectEqual(int(restored.stage(1).temporalRepeat()), int(PhaseFluxSequence::RepeatType::x2), "s1 temporalRepeat");
    expectEqual(int(restored.stage(1).pitchRepeat()),    int(PhaseFluxSequence::RepeatType::x3), "s1 pitchRepeat");
    expectEqual(int(restored.stage(1).temporalWindow()), int(PhaseFluxSequence::WindowType::Focus70), "s1 temporalWindow");
    expectEqual(int(restored.stage(1).pitchWindow()),    int(PhaseFluxSequence::WindowType::Focus50), "s1 pitchWindow");

    expectEqual(int(restored.stage(2).temporalRepeat()), int(PhaseFluxSequence::RepeatType::x3), "s2 temporalRepeat");
    expectEqual(int(restored.stage(2).pitchRepeat()),    int(PhaseFluxSequence::RepeatType::x2), "s2 pitchRepeat");
    expectEqual(int(restored.stage(2).temporalWindow()), int(PhaseFluxSequence::WindowType::Polarize70), "s2 temporalWindow");
    expectEqual(int(restored.stage(2).pitchWindow()),    int(PhaseFluxSequence::WindowType::Off),       "s2 pitchWindow");
}

CASE("stage_edge_values_persist") {
    // Pin min + max on stages 0/1 for every numeric field. Bool fields toggle
    // across stages. Catches sign-extension bugs and clamp drift.
    PhaseFluxSequence seq;
    seq.clear();

    seq.stage(0).setPulseCount(1);
    seq.stage(1).setPulseCount(8);

    seq.stage(0).setBasePitch(-63);
    seq.stage(1).setBasePitch(63);

    seq.stage(0).setTemporalWarp(-63);
    seq.stage(1).setTemporalWarp(63);

    seq.stage(0).setTemporalResponse(-63);
    seq.stage(1).setTemporalResponse(63);

    seq.stage(0).setPitchWarp(-63);
    seq.stage(1).setPitchWarp(63);

    seq.stage(0).setPitchResponse(-63);
    seq.stage(1).setPitchResponse(63);

    seq.stage(0).setMaskMelody(0);
    seq.stage(1).setMaskMelody(100);

    seq.stage(0).setTiltMelody(0);
    seq.stage(1).setTiltMelody(100);

    seq.stage(0).setPhaseShift(0);
    seq.stage(1).setPhaseShift(7);

    seq.stage(0).setMaskShift(0);
    seq.stage(1).setMaskShift(7);

    seq.stage(0).setAccumulatorStep(-15);
    seq.stage(1).setAccumulatorStep(15);

    seq.stage(0).setPulseAccumStep(-7);
    seq.stage(1).setPulseAccumStep(7);

    seq.stage(0).setAccumulatorTrigger(PhaseFluxSequence::AccumulatorTriggerType::Stage);
    seq.stage(1).setAccumulatorTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse);

    seq.stage(0).setPulseAccumTrigger(PhaseFluxSequence::AccumulatorTriggerType::Stage);
    seq.stage(1).setPulseAccumTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse);

    seq.stage(0).setGateLength(0);
    seq.stage(1).setGateLength(100);

    seq.stage(0).setLength(1);
    seq.stage(1).setLength(127);

    seq.stage(0).setPitchRange(PhaseFluxSequence::PitchRangeType::Half);
    seq.stage(1).setPitchRange(PhaseFluxSequence::PitchRangeType::Three);

    seq.stage(0).setPitchDirection(PhaseFluxSequence::PitchDirectionType::Up);
    seq.stage(1).setPitchDirection(PhaseFluxSequence::PitchDirectionType::Bipolar);

    seq.stage(0).setTemporalCurve(PhaseFluxSequence::TemporalCurveType::Linear);
    seq.stage(1).setTemporalCurve(PhaseFluxSequence::TemporalCurveType::Bounce);

    seq.stage(0).setPitchCurve(PhaseFluxSequence::PitchCurveType::Ramp);
    seq.stage(1).setPitchCurve(PhaseFluxSequence::PitchCurveType::Triangle);

    seq.stage(0).setMask(PhaseFluxSequence::MaskType::Off);
    seq.stage(1).setMask(PhaseFluxSequence::MaskType::OneInEight);

    seq.stage(0).setSkip(false);
    seq.stage(1).setSkip(true);

    seq.stage(0).setTemporalFlipV(false);
    seq.stage(1).setTemporalFlipV(true);
    seq.stage(0).setTemporalFlipH(false);
    seq.stage(1).setTemporalFlipH(true);
    seq.stage(0).setPitchFlipV(false);
    seq.stage(1).setPitchFlipV(true);
    seq.stage(0).setPitchFlipH(false);
    seq.stage(1).setPitchFlipH(true);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    expectEqual(r.stage(0).pulseCount(), 1, "min pulseCount");
    expectEqual(r.stage(1).pulseCount(), 8, "max pulseCount");
    expectEqual(r.stage(0).basePitch(), -63, "min basePitch");
    expectEqual(r.stage(1).basePitch(), 63, "max basePitch");
    expectEqual(r.stage(0).temporalWarp(), -63, "min temporalWarp");
    expectEqual(r.stage(1).temporalWarp(), 63, "max temporalWarp");
    expectEqual(r.stage(0).temporalResponse(), -63, "min temporalResponse");
    expectEqual(r.stage(1).temporalResponse(), 63, "max temporalResponse");
    expectEqual(r.stage(0).pitchWarp(), -63, "min pitchWarp");
    expectEqual(r.stage(1).pitchWarp(), 63, "max pitchWarp");
    expectEqual(r.stage(0).pitchResponse(), -63, "min pitchResponse");
    expectEqual(r.stage(1).pitchResponse(), 63, "max pitchResponse");
    expectEqual(r.stage(0).maskMelody(), 0, "min maskMelody");
    expectEqual(r.stage(1).maskMelody(), 100, "max maskMelody");
    expectEqual(r.stage(0).tiltMelody(), 0, "min tiltMelody");
    expectEqual(r.stage(1).tiltMelody(), 100, "max tiltMelody");
    expectEqual(r.stage(0).phaseShift(), 0, "min phaseShift");
    expectEqual(r.stage(1).phaseShift(), 7, "max phaseShift");
    expectEqual(r.stage(0).maskShift(), 0, "min maskShift");
    expectEqual(r.stage(1).maskShift(), 7, "max maskShift");
    expectEqual(r.stage(0).accumulatorStep(), -15, "min accumulatorStep");
    expectEqual(r.stage(1).accumulatorStep(), 15, "max accumulatorStep");
    expectEqual(r.stage(0).pulseAccumStep(), -7, "min pulseAccumStep");
    expectEqual(r.stage(1).pulseAccumStep(), 7, "max pulseAccumStep");
    expectEqual(int(r.stage(0).accumulatorTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Stage), "min accumulatorTrigger");
    expectEqual(int(r.stage(1).accumulatorTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Pulse), "max accumulatorTrigger");
    expectEqual(int(r.stage(0).pulseAccumTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Stage), "min pulseAccumTrigger");
    expectEqual(int(r.stage(1).pulseAccumTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Pulse), "max pulseAccumTrigger");
    expectEqual(r.stage(0).gateLength(), 0, "min gateLength");
    expectEqual(r.stage(1).gateLength(), 100, "max gateLength");
    expectEqual(r.stage(0).length(), 1, "min length");
    expectEqual(r.stage(1).length(), 127, "max length");
    expectEqual(int(r.stage(0).pitchRange()), int(PhaseFluxSequence::PitchRangeType::Half), "min pitchRange");
    expectEqual(int(r.stage(1).pitchRange()), int(PhaseFluxSequence::PitchRangeType::Three), "max pitchRange");
    expectEqual(int(r.stage(1).mask()), int(PhaseFluxSequence::MaskType::OneInEight), "max mask");
    expectEqual(r.stage(0).skip(), false, "skip false");
    expectEqual(r.stage(1).skip(), true, "skip true");
    expectEqual(r.stage(1).temporalFlipV(), true, "temporalFlipV true");
    expectEqual(r.stage(1).temporalFlipH(), true, "temporalFlipH true");
    expectEqual(r.stage(1).pitchFlipV(), true, "pitchFlipV true");
    expectEqual(r.stage(1).pitchFlipH(), true, "pitchFlipH true");
}

CASE("lock_does_not_persist") {
    PhaseFluxTrack track;
    track.clear();
    track.setLock(true);
    expectEqual(track.lock(), true, "lock true before write");

    uint8_t buf[65536];
    std::memset(buf, 0, sizeof(buf));
    writeTrack(track, buf, sizeof(buf));

    PhaseFluxTrack restored;
    restored.clear();
    readTrack(restored, buf, sizeof(buf));

    expectEqual(restored.lock(), false, "lock must NOT persist (parity with Stochastic)");
}

CASE("bitpack_no_overlap") {
    // Retro lesson #4 gate. For every Stage field, set ONE field to its max
    // on stage 0 and verify all 23 other fields still read their default
    // values. Adjacent bit offset swaps must make this fail.
    PhaseFluxSequence defaults;
    defaults.clear();
    const auto &def = defaults.stage(0);

    auto probe = [&](int idx, void (*set)(PhaseFluxSequence::Stage &)) {
        PhaseFluxSequence seq;
        seq.clear();
        set(seq.stage(0));

        uint8_t buf[4096];
        std::memset(buf, 0, sizeof(buf));
        writeSequence(seq, buf, sizeof(buf));

        PhaseFluxSequence r;
        r.clear();
        readSequence(r, buf, sizeof(buf));

        expectStageEqualExceptOne(r.stage(0), def, "no overlap", idx);
    };

    // Order must match expectStageEqualExceptOne's enumeration.
    probe(0,  [](PhaseFluxSequence::Stage &s) { s.setPulseCount(8); });
    probe(1,  [](PhaseFluxSequence::Stage &s) { s.setBasePitch(63); });
    probe(2,  [](PhaseFluxSequence::Stage &s) { s.setPitchRange(PhaseFluxSequence::PitchRangeType::Three); });
    probe(3,  [](PhaseFluxSequence::Stage &s) { s.setPitchDirection(PhaseFluxSequence::PitchDirectionType::Bipolar); });
    probe(4,  [](PhaseFluxSequence::Stage &s) { s.setTemporalCurve(PhaseFluxSequence::TemporalCurveType::Bounce); });
    probe(5,  [](PhaseFluxSequence::Stage &s) { s.setTemporalFlipV(true); });
    probe(6,  [](PhaseFluxSequence::Stage &s) { s.setTemporalFlipH(true); });
    probe(7,  [](PhaseFluxSequence::Stage &s) { s.setTemporalWarp(63); });
    probe(8,  [](PhaseFluxSequence::Stage &s) { s.setTemporalResponse(63); });
    probe(9,  [](PhaseFluxSequence::Stage &s) { s.setPitchCurve(PhaseFluxSequence::PitchCurveType::Triangle); });
    probe(10, [](PhaseFluxSequence::Stage &s) { s.setPitchFlipV(true); });
    probe(11, [](PhaseFluxSequence::Stage &s) { s.setPitchFlipH(true); });
    probe(12, [](PhaseFluxSequence::Stage &s) { s.setPitchWarp(63); });
    probe(13, [](PhaseFluxSequence::Stage &s) { s.setPitchResponse(63); });
    probe(14, [](PhaseFluxSequence::Stage &s) { s.setMaskMelody(0); });
    probe(15, [](PhaseFluxSequence::Stage &s) { s.setTiltMelody(100); });
    probe(16, [](PhaseFluxSequence::Stage &s) { s.setPhaseShift(7); });
    probe(17, [](PhaseFluxSequence::Stage &s) { s.setMask(PhaseFluxSequence::MaskType::OneInEight); });
    probe(18, [](PhaseFluxSequence::Stage &s) { s.setMaskShift(7); });
    probe(19, [](PhaseFluxSequence::Stage &s) { s.setAccumulatorStep(15); });
    probe(20, [](PhaseFluxSequence::Stage &s) { s.setPulseAccumStep(7); });
    probe(21, [](PhaseFluxSequence::Stage &s) { s.setGateLength(100); });
    probe(22, [](PhaseFluxSequence::Stage &s) { s.setSkip(true); });
    probe(23, [](PhaseFluxSequence::Stage &s) { s.setAccumulatorTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse); });
    probe(24, [](PhaseFluxSequence::Stage &s) { s.setPulseAccumTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse); });
    probe(25, [](PhaseFluxSequence::Stage &s) { s.setLength(63); });
}

CASE("track_writes_phaseflux_mode") {
    // Full Container + union + dispatch path: Project::setTrackMode flips
    // track 0 to PhaseFlux, we poke a few chassis + stage fields, then
    // serialize via Track::write and restore via Track::read.
    Project project;
    project.setTrackMode(0, Track::TrackMode::PhaseFlux);
    Track &track = project.track(0);
    expectEqual(int(track.trackMode()), int(Track::TrackMode::PhaseFlux), "track in PhaseFlux mode");

    auto &pfTrack = track.phaseFluxTrack();
    pfTrack.setOctave(3);
    pfTrack.setTranspose(-7);
    pfTrack.setSlideTime(42);

    auto &seq = pfTrack.sequence(0);
    seq.setPitchRate(3);  // some non-default ratio index
    seq.setPitchMode(PhaseFluxSequence::PitchMode::Global);

    auto &stage0 = seq.stage(0);
    stage0.setBasePitch(12);
    stage0.setMask(PhaseFluxSequence::MaskType::OneInFour);
    stage0.setPulseCount(4);

    uint8_t buf[65536];
    std::memset(buf, 0, sizeof(buf));
    MemoryWriter mw(buf, sizeof(buf));
    VersionedSerializedWriter writer([&mw](const void *d, size_t s){ mw.write(d, s); }, ProjectVersion::Latest);
    track.write(writer);

    // restored track must be flipped to PhaseFlux *before* read; Track::read
    // reads the mode byte then dispatches into the matching Container slot.
    Project project2;
    project2.setTrackMode(0, Track::TrackMode::PhaseFlux);
    Track &restored = project2.track(0);

    MemoryReader mr(buf, sizeof(buf));
    VersionedSerializedReader reader([&mr](void *d, size_t s){ mr.read(d, s); }, ProjectVersion::Latest);
    restored.read(reader);

    expectEqual(int(restored.trackMode()), int(Track::TrackMode::PhaseFlux), "mode preserved");
    expectEqual(restored.phaseFluxTrack().octave(), 3, "octave preserved");
    expectEqual(restored.phaseFluxTrack().transpose(), -7, "transpose preserved");
    expectEqual(restored.phaseFluxTrack().slideTime(), 42, "slideTime preserved");
    expectEqual(int(restored.phaseFluxTrack().sequence(0).pitchMode()), int(PhaseFluxSequence::PitchMode::Global), "pitchMode preserved");
    expectEqual(restored.phaseFluxTrack().sequence(0).pitchRate(), 3, "pitchRate preserved");
    expectEqual(restored.phaseFluxTrack().sequence(0).stage(0).basePitch(), 12, "stage 0 basePitch preserved");
    expectEqual(int(restored.phaseFluxTrack().sequence(0).stage(0).mask()), int(PhaseFluxSequence::MaskType::OneInFour), "stage 0 mask preserved");
    expectEqual(restored.phaseFluxTrack().sequence(0).stage(0).pulseCount(), 4, "stage 0 pulseCount preserved");
}

CASE("default_clear_initializes_pulse_lims_to_4") {
    PhaseFluxSequence seq;
    seq.clear();

    expectEqual(int(seq.noteAccumConfig().scope()), int(AccumulatorConfig::Scope::Local), "note scope default Local");
    expectEqual(int(seq.noteAccumConfig().order()), int(AccumulatorConfig::Order::Wrap), "note order default Wrap");
    expectEqual(int(seq.noteAccumConfig().polarity()), int(AccumulatorConfig::Polarity::Uni), "note polarity default Uni");
    expectEqual(int(seq.noteAccumConfig().reset()), 0, "note reset default 0");
    expectEqual(int(seq.noteAccumConfig().posLim()), 28, "note posLim default 28");
    expectEqual(int(seq.noteAccumConfig().negLim()), 28, "note negLim default 28");

    expectEqual(int(seq.pulseAccumConfig().scope()), int(AccumulatorConfig::Scope::Local), "pulse scope default Local");
    expectEqual(int(seq.pulseAccumConfig().order()), int(AccumulatorConfig::Order::Wrap), "pulse order default Wrap");
    expectEqual(int(seq.pulseAccumConfig().polarity()), int(AccumulatorConfig::Polarity::Uni), "pulse polarity default Uni");
    expectEqual(int(seq.pulseAccumConfig().reset()), 0, "pulse reset default 0");
    expectEqual(int(seq.pulseAccumConfig().posLim()), 16, "pulse posLim default 16 (pulseCount ceiling)");
    expectEqual(int(seq.pulseAccumConfig().negLim()), 16, "pulse negLim default 16 (pulseCount ceiling)");
}

CASE("note_accum_config_roundtrips_with_non_defaults") {
    PhaseFluxSequence seq;
    seq.clear();
    seq.noteAccumConfig().setScope(AccumulatorConfig::Scope::Track);
    seq.noteAccumConfig().setOrder(AccumulatorConfig::Order::Pendulum);
    seq.noteAccumConfig().setPolarity(AccumulatorConfig::Polarity::Bi);
    seq.noteAccumConfig().setReset(11);
    seq.noteAccumConfig().setPosLim(23);
    seq.noteAccumConfig().setNegLim(17);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    expectEqual(int(r.noteAccumConfig().scope()), int(AccumulatorConfig::Scope::Track), "note scope persists");
    expectEqual(int(r.noteAccumConfig().order()), int(AccumulatorConfig::Order::Pendulum), "note order persists");
    expectEqual(int(r.noteAccumConfig().polarity()), int(AccumulatorConfig::Polarity::Bi), "note polarity persists");
    expectEqual(int(r.noteAccumConfig().reset()), 11, "note reset persists");
    expectEqual(int(r.noteAccumConfig().posLim()), 23, "note posLim persists");
    expectEqual(int(r.noteAccumConfig().negLim()), 17, "note negLim persists");
}

CASE("pulse_accum_config_roundtrips_with_non_defaults") {
    PhaseFluxSequence seq;
    seq.clear();
    seq.pulseAccumConfig().setScope(AccumulatorConfig::Scope::Track);
    seq.pulseAccumConfig().setOrder(AccumulatorConfig::Order::RTZ);
    seq.pulseAccumConfig().setPolarity(AccumulatorConfig::Polarity::Bi);
    seq.pulseAccumConfig().setReset(9);
    seq.pulseAccumConfig().setPosLim(8);
    seq.pulseAccumConfig().setNegLim(6);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    expectEqual(int(r.pulseAccumConfig().scope()), int(AccumulatorConfig::Scope::Track), "pulse scope persists");
    expectEqual(int(r.pulseAccumConfig().order()), int(AccumulatorConfig::Order::RTZ), "pulse order persists");
    expectEqual(int(r.pulseAccumConfig().polarity()), int(AccumulatorConfig::Polarity::Bi), "pulse polarity persists");
    expectEqual(int(r.pulseAccumConfig().reset()), 9, "pulse reset persists");
    expectEqual(int(r.pulseAccumConfig().posLim()), 8, "pulse posLim persists");
    expectEqual(int(r.pulseAccumConfig().negLim()), 6, "pulse negLim persists");
}

CASE("pulse_accum_step_per_cell_roundtrips") {
    PhaseFluxSequence seq;
    seq.clear();
    seq.stage(3).setPulseAccumStep(5);
    seq.stage(11).setPulseAccumStep(-4);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    expectEqual(r.stage(3).pulseAccumStep(), 5, "stage 3 pulseAccumStep persists");
    expectEqual(r.stage(11).pulseAccumStep(), -4, "stage 11 pulseAccumStep persists");
    expectEqual(r.stage(0).pulseAccumStep(), 0, "other stages keep default");
}

CASE("accumulator_trigger_per_cell_roundtrips") {
    PhaseFluxSequence seq;
    seq.clear();
    seq.stage(7).setAccumulatorTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    expectEqual(int(r.stage(7).accumulatorTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Pulse), "stage 7 accumulatorTrigger persists");
    expectEqual(int(r.stage(0).accumulatorTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Stage), "other stages keep default Stage");
}

CASE("pulse_accum_trigger_per_cell_roundtrips") {
    PhaseFluxSequence seq;
    seq.clear();
    seq.stage(9).setPulseAccumTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    expectEqual(int(r.stage(9).pulseAccumTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Pulse), "stage 9 pulseAccumTrigger persists");
    expectEqual(int(r.stage(0).pulseAccumTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Stage), "other stages keep default Stage");
}

CASE("bit_pack_no_collision") {
    // Write every per-cell field that lives in _data2 to a distinct,
    // non-default value on the same Stage. If any pair shares bits, at
    // least one read-back will be corrupted.
    PhaseFluxSequence seq;
    seq.clear();
    auto &s = seq.stage(0);
    s.setPhaseShift(5);
    s.setMask(PhaseFluxSequence::MaskType::TwoInFour);
    s.setMaskShift(6);
    s.setAccumulatorStep(-11);
    s.setPulseAccumStep(6);
    s.setGateLength(73);
    // _data2 bits 25..27 are spare (freed by the F1 stageDivisor removal).
    s.setSkip(true);
    s.setAccumulatorTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse);
    s.setPulseAccumTrigger(PhaseFluxSequence::AccumulatorTriggerType::Pulse);

    uint8_t buf[4096];
    std::memset(buf, 0, sizeof(buf));
    writeSequence(seq, buf, sizeof(buf));

    PhaseFluxSequence r;
    r.clear();
    readSequence(r, buf, sizeof(buf));

    const auto &rs = r.stage(0);
    expectEqual(rs.phaseShift(), 5, "phaseShift survives collision check");
    expectEqual(int(rs.mask()), int(PhaseFluxSequence::MaskType::TwoInFour), "mask survives collision check");
    expectEqual(rs.maskShift(), 6, "maskShift survives collision check");
    expectEqual(rs.accumulatorStep(), -11, "accumulatorStep survives collision check");
    expectEqual(rs.pulseAccumStep(), 6, "pulseAccumStep survives collision check");
    expectEqual(rs.gateLength(), 73, "gateLength survives collision check");
    expectEqual(rs.skip(), true, "skip survives collision check");
    expectEqual(int(rs.accumulatorTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Pulse), "accumulatorTrigger survives collision check");
    expectEqual(int(rs.pulseAccumTrigger()), int(PhaseFluxSequence::AccumulatorTriggerType::Pulse), "pulseAccumTrigger survives collision check");
}

CASE("loop_bounds_clamp_and_single_stage") {
    PhaseFluxSequence seq;
    seq.clear();
    expectEqual(seq.firstStage(), 0, "first defaults to 0");
    expectEqual(seq.lastStage(), PhaseFluxSequence::StageCount - 1, "last defaults to 15");

    // last cannot drop below first
    seq.setFirstStage(6);
    seq.setLastStage(3);
    expectEqual(seq.lastStage(), 6, "last clamped up to first");

    // single-stage loop: raise last first (first clamps to <= last), then first
    seq.setLastStage(9);
    seq.setFirstStage(9);
    expectEqual(seq.firstStage(), 9, "single-stage first");
    expectEqual(seq.lastStage(), 9, "single-stage last");

    // first cannot exceed last
    seq.setFirstStage(15);
    expectEqual(seq.firstStage(), 9, "first clamped down to last");
}

CASE("loop_bounds_offset_moves_window") {
    PhaseFluxSequence seq;
    seq.clear();
    seq.setFirstStage(2);
    seq.setLastStage(5);

    // shift gesture slides both bounds together
    seq.editFirstStage(3, true);
    expectEqual(seq.firstStage(), 5, "window first slid +3");
    expectEqual(seq.lastStage(), 8, "window last slid +3");

    // window clamps at the top edge without collapsing
    seq.editLastStage(20, true);
    expectEqual(seq.lastStage(), 15, "window last pinned at 15");
    expectEqual(seq.lastStage() - seq.firstStage(), 3, "window width preserved at edge");
}

} // UNIT_TEST("PhaseFluxSequenceSerialization")
