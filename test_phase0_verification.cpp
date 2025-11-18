// Phase 0 Verification Test - Standalone compilation check
// This file verifies Gate struct compiles correctly with flag=0 and flag=1

#include <cstdint>

// Simulate the Config.h flag
#ifndef CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
#define CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS 0  // Test with flag=0 first
#endif

// Minimal NoteTrackEngine Gate struct (copied from NoteTrackEngine.h)
class NoteTrackEngine {
public:
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    static constexpr uint8_t MainSequenceId = 0;
    static constexpr uint8_t FillSequenceId = 1;
#endif

    struct Gate {
        uint32_t tick;
        bool gate;
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
        bool shouldTickAccumulator;
        uint8_t sequenceId;
#endif
    };
};

// Test code
int main() {
    // Test 1: Basic gate creation (should work with flag=0 and flag=1)
    NoteTrackEngine::Gate gate1;
    gate1.tick = 100;
    gate1.gate = true;

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // Test 2: Experimental fields (only when flag=1)
    gate1.shouldTickAccumulator = true;
    gate1.sequenceId = NoteTrackEngine::MainSequenceId;

    NoteTrackEngine::Gate gate2 = { 200, false, true, NoteTrackEngine::FillSequenceId };

    // Test 3: Size check
    static_assert(sizeof(NoteTrackEngine::Gate) <= 16, "Gate struct too large");

    return (gate2.sequenceId == 1) ? 0 : 1;
#else
    // Test 2: Without experimental fields (flag=0)
    NoteTrackEngine::Gate gate2 = { 200, false };

    // Test 3: Size check (should be 8 bytes)
    static_assert(sizeof(NoteTrackEngine::Gate) == 8, "Gate struct should be 8 bytes with flag=0");

    return (gate2.gate == false) ? 0 : 1;
#endif
}
