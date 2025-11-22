# Tuesday Track Tick Distribution Implementation Plan

## Overview

This document outlines the plan for enhancing the PEW|FORMER Tuesday track to implement a more authentic timing system similar to the original Tuesday Eurorack module. The key enhancement is to add support for multiple algorithmic "ticks" within each main "step", with the ticks distributed across the step duration rather than processed as a burst at the beginning.

Based on resource analysis, this implementation will be restricted to only 1, 3, and 5 tick values to optimize performance and reliability on the STM32 hardware platform.

## Current State Analysis

### PEW|FORMER Tuesday Implementation
The current PEW|FORMER Tuesday track processes algorithmic output at fixed intervals based on the sequencer's timing (typically 16th notes). Each "step" corresponds to a timing boundary where the algorithm generates one output.

### Original Tuesday Module
The original Tuesday module operates with a more complex timing system where:
- Algorithms populate complete pattern arrays during initialization
- Each pattern contains multiple timed events
- Complex internal timing relationships allow for sophisticated rhythmic patterns
- Ticks Per Beat (TPB) and pattern structure provide fine-grained timing control

## Proposed Enhancement: Distributed Sub-Ticks

### Core Concept
Implement a "spread mode" where each main sequencer step can contain multiple algorithmic "ticks" (sub-ticks), with the algorithmic processing distributed across the step duration.

### Key Parameters
- **ticksPerStep**: A new parameter restricted to values 1, 3, or 5 that controls how many sub-ticks occur within each main step
  - 1: Standard timing (current behavior)
  - 3: Triplet subdivision (3 events per main step) - authentic triplet feel
  - 5: Quintuplet subdivision (5 events per main step) - authentic quintuplet feel
- **Sub-tick distribution**: Rather than processing all sub-ticks at the beginning of a step (burst mode), distribute them evenly across the step duration

## Implementation Architecture

### 1. Core State Management

#### New Engine Variables
```cpp
uint8_t _ticksPerStep = 1;        // Number of algorithmic ticks per main step (1, 3, or 5 only)
uint8_t _currentSubTick = 0;      // Current sub-tick within main step (0 to _ticksPerStep-1)
uint32_t _subTickDivisor = 0;     // Timing divisor for sub-ticks (main divisor / _ticksPerStep)
bool _subTickTriggered = false;   // Whether current sub-tick has been processed
uint32_t _subTickCounter = 0;     // Counter for sub-tick timing
```

#### Updated Timing Logic
- Calculate main step timing as currently done
- Calculate sub-tick timing by dividing the main step duration by `_ticksPerStep`
- Process algorithm logic at sub-tick intervals while maintaining main step coherence
- Use restricted values (1, 3, 5) to ensure predictable timing and bounded resource usage

### 2. TuesdayTrackEngine Modifications

#### Data Model Updates
- Add new member variables to manage sub-tick state in `TuesdayTrackEngine.h`
- Initialize new state variables in constructors and reset methods
- Maintain backward compatibility with existing functionality

#### Timing System Updates
- Modify the `tick()` method to detect both main step and sub-tick boundaries
- Process algorithm state evolution at sub-tick intervals
- Apply gate/CV outputs based on cooldown and algorithm conditions at each sub-tick
- Implement resource monitoring to prevent system overload

### 3. Algorithm Enhancements

#### State Evolution
Each algorithm will be enhanced to allow internal state to update at sub-tick resolution while maintaining musical coherence within each main step:

- **TEST Algorithm**: Vary note progression within step rather than just at boundaries
- **TRITRANCE Algorithm**: Update internal variables (b1, b2, b3) more frequently for complex arpeggiation
- **STOMPER Algorithm**: Update pattern state machine at sub-tick intervals for complex rhythms
- **MARKOV Algorithm**: Make multiple predictions within one main step for more activity

#### Output Logic
- Apply cooldown system at sub-tick level to control note density
- Maintain gate length calculations appropriate for sub-tick timing
- Update slide/portamento processing for distributed timing
- Optimize complex algorithms (MARKOV) for efficient processing at sub-tick rates

### 4. TuesdayTrack Model Integration

#### New Parameter
- Add `ticksPerStep` property with getter, setter, editor, and printer methods
- Range limited to 1, 3, or 5 (triangular, triplet, quintuplet subdivisions)
- Default value of 1 to ensure backward compatibility
- Include in serialization for project save/load operations

#### Editor Implementation
```cpp
void editTicksPerStep(int value, bool shift) {
    int current = ticksPerStep();
    if (value > 0) {
        // Cycle through valid values: 1 -> 3 -> 5 -> 1
        current = (current == 1) ? 3 : (current == 3) ? 5 : 1;
    } else if (value < 0) {
        // Cycle backwards: 1 -> 5 -> 3 -> 1
        current = (current == 1) ? 5 : (current == 5) ? 3 : 1;
    }
    setTicksPerStep(current);
}
```

### 5. Performance Considerations

#### Resource Management
- **Memory Impact**: Additional memory requirement of ~8-10 bytes per Tuesday track is negligible (0.005% of 192KB RAM)
- **CPU Overhead**: With restricted values (max 5 ticks), processing remains well within STM32 capabilities
- **Timing Precision**: High-resolution timer system supports required microsecond-level timing with predictable intervals
- **Resource Monitoring**: Implement rate limiting to prevent system overload when multiple Tuesday tracks use high subdivisions

#### Performance Testing
- Measure CPU usage at different `ticksPerStep` settings (1, 3, 5)
- Verify timing precision is maintained with sub-tick processing
- Test algorithm complexity scaling with multiple Tuesday tracks running
- Validate system stability with worst-case scenario: 4 Tuesday tracks with ticksPerStep=5

### 6. UI Integration

#### Parameter Access
- Add `ticksPerStep` to TuesdayPage parameter list
- Update F1-F5 selection cycle to include the parameter in rotation
- Provide visual feedback for current value and its musical meaning
- Follow existing UI conventions for parameter adjustment

#### User Experience
- Immediate feedback when changing parameter during playback
- Appropriate visual indication of current setting (e.g., "1" for straight, "3" for triplet, "5" for quintuplet)
- Context-sensitive help showing musical implications of each setting

### 7. Compatibility Verification

#### Existing Features Integration
- **Fill modes**: Maintain proper sub-tick processing during fill sequences
- **Mute/Solo**: Ensure all sub-tick processing stops when track is muted
- **Reset/Sync**: Properly reset sub-tick counters and algorithm state on sync signals
- **Pattern switching**: Handle parameter changes seamlessly during pattern changes
- **Loop boundaries**: Continue sub-tick processing correctly across loop boundaries
- **Gate settings**: Maintain compatibility with gate length, probability, and other parameters
- **Scale/Transposition**: Apply scale quantization and transposition to all sub-tick outputs
- **Project save/load**: Maintain backward compatibility - projects without the parameter default to 1

## Technical Implementation Steps

### Phase 1: Core Architecture
1. Add new state variables to TuesdayTrackEngine with restricted value handling
2. Implement sub-tick timing distribution algorithm optimized for 1/3/5 ratios
3. Update the tick() method to handle both main steps and sub-ticks while monitoring resource usage
4. Ensure backward compatibility with existing projects

### Phase 2: Algorithm Updates
1. Modify each algorithm to work with distributed sub-tick timing
2. Update state evolution logic to occur at sub-tick intervals
3. Maintain gate/CV output logic that works with distributed timing
4. Optimize complex algorithms (MARKOV, STOMPER) for efficient sub-tick processing
5. Test algorithm-specific behavior with various ticksPerStep values

### Phase 3: Model Integration
1. Add ticksPerStep parameter to TuesdayTrack model with restricted values
2. Implement getter/setter/editor/printer methods with cycle logic
3. Update serialization to include new parameter
4. Ensure default value maintains backward compatibility

### Phase 4: UI Integration
1. Add parameter to TuesdayPage UI with musical labeling
2. Update parameter selection cycle
3. Implement value adjustment controls with cycling through 1/3/5 values
4. Add visual feedback for current setting and musical implication

### Phase 5: Testing & Validation
1. Performance testing with different ticksPerStep values (1, 3, 5)
2. Compatibility testing with existing sequencer features
3. Timing precision verification at different subdivision rates
4. Stress testing with multiple Tuesday tracks at max subdivisions
5. User experience testing

## Benefits

### Musical Benefits
- **Enhanced Rhythmic Complexity**: Each main step can contain multiple algorithmic events (1, 3, or 5)
- **Authentic Timing**: Triplet and quintuplet subdivisions as fundamental rhythmic building blocks
- **Micro-Timing Control**: Fine-grained control over rhythmic patterns with musically relevant subdivisions
- **Complex Patterns**: Enable sophisticated algorithmic behavior with predictable polyrhythmic relationships (1:3, 1:5, 3:5 ratios)

### Technical Benefits
- **Hardware Compatibility**: Optimized for STM32F405 constraints with bounded resource usage
- **Performance**: Predictable timing with maximum 5x processing overhead
- **Reliability**: No risk of system overload from unrestricted subdivision values
- **Backward Compatibility**: Existing projects continue to work unchanged
- **Real-time Operation**: Bounded execution time ensures consistent timing

## Risks and Mitigation

### Performance Risks
- **Risk**: Multiple Tuesday tracks with high subdivisions (5) could impact CPU performance
- **Mitigation**: Implement resource monitoring and rate limiting, recommend max 4 Tuesday tracks with high subdivisions

### Timing Complexity
- **Risk**: Complex timing relationships could cause synchronization issues
- **Mitigation**: Thorough testing with various timing scenarios, maintain clear timing architecture

### User Complexity
- **Risk**: New parameter might confuse users about musical implications
- **Mitigation**: Default to 1 (existing behavior), provide clear UI and musical context for 3 (triplet) and 5 (quintuplet)

## Updated Conclusion

This plan provides a comprehensive approach to implementing distributed sub-tick timing in the PEW|FORMER Tuesday track with important constraints for hardware reliability. By restricting tick values to 1, 3, and 5, the implementation maintains the core musical benefits (straight, triplet, quintuplet subdivisions) while ensuring predictable resource usage and system stability on the STM32 platform.

The enhancement brings the implementation closer to the original Tuesday module's sophisticated timing system while maintaining the real-time performance and hardware compatibility expected from a Eurorack device. The restricted value approach creates an optimal balance between musical versatility and system reliability.

The implementation follows a phased approach that prioritizes backward compatibility, performance considerations, and user experience while delivering significant musical benefits in terms of rhythmic complexity and timing precision.