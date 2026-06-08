// PARKED — NOT COMPILED. Reference only.
//
// The five stateful routing shapers, extracted verbatim from RoutingEngine.cpp on
// 2026-06-08 when the legacy apply branch was deleted. They are LFO-analysis stages, not
// value shapers — each reads a continuously-moving source over time and is hardcoded to a
// ~1 Hz LFO at the 1 kHz engine update rate. They left the routing lane (which now keeps
// only the stateless folds None/Fold/Crease + off-center variants).
//
// Pick these up when migrating them to **input-reading modulators** (see
// .tasks/modulator-enhancements.md). In the modulator engine they become input-driven
// modulator shapes with a real, configurable rate — the hardcoded constants below are the
// starting tuning, to be re-derived against the modulator's Hz rate instead of "assume 1 Hz".
//
// Not parked here: VcaNext (multiply by a neighbor source) → that's routing `scaleSource`.
// ProgressiveDivider is a clock-edge divider → gate; it's a clock/gate utility, may not want
// to be a CV modulator at all.
//
// This file is outside the build tree on purpose — it references no current engine types.

#include "core/math/Math.h"   // clamp
#include <cmath>              // fabsf
#include <algorithm>          // std::min/std::max
#include <cstdint>

namespace parked_stateful_shapers {

// Self-contained state (was RouteState::TrackStateUnion variants).
struct LocationState  { float location; };
struct EnvelopeState  { float envelope; };
struct FreqFollowState { float freqAcc; bool freqSign; uint16_t ffHold; };
struct ActivityState  { float activityPrev; float activityLevel; bool activitySign; uint16_t actHold; };
struct ProgDivState   { float progCount; float progThreshold; bool progSign; float progOut; float progOutSlewed; uint16_t progHold; };

// Integrator: velocity → position. ~4 s rail-to-rail at 1 kHz.
inline float applyLocation(float srcNormalized, float &state) {
    constexpr float kRate = 0.000125f;   // 0.5 span / 4000 ticks
    state = clamp(state + (srcNormalized - 0.5f) * kRate, 0.f, 1.f);
    return state;
}

// Envelope follower on the deviation from center. Instant attack, ~2 s release.
inline float applyEnvelope(float srcNormalized, float &envState) {
    float rect = fabsf(srcNormalized - 0.5f) * 2.f;
    constexpr float attackCoeff = 1.0f;
    constexpr float releaseCoeff = 0.0005f;   // tau ~2 s at 1 kHz
    float coeff = rect > envState ? attackCoeff : releaseCoeff;
    envState += (rect - envState) * coeff;
    return clamp(envState, 0.f, 1.f);
}

// Frequency-to-CV: counts center-crossings; builds ~7 s over 14 crossings, leaks ~10 s.
inline float applyFrequencyFollower(float srcNormalized, FreqFollowState &st) {
    bool signNow = srcNormalized > 0.5f;
    if (signNow != st.freqSign) {
        st.freqAcc = std::min(1.f, st.freqAcc + 0.10f);   // 14 crossings → 1.0 (tuned 1 Hz)
        st.freqSign = signNow;
    }
    st.freqAcc *= 0.9999f;   // leak tau ~10 s
    if (st.freqAcc >= 0.999f) {
        if (++st.ffHold > 3000) {                 // ~3 s hold (1× max LFO period)
            constexpr float fadeCoeff = 0.00015f; // fade to 0 over ~7 s
            st.freqAcc += (0.f - st.freqAcc) * fadeCoeff;
            if (st.freqAcc < 0.01f) { st.freqAcc = 0.f; st.ffHold = 0; }
        }
    } else {
        st.ffHold = 0;
    }
    return st.freqAcc;
}

// Movement/activity detector: rate-of-change + sign-flip spike, ~2 s decay.
inline float applyActivity(float srcNormalized, ActivityState &st) {
    float delta = fabsf(srcNormalized - st.activityPrev);
    constexpr float decay = 0.9995f;   // tau ~2 s (tuned 1-3 s LFOs)
    constexpr float gain = 0.05f;
    st.activityLevel = st.activityLevel * decay + delta * gain;
    bool signNow = srcNormalized > 0.5f;
    if (signNow != st.activitySign) { st.activityLevel = 1.f; st.activitySign = signNow; }
    if (st.activityLevel >= 0.999f) {
        if (++st.actHold > 6000) {                 // ~6 s hold (2× max LFO period)
            constexpr float fadeCoeff = 0.00033f;  // fade to 0 over ~3 s
            st.activityLevel += (0.f - st.activityLevel) * fadeCoeff;
            if (st.activityLevel < 0.01f) { st.activityLevel = 0.f; st.actHold = 0; }
        }
    } else {
        st.actHold = 0;
    }
    st.activityPrev = srcNormalized;
    return clamp(st.activityLevel, 0.f, 1.f);
}

// Self-accelerating clock divider → slewed gate. Counts center-crossings; division ratio
// grows ×1.25 each fire up to 128, recovers, resets. More a clock/gate utility than a CV.
inline float applyProgressiveDivider(float srcNormalized, ProgDivState &st) {
    bool signNow = srcNormalized > 0.5f;
    if (signNow != st.progSign) { st.progCount += 1.f; st.progSign = signNow; }
    if (st.progCount >= st.progThreshold) {
        st.progOut = st.progOut > 0.5f ? 0.f : 1.f;
        st.progCount = 0.f;
        constexpr float growth = 1.25f;
        constexpr float thresholdMax = 128.f;
        st.progThreshold = std::min(st.progThreshold * growth, thresholdMax);
    } else {
        constexpr float decay = 0.999f;   // recover tau ~1 s
        if (st.progThreshold > 1.f) st.progThreshold = std::max(1.f, st.progThreshold * decay);
    }
    if (st.progThreshold >= 127.f) {
        if (++st.progHold > 2000) { st.progThreshold = 1.f; st.progHold = 0; }   // ~2 s reset
    } else {
        st.progHold = 0;
    }
    constexpr float gateSlew = 0.001f;   // slew gate over ~1 s
    st.progOutSlewed += (st.progOut - st.progOutSlewed) * gateSlew;
    return st.progOutSlewed;
}

// Initial state defaults (were RoutingEngine::resetState): Location.location = 0.5,
// Activity.activityPrev = 0.5, ProgDiv.progThreshold = 1.0; the rest zero-init.

} // namespace parked_stateful_shapers
