#include "UnitTest.h"

#include "apps/sequencer/engine/DiscreteMapTrackEngine.h"

UNIT_TEST("DiscreteMapTrackEngine") {

CASE("header_compiles") {
    expectTrue(true, "header available");
}

CASE("track_mode_constant") {
    expectEqual(static_cast<int>(Track::TrackMode::DiscreteMap), 4, "DiscreteMap mode enum value");
}

} // UNIT_TEST
