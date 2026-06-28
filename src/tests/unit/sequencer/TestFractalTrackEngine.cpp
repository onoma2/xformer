#include "UnitTest.h"

#include "apps/sequencer/engine/FractalTrackEngine.h"
#include "apps/sequencer/engine/Engine.h"
#include "apps/sequencer/model/FractalSequence.h"
#include "apps/sequencer/model/FractalTrack.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/model/Scale.h"
#include "apps/sequencer/Config.h"

#include <new>
#include <vector>
#include <cmath>

// Branch op codes mirror the engine's private enum BranchOp.
enum { kOpTranspose = 0, kOpReverse = 1, kOpRotate = 4, kOpGateThin = 7 };

// Traversal is exercised capture-free: replaySection() never dereferences the
// Engine when the track is not armed, so a placeholder Engine reference (never
// touched) is enough to construct the engine. The full Engine wants the
// Simulator singleton + hardware drivers, which a pure traversal test must not
// boot. replaySectionForTest() drives one section and updates readPos().
namespace {

alignas(Engine) unsigned char g_engineStorage[sizeof(Engine)];
Engine &placeholderEngine() { return *reinterpret_cast<Engine *>(g_engineStorage); }

FractalSequence &setupLoop(Model &model, int loopFirst, int loopLast, FractalSequence::OrderMode orderMode) {
    model.project().setTrackMode(0, Track::TrackMode::Fractal);
    FractalTrack &track = model.project().track(0).fractalTrack();
    FractalSequence &seq = track.sequence(0);
    seq.setLoopFirst(loopFirst);
    seq.setLoopLast(loopLast);
    seq.setRotate(0);
    seq.setBranchCount(0);
    seq.setResetMeasure(0);
    seq.setOrderMode(orderMode);
    return seq;
}

std::vector<int> readOrder(FractalTrackEngine &engine, int count) {
    std::vector<int> order;
    engine.reset();
    for (int i = 0; i < count; ++i) {
        engine.replaySectionForTest(0, 48);
        order.push_back(engine.readPos());
    }
    return order;
}

} // namespace

UNIT_TEST("FractalTrackEngine") {

CASE("order_forward_reads_ascending") {
    Model model;
    setupLoop(model, 2, 6, FractalSequence::OrderMode::Forward);
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    // Characterization: Forward reads trunk indices 2,3,4,5,6,2,... in order.
    auto order = readOrder(fractal, 7);
    std::vector<int> expected = { 2, 3, 4, 5, 6, 2, 3 };
    for (size_t i = 0; i < expected.size(); ++i) {
        expectEqual(order[i], expected[i], "Forward must read ascending trunk indices");
    }
}

CASE("order_reverse_reads_descending") {
    Model model;
    setupLoop(model, 2, 6, FractalSequence::OrderMode::Reverse);
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    // Reverse must read trunk indices 6,5,4,3,2,6,... (descending within the loop).
    auto order = readOrder(fractal, 7);
    std::vector<int> expected = { 6, 5, 4, 3, 2, 6, 5 };
    for (size_t i = 0; i < expected.size(); ++i) {
        expectEqual(order[i], expected[i], "Reverse must read descending trunk indices");
    }
}

CASE("order_converge_interleaves_ends") {
    Model model;
    setupLoop(model, 0, 7, FractalSequence::OrderMode::Converge);
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    // Converge reads first, last, first+1, last-1, ... (ported sim order code 4).
    auto order = readOrder(fractal, 8);
    std::vector<int> expected = { 0, 7, 1, 6, 2, 5, 3, 4 };
    for (size_t i = 0; i < expected.size(); ++i) {
        expectEqual(order[i], expected[i], "Converge must interleave loop ends inward");
    }
}

CASE("branch_determinism_same_seed_same_segment") {
    // Swap 1/2: branch transforms now draw from a Random seeded by branchSeed.
    // Same seed → identical resolved branch segment on every rebuild.
    Model model;
    FractalSequence &seq = setupLoop(model, 0, 7, FractalSequence::OrderMode::Forward);
    seq.setBranchCount(3);
    seq.setBranchPool(0xff);
    seq.setBranchSeed(1234);
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    for (int i = 0; i < 8; ++i)
        fractal.setTrunkCellForTest(i, FractalTrackEngine::encodeCell(float(i), 8, true));

    fractal.rebuildBranchTransformsForTest();
    std::vector<float> first;
    for (int p = 0; p < 8; ++p) first.push_back(fractal.segmentSemiForTest(1, p));

    // Disturb with a different seed, then rebuild the original — must reproduce.
    seq.setBranchSeed(999);
    fractal.rebuildBranchTransformsForTest();
    seq.setBranchSeed(1234);
    fractal.rebuildBranchTransformsForTest();
    for (int p = 0; p < 8; ++p)
        expectEqual(std::abs(fractal.segmentSemiForTest(1, p) - first[p]) < 1e-4f, true,
                    "Same branchSeed must resolve identical branch segment");
}

CASE("branch_pool_restriction_honored") {
    // Swap 1/2: with the pool restricted to one op, every branch resolves to it.
    Model model;
    FractalSequence &seq = setupLoop(model, 0, 7, FractalSequence::OrderMode::Forward);
    seq.setBranchCount(7);
    seq.setBranchPool(1 << kOpReverse);   // Reverse only
    seq.setBranchSeed(42);
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);
    fractal.rebuildBranchTransformsForTest();
    for (int b = 1; b <= 7; ++b)
        expectEqual(int(fractal.branchKindForTest(b)), int(kOpReverse),
                    "Restricted pool must only draw the enabled op");
}

CASE("track_delay_surfaces_three_sections_later") {
    // Swap 3: the DelayEntry ring is now a RingBuffer; delay=3 still holds a note
    // for three sections, then surfaces it with the same playback CV.
    Model model;
    FractalSequence &seq = setupLoop(model, 0, 7, FractalSequence::OrderMode::Forward);
    seq.setOrnFirst(1); seq.setOrnLast(0);   // ornaments disabled → plain note
    Track &track = model.project().track(0);
    FractalTrack &ftrack = track.fractalTrack();
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    for (int i = 0; i < 8; ++i)
        fractal.setTrunkCellForTest(i, FractalTrackEngine::encodeCell(float(i + 1), 8, true));
    const uint32_t div = 12;

    // delay 0: schedules section 0's note immediately.
    ftrack.setTrackDelay(0);
    fractal.reset();
    for (int i = 0; i < 8; ++i)
        fractal.setTrunkCellForTest(i, FractalTrackEngine::encodeCell(float(i + 1), 8, true));
    fractal.replaySectionForTest(0, div);
    expectEqual(fractal.cvEventCountForTest() > 0, true, "delay 0 schedules immediately");
    float cv0 = fractal.cvEventForTest(0);

    // delay 3: first three sections defer, fourth surfaces section 0's note.
    ftrack.setTrackDelay(3);
    fractal.reset();
    for (int i = 0; i < 8; ++i)
        fractal.setTrunkCellForTest(i, FractalTrackEngine::encodeCell(float(i + 1), 8, true));
    fractal.replaySectionForTest(0, div);
    expectEqual(int(fractal.delayPendingForTest()), 1, "section 1 deferred");
    fractal.replaySectionForTest(0, div);
    expectEqual(int(fractal.delayPendingForTest()), 2, "section 2 deferred");
    fractal.replaySectionForTest(0, div);
    expectEqual(int(fractal.delayPendingForTest()), 3, "section 3 deferred");
    fractal.replaySectionForTest(0, div);
    expectEqual(fractal.cvEventCountForTest() > 0, true, "4th section surfaces note");
    expectEqual(std::abs(fractal.cvEventForTest(0) - cv0) < 1e-4f, true,
                "surfaced note carries the original playback CV");
}

CASE("scale_snap_uses_noteFromVolts") {
    // Swap 4: nearestDegree now delegates to Scale::noteFromVolts (floor-style).
    // Edge shift vs the old round-to-nearest scan: notes exactly halfway between
    // two degrees (e.g. 1.5 semis in Major) now snap down to the lower degree.
    // In-scale notes and non-midpoint notes are unchanged. Affects only ornament
    // grace/turn notes (stepDegrees), never the raw trunk main note.
    Model model;
    setupLoop(model, 0, 7, FractalSequence::OrderMode::Forward);
    model.project().setScale(1);   // Major
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    const Scale &scale = Scale::get(1);
    const float perOct = 12.f;
    // A range of off-scale and in-scale semitones must match noteFromVolts.
    for (float semi = -24.f; semi <= 24.f; semi += 1.f) {
        int expected = scale.noteFromVolts(semi / perOct);
        expectEqual(fractal.nearestDegreeForTest(semi), expected,
                    "nearestDegree must equal Scale::noteFromVolts");
    }
    // In-scale C (degree 0) snaps to 0; a Major scale degree (E = 4 semis) snaps
    // to its own degree, not below it.
    expectEqual(fractal.nearestDegreeForTest(0.f), scale.noteFromVolts(0.f),
                "tonic snaps to degree 0");
}

CASE("observe_over_section_gate_length_proportional") {
    // KD-1: gateLen comes from accumulated gate-high ticks over the section, not
    // a fixed 8. A held gate (full section high) → 15; a brief gate (≤18%) → 1; a
    // half-held gate → a proportional middle value; never high → rest (0).
    using FTE = FractalTrackEngine;
    const int sec = 48;
    expectEqual(FTE::gateLenFromObservation(48, sec), 15, "held gate → full");
    expectEqual(FTE::gateLenFromObservation(0, sec), 0, "silent → rest");
    expectEqual(FTE::gateLenFromObservation(4, sec), 1, "very short → trig");
    int half = FTE::gateLenFromObservation(24, sec);
    expectEqual(half >= 2 && half <= 14, true, "half-held → proportional middle");
    // A longer held gate must yield a strictly larger gateLen than a short one.
    expectEqual(FTE::gateLenFromObservation(36, sec) > FTE::gateLenFromObservation(12, sec),
                true, "longer hold → larger gateLen");
}

CASE("onset_phase_maps_first_edge_to_nibble") {
    // KD-14b: the first rising-edge tick maps to a 4-bit onset phase over the
    // section. Edge at the start → 0; edge at the midpoint → ~8; late → toward 15.
    using FTE = FractalTrackEngine;
    const int sec = 48;
    expectEqual(FTE::onsetNibbleFromObservation(0, sec), 0, "edge at start → 0");
    expectEqual(FTE::onsetNibbleFromObservation(-1, sec), 0, "no edge → 0");
    expectEqual(FTE::onsetNibbleFromObservation(24, sec), 8, "edge at mid → 8");
    int late = FTE::onsetNibbleFromObservation(45, sec);
    expectEqual(late >= 14 && late <= 15, true, "late edge → near 15");
}

CASE("feel_onset_side_array_packs_two_per_byte") {
    // KD-14b: the onset side-array holds a 4-bit phase per cell, packed two cells
    // per byte. Round-trip the low and high nibble independently; Quantized cells
    // (never written) read 0.
    Model model;
    setupLoop(model, 0, 7, FractalSequence::OrderMode::Forward);
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    expectEqual(fractal.onsetNibbleForTest(3), 0, "unwritten onset reads 0");
    fractal.setOnsetNibbleForTest(4, 11);   // even cell → low nibble
    fractal.setOnsetNibbleForTest(5, 6);    // odd cell  → high nibble (same byte)
    expectEqual(fractal.onsetNibbleForTest(4), 11, "even cell round-trips");
    expectEqual(fractal.onsetNibbleForTest(5), 6, "odd cell round-trips");
    // Writing one nibble must not corrupt its byte-mate.
    fractal.setOnsetNibbleForTest(4, 2);
    expectEqual(fractal.onsetNibbleForTest(5), 6, "byte-mate nibble preserved");
}

CASE("feel_onset_delays_replay_gate") {
    // KD-14b replay: in Feel, the sounding cell's gate is delayed by its onset
    // phase (onset/16 of the section). Quantized (onset 0) fires at tick 0.
    Model model;
    FractalSequence &seq = setupLoop(model, 0, 7, FractalSequence::OrderMode::Forward);
    seq.setOrnFirst(1); seq.setOrnLast(0);   // ornaments off → plain note
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);
    const uint32_t div = 48;

    // gate-on is the first gate edge scheduleSection pushes (index 0).
    auto firstGateOnTick = [&]() { return fractal.gateEventTickForTest(0); };

    // Quantized: onset array is 0 → gate at section tick 0.
    seq.setCaptureFidelity(FractalSequence::CaptureFidelity::Quantized);
    fractal.reset();
    for (int i = 0; i < 8; ++i)
        fractal.setTrunkCellForTest(i, FractalTrackEngine::encodeCell(float(i + 1), 8, true));
    fractal.replaySectionForTest(0, div);
    expectEqual(int(firstGateOnTick()), 0, "Quantized fires at the section start");

    // Feel: trunk index 0 gets onset nibble 8 → half-section delay (48*8/16 = 24).
    seq.setCaptureFidelity(FractalSequence::CaptureFidelity::Feel);
    fractal.reset();
    for (int i = 0; i < 8; ++i)
        fractal.setTrunkCellForTest(i, FractalTrackEngine::encodeCell(float(i + 1), 8, true));
    fractal.setOnsetNibbleForTest(0, 8);
    fractal.replaySectionForTest(0, div);
    expectEqual(int(firstGateOnTick()), 24, "Feel delays the gate by the onset phase");
}

CASE("queued_ornament_forces_next_roll") {
    // Hero Ornament-page punch-in: a queued ornament fires on the next roll even
    // with rate=0 and lock on, then clears so subsequent rolls run normally.
    Model model;
    setupLoop(model, 0, 7, FractalSequence::OrderMode::Forward);
    Track &track = model.project().track(0);
    FractalTrack &ftrack = track.fractalTrack();
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    ftrack.setLock(true);
    fractal.queueOrnament(9);
    expectEqual(fractal.hasQueuedOrnament(), true, "queue sets the pending flag");
    expectEqual(fractal.rollOrnamentForTest(0, 100), 9, "forced punch ignores rate=0 and lock");
    expectEqual(fractal.hasQueuedOrnament(), false, "forced punch clears the queue");
    expectEqual(fractal.rollOrnamentForTest(0, 100), -1, "after punch, rate=0 rolls none");

    // queueSegment is reachable (capture/replay jump is build-verified).
    fractal.queueSegment(2);
}

} // UNIT_TEST("FractalTrackEngine")
