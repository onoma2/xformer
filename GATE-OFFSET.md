# GateOffset Implementation Plan for TuesdayTrack

## Overview
This document outlines the plan for adding GateOffset functionality to TuesdayTrack, allowing algorithms to generate timing variations that can be overridden by a user parameter, similar to the existing Glide override system.

## Key Design Principles
- **Positive-only range**: GateOffset values are 0-100% (no negative timing)
- **Algorithmic generation**: Each algorithm can generate its own timing variations
- **Override capability**: Global GateOffset parameter can override algorithmic values
- **Buffer-based**: Uses the same buffered step approach as existing parameters

## Implementation Changes

### 1. Extend BufferedStep Structure
**File**: `src/apps/sequencer/engine/TuesdayTrackEngine.h`

```cpp
// Structure for pre-generated step data
struct BufferedStep {
    int8_t note;
    int8_t octave;
    uint8_t gatePercent;
    uint8_t slide;
    uint8_t gateOffset;  // NEW: 0-100% timing offset
};
```

### 2. TuesdayTrack Model Changes
**File**: `src/apps/sequencer/model/TuesdayTrack.h`

Add new property:
```cpp
// gateOffset (0-100% user override for algorithmic gate timing)
int gateOffset() const { return _gateOffset; }
void setGateOffset(int gateOffset) { 
    _gateOffset = clamp(gateOffset, 0, 100); 
}

void editGateOffset(int value, bool shift) {
    setGateOffset(this->gateOffset() + value * (shift ? 10 : 1));
}

void printGateOffset(StringBuilder &str) const {
    str("%d%%", gateOffset());
}
```

Add member variable:
```cpp
uint8_t _gateOffset = 0;  // Default: 0% (no offset)
```

Add to serialization in `TuesdayTrack.cpp`:
```cpp
writer.write(_gateOffset);
reader.read(_gateOffset, ProjectVersion::VersionXX);  // Add new version
```

### 3. Update Buffer Generation
**File**: `src/apps/sequencer/engine/TuesdayTrackEngine.cpp`

In the `generateBuffer()` function, for each algorithm case in the main loop, initialize the `gateOffset` value:

```cpp
// Default initialization in buffer generation loop
for (int step = 0; step < BUFFER_SIZE; step++) {
    int note = 0;
    int octave = 0;
    uint8_t gatePercent = 75;
    uint8_t slide = 0;
    uint8_t gateOffset = 0;  // NEW: Initialize default value

    switch (algorithm) {
        case 0: // TEST
            // ... existing logic ...
            gateOffset = 0;  // Algorithm-specific logic here
            break;
        case 1: // TRITRANCE
            // ... existing logic ...
            gateOffset = 0;  // Algorithm might set pattern-based offsets
            break;
        // ... continue for all algorithms ...
    }

    // Store in buffer
    _buffer[step].note = note;
    _buffer[step].octave = octave;
    _buffer[step].gatePercent = gatePercent;
    _buffer[step].slide = slide;
    _buffer[step].gateOffset = gateOffset;  // NEW
}
```

### 4. Runtime Processing with Override Logic
**File**: `src/apps/sequencer/engine/TuesdayTrackEngine.cpp`

In the `tick()` function, update the step processing to handle the new gateOffset:

```cpp
// After retrieving buffered data
int effectiveStep = (stepIndex + _tuesdayTrack.rotate()) % actualLength;
int8_t note = _buffer[effectiveStep].note;
int8_t octave = _buffer[effectiveStep].octave;
uint8_t bufferedGatePercent = _buffer[effectiveStep].gatePercent;
uint8_t bufferedSlide = _buffer[effectiveStep].slide;
uint8_t bufferedGateOffset = _buffer[effectiveStep].gateOffset;  // NEW

// Apply user override for gateOffset (similar to glide override)
uint8_t effectiveGateOffset = bufferedGateOffset;
if (_tuesdayTrack.gateOffset() > 0 && _rng.nextRange(100) < _tuesdayTrack.gateOffset()) {
    effectiveGateOffset = _tuesdayTrack.gateOffset();  // Override with user value
}

// Apply algorithmic processing to gateOffset if needed
// (this could be algorithm-specific enhancement)

// Convert effectiveGateOffset percentage to timing offset
uint32_t actualGateOffset = (divisor * effectiveGateOffset) / 100;
// This value would be applied to gate timing in the step processing logic
```

### 5. Algorithm-Specific GateOffset Implementations

Each algorithm in the buffer generation should implement its own timing variations:

#### TRITRANCE (1)
- Generate timing offsets that complement the 3-phase pattern
- Could add slight delays to create swing feel on certain phases

#### FUNK (9)
- Generate syncopated timing offsets for backbeat patterns
- Shift certain notes slightly ahead or behind the beat

#### PHASE (11)
- Use phase accumulator to determine timing variations
- Create evolving rhythmic patterns

#### MARKOV (3)
- Use transition probabilities to determine timing variations
- Different states could have different timing characteristics

#### AMBIENT (13)
- Generate random timing variations for organic feel
- Sparse events could have more timing flexibility

#### Other algorithms
- Each algorithm can implement its own approach to timing variation

### 6. UI Integration
**File**: `src/apps/sequencer/ui/pages/TuesdayTrackPage.cpp`

Add UI control for GateOffset parameter:
- Add to parameter selection list
- Add editing functionality
- Add display in the UI

## Integration with Existing Systems

### 1. Compatibility Considerations
- Ensure gate timing changes integrate properly with existing TuesdayTrack features
- GateOffset should work with all existing parameters and modes

### 2. Clock Sync
- GateOffset timing should be properly synchronized with the clock system
- Respects divisor settings and timing constraints

### 3. Global Override Mechanism
- Like Glide parameter, the global GateOffset parameter can override algorithmic values
- User GateOffset value overrides algorithmic values based on probability (glide > 0 && _rng.nextRange(100) < glide)
- Provides user control while preserving algorithmic diversity

### 4. Performance Considerations
- Minimal impact on CPU usage since timing is pre-calculated in buffer
- Memory impact is just 1 byte per buffer entry

## Testing Strategy

### 1. Unit Tests
- Test buffer generation with GateOffset values
- Test override logic works correctly
- Test all algorithms generate valid GateOffset values

### 2. Integration Tests
- Test GateOffset works correctly with each algorithm
- Test global override properly overrides algorithmic values
- Test timing accuracy with different divisor settings

## Timeline

### Phase 1: Core Implementation (Week 1)
- Modify BufferedStep structure
- Add GateOffset parameter to TuesdayTrack model
- Update serialization

### Phase 2: Buffer Generation (Week 1-2)
- Update all algorithm buffer generation to include GateOffset
- Add basic algorithmic timing variations

### Phase 3: Runtime Processing (Week 2)
- Implement override logic
- Integrate timing calculation into step processing

### Phase 4: UI Integration (Week 2-3)
- Add UI controls for GateOffset
- Test parameter editing and persistence

### Phase 5: Algorithm Enhancement (Week 3)
- Enhance algorithms with meaningful GateOffset generation
- Fine-tune algorithm-specific timing variations

## Benefits

- Adds expressive rhythmic timing control to TuesdayTrack
- Maintains algorithmic nature while adding user control
- Follows established patterns from Glide override
- Enables complex algorithmic timing variations
- Provides both algorithmic and manual control over timing