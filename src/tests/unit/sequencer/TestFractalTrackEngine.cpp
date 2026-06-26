#include "UnitTest.h"

#include "apps/sequencer/engine/FractalTrackEngine.h"
#include "apps/sequencer/engine/Engine.h"
#include "apps/sequencer/model/FractalSequence.h"
#include "apps/sequencer/model/FractalTrack.h"
#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/Model.h"
#include "apps/sequencer/Config.h"

#include <new>
#include <vector>

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

} // UNIT_TEST("FractalTrackEngine")
