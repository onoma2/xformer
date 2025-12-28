/*
 * Code snippets demonstrating how bias and depth affect incoming signals in the routing system
 * 
 * This example shows how bias and depth parameters modify normalized source signals (0.0 to 1.0)
 * before they are applied to routing targets.
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

int main() {
    std::cout << "Routing System: Bias and Depth Effects on Incoming Signals\n";
    std::cout << "=========================================================\n\n";

    // Example 1: LFO signal with different bias/depth settings
    std::cout << "Example 1: LFO Signal Processing\n";
    std::cout << "Original LFO (no bias/depth):\n";
    
    for (int i = 0; i < 20; ++i) {
        float phase = (float)i / 20.0f * 2.0f * M_PI;  // Full cycle
        float lfo = generateLfoSignal(phase);
        std::cout << "  Phase " << i << ": " << lfo << "\n";
    }
    
    std::cout << "\nWith Bias=25%, Depth=100% (shift up by 0.25):\n";
    for (int i = 0; i < 5; ++i) {  // Just show first 5 for brevity
        float phase = (float)i / 20.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyBiasDepthToSource(lfo, 25, 100);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith Bias=0%, Depth=50% (reduce amplitude around center):\n";
    for (int i = 0; i < 5; ++i) {
        float phase = (float)i / 20.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyBiasDepthToSource(lfo, 0, 50);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith Bias=-20%, Depth=150% (shift down, increase amplitude):\n";
    for (int i = 0; i < 5; ++i) {
        float phase = (float)i / 20.0f * 2.0f * M_PI;
        float lfo = generateLfoSignal(phase);
        float processed = applyBiasDepthToSource(lfo, -20, 150);
        std::cout << "  Phase " << i << ": " << lfo << " -> " << processed << "\n";
    }

    // Example 2: Envelope signal with different bias/depth settings
    std::cout << "\n\nExample 2: Envelope Signal Processing\n";
    std::cout << "Original Envelope:\n";
    
    for (int i = 0; i < 20; ++i) {
        float phase = (float)i / 20.0f;  // 0 to 1 over full cycle
        float env = generateEnvelopeSignal(phase);
        std::cout << "  Phase " << i << ": " << env << "\n";
    }
    
    std::cout << "\nWith Bias=10%, Depth=80%:\n";
    for (int i = 0; i < 5; ++i) {
        float phase = (float)i / 20.0f;
        float env = generateEnvelopeSignal(phase);
        float processed = applyBiasDepthToSource(env, 10, 80);
        std::cout << "  Phase " << i << ": " << env << " -> " << processed << "\n";
    }
    
    std::cout << "\nWith Bias=-15%, Depth=120%:\n";
    for (int i = 0; i < 5; ++i) {
        float phase = (float)i / 20.0f;
        float env = generateEnvelopeSignal(phase);
        float processed = applyBiasDepthToSource(env, -15, 120);
        std::cout << "  Phase " << i << ": " << env << " -> " << processed << "\n";
    }

    return 0;
}