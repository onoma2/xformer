#pragma once

#include <cstdint>
#include <cmath>

// Deterministic coupled autoregressive map ported from Nebulae 2's helical
// generator (VhGenHelicalVoice). Each step folds the previous, duration-dominated
// state into a scale-degree note, derives this step's duration from a
// sqrt-frequency law, and feeds back — so rhythm steers melody and pitch sets
// timing. No RNG; the seed sets the initial (pitch, dur) pair. Pure (no model
// dependency): the scale mapping happens at playback, the law works in octaves.
struct HelicalAr {
    // borrowed helical constants: duration dominates the feedback (~35:1)
    static constexpr float kWeightDuration = 17.31f;
    static constexpr float kWeightPitch = 0.5f;
    static constexpr float kGateMinFraction = 0.2f; // gate spans 20..100% of duration
    static constexpr float kIdentity = 1.123f;       // irrational fold offset (helical.instr giVoiceOffsets)

    struct Result {
        int noteIndex;
        int durationTicks;
        int gateLength;
    };

    float pitch = 0.f;
    float dur = 0.f;

    void seed(uint32_t value) {
        pitch = float(value % 997) * 0.01f;
        dur = float((value / 997) % 997) * 0.1f + 1.f;
    }

    // base x 2^(octaves*depth) law; depth ("helicity") sets how hard pitch stretches
    // duration (0.5 = sqrt-freq, 1 = freq-proportional). lawDir>=0 = higher pitch longer.
    static float durationFloat(float octaves, int base, int lawDir, float depth) {
        float e = octaves * depth;
        return float(base) * std::pow(2.f, lawDir >= 0 ? e : -e);
    }

    static int durationForNote(int noteIndex, int scaleSize, int base, int lawDir, float depth, int minTicks, int maxTicks) {
        if (scaleSize < 1) scaleSize = 1;
        int ticks = int(durationFloat(float(noteIndex) / float(scaleSize), base, lawDir, depth) + 0.5f);
        if (ticks < minTicks) ticks = minTicks;
        if (ticks > maxTicks) ticks = maxTicks;
        return ticks;
    }

    // span = octaveRange x scaleSize (fold modulus / usable scale degrees).
    Result step(int span, int scaleSize, int base, int lawDir, float depth, int minTicks, int maxTicks) {
        if (span < 1) span = 1;

        if (scaleSize < 1) scaleSize = 1;
        float spanF = float(span);

        // irrational identity offset keeps the fold off trivial fixed points
        float raw = dur * kWeightDuration + pitch * kWeightPitch + kIdentity;
        float influence = std::fmod(raw, spanF);
        if (influence < 0.f) {
            influence += spanF;
        }

        int note = int(influence + 0.5f);
        if (note >= span) note = span - 1;
        if (note < 0) note = 0;

        // law on the CONTINUOUS folded pitch, centered on the span midpoint so duration
        // swings below AND above base (no base floor); fed-back duration stays continuous
        float octavesCentered = (influence - 0.5f * spanF) / float(scaleSize);
        float rawDur = durationFloat(octavesCentered, base, lawDir, depth);
        int durationTicks = int(rawDur + 0.5f);
        if (durationTicks < minTicks) durationTicks = minTicks;
        if (durationTicks > maxTicks) durationTicks = maxTicks;

        // the rounding discards a free [0,1) value -> per-step gate length
        float rem = influence - float(note);
        float gate01 = rem + 0.5f;
        float gateFraction = kGateMinFraction + (1.f - kGateMinFraction) * gate01;
        int gateLength = int(gateFraction * float(durationTicks) + 0.5f);
        if (gateLength > durationTicks) gateLength = durationTicks;
        if (gateLength < 0) gateLength = 0;

        // feedback: continuous (unclamped) duration + folded pitch — the coupling
        pitch = influence;
        dur = rawDur;

        return Result{ note, durationTicks, gateLength };
    }
};
