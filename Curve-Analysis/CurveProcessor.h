#pragma once

#include "Curve.h"
#include "dj_fft.h"
#include <vector>
#include <cmath>
#include <cstdint>

// Replicating Types from the main project
enum class VoltageRange : uint8_t {
    Unipolar1V, Unipolar2V, Unipolar3V, Unipolar4V, Unipolar5V,
    Bipolar1V, Bipolar2V, Bipolar3V, Bipolar4V, Bipolar5V,
    Last
};

struct VoltageRangeInfo {
    float lo;
    float hi;

    float normalize(float value) const {
        return std::max(0.0f, std::min(1.0f, (value - lo) / (hi - lo)));
    }

    float denormalize(float value) const {
        return value * (hi - lo) + lo;
    }
};

const VoltageRangeInfo& voltageRangeInfo(VoltageRange range);
const char* voltageRangeName(VoltageRange range);

enum class SpectrumSource {
    Input,
    PostWavefolder,
    PostFilter,
    PostCompensation,
    FinalOutput,
    Last
};

class CurveProcessor {
public:
    struct AdvancedParameters {
        float foldAmount = 8.0f;
        float hpfCurve = 0.85f;
        float resonanceGain = 1.5f;
        float resonanceTame = 0.8f;
        float feedbackCurve = 1.0f;
        float foldComp = 0.8f;
        float lpfComp = 0.3f;
        float hpfComp = 0.5f;
        float resComp = 0.1f;
        float maxComp = 2.5f;
        float lfoLimiterAmount = 3.0f;
        float lfoLimiterMin = 0.5f;
        float feedbackLimit = 4.0f;
    };

    struct Parameters {
        float globalPhase = 0.0f;
        float wavefolderFold = 0.0f;
        float wavefolderGain = 0.0f;
        float wavefolderSymmetry = 0.0f;
        float djFilter = 0.0f;
        float filterF = 0.0f;
        float foldF = 0.0f;
        float xFade = 1.0f;
        float min = 0.0f;
        float max = 1.0f;
        Curve::Type shape = Curve::Type::Linear;
        VoltageRange range = VoltageRange::Bipolar5V;
        SpectrumSource spectrumSource = SpectrumSource::FinalOutput;
        AdvancedParameters advanced;
        bool shapeVariation = false;
        bool invert = false;

        // Hardware simulation parameters
        int dacResolutionBits = 16;        // PEW|FORMER uses 16-bit DAC (though effective resolution may vary)
        float dacUpdateRate = 1.0f;        // Update interval in milliseconds (PEW|FORMER: 1.0ms for 1000Hz update rate)
        float timingJitter = 0.0f;         // Timing inaccuracy in milliseconds (minimal in real hardware)
    };

    struct SignalData {
        std::vector<float> originalSignal; // 0-1
        std::vector<float> phasedSignal;   // 0-1, this is the original signal with phase offset
        std::vector<float> postWavefolder; // 0-1
        std::vector<float> postFilter;     // Voltage
        std::vector<float> postCompensation; // Voltage
        std::vector<float> finalOutput;      // Voltage
        std::vector<float> hardwareLimitedOutput; // Voltage with hardware constraints applied
        std::vector<float> spectrum;
        std::vector<float> spectrum_oversampled;
    };

    struct PerformanceData {
        float processTimeMs = 0.0f;      // Time taken for the process function in milliseconds
        float timeBudgetMs = 0.0f;       // Available time budget for processing
        float cpuUsagePercent = 0.0f;    // CPU usage as a percentage of available time
        int sampleRate = 48000;          // The sample rate being used
    };

    CurveProcessor(int bufferSize = 1024);
    ~CurveProcessor() = default;

    SignalData process(const Parameters& params, int sampleRate = 48000);
    void resetStates();
    const PerformanceData& getPerformance() const { return m_performance; }

private:
    int _bufferSize;
    float _lpfState;
    float _feedbackState;
    PerformanceData m_performance;
};