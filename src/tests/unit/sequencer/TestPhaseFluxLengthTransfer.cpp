#include "UnitTest.h"

#include "apps/sequencer/model/PhaseFluxMath.h"

UNIT_TEST("PhaseFluxLengthTransfer") {

CASE("conserves_sum_on_normal_transfer") {
    int a = 10, b = 10;
    int d = PhaseFluxMath::transferLength(a, b, +3, 1, 127);
    expectEqual(d, 3, "applied delta is +3");
    expectEqual(a, 13, "source gains 3");
    expectEqual(b, 7, "neighbor loses 3");
    expectEqual(a + b, 20, "sum conserved");
}

CASE("clamps_at_source_ceiling") {
    int a = 126, b = 50;
    int d = PhaseFluxMath::transferLength(a, b, +10, 1, 127);
    expectEqual(d, 1, "delta clamped so source stops at 127");
    expectEqual(a, 127, "source pinned at max");
    expectEqual(b, 49, "neighbor gave only 1");
    expectEqual(a + b, 176, "sum conserved");
}

CASE("clamps_at_neighbor_floor") {
    int a = 50, b = 3;
    int d = PhaseFluxMath::transferLength(a, b, +10, 1, 127);
    expectEqual(d, 2, "delta clamped so neighbor stops at 1");
    expectEqual(a, 52, "source took only 2");
    expectEqual(b, 1, "neighbor pinned at min");
    expectEqual(a + b, 53, "sum conserved");
}

CASE("negative_delta_pulls_back") {
    int a = 20, b = 20;
    int d = PhaseFluxMath::transferLength(a, b, -5, 1, 127);
    expectEqual(d, -5, "applied delta is -5");
    expectEqual(a, 15, "source loses 5");
    expectEqual(b, 25, "neighbor gains 5");
    expectEqual(a + b, 40, "sum conserved");
}

CASE("nearest_note_value_snaps_to_powers_of_two") {
    expectEqual(PhaseFluxMath::nearestNoteValue(1), 1, "1 -> 1");
    expectEqual(PhaseFluxMath::nearestNoteValue(2), 2, "2 -> 2");
    expectEqual(PhaseFluxMath::nearestNoteValue(4), 4, "4 -> 4");
    expectEqual(PhaseFluxMath::nearestNoteValue(5), 4, "5 -> 4 (closer to 4)");
    expectEqual(PhaseFluxMath::nearestNoteValue(7), 8, "7 -> 8 (closer to 8)");
    expectEqual(PhaseFluxMath::nearestNoteValue(64), 64, "64 -> 64");
    expectEqual(PhaseFluxMath::nearestNoteValue(120), 64, "120 -> 64 (capped)");
}

CASE("nearest_note_value_ties_round_up") {
    expectEqual(PhaseFluxMath::nearestNoteValue(3), 4, "3 -> 4 (tie up)");
    expectEqual(PhaseFluxMath::nearestNoteValue(6), 8, "6 -> 8 (tie up)");
    expectEqual(PhaseFluxMath::nearestNoteValue(12), 16, "12 -> 16 (tie up)");
}

CASE("nearest_note_value_clamps_low") {
    expectEqual(PhaseFluxMath::nearestNoteValue(0), 1, "0 clamps to 1");
    expectEqual(PhaseFluxMath::nearestNoteValue(-5), 1, "negative clamps to 1");
}

} // UNIT_TEST("PhaseFluxLengthTransfer")
