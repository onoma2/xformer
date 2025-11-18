#include "catch.hpp"
#include "apps/sequencer/engine/NoteTrackEngine.h"
#include "apps/sequencer/Config.h"

TEST_CASE("Gate struct basic fields") {
    SECTION("stores tick and gate values") {
        NoteTrackEngine::Gate gate;
        gate.tick = 100;
        gate.gate = true;

        REQUIRE(gate.tick == 100);
        REQUIRE(gate.gate == true);
    }

    SECTION("stores different tick values") {
        NoteTrackEngine::Gate gate;
        gate.tick = 48;
        gate.gate = false;

        REQUIRE(gate.tick == 48);
        REQUIRE(gate.gate == false);
    }
}

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS

TEST_CASE("Gate struct experimental spread-ticks fields") {
    SECTION("stores shouldTickAccumulator flag") {
        NoteTrackEngine::Gate gate;
        gate.tick = 100;
        gate.gate = true;
        gate.shouldTickAccumulator = true;
        gate.sequenceId = 0;

        REQUIRE(gate.tick == 100);
        REQUIRE(gate.gate == true);
        REQUIRE(gate.shouldTickAccumulator == true);
        REQUIRE(gate.sequenceId == 0);
    }

    SECTION("shouldTickAccumulator defaults to false") {
        NoteTrackEngine::Gate gate = { 100, true };

        // With 2-arg constructor, should default to false
        REQUIRE(gate.shouldTickAccumulator == false);
        REQUIRE(gate.sequenceId == 0);
    }

    SECTION("stores sequenceId correctly") {
        NoteTrackEngine::Gate mainGate;
        mainGate.tick = 100;
        mainGate.gate = true;
        mainGate.shouldTickAccumulator = false;
        mainGate.sequenceId = 0;  // MainSequenceId

        NoteTrackEngine::Gate fillGate;
        fillGate.tick = 200;
        fillGate.gate = true;
        fillGate.shouldTickAccumulator = true;
        fillGate.sequenceId = 1;  // FillSequenceId

        REQUIRE(mainGate.sequenceId == 0);
        REQUIRE(fillGate.sequenceId == 1);
        REQUIRE(mainGate.sequenceId != fillGate.sequenceId);
    }

    SECTION("4-arg constructor sets all fields") {
        NoteTrackEngine::Gate gate = { 100, true, true, 1 };

        REQUIRE(gate.tick == 100);
        REQUIRE(gate.gate == true);
        REQUIRE(gate.shouldTickAccumulator == true);
        REQUIRE(gate.sequenceId == 1);
    }

    SECTION("Gate struct size is acceptable") {
        // Ensure struct doesn't exceed memory constraints
        // With experimental fields: tick(4) + gate(1) + shouldTickAccum(1) + seqId(1) + padding(1) = 8 bytes
        // Could be up to 12 bytes with different alignment
        REQUIRE(sizeof(NoteTrackEngine::Gate) <= 16);
    }
}

TEST_CASE("Gate struct sequence ID constants") {
    SECTION("MainSequenceId is 0") {
        REQUIRE(NoteTrackEngine::MainSequenceId == 0);
    }

    SECTION("FillSequenceId is 1") {
        REQUIRE(NoteTrackEngine::FillSequenceId == 1);
    }

    SECTION("Main and Fill IDs are different") {
        REQUIRE(NoteTrackEngine::MainSequenceId != NoteTrackEngine::FillSequenceId);
    }
}

#endif // CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS

#if !CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS

TEST_CASE("Gate struct without experimental features") {
    SECTION("Gate struct has minimal size") {
        // Without experimental fields: tick(4) + gate(1) + padding(3) = 8 bytes
        REQUIRE(sizeof(NoteTrackEngine::Gate) == 8);
    }

    SECTION("Basic 2-arg construction works") {
        NoteTrackEngine::Gate gate = { 100, true };

        REQUIRE(gate.tick == 100);
        REQUIRE(gate.gate == true);
    }
}

#endif // CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
