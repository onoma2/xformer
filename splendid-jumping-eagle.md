# CurveTrack Global Phase Offset - Simplified Implementation Plan

## Overview
Implement a simple global phase offset parameter for CurveTrack that shifts the curve evaluation point within each step, enabling true LFO-style phasing effects without the complexity of tick-level timing manipulation.

## Core Concept
Instead of manipulating timing at the discrete tick level, apply a fractional phase offset directly to the curve evaluation. This leverages the existing `_currentStepFraction` (0.0-1.0) calculation and simply adds an offset with wraparound.

**Mathematical approach:**
```cpp
// Current implementation
float value = curveFunction(_currentStepFraction);

// With phase offset
float phasedFraction = fmod(_currentStepFraction + _phaseOffset, 1.0f);
float value = curveFunction(phasedFraction);
```

## Why This Is Simpler

1. **No Clock System Changes** - Works entirely within CurveTrackEngine, no modifications to Engine tick distribution
2. **Fractional Domain** - Operates on normalized 0.0-1.0 values, not discrete ticks
3. **Minimal Code Changes** - One calculation in `updateOutput()` method
4. **Low Overhead** - Single `fmod()` operation per curve evaluation
5. **No Synchronization Issues** - Doesn't affect how tracks receive clock ticks

## Implementation Steps

### 1. Model Layer (CurveTrack.h/cpp)

**Add phase offset parameter:**
- Property: `_phaseOffset` (0-100, representing 0% to 100% of step duration)
- Storage: Use existing bitfield structure (check `_data` union for available bits)
- Range: 0-100 (maps to 0.0-1.0 internally in engine)
- Routable: No (not in this phase - user specified no routing integration)
- **User specification**: 0-100% unidirectional range

**Methods to add:**
```cpp
// CurveTrack.h
int phaseOffset() const;                    // Returns 0-100
void setPhaseOffset(int offset);            // Clamps to 0-100
void editPhaseOffset(int value, bool shift);
void printPhaseOffset(StringBuilder &str) const;
void clear(); // Update to reset phase offset to 0
```

**Serialization:**
- Add to `write()` method (bit-pack with other parameters)
- Add to `read()` method (unpack from stored byte)
- Version compatibility: Default to 0 for older projects

**Bitfield allocation:**
- Need 7 bits for 0-100 range (127 max)
- Check `_data` union for available space
- Follow existing pattern from slideTime, rotate, etc.

### 2. Engine Layer (CurveTrackEngine.cpp)

**Modify `updateOutput()` method:**

Current code (line ~220):
```cpp
void CurveTrackEngine::updateOutput(uint32_t relativeTick, uint32_t divisor) {
    _currentStepFraction = float(relativeTick % divisor) / divisor;
    float value = evalStepShape(step, _shapeVariation || fillVariation,
                                 fillInvert, _currentStepFraction);
    _cvOutputTarget = value;
}
```

Modified code:
```cpp
void CurveTrackEngine::updateOutput(uint32_t relativeTick, uint32_t divisor) {
    // Calculate base step fraction
    _currentStepFraction = float(relativeTick % divisor) / divisor;

    // Apply phase offset (convert 0-100 to 0.0-1.0 and wrap)
    float phaseOffset = float(_curveTrack.phaseOffset()) / 100.f;
    float phasedFraction = fmod(_currentStepFraction + phaseOffset, 1.0f);

    // Evaluate curve at phased position
    float value = evalStepShape(step, _shapeVariation || fillVariation,
                                 fillInvert, phasedFraction);
    _cvOutputTarget = value;
}
```

**No other engine changes needed:**
- **Gate timing unaffected** (user requirement: gates stay at step boundaries)
- Slide still works (applied to `_cvOutputTarget` in `update()`)
- Recording unaffected
- Link data unaffected

### 3. UI Integration

**A. CurveTrackListModel (Track Page Display)**

Add "PHAS" parameter to the track parameter list in `src/apps/sequencer/ui/model/CurveTrackListModel.cpp`:

**Parameter implementation:**
```cpp
case Parameter::PhaseOffset:
    cell(col++, "PHAS");
    cell(col++, FixedStringBuilder<8>(_curveTrack.phaseOffset()));
    break;
```

**Edit handler:**
```cpp
case Parameter::PhaseOffset:
    _curveTrack.editPhaseOffset(value, shift);
    break;
```

**Print handler:**
```cpp
case Parameter::PhaseOffset:
    _curveTrack.printPhaseOffset(str);
    break;
```

**B. CurveSequenceEditPage (F4 Button + Indicator)**

**User requirement**: Phase offset control accessible on CurveSequenceEditPage via F4 (rightmost) button with tiny font indication.

**Implementation approach:**

1. **Add F4 Function** (`src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` lines 45-52):
```cpp
enum class Function {
    Shape   = 0,    // F1
    Min     = 1,    // F2
    Max     = 2,    // F3
    Gate    = 3,    // F4
    Phase   = 4,    // F5 (new - will map to rightmost physical button)
};

static const char *functionNames[] = { "SHAPE", "MIN", "MAX", "GATE", "PHASE", nullptr };
```

2. **Add Phase Edit Mode** (similar to lines 149-152):
```cpp
// In keyDown handler
case Key::F4:  // Rightmost function button
    setFunction(Function::Phase);
    break;
```

3. **Phase Display in Header** (`src/apps/sequencer/ui/painters/WindowPainter.h/cpp`):

Add method declaration (WindowPainter.h):
```cpp
void drawPhaseOffset(Canvas &canvas, int phaseOffset);
```

Implementation (WindowPainter.cpp, following pattern from drawAccumulatorValue lines 114-123):
```cpp
void WindowPainter::drawPhaseOffset(Canvas &canvas, int phaseOffset) {
    canvas.setFont(Font::Tiny);
    canvas.setBlendMode(BlendMode::Set);
    canvas.setColor(Color::Medium);
    // Position in header, right side (x~170-180)
    canvas.drawText(170, 8 - 2, FixedStringBuilder<8>("PH:%d", phaseOffset));
}
```

4. **Call from CurveSequenceEditPage::draw()** (around line 180-190):
```cpp
void CurveSequenceEditPage::draw(Canvas &canvas) {
    // ... existing code ...

    // Draw phase offset indicator
    WindowPainter::drawPhaseOffset(canvas, _curveTrack.phaseOffset());

    // ... rest of draw code ...
}
```

5. **Handle Phase Editing** (in keyPress/encoder handler):
When Function::Phase is active:
- Encoder adjusts _curveTrack phase offset
- Display updates in real-time
- Shows current value in header

**UI Layout:**
- Header indicator: x=170, y=6, Font::Tiny, format "PH:nnn"
- Doesn't interfere with existing step display (main area y=16-48)
- F4/F5 button label: "PHASE"
- Active when phase editing mode enabled

### 4. Routing Integration

**Not implemented in this phase** - User has specified no routing integration at this point.

Phase offset will be a direct track parameter only, editable via UI but not routable via CV sources. This can be added in a future iteration if needed.

### 5. Testing Strategy

**Unit Tests (src/tests/unit/sequencer/):**

Create `TestCurveTrackPhaseOffset.cpp`:
```cpp
UNIT_TEST("CurveTrackPhaseOffset") {

CASE("default_value") {
    CurveTrack track;
    expectEqual(track.phaseOffset(), 0, "default phase offset should be 0");
}

CASE("setter_getter") {
    CurveTrack track;
    track.setPhaseOffset(50);
    expectEqual(track.phaseOffset(), 50, "phase offset should be 50");
}

CASE("clamping") {
    CurveTrack track;
    track.setPhaseOffset(200);
    expectEqual(track.phaseOffset(), 100, "phase offset should clamp to 100");

    track.setPhaseOffset(-10);
    expectEqual(track.phaseOffset(), 0, "phase offset should clamp to 0");
}

CASE("serialization") {
    CurveTrack track;
    track.setPhaseOffset(75);

    VersionedSerializedWriter writer;
    track.write(writer);

    CurveTrack loaded;
    VersionedSerializedReader reader(writer.data(), writer.dataSize());
    loaded.read(reader);

    expectEqual(loaded.phaseOffset(), 75, "phase offset should persist");
}

} // UNIT_TEST
```

**Engine Tests:**

Create `TestCurveTrackEnginePhase.cpp`:
- Test that phase offset shifts curve evaluation correctly
- Test phase wraparound (offset + fraction > 1.0)
- Test with different curve shapes
- Test interaction with slide, offset, and other parameters
- Test that gates are unaffected by phase offset

**Integration Tests:**
- Test multiple CurveTracks with different phase offsets
- Test routing CV to phase offset
- Test project save/load with phase offset

### 6. Performance Considerations

**Computational Cost:**
- One `fmod()` operation per curve update
- Negligible compared to curve function evaluation
- No memory allocation
- No additional state tracking

**Memory Cost:**
- ~7 bits per CurveTrack (fits in existing bitfield)
- No additional per-step storage needed
- Total: <1 byte per track

**STM32 Compatibility:**
- Well within processing budget
- No floating-point concerns (already using FP in curve evaluation)
- No stack or heap impact

## Critical Files to Modify

1. **Model Layer:**
   - `src/apps/sequencer/model/CurveTrack.h` - Add phase offset property and methods
   - `src/apps/sequencer/model/CurveTrack.cpp` - Implement methods and serialization

2. **Engine Layer:**
   - `src/apps/sequencer/engine/CurveTrackEngine.cpp` - Modify `updateOutput()` method (line ~220)

3. **UI Layer - Track Page:**
   - `src/apps/sequencer/ui/model/CurveTrackListModel.h` - Add Phase parameter to enum
   - `src/apps/sequencer/ui/model/CurveTrackListModel.cpp` - Add parameter display/edit

4. **UI Layer - Sequence Edit Page:**
   - `src/apps/sequencer/ui/pages/CurveSequenceEditPage.h` - Add Phase function enum
   - `src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp` - Add F4 handler and phase editing
   - `src/apps/sequencer/ui/painters/WindowPainter.h` - Add drawPhaseOffset() declaration
   - `src/apps/sequencer/ui/painters/WindowPainter.cpp` - Implement phase offset indicator

5. **Tests:**
   - Create `src/tests/unit/sequencer/TestCurveTrackPhaseOffset.cpp`
   - Create `src/tests/unit/sequencer/TestCurveTrackEnginePhase.cpp`
   - Update `src/tests/unit/sequencer/CMakeLists.txt` to register new tests

## Advantages Over Original Plan

1. **Much simpler implementation** - No tick-level phase tracking
2. **No architectural changes** - Works within existing framework
3. **Easier to test** - Purely functional transformation
4. **No timing synchronization concerns** - Operates independently per track
5. **Lower maintenance burden** - Minimal code surface area

## Limitations

**Quantization to tick resolution:**
- Phase shifts are still quantized to PPQN ticks (192 per quarter note)
- This is fine for musical applications (sub-millisecond resolution)
- True analog-style continuous phase would require sub-tick interpolation

**Step boundaries:**
- Phase offset applies within each step independently
- Doesn't create "true" cross-step phasing like continuous LFO drift
- This is acceptable - provides the desired effect for musical purposes

**No automatic drift:**
- Phase offset is static (unless CV-controlled)
- For evolving textures, modulate via CV routing or automation

## Success Criteria

- ✓ Phase offset range: 0-100% of step duration
- ✓ Smooth curve shifting without clicks or discontinuities
- ✓ Works with all curve shapes
- ✓ Compatible with slide, offset, rotate parameters
- ✓ Accessible via F4 button on CurveSequenceEditPage
- ✓ Tiny font indicator in header doesn't hinder other UI elements
- ✓ Gates remain at step boundaries (unaffected by phase)
- ✓ Saves/loads correctly in projects
- ✓ Negligible performance impact
- ✓ All existing functionality preserved

## Implementation Order (TDD Approach)

Following Test-Driven Development methodology:

1. **Model layer tests** - Write failing tests for CurveTrack phase offset
2. **Model layer implementation** - Implement CurveTrack property, methods, serialization
3. **Engine layer tests** - Write failing tests for phase-shifted curve evaluation
4. **Engine layer implementation** - Modify `updateOutput()` to apply phase offset
5. **UI integration** - Add to CurveTrackListModel, CurveSequenceEditPage, WindowPainter
6. **Integration tests** - Full workflow validation
7. **Hardware validation** - Test on actual STM32 hardware

**Test-first for each component ensures:**
- Clear requirements before implementation
- Immediate verification of correctness
- No regressions introduced

## Estimated Scope

- **Code changes:** ~100-150 lines total
- **Test code:** ~200-300 lines
- **Complexity:** Low - straightforward fractional offset
- **Risk:** Low - isolated change, well-defined behavior
