# Feature Specification: CvRouter4x4 CV Mapping Engine

## Overview
The CvRouter4x4 CV mapping engine provides a 4x4 matrix for CV routing that operates independently of the sequencer's play/stop state and track assignments. This engine continuously processes CV signals at the system's control rate (approximately 1000Hz) and integrates with the existing routing system for parameter control.

## Motivation
Current PEW|FORMER firmware lacks a dedicated CV routing engine that operates independently of the sequencer state. Users need a way to dynamically map CV inputs to CV outputs in real-time, similar to Vostok Instruments' CvRouter module, but integrated into the existing firmware architecture.

## Code example

#include <array>
#include <cmath>
#include <algorithm>

class CvRouter4x4 {
public:
    static constexpr size_t NUM_CHANNELS = 4;

    CvRouter4x4() {
        setInputSlopeCurve(1.5f);
        setOutputSlopeCurve(1.5f);
        setDefaultInputVoltage(1.0f);
    }

    // Process one sample at signal rate (up to 1000Hz)
    void process(float inputScanCV, float outputRouteCV,
                const std::array<float, NUM_CHANNELS>& inputs,
                std::array<float, NUM_CHANNELS>& outputs) {

        // Stage 1: Scan inputs using Trace-style scanning
        float intermediateSignal = scanInputs(inputScanCV, inputs);

        // Stage 2: Route to outputs using Path-style routing
        routeOutputs(intermediateSignal, outputRouteCV, outputs);
    }

    // Process with attenuation and inversion controls
    void processWithControls(float inputScanCV, float outputRouteCV,
                           float inputAttenuation, float outputAttenuation,
                           bool inputInvert, bool outputInvert,
                           const std::array<float, NUM_CHANNELS>& inputs,
                           std::array<float, NUM_CHANNELS>& outputs) {

        // Apply attenuation and inversion to CV signals
        float processedInputCV = inputScanCV * inputAttenuation * (inputInvert ? -1.0f : 1.0f);
        float processedOutputCV = outputRouteCV * outputAttenuation * (outputInvert ? -1.0f : 1.0f);

        process(processedInputCV, processedOutputCV, inputs, outputs);
    }

    // Set custom slope curve for smoother transitions
    void setInputSlopeCurve(float curve) { m_inputSlopeCurve = std::max(0.1f, curve); }
    void setOutputSlopeCurve(float curve) { m_outputSlopeCurve = std::max(0.1f, curve); }

    // Set default voltage when no input is connected
    void setDefaultInputVoltage(float voltage) { m_defaultInputVoltage = voltage; }

    // Handle missing inputs (nullptr means no connection)
    void processWithMissingInputs(float inputScanCV, float outputRouteCV,
                                 const std::array<float*, NUM_CHANNELS>& inputPointers,
                                 std::array<float, NUM_CHANNELS>& outputs) {

        std::array<float, NUM_CHANNELS> actualInputs;

        // Fill in default voltages for missing inputs
        for (size_t i = 0; i < NUM_CHANNELS; ++i) {
            if (inputPointers[i] != nullptr) {
                actualInputs[i] = *inputPointers[i];
            } else {
                actualInputs[i] = m_defaultInputVoltage;
            }
        }

        process(inputScanCV, outputRouteCV, actualInputs, outputs);
    }

private:
    float m_inputSlopeCurve;
    float m_outputSlopeCurve;
    float m_defaultInputVoltage;

    // Trace-style input scanning
    float scanInputs(float cv, const std::array<float, NUM_CHANNELS>& inputs) {
        // Normalize CV to 0-1 range (assuming 0-5V typical Eurorack range)
        float normalizedCV = std::clamp(cv / 5.0f, 0.0f, 1.0f);

        // Calculate position across 4 channels
        float position = normalizedCV * (NUM_CHANNELS - 1);

        // Find surrounding channels
        int lowerIdx = static_cast<int>(std::floor(position));
        int upperIdx = static_cast<int>(std::ceil(position));

        // Calculate interpolation factor
        float t = position - lowerIdx;

        // Apply custom slope curve for smooth transitions
        float tSmooth = applySlopeCurve(t, m_inputSlopeCurve);

        // Clamp indices to valid range
        lowerIdx = std::clamp(lowerIdx, 0, static_cast<int>(NUM_CHANNELS) - 1);
        upperIdx = std::clamp(upperIdx, 0, static_cast<int>(NUM_CHANNELS) - 1);

        // Linear interpolation between channels
        if (lowerIdx == upperIdx) {
            return inputs[lowerIdx];
        } else {
            return inputs[lowerIdx] * (1.0f - tSmooth) + inputs[upperIdx] * tSmooth;
        }
    }

    // Path-style output routing
    void routeOutputs(float signal, float cv, std::array<float, NUM_CHANNELS>& outputs) {
        // Normalize CV to 0-1 range
        float normalizedCV = std::clamp(cv / 5.0f, 0.0f, 1.0f);

        // Calculate position across 4 outputs
        float position = normalizedCV * (NUM_CHANNELS - 1);

        // Calculate weights for all outputs
        std::array<float, NUM_CHANNELS> weights = calculateChannelWeights(position, m_outputSlopeCurve);

        // Apply routing to each output
        for (size_t i = 0; i < NUM_CHANNELS; ++i) {
            outputs[i] = signal * weights[i];
        }
    }

    // Calculate smooth channel weights with custom slope
    std::array<float, NUM_CHANNELS> calculateChannelWeights(float position, float slopeCurve) {
        std::array<float, NUM_CHANNELS> weights = {0.0f};

        int lowerIdx = static_cast<int>(std::floor(position));
        int upperIdx = static_cast<int>(std::ceil(position));
        float t = position - lowerIdx;

        float tSmooth = applySlopeCurve(t, slopeCurve);

        lowerIdx = std::clamp(lowerIdx, 0, static_cast<int>(NUM_CHANNELS) - 1);
        upperIdx = std::clamp(upperIdx, 0, static_cast<int>(NUM_CHANNELS) - 1);

        if (lowerIdx == upperIdx) {
            weights[lowerIdx] = 1.0f;
        } else {
            weights[lowerIdx] = 1.0f - tSmooth;
            weights[upperIdx] = tSmooth;
        }

        // Ensure weights sum to 1.0
        float sum = 0.0f;
        for (float w : weights) sum += w;
        if (sum > 0.0f) {
            for (float& w : weights) w /= sum;
        }

        return weights;
    }

    // Custom slope curve for smooth transitions at middle positions
    float applySlopeCurve(float t, float curveStrength) {
        // Custom curve that provides smoother transitions in the middle
        // This creates the characteristic Vostok Instruments smooth slope response
        float numerator = std::pow(t, curveStrength);
        float denominator = numerator + std::pow(1.0f - t, curveStrength);
        return numerator / denominator;
    }
};

// Usage example for 1000Hz signal rate processing
/*
#include <iostream>
#include <array>
#include <vector>

int main() {
    CvRouter4x4 matrix;

    const float sampleRate = 1000.0f; // 1000 updates/second
    const float duration = 1.0f;      // 1 second
    const size_t numSamples = static_cast<size_t>(sampleRate * duration);

    // Input signals (could be CV or audio-rate signals downsampled to 1000Hz)
    std::array<std::vector<float>, 4> inputSignals;
    for (auto& signal : inputSignals) {
        signal.resize(numSamples);
    }

    // Generate example input signals
    for (size_t i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;

        // Different waveforms for each input
        inputSignals[0][i] = std::sin(2.0f * M_PI * 0.5f * t);      // 0.5Hz sine
        inputSignals[1][i] = (std::fmod(t * 0.25f, 1.0f) < 0.5f) ? 1.0f : -1.0f; // 0.25Hz square
        inputSignals[2][i] = std::fmod(t * 1.0f, 1.0f) * 2.0f - 1.0f; // 1Hz saw
        inputSignals[3][i] = (std::rand() % 2000 - 1000) / 1000.0f;  // Random noise
    }

    // CV control signals
    std::vector<float> inputScanCV(numSamples);
    std::vector<float> outputRouteCV(numSamples);

    // Generate CV signals (LFOs)
    for (size_t i = 0; i < numSamples; ++i) {
        float t = static_cast<float>(i) / sampleRate;

        // Input scanning CV: 0.1Hz LFO
        inputScanCV[i] = (std::sin(2.0f * M_PI * 0.1f * t) + 1.0f) * 2.5f; // 0-5V range

        // Output routing CV: 0.05Hz LFO with phase shift
        outputRouteCV[i] = (std::sin(2.0f * M_PI * 0.05f * t + M_PI_2) + 1.0f) * 2.5f;
    }

    // Process all samples
    std::array<std::vector<float>, 4> outputSignals;
    for (auto& signal : outputSignals) {
        signal.resize(numSamples);
    }

    for (size_t i = 0; i < numSamples; ++i) {
        std::array<float, 4> currentInputs = {
            inputSignals[0][i],
            inputSignals[1][i],
            inputSignals[2][i],
            inputSignals[3][i]
        };

        std::array<float, 4> currentOutputs;

        // Process with CV controls
        matrix.processWithControls(
            inputScanCV[i],           // Input scan CV
            outputRouteCV[i],         // Output route CV
            0.8f,                    // Input attenuation
            1.0f,                    // Output attenuation
            false,                   // Input invert
            true,                    // Output invert (for interesting effect)
            currentInputs,
            currentOutputs
        );

        // Store outputs
        for (size_t ch = 0; ch < 4; ++ch) {
            outputSignals[ch][i] = currentOutputs[ch];
        }
    }

    std::cout << "Processing complete - 4x4 matrix processed at 1000Hz" << std::endl;
    return 0;
}
*/

## Technical Requirements

### 1. Independent Operation
- Engine must operate regardless of sequencer play/stop state
- Must work without track assignments
- Continuous processing at system control rate (approx. 1000Hz)

### 2. CV Matrix Capabilities
- 4x4 matrix routing (4 inputs to 4 outputs)
- Configurable routing weights for each input-output pair
- Support for different routing modes (linear, exponential, bipolar, unipolar)

### 3. Parameter Control
- All parameters controllable via existing routing system
- Real-time parameter updates
- Preset saving/loading capability

### 4. Resource Management
- CPU throttling to prevent overload
- Memory-efficient implementation
- Adjustable processing rate

## Implementation Plan

### Phase 1: Core Engine Implementation

#### 1.1 CvRouter4x4Engine Class Definition

```cpp
#pragma once

#include "Config.h"
#include "Engine.h"
#include "CvInput.h"
#include "CvOutput.h"
#include "Routing.h"

#include <array>
#include <cmath>

class CvRouter4x4Engine {
public:
    static constexpr size_t NumChannels = 4;

    enum class ProcessingMode {
        Linear,
        Exponential,
        Bipolar,
        Unipolar,
        Last
    };

    enum class FilterType {
        None,
        LowPass,
        HighPass,
        BandPass,
        Last
    };

    CvRouter4x4Engine(Engine &engine);

    void init();
    void update(); // Called at control rate (approx. 1000Hz)

    // Matrix control
    void setRouteWeight(size_t input, size_t output, float weight);
    float routeWeight(size_t input, size_t output) const;
    void setAllRoutes(float weight);
    void clearMatrix();
    void setUnityMatrix();

    // Global parameters
    void setEnabled(bool enabled) { _enabled = enabled; }
    bool isEnabled() const { return _enabled; }

    void setMix(float mix) { _mix = clamp(mix, 0.0f, 1.0f); }
    float mix() const { return _mix; }

    void setProcessingMode(ProcessingMode mode) { _processingMode = mode; }
    ProcessingMode processingMode() const { return _processingMode; }

    void setFilterType(FilterType type) { _filterType = type; }
    FilterType filterType() const { return _filterType; }

    void setFilterCutoff(float cutoff) { _filterCutoff = clamp(cutoff, 0.0f, 1.0f); }
    float filterCutoff() const { return _filterCutoff; }

    // Performance controls
    void setCpuThrottle(float throttle) { _cpuThrottle = clamp(throttle, 0.0f, 1.0f); }
    float cpuThrottle() const { return _cpuThrottle; }

private:
    Engine &_engine;

    // Matrix weights: _matrix[input][output]
    std::array<std::array<float, NumChannels>, NumChannels> _matrix;

    // Current input and output values
    std::array<float, NumChannels> _inputs;
    std::array<float, NumChannels> _outputs;

    // Filter states (one per output channel)
    std::array<float, NumChannels> _filterState;

    // Global parameters
    bool _enabled = true;
    float _mix = 1.0f;  // 0 = original, 1 = processed
    ProcessingMode _processingMode = ProcessingMode::Linear;
    FilterType _filterType = FilterType::None;
    float _filterCutoff = 0.5f;

    // Performance controls
    float _cpuThrottle = 1.0f;  // 0 = no processing, 1 = full processing

    // Internal processing helpers
    float processSample(float input, size_t channel);
    float applyFilter(float input, size_t channel);
    float applyProcessingMode(float input);
};
```

#### 1.2 CvRouter4x4Engine Implementation

```cpp
#include "CvRouter4x4Engine.h"
#include "core/math/Math.h"

CvRouter4x4Engine::CvRouter4x4Engine(Engine &engine) :
    _engine(engine)
{
    // Initialize matrix to unity (diagonal)
    setUnityMatrix();

    // Initialize filter states
    _filterState.fill(0.0f);

    // Initialize inputs and outputs
    _inputs.fill(0.0f);
    _outputs.fill(0.0f);
}

void CvRouter4x4Engine::init() {
    // Initialize with default values
    setUnityMatrix();
    _filterState.fill(0.0f);
    _inputs.fill(0.0f);
    _outputs.fill(0.0f);
}

void CvRouter4x4Engine::update() {
    if (!_enabled) return;

    // Skip processing based on CPU throttle
    if (random() > (_cpuThrottle * UINT32_MAX)) return;

    // Read current CV inputs
    for (size_t i = 0; i < NumChannels; ++i) {
        _inputs[i] = _engine.cvInput().channel(i);
    }

    // Process matrix routing
    for (size_t outputIdx = 0; outputIdx < NumChannels; ++outputIdx) {
        float mixedOutput = 0.0f;

        // Sum all inputs weighted by matrix values
        for (size_t inputIdx = 0; inputIdx < NumChannels; ++inputIdx) {
            float processedInput = applyProcessingMode(_inputs[inputIdx]);
            mixedOutput += processedInput * _matrix[inputIdx][outputIdx];
        }

        // Apply mix between original and processed
        float originalOutput = _inputs[outputIdx]; // Pass-through as fallback
        _outputs[outputIdx] = originalOutput * (1.0f - _mix) + mixedOutput * _mix;

        // Apply filtering
        _outputs[outputIdx] = applyFilter(_outputs[outputIdx], outputIdx);

        // Set CV output
        _engine.setCvOutput(outputIdx, _outputs[outputIdx]);
    }
}

void CvRouter4x4Engine::setRouteWeight(size_t input, size_t output, float weight) {
    if (input < NumChannels && output < NumChannels) {
        _matrix[input][output] = clamp(weight, -1.0f, 1.0f);
    }
}

float CvRouter4x4Engine::routeWeight(size_t input, size_t output) const {
    if (input < NumChannels && output < NumChannels) {
        return _matrix[input][output];
    }
    return 0.0f;
}

void CvRouter4x4Engine::setAllRoutes(float weight) {
    for (size_t i = 0; i < NumChannels; ++i) {
        for (size_t j = 0; j < NumChannels; ++j) {
            _matrix[i][j] = clamp(weight, -1.0f, 1.0f);
        }
    }
}

void CvRouter4x4Engine::clearMatrix() {
    for (size_t i = 0; i < NumChannels; ++i) {
        for (size_t j = 0; j < NumChannels; ++j) {
            _matrix[i][j] = 0.0f;
        }
    }
}

void CvRouter4x4Engine::setUnityMatrix() {
    for (size_t i = 0; i < NumChannels; ++i) {
        for (size_t j = 0; j < NumChannels; ++j) {
            _matrix[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

float CvRouter4x4Engine::processSample(float input, size_t channel) {
    // Apply processing mode to input
    float processed = applyProcessingMode(input);

    // Apply filter
    processed = applyFilter(processed, channel);

    return processed;
}

float CvRouter4x4Engine::applyProcessingMode(float input) {
    switch (_processingMode) {
        case ProcessingMode::Linear:
            return input;
        case ProcessingMode::Exponential:
            // Exponential scaling around 0
            return input >= 0.0f ? input * input : -(input * input);
        case ProcessingMode::Bipolar:
            // Ensure bipolar processing
            return clamp(input, -1.0f, 1.0f);
        case ProcessingMode::Unipolar:
            // Convert to unipolar (0 to 1 range)
            return (input + 1.0f) * 0.5f;
        default:
            return input;
    }
}

float CvRouter4x4Engine::applyFilter(float input, size_t channel) {
    if (_filterType == FilterType::None) return input;

    // Simple one-pole filter implementation
    float cutoff = _filterCutoff;
    float alpha = cutoff; // Simplified alpha calculation

    switch (_filterType) {
        case FilterType::LowPass:
            _filterState[channel] = _filterState[channel] + alpha * (input - _filterState[channel]);
            return _filterState[channel];
        case FilterType::HighPass:
            _filterState[channel] = _filterState[channel] + alpha * (input - _filterState[channel]);
            return input - _filterState[channel];
        case FilterType::BandPass:
            // Simplified bandpass: lowpass result minus highpass result
            float lp_result = _filterState[channel] + alpha * (input - _filterState[channel]);
            float hp_result = input - lp_result;
            _filterState[channel] = lp_result;
            return lp_result - hp_result;
        default:
            return input;
    }
}
```

### Phase 2: Integration with Engine

#### 2.1 Modify Engine.h

```cpp
// Add to Engine.h includes
#include "CvRouter4x4Engine.h"

// Add to Engine class members
private:
    CvRouter4x4Engine _cvRouter4x4Engine;

// Add to Engine public interface
public:
    const CvRouter4x4Engine &cvRouter4x4Engine() const { return _cvRouter4x4Engine; }
          CvRouter4x4Engine &cvRouter4x4Engine()       { return _cvRouter4x4Engine; }
```

#### 2.2 Modify Engine.cpp

```cpp
// In Engine constructor, add:
, _cvRouter4x4Engine(*this)

// In Engine::init(), add:
_cvRouter4x4Engine.init();

// In Engine::update(), add:
// Update CvRouter4x4 engine (runs independently of sequencer state)
_cvRouter4x4Engine.update();
```

### Phase 3: Routing System Integration

#### 3.1 Extend Routing.h with new targets

```cpp
// Add to Routing::Target enum:
enum class Target : uint8_t {
    // ... existing targets ...

    // CvRouter4x4 Targets
    CvRouterFirst,
    CvRouterEnable = CvRouterFirst,
    CvRouterMix,
    CvRouterProcessingMode,
    CvRouterFilterType,
    CvRouterFilterCutoff,
    CvRouterRoute00,  // Input 0 to Output 0
    CvRouterRoute01,  // Input 0 to Output 1
    CvRouterRoute02,
    CvRouterRoute03,
    CvRouterRoute10,  // Input 1 to Output 0
    CvRouterRoute11,
    CvRouterRoute12,
    CvRouterRoute13,
    CvRouterRoute20,  // Input 2 to Output 0
    CvRouterRoute21,
    CvRouterRoute22,
    CvRouterRoute23,
    CvRouterRoute30,  // Input 3 to Output 0
    CvRouterRoute31,
    CvRouterRoute32,
    CvRouterRoute33,
    CvRouterLast = CvRouterRoute33,

    Last,
};

// Add helper functions
static bool isCvRouterTarget(Target target) {
    return target >= Target::CvRouterFirst && target <= Target::CvRouterLast;
}

// Add target name function
static const char *targetName(Target target) {
    switch (target) {
        // ... existing cases ...

        case Target::CvRouterEnable:        return "TP Enable";
        case Target::CvRouterMix:           return "TP Mix";
        case Target::CvRouterProcessingMode:return "TP Proc Mode";
        case Target::CvRouterFilterType:    return "TP Filter";
        case Target::CvRouterFilterCutoff:  return "TP FCutoff";
        case Target::CvRouterRoute00:       return "TP 0->0";
        case Target::CvRouterRoute01:       return "TP 0->1";
        case Target::CvRouterRoute02:       return "TP 0->2";
        case Target::CvRouterRoute03:       return "TP 0->3";
        case Target::CvRouterRoute10:       return "TP 1->0";
        case Target::CvRouterRoute11:       return "TP 1->1";
        case Target::CvRouterRoute12:       return "TP 1->2";
        case Target::CvRouterRoute13:       return "TP 1->3";
        case Target::CvRouterRoute20:       return "TP 2->0";
        case Target::CvRouterRoute21:       return "TP 2->1";
        case Target::CvRouterRoute22:       return "TP 2->2";
        case Target::CvRouterRoute23:       return "TP 2->3";
        case Target::CvRouterRoute30:       return "TP 3->0";
        case Target::CvRouterRoute31:       return "TP 3->1";
        case Target::CvRouterRoute32:       return "TP 3->2";
        case Target::CvRouterRoute33:       return "TP 3->3";

        case Target::Last:                   break;
    }
    return nullptr;
}
```

#### 3.2 Update RoutingEngine.cpp to handle new targets

```cpp
// In RoutingEngine::writeEngineTarget, add case for CvRouter targets:
void RoutingEngine::writeEngineTarget(Routing::Target target, float normalized) {
    // ... existing cases ...

    if (Routing::isCvRouterTarget(target)) {
        switch (target) {
            case Routing::Target::CvRouterEnable:
                _engine.cvRouter4x4Engine().setEnabled(normalized > 0.5f);
                break;
            case Routing::Target::CvRouterMix:
                _engine.cvRouter4x4Engine().setMix(normalized);
                break;
            case Routing::Target::CvRouterProcessingMode:
                _engine.cvRouter4x4Engine().setProcessingMode(
                    static_cast<CvRouter4x4Engine::ProcessingMode>(
                        static_cast<int>(normalized *
                            static_cast<int>(CvRouter4x4Engine::ProcessingMode::Last))
                    )
                );
                break;
            case Routing::Target::CvRouterFilterType:
                _engine.cvRouter4x4Engine().setFilterType(
                    static_cast<CvRouter4x4Engine::FilterType>(
                        static_cast<int>(normalized *
                            static_cast<int>(CvRouter4x4Engine::FilterType::Last))
                    )
                );
                break;
            case Routing::Target::CvRouterFilterCutoff:
                _engine.cvRouter4x4Engine().setFilterCutoff(normalized);
                break;
            case Routing::Target::CvRouterRoute00:
                _engine.cvRouter4x4Engine().setRouteWeight(0, 0, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute01:
                _engine.cvRouter4x4Engine().setRouteWeight(0, 1, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute02:
                _engine.cvRouter4x4Engine().setRouteWeight(0, 2, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute03:
                _engine.cvRouter4x4Engine().setRouteWeight(0, 3, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute10:
                _engine.cvRouter4x4Engine().setRouteWeight(1, 0, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute11:
                _engine.cvRouter4x4Engine().setRouteWeight(1, 1, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute12:
                _engine.cvRouter4x4Engine().setRouteWeight(1, 2, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute13:
                _engine.cvRouter4x4Engine().setRouteWeight(1, 3, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute20:
                _engine.cvRouter4x4Engine().setRouteWeight(2, 0, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute21:
                _engine.cvRouter4x4Engine().setRouteWeight(2, 1, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute22:
                _engine.cvRouter4x4Engine().setRouteWeight(2, 2, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute23:
                _engine.cvRouter4x4Engine().setRouteWeight(2, 3, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute30:
                _engine.cvRouter4x4Engine().setRouteWeight(3, 0, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute31:
                _engine.cvRouter4x4Engine().setRouteWeight(3, 1, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute32:
                _engine.cvRouter4x4Engine().setRouteWeight(3, 2, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::CvRouterRoute33:
                _engine.cvRouter4x4Engine().setRouteWeight(3, 3, normalized * 2.0f - 1.0f);
                break;
            default:
                break;
        }
    }
}
```

### Phase 4: Resource Management and Optimization

#### 4.1 Add performance monitoring

```cpp
// In CvRouter4x4Engine.h, add:
struct Stats {
    uint32_t processCalls;
    uint32_t skipCalls;  // Due to CPU throttling
    uint32_t lastProcessTimeUs;
};

Stats stats() const { return _stats; }

// In CvRouter4x4Engine.cpp, add:
private:
    Stats _stats = {};

// In update():
void CvRouter4x4Engine::update() {
    if (!_enabled) return;

    // Skip processing based on CPU throttle
    if (random() > (_cpuThrottle * UINT32_MAX)) {
        _stats.skipCalls++;
        return;
    }

    _stats.processCalls++;

    // ... rest of processing ...
}
```

## Testing Strategy

### Unit Tests
- Test matrix operations (set, get, clear, unity)
- Test processing modes
- Test filter implementations
- Test parameter boundaries and clamping

### Integration Tests
- Test integration with CV input/output systems
- Test routing system integration
- Test performance under different CPU loads

### Performance Tests
- Measure CPU usage at different processing rates
- Verify real-time performance at 1000Hz
- Test stability under various load conditions

## Potential Challenges and Solutions

### Challenge 1: CPU Load Management
**Solution**: Implement CPU throttling mechanism and provide adjustable processing rate options.

### Challenge 2: Audio Rate Processing
**Solution**: Optimize algorithms for efficiency and provide loop unrolling where beneficial.

### Challenge 3: Integration with Existing Systems
**Solution**: Carefully design the API to integrate seamlessly with existing CV input/output and routing systems.

## Expected Resource Usage
- RAM: ~200 bytes for matrix storage and state
- Flash: ~2-3KB for implementation
- CPU: Variable based on CPU throttle setting (0-10% at full throttle)
- Processing rate: Fixed at system control rate (approx. 1000Hz)

## Future Enhancements
- Preset management for different routing configurations
- Advanced filter types (biquad, FIR)
- Additional processing modes
- MIDI learn for CvRouter parameters
- Visual feedback in UI showing current routing matrix
