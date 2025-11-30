# PLAN-CurvePhase.md - CurveTrack Phasing Implementation Plan

## Overview
This document outlines the plan for implementing system tick granularity phasing capability for CurveTrack in the PEW|FORMER sequencer firmware. The goal is to add true LFO-style phasing effects with sample-accurate timing while maintaining compatibility with existing functionality.

## Requirements Analysis

### Objective
- Add phase control with system tick granularity for true LFO phasing effects
- Enable precise timing control for creating evolving textures and synchronization between multiple LFOs
- Maintain low CPU and memory overhead for STM32F4 compatibility

### Current Limitations
- Current rotation system operates at step level (coarse phasing between sequences)
- No fine-grained phase adjustment within a single step's duration
- No capability to achieve precise phasing effects between multiple CurveTracks

## Implementation Plan

### Phase 1: System Architecture Design
1. **Define phase parameters**
   - Phase offset parameter: 0.0 to 1.0 representing fractional position within the step
   - Phase modulation input capability for CV control
   - Phase range parameter for LFO-specific phasing

2. **Analyze resource constraints**
   - Calculate RAM requirements (estimated ~4-8 bytes per track for phase accumulator)
   - Estimate CPU overhead (additional 50-80 cycles per step update)
   - Verify compatibility with STM32F4 memory and processing limits

### Phase 2: Model Layer Enhancement
1. **Extend CurveTrack class**
   - Add phaseOffset property (0.0 to 1.0, mapped to 0-255 internally)
   - Add phaseModulationAmount property for CV control
   - Add phaseEnabled flag to enable/disable phasing effect

2. **Serialization updates**
   - Update write/read methods to include new phase parameters
   - Maintain backward compatibility with existing projects

### Phase 3: Engine Layer Implementation
1. **Update CurveTrackEngine**
   - Add phase accumulator variable to track precise phase position
   - Modify updateOutput function to apply phase offset to _currentStepFraction
   - Handle wraparound when phase-adjusted fraction exceeds 1.0 or goes below 0.0

2. **Phase calculation logic**
   - Implement phase-adjusted fraction: fmod(_currentStepFraction + phaseOffset, 1.0f)
   - Handle negative wraparound: if (phaseAdjustedFraction < 0.0f) phaseAdjustedFraction += 1.0f
   - Ensure smooth transitions when phase parameters change

### Phase 4: UI Integration
1. **CurveTrack parameter addition**
   - Add "PHASE" parameter to CurveTrackListModel
   - Implement editPhaseValue function with appropriate step sizes
   - Add printPhase function for display

2. **CurveSequenceEditPage enhancement**
   - Add phase visualization to the waveform display
   - Show current phase offset as a vertical indicator on the waveform

### Phase 5: Performance Optimization
1. **Fixed-point arithmetic implementation**
   - Use 32-bit fixed-point for phase calculations to avoid floating point overhead
   - Implement efficient phase wrapping using bit masking where possible

2. **Memory optimization**
   - Pack phase parameters efficiently in existing bitfield structure
   - Minimize additional memory allocation

### Phase 6: Testing Strategy
1. **Unit tests (TDD approach)**
   - Test phase calculation accuracy across different input values
   - Verify phase wraparound behavior
   - Test extreme values and edge cases
   - Validate phase parameter serialization/deserialization

2. **Integration tests**
   - Test phase interaction with existing CurveTrack parameters
   - Verify compatibility with sequence rotation
   - Test routing functionality for phase parameters
   - Validate performance impact measurements

3. **System tests**
   - Test with multiple CurveTracks simultaneously
   - Verify audio quality and absence of clicks/pops
   - Measure CPU utilization impact
   - Test with various tempo and divisor settings

### Phase 7: Documentation and Examples
1. **API documentation**
   - Update CurveTrack class documentation
   - Document new parameters and their ranges
   - Add usage examples

### Phase 8: Validation and Quality Assurance
1. **Hardware validation**
   - Test implementation on actual STM32 hardware
   - Verify timing accuracy and stability
   - Check for any performance degradation

2. **Regression testing**
   - Ensure all existing CurveTrack functionality remains intact
   - Validate that no performance regressions are introduced
   - Verify that all existing user projects continue to load and function correctly

## Technical Considerations

### Phase Calculation Approach
- The core logic will modify the curve evaluation:
  phaseAdjustedFraction = fmod(_currentStepFraction + phaseOffset, 1.0f)
- This allows precise shifting of the waveform within each step's duration

### Performance Targets
- CPU overhead: <0.05% additional processor load
- Memory overhead: <10 bytes additional RAM per CurveTrack
- No impact on audio quality or timing precision

### Integration Points
- Must integrate seamlessly with existing CurveTrack engine
- Compatible with all existing curve types and modulation features
- Maintains all current sequencing functionality

## Success Criteria
- Phasing works with all curve types (sine, triangle, sawtooth, square, etc.)
- Phase parameter can be smoothly adjusted in real-time
- Multiple CurveTracks can be precisely phased relative to each other
- Implementation has minimal impact on CPU and memory usage
- All existing functionality remains intact
- Phasing can be controlled via CV routing and MIDI

## Risk Mitigation
- Thorough unit testing to prevent regressions
- Performance monitoring to ensure no audio artifacts
- Careful memory management to stay within STM32 constraints
- Compatibility testing with existing projects
