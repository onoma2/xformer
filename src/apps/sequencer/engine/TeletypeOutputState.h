#pragma once

#include <cstdint>
#include <cstring>

static constexpr int TT2_OUTPUT_CV_COUNT = 8;
static constexpr int TT2_OUTPUT_TR_COUNT = 8;

// Per-CV output with a native linear ms slew. currentQ/startQ are the live and
// seed values in raw<<8 fixed-point (8 fractional bits) so slow ramps make
// sub-LSB progress instead of stalling on integer truncation.
struct TT2CvOutput {
    int16_t targetRaw;
    int16_t slewMs;
    int16_t remainingMs;
    int32_t currentQ;
    int32_t startQ;
};

// Per-TR output with a one-shot ms pulse. level is the live gate level;
// restLevel is restored when the pulse expires.
struct TT2TrOutput {
    uint8_t level;
    uint8_t restLevel;
    int16_t pulseRemainingMs;
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

// Seed a CV ramp toward targetRaw over slewMs (linear). slewMs<=0 snaps.
// The ramp interpolates from the current live value, so retargeting mid-ramp
// glides from wherever it is.
inline void tt2SeedCvSlew(TT2CvOutput &cv, int16_t targetRaw, int16_t slewMs) {
    if (targetRaw < 0) targetRaw = 0;
    if (targetRaw > 16383) targetRaw = 16383;
    cv.targetRaw = targetRaw;
    int32_t targetQ = int32_t(targetRaw) << 8;
    if (slewMs <= 0) {
        cv.currentQ = targetQ;
        cv.startQ = targetQ;
        cv.slewMs = 0;
        cv.remainingMs = 0;
        return;
    }
    cv.startQ = cv.currentQ;
    cv.slewMs = slewMs;
    cv.remainingMs = slewMs;
}

// Advance a CV ramp by ms. Exact linear interpolation from startQ to target
// over slewMs; settles precisely on the final ms. int64 intermediate avoids
// overflow at full-scale ramps.
inline void tt2AdvanceCvSlew(TT2CvOutput &cv, int ms) {
    if (cv.remainingMs <= 0 || ms <= 0) {
        return;
    }
    int32_t targetQ = int32_t(cv.targetRaw) << 8;
    int rem = int(cv.remainingMs) - ms;
    if (rem <= 0) {
        cv.currentQ = targetQ;
        cv.remainingMs = 0;
        return;
    }
    cv.remainingMs = int16_t(rem);
    int elapsed = int(cv.slewMs) - rem;
    cv.currentQ = cv.startQ +
        int32_t(int64_t(targetQ - cv.startQ) * elapsed / cv.slewMs);
}

// Arm a one-shot pulse: active level = pol, rest level = !pol (upstream
// tele_tr_pulse / tele_tr_pulse_end). timeMs<=0 does nothing.
inline void tt2ArmTrPulse(TT2TrOutput &tr, uint8_t pol, int16_t timeMs) {
    if (timeMs <= 0) {
        return;
    }
    tr.level = pol ? 1 : 0;
    tr.restLevel = pol ? 0 : 1;
    tr.pulseRemainingMs = timeMs;
}

// Advance a TR pulse by ms; on expiry restore the rest level. Unarmed
// channels (remaining 0) are a no-op.
inline void tt2AdvanceTrPulse(TT2TrOutput &tr, int ms) {
    if (tr.pulseRemainingMs <= 0 || ms <= 0) {
        return;
    }
    int rem = int(tr.pulseRemainingMs) - ms;
    if (rem <= 0) {
        tr.pulseRemainingMs = 0;
        tr.level = tr.restLevel;
        return;
    }
    tr.pulseRemainingMs = int16_t(rem);
}

static_assert(sizeof(TT2CvOutput) == 16, "TT2CvOutput size drift");
static_assert(sizeof(TT2TrOutput) == 4, "TT2TrOutput size drift");
static_assert(sizeof(TT2OutputState) == 164, "TT2OutputState size drift");
