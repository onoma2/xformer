#include "CurveProcessor.h"
#include "Curve.h"
#include <cmath>
#include <algorithm>
#include <vector>

// Helper function to compute spectrum
static std::vector<float> computeSpectrum(const std::vector<float>& signal, bool window) {
    (void)window; // TODO: add windowing
    int size = signal.size();
    dj::fft_arg<float> fft_data(size);
    for(int i = 0; i < size; ++i) {
        fft_data[i] = {signal[i], 0.0f};
    }

    auto fft_result = dj::fft1d(fft_data, dj::fft_dir::DIR_FWD);

    std::vector<float> spectrum(size / 2);
    for(int i = 0; i < size / 2; ++i) {
        float mag = std::abs(fft_result[i]);
        spectrum[i] = 20.0f * log10(mag + 1e-6);
    }
    return spectrum;
}

static const VoltageRangeInfo voltageRangeInfos[] = {
    [int(VoltageRange::Unipolar1V)]  = { 0.f, 1.f },
    [int(VoltageRange::Unipolar2V)]  = { 0.f, 2.f },
    [int(VoltageRange::Unipolar3V)]  = { 0.f, 3.f },
    [int(VoltageRange::Unipolar4V)]  = { 0.f, 4.f },
    [int(VoltageRange::Unipolar5V)]  = { 0.f, 5.f },
    [int(VoltageRange::Bipolar1V)]   = { -1.f, 1.f },
    [int(VoltageRange::Bipolar2V)]   = { -2.f, 2.f },
    [int(VoltageRange::Bipolar3V)]   = { -3.f, 3.f },
    [int(VoltageRange::Bipolar4V)]   = { -4.f, 4.f },
    [int(VoltageRange::Bipolar5V)]   = { -5.f, 5.f },
};

const VoltageRangeInfo& voltageRangeInfo(VoltageRange range) {
    return voltageRangeInfos[int(range)];
}

const char* voltageRangeName(VoltageRange range) {
    switch (range) {
        case VoltageRange::Unipolar1V: return "1V Unipolar";
        case VoltageRange::Unipolar2V: return "2V Unipolar";
        case VoltageRange::Unipolar3V: return "3V Unipolar";
        case VoltageRange::Unipolar4V: return "4V Unipolar";
        case VoltageRange::Unipolar5V: return "5V Unipolar";
        case VoltageRange::Bipolar1V:  return "1V Bipolar";
        case VoltageRange::Bipolar2V:  return "2V Bipolar";
        case VoltageRange::Bipolar3V:  return "3V Bipolar";
        case VoltageRange::Bipolar4V:  return "4V Bipolar";
        case VoltageRange::Bipolar5V:  return "5V Bipolar";
        default: return "Unknown";
    }
}


// Convert linear feedback value to logarithmic scale for smoother control
static float linearToLogarithmicFeedback(float linearValue, const CurveProcessor::AdvancedParameters& advanced) {
    if (linearValue <= 0.0f) return 0.0f;
    if (linearValue >= 1.0f) return 1.0f;
    // The curve is controlled by feedbackCurve, where 1.0 is the default log curve
    float curve = 1.0f + (advanced.feedbackCurve - 1.0f) * 9.0f;
    return log10f(linearValue * curve + 1.0f) / log10f(curve + 1.0f);
}

// Apply hardware constraints to the signal
static void applyHardwareConstraints(std::vector<float>& output, const CurveProcessor::Parameters& params, int sampleRate) {
    (void)sampleRate; // unused in this simplified logic
    if (output.empty()) return;

    // Calculate DAC resolution constraints (quantization)
    int maxDigitalValue = (1 << params.dacResolutionBits) - 1; // 2^bits - 1
    float voltageRange = 10.0f; // Eurorack standard is typically ±5V = 10V total range
    float quantizationStep = voltageRange / maxDigitalValue;

    // Calculate step size: How many buffer samples per hardware update?
    // Buffer represents exactly 1 cycle.
    // Period (ms) = 1000.0f / params.frequency
    // Updates per period = Period / params.dacUpdateRate
    // Samples per update = BufferSize / Updates per period
    //                    = BufferSize / ( (1000.0f / params.frequency) / params.dacUpdateRate )
    //                    = BufferSize * params.dacUpdateRate * params.frequency / 1000.0f;
    
    float samplesPerUpdate = output.size() * params.dacUpdateRate * params.frequency / 1000.0f;
    
    // If samplesPerUpdate < 1, hardware is faster than our buffer resolution -> No skipping
    if (samplesPerUpdate < 1.0f) {
        for (size_t i = 0; i < output.size(); ++i) {
             output[i] = roundf(output[i] / quantizationStep) * quantizationStep;
        }
        return;
    }

    // Accumulator logic for sample-and-hold
    float accumulator = samplesPerUpdate; // Start ready to update
    float lastValidValue = output[0];

    for (size_t i = 0; i < output.size(); ++i) {
        accumulator += 1.0f;

        if (accumulator >= samplesPerUpdate) {
            // Time to update
            float quantizedValue = roundf(output[i] / quantizationStep) * quantizationStep;
            output[i] = quantizedValue;
            lastValidValue = quantizedValue;
            accumulator -= samplesPerUpdate; // Keep remainder
        } else {
            // Hold previous value
            output[i] = lastValidValue;
        }
    }
}

static float applyDjFilter(float input, std::vector<float>& states, float control, float resonance, FilterSlope slope, const CurveProcessor::AdvancedParameters& advanced) {
    if (control > -0.02f && control < 0.02f) return input;

    // Determine number of poles (stages)
    int stages = 1;
    if (slope == FilterSlope::dB12) stages = 2;
    else if (slope == FilterSlope::dB24) stages = 4;

    // Ensure we have enough state variables
    if (states.size() < (size_t)stages) states.resize(stages, 0.0f);

    float alpha = (control < 0.f) ? 1.f - std::abs(control) : 0.1f + std::abs(control) * advanced.hpfCurve;
    alpha = std::clamp(alpha * alpha, 0.005f, 0.95f);
    float logResonance = linearToLogarithmicFeedback(resonance, advanced);
    if (std::abs(control) > 0.7f) logResonance *= (1.0f - (std::abs(control) - 0.7f) * advanced.resonanceTame);
    float feedback = logResonance * advanced.resonanceGain;
    
    // Use first stage for feedback (or average?) - sticking to first stage to maintain character
    float feedback_input = input - states[0] * feedback;

    float currentInput = feedback_input;

    for (int i = 0; i < stages; ++i) {
        states[i] = states[i] + alpha * (currentInput - states[i]);
        states[i] = std::max(-6.0f, std::min(6.0f, states[i]));
        currentInput = states[i];
    }

    return (control < 0.f) ? currentInput : input - currentInput;
}

static float applyWavefolder(float input, float fold, float gain, float symmetry, const CurveProcessor::AdvancedParameters& advanced) {
    float bipolar_input = (input * 2.f) - 1.f;
    float biased_input = bipolar_input + symmetry;
    float gained_input = biased_input * gain;
    float fold_count = 1.f + fold * advanced.foldAmount;
    float folded_output = sinf(gained_input * float(M_PI) * fold_count);
    return (folded_output + 1.f) * 0.5f;
}

static float applyLfoLimiting(float input, float resonance, const CurveProcessor::AdvancedParameters& advanced) {
    float threshold = 5.0f - (resonance * advanced.lfoLimiterAmount);
    float maxThreshold = 5.0f;
    threshold = std::max(advanced.lfoLimiterMin, std::min(threshold, maxThreshold));
    return std::max(-maxThreshold, std::min(maxThreshold, input));
}

static float calculateAmplitudeCompensation(float fold, float filterControl, float filterResonance, const CurveProcessor::AdvancedParameters& advanced) {
    if (fold < 0.01f) return 1.0f;
    float foldCompensation = 1.0f + (fold * advanced.foldComp);
    float filterCompensation = 1.0f;
    if (filterControl < 0.0f) {
        filterCompensation = 1.0f + (std::abs(filterControl) * advanced.lpfComp);
    } else if (filterControl > 0.0f) {
        filterCompensation = 1.0f + (filterControl * advanced.hpfComp);
    }
    float resonanceCompensation = 1.0f + (filterResonance * advanced.resComp);
    float compensation = foldCompensation * filterCompensation * resonanceCompensation;
    return std::clamp(compensation, 1.0f, advanced.maxComp);
}

// Hardware Analysis Helpers
static float calculateMaxSlewRate(const std::vector<float>& signal) {
    if (signal.empty()) return 0.0f;
    float maxDiff = 0.0f;
    for (size_t i = 1; i < signal.size(); ++i) {
        float diff = std::abs(signal[i] - signal[i-1]);
        if (diff > maxDiff) maxDiff = diff;
    }
    return maxDiff;
}

static int calculateAlgoComplexity(const CurveProcessor::Parameters& params) {
    int score = 1; // Base cost for phase/shape generation
    
    // Wavefolder cost (expensive sin/cos)
    if (params.wavefolderFold > 0.0f) {
        score += 10; 
        if (params.foldF > 0.0f) score += 2; // Feedback adds complexity
    }

    // Filter cost (multiplications/state updates)
    if (std::abs(params.djFilter) > 0.02f) {
        int stages = 1;
        if (params.filterSlope == FilterSlope::dB12) stages = 2;
        if (params.filterSlope == FilterSlope::dB24) stages = 4;
        score += (3 * stages);
        if (params.filterF > 0.0f) score += 2; // Resonance math
    }

    // Compensation cost (logic + mults)
    bool usingFold = params.wavefolderFold > 0.01f;
    bool usingFilter = std::abs(params.djFilter) > 0.02f;
    if (usingFold || usingFilter) {
        score += 2;
    }

    return score;
}

static float calculateClippingPercent(const std::vector<float>& signal) {
    if (signal.empty()) return 0.0f;
    int clippedSamples = 0;
    const float limit = 5.0f - 0.01f; // Close to 5V rail
    const float negLimit = -5.0f + 0.01f; // Close to -5V rail

    for (float val : signal) {
        if (val >= limit || val <= negLimit) {
            clippedSamples++;
        }
    }
    return (float)clippedSamples / signal.size() * 100.0f;
}

CurveProcessor::CurveProcessor(int bufferSize) : _bufferSize(bufferSize), _feedbackState(0.0f) {
    _lpfState.resize(4, 0.0f); // Pre-allocate for max 4 stages
}

static float applyPhaseSkew(float phase, float skew) {
    // Simple power curve skewing: phase^exp
    // Skew 0 -> exp 1 (Linear)
    // Skew -1 -> exp 0.25 (Rushing / Logarithmic)
    // Skew +1 -> exp 4.0 (Dragging / Exponential)
    
    // Map skew -1..1 to exponent 0.25..4.0
    float exponent;
    if (skew >= 0) {
        exponent = 1.0f + skew * 3.0f; // 1.0 to 4.0
    } else {
        exponent = 1.0f / (1.0f + std::abs(skew) * 3.0f); // 1.0 to 0.25
    }
    
    return powf(phase, exponent);
}

static void process_once(const CurveProcessor::Parameters& params, CurveProcessor::SignalData& data, int size, std::vector<float>& lpfState, float& feedbackState) {
    const auto& range = voltageRangeInfo(params.range);
    auto curveFunc = Curve::function(params.shape);

    for (int i = 0; i < size; ++i) {
        float fraction = fmod((float(i) + params.globalPhase * size) / size, 1.0f);
        if (fraction < 0) fraction += 1.0f;

        // Calculate Phase Skew (with Feedback)
        float dynamicSkew = params.phaseSkew;
        
        // Feedback: Shape -> Skew (using previous sample's shape value would be ideal, but recursive is fine)
        // We can use 'data.originalSignal[i-1]' if available, but for simplicity let's use feedbackState (processed)
        // Or better, let's use the "previous loop's normalized value" concept. 
        // Since this is a tight loop, we don't have easy access to "last frame's shape at this index".
        // We will use 'feedbackState' (Filter Output) for the Filter->Skew path.
        
        if (std::abs(params.filterToPhaseSkew) > 0.001f) {
             float modSource = std::max(-1.0f, std::min(1.0f, feedbackState / 5.0f));
             dynamicSkew += modSource * params.filterToPhaseSkew;
        }
        
        // Feedback: Shape -> Skew requires the shape itself. 
        // We haven't calculated the shape yet! This creates a causality dilemma.
        // Solution: Use the *unskewed* shape to modulate the skew of the *skewed* shape.
        // It's a subtle but valid effect.
        if (std::abs(params.shapeToPhaseSkew) > 0.001f) {
             float tempShape = curveFunc(fraction); // Unskewed shape
             dynamicSkew += (tempShape - 0.5f) * 2.0f * params.shapeToPhaseSkew;
        }

        dynamicSkew = std::max(-1.0f, std::min(1.0f, dynamicSkew));
        
        // Apply Skew
        float skewedFraction = applyPhaseSkew(fraction, dynamicSkew);
        if (i < data.skewedPhase.size()) data.skewedPhase[i] = skewedFraction;

        float value = curveFunc(skewedFraction);
        if (params.invert) value = 1.f - value;
        float normalizedValue = params.min + value * (params.max - params.min);

        if (i < data.originalSignal.size()) data.originalSignal[i] = normalizedValue;

        float originalVoltage = range.denormalize(normalizedValue);

        // 1. Shape -> Wavefolder Fold Feedback
        // Modulates the 'wavefolderFold' parameter based on the original shape intensity
        float dynamicFold = params.wavefolderFold;
        if (std::abs(params.shapeToWavefolderFold) > 0.001f) {
            dynamicFold += (normalizedValue - 0.5f) * 2.0f * params.shapeToWavefolderFold; // Bipolar modulation
            dynamicFold = std::max(0.0f, std::min(1.0f, dynamicFold));
        }

        // 3. Filter Output -> Wavefolder Fold Feedback (Uses previous sample's filter output)
        // We use the previous loop's 'processedSignal' or 'uncompensatedVoltage' indirectly via a state variable if needed.
        // But since we don't have sample-delay state for this specific path readily available in this simplified loop, 
        // we can use the 'feedbackState' which captures the processed signal.
        if (std::abs(params.filterToWavefolderFold) > 0.001f) {
             // feedbackState is roughly the previous processed signal
             // Normalize -5V..5V to -1..1 for modulation
             float modSource = std::max(-1.0f, std::min(1.0f, feedbackState / 5.0f));
             dynamicFold += modSource * params.filterToWavefolderFold;
             dynamicFold = std::max(0.0f, std::min(1.0f, dynamicFold));
        }

        float folderInput = normalizedValue;
        if (dynamicFold > 0.0f) {
            float logShaperFeedback = linearToLogarithmicFeedback(params.foldF, params.advanced);
            folderInput += feedbackState * logShaperFeedback;
            float gain = 1.0f + (params.wavefolderGain * 2.0f);
            folderInput = applyWavefolder(folderInput, dynamicFold * dynamicFold, gain, params.wavefolderSymmetry, params.advanced);
        }
        if (i < data.postWavefolder.size()) data.postWavefolder[i] = std::max(0.0f, std::min(1.0f, folderInput));

        float voltage = range.denormalize(folderInput);

        // 2. Fold Output -> Filter Freq Feedback
        // Modulates filter cutoff based on the wavefolder output
        float dynamicFilter = params.djFilter;
        if (std::abs(params.foldToFilterFreq) > 0.001f) {
            // folderInput is 0..1 (unipolar). Center it to -0.5..0.5 for modulation? Or use as is.
            // Let's map 0..1 to -1..1 for full range bipolar modulation
            float modSource = (folderInput - 0.5f) * 2.0f; 
            dynamicFilter += modSource * params.foldToFilterFreq;
            dynamicFilter = std::max(-1.0f, std::min(1.0f, dynamicFilter));
        }

        if (i < data.postFilter.size()) data.postFilter[i] = std::max(-5.0f, std::min(5.0f, applyDjFilter(voltage, lpfState, dynamicFilter, params.filterF, params.filterSlope, params.advanced)));

        float uncompensatedVoltage = data.postFilter[i];
        float compensatedVoltage = uncompensatedVoltage;
        if (dynamicFilter < -0.02f || dynamicFilter > 0.02f) {
            compensatedVoltage *= calculateAmplitudeCompensation(dynamicFold, dynamicFilter, params.filterF, params.advanced);
        }
        // For visualization purposes, clamp postCompensation to prevent extreme values in plots
        if (i < data.postCompensation.size()) data.postCompensation[i] = std::max(-5.0f, std::min(5.0f, compensatedVoltage));

        float processedSignal = compensatedVoltage;

        voltage = originalVoltage * (1.0f - params.xFade) + compensatedVoltage * params.xFade;

        // Apply LFO limiting to the crossfaded voltage (not just the compensated voltage)
        voltage = applyLfoLimiting(voltage, params.filterF, params.advanced);

        feedbackState = std::max(-params.advanced.feedbackLimit, std::min(params.advanced.feedbackLimit, processedSignal));

        if (i < data.finalOutput.size()) data.finalOutput[i] = std::max(-5.0f, std::min(5.0f, voltage));
    }
}


#include <chrono>

CurveProcessor::SignalData CurveProcessor::process(const CurveProcessor::Parameters& params, int sampleRate) {
    (void)sampleRate;

    // Start timing the main process
    auto start = std::chrono::high_resolution_clock::now();

    SignalData data;

    // Process at normal sample rate
    data.originalSignal.resize(_bufferSize);
    data.phasedSignal.resize(_bufferSize); // Not used anymore, but let's keep it for now
    data.skewedPhase.resize(_bufferSize);  // NEW: Visualizing the phase warp
    data.postWavefolder.resize(_bufferSize);
    data.postFilter.resize(_bufferSize);
    data.postCompensation.resize(_bufferSize);
    data.finalOutput.resize(_bufferSize);
    process_once(params, data, _bufferSize, _lpfState, _feedbackState);

    // Compute spectrum
    const std::vector<float>* source_vector = nullptr;
    switch (params.spectrumSource) {
        case SpectrumSource::Input: source_vector = &data.originalSignal; break;
        case SpectrumSource::SkewedPhase: source_vector = &data.skewedPhase; break;
        case SpectrumSource::PostWavefolder: source_vector = &data.postWavefolder; break;
        case SpectrumSource::PostFilter: source_vector = &data.postFilter; break;
        case SpectrumSource::PostCompensation: source_vector = &data.postCompensation; break;
        case SpectrumSource::FinalOutput: source_vector = &data.finalOutput; break;
        default: source_vector = &data.finalOutput; break;
    }
    data.spectrum = computeSpectrum(*source_vector, true);

    // Process at 2x oversample rate for aliasing analysis
    int oversample_size = _bufferSize * 2;
    SignalData oversample_data;
    oversample_data.originalSignal.resize(oversample_size);
    oversample_data.skewedPhase.resize(oversample_size); // Resize oversampled buffer
    oversample_data.postWavefolder.resize(oversample_size);
    oversample_data.postFilter.resize(oversample_size);
    oversample_data.postCompensation.resize(oversample_size);
    oversample_data.finalOutput.resize(oversample_size);
    std::vector<float> oversample_lpfState(4, 0.0f);
    float oversample_feedbackState = 0.f;
    process_once(params, oversample_data, oversample_size, oversample_lpfState, oversample_feedbackState);

    const std::vector<float>* oversample_source_vector = nullptr;
    switch (params.spectrumSource) {
        case SpectrumSource::Input: oversample_source_vector = &oversample_data.originalSignal; break;
        case SpectrumSource::SkewedPhase: oversample_source_vector = &oversample_data.skewedPhase; break;
        case SpectrumSource::PostWavefolder: oversample_source_vector = &oversample_data.postWavefolder; break;
        case SpectrumSource::PostFilter: oversample_source_vector = &oversample_data.postFilter; break;
        case SpectrumSource::PostCompensation: oversample_source_vector = &oversample_data.postCompensation; break;
        case SpectrumSource::FinalOutput: oversample_source_vector = &oversample_data.finalOutput; break;
        default: oversample_source_vector = &oversample_data.finalOutput; break;
    }
    data.spectrum_oversampled = computeSpectrum(*oversample_source_vector, true);

    // Create a copy of the final output for hardware constraint simulation
    data.hardwareLimitedOutput = data.finalOutput;

    // Apply hardware constraints to create the limited output
    applyHardwareConstraints(data.hardwareLimitedOutput, params, sampleRate);

    // Calculate Hardware Safety Stats
    m_hardwareStats.maxSlewRate = calculateMaxSlewRate(data.hardwareLimitedOutput);
    m_hardwareStats.algoComplexityScore = calculateAlgoComplexity(params);
    m_hardwareStats.clippingPercent = calculateClippingPercent(data.hardwareLimitedOutput);

    // Store last output for real-time access
    if (!data.hardwareLimitedOutput.empty()) {
        m_lastHardwareOutput = data.hardwareLimitedOutput.back();
    }

    // End timing and calculate performance metrics
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    float totalProcessTimeMs = duration.count() / 1000.0f;  // Convert to milliseconds

    // Calculate time per sample (average)
    float timePerSampleMs = totalProcessTimeMs / _bufferSize;

    m_performance.processTimeMs = totalProcessTimeMs;
    m_performance.sampleRate = sampleRate;
    m_performance.timeBudgetMs = 1000.0f / sampleRate;  // Time budget per sample in ms
    m_performance.cpuUsagePercent = (timePerSampleMs / m_performance.timeBudgetMs) * 100.0f;

    return data;
}

void CurveProcessor::resetStates() {
    std::fill(_lpfState.begin(), _lpfState.end(), 0.0f);
    _feedbackState = 0.0f;
}