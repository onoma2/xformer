#include "CurveProcessor.h"
#include "Curve.h"
#include <cmath>
#include <algorithm>
#include <vector>

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
static float linearToLogarithmicFeedback(float linearValue) {
    if (linearValue <= 0.0f) return 0.0f;
    if (linearValue >= 1.0f) return 1.0f;
    return log10f(linearValue * 9.0f + 1.0f) / log10f(10.0f);
}

static float applyDjFilter(float input, float &lpfState, float control, float resonance) {
    if (control > -0.02f && control < 0.02f) return input;
    float alpha = (control < 0.f) ? 1.f - std::abs(control) : 0.1f + std::abs(control) * 0.85f;
    alpha = std::clamp(alpha * alpha, 0.005f, 0.95f);
    float logResonance = linearToLogarithmicFeedback(resonance);
    if (std::abs(control) > 0.7f) logResonance *= (1.0f - (std::abs(control) - 0.7f) * 0.8f);
    float feedback = logResonance * 1.5f;
    float feedback_input = input - lpfState * feedback;
    lpfState = lpfState + alpha * (feedback_input - lpfState);
    lpfState = std::max(-6.0f, std::min(6.0f, lpfState));
    return (control < 0.f) ? lpfState : input - lpfState;
}

static float applyWavefolder(float input, float fold, float gain, float symmetry) {
    float bipolar_input = (input * 2.f) - 1.f;
    float biased_input = bipolar_input + symmetry;
    float gained_input = biased_input * gain;
    float fold_count = 1.f + fold * 8.f;
    float folded_output = sinf(gained_input * float(M_PI) * fold_count);
    return (folded_output + 1.f) * 0.5f;
}

static float applyLfoLimiting(float input, float resonance) {
    float threshold = 5.0f - (resonance * 3.0f);
    float maxThreshold = 5.0f;
    threshold = std::max(0.5f, std::min(threshold, maxThreshold));
    return std::max(-maxThreshold, std::min(maxThreshold, input));
}

static float calculateAmplitudeCompensation(float fold, float filterControl, float filterResonance) {
    if (fold < 0.01f) return 1.0f;
    float foldCompensation = 1.0f + (fold * 0.8f);
    float filterCompensation = 1.0f;
    if (filterControl < 0.0f) {
        filterCompensation = 1.0f + (std::abs(filterControl) * 0.3f);
    } else if (filterControl > 0.0f) {
        filterCompensation = 1.0f + (filterControl * 0.5f);
    }
    float resonanceCompensation = 1.0f + (filterResonance * 0.1f);
    float compensation = foldCompensation * filterCompensation * resonanceCompensation;
    return std::clamp(compensation, 1.0f, 2.5f);
}

CurveProcessor::CurveProcessor(int bufferSize) : _bufferSize(bufferSize), _lpfState(0.0f), _feedbackState(0.0f) {}

CurveProcessor::SignalData CurveProcessor::process(const CurveProcessor::Parameters& params, int sampleRate) {
    SignalData data;
    data.originalSignal.resize(_bufferSize);
    data.phasedSignal.resize(_bufferSize);
    data.postWavefolder.resize(_bufferSize);
    data.postFilter.resize(_bufferSize);
    data.postCompensation.resize(_bufferSize);
    data.finalOutput.resize(_bufferSize);

    const auto& range = voltageRangeInfo(params.range);
    float phaseOffset = params.globalPhase * _bufferSize;
    auto curveFunc = Curve::function(params.shape);

    for (int i = 0; i < _bufferSize; ++i) {
        float fraction = fmod((float(i) + phaseOffset) / _bufferSize, 1.0f);
        if (fraction < 0) fraction += 1.0f;

        float value = curveFunc(fraction);
        if (params.invert) value = 1.f - value;
        float normalizedValue = params.min + value * (params.max - params.min);

        data.originalSignal[i] = normalizedValue;
        data.phasedSignal[i] = normalizedValue;

        float originalVoltage = range.denormalize(normalizedValue);

        float folderInput = normalizedValue;
        if (params.wavefolderFold > 0.0f) {
            float logShaperFeedback = linearToLogarithmicFeedback(params.foldF);
            folderInput += _feedbackState * logShaperFeedback;
            float gain = 1.0f + (params.wavefolderGain * 2.0f);
            folderInput = applyWavefolder(folderInput, params.wavefolderFold * params.wavefolderFold, gain, params.wavefolderSymmetry);
        }
        data.postWavefolder[i] = folderInput;

        float voltage = range.denormalize(folderInput);
        data.postFilter[i] = applyDjFilter(voltage, _lpfState, params.djFilter, params.filterF);

        float compensatedVoltage = data.postFilter[i];
        if (params.djFilter < -0.02f || params.djFilter > 0.02f) {
            compensatedVoltage *= calculateAmplitudeCompensation(params.wavefolderFold, params.djFilter, params.filterF);
        }
        data.postCompensation[i] = compensatedVoltage;
        
        float processedSignal = compensatedVoltage;
        
        voltage = originalVoltage * (1.0f - params.xFade) + compensatedVoltage * params.xFade;
        
        processedSignal = applyLfoLimiting(processedSignal, params.filterF);
        
        _feedbackState = std::max(-4.0f, std::min(4.0f, processedSignal));
        
        data.finalOutput[i] = std::max(-5.0f, std::min(5.0f, voltage));
    }

    return data;
}

void CurveProcessor::resetStates() {
    _lpfState = 0.0f;
    _feedbackState = 0.0f;
}