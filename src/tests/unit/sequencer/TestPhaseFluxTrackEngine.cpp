#include "UnitTest.h"

#include "apps/sequencer/engine/PhaseFluxTrackEngine.h"
#include "apps/sequencer/model/Track.h"

// Full tick-level testing requires Engine construction (hardware drivers).
// Gate/pulse timing is covered by TestPhaseFluxCumulativeTable and
// TestPhaseFluxPerTickDerivation. These cases verify static invariants.

UNIT_TEST("PhaseFluxTrackEngine") {

CASE("track_mode_constant") {
    expectEqual(static_cast<int>(Track::TrackMode::PhaseFlux), 8, "PhaseFlux serialize id is 8");
}

CASE("stage_count_parity") {
    expectEqual(PhaseFluxTrackEngine::kStageCount, 16, "engine kStageCount == 16");
}

CASE("max_pulses_constant") {
    expectEqual(PhaseFluxTrackEngine::kMaxPulses, 8, "kMaxPulses == 8");
}

} // UNIT_TEST("PhaseFluxTrackEngine")
