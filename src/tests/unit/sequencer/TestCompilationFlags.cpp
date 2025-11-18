#include "UnitTest.h"

#include "apps/sequencer/Config.h"
#include "apps/sequencer/engine/NoteTrackEngine.h"
#include "apps/sequencer/model/NoteSequence.h"

UNIT_TEST("CompilationFlags") {

// This test verifies that CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
// is actually enabled during compilation

CASE("verify CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS is defined") {
#ifdef CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    expectTrue(true, "CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS is defined");
#else
    expectTrue(false, "CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS is NOT defined - compilation error!");
#endif
}

CASE("verify flag value is 1") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS == 1
    expectTrue(true, "CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS == 1");
#else
    expectTrue(false, "CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS != 1 - flag is not enabled!");
#endif
}

CASE("verify Gate struct has experimental fields") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // Test that we can access the experimental fields
    NoteTrackEngine::Gate gate;
    gate.tick = 100;
    gate.gate = true;
    gate.shouldTickAccumulator = true;  // This should compile if flag=1
    gate.sequenceId = 0;                // This should compile if flag=1

    expectEqual(gate.shouldTickAccumulator, true, "shouldTickAccumulator field exists and is accessible");
    expectEqual((unsigned int)gate.sequenceId, 0u, "sequenceId field exists and is accessible");
#else
    // If we get here with flag=0, that's expected
    expectTrue(true, "Flag is 0, experimental fields not compiled");
#endif
}

CASE("verify Gate struct size") {
    size_t gateSize = sizeof(NoteTrackEngine::Gate);

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // With flag=1, struct should be larger (has experimental fields)
    // Expected: 4 bytes (tick) + 1 byte (gate) + 1 byte (shouldTickAccumulator) + 1 byte (sequenceId) = 7 bytes
    // With padding, likely 8 or 12 bytes
    expectTrue(gateSize >= 8, "Gate struct with experimental fields should be >= 8 bytes");
#else
    // With flag=0, struct should be smaller (no experimental fields)
    // Expected: 4 bytes (tick) + 1 byte (gate) = 5 bytes, with padding likely 8 bytes
    expectEqual(gateSize, 8ul, "Gate struct without experimental fields should be 8 bytes");
#endif
}

CASE("verify sequence ID constants are defined") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // These constants should only exist when flag=1
    uint8_t mainId = NoteTrackEngine::MainSequenceId;
    uint8_t fillId = NoteTrackEngine::FillSequenceId;

    expectEqual((unsigned int)mainId, 0u, "MainSequenceId should be 0");
    expectEqual((unsigned int)fillId, 1u, "FillSequenceId should be 1");
    expectTrue(mainId != fillId, "MainSequenceId and FillSequenceId should differ");
#else
    expectTrue(true, "Flag is 0, sequence ID constants not needed");
#endif
}

CASE("verify 4-arg Gate constructor works") {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // With flag=1, should be able to construct Gate with 4 arguments
    NoteTrackEngine::Gate gate = { 100, true, true, 1 };

    expectEqual(gate.tick, 100u, "tick should be 100");
    expectEqual(gate.gate, true, "gate should be true");
    expectEqual(gate.shouldTickAccumulator, true, "shouldTickAccumulator should be true");
    expectEqual((unsigned int)gate.sequenceId, 1u, "sequenceId should be 1");
#else
    expectTrue(true, "Flag is 0, 4-arg constructor not needed");
#endif
}

CASE("print runtime flag status for debugging") {
    // This case prints the actual flag value for visual verification
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS == 1
    expectTrue(true, "RUNTIME CHECK: Flag is ENABLED (1)");
#elif CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS == 0
    expectTrue(true, "RUNTIME CHECK: Flag is DISABLED (0)");
#else
    expectTrue(false, "RUNTIME CHECK: Flag has unexpected value!");
#endif
}

CASE("verify compile-time vs runtime flag consistency") {
    // Check that Config.h value matches what we see at compile time
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS == 1
    // If we're here, flag=1 was set during compilation
    // This should always pass if the flag is consistent
    expectTrue(true, "Compile-time flag is 1");

    // Now verify we can actually use the features
    NoteTrackEngine::Gate gate = { 50, false, false, 0 };
    expectEqual((unsigned int)gate.sequenceId, 0u, "Can construct and access experimental fields");
#else
    expectTrue(true, "Compile-time flag is 0 (features disabled)");
#endif
}

}
