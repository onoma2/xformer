# Tuesday Track Technical Implementation Details

## Implementation Reference (Engine Details)

This section documents the actual implementation behavior learned from the original Tuesday source code.

### Power Parameter: Cooldown-Based Density

Power does **NOT** control gate probability. It controls note density through a **cooldown mechanism**:

```cpp
// In tick():
_coolDownMax = 17 - power;  // power 1 -> 16, power 16 -> 1

// Each step:
if (_coolDown > 0) {
    _coolDown--;
}

// Note triggers only when cooldown expired:
if (shouldGate && _coolDown == 0) {
    _gateOutput = true;
    _coolDown = _coolDownMax;  // Reset cooldown
}
```

**Effect:**
- **Power 1** → CoolDownMax 16 → Very sparse (16 steps between notes)
- **Power 16** → CoolDownMax 1 → Very dense (can trigger every step)

**Reference:** `Tuesday.c` lines 743-800, `Tuesday_Tick()` function

### Flow and Ornament: Dual RNG Seeding

Flow (seed1) and Ornament (seed2) **seed RNGs that generate algorithm parameters**. They do NOT directly control musical features.

#### TEST Algorithm
```cpp
// Init:
_testMode = (flow - 1) >> 3;        // 0 or 1 (OCTSWEEPS/SCALEWALKER)
_testSweepSpeed = ((flow - 1) & 0x3); // 0-3 (slide amount)
_testAccent = (ornament - 1) >> 3;    // 0 or 1
_testVelocity = ((ornament - 1) << 4); // 0-240
```

#### TRITRANCE Algorithm
```cpp
// Init:
_rng = Random((flow - 1) << 4);
_extraRng = Random((ornament - 1) << 4);

_triB1 = (_rng.next() & 0x7);     // High note 0-7
_triB2 = (_rng.next() & 0x7);     // Phase offset 0-7
_triB3 = ...from _extraRng...     // Note offset -4 to +3
```

#### STOMPER Algorithm
```cpp
// Init - NOTE: Seeds are swapped!
_rng = Random((ornament - 1) << 4);    // For note choices
_extraRng = Random((flow - 1) << 4);   // For pattern modes

_stomperMode = (_extraRng.next() % 7) * 2;  // Initial mode
_stomperLowNote = _rng.next() % 3;
_stomperHighNote[0] = _rng.next() % 7;
_stomperHighNote[1] = _rng.next() % 5;
```

#### MARKOV Algorithm
```cpp
// Init:
_rng = Random((flow - 1) << 4);
_extraRng = Random((ornament - 1) << 4);

_markovHistory1 = (_rng.next() & 0x7);
_markovHistory3 = (_rng.next() & 0x7);

// Generate 8x8x2 Markov matrix
for (i = 0; i < 8; i++) {
    for (j = 0; j < 8; j++) {
        _markovMatrix[i][j][0] = (_rng.next() % 8);
        _markovMatrix[i][j][1] = (_extraRng.next() % 8);
    }
}
```

**Key Insight:** Same Flow/Ornament values always produce the same pattern (deterministic seeding).

### Gate Length

Gate length is controlled per-algorithm via `_gatePercent` (0-100%).

#### Default Gate Length
```cpp
_gatePercent = 75;  // 75% of step duration
```

#### Random Gate Length (RandomSlideAndLength pattern)
```cpp
// Short gates (40% chance)
if (gateLengthChoice < 40) {
    _gatePercent = 25 + (_rng.nextRange(4) * 25);  // 25%, 50%, 75%, 100%
}
// Medium gates (30% chance)
else if (gateLengthChoice < 70) {
    _gatePercent = 100 + (_rng.nextRange(4) * 25); // 100%, 125%, 150%, 175%
}
// Long gates (30% chance) - 3-4 beat sustains
else {
    _gatePercent = 200 + (_rng.nextRange(9) * 25); // 200%-400%
}
```

#### Per-Algorithm Gate Lengths
| Algorithm | Gate Length |
|-----------|-------------|
| TEST | Fixed 75% |
| TRITRANCE | Random (25-400%) with long gates |
| STOMPER | 75% default, varies with countdown |
| MARKOV | Random (25-400%) with long gates |

**Long Gates (200-400%)**: Enable multi-beat sustained notes for melodic algorithms. At 120 BPM with 16th note divisor, 400% = 4 beats.

**Application:**
```cpp
_gateTicks = (divisor * _gatePercent) / 100;
if (_gateTicks < 1) _gateTicks = 1;
```

### Slide/Portamento

Slide creates smooth CV glide between notes. Value 0-3 where 0=instant, 1-3=glide time.

#### Random Slide (RandomSlideAndLength pattern)
```cpp
// 25% chance of slide (BoolChance && BoolChance = 25%)
if (_rng.nextBinary() && _rng.nextBinary()) {
    _slide = (_rng.nextRange(3)) + 1;  // 1-3
} else {
    _slide = 0;
}
```

#### Per-Algorithm Slide
| Algorithm | Slide Behavior |
|-----------|---------------|
| TEST | Fixed from SweepSpeed (0-3) |
| TRITRANCE | Random (25% chance, 1-3) |
| STOMPER | On SLIDEUP2/SLIDEDOWN2 modes only |
| MARKOV | Random (25% chance, 1-3) |

#### Slide Processing
```cpp
// On new note:
_cvTarget = noteVoltage;
if (_slide > 0) {
    int slideTicks = _slide * 12;  // Scaled for timing
    _cvDelta = (_cvTarget - _cvCurrent) / slideTicks;
    _slideCountDown = slideTicks;
} else {
    _cvCurrent = _cvTarget;
    _cvOutput = _cvTarget;
}

// Each tick:
if (_slideCountDown > 0) {
    _cvCurrent += _cvDelta;
    _slideCountDown--;
    if (_slideCountDown == 0) {
        _cvCurrent = _cvTarget;  // Hit target exactly
    }
    _cvOutput = _cvCurrent;
}
```

### Original Source Code Reference

Location: `ALGO-RESEARCH/Tuesday/Sources/`

| File | Contents |
|------|----------|
| **Algo.h** | Enums, structs, state structures for all algorithms |
| **Algo_Test.h** | TEST algorithm - `Algo_Test_Init()`, `Algo_Test_Gen()` |
| **Algo_TriTrance.h** | TRITRANCE - dual RNG seeding pattern |
| **Algo_Stomper.h** | STOMPER - 14-state machine, slide modes |
| **Algo_Markov.h** | MARKOV - transition matrix generation |
| **Tuesday.c** | Main engine, cooldown system (`Tuesday_Tick()`), clock |
| **Tuesday.h** | Core structures, constants (`TUESDAY_SUBTICKRES`, etc.) |

### Key Functions to Study

| Function | Purpose |
|----------|---------|
| `Algo_XXX_Init()` | How Flow/Ornament seed RNGs |
| `Algo_XXX_Gen()` | Per-step pattern generation |
| `DefaultTick()` | Default gate length (75%) |
| `RandomSlideAndLength()` | Random slide + gate length |
| `Tuesday_Tick()` | Cooldown system, gate/CV output |

### Not Implemented

| Feature | Reason |
|---------|--------|
| **Velocity Output** | Would require additional CV output per track |
| **Accent Output** | Would require additional gate output per track |

PEW|FORMER tracks have single CV/gate pairs. Original Tuesday has dedicated VEL and ACCENT jacks.

### Glide Parameter

The Glide parameter (0-100%) controls the probability of slide/portamento being applied to notes.

```cpp
// Check glide parameter before applying slide
if (_tuesdayTrack.glide() > 0) {
    if (_rng.nextRange(100) < _tuesdayTrack.glide()) {
        _slide = (_rng.nextRange(3)) + 1;  // 1-3
    } else {
        _slide = 0;
    }
} else {
    _slide = 0;  // Glide 0% = never slide
}
```

**Effect:**
- **Glide 0%**: No slides ever (default)
- **Glide 50%**: 50% chance of slide on each note
- **Glide 100%**: Always slide between notes

### Scale Parameter

The Scale parameter toggles between chromatic (Free) and project scale quantization.

```cpp
if (_tuesdayTrack.useScale()) {
    const Scale &scale = _model.project().selectedScale();
    int rootNote = _model.project().rootNote();
    int scaleNote = note + octave * scale.notesPerOctave();
    noteVoltage = scale.noteToVolts(scaleNote) +
        (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
} else {
    // Free mode - use chromatic scale
    noteVoltage = (note + octave * 12) * (1.f / 12.f);
}
```

**Effect:**
- **Free**: Notes are chromatic (all 12 semitones)
- **Project**: Notes quantized to project scale (set in PROJECT page)

### Skew Implementation

The Skew parameter creates a step function for density variation across the loop.

```cpp
int skew = _tuesdayTrack.skew();
if (skew != 0 && loopLength > 0) {
    float position = (float)_stepIndex / (float)loopLength;

    if (skew > 0) {
        // Build-up: last (skew/16) at power 16
        float switchPoint = 1.0f - (float)skew / 16.0f;
        if (position < switchPoint) {
            _coolDownMax = baseCooldown;  // Use power setting
        } else {
            _coolDownMax = 1;  // Maximum density
        }
    } else {
        // Fade-out: first (|skew|/16) at power 16
        float switchPoint = (float)(-skew) / 16.0f;
        if (position < switchPoint) {
            _coolDownMax = 1;  // Maximum density
        } else {
            _coolDownMax = baseCooldown;  // Use power setting
        }
    }
}
```

### Reseed Implementation

The reseed() function generates new random patterns while preserving parameters.

```cpp
void TuesdayTrackEngine::reseed() {
    _stepIndex = 0;
    _coolDown = 0;

    // Generate new seeds from current RNG state
    uint32_t newSeed1 = _rng.next();
    uint32_t newSeed2 = _extraRng.next();

    // Reinitialize RNGs with new seeds
    _rng = Random(newSeed1);
    _extraRng = Random(newSeed2);

    // Reinitialize algorithm with new seeds
    initAlgorithm();
}
```

**Access:**
- **Shift+F5**: Keyboard shortcut on TrackPage
- **Context Menu**: RESEED option in TrackPage context menu (Tuesday tracks only)

### Adding New Algorithms

1. Add state variables to `TuesdayTrackEngine.h`
2. Add initialization in `initAlgorithm()` - seed RNGs from Flow/Ornament
3. Add case in `tick()` switch statement
4. Set `_gatePercent` (gate length) appropriately
5. Set `_slide` (portamento) appropriately
6. Reference original `Algo_XXX.h` for authentic behavior