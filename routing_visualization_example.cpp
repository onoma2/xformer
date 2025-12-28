/*
 * Visual examples of how LFO and envelope signals are shaped in the routing system
 * 
 * This example provides text-based visualizations showing how bias, depth, and shapers
 * transform incoming LFO and envelope signals.
 */

#include <iostream>
#include <vector>
#include <cmath>
#include <string>

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

// Generate a simple envelope signal (attack and decay)
float generateEnvelopeSignal(float phase) {
    // Simple ADSR-like envelope: attack, decay, sustain, release
    if (phase < 0.2f) {
        // Attack: 0 to 1 over first 20% of cycle
        return phase / 0.2f;
    } else if (phase < 0.6f) {
        // Decay: 1 to 0.7 over next 40% of cycle
        return 1.0f - ((phase - 0.2f) / 0.4f) * 0.3f;
    } else if (phase < 0.9f) {
        // Sustain: hold at 0.7 for 30% of cycle
        return 0.7f;
    } else {
        // Release: 0.7 to 0 over last 10% of cycle
        return 0.7f - ((phase - 0.9f) / 0.1f) * 0.7f;
    }
}

// Create a text-based visualization of a signal
std::string visualizeSignal(const std::vector<float>& signal, int width = 40) {
    std::string result;
    for (float value : signal) {
        int pos = static_cast<int>(value * width);
        if (pos < 0) pos = 0;
        if (pos > width) pos = width;
        
        std::string line(width + 1, ' ');
        line[pos] = '*';
        result += line + "\n";
    }
    return result;
}

// Shaper: Crease - Creates a discontinuity at 0.5 by folding
float applyCreaseSource(float srcNormalized) {
    constexpr float creaseAmount = 0.5f;
    float creased = srcNormalized + (srcNormalized <= 0.5f ? creaseAmount : -creaseAmount);
    return clamp(creased, 0.f, 1.f);
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

int main() {
    std::cout << "Visual Examples: LFO and Envelope Signal Shaping\n";
    std::cout << "===============================================\n\n";

    // Generate LFO signal samples
    std::vector<float> lfoSignal;
    for (int i = 0; i < 20; ++i) {
        float phase = (float)i / 20.0f * 2.0f * M_PI;  // Full cycle
        lfoSignal.push_back(generateLfoSignal(phase));
    }

    std::cout << "Original LFO Signal:\n";
    std::cout << visualizeSignal(lfoSignal);

    // Apply bias and depth
    std::vector<float> lfoWithBiasDepth;
    for (float value : lfoSignal) {
        lfoWithBiasDepth.push_back(applyBiasDepthToSource(value, 20, 120));
    }

    std::cout << "LFO with Bias=20%, Depth=120%:\n";
    std::cout << visualizeSignal(lfoWithBiasDepth);

    // Apply different shapers
    std::vector<float> lfoWithCrease;
    for (float value : lfoWithBiasDepth) {
        lfoWithCrease.push_back(applyCreaseSource(value));
    }

    std::cout << "LFO with Crease Shaper (after bias/depth):\n";
    std::cout << visualizeSignal(lfoWithCrease);

    // Envelope shaper (stateful, so we need to process sequentially)
    std::vector<float> lfoWithEnvelope;
    float envState = 0.0f;
    for (float value : lfoWithBiasDepth) {
        lfoWithEnvelope.push_back(applyEnvelope(value, envState));
    }

    std::cout << "LFO with Envelope Shaper (after bias/depth):\n";
    std::cout << visualizeSignal(lfoWithEnvelope);

    // Triangle fold shaper
    std::vector<float> lfoWithTriangleFold;
    for (float value : lfoWithBiasDepth) {
        lfoWithTriangleFold.push_back(applyTriangleFold(value));
    }

    std::cout << "LFO with TriangleFold Shaper (after bias/depth):\n";
    std::cout << visualizeSignal(lfoWithTriangleFold);

    // Now with envelope signals
    std::vector<float> envSignal;
    for (int i = 0; i < 20; ++i) {
        float phase = (float)i / 20.0f;  // 0 to 1 over full cycle
        envSignal.push_back(generateEnvelopeSignal(phase));
    }

    std::cout << "\nOriginal Envelope Signal:\n";
    std::cout << visualizeSignal(envSignal);

    // Apply bias and depth to envelope
    std::vector<float> envWithBiasDepth;
    for (float value : envSignal) {
        envWithBiasDepth.push_back(applyBiasDepthToSource(value, -15, 80));
    }

    std::cout << "Envelope with Bias=-15%, Depth=80%:\n";
    std::cout << visualizeSignal(envWithBiasDepth);

    // Apply crease to envelope
    std::vector<float> envWithCrease;
    for (float value : envWithBiasDepth) {
        envWithCrease.push_back(applyCreaseSource(value));
    }

    std::cout << "Envelope with Crease Shaper (after bias/depth):\n";
    std::cout << visualizeSignal(envWithCrease);

    // Summary of effects
    std::cout << "\nSummary of Effects:\n";
    std::cout << "==================\n";
    std::cout << "Bias: Shifts the entire signal up or down\n";
    std::cout << "  - Positive bias shifts up\n";
    std::cout << "  - Negative bias shifts down\n";
    std::cout << "  - Applied as an offset after depth scaling\n\n";
    
    std::cout << "Depth: Scales the signal amplitude around the center point (0.5)\n";
    std::cout << "  - Values > 100% increase amplitude (can clip at 0/1)\n";
    std::cout << "  - Values < 100% decrease amplitude\n";
    std::cout << "  - 0% depth results in a constant 0.5 output\n\n";
    
    std::cout << "Shapers: Apply non-linear transformations to the signal\n";
    std::cout << "  - Crease: Creates a discontinuity at 0.5 by folding the signal\n";
    std::cout << "  - Envelope: Creates an envelope follower based on input amplitude\n";
    std::cout << "  - TriangleFold: Applies a triangular folding pattern\n";
    std::cout << "  - Location: Integrates the input to create a position accumulator\n";
    std::cout << "  - FrequencyFollower: Detects frequency by counting zero crossings\n";
    std::cout << "  - Activity: Measures signal activity based on changes\n";
    std::cout << "  - ProgressiveDivider: Creates binary output that divides based on input\n";
    std::cout << "  - VcaNext: Uses next route as a VCA for this route\n";

    return 0;
}