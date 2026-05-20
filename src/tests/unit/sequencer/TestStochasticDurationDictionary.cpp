#include "UnitTest.h"

#include "apps/sequencer/engine/StochasticTrackEngine.h"

UNIT_TEST("StochasticDurationDictionary") {

CASE("slot_0_is_quarter") {
    expectEqual(StochasticTrackEngine::getDurationMultiplier(0), uint32_t(192), "slot 0 = 192 (1/4)");
}

CASE("slot_1_is_eighth") {
    expectEqual(StochasticTrackEngine::getDurationMultiplier(1), uint32_t(96), "slot 1 = 96 (1/8)");
}

CASE("slot_2_is_sixteenth") {
    expectEqual(StochasticTrackEngine::getDurationMultiplier(2), uint32_t(48), "slot 2 = 48 (1/16)");
}

CASE("slot_3_is_thirtysecond") {
    expectEqual(StochasticTrackEngine::getDurationMultiplier(3), uint32_t(24), "slot 3 = 24 (1/32)");
}

CASE("slot_4_is_sixtyfourth") {
    expectEqual(StochasticTrackEngine::getDurationMultiplier(4), uint32_t(12), "slot 4 = 12 (1/64)");
}

CASE("slot_5_is_eighth_triplet") {
    expectEqual(StochasticTrackEngine::getDurationMultiplier(5), uint32_t(64), "slot 5 = 64 (1/8T)");
}

CASE("slot_6_is_sixteenth_triplet") {
    expectEqual(StochasticTrackEngine::getDurationMultiplier(6), uint32_t(32), "slot 6 = 32 (1/16T)");
}

CASE("slot_7_is_dotted_eighth") {
    expectEqual(StochasticTrackEngine::getDurationMultiplier(7), uint32_t(144), "slot 7 = 144 (3/16)");
}

} // UNIT_TEST("StochasticDurationDictionary")
