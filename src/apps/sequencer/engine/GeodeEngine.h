#pragma once

#include <array>
#include <cstdint>

class GeodeEngine {
public:
    static constexpr int VoiceCount = 6;

    GeodeEngine();

    // Voice triggering (called from G.V operation)
    void triggerVoice(int voiceIndex, int divs, int repeats);
    void triggerAllVoices(int divs, int repeats);
    void triggerImmediate(int voiceIndex, float time, float intone, float run, uint8_t mode);
    void triggerImmediateAll(float time, float intone, float run, uint8_t mode);
    void syncMeasureFraction(float measureFraction);
    void setVoicePhase(int voiceIndex, float phase);
    void markVoiceTriggered(int voiceIndex);

    // Main update (1kHz, called from TeletypeTrackEngine)
    // Parameters are normalized floats:
    //   time: 0.0-1.0 (envelope base time)
    //   intone: -1.0 to +1.0 (voice time spread)
    //   ramp: 0.0-1.0 (attack/decay balance)
    //   curve: -1.0 to +1.0 (shape: log/lin/exp)
    //   run: 0.0-1.0 (physics parameter)
    //   mode: 0=Transient, 1=Sustain, 2=Cycle
    void update(float dt, float measureFraction,
                float time, float intone, float ramp, float curve,
                float run, uint8_t mode);

    // Outputs - offset is raw 0-16383 for silence voltage
    float mixLevel() const;                         // 0.0-1.0 mix level
    int16_t outputRaw(int16_t offsetRaw) const;     // Raw value with offset
    float voiceLevel(int index) const;              // Individual voice 0.0-1.0
    int16_t voiceOutputRaw(int index, int16_t offsetRaw) const;

    // Per-voice tuning ratios (INTONE multiplier overrides)
    void setVoiceTune(int voiceIndex, int16_t numerator, int16_t denominator);
    int16_t voiceTuneNumerator(int voiceIndex) const;
    int16_t voiceTuneDenominator(int voiceIndex) const;

    // State query
    bool voiceActive(int index) const;
    bool anyVoiceActive() const;

    void reset();

private:
    struct Voice {
        // Sequencer state
        float phase;              // 0.0-1.0 (measure fraction * divs, wraps)
        int divs;                 // 1-64 (rhythm divisor)
        int repeatsTotal;         // -1=infinite, 0-255
        int repeatsRemaining;     // Countdown
        int stepIndex;            // Current step in burst (for physics)
        bool active;              // Voice is running
        bool justTriggered;       // Suppress wrap-trigger after immediate trigger

        // Envelope state
        float level;              // Current output level (0.0-1.0)
        float targetLevel;        // From physics calculation
        float riseTimeMs;         // Attack time (scaled by INTONE)
        float fallTimeMs;         // Decay time (scaled by INTONE)
        float envelopePhase;      // 0.0-1.0 through AR cycle
        bool inAttack;            // true=attack, false=decay

        void reset();
    };

    std::array<Voice, VoiceCount> _voices;
    float _prevMeasureFraction;
    std::array<int16_t, VoiceCount> _tuneNum;
    std::array<int16_t, VoiceCount> _tuneDen;
    mutable std::array<float, VoiceCount> _cachedTimeScale;
    mutable std::array<float, VoiceCount> _cachedIntone;
    mutable std::array<int16_t, VoiceCount> _cachedTuneNum;
    mutable std::array<int16_t, VoiceCount> _cachedTuneDen;
    mutable std::array<bool, VoiceCount> _timeScaleValid;
    float _cachedMixLevel = 0.0f;

    // Helper functions
    bool updateVoicePhase(int voiceIndex, Voice &voice, float measureDelta);
    void triggerVoiceEnvelope(Voice &voice, float velocity, float timeMs);
    float calculatePhysics(const Voice &voice, float run, uint8_t mode) const;
    void updateVoiceEnvelope(Voice &voice, float dt, float ramp, float curve);
    float computeMixLevel() const;
    float getVoiceTimeScale(int voiceIndex, float intone) const;
    float applyCurveShape(float phase, float curve, bool isAttack) const;
    float timeParamToMs(float time) const;
};
