# General Algorithm Design - Signal Flow Plan

---

## Part 1: Parameter & State Architecture

### 1. User Parameters (Sequence-Determined)
*Stored in `TuesdaySequence`. Saved with Project. Controlled by User.*

**Core Algorithm Control**
| Parameter | Range | Purpose |
|-----------|-------|---------|
| **algorithm** | 0-20 | Algorithm selector (e.g., MINIMAL, STOMPER) |
| **flow** | 0-16 | Primary RNG seed & Structure control |
| **ornament** | 0-16 | Secondary RNG seed, Detail/Density, & Polyrhythm subdivisions |
| **power** | 0-16 | Gate density (Step & Microgate level cooldown control) |

**Musical & Loop**
| Parameter | Range | Purpose |
|-----------|-------|---------|
| **loopLength** | 0-29 | 0=Infinite, 1-29=Fixed step loop |
| **start** | 0-16 | Start delay in steps |
| **scale** | -1 to Count | Scale selection (-1=Project) |
| **rootNote** | -1 to 11 | Root note override |
| **octave** | -10 to +10 | Global octave shift |
| **transpose** | -11 to +11 | Global degree shift |

**Articulation & Timing**
| Parameter | Range | Purpose |
|-----------|-------|---------|
| **glide** | 0-100% | Slide probability |
| **trill** | 0-100% | Retrigger/Subdivision probability |
| **gateLength** | 0-100% | Gate duration scaler |
| **gateOffset** | 0-100% | Micro-timing shift (0%=Quantized, 50%=Algo Groove, 100%=Exaggerated) |
| **divisor** | varies | Step clock division (1/16, etc.) |
| **resetMeasure**| 0-128 | Force reset every N bars |

### Ornament Parameter: Polyrhythm Subdivisions

The **ornament** parameter serves dual purposes:
1. **RNG Seed** for algorithm detail/variation (algorithm-specific)
2. **Polyrhythm Control** for microgate subdivisions (global system)

**Polyrhythm Mapping:**
| Ornament | Subdivisions | Musical Name | Microgates per Step |
|----------|-------------|--------------|---------------------|
| 0-4 | 4 | Straight (4/4) | 1 gate (no polyrhythm) |
| 5-8 | 3 | Triplet | Up to 3 gates |
| 9-12 | 5 | Quintuplet | Up to 5 gates |
| 13-16 | 7 | Septuplet | Up to 7 gates |

**Implementation:**
```cpp
ctx.subdivisions = 4;  // Default straight
if (ornament >= 5 && ornament <= 8) ctx.subdivisions = 3;   // Triplet
else if (ornament >= 9 && ornament <= 12) ctx.subdivisions = 5;  // Quintuplet
else if (ornament >= 13) ctx.subdivisions = 7;  // Septuplet
```

**Musical Effect:**
- **ornament < 5:** Single note per step (traditional sequencer behavior)
- **ornament ≥ 5:** Multiple microgates per step, creating polyrhythmic patterns
- Number of microgates that actually fire depends on **power** and **velocity** (see microgate cooldown)

**Interaction with Power:**
- Power controls microgate sparsity (how many of the potential microgates fire)
- Low power + high ornament = sparse polyrhythms (occasional bursts)
- High power + high ornament = dense polyrhythms (most steps have full subdivisions)

### Velocity-Dependent Cooldown Curve (Power Parameter)
The density logic is a **Nested Dynamic Threshold** operating at two hierarchical levels.

#### Level 1: Step-Level Cooldown (Controls WHICH steps fire)
1.  **Base Rule:** A step fires when `_coolDown` reaches 0. This takes `17 - Power` steps.
2.  **Velocity Override:** A step *also* fires if `velocity / 16 >= _coolDown`.
    *   High velocity notes (accents) can "break through" the cooldown early.
    *   **Example:** At Power 1 (Base 16), a standard note might wait 16 steps. But a high velocity note (200+) can trigger when cooldown drops to ~12 (every 4 steps).
    *   **Musicality:** Louder/important notes are preserved even at low density settings.

#### Level 2: Microgate-Level Cooldown (Controls HOW MANY gates per step)
When polyrhythm is active (ornament ≥ 5), each step can contain multiple microgates. The microgate cooldown controls how many fire:

1.  **Same Mapping:** `_microCoolDownMax = 17 - Power` (identical to step-level)
2.  **Harder Threshold:** Velocity override requires `velocity / 16 >= _microCoolDown × 2` (2x harder than step-level)
3.  **Decrement Timing:** Decrements once per step (not per microgate) to prevent rapid burn-through
4.  **Per-Microgate Check:**
    ```
    for each microgate in polyrhythm:
        if (_microCoolDown == 0):
            fire() and reset _microCoolDown
        else if (velocity / 16 >= _microCoolDown × 2):
            fire() and reset _microCoolDown
    ```

**Musical Effect:**
- **Power 0:** MUTE (no gates fire)
- **Power 1-4:** Sparse steps, mostly 1 microgate per step (occasionally 2-3 if high velocity)
- **Power 8:** Medium density, mix of 1-3 microgates per step
- **Power 16:** Dense steps, mostly full polyrhythms (all microgates fire)

**Algorithm Interaction:**
- High velocity algorithms (Chip1, Wobble: vel=255) → Rich polyrhythmic texture at medium/high power
- Medium velocity (StepWave: vel=200-255) → Moderate variation in microgate count
- Low velocity (Stomper: vel=0-63) → Mostly single notes (polyrhythm limited)

**Why 2x Harder?** Prevents all microgates from firing every time, creating meaningful sparsity and variation within steps.

---

### 2. Algorithm State (Persistence)
*Stored in `TuesdayTrackEngine`. Runtime RAM only. "Remembers" context between steps.*

**Common State**
- `_rng`, `_extraRng`: The random number generators (seeded by Flow/Ornament).
- `_stepIndex`: Current step position.

**Algorithm-Specific State (MINIMAL Example)**
| Variable | Type | Purpose |
|----------|------|---------|
| `_minimal_mode` | 0/1 | Current state: SILENCE or BURST |
| `_minimal_burstTimer` | int | Steps remaining in current burst |
| `_minimal_silenceTimer`| int | Steps remaining in current silence |
| `_minimal_noteIndex` | int | Current position in melody scale |
| `_minimal_clickDensity`| int | Cached density threshold (from Ornament) |

---

### 3. Tick Properties (Transient Flags)
*Generated by Algorithm. Passed to Engine. Valid for **one step** only.*

**The `TuesdayTickResult` Struct**
| Property | Purpose | Usage |
|----------|---------|-------|
| **note** | Scale degree index | Generated by algorithm (0-11 or 0-6) |
| **octave** | Octave offset | Generated by algorithm |
| **velocity** | 0-255 (0=Rest) | Controls density override (vel/16 vs cooldown) |
| **accent** | bool (Force/Boost) | Always fires, bypasses cooldown |
| **gateRatio** | 0-100% duration | Gate length percentage |
| **slide** | bool (Glide) | Portamento to next note |
| **polyCount**| int (Subdivisions) | Number of microgates (from ornament) |
| **isSpatial**| bool (Spread/Rapid)| Polyrhythm timing mode |
| **beatSpread**| int (0-100) | Polyrhythm window spread |
| **noteOffsets**| int8_t[8] | Per-microgate pitch offsets |

---

## Scale Parameter: Output Quantization (NOT Algorithm Control)

**IMPORTANT:** The `scale` parameter controls **output quantization** in `scaleToVolts()`.

**It does NOT affect algorithm behavior.** Algorithms generate note indices (scale degrees or semitones) and the quantization layer transforms them to voltages.

### Separation of Concerns

**Algorithm Stage** (`generateStep()`):
- Generates raw note indices (scale degrees or semitones)
- Scale-based algorithms use 0-6 (for 7-note scales)
- Chromatic algorithms can use 0-11 (semitones)
- **Algorithm does not know or care about quantization**

**Quantization Stage** (`scaleToVolts()`):
- Transforms note indices to voltages using selected scale
- Applies scale quantization, rootNote, transpose, octave
- **Scale 0 (Semitones) = all 12 semitones** (effectively chromatic)

**Example:** An algorithm that outputs note index 5 will produce:
- C Major scale (scale=1): G (5th degree of C major = G)
- Chromatic (scale=0): F (5th semitone from C = F)
- Project scale (scale=-1): Depends on project settings

---

## Part 2: Implemented Algorithms

### Algorithm Overview

| Index | Name | Function | Velocity Range | Polyrhythm Support |
|-------|------|----------|----------------|-------------------|
| 0 | Test | `generateTest()` | `(ornament-1) << 4` (0-240) | Yes |
| 1 | TriTrance | `generateTritrance()` | 255 or 0-127 (phase) | Yes |
| 2 | Stomper | `generateStomper()` | 0-63 (random) | Limited (low vel) |
| 3 | Aphex | `generateAphex()` | 180 or 80 (state) | Yes |
| 4 | Autech | `generateAutechre()` | 160 (fixed) | Yes |
| 5 | StepWave | `generateStepwave()` | 200-255 (state) | Yes |
| 6 | Markov | `generateMarkov()` | 40-167 (random) | Variable |
| 7 | Chip1 | `generateChipArp1()` | 255 (fixed) | Excellent |
| 8 | Chip2 | `generateChipArp2()` | 0-255 (random) | Variable |
| 9 | Wobble | `generateWobble()` | 255 (fixed) | Excellent |
| 10 | ScaleWlk | `generateScalewalker()` | 100-260 (power!) | Excellent |

### Velocity & Polyrhythm Interaction

**Microgate Override Capability:**
- **velDensity = velocity / 16** (0-15 range)
- **Microgate threshold = microCoolDown × 2** (2x harder than step-level)
- **Can override up to:** velDensity / 2 = cooldown value

**Algorithm Categories:**

**Excellent Polyrhythm Support (vel ≥ 200):**
- **Chip1, Wobble:** vel=255 → velDensity=15 → overrides microCoolDown ≤ 7
- **StepWave:** vel=200-255 → velDensity=12-15 → overrides microCoolDown ≤ 6-7
- **ScaleWalker:** vel=100-260 (power-linked!) → smooth density control
- **Result:** Rich polyrhythmic texture at medium/high power

**Moderate Polyrhythm Support (vel ~160-180):**
- **Autechre:** vel=160 → velDensity=10 → overrides microCoolDown ≤ 5
- **Aphex:** vel=180 or 80 → variable texture
- **Result:** Predictable patterns, moderate variation

**Variable Polyrhythm Support (random velocity):**
- **TriTrance:** vel=255 or 0-127 → dual personality (full or limited)
- **Markov:** vel=40-167 → organic/unpredictable variation
- **Chip2:** vel=0-255 → chaotic variation
- **Result:** Unpredictable microgate counts, organic feel

**Limited Polyrhythm Support (vel < 100):**
- **Stomper:** vel=0-63 → velDensity=0-3 → overrides microCoolDown ≤ 1
- **Result:** Mostly single notes, polyrhythm setting has minimal effect

### Power Sweep Behavior Summary

**All Algorithms (with Polyrhythm = 3):**

| Power | Step Density | Microgate Behavior | Sound Character |
|-------|-------------|-------------------|-----------------|
| 0 | Mute | None | Silence |
| 1-4 | Very sparse (~every 13-16 steps) | Mostly 1 gate | "ping.......ping......" |
| 5-8 | Sparse (~every 9-12 steps) | 1-2 gates | "ping..brrt...ping.." |
| 9-12 | Medium (~every 5-8 steps) | 1-3 gates (varies) | "ping-brrt-brrt-ping" |
| 13-16 | Dense (~every 1-4 steps) | 2-3 gates (mostly full) | "brrt-brrt-brrt-brrt" |

**High Velocity Algorithms (Chip1, Wobble):** Full polyrhythmic expression at power ≥ 8
**Low Velocity Algorithms (Stomper):** Minimal polyrhythmic effect at all power levels

---

## Part 3: Implementation Details

### Key Engine Components

**Cooldown System:**
```cpp
// Step-level
int _coolDown = 0;
int _coolDownMax = 0;

// Microgate-level (Option 1.5)
int _microCoolDown = 0;
int _microCoolDownMax = 0;

// Power mapping (both levels)
_coolDownMax = _microCoolDownMax = 17 - power;
```

**Microgate Queue:**
```cpp
struct MicroGate {
    uint32_t tick;    // Absolute tick time
    bool state;       // true=ON, false=OFF
    float cv;         // CV voltage for this gate
};

SortedQueue<MicroGate, 32, MicroGateCompare> _microGateQueue;
```

**Generation Context:**
```cpp
struct GenerationContext {
    int stepIndex;
    int flow;
    int ornament;
    int subdivisions;  // Set by ornament (3,5,7 for polyrhythm)
    int tpb;          // Ticks per beat
};
```

### Signal Flow

1. **Parameter Read** → `TuesdaySequence` (flow, ornament, power, etc.)
2. **Context Setup** → Map ornament to subdivisions
3. **Algorithm Generate** → `generateStep()` calls algorithm-specific function
4. **Result Processing** → TuesdayTickResult with velocity, polyCount, noteOffsets
5. **Step Cooldown Check** → Can step fire? (power + velocity)
6. **Microgate Scheduling** → Per-microgate cooldown checks (Option 1.5)
7. **Queue Management** → MicroGate events sorted by tick
8. **Gate Output** → Process queue, fire gates at precise ticks
9. **Quantization** → `scaleToVolts()` applies scale/transpose
10. **CV Output** → Final voltage to hardware DAC

---
