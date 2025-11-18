# HARMONY-TDD-PLAN.md - TDD Implementation Plan for Westlicht Performer Harmony Features

## Project Overview

This document outlines a comprehensive Test-Driven Development (TDD) plan for implementing the harmonic sequencing features in the Westlicht Performer. The plan follows the RED-GREEN-REFACTOR cycle and is structured according to the architecture defined in HARMONY-IDEATION.md and the conventions from HARMONAIG.md.

## Implementation Order (Based on HARMONAIG-FEASIBILITY.md)

1. **Core Harmony Engine** - Foundation logic and data structures
2. **Data Model Extensions** - Extend NoteSequence with harmony properties
3. **Engine Integration** - Modify NoteTrackEngine for harmony processing
4. **Global UI Pages** - HARM page for global settings
5. **Track Configuration** - UI for HarmonyRoot/follower assignment
6. **Step-Level Controls** - Voicing/Inversion layers under F3 NOTE button
7. **Accumulator Integration** - Ensure accumulator influences HarmonyRoot steps
8. **Testing & Validation** - Complete testing cycle

## Phase 1: Core Harmony Engine (Model Layer)

### Task 1.1: HarmonyEngine Scaffolding
**RED**: Write a failing test that attempts to create a HarmonyEngine instance and access default properties
```cpp
TEST(HarmonyEngine, DefaultConstruction) {
    HarmonyEngine engine;
    ASSERT_EQ(engine.mode(), HarmonyEngine::Mode::Ionian);
    ASSERT_EQ(engine.chordQuality(), HarmonyEngine::ChordQuality::Major7);
    // Add other default property assertions
}
```

**GREEN**: Implement basic HarmonyEngine class with default values
- Create `HarmonyEngine.h` and `HarmonyEngine.cpp`
- Define enums (Mode, ChordQuality, Voicing, Inversion)
- Implement default constructor with proper initialization

**REFACTOR**: Ensure consistent naming and structure

### Task 1.2: Mode and Scale Selection
**RED**: Write tests for setting and getting all 14 modes (7 Ionian + 7 Harmonic Minor)
```cpp
TEST(HarmonyEngine, SetAndGetIonianModes) {
    HarmonyEngine engine;
    for (int i = 0; i < 7; i++) {
        auto mode = static_cast<HarmonyEngine::Mode>(i);
        engine.setMode(mode);
        ASSERT_EQ(engine.mode(), mode);
    }
}
```

**GREEN**: Implement mode selection and storage
- Add mode property with getter/setter
- Ensure all 14 modes are supported

**REFACTOR**: Optimize mode representation

### Task 1.3: Chord Quality Implementation
**RED**: Write tests for all 8 chord qualities with correct interval definitions
```cpp
TEST(HarmonyEngine, ChordIntervals) {
    HarmonyEngine engine;
    auto intervals = engine.getChordIntervals(HarmonyEngine::ChordQuality::Major7);
    ASSERT_EQ(intervals[0], 0);  // Root
    ASSERT_EQ(intervals[1], 4);  // 3rd
    ASSERT_EQ(intervals[2], 7);  // 5th
    ASSERT_EQ(intervals[3], 11); // 7th
}
```

**GREEN**: Implement chord interval lookup tables
- Define all 8 chord qualities with correct semitone intervals
- Create lookup mechanism

**REFACTOR**: Optimize interval storage

### Task 1.4: Diatonic Chord Lookup Tables
**RED**: Write tests that verify correct chord quality selection for each scale degree
```cpp
TEST(HarmonyEngine, DiatonicChordQuality) {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);
    // Test C Major: I should be Major7, ii should be Minor7, etc.
    ASSERT_EQ(engine.getDiatonicChordQuality(0), HarmonyEngine::ChordQuality::Major7);   // I
    ASSERT_EQ(engine.getDiatonicChordQuality(1), HarmonyEngine::ChordQuality::Minor7);   // ii
    // Test other degrees...
}
```

**GREEN**: Implement diatonic chord lookup tables for all 14 modes
- Create comprehensive lookup tables from HARMONAIG.md research
- Ensure correct chord quality assignment per scale degree

**REFACTOR**: Optimize table structure and access

### Task 1.5: Basic Harmonization Method
**RED**: Write test for basic chord generation from root note
```cpp
TEST(HarmonyEngine, BasicHarmonization) {
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);
    engine.setChordQuality(HarmonyEngine::ChordQuality::Major7);
    HarmonyEngine::ChordNotes chord = engine.harmonize(60); // C4
    ASSERT_EQ(chord.root, 60);
    ASSERT_EQ(chord.third, 64);   // E4
    ASSERT_EQ(chord.fifth, 67);   // G4
    ASSERT_EQ(chord.seventh, 71); // B4
}
```

**GREEN**: Implement harmonize() method
- Calculate chord notes based on root and chord quality
- Handle all 8 chord types

**REFACTOR**: Optimize calculation logic

### Task 1.6: Inversion Implementation
**RED**: Write test for chord inversions (Root, 1st for initial implementation)
```cpp
TEST(HarmonyEngine, Inversion) {
    HarmonyEngine engine;
    engine.setChordQuality(HarmonyEngine::ChordQuality::Major7);
    HarmonyEngine::ChordNotes rootChord = engine.harmonizeWithInversion(60, 0); // Root
    HarmonyEngine::ChordNotes firstInversion = engine.harmonizeWithInversion(60, 1); // 1st
    
    // Root: C-E-G-B
    // 1st: E-G-B-C (E in bass)
    ASSERT_EQ(rootChord.root, 60);    // C
    ASSERT_EQ(firstInversion.root, 64); // E (moved from middle to bass)
}
```

**GREEN**: Implement inversion logic
- Handle Root (0) and 1st inversion (1) for initial implementation
- Apply octave shifts appropriately

**REFACTOR**: Ensure clean inversion algorithm

### Task 1.7: Voicing Implementation
**RED**: Write test for chord voicings (Close and Open for initial implementation)
```cpp
TEST(HarmonyEngine, Voicing) {
    HarmonyEngine engine;
    engine.setChordQuality(HarmonyEngine::ChordQuality::Major7);
    HarmonyEngine::ChordNotes closeVoicing = engine.harmonizeWithVoicing(60, HarmonyEngine::Voicing::Close);
    HarmonyEngine::ChordNotes openVoicing = engine.harmonizeWithVoicing(60, HarmonyEngine::Voicing::Open);
    
    // Close voicing should keep notes in tightest possible range
    // Open voicing should spread notes apart
    // Specific intervals will depend on implementation
}
```

**GREEN**: Implement voicing algorithms
- Implement Close and Open voicing for initial implementation
- Apply octave displacement as needed

**REFACTOR**: Optimize voicing calculations

### Task 1.8: Unison Mode Implementation
**RED**: Write test for unison mode where all outputs are identical
```cpp
TEST(HarmonyEngine, UnisonMode) {
    HarmonyEngine engine;
    engine.setUnisonMode(true);
    HarmonyEngine::ChordNotes chord = engine.harmonize(60);
    ASSERT_EQ(chord.root, 60);
    ASSERT_EQ(chord.third, 60);   // Same as root
    ASSERT_EQ(chord.fifth, 60);   // Same as root
    ASSERT_EQ(chord.seventh, 60); // Same as root
}
```

**GREEN**: Implement unison mode toggle
- Add unison mode flag and logic
- Return identical notes for all chord positions when enabled

**REFACTOR**: Integrate cleanly with existing harmonization

## Phase 2: Data Model Extensions

### Task 2.1: HarmonySequence Extension
**RED**: Write test for extended NoteSequence with harmony properties
```cpp
TEST(HarmonySequence, Extension) {
    HarmonySequence sequence;
    sequence.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);  // HarmonyRoot
    sequence.setFollowerAssignment(HarmonyEngine::FollowerType::Third);
    ASSERT_EQ(sequence.harmonyRole(), HarmonyEngine::HarmonyRole::Master);
    ASSERT_EQ(sequence.followerAssignment(), HarmonyEngine::FollowerType::Third);
}
```

**GREEN**: Create HarmonySequence class extending NoteSequence
- Add harmony role property (Master/HarmonyRoot, Follower)
- Add follower assignment property (Third/Fifth/Seventh)
- Maintain compatibility with existing NoteSequence functionality

**REFACTOR**: Ensure proper inheritance and composition

### Task 2.2: Per-Step Harmony Data
**RED**: Write test for per-step voicing and inversion storage
```cpp
TEST(HarmonySequence, PerStepData) {
    HarmonySequence sequence;
    sequence.setStepVoicing(0, HarmonyEngine::Voicing::Open);
    sequence.setStepInversion(0, 1); // 1st inversion
    ASSERT_EQ(sequence.stepVoicing(0), HarmonyEngine::Voicing::Open);
    ASSERT_EQ(sequence.stepInversion(0), 1);
}
```

**GREEN**: Add per-step harmony properties to sequence
- Add voicing and inversion storage per step
- Ensure proper serialization support

**REFACTOR**: Optimize data structure for memory efficiency

### Task 2.3: Harmony-Specific Step Properties
**RED**: Write test for harmony-specific step extensions
```cpp
TEST(HarmonySequence, StepExtensions) {
    HarmonySequence sequence;
    NoteSequence::Step step = sequence.step(0);
    // Test that harmony-specific properties are properly initialized
    // and accessible through the step interface
}
```

**GREEN**: Extend NoteSequence::Step with harmony-specific properties
- Add harmony parameters to step data structure
- Maintain backward compatibility

**REFACTOR**: Clean up step property organization

## Phase 3: Engine Integration

### Task 3.1: HarmonyTrackEngine Foundation
**RED**: Write test for basic HarmonyTrackEngine functionality
```cpp
TEST(HarmonyTrackEngine, Foundation) {
    Engine engine; // Mock or real engine as appropriate
    Model model; // Mock or real model
    Track track;
    HarmonyTrackEngine harmonyEngine(engine, model, track);
    
    // Test basic initialization
    ASSERT_NE(&harmonyEngine, nullptr);
}
```

**GREEN**: Create HarmonyTrackEngine class
- Implement base TrackEngine interface
- Connect to HarmonySequence
- Prepare for master/follower logic

**REFACTOR**: Ensure proper engine architecture alignment

### Task 3.2: Master Track Logic
**RED**: Write test for HarmonyRoot (Master) track behavior
```cpp
TEST(HarmonyTrackEngine, MasterTrack) {
    HarmonySequence sequence;
    sequence.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    
    HarmonyTrackEngine engine(/*setup*/);
    engine.setHarmonySequence(&sequence);
    
    // When acting as master, the track should evaluate normally
    // but also calculate harmonized notes for followers
    int16_t rootNote = engine.evalStepNote(/*tick*/);
    HarmonyEngine::ChordNotes chord = engine.currentChord();
    
    // The chord should be available for follower tracks
    // to access and transform appropriately
}
```

**GREEN**: Implement master track logic
- Normal sequence evaluation
- Harmonized chord calculation
- Make chord available to followers

**REFACTOR**: Optimize chord sharing mechanism

### Task 3.3: Follower Track Logic
**RED**: Write test for follower track behavior
```cpp
TEST(HarmonyTrackEngine, FollowerTrack) {
    HarmonySequence followerSequence;
    followerSequence.setHarmonyRole(HarmonyEngine::HarmonyRole::Follower);
    followerSequence.setFollowerAssignment(HarmonyEngine::FollowerType::Third);
    
    HarmonyTrackEngine masterEngine(/*setup*/);
    HarmonyTrackEngine followerEngine(/*setup*/);
    followerEngine.setMasterEngine(&masterEngine);
    
    // The follower should output the third of the master's chord
    int16_t followerNote = followerEngine.evalStepNote(/*tick*/);
    // This should equal the third degree of the master's current chord
}
```

**GREEN**: Implement follower track logic
- Connect to master track
- Extract appropriate chord degree (3rd, 5th, 7th)
- Output correct harmonized note

**REFACTOR**: Optimize follower communication

### Task 3.4: Track Restriction Logic (Tracks 1&5 Only)
**RED**: Write test to ensure only tracks 1 and 5 can be HarmonyRoot
```cpp
TEST(HarmonyTrackEngine, TrackRestrictions) {
    // Test that track 1 and 5 can become HarmonyRoot
    HarmonySequence seq1, seq5;
    seq1.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    seq5.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    
    // Test that tracks 2,3,4,6,7,8 cannot become HarmonyRoot
    // They should be limited to Follower roles only
    HarmonySequence seq2;
    seq2.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    // This should either fail gracefully or be ignored
}
```

**GREEN**: Implement track restriction logic
- Enforce that only tracks 1 and 5 can be HarmonyRoot
- Limit other tracks to follower roles
- Enforce follower position limitations

**REFACTOR**: Integrate cleanly with existing track system

## Phase 4: Global UI Pages

### Task 4.1: HarmonyListModel (HARM Page Data Model)
**RED**: Write test for HarmonyListModel that verifies access to harmony parameters
```cpp
TEST(HarmonyListModel, ParameterAccess) {
    HarmonyEngine engine;
    HarmonyListModel model(&engine);
    
    // Test that the model can retrieve and set harmony parameters
    model.edit(0, 1, 1, false); // Try to change mode
    // Verify the underlying engine was updated
    ASSERT_EQ(engine.mode(), HarmonyEngine::Mode::Dorian); // Assuming 1 corresponds to Dorian
}
```

**GREEN**: Create HarmonyListModel class
- Implement ListModel interface for HARM page
- Connect to HarmonyEngine
- Handle parameter editing

**REFACTOR**: Follow existing ListModel patterns

### Task 4.2: HARM Global Page Implementation
**RED**: Write test for HARM page functionality (UI model logic)
```cpp
TEST(HarmonyGlobalPage, PageLogic) {
    HarmonyGlobalPage page;
    // Test navigation through parameters
    // Test that parameter changes are reflected in the engine
    // Test visual feedback elements
}
```

**GREEN**: Create HarmonyGlobalPage class
- Implement ListPage interface
- Display all global harmony parameters
- Enable parameter editing

**REFACTOR**: Match Performer UI styling and patterns

### Task 4.3: Global Parameter Controls
**RED**: Write test for all HARM page parameters
```cpp
TEST(HarmonyGlobalPage, AllParameters) {
    HarmonyGlobalPage page;
    // Test each parameter: Mode, Quality, Unison, etc.
    // Verify each can be set and retrieved correctly
    // Verify they update the engine properly
}
```

**GREEN**: Implement all global controls
- Scale/Mode selection (14 options)
- Chord quality selection (8 options for diatonic)
- Unison mode toggle
- Global voicing options
- Global inversion options

**REFACTOR**: Optimize parameter organization

## Phase 5: Track Configuration UI

### Task 5.1: Harmony Track Mode Selection
**RED**: Write test for harmony track configuration options
```cpp
TEST(HarmonyTrackConfig, ModeSelection) {
    HarmonySequence sequence;
    // Test that HarmonyRoot can only be set on tracks 1 and 5
    // Test that follower options are available on tracks 2-4 and 6-8
    // Test that follower assignments (3rd, 5th, 7th) work correctly
}
```

**GREEN**: Implement track configuration UI
- Add harmony role selection to track settings
- Enforce track position restrictions
- Enable follower type selection

**REFACTOR**: Integrate with existing track configuration

### Task 5.2: Visual Indicators
**RED**: Write test for visual feedback of harmony relationships
```cpp
TEST(HarmonyTrackConfig, VisualFeedback) {
    // Test that HarmonyRoot tracks are clearly indicated
    // Test that follower tracks show their relationship
    // Test that chord information displays correctly
}
```

**GREEN**: Implement visual feedback elements
- Clear HarmonyRoot indicators
- Follower relationship indicators
- Chord symbol displays

**REFACTOR**: Ensure consistent visual language

## Phase 6: Step-Level Controls (F3 NOTE Button Layers)

### Task 6.1: Voicing Layer Implementation
**RED**: Write test for per-step voicing control in F3 NOTE button cycle
```cpp
TEST(HarmonyStepLayers, VoicingLayer) {
    NoteSequence sequence;
    // Simulate cycling through F3 NOTE button layers
    // Verify Voicing layer is accessible
    // Verify per-step voicing can be set and retrieved
    sequence.setLayer(NoteSequence::Layer::HarmonyVoicing);
    sequence.setStepValue(0, static_cast<int>(HarmonyEngine::Voicing::Open));
    ASSERT_EQ(sequence.stepVoicing(0), HarmonyEngine::Voicing::Open);
}
```

**GREEN**: Implement voicing layer in existing NOTE button cycle
- Add HarmonyVoicing to Layer enum
- Extend layerRange and layerDefaultValue functions
- Implement voicing editing functionality

**REFACTOR**: Integrate cleanly with existing layer system

### Task 6.2: Inversion Layer Implementation
**RED**: Write test for per-step inversion control in F3 NOTE button cycle
```cpp
TEST(HarmonyStepLayers, InversionLayer) {
    NoteSequence sequence;
    // Similar to voicing but for inversion
    sequence.setLayer(NoteSequence::Layer::HarmonyInversion);
    sequence.setStepValue(0, 1); // 1st inversion
    ASSERT_EQ(sequence.stepInversion(0), 1);
}
```

**GREEN**: Implement inversion layer in existing NOTE button cycle
- Add HarmonyInversion to Layer enum
- Extend layer handling functions
- Implement inversion editing

**REFACTOR**: Maintain consistency with voicing layer

### Task 6.3: Layer Cycling Integration
**RED**: Write test for complete F3 NOTE button cycling sequence
```cpp
TEST(HarmonyStepLayers, CompleteCycle) {
    // Test cycle: Note → NoteVariationRange → NoteVariationProbability → Voicing → Inversion → Note
    NoteSequence sequence;
    int currentLayer = sequence.layer();
    ASSERT_EQ(currentLayer, NoteSequence::Layer::Note);
    
    // Simulate multiple button presses
    sequence.setLayer(NextLayer(currentLayer)); // NoteVariationRange
    sequence.setLayer(NextLayer(sequence.layer())); // NoteVariationProbability
    sequence.setLayer(NextLayer(sequence.layer())); // HarmonyVoicing
    sequence.setLayer(NextLayer(sequence.layer())); // HarmonyInversion
    sequence.setLayer(NextLayer(sequence.layer())); // Note (wraps around)
}
```

**GREEN**: Integrate harmony layers into existing cycle
- Modify layer cycling logic to include harmony layers
- Ensure proper wrapping in both directions
- Update UI to reflect new layers

**REFACTOR**: Optimize layer management

## Phase 7: Accumulator Integration

### Task 7.1: HarmonyRoot Accumulator Influence
**RED**: Write test to verify accumulator affects HarmonyRoot steps
```cpp
TEST(HarmonyAccumulator, RootInfluence) {
    HarmonySequence harmonySequence;
    harmonySequence.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    
    Accumulator accumulator;
    accumulator.setEnabled(true);
    accumulator.setStepValue(1);
    accumulator.setMinValue(-12);
    accumulator.setMaxValue(12);
    
    // Test that accumulator affects HarmonyRoot note output
    // This should then propagate harmonically to followers
}
```

**GREEN**: Ensure accumulator integration works with HarmonyRoot
- Verify accumulator modulation of HarmonyRoot sequence
- Ensure harmonized followers receive modulated root
- Test that chord qualities adapt appropriately

**REFACTOR**: Optimize accumulator-harmony interaction

### Task 7.2: Propagated Modulation
**RED**: Write test for accumulator modulation propagating to followers
```cpp
TEST(HarmonyAccumulator, Propagation) {
    // Set up master track with accumulator
    // Set up follower tracks
    // Change accumulator value
    // Verify all follower tracks respond appropriately
}
```

**GREEN**: Implement proper modulation propagation
- When HarmonyRoot note changes due to accumulator
- Recalculate harmonized notes for followers
- Apply any per-step voicing/inversion changes

**REFACTOR**: Optimize propagation efficiency

## Phase 8: Comprehensive Testing & Validation

### Task 8.1: Edge Cases and Boundary Conditions Testing
**RED**: Write comprehensive tests for edge cases and boundary conditions
```cpp
// Test 8.1.1: Note range boundaries
TEST(HarmonyEngine, NoteRangeBoundaries) {
    HarmonyEngine engine;
    // Test lowest possible note (0) - should handle gracefully
    HarmonyEngine::ChordNotes lowChord = engine.harmonize(0);
    ASSERT_GE(lowChord.root, 0);
    ASSERT_LE(lowChord.root, 127);
    ASSERT_GE(lowChord.third, 0);
    ASSERT_LE(lowChord.third, 127);
    // Test highest possible note (127) - should handle gracefully
    HarmonyEngine::ChordNotes highChord = engine.harmonize(127);
    ASSERT_GE(highChord.root, 0);
    ASSERT_LE(highChord.root, 127);
    // Test notes that would cause chord extensions to exceed MIDI range
    HarmonyEngine::ChordNotes extendedChord = engine.harmonize(120); // G9 would go above 127
    // Should handle gracefully with octave reduction or similar
}

// Test 8.1.2: Invalid mode and chord quality combinations
TEST(HarmonyEngine, InvalidCombinations) {
    HarmonyEngine engine;
    // Test setting chord quality that doesn't exist
    // Test setting mode that doesn't exist
    // Test out-of-bounds values
    engine.setMode(static_cast<HarmonyEngine::Mode>(-1));
    ASSERT_EQ(engine.mode(), HarmonyEngine::Mode::Ionian); // Should default or stay in valid range
    engine.setMode(static_cast<HarmonyEngine::Mode>(20)); // Beyond valid range
    ASSERT_EQ(engine.mode(), HarmonyEngine::Mode::Ionian); // Should default or stay in valid range
}

// Test 8.1.3: Zero or negative chord quality intervals
TEST(HarmonyEngine, ZeroIntervalChords) {
    HarmonyEngine engine;
    // Test special cases where chord intervals might be zero
    // Test unison chord quality if implemented
    // Test chord qualities that might have duplicate notes
}

// Test 8.1.4: Maximum inversion handling
TEST(HarmonyEngine, MaxInversion) {
    HarmonyEngine engine;
    // Test inversion values beyond what's supported
    HarmonyEngine::ChordNotes chord = engine.harmonizeWithInversion(60, 10); // Way beyond valid range
    // Should handle gracefully, probably default to root position
    ASSERT_EQ(chord.root, 60); // Root should still be correct
}

// Test 8.1.5: Extreme voicing scenarios
TEST(HarmonyEngine, ExtremeVoicing) {
    HarmonyEngine engine;
    // Test voicing that would cause notes to go outside MIDI range
    // Test when close voicing would create very wide intervals
    // Test when open voicing would go outside MIDI range
}
```

**GREEN**: Implement boundary checks and graceful handling
- Add validation for all input parameters
- Implement range checking for note values
- Add proper error handling for invalid combinations
- Ensure graceful degradation for out-of-bounds values

**REFACTOR**: Centralize validation logic

### Task 8.2: Error Handling and Robustness Testing
**RED**: Write tests for error conditions and system robustness
```cpp
// Test 8.2.1: Null pointer handling
TEST(HarmonyEngine, NullPointerSafety) {
    HarmonyEngine* engine = nullptr;
    // Test that operations on null pointers don't crash
    // This might involve testing with mock objects that can return null
    // Or testing the engine's handling of null references internally
}

// Test 8.2.2: Invalid sequence data handling
TEST(HarmonySequence, InvalidData) {
    HarmonySequence sequence;
    // Test loading corrupted sequence data
    // Test with invalid chord quality values in sequence
    // Test with invalid inversion/voicing values
    sequence.setStepValue(0, 999); // Invalid value for harmony layer
    // Should handle gracefully without crashing
}

// Test 8.2.3: Memory allocation failure simulation
TEST(HarmonyEngine, MemoryFailure) {
    // If possible, simulate memory allocation failures
    // Test that the engine handles out-of-memory conditions gracefully
    // This might require special testing infrastructure
}

// Test 8.2.4: Invalid track assignments
TEST(HarmonyTrackEngine, InvalidAssignments) {
    // Test assigning follower to non-existent master
    // Test circular references between tracks
    // Test assigning follower to follower (invalid chain)
    HarmonyTrackEngine followerEngine(/*setup*/);
    followerEngine.setMasterEngine(nullptr); // No master assigned
    // Should handle gracefully, possibly outputting silence or original note
}
```

**GREEN**: Implement robust error handling
- Add null pointer checks
- Implement data validation
- Add graceful degradation for invalid states
- Log errors appropriately without crashing

**REFACTOR**: Standardize error handling patterns

### Task 8.3: Performance Benchmarking
**RED**: Write comprehensive performance tests with benchmarks
```cpp
// Test 8.3.1: Real-time performance under load
TEST(HarmonyPerformance, RealTimeConstraints) {
    HarmonyEngine engine;
    const int iterations = 10000;
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        HarmonyEngine::ChordNotes chord = engine.harmonize(60 + (i % 60)); // Cycle through notes
        engine.setMode(static_cast<HarmonyEngine::Mode>((i % 14)));
        engine.setChordQuality(static_cast<HarmonyEngine::ChordQuality>((i % 8)));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto avgMicroseconds = duration.count() / iterations;

    // Should be well under 2ms (2000 microseconds) per operation
    ASSERT_LT(avgMicroseconds, 100); // Aim for sub-100 microsecond operations
}

// Test 8.3.2: Memory usage profiling
TEST(HarmonyPerformance, MemoryUsage) {
    size_t initialMemory = getCurrentMemoryUsage();

    // Create multiple HarmonyEngine instances
    std::vector<std::unique_ptr<HarmonyEngine>> engines;
    for (int i = 0; i < 100; ++i) {
        engines.push_back(std::make_unique<HarmonyEngine>());
    }

    size_t finalMemory = getCurrentMemoryUsage();
    size_t perEngineMemory = (finalMemory - initialMemory) / 100;

    // Each engine should use minimal memory (less than 1KB)
    ASSERT_LT(perEngineMemory, 1024);
}

// Test 8.3.3: Peak performance with all tracks active
TEST(HarmonyPerformance, PeakLoad) {
    // Simulate all 8 tracks running with harmony features
    // Measure CPU usage and timing
    std::vector<HarmonyTrackEngine> trackEngines;
    for (int i = 0; i < 8; ++i) {
        trackEngines.emplace_back(/*setup*/);
    }

    // Simulate a full performance cycle
    const int ticksPerSecond = 1000; // Approximate Performer tick rate
    const int durationMs = 1000; // 1 second test
    const int totalTicks = (ticksPerSecond * durationMs) / 1000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int tick = 0; tick < totalTicks; ++tick) {
        for (auto& engine : trackEngines) {
            // Simulate engine processing for this tick
            engine.processTick(tick);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Total processing time should be much less than 1000ms
    // Leave plenty of headroom for other system operations
    ASSERT_LT(duration.count(), 200000); // Less than 200ms for 1 second of audio
}

// Test 8.3.4: Lookup table access performance
TEST(HarmonyPerformance, LookupTable) {
    HarmonyEngine engine;
    const int iterations = 100000;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        auto quality = engine.getDiatonicChordQuality(i % 7);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    auto avgNanoseconds = duration.count() / iterations;

    // Lookup should be very fast (single digit nanoseconds)
    ASSERT_LT(avgNanoseconds, 100);
}
```

**GREEN**: Implement performance optimizations
- Optimize lookup table access with direct array indexing
- Pre-calculate common values
- Use efficient data structures
- Profile and optimize hot paths

**REFACTOR**: Optimize critical performance paths

### Task 8.4: Memory Usage Tests
**RED**: Write tests to validate memory constraints
```cpp
// Test 8.4.1: Sequence memory overhead
TEST(HarmonyMemory, SequenceOverhead) {
    NoteSequence baseSequence;
    HarmonySequence harmonySequence;

    size_t baseSize = sizeof(baseSequence);
    size_t harmonySize = sizeof(harmonySequence);
    size_t overhead = harmonySize - baseSize;

    // Harmony extension should add minimal overhead
    ASSERT_LT(overhead, 512); // Less than 0.5KB overhead per sequence
}

// Test 8.4.2: Per-step memory usage
TEST(HarmonyMemory, PerStepUsage) {
    HarmonySequence sequence;

    // Test memory usage with maximum steps (64)
    for (int i = 0; i < 64; ++i) {
        sequence.setStepVoicing(i, HarmonyEngine::Voicing::Open);
        sequence.setStepInversion(i, 2); // Second inversion
    }

    // Calculate approximate memory usage for per-step data
    // Should be minimal per step to maintain performance
    size_t memoryPerStep = estimateMemoryPerStep(&sequence);
    ASSERT_LT(memoryPerStep, 16); // Less than 16 bytes per step for harmony data
}

// Test 8.4.3: Lookup table memory optimization
TEST(HarmonyMemory, LookupTables) {
    HarmonyEngine engine;

    // Verify lookup tables are efficiently stored
    // No dynamically allocated memory for fixed tables
    size_t lookupMemory = engine.getLookupTableMemoryUsage();
    ASSERT_LT(lookupMemory, 1024); // Less than 1KB for all lookup tables
}
```

**GREEN**: Optimize memory usage
- Use packed structures where possible
- Minimize per-step data storage
- Optimize lookup table storage
- Consider bit-packing for boolean flags

**REFACTOR**: Optimize memory layout

### Task 8.5: Integration Tests with Existing Performer Features
**RED**: Write comprehensive integration tests with Performer features
```cpp
// Test 8.5.1: Ratchet integration
TEST(HarmonyIntegration, Ratchets) {
    // Set up HarmonyRoot track with ratchets enabled
    HarmonySequence harmonySeq;
    harmonySeq.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);

    // Set ratchet value for a step
    harmonySeq.setRatchet(0, 3); // Triple note on first step

    // Set up follower tracks
    HarmonySequence followerSeq;
    followerSeq.setHarmonyRole(HarmonyEngine::HarmonyRole::Follower);
    followerSeq.setFollowerAssignment(HarmonyEngine::FollowerType::Third);

    // Verify that ratchets properly trigger harmonized notes
    // Each ratchet hit should produce the appropriate harmonized chord
    // All follower tracks should respond to each ratchet hit
}

// Test 8.5.2: Probability integration
TEST(HarmonyIntegration, Probability) {
    // Set up track with probability for step activation
    HarmonySequence harmonySeq;
    harmonySeq.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    harmonySeq.setProbability(0, 50); // 50% chance for first step

    // Set up follower tracks
    HarmonySequence followerSeq;
    followerSeq.setHarmonyRole(HarmonyEngine::HarmonyRole::Follower);
    followerSeq.setFollowerAssignment(HarmonyEngine::FollowerType::Fifth);

    // Run multiple iterations and verify probability behavior
    // When HarmonyRoot step fires, followers should also fire
    // When HarmonyRoot step is skipped, followers should also be silent
    int followerFiredCount = 0;
    const int iterations = 1000;

    for (int i = 0; i < iterations; ++i) {
        bool rootFired = harmonySeq.stepShouldFire(0); // Based on probability
        bool followerFired = /*check if follower fired*/;

        if (rootFired) {
            followerFiredCount++; // Should match follower fire count
        }
    }

    // Probability should be maintained across harmony relationships
    ASSERT_NEAR(followerFiredCount, 500, 50); // ~50% with tolerance
}

// Test 8.5.3: Variation integration
TEST(HarmonyIntegration, Variations) {
    // Set up sequence with note variations
    HarmonySequence harmonySeq;
    harmonySeq.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    harmonySeq.setNote(0, 60); // C4
    harmonySeq.setNoteVariationRange(0, 2); // +/-2 semitones
    harmonySeq.setNoteVariationProbability(0, 100); // Always vary

    // Set up follower tracks
    // Verify that variations in HarmonyRoot properly propagate harmonically
    // If root goes from C4 to D4, followers should follow appropriately
    // (e.g., if followers were E4, G4, B4, they should become F4, A4, C5)
}

// Test 8.5.4: Gate length integration
TEST(HarmonyIntegration, GateLength) {
    // Set up tracks with different gate lengths
    // Verify that harmonized notes have appropriate timing
    // Gate length should be preserved in follower tracks
}

// Test 8.5.5: Pattern chaining integration
TEST(HarmonyIntegration, PatternChaining) {
    // Set up pattern chains with harmony tracks
    // Test that harmony relationships maintain across pattern changes
    // Test that global harmony settings persist across pattern changes
    // Test that per-step harmony settings can be pattern-specific
}
```

**GREEN**: Implement proper integration with existing features
- Ensure harmony calculations respect ratchet timing
- Maintain probability behavior across harmony relationships
- Properly propagate variations harmonically
- Preserve timing relationships

**REFACTOR**: Optimize integration points

### Task 8.6: UI Interaction Pattern Tests
**RED**: Write tests for UI interaction patterns and user workflows
```cpp
// Test 8.6.1: Global HARM page navigation
TEST(HarmonyUI, GlobalPageNavigation) {
    HarmonyGlobalPage page;

    // Test navigation through all global parameters
    // Verify parameter values update correctly
    // Test parameter limits and wrapping behavior
    int initialParam = page.currentParameter();
    page.navigateNext();
    ASSERT_NE(page.currentParameter(), initialParam);

    // Test that parameter changes are immediately reflected
    page.setValue(5); // Change current parameter
    ASSERT_EQ(page.getValue(), 5);
}

// Test 8.6.2: Track configuration workflow
TEST(HarmonyUI, TrackConfigurationWorkflow) {
    // Test complete workflow: selecting track → setting to HarmonyRoot → configuring followers
    // Verify proper restrictions are enforced (only tracks 1&5 for HarmonyRoot)
    // Test error messages when invalid assignments are attempted
    HarmonySequence track1Seq, track2Seq;

    // Attempt to set track 2 as HarmonyRoot (should fail)
    track2Seq.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    // Should either be ignored or reverted to follower
    ASSERT_EQ(track2Seq.harmonyRole(), HarmonyEngine::HarmonyRole::Follower);

    // Set track 1 as HarmonyRoot (should succeed)
    track1Seq.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    ASSERT_EQ(track1Seq.harmonyRole(), HarmonyEngine::HarmonyRole::Master);
}

// Test 8.6.3: Step-level editing workflow
TEST(HarmonyUI, StepLevelWorkflow) {
    NoteSequence sequence;

    // Test cycling through F3 NOTE layers: Note → NoteVariationRange → NoteVariationProbability → Voicing → Inversion → Note
    sequence.setLayer(NoteSequence::Layer::Note);
    sequence.setLayer(NextLayer(sequence.layer())); // Should be NoteVariationRange
    ASSERT_EQ(sequence.layer(), NoteSequence::Layer::NoteVariationRange);

    sequence.setLayer(NextLayer(sequence.layer())); // Should be NoteVariationProbability
    ASSERT_EQ(sequence.layer(), NoteSequence::Layer::NoteVariationProbability);

    sequence.setLayer(NextLayer(sequence.layer())); // Should be HarmonyVoicing
    ASSERT_EQ(sequence.layer(), NoteSequence::Layer::HarmonyVoicing);

    sequence.setLayer(NextLayer(sequence.layer())); // Should be HarmonyInversion
    ASSERT_EQ(sequence.layer(), NoteSequence::Layer::HarmonyInversion);

    sequence.setLayer(NextLayer(sequence.layer())); // Should wrap back to Note
    ASSERT_EQ(sequence.layer(), NoteSequence::Layer::Note);

    // Test reverse cycling
    sequence.setLayer(PrevLayer(sequence.layer())); // Should go back to HarmonyInversion
    ASSERT_EQ(sequence.layer(), NoteSequence::Layer::HarmonyInversion);
}

// Test 8.6.4: Real-time parameter changes
TEST(HarmonyUI, RealTimeChanges) {
    // Test that changing harmony parameters during playback
    // produces immediate audible results
    // This might require audio output verification or state tracking
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);
    HarmonyEngine::ChordNotes chordBefore = engine.harmonize(60);

    engine.setMode(HarmonyEngine::Mode::Dorian);
    HarmonyEngine::ChordNotes chordAfter = engine.harmonize(60);

    // Chord should be different after mode change
    ASSERT_NE(chordBefore.third, chordAfter.third);
}

// Test 8.6.5: Preset saving/loading
TEST(HarmonyUI, PresetHandling) {
    // Test saving harmony settings to presets
    // Test loading harmony settings from presets
    // Test project save/load with harmony data
    HarmonySequence sequence;
    sequence.setHarmonyRole(HarmonyEngine::HarmonyRole::Master);
    sequence.setStepVoicing(0, HarmonyEngine::Voicing::Open);
    sequence.setStepInversion(0, 1);

    // Save to preset
    std::string presetData = sequence.saveToPreset();

    // Load from preset to new sequence
    HarmonySequence loadedSequence;
    loadedSequence.loadFromPreset(presetData);

    // Verify all harmony settings were preserved
    ASSERT_EQ(loadedSequence.harmonyRole(), HarmonyEngine::HarmonyRole::Master);
    ASSERT_EQ(loadedSequence.stepVoicing(0), HarmonyEngine::Voicing::Open);
    ASSERT_EQ(loadedSequence.stepInversion(0), 1);
}
```

**GREEN**: Implement proper UI workflows and validation
- Add proper navigation and parameter validation
- Implement clear error messaging
- Ensure real-time parameter updates
- Support preset and project serialization

**REFACTOR**: Optimize UI response and feedback

### Task 8.7: User Workflow Validation
**RED**: Create comprehensive user workflow tests
```cpp
// Test 8.7.1: Common harmony progression workflow
TEST(HarmonyUserWorkflow, CommonProgressions) {
    // Test setting up I-V-vi-IV progression
    // Track 1: HarmonyRoot playing C-F-G-Am pattern
    // Track 2: Third follower (E-A-B-C)
    // Track 3: Fifth follower (G-C-D-E)
    // Verify the resulting harmony sounds musically correct

    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);

    // Simulate progression: C - F - G - Am
    HarmonyEngine::ChordNotes cChord = engine.harmonize(60);    // C Major7
    HarmonyEngine::ChordNotes fChord = engine.harmonize(65);    // F Major7
    HarmonyEngine::ChordNotes gChord = engine.harmonize(67);    // G Major7
    HarmonyEngine::ChordNotes amChord = engine.harmonize(69);   // A Minor7

    // Verify chord qualities match expected diatonic qualities
    // I: Major7, IV: Major7, V: Major7, vi: Minor7
    // This validates the diatonic chord quality lookup tables
}

// Test 8.7.2: Modal interchange workflow
TEST(HarmonyUserWorkflow, ModalInterchange) {
    // Test switching between related modes within a sequence
    // e.g., using notes from parallel minor in major key
    // Verify that chord qualities adjust appropriately

    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian); // C Major

    // Play C Major scale degrees but switch to Dorian for ii chord
    // This should give D minor instead of D minor 7(b5)
    engine.setMode(HarmonyEngine::Mode::Dorian);
    HarmonyEngine::ChordNotes dChord = engine.harmonize(62); // D
    // Should be D Minor7 not D Minor7(b5)
}

// Test 8.7.3: Voice leading workflow
TEST(HarmonyUserWorkflow, VoiceLeading) {
    // Test per-step voicing to create smooth voice leading
    // C Major (close) → F Major (open) → G Major (close) → C Major (open)
    // Verify that voicing changes create the intended textural effect

    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);

    // Close voicing for C
    HarmonyEngine::ChordNotes cClose = engine.harmonizeWithVoicing(60, HarmonyEngine::Voicing::Close);
    // Open voicing for F
    HarmonyEngine::ChordNotes fOpen = engine.harmonizeWithVoicing(65, HarmonyEngine::Voicing::Open);

    // Measure voice leading smoothness (minimal interval movement)
    // This might involve calculating interval changes between chord notes
}

// Test 8.7.4: Bass line creation workflow
TEST(HarmonyUserWorkflow, BassLineCreation) {
    // Test using inversions to create interesting bass lines
    // C (root) → E (1st inv) → G (2nd inv) → C (root) = C bass line
    // This creates a melodic bass movement while maintaining harmony

    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);

    HarmonyEngine::ChordNotes cRoot = engine.harmonizeWithInversion(60, 0);     // C in bass
    HarmonyEngine::ChordNotes cFirstInv = engine.harmonizeWithInversion(60, 1); // E in bass
    HarmonyEngine::ChordNotes cSecondInv = engine.harmonizeWithInversion(60, 2); // G in bass
    HarmonyEngine::ChordNotes cThirdInv = engine.harmonizeWithInversion(60, 3);  // B in bass

    // Verify bass notes are as expected
    ASSERT_EQ(cRoot.root, 60);      // C bass
    ASSERT_EQ(cFirstInv.root, 64);  // E bass (from 1st inversion)
    ASSERT_EQ(cSecondInv.root, 67); // G bass (from 2nd inversion)
    ASSERT_EQ(cThirdInv.root, 71);  // B bass (from 3rd inversion)
}

// Test 8.7.5: Live performance workflow
TEST(HarmonyUserWorkflow, LivePerformance) {
    // Test rapid parameter changes during live performance
    // Global scale changes
    // Track role switching (Master ↔ Follower)
    // Per-step parameter adjustments
    // Pattern chaining with harmony
    // Verify no audio glitches or hangs

    HarmonyEngine engine;

    // Simulate rapid scale changes during performance
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i) {
        engine.setMode(static_cast<HarmonyEngine::Mode>(i % 14));
        HarmonyEngine::ChordNotes chord = engine.harmonize(60 + (i % 12));
        // Should complete quickly without blocking audio thread
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete in well under 100ms for 100 operations
    ASSERT_LT(duration.count(), 100);
}
```

**GREEN**: Validate all user workflows work as expected
- Ensure musical correctness of generated harmonies
- Optimize for live performance scenarios
- Verify smooth parameter changes
- Test all common usage patterns

**REFACTOR**: Fine-tune user experience based on workflow validation

### Task 8.8: Regression Testing Suite
**RED**: Create comprehensive regression tests
```cpp
// Test 8.8.1: HarmonyEngine regression tests
TEST(HarmonyRegression, EngineConsistency) {
    // Test that harmony calculations remain consistent across versions
    // Use fixed inputs and verify fixed outputs
    HarmonyEngine engine;
    engine.setMode(HarmonyEngine::Mode::Ionian);
    engine.setChordQuality(HarmonyEngine::ChordQuality::Major7);

    HarmonyEngine::ChordNotes expected = {60, 64, 67, 71}; // C Major7
    HarmonyEngine::ChordNotes actual = engine.harmonize(60);

    ASSERT_EQ(actual.root, expected.root);
    ASSERT_EQ(actual.third, expected.third);
    ASSERT_EQ(actual.fifth, expected.fifth);
    ASSERT_EQ(actual.seventh, expected.seventh);
}

// Test 8.8.2: Integration regression tests
TEST(HarmonyRegression, IntegrationConsistency) {
    // Test complete harmony scenarios with known expected results
    // This prevents breaking changes from affecting existing projects
    // Test specific chord progressions that should always work the same way
}
```

**GREEN**: Establish regression test baseline
- Document expected behavior for key scenarios
- Create snapshot tests for complex outputs
- Ensure backward compatibility

**REFACTOR**: Organize regression tests for maintainability

## Test File Organization

- `TestHarmonyEngine.cpp` - Core HarmonyEngine functionality, edge cases, boundary conditions, error handling
- `TestHarmonySequence.cpp` - Harmony sequence model extensions, memory usage tests
- `TestHarmonyTrackEngine.cpp` - Engine integration (may be disabled like TestNoteTrackEngine), invalid assignments
- `TestHarmonyUI.cpp` - UI component tests, interaction patterns, user workflow validation
- `TestHarmonyIntegration.cpp` - Full system integration tests, performer feature integration (ratchets, probability, variations, etc.)
- `TestHarmonyPerformance.cpp` - Performance benchmarking, memory profiling, peak load testing
- `TestHarmonyMemory.cpp` - Memory usage validation, overhead analysis
- `TestHarmonyUserWorkflow.cpp` - User workflow validation, common progressions, live performance scenarios
- `TestHarmonyRegression.cpp` - Regression testing suite, consistency validation

## Implementation Timeline

**Phase 1 (Core Engine)**: 1 week
**Phase 2 (Data Model)**: 1 week
**Phase 3 (Engine Integration)**: 2 weeks
**Phase 4 (Global UI)**: 1 week
**Phase 5 (Track Config UI)**: 1 week
**Phase 6 (Step Controls)**: 1 week
**Phase 7 (Accumulator Integration)**: 1 week
**Phase 8 (Testing)**: 1 week

**Total Estimated Time**: 9 weeks

## Testing Strategies and Methodologies

### Automated Testing Framework
- **Unit Testing**: Use Google Test framework for C++ unit tests
- **Integration Testing**: Combine multiple components to verify interaction
- **Mock Objects**: Create mock versions of dependent components for isolated testing
- **Continuous Integration**: Integrate tests into CI pipeline for automatic validation

### Performance Testing Methodology
- **Benchmarking Tools**: Use Google Benchmark for performance measurement
- **Profiling**: Regular profiling with tools like perf or Valgrind
- **Memory Analysis**: Use Valgrind, AddressSanitizer for memory leak detection
- **Real-time Constraints**: Verify all operations complete within audio thread requirements

### Edge Case Testing Strategy
- **Fuzz Testing**: Randomized input testing to find unexpected behaviors
- **Boundary Value Analysis**: Test minimum and maximum value ranges
- **Equivalence Partitioning**: Group inputs into equivalent classes for testing
- **Error Condition Testing**: Verify proper error handling and recovery

### Integration Testing Approach
- **API Contract Testing**: Verify interface contracts between components
- **End-to-End Testing**: Complete workflow validation from UI to audio output
- **Regression Testing**: Automated tests to prevent feature breakage
- **Compatibility Testing**: Verify compatibility with existing Performer features

## Risk Mitigation

- **Engine Performance**: Profile early and often, optimize lookup tables
- **Memory Constraints**: Carefully manage lookup table sizes
- **Integration Complexity**: Implement incrementally with frequent testing
- **UI Complexity**: Follow existing Performer patterns to minimize learning curve
- **Testing Coverage**: Maintain >90% code coverage for critical paths
- **Real-time Safety**: Ensure no operations block the audio thread

---

**Document Maintainer**: Onoma2
**Last Updated**: November 19, 2025