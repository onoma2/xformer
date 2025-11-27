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
    if (output.empty()) return;

    // Calculate DAC resolution constraints (quantization)
    int maxDigitalValue = (1 << params.dacResolutionBits) - 1; // 2^bits - 1
    float voltageRange = 10.0f; // Eurorack standard is typically ±5V = 10V total range
    float quantizationStep = voltageRange / maxDigitalValue;

    // For PEW|FORMER hardware, CV updates happen every 'dacUpdateRate' ms regardless of musical timing
    // Calculate how many samples correspond to the specified update interval based on the sample rate
    float samplesPerMs = sampleRate / 1000.0f;
    int updateInterval = static_cast<int>(params.dacUpdateRate * samplesPerMs); // samples per update interval
    if (updateInterval < 1) updateInterval = 1; // Ensure at least every sample

    // Track the last valid value to use when skipping samples due to update rate
    float lastValidValue = output[0];

    for (size_t i = 0; i < output.size(); ++i) {
        // Apply quantization (DAC resolution)
        float quantizedValue = roundf(output[i] / quantizationStep) * quantizationStep;

        // Apply update rate constraints (based on real hardware: updates every dacUpdateRate ms)
        bool shouldUpdate = (i % updateInterval == 0);

        if (shouldUpdate) {
            output[i] = quantizedValue;
            lastValidValue = quantizedValue;
        } else {
            // Use the last valid value when update is skipped
            output[i] = lastValidValue;
        }
    }
}

static float applyDjFilter(float input, float &lpfState, float control, float resonance, const CurveProcessor::AdvancedParameters& advanced) {
    if (control > -0.02f && control < 0.02f) return input;
    float alpha = (control < 0.f) ? 1.f - std::abs(control) : 0.1f + std::abs(control) * advanced.hpfCurve;
    alpha = std::clamp(alpha * alpha, 0.005f, 0.95f);
    float logResonance = linearToLogarithmicFeedback(resonance, advanced);
    if (std::abs(control) > 0.7f) logResonance *= (1.0f - (std::abs(control) - 0.7f) * advanced.resonanceTame);
    float feedback = logResonance * advanced.resonanceGain;
    float feedback_input = input - lpfState * feedback;
    lpfState = lpfState + alpha * (feedback_input - lpfState);
    lpfState = std::max(-6.0f, std::min(6.0f, lpfState));
    return (control < 0.f) ? lpfState : input - lpfState;
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

CurveProcessor::CurveProcessor(int bufferSize) : _bufferSize(bufferSize), _lpfState(0.0f), _feedbackState(0.0f) {}

static void process_once(const CurveProcessor::Parameters& params, CurveProcessor::SignalData& data, int size, float& lpfState, float& feedbackState) {
    const auto& range = voltageRangeInfo(params.range);
    auto curveFunc = Curve::function(params.shape);

    for (int i = 0; i < size; ++i) {
        float fraction = fmod((float(i) + params.globalPhase * size) / size, 1.0f);
        if (fraction < 0) fraction += 1.0f;

        float value = curveFunc(fraction);
        if (params.invert) value = 1.f - value;
        float normalizedValue = params.min + value * (params.max - params.min);

        if (i < data.originalSignal.size()) data.originalSignal[i] = normalizedValue;

        float originalVoltage = range.denormalize(normalizedValue);

        float folderInput = normalizedValue;
        if (params.wavefolderFold > 0.0f) {
            float logShaperFeedback = linearToLogarithmicFeedback(params.foldF, params.advanced);
            folderInput += feedbackState * logShaperFeedback;
            float gain = 1.0f + (params.wavefolderGain * 2.0f);
            folderInput = applyWavefolder(folderInput, params.wavefolderFold * params.wavefolderFold, gain, params.wavefolderSymmetry, params.advanced);
        }
        if (i < data.postWavefolder.size()) data.postWavefolder[i] = std::max(0.0f, std::min(1.0f, folderInput));

        float voltage = range.denormalize(folderInput);
        if (i < data.postFilter.size()) data.postFilter[i] = std::max(-5.0f, std::min(5.0f, applyDjFilter(voltage, lpfState, params.djFilter, params.filterF, params.advanced)));

        float uncompensatedVoltage = data.postFilter[i];
        float compensatedVoltage = uncompensatedVoltage;
        if (params.djFilter < -0.02f || params.djFilter > 0.02f) {
            compensatedVoltage *= calculateAmplitudeCompensation(params.wavefolderFold, params.djFilter, params.filterF, params.advanced);
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
    data.postWavefolder.resize(_bufferSize);
    data.postFilter.resize(_bufferSize);
    data.postCompensation.resize(_bufferSize);
    data.finalOutput.resize(_bufferSize);
    process_once(params, data, _bufferSize, _lpfState, _feedbackState);

    // Compute spectrum
    const std::vector<float>* source_vector = nullptr;
    switch (params.spectrumSource) {
        case SpectrumSource::Input: source_vector = &data.originalSignal; break;
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
    oversample_data.postWavefolder.resize(oversample_size);
    oversample_data.postFilter.resize(oversample_size);
    oversample_data.postCompensation.resize(oversample_size);
    oversample_data.finalOutput.resize(oversample_size);
    float oversample_lpfState = 0.f;
    float oversample_feedbackState = 0.f;
    process_once(params, oversample_data, oversample_size, oversample_lpfState, oversample_feedbackState);

    const std::vector<float>* oversample_source_vector = nullptr;
    switch (params.spectrumSource) {
        case SpectrumSource::Input: oversample_source_vector = &oversample_data.originalSignal; break;
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
    _lpfState = 0.0f;
    _feedbackState = 0.0f;
}