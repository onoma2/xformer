# Feature Specification: TracePath4x4 CV Mapping Engine

## Overview
The TracePath4x4 CV mapping engine provides a 4x4 matrix for CV routing that operates independently of the sequencer's play/stop state and track assignments. This engine continuously processes CV signals at the system's control rate (approximately 1000Hz) and integrates with the existing routing system for parameter control.

## Motivation
Current PEW|FORMER firmware lacks a dedicated CV routing engine that operates independently of the sequencer state. Users need a way to dynamically map CV inputs to CV outputs in real-time, similar to Vostok Instruments' TracePath module, but integrated into the existing firmware architecture.

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

#### 1.1 TracePath4x4Engine Class Definition

```cpp
#pragma once

#include "Config.h"
#include "Engine.h"
#include "CvInput.h"
#include "CvOutput.h"
#include "Routing.h"

#include <array>
#include <cmath>

class TracePath4x4Engine {
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

    TracePath4x4Engine(Engine &engine);

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

#### 1.2 TracePath4x4Engine Implementation

```cpp
#include "TracePath4x4Engine.h"
#include "core/math/Math.h"

TracePath4x4Engine::TracePath4x4Engine(Engine &engine) :
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

void TracePath4x4Engine::init() {
    // Initialize with default values
    setUnityMatrix();
    _filterState.fill(0.0f);
    _inputs.fill(0.0f);
    _outputs.fill(0.0f);
}

void TracePath4x4Engine::update() {
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

void TracePath4x4Engine::setRouteWeight(size_t input, size_t output, float weight) {
    if (input < NumChannels && output < NumChannels) {
        _matrix[input][output] = clamp(weight, -1.0f, 1.0f);
    }
}

float TracePath4x4Engine::routeWeight(size_t input, size_t output) const {
    if (input < NumChannels && output < NumChannels) {
        return _matrix[input][output];
    }
    return 0.0f;
}

void TracePath4x4Engine::setAllRoutes(float weight) {
    for (size_t i = 0; i < NumChannels; ++i) {
        for (size_t j = 0; j < NumChannels; ++j) {
            _matrix[i][j] = clamp(weight, -1.0f, 1.0f);
        }
    }
}

void TracePath4x4Engine::clearMatrix() {
    for (size_t i = 0; i < NumChannels; ++i) {
        for (size_t j = 0; j < NumChannels; ++j) {
            _matrix[i][j] = 0.0f;
        }
    }
}

void TracePath4x4Engine::setUnityMatrix() {
    for (size_t i = 0; i < NumChannels; ++i) {
        for (size_t j = 0; j < NumChannels; ++j) {
            _matrix[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
}

float TracePath4x4Engine::processSample(float input, size_t channel) {
    // Apply processing mode to input
    float processed = applyProcessingMode(input);
    
    // Apply filter
    processed = applyFilter(processed, channel);
    
    return processed;
}

float TracePath4x4Engine::applyProcessingMode(float input) {
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

float TracePath4x4Engine::applyFilter(float input, size_t channel) {
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
#include "TracePath4x4Engine.h"

// Add to Engine class members
private:
    TracePath4x4Engine _tracePath4x4Engine;

// Add to Engine public interface
public:
    const TracePath4x4Engine &tracePath4x4Engine() const { return _tracePath4x4Engine; }
          TracePath4x4Engine &tracePath4x4Engine()       { return _tracePath4x4Engine; }
```

#### 2.2 Modify Engine.cpp

```cpp
// In Engine constructor, add:
, _tracePath4x4Engine(*this)

// In Engine::init(), add:
_tracePath4x4Engine.init();

// In Engine::update(), add:
// Update TracePath4x4 engine (runs independently of sequencer state)
_tracePath4x4Engine.update();
```

### Phase 3: Routing System Integration

#### 3.1 Extend Routing.h with new targets

```cpp
// Add to Routing::Target enum:
enum class Target : uint8_t {
    // ... existing targets ...
    
    // TracePath4x4 Targets
    TracePathFirst,
    TracePathEnable = TracePathFirst,
    TracePathMix,
    TracePathProcessingMode,
    TracePathFilterType,
    TracePathFilterCutoff,
    TracePathRoute00,  // Input 0 to Output 0
    TracePathRoute01,  // Input 0 to Output 1
    TracePathRoute02,
    TracePathRoute03,
    TracePathRoute10,  // Input 1 to Output 0
    TracePathRoute11,
    TracePathRoute12,
    TracePathRoute13,
    TracePathRoute20,  // Input 2 to Output 0
    TracePathRoute21,
    TracePathRoute22,
    TracePathRoute23,
    TracePathRoute30,  // Input 3 to Output 0
    TracePathRoute31,
    TracePathRoute32,
    TracePathRoute33,
    TracePathLast = TracePathRoute33,
    
    Last,
};

// Add helper functions
static bool isTracePathTarget(Target target) {
    return target >= Target::TracePathFirst && target <= Target::TracePathLast;
}

// Add target name function
static const char *targetName(Target target) {
    switch (target) {
        // ... existing cases ...
        
        case Target::TracePathEnable:        return "TP Enable";
        case Target::TracePathMix:           return "TP Mix";
        case Target::TracePathProcessingMode:return "TP Proc Mode";
        case Target::TracePathFilterType:    return "TP Filter";
        case Target::TracePathFilterCutoff:  return "TP FCutoff";
        case Target::TracePathRoute00:       return "TP 0->0";
        case Target::TracePathRoute01:       return "TP 0->1";
        case Target::TracePathRoute02:       return "TP 0->2";
        case Target::TracePathRoute03:       return "TP 0->3";
        case Target::TracePathRoute10:       return "TP 1->0";
        case Target::TracePathRoute11:       return "TP 1->1";
        case Target::TracePathRoute12:       return "TP 1->2";
        case Target::TracePathRoute13:       return "TP 1->3";
        case Target::TracePathRoute20:       return "TP 2->0";
        case Target::TracePathRoute21:       return "TP 2->1";
        case Target::TracePathRoute22:       return "TP 2->2";
        case Target::TracePathRoute23:       return "TP 2->3";
        case Target::TracePathRoute30:       return "TP 3->0";
        case Target::TracePathRoute31:       return "TP 3->1";
        case Target::TracePathRoute32:       return "TP 3->2";
        case Target::TracePathRoute33:       return "TP 3->3";
        
        case Target::Last:                   break;
    }
    return nullptr;
}
```

#### 3.2 Update RoutingEngine.cpp to handle new targets

```cpp
// In RoutingEngine::writeEngineTarget, add case for TracePath targets:
void RoutingEngine::writeEngineTarget(Routing::Target target, float normalized) {
    // ... existing cases ...
    
    if (Routing::isTracePathTarget(target)) {
        switch (target) {
            case Routing::Target::TracePathEnable:
                _engine.tracePath4x4Engine().setEnabled(normalized > 0.5f);
                break;
            case Routing::Target::TracePathMix:
                _engine.tracePath4x4Engine().setMix(normalized);
                break;
            case Routing::Target::TracePathProcessingMode:
                _engine.tracePath4x4Engine().setProcessingMode(
                    static_cast<TracePath4x4Engine::ProcessingMode>(
                        static_cast<int>(normalized * 
                            static_cast<int>(TracePath4x4Engine::ProcessingMode::Last))
                    )
                );
                break;
            case Routing::Target::TracePathFilterType:
                _engine.tracePath4x4Engine().setFilterType(
                    static_cast<TracePath4x4Engine::FilterType>(
                        static_cast<int>(normalized * 
                            static_cast<int>(TracePath4x4Engine::FilterType::Last))
                    )
                );
                break;
            case Routing::Target::TracePathFilterCutoff:
                _engine.tracePath4x4Engine().setFilterCutoff(normalized);
                break;
            case Routing::Target::TracePathRoute00:
                _engine.tracePath4x4Engine().setRouteWeight(0, 0, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute01:
                _engine.tracePath4x4Engine().setRouteWeight(0, 1, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute02:
                _engine.tracePath4x4Engine().setRouteWeight(0, 2, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute03:
                _engine.tracePath4x4Engine().setRouteWeight(0, 3, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute10:
                _engine.tracePath4x4Engine().setRouteWeight(1, 0, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute11:
                _engine.tracePath4x4Engine().setRouteWeight(1, 1, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute12:
                _engine.tracePath4x4Engine().setRouteWeight(1, 2, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute13:
                _engine.tracePath4x4Engine().setRouteWeight(1, 3, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute20:
                _engine.tracePath4x4Engine().setRouteWeight(2, 0, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute21:
                _engine.tracePath4x4Engine().setRouteWeight(2, 1, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute22:
                _engine.tracePath4x4Engine().setRouteWeight(2, 2, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute23:
                _engine.tracePath4x4Engine().setRouteWeight(2, 3, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute30:
                _engine.tracePath4x4Engine().setRouteWeight(3, 0, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute31:
                _engine.tracePath4x4Engine().setRouteWeight(3, 1, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute32:
                _engine.tracePath4x4Engine().setRouteWeight(3, 2, normalized * 2.0f - 1.0f);
                break;
            case Routing::Target::TracePathRoute33:
                _engine.tracePath4x4Engine().setRouteWeight(3, 3, normalized * 2.0f - 1.0f);
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
// In TracePath4x4Engine.h, add:
struct Stats {
    uint32_t processCalls;
    uint32_t skipCalls;  // Due to CPU throttling
    uint32_t lastProcessTimeUs;
};

Stats stats() const { return _stats; }

// In TracePath4x4Engine.cpp, add:
private:
    Stats _stats = {};

// In update():
void TracePath4x4Engine::update() {
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
- MIDI learn for TracePath parameters
- Visual feedback in UI showing current routing matrix