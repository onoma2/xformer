#include "UnitTest.h"

#include "apps/sequencer/engine/StochasticTrackEngine.h"
#include <cstddef>

UNIT_TEST("StochasticDurationDictionary") {

// V5 LUT — divisor-relative multipliers, sorted descending by duration.
// Slot ordering: ×8, ×4, ×3, ×2, ×4/3, ×1, ×2/3, ×1/2.
// Labels assume divisor = 1/16 (12 ticks at PPQN=48 → 48 at PPQN=192).
// getDurationMultiplier() returns absolute ticks at PPQN=192 assuming
// divisor=1/16 (legacy helper for tests + back-compat).

CASE("entry_0_is_half") {
    auto f = StochasticTrackEngine::getDurationFraction(0);
    expectEqual(int(f.num), 8, "entry 0 num=8");
    expectEqual(int(f.den), 1, "entry 0 den=1");
    expectEqual(StochasticTrackEngine::getDurationMultiplier(0), uint32_t(384), "entry 0 = 384 ticks (1/2 @ divisor=1/16)");
}

CASE("entry_1_is_quarter") {
    auto f = StochasticTrackEngine::getDurationFraction(1);
    expectEqual(int(f.num), 4, "entry 1 num=4");
    expectEqual(int(f.den), 1, "entry 1 den=1");
    expectEqual(StochasticTrackEngine::getDurationMultiplier(1), uint32_t(192), "entry 1 = 192 ticks (1/4 @ divisor=1/16)");
}

CASE("entry_2_is_dotted_eighth") {
    auto f = StochasticTrackEngine::getDurationFraction(2);
    expectEqual(int(f.num), 3, "entry 2 num=3");
    expectEqual(int(f.den), 1, "entry 2 den=1");
    expectEqual(StochasticTrackEngine::getDurationMultiplier(2), uint32_t(144), "entry 2 = 144 ticks (3/16 @ divisor=1/16)");
}

CASE("entry_3_is_eighth") {
    auto f = StochasticTrackEngine::getDurationFraction(3);
    expectEqual(int(f.num), 2, "entry 3 num=2");
    expectEqual(int(f.den), 1, "entry 3 den=1");
    expectEqual(StochasticTrackEngine::getDurationMultiplier(3), uint32_t(96), "entry 3 = 96 ticks (1/8 @ divisor=1/16)");
}

CASE("entry_4_is_eighth_triplet") {
    auto f = StochasticTrackEngine::getDurationFraction(4);
    expectEqual(int(f.num), 4, "entry 4 num=4");
    expectEqual(int(f.den), 3, "entry 4 den=3");
    expectEqual(StochasticTrackEngine::getDurationMultiplier(4), uint32_t(64), "entry 4 = 64 ticks (1/8T @ divisor=1/16)");
}

CASE("entry_5_is_sixteenth_base") {
    auto f = StochasticTrackEngine::getDurationFraction(5);
    expectEqual(int(f.num), 1, "entry 5 num=1");
    expectEqual(int(f.den), 1, "entry 5 den=1 (= divisor)");
    expectEqual(StochasticTrackEngine::getDurationMultiplier(5), uint32_t(48), "entry 5 = 48 ticks (1/16 @ divisor=1/16)");
}

CASE("entry_6_is_sixteenth_triplet") {
    auto f = StochasticTrackEngine::getDurationFraction(6);
    expectEqual(int(f.num), 2, "entry 6 num=2");
    expectEqual(int(f.den), 3, "entry 6 den=3");
    expectEqual(StochasticTrackEngine::getDurationMultiplier(6), uint32_t(32), "entry 6 = 32 ticks (1/16T @ divisor=1/16)");
}

CASE("entry_7_is_thirtysecond") {
    auto f = StochasticTrackEngine::getDurationFraction(7);
    expectEqual(int(f.num), 1, "entry 7 num=1");
    expectEqual(int(f.den), 2, "entry 7 den=2");
    expectEqual(StochasticTrackEngine::getDurationMultiplier(7), uint32_t(24), "entry 7 = 24 ticks (1/32 @ divisor=1/16)");
}

CASE("lut_is_descending") {
    // Each entry strictly shorter than the previous.
    auto prev = StochasticTrackEngine::getDurationMultiplier(0);
    for (int i = 1; i < 8; ++i) {
        auto cur = StochasticTrackEngine::getDurationMultiplier(i);
        expect(cur < prev, "LUT must descend by ticks");
        prev = cur;
    }
}

CASE("direct_history_event_is_compact_ui_truth") {
    StochasticTrackEngine::DirectHistoryEvent event;
    event.cv = 1.25f;
    event.children = 3;
    event.rest = false;
    event.gate = true;

    expectTrue(sizeof(StochasticTrackEngine::DirectHistoryEvent) <= 8,
               "Direct history event should stay compact");
    expectEqual(int(StochasticTrackEngine::kDirectHistoryMax), 12,
                "Direct history should match the hero trail length");
    expectEqual(event.children, uint8_t(3), "children count should be stored");
    expectTrue(event.gate, "gate truth should be stored");
}

} // UNIT_TEST("StochasticDurationDictionary")
