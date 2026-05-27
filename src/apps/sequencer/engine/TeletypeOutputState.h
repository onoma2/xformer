#pragma once

#include <cstdint>
#include <cstring>

static constexpr int TT2_OUTPUT_CV_COUNT = 8;
static constexpr int TT2_OUTPUT_TR_COUNT = 8;

struct TT2CvOutput {
    int16_t targetRaw;
};

struct TT2TrOutput {
    uint8_t level;
};

struct TT2OutputState {
    TT2CvOutput cv[TT2_OUTPUT_CV_COUNT];
    TT2TrOutput tr[TT2_OUTPUT_TR_COUNT];
    uint8_t cvDirty;
    uint8_t trDirty;
};

inline void init(TT2OutputState &o) {
    memset(&o, 0, sizeof(TT2OutputState));
}

static_assert(sizeof(TT2CvOutput) == 2, "TT2CvOutput size drift");
static_assert(sizeof(TT2TrOutput) == 1, "TT2TrOutput size drift");
static_assert(sizeof(TT2OutputState) == 26, "TT2OutputState size drift");
