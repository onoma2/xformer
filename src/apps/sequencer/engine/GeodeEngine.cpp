#include "GeodeEngine.h"

#include "model/Curve.h"
#include "core/math/Math.h"

#include <algorithm>
#include <cmath>

static constexpr float Pi = 3.1415926536f;
static constexpr float TwoPi = 2.f * Pi;

// Time mapping: 0.0-1.0 → 5ms to 5000ms (logarithmic feel)
static constexpr float MinTimeMs = 5.f;
static constexpr float MaxTimeMs = 5000.f;

GeodeEngine::GeodeEngine() {
    reset();
}

void GeodeEngine::Voice::reset() {
    phase = 0.f;
    divs = 1;
    repeatsTotal = 0;
    repeatsRemaining = 0;
    stepIndex = 0;
    active = false;
    justTriggered = false;
    level = 0.f;
    targetLevel = 0.f;
    riseTimeMs = 100.f;
    fallTimeMs = 100.f;
    envelopePhase = 0.f;
    inAttack = false;
}

void GeodeEngine::reset() {
    for (auto &voice : _voices) {
        voice.reset();
    }
    _prevMeasureFraction = 0.f;
    for (int i = 0; i < VoiceCount; ++i) {
        _tuneNum[i] = i + 1;
        _tuneDen[i] = 1;
    }
}

void GeodeEngine::triggerVoice(int voiceIndex, int divs, int repeats) {
    if (voiceIndex < 0 || voiceIndex >= VoiceCount) {
        return;
    }

    Voice &voice = _voices[voiceIndex];
    voice.divs = clamp(divs, 1, 64);
    voice.repeatsTotal = clamp(repeats, -1, 255);
    voice.repeatsRemaining = voice.repeatsTotal;
    voice.phase = 0.f;
    voice.stepIndex = 0;
    voice.active = true;
    voice.level = 0.f;
    voice.targetLevel = 0.f;
    voice.envelopePhase = 1.f;
    voice.inAttack = false;
    voice.riseTimeMs = 100.f;  // Defaults updated on first update()
    voice.fallTimeMs = 100.f;
}

void GeodeEngine::triggerAllVoices(int divs, int repeats) {
    for (int i = 0; i < VoiceCount; i++) {
        triggerVoice(i, divs, repeats);
    }
}

void GeodeEngine::triggerImmediate(int voiceIndex, float time, float intone, float run, uint8_t mode) {
    if (voiceIndex < 0 || voiceIndex >= VoiceCount) {
        return;
    }
    Voice &voice = _voices[voiceIndex];
    if (!voice.active) {
        return;
    }
    float velocity = calculatePhysics(voice, run, mode);
    float baseTimeMs = timeParamToMs(time);
    float voiceTimeScale = getVoiceTimeScale(voiceIndex, intone);
    float voiceTimeMs = baseTimeMs * voiceTimeScale;
    triggerVoiceEnvelope(voice, velocity, voiceTimeMs, intone, voiceIndex);
    if (voice.repeatsRemaining == 0) {
        // No scheduled repeats after the immediate trigger.
        voice.active = false;
    }
}

void GeodeEngine::triggerImmediateAll(float time, float intone, float run, uint8_t mode) {
    for (int i = 0; i < VoiceCount; ++i) {
        triggerImmediate(i, time, intone, run, mode);
    }
}

void GeodeEngine::syncMeasureFraction(float measureFraction) {
    _prevMeasureFraction = measureFraction;
}

void GeodeEngine::setVoicePhase(int voiceIndex, float phase) {
    if (voiceIndex < 0 || voiceIndex >= VoiceCount) {
        return;
    }
    _voices[voiceIndex].phase = clamp(phase, 0.f, 1.f);
}

void GeodeEngine::markVoiceTriggered(int voiceIndex) {
    if (voiceIndex < 0 || voiceIndex >= VoiceCount) {
        return;
    }
    _voices[voiceIndex].justTriggered = true;
}

float GeodeEngine::timeParamToMs(float time) const {
    // Logarithmic mapping: 0→5ms, 0.5→~158ms, 1.0→5000ms
    float t = clamp(time, 0.f, 1.f);
    return MinTimeMs * std::pow(MaxTimeMs / MinTimeMs, t);
}

float GeodeEngine::getVoiceTimeScale(int voiceIndex, float intone) const {
    // JF-style mapping: scale = ratio^intone
    // intone: -1.0 (undertones) to 0.0 (noon) to +1.0 (overtones)
    int16_t num = voiceTuneNumerator(voiceIndex);
    int16_t den = voiceTuneDenominator(voiceIndex);
    if (den == 0) {
        return 1.0f;
    }
    float ratio = static_cast<float>(num) / static_cast<float>(den);
    return std::pow(ratio, intone);
}

void GeodeEngine::setVoiceTune(int voiceIndex, int16_t numerator, int16_t denominator) {
    if (voiceIndex < 0 || voiceIndex >= VoiceCount) {
        return;
    }
    if (numerator == 0 || denominator == 0) {
        _tuneNum[voiceIndex] = voiceIndex + 1;
        _tuneDen[voiceIndex] = 1;
        return;
    }
    _tuneNum[voiceIndex] = numerator;
    _tuneDen[voiceIndex] = denominator;
}

int16_t GeodeEngine::voiceTuneNumerator(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= VoiceCount) {
        return 1;
    }
    return _tuneNum[voiceIndex];
}

int16_t GeodeEngine::voiceTuneDenominator(int voiceIndex) const {
    if (voiceIndex < 0 || voiceIndex >= VoiceCount) {
        return 1;
    }
    return _tuneDen[voiceIndex];
}

void GeodeEngine::update(float dt, float measureFraction,
                          float time, float intone, float ramp, float curve,
                          float run, uint8_t mode) {
    // Calculate measure delta (handle wrap)
    float measureDelta = measureFraction - _prevMeasureFraction;
    if (measureDelta < 0.f) {
        // Wrapped past 1.0
        measureDelta += 1.f;
    }
    _prevMeasureFraction = measureFraction;

    // Convert time parameter to base milliseconds
    float baseTimeMs = timeParamToMs(time);

    // Update each voice
    for (int i = 0; i < VoiceCount; i++) {
        Voice &voice = _voices[i];

        if (!voice.active) {
            // Still update envelope decay for inactive voices
            updateVoiceEnvelope(voice, dt * 1000.f, ramp, curve);
            continue;
        }

        // Check for phase wrap (trigger event)
        bool triggered = updateVoicePhase(i, voice, measureDelta);

        if (triggered) {
            // Calculate velocity from physics (uses current stepIndex)
            float velocity = calculatePhysics(voice, run, mode);

            // Increment stepIndex AFTER physics calculation
            voice.stepIndex++;

            // Calculate voice-specific time (INTONE scaled)
            float voiceTimeScale = getVoiceTimeScale(i, intone);
            float voiceTimeMs = baseTimeMs * voiceTimeScale;

            // Trigger envelope
            triggerVoiceEnvelope(voice, velocity, voiceTimeMs, intone, i);
        }

        // Update envelope
        updateVoiceEnvelope(voice, dt * 1000.f, ramp, curve);
    }

}

bool GeodeEngine::updateVoicePhase(int voiceIndex, Voice &voice, float measureDelta) {
    if (!voice.active) {
        return false;
    }

    // Increment phase by measure delta * divs
    voice.phase += measureDelta * voice.divs;

    // Detect wrap (trigger event)
    if (voice.phase >= 1.0f) {
        voice.phase = std::fmod(voice.phase, 1.0f);
        if (voice.justTriggered) {
            // Suppress a wrap-trigger caused by a large measure delta right after G.V.
            voice.justTriggered = false;
            return false;
        }
        voice.justTriggered = false;

        // Check if repeats remaining
        if (voice.repeatsRemaining > 0) {
            voice.repeatsRemaining--;
            // Note: stepIndex incremented in update() AFTER physics calculation
            return true;
        } else if (voice.repeatsRemaining < 0) {
            // Infinite repeats (-1)
            // Note: stepIndex incremented in update() AFTER physics calculation
            return true;
        } else {
            // Exhausted - stop voice
            voice.active = false;
            return false;
        }
    }

    voice.justTriggered = false;
    return false;
}

float GeodeEngine::calculatePhysics(const Voice &voice, float run, uint8_t mode) const {
    float absRun = std::fabs(run);
    switch (mode) {
    case 0: // Transient (Rhythmic Accent)
        {
            // Map |run| to cycle 1-10, reverse saw when run < 0
            int cycle = static_cast<int>(absRun * 9.0f) + 1;
            int pos = (cycle > 0) ? (voice.stepIndex % cycle) : 0;
            float t = (cycle <= 1) ? 0.0f : (pos / static_cast<float>(cycle - 1));
            float amp = (run >= 0.0f) ? (1.0f - t) : t;
            return clamp(amp, 0.0f, 1.0f);
        }

    case 1: // Sustain (Decay/Gravity)
        {
            int burstLength = (voice.repeatsTotal > 0) ? voice.repeatsTotal : 16;
            float t = (burstLength > 0)
                ? (static_cast<float>(voice.stepIndex % burstLength) / static_cast<float>(burstLength))
                : 0.0f;
            if (run >= 0.0f) {
                // Linear decay at RUN=0, blend in fold/bounce as RUN increases
                float base = 1.0f - clamp(t, 0.0f, 1.0f);
                int cycles = static_cast<int>(absRun * 4.0f) + 1;
                float phase = t * cycles;
                float frac = phase - std::floor(phase);
                float tri = 1.0f - std::fabs(2.0f * frac - 1.0f);
                float mix = clamp(absRun * 5.0f, 0.0f, 1.0f);
                float velocity = base * (1.0f - mix) + tri * mix;
                return clamp(velocity, 0.0f, 1.0f);
            } else {
                // Slow decay as run goes negative
                float exponent = 1.0f - absRun * 0.8f;
                float velocity = std::pow(1.0f - clamp(t, 0.0f, 1.0f), exponent);
                return clamp(velocity, 0.0f, 1.0f);
            }
        }

    case 2: // Cycle (Amplitude LFO)
        {
            // Bipolar rate: positive = slow emphasis, negative = dense cycling
            float rate = (run >= 0.0f) ? (1.0f + absRun * 3.0f) : (1.0f + absRun * 12.0f);
            int burstLength = (voice.repeatsTotal > 0) ? voice.repeatsTotal : 16;
            float t = (burstLength > 0)
                ? (static_cast<float>(voice.stepIndex % burstLength) / static_cast<float>(burstLength))
                : 0.0f;
            if (run < 0.0f) {
                // Slow secondary tri to add chaotic feel at negative RUN
                float slowRate = 0.25f + absRun * 0.75f; // 0.25..1.0 cycles per burst
                float slowPhase = t * slowRate;
                float slowFrac = slowPhase - std::floor(slowPhase);
                float tri = 1.0f - std::fabs(2.0f * slowFrac - 1.0f);
                float modDepth = 0.15f + absRun * 0.35f; // 0.15..0.5
                float rateMod = 1.0f + (tri - 0.5f) * 2.0f * modDepth;
                rate *= rateMod;
            }
            float lfoPhase = t * rate * TwoPi;
            return 0.5f + 0.5f * std::sin(lfoPhase);
        }

    default:
        return 1.0f;
    }
}

void GeodeEngine::triggerVoiceEnvelope(Voice &voice, float velocity,
                                        float timeMs, float intone, int voiceIndex) {
    voice.targetLevel = clamp(velocity, 0.f, 1.f);
    voice.envelopePhase = 0.f;
    voice.inAttack = true;

    // Store time for this voice (will be split by ramp in updateVoiceEnvelope)
    voice.riseTimeMs = timeMs;
    voice.fallTimeMs = timeMs;
}

float GeodeEngine::applyCurveShape(float phase, float curve, bool isAttack) const {
    // curve: -1.0 (rectangular/step) to 0.0 (linear) to +1.0 (smooth/sine)
    // Use Curve library for shape evaluation

    if (curve < -0.5f) {
        // Rectangular/step
        return isAttack ? Curve::eval(Curve::StepUp, phase)
                        : Curve::eval(Curve::StepDown, phase);
    } else if (curve < 0.f) {
        // Log (fast start, slow end)
        return isAttack ? Curve::eval(Curve::LogUp, phase)
                        : Curve::eval(Curve::LogDown, phase);
    } else if (curve < 0.5f) {
        // Linear
        return isAttack ? Curve::eval(Curve::RampUp, phase)
                        : Curve::eval(Curve::RampDown, phase);
    } else {
        // Exponential / Smooth (slow start, fast end)
        return isAttack ? Curve::eval(Curve::SmoothUp, phase)
                        : Curve::eval(Curve::SmoothDown, phase);
    }
}

void GeodeEngine::updateVoiceEnvelope(Voice &voice, float dtMs, float ramp, float curve) {
    if (voice.level <= 0.0001f && !voice.inAttack && voice.envelopePhase >= 1.f) {
        voice.level = 0.f;
        return;
    }

    // Calculate rise/fall times from ramp parameter
    // ramp 0.0 = instant attack, long decay (percussive)
    // ramp 0.5 = equal attack/decay (triangle)
    // ramp 1.0 = long attack, instant decay (reverse)
    float totalTime = voice.riseTimeMs + voice.fallTimeMs;
    float riseRatio = clamp(ramp, 0.01f, 0.99f);
    float currentRiseTime = totalTime * riseRatio;
    float currentFallTime = totalTime * (1.f - riseRatio);

    // Minimum time to avoid division by zero
    currentRiseTime = std::max(currentRiseTime, 1.f);
    currentFallTime = std::max(currentFallTime, 1.f);

    float timeConstant = voice.inAttack ? currentRiseTime : currentFallTime;

    // Advance envelope phase
    voice.envelopePhase += dtMs / timeConstant;

    if (voice.envelopePhase >= 1.0f) {
        if (voice.inAttack) {
            // Attack complete, switch to decay
            voice.inAttack = false;
            voice.envelopePhase = 0.f;
            voice.level = voice.targetLevel;
        } else {
            // Decay complete
            voice.level = 0.f;
            voice.envelopePhase = 1.f;
        }
    } else {
        // Apply curve shaping
        float shapedPhase = applyCurveShape(voice.envelopePhase, curve, voice.inAttack);

        if (voice.inAttack) {
            voice.level = shapedPhase * voice.targetLevel;
        } else {
            // shapedPhase already goes 1→0 for down curves
            voice.level = shapedPhase * voice.targetLevel;
        }
    }
}

float GeodeEngine::computeMixLevel() const {
    // JF MIX algorithm: divide each voice by its index, take max
    float mix = 0.0f;

    for (int i = 0; i < VoiceCount; i++) {
        float scaled = _voices[i].level / static_cast<float>(i + 1);
        mix = std::max(mix, scaled);
    }

    return mix;
}

float GeodeEngine::mixLevel() const {
    return computeMixLevel();
}

int16_t GeodeEngine::outputRaw(int16_t offsetRaw) const {
    float mix = computeMixLevel();

    // Interpolate between offset (silence) and 16383 (max)
    int16_t targetRaw = 16383;
    int32_t result = offsetRaw + static_cast<int32_t>(mix * (targetRaw - offsetRaw));
    return static_cast<int16_t>(clamp<int32_t>(result, 0, 16383));
}

float GeodeEngine::voiceLevel(int index) const {
    if (index < 0 || index >= VoiceCount) {
        return 0.f;
    }
    return _voices[index].level;
}

int16_t GeodeEngine::voiceOutputRaw(int index, int16_t offsetRaw) const {
    if (index < 0 || index >= VoiceCount) {
        return offsetRaw;
    }

    float level = _voices[index].level;
    int16_t targetRaw = 16383;
    int32_t result = offsetRaw + static_cast<int32_t>(level * (targetRaw - offsetRaw));
    return static_cast<int16_t>(clamp<int32_t>(result, 0, 16383));
}

bool GeodeEngine::voiceActive(int index) const {
    if (index < 0 || index >= VoiceCount) {
        return false;
    }
    return _voices[index].active;
}

bool GeodeEngine::anyVoiceActive() const {
    for (const auto &voice : _voices) {
        if (voice.active || voice.level > 0.0001f) {
            return true;
        }
    }
    return false;
}
