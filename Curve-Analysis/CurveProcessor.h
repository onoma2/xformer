#pragma once

#include "Curve.h"
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


class CurveProcessor {
public:
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
        bool shapeVariation = false;
        bool invert = false;
    };

    struct SignalData {
        std::vector<float> originalSignal; // 0-1
        std::vector<float> phasedSignal;   // 0-1, this is the original signal with phase offset
        std::vector<float> postWavefolder; // 0-1
        std::vector<float> postFilter;     // Voltage
        std::vector<float> postCompensation; // Voltage
        std::vector<float> finalOutput;      // Voltage
    };

    CurveProcessor(int bufferSize = 1024);
    ~CurveProcessor() = default;

    SignalData process(const Parameters& params, int sampleRate = 48000);
    void resetStates();

private:
    int _bufferSize;
    float _lpfState;
    float _feedbackState;
};