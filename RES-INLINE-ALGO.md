# Research: Inline Algorithm Refactoring for Tuesday Track

## Overview

This document analyzes the feasibility and benefits of refactoring the Tuesday track algorithms using an inline include approach rather than a full polymorphic pattern. The analysis focuses on STM32 real-time constraints and performance considerations.

## Current Implementation

The `TuesdayTrackEngine.cpp` file currently contains:
- Approximately 2,200+ lines of code
- 13 different algorithm implementations (0-12)
- Algorithm-specific logic in 3 main methods:
  - `initAlgorithm()` - Initializes algorithm state
  - `generateBuffer()` - Pre-generates pattern buffer with warmup
  - `tick()` - Main processing with live algorithm execution

## Approach Comparison

### 1. Full Polymorphic Refactoring

**Concept:**
- Create base Algorithm interface
- Implement each algorithm as separate class
- Use factory pattern to instantiate correct algorithm

**Pros:**
- Clean separation of concerns
- Easier unit testing of individual algorithms
- Extensible architecture

**Cons:**
- Virtual function call overhead (1-3 cycles in tight loop)
- Additional memory overhead for vtable pointers
- Potential cache misses and timing jitter
- Complex state management across instances
- Violates real-time system principles

### 2. Inline Include Approach (Recommended)

**Concept:**
- Keep current switch-based structure
- Organize algorithms into separate `.inl` or `.inc` files
- Include these files into TuesdayTrackEngine.cpp

**Implementation Structure:**
```
src/apps/sequencer/engine/
├── TuesdayTrackEngine.h/cpp
└── algorithms/
    ├── test_algorithm.inl
    ├── tritrance_algorithm.inl
    ├── stomper_algorithm.inl
    ├── markov_algorithm.inl
    ├── chiparp_algorithm.inl
    ├── goaacid_algorithm.inl
    ├── snh_algorithm.inl
    ├── wobble_algorithm.inl
    ├── techno_algorithm.inl
    ├── funk_algorithm.inl
    ├── drone_algorithm.inl
    ├── phase_algorithm.inl
    └── raga_algorithm.inl
```

**Pros:**
- Zero performance overhead in critical path
- Optimal memory usage
- Predictable real-time behavior
- Maintains current working architecture
- Better compiler optimization potential
- Easier debugging in embedded environment

**Cons:**
- Code still centralized in one class
- Harder to unit test individual algorithms
- Mixed algorithm-specific state variables

## Performance Analysis

### Critical Path Impact

**Virtual Function Calls:**
- 1-3 cycles per call in `tick()` loop
- At high tempos (e.g., 120 BPM, 48 PPQN), this could matter significantly
- Potential for timing jitter in real-time audio processing

**Inline Includes:**
- Zero function call overhead
- Compiler can optimize as if all code was in one file
- Deterministic timing behavior

### Memory Usage

**Virtual Approach:**
- Additional vtable pointers (~4 bytes per instance)
- Potential for dynamic allocation overhead
- More complex memory management

**Inline Approach:**
- No additional memory overhead
- All state variables remain in main class
- Optimal for STM32 memory constraints

## STM32 Real-time Constraints

### Critical Requirements for Eurorack Modules

1. **Deterministic Timing**: Audio-rate processing requires predictable timing
2. **Low Latency**: <2ms response for real-time performance
3. **Memory Constraints**: Limited RAM (192KB on STM32F405)
4. **Power Efficiency**: Efficient processing to minimize power draw

### Suitability Analysis

| Requirement | Virtual Approach | Inline Approach |
|-------------|------------------|-----------------|
| Deterministic Timing | ❌ Potential jitter | ✅ Predictable |
| Memory Efficiency | ❌ Overhead | ✅ Optimal usage |
| Cache Performance | ❌ Potential misses | ✅ Better locality |
| Real-time Safety | ❌ Dynamic elements | ✅ Static compilation |

## Implementation Strategy

### Recommended Approach: Inline Includes

1. **File Organization:**
   - Create `algorithms/` subdirectory
   - Move each algorithm's code to separate `.inl` files
   - Include them in TuesdayTrackEngine.cpp

2. **Maintain Current Structure:**
   - Keep switch statement in `tick()` method
   - Preserve existing state variable structure
   - Maintain current initialization and buffer generation

3. **Code Organization Benefits:**
   - Cleaner separation of algorithm implementations
   - Easier to locate specific algorithm code
   - Maintainable while preserving performance

### Example Structure:

```cpp
// In TuesdayTrackEngine.cpp
#include "algorithms/test_algorithm.inl"
#include "algorithms/tritrance_algorithm.inl"
#include "algorithms/stomper_algorithm.inl"
// ... etc

// The main switch statement remains unchanged
// but each algorithm case is defined in its own include file
```

## Conclusion

The inline include approach is strongly recommended for the Tuesday track algorithm organization. It provides the code organization benefits needed for maintainability while preserving the critical performance characteristics required for real-time audio processing on the STM32 platform.

This approach maintains:
- Deterministic timing behavior essential for Eurorack modules
- Optimal memory usage for the constrained hardware
- Predictable performance at high tempos
- Simplicity for debugging and maintenance

The current monolithic file structure can be improved with inline includes without sacrificing the real-time performance that is critical for the PEW|FORMER project.