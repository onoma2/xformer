#pragma once

#include <cstdint>

class TeletypeTrackEngine;

class TeletypeBridge {
public:
    class ScopedEngine {
    public:
        explicit ScopedEngine(TeletypeTrackEngine &engine);
        ~ScopedEngine();

    private:
        TeletypeTrackEngine *_prev = nullptr;
    };

    static TeletypeTrackEngine *activeEngine();
    static void setActiveEngine(TeletypeTrackEngine *engine);
    static uint32_t ticksMs();
    static bool hasDelays();
    static bool hasStack();
    static int dashboardScreen();
};
