// Minimal compilation test for flag=1
#define CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS 1

#include <cstdint>

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

    void testMethod() {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
        // Test using the constants
        uint8_t seqId = true ? FillSequenceId : MainSequenceId;

        // Test creating gates with 4 args
        Gate g1 = { 100, true, true, seqId };
        Gate g2 = { 200, false, false, MainSequenceId };
#endif
    }
};

int main() {
    NoteTrackEngine engine;
    engine.testMethod();
    return 0;
}
