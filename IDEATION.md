# IDEATION.md - PEW|FORMER Feature Ideas & Feasibility Analysis

This document explores potential feature enhancements for the PEW|FORMER firmware, with technical feasibility assessments based on the current architecture.

## Table of Contents
1. [CV-Based Clock Control](#cv-based-clock-control)
2. [Per-Track Play/Pause](#per-track-play-pause)
3. [Extended Ratcheting](#extended-ratcheting)
4. [Pulse Count System](#pulse-count-system)

---

## CV-Based Clock Control

### Original Proposal: "Analog Style" External Clock Per Track

**Concept:**
- Add a `ClockSource` enum to each track (INTERNAL, EXTERNAL_CV_A, etc.)
- Implement rising-edge detection on CV inputs (running faster than musical clock)
- When edge detected on CV Input A, loop through all 8 tracks
- For tracks with `_clockSource` set to `EXTERNAL_CV_A`, call their `processTick()` function

**Musical Goal:**
Enable each track to run at independent tempos/rhythms driven by external CV signals, creating complex polyrhythmic textures similar to analog modular sequencers.

### Technical Analysis

#### Current Architecture Findings

**Existing Clock Infrastructure:**
- `Clock` class (`src/apps/sequencer/engine/Clock.h`) already supports slave sources
- 4 slave inputs available via `slaveTick()` method
- Clock modes: Auto, Master, Slave (global for all tracks)
- High-precision timing via `ClockTimer` (CONFIG_PPQN = 192)

**CV Input System:**
- `CvInput` class reads 4 CV inputs as float values (0-10V)
- Updated in `Engine::update()` at engine task rate
- No edge detection currently implemented
- Simple voltage reading: `float channel(int index)`

**Track Timing Model:**
```cpp
// Current: All tracks share global tick
for (auto &trackEngine : _trackEngines) {
    trackEngine->tick(tick);  // Same tick for all!
}
```
- Each `NoteTrackEngine::tick()` receives global tick value
- Track divisor applied: `tick % divisor == 0`
- Play modes: Aligned (sync to global) or Free (internal counter)

#### Critical Issues with Original Proposal

**1. Architectural Conflict** ⚠️
- Current design: Unified global clock → all tracks advance together
- Proposed design: Per-track clocks → tracks advance independently
- **Impact:** Breaks fundamental synchronization model
- Track linking, pattern sync, and live performance features depend on shared timing

**2. Missing Infrastructure** ⚠️
```cpp
// Would need to add to CvInput.h:
class CvInput {
private:
    float _prevChannels[4];      // Previous sample values
    bool  _risingEdge[4];         // Edge detection flags
    float _thresholdHigh = 2.5f;  // Schmitt trigger high
    float _thresholdLow = 2.0f;   // Schmitt trigger low (hysteresis)

public:
    bool checkRisingEdge(int channel);  // Returns true once per edge
};
```
- CV inputs provide continuous voltage, not discrete events
- Need edge detection with hysteresis to avoid noise-triggered false positives
- Eurorack CV can be noisy; simple threshold crossing insufficient

**3. Task Timing Violations** ⚠️
Current execution order:
```
Driver Task (priority 5) → Hardware I/O
    ↓
Engine Task (priority 4) → Clock tick → Track ticks → CV read
    ↓
UI Task (priority 2) → Display update
```

Proposed execution order would be:
```
Engine Task → CV read → Edge detect → Some tracks tick
    ↓
             → Clock tick → Other tracks tick
```

**Problems:**
- Tracks tick out of order (CV-driven before clock-driven)
- State inconsistency: `_sequenceState` accessed by different timing sources
- Potential race conditions without extensive locking
- FreeRTOS task priorities designed for current flow

**4. Thread Safety** ⚠️
```cpp
// NoteTrackEngine state not designed for multiple tick sources:
int _currentStep;           // Which step is playing?
bool _gateOutput;           // Current gate state
SequenceState _sequenceState;  // Step position, iteration count
```
- Calling `tick()` from CV interrupt + Clock callback = race conditions
- Would need mutex locks → performance degradation on STM32F4
- Engine timing budget: ~2ms per update cycle (task priority 4, 4096 stack)

**5. Memory Cost** ⚠️
Per-track additions needed:
```cpp
class NoteTrackEngine {
    ClockSource _clockSource;    // 1 byte (enum)
    uint32_t _trackTick;         // 4 bytes (independent tick counter)
    bool _lastCvState;           // 1 byte (edge detection)
    // Total: ~6 bytes × 8 tracks = 48 bytes
};

class CvInput {
    float _prevChannels[4];      // 16 bytes
    bool _risingEdge[4];         // 4 bytes
    // Total: 20 bytes
};
```
- Not huge, but adds complexity to already tight memory budget
- STM32F4 has 192KB RAM; every byte counts in embedded systems

### Feasibility Assessment: ⚠️ **NOT RECOMMENDED**

**Overall Rating:** 2/10 (High effort, breaks architecture, better alternatives exist)

**Reasoning:**
1. Requires major refactoring of core Engine timing model
2. Breaks existing track linking and synchronization features
3. Thread safety concerns require extensive locking
4. Missing edge detection infrastructure
5. Better alternatives achieve similar musical results with less risk

---

## Alternative Approaches: CV Clock Control

### Alternative 1: Global CV Clock Mode ⭐⭐⭐⭐⭐

**Feasibility:** 9/10 (Easy implementation, uses existing infrastructure)

**Concept:**
Add CV input as a global clock source option (alongside existing MIDI/External clock).

**Implementation:**
```cpp
// In ClockSetup.h - add to existing slave sources
enum class SlaveSource {
    External,  // Existing: CLK IN jack
    Midi,      // Existing: MIDI clock
    UsbMidi,   // Existing: USB MIDI clock
    Cv1,       // NEW: CV Input 1 as clock
    Cv2,       // NEW: CV Input 2 as clock
    Cv3,       // NEW: CV Input 3 as clock
    Cv4,       // NEW: CV Input 4 as clock
};

// In CvInput.cpp - add edge detection
void CvInput::update() {
    for (int i = 0; i < Channels; ++i) {
        float prevValue = _channels[i];
        _channels[i] = _adc.channel(i).voltage();

        // Rising edge detection with hysteresis
        if (prevValue < 2.0f && _channels[i] > 2.5f) {
            _risingEdge[i] = true;
        }
    }
}

// In Engine.cpp - check for CV clock edges
void Engine::update() {
    _cvInput.update();

    // Check if any CV input is configured as clock source
    if (_model.settings().clockSetup().slaveSource() == SlaveSource::Cv1) {
        if (_cvInput.checkRisingEdge(0)) {
            _clock.slaveTick(0);  // Use existing slave infrastructure!
        }
    }
    // ... rest of engine update
}
```

**Advantages:**
- ✅ Uses proven `Clock::slaveTick()` infrastructure
- ✅ All tracks stay synchronized
- ✅ Minimal code changes (~100 lines total)
- ✅ No threading issues
- ✅ Can switch between CV/MIDI/Internal clock seamlessly
- ✅ Standard eurorack workflow (CV clock is common)

**Disadvantages:**
- ❌ Not per-track (all tracks share the CV clock)
- ❌ Uses one of 4 CV inputs

**Effort Estimate:** 1-2 days (including simulator testing)

**Files to Modify:**
- `src/apps/sequencer/engine/CvInput.h/cpp` - Edge detection
- `src/apps/sequencer/model/ClockSetup.h` - Add CV sources
- `src/apps/sequencer/engine/Engine.cpp` - Wire CV edges to clock
- `src/apps/sequencer/ui/pages/ClockSetupPage.cpp` - UI for CV clock selection

---

### Alternative 2: CV-Controlled Track Divisor ⭐⭐⭐⭐⭐

**Feasibility:** 8/10 (Moderate effort, extends existing routing system)

**Concept:**
Use CV voltage to dynamically control each track's clock divisor, creating tempo relationships without breaking synchronization.

**Implementation:**
```cpp
// In Routing.h - add new routing target
enum class Target {
    // Existing targets...
    TrackDivisor1,  // NEW: Control track 1 divisor via CV
    TrackDivisor2,
    // ... tracks 3-8
};

// In NoteTrackEngine.cpp - apply CV to divisor
uint32_t NoteTrackEngine::getCurrentDivisor() {
    uint32_t baseDivisor = _sequence->divisor();

    // Check if CV is routed to track divisor
    float cvValue = _routingEngine.getTrackDivisorModulation(_trackIndex);

    if (cvValue != 0.0f) {
        // Map CV (0-10V) to divisor multipliers
        // 0V = 1/4x, 5V = 1x, 10V = 4x
        float multiplier = 0.25f + (cvValue * 0.375f);
        baseDivisor = uint32_t(baseDivisor * multiplier);
    }

    return baseDivisor;
}
```

**CV Voltage Mapping:**
```
0.0V  →  ×1/16  (very slow)
2.5V  →  ×1/4
5.0V  →  ×1     (normal speed)
7.5V  →  ×4
10.0V →  ×16    (very fast)
```

**Advantages:**
- ✅ Per-track control (each track can have different CV input)
- ✅ Tracks remain synchronized to master clock
- ✅ Uses existing RoutingEngine infrastructure
- ✅ Continuous control (not just discrete steps)
- ✅ Can modulate divisor with LFOs, envelopes, sequencer CVs
- ✅ Maintains pattern relationships

**Disadvantages:**
- ❌ Not true independent clock (still derived from master)
- ❌ Divisor changes are quantized to nearest clock tick
- ❌ Requires routing configuration (more complex UI)

**Effort Estimate:** 2-3 days

**Musical Use Cases:**
- LFO modulating track speed for evolving polyrhythms
- Pressure/expression controlling one track's tempo
- Sequencer CV creating programmed tempo changes
- Performance control of relative track speeds

**Files to Modify:**
- `src/apps/sequencer/engine/RoutingEngine.h/cpp` - Add divisor targets
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Apply CV modulation
- `src/apps/sequencer/ui/pages/RoutingPage.cpp` - UI for routing setup

---

### Alternative 3: Per-Track Clock Enable/Disable ⭐⭐⭐⭐

**Feasibility:** 7/10 (Moderate effort, architectural fit)

**Concept:**
CV gate inputs enable/disable individual tracks, creating on-the-fly muting/unmuting without breaking sync.

**Implementation:**
```cpp
// In NoteTrack.h - add clock enable state
class NoteTrack {
public:
    enum class ClockEnable {
        Always,      // Track always runs
        CvGate1,     // Track runs when CV1 > threshold
        CvGate2,     // Track runs when CV2 > threshold
        CvGate3,     // Track runs when CV3 > threshold
        CvGate4,     // Track runs when CV4 > threshold
    };

private:
    ClockEnable _clockEnable = ClockEnable::Always;
};

// In NoteTrackEngine.cpp - check gate state before ticking
TrackEngine::TickResult NoteTrackEngine::tick(uint32_t tick) {
    // Check if track clock is enabled
    if (!isClockEnabled()) {
        return TickResult::NoUpdate;  // Skip this tick
    }

    // ... existing tick logic
}

bool NoteTrackEngine::isClockEnabled() const {
    switch (_noteTrack.clockEnable()) {
        case ClockEnable::Always:
            return true;
        case ClockEnable::CvGate1:
            return _engine.cvInput().channel(0) > 2.5f;
        case ClockEnable::CvGate2:
            return _engine.cvInput().channel(1) > 2.5f;
        // ... etc
    }
}
```

**Advantages:**
- ✅ Per-track control
- ✅ Tracks stay in sync when running (just pause/resume)
- ✅ Simple gate logic (no edge detection needed)
- ✅ Useful for live performance (CV gate = track enable)
- ✅ Can create polyrhythmic patterns by enabling tracks at different times
- ✅ Maintains sequence position when paused

**Disadvantages:**
- ❌ Not a "clock source" (just an enable gate)
- ❌ Track doesn't advance while disabled
- ❌ Different from traditional modular clock behavior

**Effort Estimate:** 3-4 days

**Musical Use Cases:**
- LFO gating tracks for stuttering rhythms
- Manual gates for bringing tracks in/out
- Sequencer gates creating programmed track muting
- Probability/randomness in track activation

**Files to Modify:**
- `src/apps/sequencer/model/NoteTrack.h` - Add clock enable enum
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` - Check enable state
- `src/apps/sequencer/ui/pages/TrackPage.cpp` - UI for enable selection

---

### Alternative 4: CV-Triggered Pattern Changes ⭐⭐⭐

**Feasibility:** 6/10 (Moderate effort, new feature area)

**Concept:**
CV edges trigger pattern/sequence changes per track, creating dynamic arrangement control.

**Implementation:**
```cpp
// In NoteTrack.h
class NoteTrack {
public:
    enum class PatternTrigger {
        Manual,      // User selects pattern
        CvEdge1,     // CV1 rising edge advances pattern
        CvEdge2,     // CV2 rising edge advances pattern
        CvEdge3,     // CV3 rising edge advances pattern
        CvEdge4,     // CV4 rising edge advances pattern
    };
};

// In NoteTrackEngine.cpp
void NoteTrackEngine::checkPatternTrigger() {
    auto trigger = _noteTrack.patternTrigger();

    if (trigger >= PatternTrigger::CvEdge1 && trigger <= PatternTrigger::CvEdge4) {
        int cvChannel = int(trigger) - int(PatternTrigger::CvEdge1);

        if (_engine.cvInput().checkRisingEdge(cvChannel)) {
            // Advance to next pattern in chain
            advancePattern();
        }
    }
}
```

**Advantages:**
- ✅ Per-track control
- ✅ Doesn't affect timing/sync
- ✅ Enables complex arrangements via CV
- ✅ Works well with existing pattern chain system

**Disadvantages:**
- ❌ Not related to tempo/clock
- ❌ Requires edge detection implementation
- ❌ Different use case than original proposal

**Effort Estimate:** 4-5 days

---

## Recommended Implementation Path

Based on feasibility analysis and musical utility:

### Phase 1: Foundation (Week 1)
**Global CV Clock Mode** - Alternative 1
- Lowest risk, highest value for common use case
- Provides CV clock functionality immediately
- Tests edge detection infrastructure for later features

### Phase 2: Expressive Control (Week 2-3)
**CV-Controlled Track Divisor** - Alternative 2
- Builds on existing routing system
- Provides per-track tempo variation
- Maintains synchronization benefits

### Phase 3: Performance Features (Week 4)
**Per-Track Clock Enable** - Alternative 3
- Adds live performance capability
- Uses gate detection (simpler than edges)
- Enables polyrhythmic composition techniques

### Phase 4: Advanced Features (Future)
**CV-Triggered Pattern Changes** - Alternative 4
- Different use case, but valuable
- Enables complex arrangement control
- Can be combined with other CV features

---

## Per-Track Play/Pause

### Concept
Independent play/pause control for each of the 8 tracks, allowing selective track activation.

### Current State
- Global play/pause via `Engine::clockStart()` / `clockStop()`
- Track muting exists but doesn't stop sequence advancement
- Fill mode temporarily unmutes tracks

### Proposed Implementation
```cpp
// In NoteTrack.h
class NoteTrack {
private:
    bool _isRunning = true;
};

// In NoteTrackEngine.cpp
TrackEngine::TickResult NoteTrackEngine::tick(uint32_t tick) {
    if (!_noteTrack.isRunning()) {
        return TickResult::NoUpdate;
    }
    // ... existing logic
}
```

### Feasibility: ⭐⭐⭐⭐ (8/10)

**Advantages:**
- ✅ Simple to implement
- ✅ Doesn't break synchronization
- ✅ Useful for composition workflow
- ✅ Maintains sequence position when paused

**Disadvantages:**
- ❌ UI challenge: How to toggle? Long-press track button already used
- ❌ Need clear visual feedback (LED state)
- ❌ Different from mute (mute continues sequence, pause stops it)

**Effort Estimate:** 2-3 days

**UI Options:**
1. SHIFT + Track button = Toggle play/pause
2. Dedicated page for track states
3. New function key when track is selected

---

## Extended Ratcheting

### Concept
Extend the existing `Retrigger` functionality with new modes:
- **MULTIPLY**: Current behavior (subdivide steps)
- **PULSE**: Fire N triggers within gate time
- **GATED**: Continuous triggers while gate is high

### Current State
- `Retrigger` layer in `NoteSequence::Step` (0-3 range = 1-4 triggers)
- `RetriggerProbability` for randomization
- Basic subdivision behavior implemented

### Proposed Enhancement
```cpp
// In NoteSequence.h
enum class RatchetMode {
    Multiply,  // Current: subdivide step evenly
    Pulse,     // Fire N pulses at step start
    Gated,     // Continuous triggers during gate
};

// Per-track or per-step setting?
class NoteSequence {
    RatchetMode _ratchetMode = RatchetMode::Multiply;
};
```

### Feasibility: ⭐⭐⭐⭐ (7/10)

**Advantages:**
- ✅ Builds on existing feature
- ✅ Clear musical utility
- ✅ Contained to NoteTrackEngine
- ✅ No architectural changes needed

**Disadvantages:**
- ❌ Need to decide: per-track or per-step?
- ❌ UI complexity (more parameters to edit)
- ❌ Pulse/Gated modes need gate timing logic

**Effort Estimate:** 3-5 days

**Implementation Notes:**
- Keep existing Retrigger as base
- Add mode selector to track or sequence
- Modify `triggerStep()` logic based on mode
- Test timing accuracy on hardware

---

## Pulse Count System

### Original Proposal
Replace "one step per tick" with a pulse counting system:
- Add `pulseCount` to step structure
- Add `_pulseCounter` to track
- Decrement counter instead of advancing on every tick

### Feasibility: ⭐⭐ (3/10 - Major refactor, unclear benefit)

**Analysis:**
The current system already has fine-grained timing control:
- CONFIG_PPQN = 192 (192 pulses per quarter note)
- CONFIG_SEQUENCE_PPQN = 48 (48 pulses per step)
- Divisor system allows flexible timing

**What problem does pulse count solve?**
- Unclear improvement over existing divisor system
- Current architecture already "counts" (via tick % divisor)
- Would require rewriting core timing logic

**Recommendation:** ❌ Not recommended unless specific use case identified

**Alternative:**
If the goal is variable step lengths, consider:
- Per-step duration multiplier (use existing Length parameter)
- Pattern-level timing modifications
- Groove templates (already partially implemented)

---

## Summary: Feasibility Ratings

| Feature | Feasibility | Effort | Value | Recommend |
|---------|------------|--------|-------|-----------|
| **Global CV Clock** | 9/10 | 1-2 days | High | ✅ Yes - Phase 1 |
| **CV Track Divisor** | 8/10 | 2-3 days | High | ✅ Yes - Phase 2 |
| **Track Clock Enable** | 7/10 | 3-4 days | Medium | ✅ Yes - Phase 3 |
| **CV Pattern Trigger** | 6/10 | 4-5 days | Medium | ⚠️ Maybe - Phase 4 |
| **Per-Track Play/Pause** | 8/10 | 2-3 days | Medium | ✅ Yes - Phase 2 |
| **Extended Ratcheting** | 7/10 | 3-5 days | Medium | ✅ Yes - Phase 3 |
| **Per-Track CV Clock** | 2/10 | 8+ days | Low | ❌ No - Use alternatives |
| **Pulse Count System** | 3/10 | 8+ days | Low | ❌ No - Unclear benefit |

---

## Development Priorities

### Immediate Value (Implement First)
1. **Global CV Clock Mode** - Standard eurorack feature, easy win
2. **Per-Track Play/Pause** - Workflow improvement, simple implementation

### High Value (Implement Next)
3. **CV-Controlled Track Divisor** - Unique performance feature
4. **Extended Ratcheting** - Builds on existing, clear musical utility

### Future Consideration
5. **Track Clock Enable** - Performance tool, moderate complexity
6. **CV Pattern Trigger** - Advanced feature, needs edge detection

### Not Recommended
7. **Per-Track CV Clock** - Use alternatives instead (divisor control, clock enable)
8. **Pulse Count System** - No clear advantage over current architecture

---

## Testing Strategy

For all CV-related features:

### Simulator Testing
1. Implement basic functionality
2. Test edge cases (0V, 10V, rapid changes)
3. Verify timing accuracy
4. Check CPU usage in profiler

### Hardware Testing (Critical for CV features!)
1. Test with actual CV sources (LFOs, sequencers, keyboards)
2. Verify edge detection threshold and hysteresis
3. Check for false triggers from noise
4. Test with different slew rates (slow vs. fast rising edges)
5. Validate against oscilloscope readings
6. Test all 4 CV inputs independently
7. Stress test: All tracks using CV control simultaneously

### Eurorack Integration Testing
- CV/Gate standards compliance (0-10V, 5V gates)
- Timing accuracy vs. external clock sources
- Behavior with different sequencer brands
- Clock drift over long sessions
- Start/stop/reset synchronization

---

## Notes on Eurorack CV Standards

When implementing CV features, respect these conventions:

**Voltage Ranges:**
- CV: 0-10V (some systems use -5V to +5V)
- Gate: 0V (low) to 5V+ (high)
- Trigger: Brief pulse (1-15ms typical)

**Timing:**
- Clock: Rising edge is the event
- Minimum pulse width: 1ms (some modules need 10ms)
- Hysteresis: 0.5-1.0V typical to avoid noise triggers

**Thresholds (Schmitt Trigger):**
```cpp
const float GATE_THRESHOLD_HIGH = 2.5f;  // Gate goes high
const float GATE_THRESHOLD_LOW = 2.0f;   // Gate goes low (hysteresis)
```

**Edge Detection:**
```cpp
bool rising = (prev < GATE_THRESHOLD_LOW) && (curr > GATE_THRESHOLD_HIGH);
bool falling = (prev > GATE_THRESHOLD_HIGH) && (curr < GATE_THRESHOLD_LOW);
```

---

## Conclusion

The original proposal for per-track CV clocks is **not recommended** due to architectural conflicts and the existence of better alternatives. Instead, implement CV clock control through:

1. **Global CV Clock** (immediate value, proven architecture)
2. **CV Track Divisor Modulation** (per-track control, maintains sync)
3. **Track Clock Enable** (performance tool, simpler than per-track clock)

These alternatives provide the desired musical functionality while respecting the existing architecture and minimizing implementation risk.

---

*Last Updated: 2025-11-17*
*Author: Claude (Technical Analysis)*
*Status: Draft - Pending Review*
