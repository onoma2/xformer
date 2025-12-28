/*
 * Code snippets demonstrating how different shapers transform incoming signals in the routing system
 * 
 * This example shows how various shaper functions modify normalized source signals (0.0 to 1.0)
 * These shapers are applied after bias and depth processing.
 */

#include <iostream>
#include <vector>
#include <cmath>

// Clamp function to keep values within a range
float clamp(float value, float min, float max) {
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

// Apply bias and depth to a normalized source signal (0.0 to 1.0)
float applyBiasDepthToSource(float srcNormalized, int biasPct, int depthPct) {
    float depth = depthPct * 0.01f;  // Convert percentage to multiplier
    float bias = biasPct * 0.01f;    // Convert percentage to offset
    
    // Apply depth scaling around center point (0.5), then add bias
    float shaped = 0.5f + (srcNormalized - 0.5f) * depth + bias;
    
    // Clamp result to valid range [0.0, 1.0]
    return clamp(shaped, 0.f, 1.f);
}

// Generate a simple LFO signal (sine wave) normalized to 0.0-1.0 range
float generateLfoSignal(float phase) {
    return (sin(phase) + 1.0f) * 0.5f;  // Convert from [-1,1] to [0,1]
}

// Shaper: None - Pass through unchanged
float applyNoShaper(float srcNormalized) {
    return srcNormalized;
}

// Shaper: Crease - Creates a discontinuity at 0.5 by folding
float applyCreaseSource(float srcNormalized) {
    constexpr float creaseAmount = 0.5f;
    float creased = srcNormalized + (srcNormalized <= 0.5f ? creaseAmount : -creaseAmount);
    return clamp(creased, 0.f, 1.f);
}

// Shaper: Location - Integrates the input to create a position accumulator
float applyLocation(float srcNormalized, float &state) {
    // target ~4s rail-to-rail at 1 kHz: 0.5 span / (4000 ticks) ≈ 0.000125
    constexpr float kRate = 0.000125f;
    state = clamp(state + (srcNormalized - 0.5f) * kRate, 0.f, 1.f);
    return state;
}

// Shaper: Envelope - Creates an envelope follower based on input amplitude
float applyEnvelope(float srcNormalized, float &envState) {
    float rect = fabsf(srcNormalized - 0.5f) * 2.f; // 0..1
    constexpr float attackCoeff = 1.0f;
    // release with tau ~2s at 1 kHz: 1 - exp(-1/2000) ≈ 0.0005
    constexpr float releaseCoeff = 0.0005f;
    float coeff = rect > envState ? attackCoeff : releaseCoeff;
    envState += (rect - envState) * coeff;
    return clamp(envState, 0.f, 1.f);
}

// Shaper: TriangleFold - Folds the signal in a triangular pattern
float applyTriangleFold(float srcNormalized) {
    float x = 2.f * (srcNormalized - 0.5f); // -1..1
    float folded = (x > 0.f) ? 1.f - 2.f * fabsf(x - 0.5f) : -1.f + 2.f * fabsf(x + 0.5f);
    return clamp(0.5f + 0.5f * folded, 0.f, 1.f);
}

// Shaper: FrequencyFollower - Detects frequency by counting zero crossings
float applyFrequencyFollower(float srcNormalized, float &freqAcc, bool &freqSign) {
    bool signNow = srcNormalized > 0.5f;
    if (signNow != freqSign) {
        // Tuned for 1s LFO: reaches 1.0 in 14 crossings = 7s build time
        freqAcc = std::min(1.f, freqAcc + 0.10f);
        freqSign = signNow;
    }
    // leak with tau ~10s at 1 kHz: exp(-1/10000) ≈ 0.9999
    freqAcc *= 0.9999f;
    return freqAcc;
}

// Shaper: Activity - Measures signal activity based on changes
float applyActivity(float srcNormalized, float &activityLevel, float &activityPrev, bool &activitySign) {
    float delta = fabsf(srcNormalized - activityPrev);
    // decay with tau ~2s at 1 kHz: exp(-1/2000) ≈ 0.9995 (tuned for 1-3s LFOs)
    constexpr float decay = 0.9995f;
    constexpr float gain = 0.05f; // Higher sensitivity for slow LFO movement
    activityLevel = activityLevel * decay + delta * gain;
    bool signNow = srcNormalized > 0.5f;
    if (signNow != activitySign) {
        activityLevel = 1.f;
        activitySign = signNow;
    }
    activityPrev = srcNormalized;
    return clamp(activityLevel, 0.f, 1.f);
}

// Shaper: ProgressiveDivider - Creates a binary output that divides based on input crossings
float applyProgressiveDivider(float srcNormalized, float &progCount, float &progThreshold, 
                             bool &progSign, float &progOut, float &progOutSlewed) {
    bool signNow = srcNormalized > 0.5f;
    if (signNow != progSign) {
        progCount += 1.f;
        progSign = signNow;
    }

    if (progCount >= progThreshold) {
        progOut = progOut > 0.5f ? 0.f : 1.f;
        progCount = 0.f;
        constexpr float growth = 1.25f;
        constexpr float add = 0.f;
        constexpr float thresholdMax = 128.f;
        progThreshold = std::min(progThreshold * growth + add, thresholdMax);
    } else {
        // recover threshold: tau ~1s at 1 kHz → decay ≈ 0.999
        constexpr float decay = 0.999f;
        if (progThreshold > 1.f) {
            progThreshold = std::max(1.f, progThreshold * decay);
        }
    }

    // Slew the binary gate output over ~1s for smooth transitions
    constexpr float gateSlew = 0.001f; // tau ~1s at 1 kHz
    progOutSlewed += (progOut - progOutSlewed) * gateSlew;

    return progOutSlewed;
}

// Shaper: VcaNext - Uses the next route as a VCA for this route
float applyVcaNext(float srcNormalized, float neighborValue) {
    // VCA: Center-referenced amplitude modulation
    return 0.5f + (srcNormalized - 0.5f) * neighborValue;
}

int main() {
    std::cout << "Routing System: Shaper Effects on Incoming Signals\n";
    std::cout << "==================================================\n\n";

    // Example: LFO signal with different shapers
    std::cout << "Example: LFO Signal Processing with Different Shapers\n";
    std::cout << "Original LFO (no shaper):\n";
    
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;  // Full cycle
        float lfo = generateLfoSignal(phase);
        std::cout << "  Phase " << i << ": " << lfo << "\n";
    }
    
    std::cout << "\nWith Crease Shaper:\n";
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyCreaseSource(lfo);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith Envelope Shaper (stateful):\n";
    float envState = 0.0f;
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyEnvelope(lfo, envState);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith TriangleFold Shaper:\n";
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyTriangleFold(lfo);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith Location Shaper (stateful):\n";
    float locationState = 0.5f;
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyLocation(lfo, locationState);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith FrequencyFollower Shaper (stateful):\n";
    float freqAcc = 0.0f;
    bool freqSign = false;
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyFrequencyFollower(lfo, freqAcc, freqSign);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith Activity Shaper (stateful):\n";
    float activityLevel = 0.0f;
    float activityPrev = 0.5f;
    bool activitySign = false;
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyActivity(lfo, activityLevel, activityPrev, activitySign);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith ProgressiveDivider Shaper (stateful):\n";
    float progCount = 0.0f;
    float progThreshold = 1.0f;
    bool progSign = false;
    float progOut = 0.0f;
    float progOutSlewed = 0.0f;
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyProgressiveDivider(lfo, progCount, progThreshold, 
                                                 progSign, progOut, progOutSlewed);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith VcaNext Shaper (requires neighbor value):\n";
    float neighborValue = 0.7f; // Example neighbor route value
    for (int i = 0; i < 10; ++i) {
        float phase = (float)i / 10.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyVcaNext(lfo, neighborValue);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }

    return 0;
}