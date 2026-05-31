#pragma once

#include "drivers/HighResolutionTimer.h"

#include <cstdint>

// 32-bit microsecond free-running wall clock. Wraps the existing high-res µs
// source, matching the engine's uint32_t µs convention (Clock::_elapsedUs).
// Independent of play/stop and tempo.
class WallClock {
public:
    uint32_t now() const { return HighResolutionTimer::us(); }
};

// Absolute-deadline timer: drift-free (restart accumulates from the prior
// deadline) and wrap-safe (signed difference, never now > end). All comparisons
// use (int32_t)(now - endUs) so a 32-bit µs wrap (~71 min) is harmless.
struct WallTimer {
    uint32_t endUs = 0;
    bool running = false;

    void schedule(uint32_t deadlineUs) { endUs = deadlineUs; running = true; }

    void start(uint32_t nowUs, uint32_t delayUs) { endUs = nowUs + delayUs; running = true; }
    void start(WallClock &clock, uint32_t delayUs) { start(clock.now(), delayUs); }

    // True once when the deadline has passed; disarms until re-scheduled.
    bool elapsed(uint32_t nowUs) {
        if (running && (int32_t)(nowUs - endUs) >= 0) {
            running = false;
            return true;
        }
        return false;
    }
    bool elapsed(WallClock &clock) { return elapsed(clock.now()); }

    // Re-arm relative to the previous deadline, not the service moment.
    void restart(uint32_t delayUs) { endUs += delayUs; running = true; }
};
