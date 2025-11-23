# TDD Implementation Plan for GateOffset Feature

## Phase 1: Setup and Core Data Structure (Test-First)

### Todo 1: Create Unit Test for BufferedStep GateOffset Field
- Write test to ensure BufferedStep struct includes gateOffset field
- Test default value (0)
- Test valid range (0-100)

### Todo 2: Extend BufferedStep Structure
- Add gateOffset field (uint8_t) to BufferedStep struct in TuesdayTrackEngine.h
- Compile and verify test passes

### Todo 3: Create Unit Test for TuesdayTrack GateOffset Property
- Test getter returns correct value
- Test setter clamps value to 0-100 range
- Test default value is 0

### Todo 4: Add GateOffset Property to TuesdayTrack
- Add _gateOffset member (uint8_t)
- Add gateOffset() getter
- Add setGateOffset() setter with clamping
- Add editGateOffset() method
- Add printGateOffset() method
- Compile and verify test passes

### Todo 5: Create Serialization Test
- Test that gateOffset is properly written to serialized data
- Test that gateOffset is properly read from serialized data
- Test with edge values (0, 100)

### Todo 6: Update Serialization Code
- Add _gateOffset to write() method
- Add _gateOffset to read() method with proper version checking
- Compile and verify test passes

### Commit Phase 1
- Commit all changes from Phase 1 with message: "Add GateOffset field to TuesdayTrack buffer structure"

## Phase 2: Buffer Generation Integration (Test-First)

### Todo 7: Create Unit Test for Buffer Generation with GateOffset
- Mock all algorithms and verify they set gateOffset field in buffer
- Test default value is set when algorithm doesn't specify
- Test values are within valid range

### Todo 8: Update Buffer Generation Loop
- Initialize gateOffset variable in buffer generation loop
- Store gateOffset in buffer entry
- Compile and verify test passes for each algorithm

### Todo 9: Create Algorithm-Specific Tests
- Write tests for each algorithm setting meaningful gateOffset values
- Test TRITRANCE generates phase-based offset patterns
- Test FUNK generates syncopated offset patterns
- Test other algorithms with appropriate timing variations

### Todo 10: Implement Algorithm GateOffset Logic
- Update each algorithm's buffer generation to set gateOffset
- Start with basic implementations, then add algorithm-specific patterns
- Compile and verify all algorithm tests pass

### Commit Phase 2
- Commit all changes from Phase 2 with message: "Implement GateOffset buffer generation for all algorithms"

## Phase 3: Runtime Processing (Test-First)

### Todo 11: Create Unit Test for GateOffset Override Logic
- Test that algorithmic gateOffset is preserved when user parameter is 0
- Test that user parameter properly overrides algorithmic value based on probability
- Test the random probability check works correctly

### Todo 12: Implement Override Logic in Tick Function
- Retrieve buffered gateOffset value
- Apply override logic using user parameter and random probability
- Compile and verify test passes

### Todo 13: Create Unit Test for Timing Calculation
- Test conversion from percentage to timing offset works correctly
- Test with various divisor values
- Test with various gateOffset percentage values

### Todo 14: Implement Timing Calculation
- Calculate actual gate offset in timing ticks
- Apply to gate scheduling logic
- Compile and verify test passes

### Commit Phase 3
- Commit all changes from Phase 3 with message: "Add GateOffset runtime processing and override logic"

## Phase 4: UI Integration (Test-First)

### Todo 15: Create UI Parameter Test
- Test that GateOffset parameter appears in parameter list
- Test that parameter can be selected and modified
- Test that value display works correctly

### Todo 16: Update UI Pages
- Add GateOffset to parameter selection
- Implement editing functionality
- Update display functions
- Compile and verify test passes

### Commit Phase 4
- Commit all changes from Phase 4 with message: "Add GateOffset UI controls and parameter editing"

## Phase 5: Integration Testing (Test-First)

### Todo 17: Create End-to-End Integration Test
- Test that changing GateOffset parameter affects actual gate timing
- Test with different algorithms
- Test override probability works in real-time

### Todo 18: Performance Tests
- Test that buffer generation performance is acceptable
- Test that runtime processing overhead is minimal
- Verify no timing issues in real-time processing

### Todo 19: Algorithm Validation Tests
- Verify each algorithm produces meaningful GateOffset values
- Test that GateOffset doesn't interfere with other algorithmic outputs
- Test extreme parameter combinations don't cause issues

### Commit Phase 5
- Commit all changes from Phase 5 with message: "Complete GateOffset integration and performance testing"

## Phase 6: Quality Assurance (Test-First)

### Todo 20: Regression Tests
- Ensure existing functionality still works with GateOffset changes
- Test all existing TuesdayTrack parameters still function correctly
- Test all algorithms still produce expected output patterns
- NOTE: Skip TestTuesdayTrack.cpp due to hardware driver segfault issues
- Use alternative test methods for TuesdayTrack functionality

### Todo 21: Edge Case Tests
- Test with divisor = 1 (fastest)
- Test with very high loop lengths
- Test with Power = 0 (silent)
- Test with extreme glide/gateOffset combinations

### Todo 22: Persistence Tests
- Test that GateOffset setting is properly saved to projects
- Test that GateOffset setting is properly loaded from projects
- Test multiple project switching works correctly

### Commit Phase 6
- Commit all changes from Phase 6 with message: "Finalize GateOffset implementation with QA, regression tests, and global override mechanism"

## Implementation Checklist

- [ ] BufferedStep struct extended with gateOffset field
- [ ] TuesdayTrack model includes gateOffset property
- [ ] Serialization updated for new property
- [ ] All algorithms generate gateOffset in buffer
- [ ] Override logic implemented in runtime
- [ ] Timing calculation implemented
- [ ] UI updated to include GateOffset control
- [ ] All unit tests passing
- [ ] All integration tests passing
- [ ] Performance targets met
- [ ] Documentation updated
- [ ] User interface intuitive and well-labeled