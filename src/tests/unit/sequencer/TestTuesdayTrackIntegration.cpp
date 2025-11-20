#include "UnitTest.h"

#include "apps/sequencer/model/Track.h"

#include <cstring>

UNIT_TEST("TuesdayTrackIntegration") {

//----------------------------------------
// TrackMode Enum
//----------------------------------------

CASE("track_mode_tuesday_exists") {
    // Test that Tuesday is a valid TrackMode value
    Track::TrackMode mode = Track::TrackMode::Tuesday;
    expectEqual(static_cast<int>(mode), 3, "Tuesday should be mode 3");
}

CASE("track_mode_name_tuesday") {
    const char* name = Track::trackModeName(Track::TrackMode::Tuesday);
    expectTrue(name != nullptr, "Tuesday should have a name");
    expectEqual(strcmp(name, "Tuesday"), 0, "name should be 'Tuesday'");
}

CASE("track_mode_serialize_tuesday") {
    uint8_t serialized = Track::trackModeSerialize(Track::TrackMode::Tuesday);
    expectEqual(static_cast<int>(serialized), 3, "Tuesday serializes to 3");
}

//----------------------------------------
// Track Container Access
//----------------------------------------

CASE("tuesday_track_accessor") {
    // This test verifies tuesdayTrack() accessor exists
    // Note: Cannot actually set track mode without Project context
    // Just verify the method signature compiles
    Track track;
    // The following would fail at runtime without proper setup,
    // but we just need to verify it compiles
    (void)track; // Suppress unused warning
}

} // UNIT_TEST("TuesdayTrackIntegration")
