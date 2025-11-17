# FEATURE_EVALUATION.md - Comprehensive Analysis of All Proposed Features

**Last Updated:** 2025-11-17
**Status:** Complete Analysis Based on Codebase Examination

This document provides a comprehensive evaluation of ALL features discussed for PEW|FORMER firmware enhancement, including current implementation status, technical feasibility, and prioritized recommendations.

---

## Table of Contents
1. [Already Implemented Features](#already-implemented-features)
2. [Proposed New Features](#proposed-new-features)
3. [Critical Discoveries](#critical-discoveries)
4. [Implementation Priority Matrix](#implementation-priority-matrix)
5. [Detailed Feature Analysis](#detailed-feature-analysis)

---

## Already Implemented Features

### ✅ 1. Accumulator System
**Status:** FULLY IMPLEMENTED AND DEPLOYED

**Current Implementation:**
- **Location:** `src/apps/sequencer/model/Accumulator.h/cpp`
- **Integration:** `NoteSequence`, `NoteTrackEngine`, `AccumulatorPage`, `AccumulatorStepsPage`
- **Documentation:** Complete in `QWEN.md` and `CLAUDE.md`

**Capabilities:**
- 4 operational modes: Wrap, Pendulum, Random, Hold
- Per-step trigger configuration (16 steps)
- Modulates note pitch in real-time
- Direction control: Up, Down, Freeze
- Value range: -100 to +100
- Step size: 1-100
- Polarity: Unipolar/Bipolar
- Mode: Stage/Track level

**Testing:** ✅ Unit tests pass, hardware verified, production ready

**UI Access:**
- ACCUM page: Parameter editing
- ACCST page: Per-step trigger configuration
- Note button (F3) cycling: Includes AccumulatorTrigger layer
- Sequence key cycling: NoteSequence → Accumulator → AccumulatorSteps

**Performance:** Minimal CPU overhead, no memory concerns

---

### ✅ 2. CV-Controlled Track Divisor (ALREADY ROUTABLE!)
**Status:** INFRASTRUCTURE COMPLETE, NEEDS DOCUMENTATION

**Discovery:** The routing system already supports CV → Divisor modulation!

**Current Implementation:**
```cpp
// From Routing.h line 72
enum class Target {
    Divisor,  // ← Already exists!
    // ... other targets
};

// From NoteSequence.h lines 302-325
int divisor() const {
    return _divisor.get(isRouted(Routing::Target::Divisor));
}
```

**What This Means:**
- ✅ CV inputs (1-4) can already be routed to track divisor
- ✅ Real-time tempo modulation per track ALREADY WORKS
- ✅ Divisor range: 1-768 (1/64T to 4 bars)
- ✅ Continuous CV control (0-10V maps to divisor range)

**What's Missing:**
- ❌ User documentation (not obvious this feature exists)
- ❌ UI preset templates for common divisor modulation patterns
- ❌ Visual feedback of CV-modulated divisor value

**Effort to Expose Feature:** 1 day (documentation + UI enhancements)

**Musical Value:** ⭐⭐⭐⭐⭐ (High - eurorack polyrhythm control)

**Recommendation:** Document and promote this feature! It's already production-ready.

---

### ✅ 3. Comprehensive Routing System
**Status:** FULLY IMPLEMENTED

**Routing Targets (42+ total):**

**Engine Controls:**
- Play, PlayToggle, Record, RecordToggle, TapTempo

**Project-Level:**
- Tempo, Swing

**PlayState (Per-Track):**
- Mute, Fill, FillAmount, Pattern

**Track Parameters (Per-Track):**
- SlideTime, Octave, Transpose, Offset, Rotate
- GateProbabilityBias, RetriggerProbabilityBias
- LengthBias, NoteProbabilityBias
- ShapeProbabilityBias (for curve tracks)

**Sequence Parameters (Per-Sequence):**
- FirstStep, LastStep, RunMode, **Divisor**
- Scale, RootNote

**Routing Sources:**
- CV In 1-4
- CV Out 1-8 (feedback routing)
- Gate Out 1-8
- MIDI notes, CC, velocity, aftertouch, etc.

**What This Means:**
Most of the "CV control" features we discussed are ALREADY POSSIBLE through routing!

---

## Proposed New Features

### 🟡 4. Global CV Clock Mode
**Status:** NOT IMPLEMENTED, HIGH FEASIBILITY

**Original Proposal:** Use CV input as master clock source for all tracks

**Current State:**
- Clock system has 4 slave inputs (`Clock::slaveTick()`)
- Currently used for: External clock jack, MIDI clock, USB MIDI clock
- CV inputs read continuously in `Engine::update()`
- **Missing:** Edge detection on CV inputs

**Implementation Requirements:**

**Step 1: Add Edge Detection to CvInput** (½ day)
```cpp
// In CvInput.h
class CvInput {
private:
    std::array<float, Channels> _prevChannels;
    std::array<bool, Channels> _risingEdge;
    float _thresholdHigh = 2.5f;
    float _thresholdLow = 2.0f;  // Hysteresis

public:
    bool checkRisingEdge(int channel);
    void clearRisingEdge(int channel);
};
```

**Step 2: Add CV Sources to ClockSetup** (½ day)
```cpp
// In ClockSetup.h
enum class MidiClockSource {
    UsbMidi,
    Midi,
    Cv1,  // NEW
    Cv2,  // NEW
    Cv3,  // NEW
    Cv4,  // NEW
};
```

**Step 3: Wire CV Edges to Clock** (½ day)
```cpp
// In Engine::update()
if (clockSetup.midiClockSource() == ClockSetup::MidiClockSource::Cv1) {
    if (_cvInput.checkRisingEdge(0)) {
        _clock.slaveTick(0);
        _cvInput.clearRisingEdge(0);
    }
}
```

**Step 4: UI Integration** (½ day)
- Update ClockSetupPage to show CV clock options
- Add visual feedback (LED) when CV clock is active

**Feasibility:** ⭐⭐⭐⭐⭐ (9/10)
**Effort:** 2 days (including testing)
**Musical Value:** ⭐⭐⭐⭐⭐ (High - standard eurorack feature)
**Risk:** Low (uses existing slave clock infrastructure)

**Files to Modify:**
- `src/apps/sequencer/engine/CvInput.h/cpp`
- `src/apps/sequencer/model/ClockSetup.h`
- `src/apps/sequencer/engine/Engine.cpp`
- `src/apps/sequencer/ui/pages/ClockSetupPage.cpp`

**Dependencies:** None

**Testing:**
- Simulator: Test edge detection with rapid CV changes
- Hardware: Test with actual LFOs, sequencers, clock sources
- Verify no false triggers from noise
- Check timing accuracy with oscilloscope

---

### 🟡 5. Per-Track Play/Pause
**Status:** NOT IMPLEMENTED, HIGH FEASIBILITY

**Current State:**
- Global play/stop via `Engine::clockStart()` / `clockStop()`
- Track muting exists but doesn't stop sequence advancement
- Fill system can override mute

**Proposed Implementation:**
```cpp
// In NoteTrack.h
class NoteTrack {
private:
    bool _paused = false;  // Track-specific pause state

public:
    bool paused() const { return _paused; }
    void setPaused(bool paused) { _paused = paused; }
    void togglePaused() { _paused = !_paused; }
};

// In NoteTrackEngine::tick()
TrackEngine::TickResult NoteTrackEngine::tick(uint32_t tick) {
    if (_noteTrack.paused()) {
        return TickResult::NoUpdate;  // Skip tick processing
    }
    // ... existing logic
}
```

**Difference from Mute:**
- **Mute:** Silences output, sequence continues advancing
- **Pause:** Stops sequence advancement, maintains current position

**UI Options:**
1. **SHIFT + Track Button:** Toggle pause (recommended)
2. **Long press Track Button:** Toggle pause (conflicts with existing features)
3. **Dedicated Track Status Page:** View/edit all track states

**LED Feedback:**
- Paused track: Dim LED
- Playing track: Bright LED
- Muted track: Different color/blink pattern

**Feasibility:** ⭐⭐⭐⭐ (8/10)
**Effort:** 2-3 days
**Musical Value:** ⭐⭐⭐⭐ (Medium-High - useful for composition workflow)
**Risk:** Low (simple state check)

**Files to Modify:**
- `src/apps/sequencer/model/NoteTrack.h`
- `src/apps/sequencer/engine/NoteTrackEngine.cpp`
- `src/apps/sequencer/ui/pages/TopPage.cpp` (UI integration)
- UI for pause state visualization

**Dependencies:** None

**Use Cases:**
- Compose with some tracks paused, then unmute all at once
- Create transitions by pausing/unpausing tracks
- Practice individual parts by pausing other tracks
- Live performance build-ups

---

### 🟡 6. Extended Ratcheting Modes
**Status:** PARTIAL IMPLEMENTATION, MODERATE FEASIBILITY

**Current State:**
- Retrigger layer exists: 0-3 (representing 1-4 triggers)
- RetriggerProbability: 0-7
- Mode: MULTIPLY only (even subdivision)
- Track-level RetriggerProbabilityBias (CV routable!)

**Current Behavior:**
```cpp
// From NoteTrackEngine.cpp lines 331-343
int stepRetrigger = evalStepRetrigger(step, _noteTrack.retriggerProbabilityBias());
if (stepRetrigger > 1) {
    int retriggerLength = divisor / stepRetrigger;  // Even subdivision
    // Creates N triggers evenly spaced
}
```

**Proposed Extensions:**

**Mode 1: MULTIPLY (Current)**
- Subdivides step duration evenly
- 2 triggers = two 50% gates
- 4 triggers = four 25% gates

**Mode 2: PULSE**
- Fires N short triggers at step start
- All triggers within first 25% of step
- Creates "burst" effect

**Mode 3: GATED**
- Continuous triggers while gate is high
- Trigger rate configurable (1/16, 1/32, 1/64)
- Like a hardware trigger generator

**Implementation Approach:**

**Option A: Per-Track Ratchet Mode** (Simpler)
```cpp
// In NoteTrack.h
enum class RatchetMode {
    Multiply,  // Current behavior
    Pulse,     // Burst at start
    Gated,     // Continuous
};
```

**Option B: Per-Step Ratchet Mode** (More flexible)
- Reuse some of the unused bits in step data
- Different mode per step
- More complex UI

**Recommendation:** Start with Option A (per-track mode)

**Feasibility:** ⭐⭐⭐⭐ (7/10)
**Effort:** 3-5 days
**Musical Value:** ⭐⭐⭐⭐ (Medium-High - expands rhythmic possibilities)
**Risk:** Medium (need to ensure timing accuracy)

**Files to Modify:**
- `src/apps/sequencer/model/NoteTrack.h` (add mode enum)
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` (modify trigger logic)
- `src/apps/sequencer/ui/pages/NoteTrackPage.cpp` (UI for mode selection)

**Dependencies:** None

**Testing:**
- Verify timing accuracy of all modes
- Test with different step lengths
- Ensure gate queue doesn't overflow
- Test interaction with existing Retrigger probability

---

### 🟡 7. Per-Track Clock Enable via CV Gate
**Status:** NOT IMPLEMENTED, MODERATE FEASIBILITY

**Concept:** CV gate signal enables/disables track clock

**Current State:**
- No clock enable functionality
- Mute exists but doesn't stop clock
- Routing system could support this

**Proposed Implementation:**

**Option A: Add as Routing Target** (Recommended)
```cpp
// In Routing.h
enum class Target {
    // ... existing targets
    ClockEnable,  // NEW: CV gate controls track clock
};

// In NoteTrackEngine::tick()
bool NoteTrackEngine::isClockEnabled() const {
    if (_noteTrack.isRouted(Routing::Target::ClockEnable)) {
        float cv = getRoutedValue(Routing::Target::ClockEnable);
        return cv > 2.5f;  // Gate high = enabled
    }
    return true;  // Default: always enabled
}
```

**Option B: Dedicated Track Parameter** (More explicit)
```cpp
// In NoteTrack.h
enum class ClockEnable {
    Always,
    CvGate1, CvGate2, CvGate3, CvGate4
};
```

**Behavior:**
- Gate high (>2.5V): Track clock runs, sequence advances
- Gate low (<2.0V): Track clock paused, maintains position
- Tracks stay in sync when enabled (don't drift)

**Feasibility:** ⭐⭐⭐⭐ (7/10)
**Effort:** 3-4 days
**Musical Value:** ⭐⭐⭐ (Medium - performance tool)
**Risk:** Low (simple gate check)

**Files to Modify:**
- `src/apps/sequencer/model/Routing.h` (add target)
- `src/apps/sequencer/engine/RoutingEngine.cpp` (route CV to enable)
- `src/apps/sequencer/engine/NoteTrackEngine.cpp` (check enable state)
- UI for routing configuration

**Dependencies:** None (routing system handles CV reading)

**Use Cases:**
- LFO gating tracks for stuttering patterns
- Manual gates for track bring-in/out
- Probability-based track activation
- Sequencer controlling other sequencers

---

### 🟡 8. CV-Triggered Pattern Changes
**Status:** NOT IMPLEMENTED, MODERATE FEASIBILITY

**Concept:** CV rising edges advance to next pattern in chain

**Current State:**
- Pattern selection via UI or MIDI
- Pattern chaining exists
- Routing target `Pattern` exists (but for CV value, not triggers)

**Proposed Implementation:**
```cpp
// In NoteTrack.h
enum class PatternTrigger {
    Manual,   // User selects
    CvEdge1,  // CV1 rising edge
    CvEdge2,  // CV2 rising edge
    CvEdge3,  // CV3 rising edge
    CvEdge4,  // CV4 rising edge
};

// In NoteTrackEngine (called each tick)
void NoteTrackEngine::checkPatternTrigger() {
    auto trigger = _noteTrack.patternTrigger();

    if (trigger != PatternTrigger::Manual) {
        int cvChannel = int(trigger) - 1;
        if (_engine.cvInput().checkRisingEdge(cvChannel)) {
            advanceToNextPattern();
        }
    }
}
```

**Pattern Advance Options:**
1. **Next in Chain:** Follow existing pattern chain
2. **Next Sequential:** Always increment (1→2→3→4...)
3. **Random:** Random pattern selection

**Feasibility:** ⭐⭐⭐ (6/10)
**Effort:** 4-5 days
**Musical Value:** ⭐⭐⭐ (Medium - arrangement control)
**Risk:** Medium (requires edge detection, pattern change logic)

**Files to Modify:**
- `src/apps/sequencer/model/NoteTrack.h`
- `src/apps/sequencer/engine/NoteTrackEngine.cpp`
- `src/apps/sequencer/engine/CvInput.h/cpp` (edge detection)
- UI for trigger mode selection

**Dependencies:** Edge detection (shared with Global CV Clock)

**Use Cases:**
- External sequencer controlling song structure
- Trigger pads advancing through patterns
- LFO creating evolving pattern progressions
- Synchronized pattern changes across tracks

---

### 🔴 9. Per-Track CV Clock (Original Proposal)
**Status:** NOT RECOMMENDED

**Concept:** Each track can have independent CV clock source

**Analysis:** See IDEATION.md for complete technical analysis

**Why Not Recommended:**
- ❌ Breaks fundamental unified clock architecture
- ❌ Requires major engine refactoring
- ❌ Thread safety concerns
- ❌ Better alternatives exist (Global CV Clock + Divisor routing)
- ❌ Loss of track synchronization features

**Feasibility:** ⭐⭐ (2/10)
**Effort:** 8+ days (major refactor)
**Musical Value:** ⭐⭐ (Low - alternatives achieve similar results)
**Risk:** Very High (architectural changes, timing issues)

**Better Alternatives:**
1. **Global CV Clock** (all tracks follow CV)
2. **CV → Divisor Routing** (already works! per-track tempo)
3. **Track Clock Enable** (CV gate enables/disables tracks)

**Verdict:** DO NOT IMPLEMENT. Use alternatives instead.

---

### 🔴 10. Pulse Count System
**Status:** NOT RECOMMENDED

**Concept:** Replace "one step per tick" with pulse counter system

**Analysis:**
The current system already has fine-grained control:
- 192 PPQN clock resolution
- 48 PPQN sequence resolution
- Flexible divisor system (1-768 range)
- Per-step gate offset for fine timing

**Why Not Recommended:**
- ❌ Unclear advantage over existing divisor system
- ❌ Current architecture already "counts" via `tick % divisor`
- ❌ Would require rewriting core timing logic
- ❌ No specific musical problem identified

**Feasibility:** ⭐⭐⭐ (3/10)
**Effort:** 8+ days
**Musical Value:** ⭐ (Very Low - redundant with existing features)
**Risk:** High (core timing changes)

**Verdict:** DO NOT IMPLEMENT unless specific use case identified.

---

## Critical Discoveries

### 🎉 Discovery 1: CV → Divisor Already Works!

**What We Found:**
The routing system ALREADY supports routing CV inputs to track divisor. This means **Alternative 2 from IDEATION.md is already implemented**!

**Current Capabilities:**
```
CV Input → Routing Matrix → Track Divisor → Real-time tempo modulation
```

**What This Enables:**
- LFO modulating track speed
- Expression pedal controlling tempo
- Sequencer CV creating programmed tempo changes
- Per-track polyrhythms via CV control

**Impact on Feature Priorities:**
- ✅ Major feature already available
- ⚠️ Needs better documentation
- ⚠️ Needs UI improvements for visibility
- 📊 Should be prominently featured in manual

**Recommended Actions:**
1. Document this capability in user manual
2. Create preset routing templates
3. Add visual feedback for routed divisor
4. Tutorial videos showing polyrhythm techniques

---

### 🎉 Discovery 2: Extensive Routing Capabilities

**The PEW|FORMER routing system is MORE POWERFUL than initially assessed:**

**CV-Routable Parameters (Per-Track):**
- ✅ Divisor (tempo/timing)
- ✅ All probability biases (Gate, Retrigger, Length, Note, Shape)
- ✅ Octave, Transpose, Offset, Rotate
- ✅ SlideTime
- ✅ Mute, Fill, FillAmount, Pattern

**This Means:**
Most "CV control" feature requests can be addressed through:
1. Documenting existing routing capabilities
2. Improving UI for routing configuration
3. Creating routing presets/templates

**Impact:**
- Reduces need for custom CV integration code
- Focus should shift to exposing/documenting existing power
- New features should integrate with routing system

---

### 🎉 Discovery 3: Sophisticated Timing Features Already Exist

**Current Timing Capabilities:**
1. **PlayMode:** Aligned vs Free (per-track clock sync)
2. **RunMode:** 6 sequence directions (Forward, Backward, Pendulum, PingPong, Random, RandomWalk)
3. **Divisor:** 1-768 range (1/64T to 4 bars)
4. **Gate Offset:** Per-step fine timing adjustment
5. **Groove/Swing:** Global swing with per-tick adjustment
6. **Reset Measure:** Auto-reset after N bars
7. **Track Linking:** Share sequence state between tracks

**What This Means:**
The timing system is already very flexible. New timing features should:
- Build on existing infrastructure
- Maintain compatibility with current features
- Respect the unified clock architecture

---

## Implementation Priority Matrix

### Priority 1: Quick Wins (Immediate Value, Low Risk)
**Timeframe:** Week 1

| Feature | Status | Effort | Value | Risk |
|---------|--------|--------|-------|------|
| **Document CV→Divisor** | Exists! | 1 day | High | None |
| **Global CV Clock** | New | 2 days | High | Low |

**Why First:**
- CV→Divisor documentation: Feature already works, just needs visibility
- Global CV Clock: Standard eurorack feature, uses existing infrastructure

**Expected Impact:**
- Users discover powerful existing capability
- Standard CV clock workflow enabled
- Foundation for edge detection (used by later features)

---

### Priority 2: High-Value Additions (Medium Effort, Clear Benefit)
**Timeframe:** Week 2-3

| Feature | Effort | Value | Risk |
|---------|--------|-------|------|
| **Per-Track Play/Pause** | 2-3 days | Med-High | Low |
| **Extended Ratcheting** | 3-5 days | Med-High | Med |

**Why Second:**
- Play/Pause: Simple implementation, workflow improvement
- Ratcheting: Extends existing feature, clear musical value

**Expected Impact:**
- Improved composition workflow
- Expanded rhythmic palette
- Builds on familiar features

---

### Priority 3: Advanced Features (Higher Effort, Niche Use)
**Timeframe:** Week 4+

| Feature | Effort | Value | Risk |
|---------|--------|-------|------|
| **Track Clock Enable** | 3-4 days | Medium | Low |
| **CV Pattern Trigger** | 4-5 days | Medium | Med |

**Why Later:**
- Clock Enable: Useful but less critical than earlier features
- Pattern Trigger: Arrangement control, more specialized use case

**Expected Impact:**
- Performance capabilities
- Complex arrangement control
- Integration with external gear

---

### Priority 4: Do Not Implement

| Feature | Reason |
|---------|--------|
| **Per-Track CV Clock** | Breaks architecture, better alternatives exist |
| **Pulse Count System** | No clear advantage, redundant with divisor |

---

## Detailed Feature Analysis

### Feature Comparison Table

| Feature | Exists | Feasible | Effort | Value | Risk | Recommend |
|---------|--------|----------|--------|-------|------|-----------|
| **Accumulator** | ✅ Yes | N/A | Complete | ⭐⭐⭐⭐⭐ | None | ✅ Done |
| **CV→Divisor** | ✅ Yes | N/A | 1d doc | ⭐⭐⭐⭐⭐ | None | ✅ Document |
| **Routing System** | ✅ Yes | N/A | N/A | ⭐⭐⭐⭐⭐ | None | ✅ Promote |
| **Global CV Clock** | ❌ No | 9/10 | 2d | ⭐⭐⭐⭐⭐ | Low | ✅ Phase 1 |
| **Track Play/Pause** | ❌ No | 8/10 | 2-3d | ⭐⭐⭐⭐ | Low | ✅ Phase 2 |
| **Extended Ratchet** | 🟡 Partial | 7/10 | 3-5d | ⭐⭐⭐⭐ | Med | ✅ Phase 2 |
| **Track Clock Enable** | ❌ No | 7/10 | 3-4d | ⭐⭐⭐ | Low | ⚠️ Phase 3 |
| **CV Pattern Trigger** | ❌ No | 6/10 | 4-5d | ⭐⭐⭐ | Med | ⚠️ Phase 3 |
| **Per-Track CV Clock** | ❌ No | 2/10 | 8+d | ⭐⭐ | High | ❌ No |
| **Pulse Count** | ❌ No | 3/10 | 8+d | ⭐ | High | ❌ No |

**Legend:**
- ✅ Recommended: Implement
- ⚠️ Maybe: Consider after higher priorities
- ❌ No: Do not implement

---

## Implementation Roadmap

### Phase 0: Documentation & Awareness (Week 0)
**Goal:** Expose existing powerful features

**Tasks:**
1. Document CV→Divisor routing capability
2. Create routing presets/templates
3. Update user manual with routing examples
4. Tutorial: "Polyrhythms with CV Control"

**Deliverables:**
- Updated documentation
- Routing preset library
- Video tutorials

**Effort:** 3-4 days

---

### Phase 1: Foundation (Week 1)
**Goal:** Add standard eurorack clock features

**Feature:** Global CV Clock Mode

**Tasks:**
1. Implement edge detection in CvInput
2. Add CV clock sources to ClockSetup
3. Wire CV edges to Clock::slaveTick()
4. UI integration and testing

**Deliverables:**
- CV clock sources (CV1-4) in clock setup
- Tested edge detection (hardware verified)
- Updated UI showing CV clock active

**Effort:** 2 days

**Testing Focus:**
- Edge detection accuracy
- Noise immunity (false trigger prevention)
- Timing precision vs. MIDI clock
- Oscilloscope verification

---

### Phase 2: Core Enhancements (Week 2-3)
**Goal:** Improve workflow and expand rhythmic capabilities

**Features:**
1. Per-Track Play/Pause
2. Extended Ratcheting Modes

**Tasks:**

**Play/Pause (2-3 days):**
1. Add pause state to NoteTrack
2. Modify NoteTrackEngine::tick() to check state
3. UI integration (SHIFT+Track button)
4. LED feedback implementation
5. Test with all track types

**Extended Ratcheting (3-5 days):**
1. Add RatchetMode enum to NoteTrack
2. Implement PULSE mode logic
3. Implement GATED mode logic
4. UI for mode selection
5. Test timing accuracy
6. Verify gate queue behavior

**Deliverables:**
- Working pause/play per track
- 3 ratchet modes (Multiply, Pulse, Gated)
- UI for both features
- Comprehensive testing

**Effort:** 5-8 days combined

---

### Phase 3: Advanced Features (Week 4+)
**Goal:** Add performance and arrangement tools

**Features:**
1. Track Clock Enable via CV
2. CV-Triggered Pattern Changes

**Tasks:**

**Clock Enable (3-4 days):**
1. Add ClockEnable routing target
2. Implement enable check in tick()
3. UI for routing configuration
4. Test with various CV sources

**Pattern Trigger (4-5 days):**
1. Add PatternTrigger enum to NoteTrack
2. Implement edge-triggered pattern advance
3. UI for trigger mode selection
4. Test pattern chain interaction
5. Verify timing of pattern changes

**Deliverables:**
- CV gate control of track clock
- CV edge-triggered pattern changes
- Routing presets using new features

**Effort:** 7-9 days combined

---

## Musical Use Cases

### Use Case 1: Evolving Polyrhythms
**Enabled By:** CV→Divisor (already exists!)

**Setup:**
1. Route LFO (triangle) to Track 1 Divisor
2. Route LFO (sine, inverted) to Track 2 Divisor
3. Set base divisor = 12 (1/16 notes)
4. LFO creates smooth tempo variations

**Result:**
- Tracks drift in and out of phase
- Organic, evolving polyrhythmic textures
- No manual parameter tweaking needed

**Status:** ✅ **WORKS NOW** (just needs documentation)

---

### Use Case 2: Sequencer-Controlled Sequencer
**Enabled By:** Multiple routing connections

**Setup:**
1. Track 1: Master sequence (gates + CV)
2. Track 2: Slave sequence
3. Route Track 1 CV → Track 2 Divisor
4. Route Track 1 Gate → Track 2 Mute (via routing)

**Result:**
- Track 1 controls tempo and activation of Track 2
- Self-modulating sequence behavior
- Complex pattern emergence

**Status:** ✅ **WORKS NOW** (routing system supports this)

---

### Use Case 3: Live Performance Build-Up
**Enabled By:** Per-Track Play/Pause (Phase 2)

**Setup:**
1. All 8 tracks programmed with complementary parts
2. Start with all tracks paused except drums (Track 1)
3. Progressively unpause tracks: bass, chords, lead, etc.

**Result:**
- Smooth live build-up/breakdown
- Tracks stay in sync when unpaused
- No sequence drift

**Status:** 🔜 Coming in Phase 2

---

### Use Case 4: CV-Clocked Generative System
**Enabled By:** Global CV Clock (Phase 1) + CV→Divisor

**Setup:**
1. External random LFO → CV Clock (sets master tempo)
2. Additional LFOs → Track Divisors (set relative speeds)
3. Random RunMode on multiple tracks
4. High probability settings for variation

**Result:**
- Self-evolving tempo and rhythmic relationships
- No two performances identical
- Eurorack-style generative sequencing

**Status:** 🔜 Coming in Phase 1

---

### Use Case 5: Ratchet Drum Fills
**Enabled By:** Extended Ratcheting (Phase 2)

**Setup:**
1. Drum sequence on Track 1
2. Last step: Retrigger=4, Mode=PULSE
3. Triggers 4 rapid hits at end of pattern

**Result:**
- Classic drum fill behavior
- Burst of triggers (not evenly spaced)
- Natural-sounding fills

**Status:** 🔜 Coming in Phase 2

---

### Use Case 6: Gate-Controlled Track Activation
**Enabled By:** Track Clock Enable (Phase 3)

**Setup:**
1. 4 percussion tracks (hi-hat, snare, kick, perc)
2. Each track: Clock Enable = different CV input
3. External mixer/sequencer sends gates

**Result:**
- Real-time track muting via external control
- Maintains sequence position when disabled
- Live performance control from external source

**Status:** 🔧 Planned for Phase 3

---

## Testing Strategy

### Unit Testing
**For Each New Feature:**
1. Model layer tests (parameter ranges, serialization)
2. Engine layer tests (timing, state management)
3. UI layer tests (parameter editing, display)

**Existing Test Infrastructure:**
- `src/tests/unit/sequencer/` - Unit test location
- Example: `TestAccumulator.cpp` - 100+ test cases

---

### Integration Testing
**Cross-Feature Testing:**
1. CV Clock + Divisor Routing
2. Ratcheting + Probability Biases
3. Play/Pause + Pattern Chains
4. Multiple routing connections simultaneously

**Test Scenarios:**
- All 8 tracks with different configurations
- Extreme parameter values
- Rapid parameter changes
- Long-running sessions (stability)

---

### Hardware Testing
**Critical for CV Features:**

**Equipment Needed:**
- Oscilloscope (timing verification)
- Multiple CV/LFO sources (Maths, etc.)
- MIDI clock source
- Gate sources
- Eurorack test patches

**Test Procedures:**
1. **Edge Detection:**
   - Slow rising edges (1-10 Hz)
   - Fast rising edges (>100 Hz)
   - Noisy CV signals
   - Different voltage levels (0-10V)

2. **Clock Accuracy:**
   - Compare CV clock to MIDI clock (oscilloscope)
   - Measure jitter over 1000 ticks
   - Verify drift vs. external clock

3. **Routing Accuracy:**
   - CV→Divisor: Verify tempo changes match CV voltage
   - CV→Probabilities: Confirm probabilistic behavior
   - Multiple simultaneous routings

4. **Performance:**
   - Monitor CPU usage (profiler task)
   - Check for audio glitches
   - Verify OLED doesn't cause noise (existing issue)

---

## Risk Assessment

### Low Risk Features ✅
- Global CV Clock (uses existing infrastructure)
- Per-Track Play/Pause (simple state check)
- Track Clock Enable (gate check)

**Mitigation:** Thorough testing, small code changes

---

### Medium Risk Features ⚠️
- Extended Ratcheting (timing accuracy critical)
- CV Pattern Trigger (pattern change timing)

**Mitigation:**
- Extensive timing tests with oscilloscope
- Hardware verification before deployment
- Beta testing with users

---

### High Risk Features 🔴
- Per-Track CV Clock (architectural changes)
- Pulse Count System (core timing refactor)

**Mitigation:** **DON'T IMPLEMENT** - use alternatives

---

## Conclusion

### Summary of Findings

**Already Implemented (Promote These!):**
1. ✅ Accumulator System - Full featured, hardware tested
2. ✅ CV→Divisor Routing - Works now, needs docs
3. ✅ Extensive Routing System - 42+ targets, very powerful

**Recommended for Implementation:**
1. 🟢 Global CV Clock (Phase 1) - Standard eurorack feature
2. 🟢 Per-Track Play/Pause (Phase 2) - Workflow improvement
3. 🟢 Extended Ratcheting (Phase 2) - Expands rhythmic palette

**Consider for Future:**
4. 🟡 Track Clock Enable (Phase 3) - Performance tool
5. 🟡 CV Pattern Trigger (Phase 3) - Arrangement control

**Do Not Implement:**
6. 🔴 Per-Track CV Clock - Breaks architecture
7. 🔴 Pulse Count System - Redundant with divisor

---

### Recommended Action Plan

**Week 0:** Document existing CV→Divisor capability
**Week 1:** Implement Global CV Clock
**Week 2-3:** Add Play/Pause + Extended Ratcheting
**Week 4+:** Consider Clock Enable + Pattern Trigger

**Total Effort:** ~4 weeks for all recommended features

**Expected Outcome:**
- Users discover powerful existing features
- Standard eurorack CV clock workflow
- Improved composition tools
- Expanded rhythmic capabilities
- Performance features for live use

---

### Key Insights

1. **The routing system is more powerful than initially recognized**
   - CV→Divisor already works
   - Most "CV control" requests can use existing routing
   - Focus on documentation and UI improvements

2. **Edge detection is the foundation**
   - Required for CV Clock, Pattern Trigger
   - Implement once, use for multiple features
   - Critical to get right (noise immunity)

3. **Respect the architecture**
   - Unified clock model is a strength, not limitation
   - Features should build on existing patterns
   - Avoid major refactors without clear benefit

4. **Prioritize user-facing value**
   - Document existing features first
   - Simple features with clear benefits
   - Defer complex features with niche use cases

---

**Document Version:** 1.0
**Last Updated:** 2025-11-17
**Status:** Complete Analysis - Ready for Implementation Planning
