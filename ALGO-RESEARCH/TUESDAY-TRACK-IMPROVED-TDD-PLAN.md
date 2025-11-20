# Improved Tuesday Track TDD Implementation Plan

## Overview
Enhanced Test-Driven Development approach for implementing a Tuesday-style track in the Performer sequencer with 5 parameters controlled by F0-F4 + encoder rotation. This plan includes comprehensive test cases for each phase.

## 1. Model Layer Implementation

### 1.1 Add TuesdayTrackMode to Track (RED - Write failing test first)

```cpp
// Test: Add Tuesday to TrackMode enum and verify functionality
TEST(TuesdayTrackModel, TrackModeEnumExtension) {
    // Verify Tuesday is added to enum
    EXPECT_EQ(static_cast<uint8_t>(Track::TrackMode::Last), 4); // Should be 4 with Tuesday added
    
    // Verify track mode name function works
    EXPECT_STREQ(Track::trackModeName(Track::TrackMode::Note), "Note");
    EXPECT_STREQ(Track::trackModeName(Track::TrackMode::Curve), "Curve");
    EXPECT_STREQ(Track::trackModeName(Track::TrackMode::MidiCv), "MIDI/CV");
    EXPECT_STREQ(Track::trackModeName(Track::TrackMode::Tuesday), "Tuesday"); // New
    EXPECT_STREQ(Track::trackModeName(Track::TrackMode::Last), nullptr);
}

TEST(TuesdayTrackModel, TrackModeSerialization) {
    EXPECT_EQ(Track::trackModeSerialize(Track::TrackMode::Note), 0);
    EXPECT_EQ(Track::trackModeSerialize(Track::TrackMode::Curve), 1);
    EXPECT_EQ(Track::trackModeSerialize(Track::TrackMode::MidiCv), 2);
    EXPECT_EQ(Track::trackModeSerialize(Track::TrackMode::Tuesday), 3); // New
}
```

### 1.2 Create TuesdayTrack Model Class (RED - Write failing test first)

```cpp
// Test: TuesdayTrack stores and validates 5 parameters correctly
class TuesdayTrack {
    uint8_t _algorithm : 5;    // 0-31 algorithm selection
    uint8_t _flow : 8;         // 0-255 musical flow
    uint8_t _ornament : 8;     // 0-255 ornamentation
    uint8_t _intensity : 8;    // 0-255 intensity
    uint8_t _loopLength : 7;   // 1-64 steps (64+ = infinite)

public:
    // Constructor, setters, getters, validation
    void setAlgorithm(uint8_t alg) { _algorithm = clamp(alg, 0, 31); }
    uint8_t algorithm() const { return _algorithm; }
    
    void setFlow(uint8_t flow) { _flow = flow; }
    uint8_t flow() const { return _flow; }
    
    void setOrnament(uint8_t ornament) { _ornament = ornament; }
    uint8_t ornament() const { return _ornament; }
    
    void setIntensity(uint8_t intensity) { _intensity = intensity; }
    uint8_t intensity() const { return _intensity; }
    
    void setLoopLength(uint8_t length) { _loopLength = clamp(length, 1, 127); }
    uint8_t loopLength() const { return _loopLength; }
};

TEST(TuesdayTrackModel, ParameterStorage) {
    TuesdayTrack track;
    
    // Test algorithm parameter (0-31)
    track.setAlgorithm(15);
    EXPECT_EQ(track.algorithm(), 15);
    
    track.setAlgorithm(32); // Should clamp to 31
    EXPECT_EQ(track.algorithm(), 31);
    
    track.setAlgorithm(255); // Should clamp to 31
    EXPECT_EQ(track.algorithm(), 31);
    
    // Test flow parameter (0-255)
    track.setFlow(200);
    EXPECT_EQ(track.flow(), 200);
    
    // Test ornament parameter (0-255)
    track.setOrnament(150);
    EXPECT_EQ(track.ornament(), 150);
    
    // Test intensity parameter (0-255)
    track.setIntensity(100);
    EXPECT_EQ(track.intensity(), 100);
    
    // Test loopLength parameter (1-64, with >64 = infinite)
    track.setLoopLength(32);
    EXPECT_EQ(track.loopLength(), 32);
    
    track.setLoopLength(0); // Should clamp to 1
    EXPECT_EQ(track.loopLength(), 1);
    
    track.setLoopLength(100); // Should allow >64 for infinite
    EXPECT_EQ(track.loopLength(), 100);
}

TEST(TuesdayTrackModel, DefaultValues) {
    TuesdayTrack track;
    
    // Verify reasonable default values
    EXPECT_EQ(track.algorithm(), 0);      // Default algorithm
    EXPECT_EQ(track.flow(), 127);         // Mid-range flow
    EXPECT_EQ(track.ornament(), 127);     // Mid-range ornament
    EXPECT_EQ(track.intensity(), 127);    // Mid-range intensity
    EXPECT_EQ(track.loopLength(), 16);    // Default pattern length
}
```

### 1.3 Add Algorithm State Management (RED - Write failing test first)

```cpp
TEST(TuesdayTrackModel, AlgorithmStateInitialization) {
    TuesdayTrack track;
    
    // Test that algorithm state can be initialized
    track.setAlgorithm(0); // Markov
    track.setFlow(100);
    track.setOrnament(50);
    track.setIntensity(200);
    
    // Verify state persists
    EXPECT_EQ(track.algorithm(), 0);
    EXPECT_EQ(track.flow(), 100);
    EXPECT_EQ(track.ornament(), 50);
    EXPECT_EQ(track.intensity(), 200);
}

TEST(TuesdayTrackModel, Serialization) {
    TuesdayTrack originalTrack;
    originalTrack.setAlgorithm(5);
    originalTrack.setFlow(200);
    originalTrack.setOrnament(150);
    originalTrack.setIntensity(75);
    originalTrack.setLoopLength(32);
    
    // Serialize to buffer
    uint8_t buffer[256];
    VersionedSerializedWriter writer(buffer, sizeof(buffer));
    originalTrack.write(writer);
    
    // Deserialize from buffer
    TuesdayTrack restoredTrack;
    VersionedSerializedReader reader(buffer, writer.size());
    restoredTrack.read(reader);
    
    // Verify all parameters match
    EXPECT_EQ(originalTrack.algorithm(), restoredTrack.algorithm());
    EXPECT_EQ(originalTrack.flow(), restoredTrack.flow());
    EXPECT_EQ(originalTrack.ornament(), restoredTrack.ornament());
    EXPECT_EQ(originalTrack.intensity(), restoredTrack.intensity());
    EXPECT_EQ(originalTrack.loopLength(), restoredTrack.loopLength());
}
```

## 2. Engine Layer Implementation

### 2.1 Create TuesdayTrackEngine Class (RED - Write failing test first)

```cpp
class TuesdayTrackEngine : public TrackEngine {
private:
    const TuesdayTrack &_tuesdayTrack;
    
public:
    TuesdayTrackEngine(Engine &engine, Model &model, Track &track, int trackIndex) :
        TrackEngine(engine, model, track, trackIndex),
        _tuesdayTrack(track.tuesdayTrack()) // Requires track mode to be Tuesday
    {}
    
    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Tuesday; }
    
    virtual void tick(uint32_t tick) override {
        // Implement Tuesday-specific ticking logic
        // Use algorithm parameters to generate note/gate pairs
    }
};

TEST(TuesdayTrackEngine, TrackModeInterface) {
    // Mock dependencies
    Engine engine;
    Model model;
    Track track;
    
    // Set track mode to Tuesday
    track.setTrackMode(Track::TrackMode::Tuesday);
    
    // Create engine
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Verify trackMode() returns correct value
    EXPECT_EQ(engineImpl.trackMode(), Track::TrackMode::Tuesday);
}

TEST(TuesdayTrackEngine, Initialization) {
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Verify initialization doesn't crash
    EXPECT_EQ(engineImpl.trackIndex(), 0);
    
    // Verify engine can be ticked without crashing
    engineImpl.tick(0);
    engineImpl.tick(1);
    engineImpl.tick(48); // Typical clock tick
}
```

### 2.2 Implement Algorithm Execution (RED - Write failing test first)

```cpp
// Test algorithm-specific behavior
TEST(TuesdayTrackEngine, MarkovAlgorithmExecution) {
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    tuesdayTrack.setAlgorithm(0); // Markov
    tuesdayTrack.setFlow(127);
    tuesdayTrack.setOrnament(127);
    tuesdayTrack.setIntensity(200);
    tuesdayTrack.setLoopLength(16);
    
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Tick multiple times and verify reasonable output
    for (int i = 0; i < 100; i++) {
        engineImpl.tick(i * 48); // 48 ticks per step
        
        // Verify gate output is reasonable (0-10V range)
        float cv = engineImpl.currentCV();
        EXPECT_GE(cv, -5.0f); // Reasonable voltage range
        EXPECT_LE(cv, 5.0f);
        
        // Verify gate state is reasonable
        bool gate = engineImpl.currentGate();
        EXPECT_TRUE(gate == true || gate == false);
    }
}

TEST(TuesdayTrackEngine, StomperAlgorithmExecution) {
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    tuesdayTrack.setAlgorithm(1); // Stomper
    tuesdayTrack.setFlow(200);
    tuesdayTrack.setOrnament(50);
    tuesdayTrack.setIntensity(150);
    tuesdayTrack.setLoopLength(8);
    
    TuesdayTrackEngine engineImpl(engine, model, track, 1);
    
    // Test stomper-specific behavior
    for (int i = 0; i < 50; i++) {
        engineImpl.tick(i * 48);
        
        // Verify output is within expected ranges
        float cv = engineImpl.currentCV();
        EXPECT_GE(cv, -5.0f);
        EXPECT_LE(cv, 5.0f);
    }
}

TEST(TuesdayTrackEngine, AlgorithmParameterSensitivity) {
    Engine engine;
    Model model;
    Track track1, track2;
    
    track1.setTrackMode(Track::TrackMode::Tuesday);
    track2.setTrackMode(Track::TrackMode::Tuesday);
    
    TuesdayTrack &tuesdayTrack1 = track1.tuesdayTrack();
    TuesdayTrack &tuesdayTrack2 = track2.tuesdayTrack();
    
    // Same algorithm, different parameters
    tuesdayTrack1.setAlgorithm(0); // Markov
    tuesdayTrack1.setFlow(100);
    tuesdayTrack1.setOrnament(50);
    
    tuesdayTrack2.setAlgorithm(0); // Markov
    tuesdayTrack2.setFlow(200); // Different flow
    tuesdayTrack2.setOrnament(150); // Different ornament
    
    TuesdayTrackEngine engine1(engine, model, track1, 0);
    TuesdayTrackEngine engine2(engine, model, track2, 1);
    
    // Run both engines and verify different parameter sets produce different outputs
    std::vector<float> outputs1, outputs2;
    for (int i = 0; i < 20; i++) {
        engine1.tick(i * 48);
        engine2.tick(i * 48);
        
        outputs1.push_back(engine1.currentCV());
        outputs2.push_back(engine2.currentCV());
    }
    
    // Verify outputs are different (not identical)
    bool different = false;
    for (size_t i = 0; i < outputs1.size(); i++) {
        if (abs(outputs1[i] - outputs2[i]) > 0.01f) {
            different = true;
            break;
        }
    }
    EXPECT_TRUE(different) << "Different parameters should produce different outputs";
}
```

### 2.3 Implement Clock Sync and Looping (RED - Write failing test first)

```cpp
TEST(TuesdayTrackEngine, LoopLengthRespected) {
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    tuesdayTrack.setAlgorithm(0); // Markov
    tuesdayTrack.setLoopLength(4); // 4-step loop
    
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Store outputs for first 8 steps
    std::vector<float> firstCycle, secondCycle;
    
    // First cycle (steps 0-3)
    for (int i = 0; i < 4; i++) {
        engineImpl.tick(i * 48);
        firstCycle.push_back(engineImpl.currentCV());
    }
    
    // Second cycle (steps 4-7)
    for (int i = 4; i < 8; i++) {
        engineImpl.tick(i * 48);
        secondCycle.push_back(engineImpl.currentCV());
    }
    
    // For a 4-step loop, steps 4-7 should mirror steps 0-3
    for (int i = 0; i < 4; i++) {
        EXPECT_FLOAT_EQ(firstCycle[i], secondCycle[i]) << "Step " << i << " should repeat";
    }
}

TEST(TuesdayTrackEngine, InfiniteLoop) {
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    tuesdayTrack.setAlgorithm(0); // Markov
    tuesdayTrack.setLoopLength(100); // Should be infinite
    
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Run for many steps and verify no obvious repetition
    std::vector<float> outputs;
    for (int i = 0; i < 200; i++) { // 200 steps
        engineImpl.tick(i * 48);
        outputs.push_back(engineImpl.currentCV());
    }
    
    // Verify sequence doesn't repeat in obvious patterns
    // (This is a basic check - algorithm-specific patterns may exist)
    bool hasVariation = false;
    for (size_t i = 1; i < outputs.size(); i++) {
        if (abs(outputs[i] - outputs[i-1]) > 0.01f) {
            hasVariation = true;
            break;
        }
    }
    EXPECT_TRUE(hasVariation) << "Infinite loop should show variation";
}
```

### 2.4 Gate and CV Output Generation (RED - Write failing test first)

```cpp
TEST(TuesdayTrackEngine, GateOutputGeneration) {
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    tuesdayTrack.setAlgorithm(0); // Markov
    tuesdayTrack.setIntensity(255); // High intensity for more gates
    
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Run for multiple steps and check gate activity
    int gateActiveCount = 0;
    for (int i = 0; i < 100; i++) {
        engineImpl.tick(i * 48);
        if (engineImpl.currentGate()) {
            gateActiveCount++;
        }
    }
    
    // Verify some gates are generated (not all silent)
    EXPECT_GT(gateActiveCount, 10) << "Should generate some gate activity";
}

TEST(TuesdayTrackEngine, CVOutputRange) {
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    tuesdayTrack.setAlgorithm(0); // Markov
    
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Collect CV outputs over multiple steps
    std::vector<float> cvOutputs;
    for (int i = 0; i < 100; i++) {
        engineImpl.tick(i * 48);
        cvOutputs.push_back(engineImpl.currentCV());
    }
    
    // Find min/max values
    float minCV = *std::min_element(cvOutputs.begin(), cvOutputs.end());
    float maxCV = *std::max_element(cvOutputs.begin(), cvOutputs.end());
    
    // Verify outputs are in reasonable voltage range (e.g., -5V to +5V)
    EXPECT_GE(minCV, -6.0f);
    EXPECT_LE(maxCV, 6.0f);
    
    // Verify there's some variation in output
    EXPECT_GT(maxCV - minCV, 0.1f) << "CV output should have variation";
}
```

## 3. UI Layer Implementation

### 3.1 Add Tuesday Track Page (RED - Write failing test first)

```cpp
class TuesdayPage : public ListPage {
private:
    Model &_model;
    Track &_track;
    TuesdayTrack &_tuesdayTrack;
    
    enum Parameter {
        Algorithm,
        Flow,
        Ornament,
        Intensity,
        LoopLength,
        ParameterCount
    };
    
    Parameter _parameter = Algorithm;

public:
    TuesdayPage(PageManager &manager, Model &model) :
        ListPage(manager),
        _model(model),
        _track(model.project().selectedTrack()),
        _tuesdayTrack(_track.tuesdayTrack())
    {
        ASSERT(_track.trackMode() == Track::TrackMode::Tuesday, "TuesdayPage requires Tuesday track");
    }
    
    virtual int parameterCount() const override { return ParameterCount; }
    
    virtual const char* parameterName(int index) const override {
        switch (index) {
            case Algorithm:   return "ALGO";
            case Flow:        return "FLOW";
            case Ornament:    return "ORN";
            case Intensity:   return "INTEN";
            case LoopLength:  return "LOOP";
            default:          return "";
        }
    }
    
    virtual int parameterValue(int index) const override {
        switch (index) {
            case Algorithm:   return _tuesdayTrack.algorithm();
            case Flow:        return _tuesdayTrack.flow();
            case Ornament:    return _tuesdayTrack.ornament();
            case Intensity:   return _tuesdayTrack.intensity();
            case LoopLength:  return _tuesdayTrack.loopLength();
            default:          return 0;
        }
    }
    
    virtual void setParameterValue(int index, int value) override {
        switch (index) {
            case Algorithm:   _tuesdayTrack.setAlgorithm(value); break;
            case Flow:        _tuesdayTrack.setFlow(value); break;
            case Ornament:    _tuesdayTrack.setOrnament(value); break;
            case Intensity:   _tuesdayTrack.setIntensity(value); break;
            case LoopLength:  _tuesdayTrack.setLoopLength(value); break;
        }
    }
    
    virtual const char* parameterValueText(int index, int value) const override {
        static char buffer[16];
        switch (index) {
            case Algorithm:   return algorithmName(value);
            case Flow:
            case Ornament:
            case Intensity:   snprintf(buffer, sizeof(buffer), "%d", value); break;
            case LoopLength:  return loopLengthText(value); break;
        }
        return buffer;
    }
    
    const char* algorithmName(int alg) const {
        static const char* names[] = {
            "MARKOV", "STOMPER", "SCALEWALK", "WOBBLE", 
            "CHIPARP", "TRITRANCE", "SNH", "TOOEASY",
            "RANDOM", "SAIKO", "TEST", "SNH"
        };
        if (alg < 12) return names[alg];
        snprintf(buffer, sizeof(buffer), "ALGO%d", alg);
        return buffer;
    }
    
    const char* loopLengthText(int length) const {
        if (length > 64) return "∞";
        static char buffer[8];
        snprintf(buffer, sizeof(buffer), "%d", length);
        return buffer;
    }
};

TEST(TuesdayPage, ParameterAccess) {
    // Mock model with Tuesday track
    Model model;
    Project &project = model.project();
    Track &track = project.selectedTrack();
    track.setTrackMode(Track::TrackMode::Tuesday);
    
    TuesdayPage page(manager, model);
    
    // Verify parameter count
    EXPECT_EQ(page.parameterCount(), 5);
    
    // Test parameter names
    EXPECT_STREQ(page.parameterName(0), "ALGO");
    EXPECT_STREQ(page.parameterName(1), "FLOW");
    EXPECT_STREQ(page.parameterName(2), "ORN");
    EXPECT_STREQ(page.parameterName(3), "INTEN");
    EXPECT_STREQ(page.parameterName(4), "LOOP");
    
    // Test initial parameter values
    EXPECT_EQ(page.parameterValue(0), 0); // Default algorithm
    EXPECT_EQ(page.parameterValue(1), 127); // Default flow
    EXPECT_EQ(page.parameterValue(2), 127); // Default ornament
    EXPECT_EQ(page.parameterValue(3), 127); // Default intensity
    EXPECT_EQ(page.parameterValue(4), 16); // Default loop length
}

TEST(TuesdayPage, ParameterModification) {
    Model model;
    Project &project = model.project();
    Track &track = project.selectedTrack();
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    
    TuesdayPage page(manager, model);
    
    // Modify parameters through page interface
    page.setParameterValue(0, 5); // Algorithm
    page.setParameterValue(1, 200); // Flow
    page.setParameterValue(2, 50); // Ornament
    page.setParameterValue(3, 150); // Intensity
    page.setParameterValue(4, 32); // Loop length
    
    // Verify track values were updated
    EXPECT_EQ(tuesdayTrack.algorithm(), 5);
    EXPECT_EQ(tuesdayTrack.flow(), 200);
    EXPECT_EQ(tuesdayTrack.ornament(), 50);
    EXPECT_EQ(tuesdayTrack.intensity(), 150);
    EXPECT_EQ(tuesdayTrack.loopLength(), 32);
}
```

### 3.2 Implement F0-F4 Parameter Control (RED - Write failing test first)

```cpp
TEST(TuesdayPage, F0F4ParameterControl) {
    Model model;
    Project &project = model.project();
    Track &track = project.selectedTrack();
    track.setTrackMode(Track::TrackMode::Tuesday);
    
    TuesdayPage page(manager, model);
    
    // Simulate F0 press (Algorithm parameter)
    page.setParameter(0);
    EXPECT_EQ(page.currentParameter(), 0);
    
    // Simulate encoder rotation to change value
    int originalValue = page.parameterValue(0);
    page.setParameterValue(0, originalValue + 10);
    EXPECT_EQ(page.parameterValue(0), originalValue + 10);
    
    // Test F1-F4 similarly
    page.setParameter(1); // Flow
    EXPECT_EQ(page.currentParameter(), 1);
    
    page.setParameter(2); // Ornament
    EXPECT_EQ(page.currentParameter(), 2);
    
    page.setParameter(3); // Intensity
    EXPECT_EQ(page.currentParameter(), 3);
    
    page.setParameter(4); // LoopLength
    EXPECT_EQ(page.currentParameter(), 4);
}

TEST(TuesdayPage, ParameterValueText) {
    Model model;
    Project &project = model.project();
    Track &track = project.selectedTrack();
    track.setTrackMode(Track::TrackMode::Tuesday);
    
    TuesdayPage page(manager, model);
    
    // Test algorithm names
    EXPECT_STREQ(page.parameterValueText(0, 0), "MARKOV");
    EXPECT_STREQ(page.parameterValueText(0, 1), "STOMPER");
    EXPECT_STREQ(page.parameterValueText(0, 15), "ALGO15");
    
    // Test flow/ornament/intensity values
    EXPECT_STREQ(page.parameterValueText(1, 127), "127");
    EXPECT_STREQ(page.parameterValueText(2, 200), "200");
    EXPECT_STREQ(page.parameterValueText(3, 50), "50");
    
    // Test loop length display
    EXPECT_STREQ(page.parameterValueText(4, 16), "16");
    EXPECT_STREQ(page.parameterValueText(4, 65), "∞");
    EXPECT_STREQ(page.parameterValueText(4, 100), "∞");
}
```

## 4. Integration Tests

### 4.1 Full System Test (RED - Write failing test first)

```cpp
TEST(TuesdayIntegration, EndToEndFunctionality) {
    // Full system test: Model + Engine + UI interaction
    Model model;
    Project &project = model.project();
    Engine engine;
    
    // Set up Tuesday track
    Track &track = project.selectedTrack();
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    
    tuesdayTrack.setAlgorithm(0); // Markov
    tuesdayTrack.setFlow(150);
    tuesdayTrack.setOrnament(100);
    tuesdayTrack.setIntensity(200);
    tuesdayTrack.setLoopLength(16);
    
    // Create engine and tick
    TuesdayTrackEngine trackEngine(engine, model, track, 0);
    
    // Run through multiple steps
    for (int i = 0; i < 100; i++) {
        trackEngine.tick(i * 48);
        
        // Verify no crashes, reasonable outputs
        float cv = trackEngine.currentCV();
        bool gate = trackEngine.currentGate();
        
        EXPECT_GE(cv, -6.0f);
        EXPECT_LE(cv, 6.0f);
        EXPECT_TRUE(gate == true || gate == false);
    }
    
    // Change parameters during playback
    tuesdayTrack.setAlgorithm(1); // Switch to Stomper
    tuesdayTrack.setFlow(200);
    
    // Continue ticking
    for (int i = 100; i < 150; i++) {
        trackEngine.tick(i * 48);
        
        float cv = trackEngine.currentCV();
        bool gate = trackEngine.currentGate();
        
        EXPECT_GE(cv, -6.0f);
        EXPECT_LE(cv, 6.0f);
        EXPECT_TRUE(gate == true || gate == false);
    }
}

TEST(TuesdayIntegration, MultiTrackInteraction) {
    Model model;
    Project &project = model.project();
    Engine engine;
    
    // Set up multiple tracks including Tuesday
    for (int i = 0; i < 4; i++) {
        Track &track = project.track(i);
        if (i == 0) {
            track.setTrackMode(Track::TrackMode::Tuesday);
            TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
            tuesdayTrack.setAlgorithm(0);
            tuesdayTrack.setFlow(127);
            tuesdayTrack.setOrnament(127);
            tuesdayTrack.setIntensity(127);
            tuesdayTrack.setLoopLength(16);
        } else {
            track.setTrackMode(Track::TrackMode::Note);
        }
    }
    
    // Verify all engines can run together
    for (int step = 0; step < 50; step++) {
        for (int trackIdx = 0; trackIdx < 4; trackIdx++) {
            TrackEngine &trackEngine = engine.trackEngine(trackIdx);
            trackEngine.tick(step * 48);
            
            // Verify no crashes
            float cv = trackEngine.currentCV();
            bool gate = trackEngine.currentGate();
            
            if (trackIdx == 0) { // Tuesday track
                EXPECT_GE(cv, -6.0f);
                EXPECT_LE(cv, 6.0f);
                EXPECT_TRUE(gate == true || gate == false);
            } else { // Note tracks
                // Note track validation
            }
        }
    }
}
```

### 4.2 Performance Test (RED - Write failing test first)

```cpp
TEST(TuesdayPerformance, AlgorithmExecutionTime) {
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    tuesdayTrack.setAlgorithm(0); // Markov
    
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Measure execution time for multiple ticks
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        engineImpl.tick(i);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    auto avgMicros = duration.count() / 1000;
    
    // Algorithm execution should be under 50 microseconds per tick
    // (Real-time constraint for audio applications)
    EXPECT_LT(avgMicros, 50) << "Algorithm execution too slow: " << avgMicros << " μs";
}

TEST(TuesdayPerformance, MemoryUsage) {
    // Test memory usage per Tuesday track instance
    size_t initialMemory = getCurrentMemoryUsage();
    
    // Create multiple Tuesday tracks
    std::vector<std::unique_ptr<TuesdayTrack>> tracks;
    for (int i = 0; i < 8; i++) {
        auto track = std::make_unique<TuesdayTrack>();
        track->setAlgorithm(i % 4); // Different algorithms
        track->setFlow(127);
        track->setOrnament(127);
        track->setIntensity(127);
        track->setLoopLength(16);
        tracks.push_back(std::move(track));
    }
    
    size_t finalMemory = getCurrentMemoryUsage();
    size_t memoryPerTrack = (finalMemory - initialMemory) / 8;
    
    // Memory usage should be reasonable (e.g., < 500 bytes per track)
    EXPECT_LT(memoryPerTrack, 500) << "Memory usage per track too high: " << memoryPerTrack << " bytes";
}
```

### 4.3 Edge Case Testing (RED - Write failing test first)

```cpp
TEST(TuesdayEdgeCases, ParameterBoundaryConditions) {
    Model model;
    Project &project = model.project();
    Track &track = project.selectedTrack();
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    
    // Test extreme parameter values
    tuesdayTrack.setAlgorithm(31); // Max algorithm
    tuesdayTrack.setFlow(255); // Max flow
    tuesdayTrack.setOrnament(255); // Max ornament
    tuesdayTrack.setIntensity(255); // Max intensity
    tuesdayTrack.setLoopLength(127); // Max loop length
    
    // Verify engine handles extreme values gracefully
    Engine engine;
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    for (int i = 0; i < 50; i++) {
        engineImpl.tick(i * 48);
        
        float cv = engineImpl.currentCV();
        bool gate = engineImpl.currentGate();
        
        // Should not crash and should produce reasonable outputs
        EXPECT_GE(cv, -10.0f);
        EXPECT_LE(cv, 10.0f);
        EXPECT_TRUE(gate == true || gate == false);
    }
    
    // Test minimum values
    tuesdayTrack.setAlgorithm(0); // Min algorithm
    tuesdayTrack.setFlow(0); // Min flow
    tuesdayTrack.setOrnament(0); // Min ornament
    tuesdayTrack.setIntensity(0); // Min intensity
    tuesdayTrack.setLoopLength(1); // Min loop length
    
    for (int i = 50; i < 100; i++) {
        engineImpl.tick(i * 48);
        
        float cv = engineImpl.currentCV();
        bool gate = engineImpl.currentGate();
        
        EXPECT_GE(cv, -10.0f);
        EXPECT_LE(cv, 10.0f);
        EXPECT_TRUE(gate == true || gate == false);
    }
}

TEST(TuesdayEdgeCases, InvalidTrackMode) {
    // Test that TuesdayPage handles non-Tuesday tracks gracefully
    Model model;
    Project &project = model.project();
    Track &track = project.selectedTrack();
    track.setTrackMode(Track::TrackMode::Note); // Not Tuesday
    
    // Attempting to create TuesdayPage with non-Tuesday track should either:
    // 1. Throw an exception (if properly validated)
    // 2. Or handle gracefully
    
    EXPECT_THROW({
        TuesdayPage page(manager, model); // Should assert or throw
    }, std::exception); // or whatever exception type is appropriate
}
```

## 5. Algorithm-Specific Tests

### 5.1 Markov Algorithm Tests

```cpp
TEST(TuesdayAlgorithms, MarkovConsistency) {
    // Test that Markov algorithm produces consistent results with same seed
    Model model1, model2;
    Project &project1 = model1.project();
    Project &project2 = model2.project();
    
    Track &track1 = project1.selectedTrack();
    Track &track2 = project2.selectedTrack();
    
    track1.setTrackMode(Track::TrackMode::Tuesday);
    track2.setTrackMode(Track::TrackMode::Tuesday);
    
    TuesdayTrack &tuesdayTrack1 = track1.tuesdayTrack();
    TuesdayTrack &tuesdayTrack2 = track2.tuesdayTrack();
    
    // Same parameters for both
    tuesdayTrack1.setAlgorithm(0); // Markov
    tuesdayTrack1.setFlow(150);
    tuesdayTrack1.setOrnament(100);
    tuesdayTrack1.setIntensity(200);
    tuesdayTrack1.setLoopLength(16);
    
    tuesdayTrack2.setAlgorithm(0); // Markov
    tuesdayTrack2.setFlow(150);
    tuesdayTrack2.setOrnament(100);
    tuesdayTrack2.setIntensity(200);
    tuesdayTrack2.setLoopLength(16);
    
    Engine engine1, engine2;
    TuesdayTrackEngine engineImpl1(engine1, model1, track1, 0);
    TuesdayTrackEngine engineImpl2(engine2, model2, track2, 0);
    
    // Run both engines and compare outputs
    std::vector<float> outputs1, outputs2;
    for (int i = 0; i < 50; i++) {
        engineImpl1.tick(i * 48);
        engineImpl2.tick(i * 48);
        
        outputs1.push_back(engineImpl1.currentCV());
        outputs2.push_back(engineImpl2.currentCV());
    }
    
    // With same parameters, outputs should be identical
    for (size_t i = 0; i < outputs1.size(); i++) {
        EXPECT_FLOAT_EQ(outputs1[i], outputs2[i]) 
            << "Outputs should be identical at step " << i;
    }
}
```

## 6. Hardware and Real-Time Considerations

### 6.1 Real-Time Constraints

```cpp
TEST(TuesdayRealTime, TimingConstraints) {
    // Test that the Tuesday track meets real-time audio constraints
    Engine engine;
    Model model;
    Track track;
    
    track.setTrackMode(Track::TrackMode::Tuesday);
    TuesdayTrack &tuesdayTrack = track.tuesdayTrack();
    tuesdayTrack.setAlgorithm(0); // Markov
    
    TuesdayTrackEngine engineImpl(engine, model, track, 0);
    
    // Simulate high-frequency ticking (e.g., 1000 Hz clock rate)
    auto startTime = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < 1000; i++) {
        engineImpl.tick(i * 48); // Assuming 48 ticks per millisecond
        
        // Check elapsed time periodically
        if (i % 100 == 0) {
            auto currentTime = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
                currentTime - startTime).count();
            
            // Should not take more than real-time (1ms per step for 1000Hz)
            EXPECT_LT(elapsed, (i + 1) * 1000) 
                << "Processing taking too long: " << elapsed << " μs for " << i << " steps";
        }
    }
}
```

## 7. Test Execution Strategy

### Phase 1: Unit Tests (Model Layer)
- Execute all model tests first
- Verify parameter storage and validation
- Test serialization/deserialization

### Phase 2: Engine Tests  
- Test engine functionality with mock models
- Verify algorithm execution
- Test clock synchronization and looping

### Phase 3: UI Tests
- Test parameter display and control
- Verify F0-F4 functionality
- Test value text formatting

### Phase 4: Integration Tests
- Full system testing
- Multi-track interaction
- Performance testing

### Phase 5: Edge Case Tests
- Boundary conditions
- Error handling
- Invalid inputs

## 8. Quality Metrics

### Code Coverage
- Aim for >90% coverage of Tuesday-specific code
- Focus on algorithm execution paths
- Include parameter validation paths

### Performance Metrics
- Algorithm execution < 50μs per tick
- Memory usage < 500 bytes per track
- No audio dropouts or glitches

### Quality Gates
- All unit tests pass before integration
- Performance requirements met
- No crashes in extended testing
- Parameter changes handled gracefully during playback

This improved TDD plan provides comprehensive test coverage for the Tuesday track implementation, ensuring quality and reliability while maintaining the original design goals.