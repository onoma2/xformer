# Tuesday Track CV and Gate Behavior Analysis

## Overview

This document analyzes the differences between the original Tuesday Eurorack module behavior and the PEW|FORMER TuesdayTrack implementation, specifically regarding CV (pitch) changes and gate firing.

## Original Tuesday Module (ALGO-RESEARCH/Tuesday)

### Pattern Generation
- The `Tuesday_Generate` function creates a pre-computed pattern during initialization
- Each tick contains:
  - `note`: The pitch value
  - `vel`: Velocity (used for intensity filtering)
  - `accent`, `slide`, `maxsubticklength`: Additional parameters

### Playback Logic
In the `Tuesday_Tick` function (found in `/ALGO-RESEARCH/Tuesday/Sources/Tuesday.c`):

```c
Tuesday_Tick_t *Tick = &T->CurrentPattern.Ticks[T->Tick];

if (Tick->vel >= T->CoolDown)  // Intensity threshold
{
    T->CoolDown = CoolDownMax;
    
    // Update CV output ONLY when threshold passed
    T->CVOutTarget = (DAC_NOTE(T->CurrentPattern.Ticks[T->Tick].note, 0)) << 16;
    if (Tick->slide > 0)
    {
        T->CVOutDelta = (T->CVOutTarget - T->CVOut) / (Tick->slide * 50);
        T->CVOutCountDown = Tick->slide * 50;
    }
    else
    {
        T->CVOut = T->CVOutTarget;  // CV updates immediately!
    }
    
    // Then trigger gates
    int Ticks = (T->msecpertick * T->CurrentPattern.Ticks[T->Tick].maxsubticklength)/T->ticklengthscaler;
    if (T->Gates[GATE_GATE] > 0) { T->Gates[GATE_GATE] = -Ticks; T->GatesGap[GATE_GATE] = GATE_MINGATETIME; }
    else T->Gates[GATE_GATE] = Ticks;
}
```

### Original Behavior Summary
- **CV Output**: Only updated when intensity threshold is met (`Tick->vel >= T->CoolDown`)
- **Gate Output**: Only fired when intensity threshold is met
- **Result**: When intensity filtering prevents a note from playing, both gate and CV output remain unchanged at their previous values

## PEW|FORMER TuesdayTrack Implementation

### CV Update Logic
In `TuesdayTrackEngine::tick()` (found in `/src/apps/sequencer/engine/TuesdayTrackEngine.cpp`):

```cpp
// Apply CV with slide/portamento - EXECUTED EVERY STEP
_cvTarget = noteVoltage;
if (_slide > 0) {
    // Calculate slide time: slide * 12 ticks (scaled for our timing)
    int slideTicks = _slide * 12;
    _cvDelta = (_cvTarget - _cvCurrent) / slideTicks;
    _slideCountDown = slideTicks;
} else {
    // Instant change - ALWAYS HAPPENS EVERY STEP
    _cvCurrent = _cvTarget;
    _cvOutput = _cvTarget;
    _slideCountDown = 0;
}

// Gate firing is conditional
if (shouldGate && _coolDown == 0) {
    _gateOutput = true;
    // Gate length: use algorithm-determined percentage
    _gateTicks = (divisor * _gatePercent) / 100;
    if (_gateTicks < 1) _gateTicks = 1;  // Minimum 1 tick
    _activity = true;
    // Reset cooldown after triggering
    _coolDown = _coolDownMax;
}
```

### PEW|FORMER Behavior Summary
- **CV Output**: Updated on every step regardless of gate state
- **Gate Output**: Only fired when algorithm says `shouldGate` AND cooldown period has expired
- **Result**: Continuous pitch evolution with sparse gate articulation

## Key Differences

| Aspect | Original Tuesday | PEW|FORMER TuesdayTrack |
|--------|------------------|------------------------|
| CV Update | Only when intensity threshold passed | Every step regardless of gate state |
| Gate Firing | Only when intensity threshold passed | Based on algorithm + cooldown |
| Musical Result | Sparse notes with CV changes only when played | Continuous pitch evolution with sparse articulation |

## Musical Implications

### Original Tuesday
- More traditional sequencer behavior
- CV output is "gated" - only changes when notes are played
- Intensity parameter acts as both density control and gate filter
- Better suited for triggering envelope generators with each pitch change

### PEW|FORMER TuesdayTrack
- Modern generative music behavior
- CV output continuously evolves, creating smooth pitch progressions
- Gates fire independently, allowing for rhythmic articulation of continuous pitch changes
- Better suited for evolving textures and algorithmic pitch progressions
- Works well with external envelope generators that need to be triggered independently

## Hardware Behavior
- Users will hear continuous pitch changes even when gates are not firing in PEW|FORMER
- This is the intended behavior of the PEW|FORMER implementation and is musically distinct from the original Tuesday

## Files Referenced
- `/ALGO-RESEARCH/Tuesday/Sources/Tuesday.c` - Original Tuesday implementation
- `/ALGO-RESEARCH/Tuesday/Sources/Tuesday.h` - Original Tuesday data structures
- `/src/apps/sequencer/engine/TuesdayTrackEngine.cpp` - PEW|FORMER implementation
- `/src/apps/sequencer/engine/TuesdayTrackEngine.h` - PEW|FORMER data structures
- `/src/apps/sequencer/model/TuesdayTrack.h` - PEW|FORMER model parameters


## Implementation Ideas for Adding CV Update Mode Switch

### Approach 1: Parameter-Based Switch (Recommended)

#### Model Layer Changes (TuesdayTrack.h/cpp)
- Add a new parameter: `uint8_t _cvUpdateMode` (bitfield)
- Add getter/setter: `cvUpdateMode()`, `setCvUpdateMode()`
- Define enum: `CvUpdateMode { Free = 0, Gated = 1 }`
- Add to serialization in read/write methods

```cpp
// In TuesdayTrack.h
enum CvUpdateMode { Free = 0, Gated = 1 };
CvUpdateMode cvUpdateMode() const { return static_cast<CvUpdateMode>(_cvUpdateMode); }
void setCvUpdateMode(CvUpdateMode mode) { _cvUpdateMode = static_cast<uint8_t>(mode); }
```

#### Engine Layer Changes (TuesdayTrackEngine.cpp/h)
- Modify `TuesdayTrackEngine::tick()` method with conditional CV update logic
- Store previous note value when in GATED mode to maintain CV during non-gate steps
- Add state variable to track last valid note for gated mode:

```cpp
// In TuesdayTrackEngine.h (additional state)
float _lastValidCvOutput = 0.f;  // For gated mode

// In TuesdayTrackEngine.cpp (conditional logic)
if (tuesdayTrack.cvUpdateMode() == CvUpdateMode::Free || (shouldGate && _coolDown == 0)) {
    // Update CV normally
    _cvTarget = noteVoltage;
    // ... rest of CV update logic
} else if (tuesdayTrack.cvUpdateMode() == CvUpdateMode::Gated) {
    // Maintain last valid CV value
    _cvTarget = _lastValidCvOutput;
    _cvCurrent = _lastValidCvOutput;
}
```

#### UI Layer Changes
- Add parameter to `TuesdayTrackListModel` for editing
- Include in `TuesdayPage` parameter cycling (F1-F5 keys)
- Add display in UI showing current mode ('FREE' or 'GATED')
- Could be implemented as a toggle between 'FREE' and 'GATED' values

### Approach 2: Algorithm-Level Setting
- More complex implementation
- Could be part of algorithm-specific parameter sets
- Allows per-algorithm CV behavior configuration

### Approach 3: Hardware Button/Modifier
- Use modifier keys in UI (e.g., SHIFT + F3)
- Real-time switching capability
- More UI complexity

## Recommended Implementation Approach

Approach 1 (Parameter-Based Switch) is recommended because:
- Clean separation of concerns
- Easy to implement and maintain
- Consistent with existing parameter system
- Can be saved with projects
- Allows individual track settings
- Follows existing UI patterns

### State Management Considerations
- In GATED mode, engine needs to maintain the previous CV value when not updating
- Need to handle initialization and switching between modes correctly
- Consider how this affects slide/portamento behavior in both modes
- Should preserve existing behavior when mode is set to FREE (backward compatibility)

### Implementation Complexity
- **Model**: Low complexity (similar to existing parameters)
- **Engine**: Medium complexity (requires conditional logic and state management)
- **UI**: Low complexity (similar to existing parameters)

This approach maintains backward compatibility while giving users the option to choose between the two different CV update behaviors.