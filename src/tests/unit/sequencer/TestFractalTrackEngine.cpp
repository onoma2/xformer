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

FractalSequence &setupLoop(Model &model, int loopFirst, int loopLast, Types::RunMode runMode) {
    model.project().setTrackMode(0, Track::TrackMode::Fractal);
    FractalTrack &track = model.project().track(0).fractalTrack();
    FractalSequence &seq = track.sequence(0);
    seq.setLoopFirst(loopFirst);
    seq.setLoopLast(loopLast);
    seq.setRotate(0);
    seq.setBranchCount(0);
    seq.setResetMeasure(0);
    seq.setRunMode(runMode);
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

CASE("runmode_forward_reads_ascending") {
    Model model;
    setupLoop(model, 2, 6, Types::RunMode::Forward);
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    // Characterization: Forward reads trunk indices 2,3,4,5,6,2,... in order.
    auto order = readOrder(fractal, 7);
    std::vector<int> expected = { 2, 3, 4, 5, 6, 2, 3 };
    for (size_t i = 0; i < expected.size(); ++i) {
        expectEqual(order[i], expected[i], "Forward must read ascending trunk indices");
    }
}

CASE("runmode_reverse_reads_descending") {
    Model model;
    setupLoop(model, 2, 6, Types::RunMode::Backward);
    Track &track = model.project().track(0);
    FractalTrackEngine fractal(placeholderEngine(), model, track);

    // Reverse must read trunk indices 6,5,4,3,2,6,... (descending within the loop).
    auto order = readOrder(fractal, 7);
    std::vector<int> expected = { 6, 5, 4, 3, 2, 6, 5 };
    for (size_t i = 0; i < expected.size(); ++i) {
        expectEqual(order[i], expected[i], "Reverse must read descending trunk indices");
    }
}

CASE("branch_determinism_same_seed_same_segment") {
    // Swap 1/2: branch transforms now draw from a Random seeded by branchSeed.
    // Same seed → identical resolved branch segment on every rebuild.
    Model model;
    FractalSequence &seq = setupLoop(model, 0, 7, Types::RunMode::Forward);
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
    FractalSequence &seq = setupLoop(model, 0, 7, Types::RunMode::Forward);
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
    FractalSequence &seq = setupLoop(model, 0, 7, Types::RunMode::Forward);
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
    setupLoop(model, 0, 7, Types::RunMode::Forward);
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

} // UNIT_TEST("FractalTrackEngine")
