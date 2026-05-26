#include "UnitTest.h"

#include "apps/sequencer/model/PhaseFluxTrack.h"
#include "apps/sequencer/model/PhaseFluxSequence.h"
#include "apps/sequencer/model/NoteTrack.h"
#include "apps/sequencer/model/StochasticTrack.h"

#include <cstdio>

UNIT_TEST("PhaseFluxSize") {

CASE("stage_record_remains_16_bytes_after_accumulator_v2") {
    // Locked layout (§13.3): 4 × uint32_t = 16 bytes. If a new field bumps
    // the Stage past 16 bytes, the entire PhaseFluxTrack envelope shifts.
    expectEqual(int(sizeof(PhaseFluxSequence::Stage)), 16, "Stage stays 16 bytes after accumulator v2 bit-pack rework");
}

CASE("print_sizes") {
    std::printf("\n--- model size probe ---\n");
    std::printf("PhaseFluxSequence::Stage = %zu bytes (4x uint32_t = 16 expected)\n",
                sizeof(PhaseFluxSequence::Stage));
    std::printf("PhaseFluxSequence        = %zu bytes\n", sizeof(PhaseFluxSequence));
    std::printf("PhaseFluxTrack           = %zu bytes  (envelope ceiling 9544)\n",
                sizeof(PhaseFluxTrack));
    std::printf("NoteTrack                = %zu bytes  (reference envelope)\n",
                sizeof(NoteTrack));
    std::printf("StochasticTrack          = %zu bytes\n", sizeof(StochasticTrack));
    std::printf("Stage + _data3 hypoth.   = %zu bytes\n",
                sizeof(PhaseFluxSequence::Stage) + 4);
    std::printf("PhaseFluxSequence + 16x4 = %zu bytes  (side-array option)\n",
                sizeof(PhaseFluxSequence) + 16 * 4);
    std::printf("PhaseFluxTrack + 8x(16x4) = %zu bytes  (side-array, all 8 patterns)\n",
                sizeof(PhaseFluxTrack) + 8 * 16 * 4);
    std::printf("Stage with _data3, full track = %zu bytes  (each pattern has 16 larger stages)\n",
                sizeof(PhaseFluxTrack) + 8 * 16 * 4);
    std::printf("--- end probe ---\n");

    // Always passes — this is a measurement, not an assertion.
    expectEqual(1, 1, "probe ran");
}

}
