# TB-303 Style Algo Track Implementation

## Overview

TB-3PO (O&C applet by Phazerville) is a TB-303 inspired sequencer. This document covers porting TB-3PO concepts to XFormer's TuesdayTrackEngine as an Algo track mode.

---

## TB-3PO Features

### Global Parameters

| Parameter | Range | XFormer Equivalent |
|-----------|-------|-------------------|
| SEED | 0000-FFFF (4 hex) | flow + ornament (0-16 each) |
| LOCK | ON/OFF | Reseed control |
| DENSITY | -7 to +7 | power (controls gate + pitch range together) |
| Q.SCALE | MAJ/MIN/etc | scale (-1 = project, 0+ = specific) |
| Q.ROOT | C to B | rootNote (-1 = project, 0-11 = specific) |
| TRANS | ROOT/DEG | transpose mode (semitones vs scale degrees) |
| LENGTH | 1-32 | loopLength (0-29 in XFormer) |
| HOLD | ON/OFF | cvUpdateMode (Gated) |

### Per-Step Parameters

| Step Flag | Description | XFormer Implementation |
|-----------|-------------|------------------------|
| GATE | ON/OFF gate | Gate mode |
| ACCENT | 6V gate (vs 5V) | accent flag + separate gate2 output |
| SLIDE | Exponential glide | slide flag + glide duration |
| OCT | -1, 0, +1 octave | octave offset per step |

### Key 303 Characteristics

1. **Density controls BOTH**: gate probability AND pitch range together
   - Low density = sparse gates + limited pitch
   - High density = busy gates + full scale

2. **Fixed-time exponential glide**: Not voltage-controlled time, fixed duration

3. **Rules-based randomness**: Seed + rules = deterministic patterns

4. **Accent = 6V gate**: Secondary VCA responds to this

---

## Existing Algo Track Patterns

### TuesdayTrackEngine Parameter Access

```cpp
// GenerationContext - passed to all algorithm generators
struct GenerationContext {
    uint32_t divisor;
    int tpb, loopLength, effectiveLoopLength, rotatedStep;
    int ornament, subdivisions, stepsPerBeat;
    bool isBeatStart;
};

// TuesdayTickResult - abstract step result from generators
struct TuesdayTickResult {
    int note = 0;           // Scale Degree (0-7)
    int octave = 0;         // Octave Offset
    uint8_t velocity = 0;   // 0-255
    bool accent = false;
    bool slide = false;
    uint16_t gateRatio = 75;    // 0-200% gate duration
    uint8_t gateOffset = 0;     // 0-100% timing offset
    uint8_t trillCount = 1;     // 1-4 subdivision
    uint8_t beatSpread = 0;     // 0-100% timing window
    uint8_t polyCount = 0;      // tuplet subdivisions
};
```

### Dual RNG System

```cpp
Random _rng;        // Seed from flow parameter
Random _extraRng;   // Seed from ornament parameter
```

### Stomper Algorithm Accent Pattern

```cpp
int accentoffs = 0;
uint8_t veloffset = 0;

// Stomper modes affect accent probability
switch (_algoState.stomper.mode) {
case 4: case 5: // LOW
    accentoffs = 100;        // 100% accent
    break;
case 0: case 1: // PAUSE
    veloffset = 0;           // Quiet
    break;
}

// Accent: PercChance(50 + accentoffs)
if (int(_rng.nextRange(256)) >= (50 + accentoffs)) {
    result.accent = true;
}

// Velocity calculation
result.velocity = (_extraRng.nextRange(256) / 4) + veloffset;
```

### Markov Algorithm Pattern

```cpp
// Always accent (100%)
result.accent = true;
result.velocity = (_rng.nextRange(256) / 2) + 40;

// Random slide
if (_rng.nextBinary() && _rng.nextBinary()) {
    result.slide = true;
}
```

---

## Implementation Plan

### Phase 1: Add TB303 Step Flags

```cpp
// In TuesdayTrackEngine.h

// Extend TuesdayTickResult
struct TuesdayTickResult {
    // ... existing fields ...
    
    // TB-303 specific
    int8_t octaveShift = 0;  // -1, 0, +1 per step
    bool accentVoltage = false;  // true = 6V gate output
    uint8_t slideTime = 0;   // 0-100% slide duration
};

// TB-303 Algorithm State
struct TB303State {
    uint16_t seed;
    bool seedLocked;
    int8_t density;           // -7 to +7
    uint8_t patternLength;    // 1-32
    bool holdPitch;
    
    // Internal state
    int8_t currentDensity;
    int8_t currentRange;
    int8_t pitchAccumulator;
    bool lastWasAccent;
};
```

### Phase 2: Add TB303 Algorithm Generator

```cpp
// In TuesdayTrackEngine.cpp

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateTB303(const GenerationContext &ctx) {
    TuesdayTickResult result;
    result.velocity = 200;
    result.gateRatio = 75;
    
    TB303State &state = _algoState.tb303;
    
    // Density affects BOTH gate probability AND pitch range
    // density: -7 = sparse/limited, +7 = busy/full
    int densityRange = 7 + state.density;  // 0-14
    int gateProbability = 50 + (densityRange * 8);  // 50-162
    int pitchRange = state.density + 7;  // 0-14 semitones
    
    // Gate decision
    if (_rng.nextRange(256) < gateProbability) {
        result.accent = state.lastWasAccent;
        state.lastWasAccent = !state.lastWasAccent;
        
        // Pitch selection from density-controlled range
        int baseNote = _rng.nextRange(pitchRange + 1) - (pitchRange / 2);
        result.note = clamp(baseNote, 0, 6);  // Scale degree
        result.octave = baseNote < 0 ? -1 : (baseNote > 6 ? 1 : 0);
        
        // Octave shift per step
        if (_rng.nextBinary()) {
            result.octaveShift = _rng.nextRange(3) - 1;  // -1, 0, +1
        }
        
        // Accent voltage flag
        if (_rng.nextRange(100) > 50) {
            result.accentVoltage = true;
        }
        
        // Slide probability based on density
        if (state.density > 0 && _rng.nextRange(100) < state.density * 10) {
            result.slide = true;
            result.slideTime = 50 + (state.density * 5);
        }
    }
    
    return result;
}
```

### Phase 3: Hook into dispatch

```cpp
// In generateStep()
switch (algorithm) {
    // ... existing algorithms ...
    case 15: return generateTB303(ctx);  // Add as algorithm 15
}
```

### Phase 4: Add Model Layer Support

```cpp
// In TuesdaySequence.h
// TB-303 specific parameters (optional per-sequence settings)

int tb303Density() const { return _tb303Density.get(isRouted(Routing::Target::TB303Density)); }
void setTB303Density(int density, bool routed = false) {
    _tb303Density.set(clamp(density, -7, 7), routed);
}

// Add to private members:
Routable<int8_t> _tb303Density;  // Default: 0
```

---

## Gate Output for Accent

### Current Implementation

XFormer has 8 CV/Gate outputs per track. Accent (6V gate) can be implemented as:

```cpp
// Gate output logic in tick()
if (result.accent && result.accentVoltage) {
    _gateOutput2 = true;  // Use second gate for 6V accent
} else {
    _gateOutput = true;   // Standard 5V gate
}
```

### Alternative: Velocity-based

```cpp
// Use velocity to distinguish accent vs regular
// Accent = velocity > 200, Regular = velocity < 200
if (result.accent && result.accentVoltage) {
    _gateOutput = true;
    _cvOutput = result.cvOutput + (1.0f / 12.0f);  // +1 semitone for accent
}
```

---

## TB-303 Rules Engine

The original 303 uses simple rules for pattern generation:

1. **Slide rules**: Consecutive same-note → glide
2. **Accent rules**: Strong beats → accent
3. **Octave rules**: Certain patterns trigger octave shifts

### Implementation

```cpp
bool shouldSlide(int currentNote, int lastNote) {
    return currentNote == lastNote;  // Same note = slide
}

bool shouldAccent(int stepInPattern, int density) {
    // Accent on strong beats (1, 5, 9, 13...)
    bool strongBeat = (stepInPattern % 4) == 0;
    return strongBeat && (_rng.nextRange(100) < (50 + density * 7));
}
```

---

## File Locations

- **Engine**: `src/apps/sequencer/engine/TuesdayTrackEngine.cpp/.h`
- **Model**: `src/apps/sequencer/model/TuesdaySequence.cpp/.h`
- **UI Pages**: `src/apps/sequencer/ui/pages/TuesdayPages.cpp`
- **Tests**: `src/tests/unit/sequencer/TestTuesdayTrackEngine.cpp`

---

## References

- TB-3PO O&C applet: https://github.com/Phazerville/TB-3PO
- XFormer Tuesday algorithms: `TuesdayTrackEngine.cpp` algorithms 0-14
- Original 303 behavior: http://www.hak modular.com/2019/09/22/tb-303-the-myth-the-legend-the-facts/
