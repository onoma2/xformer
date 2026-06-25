#pragma once

#include <cstdint>

struct TT2ConfigFull {
    static constexpr int ScriptCount       = 10;
    static constexpr int DelayDepth        = 64;
    static constexpr int TriggerInputCount = 8;
    static constexpr int MetroScript       = 8;
    static constexpr int InitScript        = 9;
    static constexpr int SceneCount        = 1;
    static constexpr int PatternCount      = 4;
    static constexpr int PatternLength     = 64;
};

struct TT2ConfigMini {
    static constexpr int ScriptCount       = 3;
    static constexpr int DelayDepth        = 8;
    static constexpr int TriggerInputCount = 2;
    static constexpr int MetroScript        = 2;
    static constexpr int InitScript         = -1;
    static constexpr int SceneCount         = 4;
    static constexpr int PatternCount      = 4;
    static constexpr int PatternLength     = 64;
};
