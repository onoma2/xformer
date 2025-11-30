#include <iostream>
#include <cmath>
#include <vector>
#include <algorithm>

// Current implementation from the codebase
static float applyWavefolder(float input, float fold, float gain, float symmetry) {
    // map from [0, 1] to [-1, 1]
    float bipolar_input = (input * 2.f) - 1.f;
    // apply symmetry
    float biased_input = bipolar_input + symmetry;
    // apply gain
    float gained_input = biased_input * gain;
    // apply folding using sine function. fold parameter controls frequency.
    // map fold from [0, 1] to a range of number of folds, e.g. 1 to 9
    float fold_count = 1.f + fold * 8.f;
    float folded_output = sinf(gained_input * M_PI * fold_count);
    // map back from [-1, 1] to [0, 1]
    return (folded_output + 1.f) * 0.5f;
}

static float applyDjFilter(float input, float &lpfState, float control, float resonance) {
    // 1. Dead zone
    if (control > -0.02f && control < 0.02f) {
        return input;
    }

    float alpha;
    if (control < 0.f) { // LPF Mode (knob left)
        alpha = 1.f - std::abs(control);
    } else { // HPF Mode (knob right)
        alpha = 0.1f + std::abs(control) * 0.85f;
    }
    alpha = std::clamp(alpha * alpha, 0.005f, 0.95f);

    // Add resonance (filter-to-filter feedback)
    // The feedback is from the previous output of the LPF (lpfState)
    float feedback = resonance * 4.f; // Scale resonance for a stronger effect
    float feedback_input = input - lpfState * feedback;

    // Update the internal LPF state
    lpfState = lpfState + alpha * (feedback_input - lpfState);

    // 2. Correct LPF/HPF mapping
    if (control < 0.f) { // LPF
        return lpfState;
    } else { // HPF
        return input - lpfState;
    }
}

// Enhanced implementation with amplitude compensation
static float applyWavefolderWithCompensation(float input, float fold, float gain, float symmetry, float &amplitude_tracker, float compensation_factor = 1.0f) {
    // map from [0, 1] to [-1, 1]
    float bipolar_input = (input * 2.f) - 1.f;
    // apply symmetry
    float biased_input = bipolar_input + symmetry;
    // apply gain
    float gained_input = biased_input * gain;
    // apply folding using sine function. fold parameter controls frequency.
    // map fold from [0, 1] to a range of number of folds, e.g. 1 to 9
    float fold_count = 1.f + fold * 8.f;
    float folded_output = sinf(gained_input * M_PI * fold_count);
    
    // Track the amplitude of the folded signal for compensation
    float signal_amplitude = std::abs(folded_output);
    amplitude_tracker = 0.9f * amplitude_tracker + 0.1f * signal_amplitude; // Smooth tracking
    
    // map back from [-1, 1] to [0, 1]
    float result = (folded_output + 1.f) * 0.5f;
    
    // Apply amplitude compensation based on the tracked amplitude
    if (amplitude_tracker > 0.01f) { // Avoid division by zero
        float desired_amplitude = 0.5f; // Target amplitude
        float compensation = desired_amplitude / amplitude_tracker;
        // Limit compensation to prevent excessive amplification
        compensation = std::min(compensation, 3.0f); // Maximum 3x amplification
        result = 0.5f + (result - 0.5f) * compensation * compensation_factor;
    }
    
    return std::clamp(result, 0.0f, 1.0f);
}

// Alternative: Pre-filtering approach to preserve harmonic content
static float applyDjFilterWithPreemphasis(float input, float &lpfState, float control, float resonance) {
    // 1. Dead zone
    if (control > -0.02f && control < 0.02f) {
        return input;
    }

    // Pre-emphasis: boost high frequencies before filtering to compensate for loss
    float pre_emphasis = 1.0f;
    if (control > 0.5f) { // Strong HPF mode
        pre_emphasis = 1.0f + (control - 0.5f) * 0.5f; // Up to 1.25x boost
    } else if (control < -0.5f) { // Strong LPF mode
        // For LPF, we might want to boost the input to compensate for attenuation
        pre_emphasis = 1.0f + std::abs(control + 0.5f) * 0.3f; // Up to 1.15x boost
    }
    
    float pre_emphasized_input = input * pre_emphasis;

    float alpha;
    if (control < 0.f) { // LPF Mode (knob left)
        alpha = 1.f - std::abs(control);
    } else { // HPF Mode (knob right)
        alpha = 0.1f + std::abs(control) * 0.85f;
    }
    alpha = std::clamp(alpha * alpha, 0.005f, 0.95f);

    // Add resonance (filter-to-filter feedback)
    float feedback = resonance * 4.f;
    float feedback_input = pre_emphasized_input - lpfState * feedback;

    // Update the internal LPF state
    lpfState = lpfState + alpha * (feedback_input - lpfState);

    float filtered_output;
    // 2. Correct LPF/HPF mapping
    if (control < 0.f) { // LPF
        filtered_output = lpfState;
    } else { // HPF
        filtered_output = pre_emphasized_input - lpfState;
    }
    
    // Post-emphasis compensation to prevent excessive amplification
    float post_compensation = 1.0f / pre_emphasis;
    return filtered_output * post_compensation;
}

// Combined approach: Amplitude compensation with feedback-aware filtering
static float applyWavefolderWithAdaptiveGain(float input, float fold, float gain, float symmetry, 
                                           float filter_control, float &gain_tracker, float &amplitude_tracker) {
    // Calculate adaptive gain based on filter settings
    float filter_compensation = 1.0f;
    if (std::abs(filter_control) > 0.5f) {
        // More compensation needed for extreme filter settings
        filter_compensation = 1.0f + std::abs(filter_control) * 0.5f;
    }
    
    // Adjust gain based on current tracking
    float adjusted_gain = gain * filter_compensation;
    
    // map from [0, 1] to [-1, 1]
    float bipolar_input = (input * 2.f) - 1.f;
    // apply symmetry
    float biased_input = bipolar_input + symmetry;
    // apply adjusted gain
    float gained_input = biased_input * adjusted_gain;
    // apply folding using sine function
    float fold_count = 1.f + fold * 8.f;
    float folded_output = sinf(gained_input * M_PI * fold_count);
    
    // Track amplitude for adaptive compensation
    float signal_amplitude = std::abs(folded_output);
    amplitude_tracker = 0.95f * amplitude_tracker + 0.05f * signal_amplitude;
    
    // map back from [-1, 1] to [0, 1]
    float result = (folded_output + 1.f) * 0.5f;
    
    return std::clamp(result, 0.0f, 1.0f);
}

// Demonstration of the problem and solution
void demonstrateAmplitudeReduction() {
    std::cout << "=== Amplitude Reduction Analysis ===" << std::endl;
    
    // Simulate a sine LFO input
    std::vector<float> lfo_signal;
    for (int i = 0; i < 100; i++) {
        float phase = (float(i) / 100.0f) * 2.0f * M_PI;
        lfo_signal.push_back((sinf(phase) + 1.0f) / 2.0f); // Normalize to [0,1]
    }
    
    // Process with original algorithm
    std::vector<float> processed_original;
    float lpfState_original = 0.0f;
    for (float input : lfo_signal) {
        float folded = applyWavefolder(input, 0.7f, 2.0f, 0.0f); // High fold, moderate gain
        float filtered = applyDjFilter(folded, lpfState_original, -0.8f, 0.2f); // Strong LPF
        processed_original.push_back(filtered);
    }
    
    // Calculate amplitude statistics for original
    float original_min = *std::min_element(processed_original.begin(), processed_original.end());
    float original_max = *std::max_element(processed_original.begin(), processed_original.end());
    float original_range = original_max - original_min;
    
    std::cout << "Original processing - Min: " << original_min << ", Max: " << original_max 
              << ", Range: " << original_range << std::endl;
    
    // Process with compensation
    std::vector<float> processed_compensated;
    float lpfState_compensated = 0.0f;
    float amplitude_tracker = 0.5f;
    for (float input : lfo_signal) {
        float folded = applyWavefolderWithCompensation(input, 0.7f, 2.0f, 0.0f, amplitude_tracker, 1.2f);
        float filtered = applyDjFilterWithPreemphasis(folded, lpfState_compensated, -0.8f, 0.2f);
        processed_compensated.push_back(filtered);
    }
    
    // Calculate amplitude statistics for compensated
    float comp_min = *std::min_element(processed_compensated.begin(), processed_compensated.end());
    float comp_max = *std::max_element(processed_compensated.begin(), processed_compensated.end());
    float comp_range = comp_max - comp_min;
    
    std::cout << "Compensated processing - Min: " << comp_min << ", Max: " << comp_max 
              << ", Range: " << comp_range << std::endl;
    std::cout << "Improvement factor: " << (comp_range / original_range) << "x" << std::endl;
}

int main() {
    demonstrateAmplitudeReduction();
    return 0;
}