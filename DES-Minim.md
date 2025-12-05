##  MINIMAL Algorithm - Musical Character

### **Genre**: Minimal Techno / Microsound
### **Aesthetic**: "Less is More"

**Musical Goals**:
- Staccato bursts of activity separated by silence
- Sparse, hypnotic, evolving textures
- Minimal note movement (2-7 notes per burst)
- Tight, precise timing (25-50% gate length)
- Rare articulation (slides/trills <10% probability)

**Pattern Structure**:
```
[SILENCE: 4-16 steps] → [BURST: 2-8 steps] → [SILENCE] → [BURST] → ...
                ↑                    ↑
            controlled by        controlled by
              FLOW               ORNAMENT + POWER
```

**Key Behavior**: **Burst/Silence Cycles**
- Algorithm alternates between silence and short bursts
- Each burst plays 2-8 notes with minimal variation
- Silence duration: 4-16 steps (longer silence = more "minimal" feel)
- Burst duration: 2-8 steps (shorter burst = more "minimal" feel)

---

## Part 3: MINIMAL Signal Flow - Block Diagram

### **Overview: 5-Stage Pipeline**

```
┌─────────────────────────────────────────────────────────────────────┐
│                     MINIMAL ALGORITHM PIPELINE                       │
└─────────────────────────────────────────────────────────────────────┘

[1. INIT STAGE]
  ↓
  Parameters: flow, ornament
  ↓
  Seed RNGs: _rng = Random(flow), _extraRng = Random(ornament)
  ↓
  Initialize State:
    - burstLength = 2-8 (from _rng)
    - silenceLength = 4-16 (from flow)
    - clickDensity = ornament * 16 (0-255)
    - mode = SILENCE (start quiet)

[2. MODE SELECTION STAGE]  ← Called every step in generateStep()
  ↓
  Current Mode?
  ├─ SILENCE → velocity = 0 (mute)
  │            Decrement silenceTimer
  │            If timer == 0 → Switch to BURST
  │
  └─ BURST → Generate note + gate
               Decrement burstTimer
               If timer == 0 → Switch to SILENCE

[3. NOTE GENERATION STAGE]  ← Only in BURST mode
  ↓
  Density Check:
    if (_rng.nextRange(255) < clickDensity):
      PLAY NOTE
    else:
      REST (velocity = 0)
  ↓
  If PLAY:
    note = noteIndex % 7          (scale degrees 0-6)
    octave = (noteIndex / 7) % 3  (0-2 octaves)
    velocity = 128 + _rng.nextRange(127)  (128-255, always triggers)
    gateRatio = 25 + _rng.nextRange(25)   (25-50%, staccato)

[4. ARTICULATION STAGE]  ← Apply glide/trill
  ↓
  Slide Probability:
    if (_rng.nextRange(100) < glide/5):
      result.slide = true
  ↓
  Trill Probability:
    if (_rng.nextRange(100) < trill/10):
      result.chaos = 100 (always fire)
      result.polyCount = 3 (triplet subdivision)
      result.isSpatial = false (trill mode, not polyrhythm)

[5. OUTPUT STAGE]  ← Return TuesdayTickResult
  ↓
  TuesdayTickResult {
    note, octave,           // From Stage 3
    velocity,               // From Stage 3 (0 or 128-255)
    accent = false,         // MINIMAL doesn't use accents
    slide,                  // From Stage 4
    gateRatio,              // From Stage 3 (25-50%)
    gateOffset = 0,         // MINIMAL uses tight timing
    chaos, polyCount,       // From Stage 4 (trill)
    isSpatial,              // From Stage 4
    noteOffsets[8] = {0}    // MINIMAL doesn't use micro-sequencing
  }
```

---

## Part 4: Parameter Mapping - MINIMAL Specific

### **How Each Parameter Controls MINIMAL**

#### **1. FLOW (0-16) - Structure Seed + Silence Duration**

**RNG Mapping**: `_rng = Random(flow << 4)`

**Direct Control**:
```cpp
silenceLength = 4 + (flow % 13);  // 4-16 steps
// flow=0  → silence=4 steps   (fast cycling)
// flow=8  → silence=12 steps  (medium spacing)
// flow=16 → silence=4 steps   (wraps around)
```

**Indirect Control** (via _rng):
- burstLength randomization
- note density within bursts
- mode transitions

**Musical Effect**:
- Low flow (0-4): Rapid burst/silence cycles, frenetic
- Mid flow (5-12): Balanced phrasing, breathing room
- High flow (13-16): Longer silences, more "minimal" aesthetic

---

#### **2. ORNAMENT (0-16) - Detail Seed + Click Density**

**RNG Mapping**: `_extraRng = Random(ornament << 4)`

**Direct Control**:
```cpp
clickDensity = ornament * 16;  // 0-255
// ornament=0  → density=0   (always play during burst)
// ornament=8  → density=128 (50% notes play)
// ornament=16 → density=255 (100% notes play, but different RNG seed)
```

**Indirect Control** (via _extraRng):
- gate length variation (25-50% range)
- slide/trill probability rolls

**Musical Effect**:
- Low ornament (0-4): Sparser bursts, more space within bursts
- Mid ornament (5-12): Balanced note density
- High ornament (13-16): Fuller bursts, more events

**Important**: At ornament=0, clickDensity=0, but backup code has special case:
```cpp
bool shouldPlay = (clickDensity > 0) ?
                  (_rng.nextRange(255) < clickDensity) :
                  true;  // Always play if density=0
```
This makes ornament=0 → 100% dense (not 0% dense!)

---

#### **3. POWER (0-16) - Note Density via Cooldown System**

**Mapping**: `_coolDownMax = map(power, 0, 16, 16, 1)`
- power=0  → coolDownMax=16 (longest cooldown, sparsest)
- power=8  → coolDownMax=8  (medium)
- power=16 → coolDownMax=1  (shortest cooldown, densest)

**How Cooldown Works**:
```cpp
// In tick():
if (result.velocity == 0) {
    // Silence - no gate
} else if (_coolDown == 0) {
    // Cooldown expired - fire gate!
    _coolDown = _coolDownMax;
    gateOutput = true;
} else {
    // Still cooling down - suppress gate
    _coolDown--;
}
```

**Musical Effect for MINIMAL**:
- Since MINIMAL already has burst/silence structure, power affects **within-burst density**
- Low power (0-4): Even bursts have sparse gates (double-sparse!)
- Mid power (5-12): Bursts play most notes
- High power (13-16): Bursts play all notes (ignores cooldown)

**Interaction with ornament**:
- ornament controls: "Which steps in burst should have notes?"
- power controls: "Of those notes, which actually fire gates?"
- Combined effect: `actual_density = (ornament/16) * (power/16)`

---

#### **4. GLIDE (0-100%) - Slide Probability**

**Mapping** (MINIMAL-specific):
```cpp
if (_rng.nextRange(100) < glide/5) {
    result.slide = true;
}
```

**Note**: MINIMAL divides glide by 5 (more conservative than other algorithms)
- glide=0%  → slide probability = 0% (no slides)
- glide=50% → slide probability = 10% (rare slides)
- glide=100% → slide probability = 20% (occasional slides)

**Musical Effect**:
- MINIMAL aesthetic is staccato (short, detached notes)
- Slides are RARE to preserve minimal character
- When slides occur, they add subtle legato connection
- Use sparingly: glide=0-30% for authentic minimal feel

---

#### **5. TRILL (0-100%) - Retrigger Probability**

**Mapping** (MINIMAL-specific):
```cpp
if (_rng.nextRange(100) < trill/10) {
    result.chaos = 100;      // Always fire
    result.polyCount = 3;    // Triplet subdivision
    result.isSpatial = false; // Trill mode (rapid succession)
}
```

**Note**: MINIMAL divides trill by 10 (even more conservative)
- trill=0%  → retrigger probability = 0% (no trills)
- trill=50% → retrigger probability = 5% (very rare)
- trill=100% → retrigger probability = 10% (rare)

**Musical Effect**:
- Trills create "micro-bursts" within the burst
- Triplet subdivision = rapid 3-note roll on single step
- EXTREMELY rare in MINIMAL (preserves minimalist aesthetic)
- Use sparingly: trill=0-20% for subtle texture

---

#### **6. LOOP LENGTH (0-29) - Evolution vs Repetition**

**Mapping**:
- 0 → Infinite evolution (never resets state)
- 1-29 → Finite loop (maps to 1-64 actual steps, resets RNG on loop)

**Musical Effect for MINIMAL**:
- **Infinite (0)**: Algorithm evolves endlessly, never repeats
  - Best for: Long-form ambient, generative sets
  - Burst/silence cycle never loops exactly

- **Finite (1-29)**: Algorithm repeats deterministically
  - Best for: Song sections, A/B variations
  - Burst/silence cycle locks after 1 loop
  - User can rotate/scan the locked pattern

**Recommendation for MINIMAL**:
- Live performance: loopLength=0 (infinite surprise)
- Production/composition: loopLength=8-16 (repeatable phrases)

---

#### **7. GATE LENGTH (0-100%) - Gate Duration Multiplier**

**Mapping**:
```cpp
// MINIMAL generates gateRatio = 25-50%
// User's gateLength scales this:
finalGateLength = (algorithmGateRatio * gateLength) / 50
// gateLength=50% is neutral (1:1 scaling)
```

**Examples**:
- Algorithm says: gateRatio=40% (middle of 25-50% range)
- User sets gateLength=50% → final=40% (neutral)
- User sets gateLength=25% → final=20% (shorter, more staccato)
- User sets gateLength=100% → final=80% (longer, less staccato)

**Musical Effect for MINIMAL**:
- MINIMAL is already staccato by design (25-50% base)
- gateLength <50%: Ultra-minimal (percussive clicks)
- gateLength 50%: As designed (staccato)
- gateLength >50%: Less minimal (notes sustain longer)

**Recommendation**: 25-75% for authentic minimal feel

---

#### **8. GATE OFFSET (0-100%) - Gate Timing Shift**

**Mapping**:
```cpp
// Scaler Model:
// - User Knob 0%   -> Force 0 (Strict/Quantized)
// - User Knob 50%  -> 1x (Original Algo Timing)
// - User Knob 100% -> 2x (Exaggerated Swing)
scaledOffset = (algoOffset * userKnob * 2) / 100;
```

**Musical Effect for MINIMAL**:
- Algorithm generates micro-jitter (0-5%) for organic feel
- User parameter scales this:
  - 0%: Machine-tight minimal (Robot)
  - 50%: Organic minimal (Human)
  - 100%: Loose/sloppy minimal (Drunk)

**Recommendation**: 0-50% depending on desired tightness.

---

#### **9. OCTAVE (-10 to +10) - Global Pitch Shift**

**Mapping**:
```cpp
finalVoltage = scaleToVolts(note, algorithmOctave) + sequence.octave()
```

**Musical Effect**:
- Shifts entire pattern up/down by octaves
- Independent of algorithm logic
- Use for: Bass (-2 to 0), Lead (+1 to +3), High (+4 to +6)

---

#### **10. TRANSPOSE (-11 to +11) - Scale Degree Shift**

**Mapping**:
```cpp
totalDegree = note + (octave * scale.notesPerOctave()) + transpose
voltage = scale.noteToVolts(totalDegree)
```

**Musical Effect**:
- Shifts pattern by scale degrees (not semitones!)
- Example: transpose=+2 in C Major = shifts C→E, D→F, etc.
- Use for: Key changes, modal variations

---

#### **11. SCALE (-1 to Scale::Count-1) - Scale Selection & Quantization**

**Mapping**:
- scale = -1: Use project scale (inherit from global settings)
- scale = 0: Chromatic/free (Semitones = all 12 semitones)
- scale = 1+: Specific scales (Major, Minor, etc.)

**Note:** The `useScale` parameter has been removed (was redundant)

**Options**: Major, Minor, Dorian, Phrygian, Lydian, Mixolydian, etc.

**Musical Effect for MINIMAL**:
- Major/Ionian: Bright, uplifting minimal
- Minor/Aeolian: Dark, hypnotic minimal
- Dorian: Jazzy minimal (Âme, Dixon style)
- Phrygian: Exotic, tense minimal
- Chromatic: Atonal, experimental minimal

---

#### **12. ROOT NOTE (-1 to 11) - Tonic Pitch**

**Mapping**:
- rootNote = -1: Use project root note
- rootNote = 0-11: C, C#, D, ..., B

**Musical Effect**:
- Sets the tonal center (C, C#, D, etc.)
- Affects final pitch offset in scaleToVolts()

---

#### **13. DIVISOR (varies) - Step Rate**

**Mapping**: 1/4, 1/8, 1/16, 1/32, etc.

**Musical Effect for MINIMAL**:
- 1/16: Fast bursts (typical minimal techno)
- 1/8: Medium bursts (groovy minimal)
- 1/32: Rapid bursts (glitchy minimal)

**Recommendation**: 1/16 or 1/32 for authentic minimal

---

#### **BONUS: CV UPDATE MODE (Free/Gated) - CV Behavior**

**Mapping**:
- Free: CV updates every step (even during silence)
- Gated: CV only updates when gate fires

**Musical Effect for MINIMAL**:
- Free: CV "ghost notes" during silence (can modulate other modules)
- Gated: Pure silence during silence phase (traditional behavior)

**Recommendation**: Gated (for true silence between bursts)

---

## Part 5: MINIMAL State Variables

**Required State** (to be added to TuesdayTrackEngine.h):

```cpp
// MINIMAL algorithm state (11) - add to private section
uint8_t _minimal_burstLength;      // 2-8 steps per burst
uint8_t _minimal_silenceLength;    // 4-16 steps per silence
uint8_t _minimal_clickDensity;     // 0-255 (note probability within burst)
uint8_t _minimal_burstTimer;       // Countdown: steps remaining in burst
uint8_t _minimal_silenceTimer;     // Countdown: steps remaining in silence
uint8_t _minimal_noteIndex;        // Current note position (0-20, wraps)
uint8_t _minimal_mode;             // 0=SILENCE, 1=BURST
```

**Memory**: 7 bytes (very compact!)

---

## Part 6: MINIMAL Implementation - Pseudo-Code

### **initAlgorithm() - MINIMAL case**

```cpp
case 11:  // MINIMAL
    // Seed RNGs
    _rng = Random(flowSeed);
    _extraRng = Random(ornamentSeed);

    // Initialize state
    _minimal_burstLength = 2 + (_rng.next() % 7);  // 2-8 steps
    _minimal_silenceLength = 4 + (sequence.flow() % 13);  // 4-16 steps
    _minimal_clickDensity = sequence.ornament() * 16;  // 0-255
    _minimal_mode = 0;  // Start in SILENCE
    _minimal_silenceTimer = _minimal_silenceLength;
    _minimal_burstTimer = 0;
    _minimal_noteIndex = 0;
    break;
```

---

### **generateStep() - MINIMAL case**

```cpp
case 11: {  // MINIMAL
    TuesdayTickResult result;

    // MODE SELECTION STAGE
    if (_minimal_mode == 0) {
        // SILENCE MODE
        result.note = 0;
        result.octave = 0;
        result.velocity = 0;  // MUTE
        result.accent = false;
        result.slide = false;
        result.gateRatio = 0;  // No gate
        result.gateOffset = 0;
        result.chaos = 0;
        result.polyCount = 0;
        result.isSpatial = true;

        // Decrement silence timer
        _minimal_silenceTimer--;
        if (_minimal_silenceTimer == 0) {
            // Transition to BURST mode
            _minimal_mode = 1;
            _minimal_burstTimer = _minimal_burstLength;
            _minimal_noteIndex = 0;
        }
    } else {
        // BURST MODE

        // NOTE GENERATION STAGE
        // Density check
        bool shouldPlay = (_minimal_clickDensity > 0) ?
                          (_rng.nextRange(255) < _minimal_clickDensity) :
                          true;  // ornament=0 → always play

        if (shouldPlay) {
            // Generate note
            result.note = _minimal_noteIndex % 7;  // 0-6 (scale degrees)
            result.octave = (_minimal_noteIndex / 7) % 3;  // 0-2 octaves
            result.velocity = 128 + _rng.nextRange(127);  // 128-255 (always triggers)

            // Staccato gates (25-50%)
            result.gateRatio = 25 + _rng.nextRange(25);

            // ARTICULATION STAGE
            // Rare slides (glide/5)
            result.slide = (_rng.nextRange(100) < (sequence.glide() / 5));

            // Very rare trills (trill/10)
            if (_rng.nextRange(100) < (sequence.trill() / 10)) {
                result.chaos = 100;      // Always fire
                result.polyCount = 3;    // Triplet
                result.isSpatial = false; // Trill (not polyrhythm)
            } else {
                result.chaos = 0;
                result.polyCount = 0;
            }

            result.accent = false;  // MINIMAL doesn't use accents
            // Micro-timing jitter (0-5%)
            result.gateOffset = clamp(int(_rng.nextRange(6)), 0, 100);

            // Advance note index
            _minimal_noteIndex++;
        } else {
            // REST within burst
            result.note = 0;
            result.octave = 0;
            result.velocity = 0;  // MUTE
            result.accent = false;
            result.slide = false;
            result.gateRatio = 0;
            result.gateOffset = 0;
            result.chaos = 0;
            result.polyCount = 0;
            result.isSpatial = true;
        }

        // Decrement burst timer
        _minimal_burstTimer--;
        if (_minimal_burstTimer == 0) {
            // Transition to SILENCE mode
            _minimal_mode = 0;
            _minimal_silenceTimer = _minimal_silenceLength;
            _minimal_noteIndex = 0;  // Reset for next burst
        }
    }

    return result;
}
```

---

## Part 7: Signal Flow - Full Data Path

### **From User Knob to Synthesizer Output**

```
USER TWEAKS KNOB
  ↓
[1. UI LAYER]
  sequence.setFlow(value)  ← User turns Flow knob
  ↓
[2. MODEL LAYER]
  TuesdaySequence._flow = value  ← Stored in model
  ↓
[3. ENGINE DETECTS CHANGE]
  if (_cachedFlow != sequence.flow()) {
      initAlgorithm();  ← Re-seed RNGs, reset state
  }
  ↓
[4. GENERATE STEP]
  generateStep(tick) called every divisor ticks
  ↓
[5. MINIMAL ALGORITHM]
  Mode Selection → Note Generation → Articulation
  ↓
  Returns TuesdayTickResult {
      note: 3,           (scale degree 3 = F in C Major)
      octave: 1,         (octave 1 = middle range)
      velocity: 200,     (will trigger gate)
      slide: false,      (no portamento)
      gateRatio: 40,     (40% gate length)
      chaos: 0,          (no trill)
      ...
  }
  ↓
[6. DENSITY GATING]
  Cooldown check:
    if (velocity > 0 && _coolDown == 0) {
        densityGate = true;
        _coolDown = _coolDownMax;  ← Reset cooldown
    }
  ↓
[7. SCALE QUANTIZATION]
  float volts = scaleToVolts(3, 1)
  ↓
  Scale resolution: C Major, Root=C
  ↓
  totalDegree = 3 + (1 * 7) = 10  (10 degrees from C0)
  ↓
  volts = scale.noteToVolts(10) + rootOffset + octaveOffset
  ↓
  volts = 0.83V  (F in octave 1)
  ↓
[8. GATE SCHEDULING]
  // Scaler Logic:
  scaledOffset = (algoOffset * sequence.gateOffset() * 2) / 100
  gateOffsetTicks = (divisor * scaledOffset) / 100
  gateLengthTicks = (divisor * gateRatio * sequence.gateLength()) / 5000
  ↓
  _microGateQueue.push({ tick + gateOffsetTicks, true, 0.83V })  ← Gate ON
  _microGateQueue.push({ tick + gateOffsetTicks + gateLengthTicks, false, 0.83V })  ← Gate OFF
  ↓
[9. QUEUE PROCESSING]
  Every tick, check micro-gate queue:
    if (tick >= _microGateQueue.front().tick) {
        _gateOutput = _microGateQueue.front().gate;
        _cvOutput = _microGateQueue.front().cvTarget;
    }
  ↓
[10. HARDWARE OUTPUT]
  DAC1 ← 0.83V (pitch CV)
  GATE1 ← HIGH (gate on)
  ↓
SYNTHESIZER PLAYS F NOTE
```

---

## Part 8: Testing Strategy for MINIMAL

### **Unit Tests Required**

```cpp
UNIT_TEST("MINIMAL_Algorithm") {

CASE("mode_transitions") {
    // Verify silence→burst→silence cycle
    sequence.setAlgorithm(11);  // MINIMAL
    sequence.setFlow(8);
    sequence.setOrnament(8);

    initAlgorithm();

    // Should start in SILENCE
    auto result = generateStep(0);
    expectEqual(result.velocity, 0, "starts in silence");

    // Advance through silence (4-16 steps)
    int silenceSteps = 0;
    for (int i = 0; i < 20; i++) {
        result = generateStep(i * divisor);
        if (result.velocity == 0) silenceSteps++;
        else break;
    }
    expectTrue(silenceSteps >= 4 && silenceSteps <= 16, "silence duration correct");

    // Should transition to BURST
    expectTrue(result.velocity > 0, "transitions to burst");
}

CASE("burst_note_range") {
    // Verify notes stay in 0-6 range
    sequence.setAlgorithm(11);
    initAlgorithm();

    for (int i = 0; i < 100; i++) {
        auto result = generateStep(i * divisor);
        if (result.velocity > 0) {
            expectTrue(result.note >= 0 && result.note <= 6, "note in scale degree range");
            expectTrue(result.octave >= 0 && result.octave <= 2, "octave in range");
        }
    }
}

CASE("flow_affects_silence") {
    // Low flow = short silence, high flow = long silence
    sequence.setAlgorithm(11);

    sequence.setFlow(0);
    initAlgorithm();
    int silence0 = _minimal_silenceLength;

    sequence.setFlow(16);
    initAlgorithm();
    int silence16 = _minimal_silenceLength;

    expectTrue(silence0 != silence16, "flow changes silence length");
}

CASE("ornament_affects_density") {
    // Low ornament = sparse, high ornament = dense
    sequence.setAlgorithm(11);

    sequence.setOrnament(0);
    initAlgorithm();
    expectEqual(_minimal_clickDensity, 0, "ornament=0 → density=0");

    sequence.setOrnament(16);
    initAlgorithm();
    expectEqual(_minimal_clickDensity, 255, "ornament=16 → density=255");
}

CASE("power_affects_gate_firing") {
    // Verify cooldown system interacts with MINIMAL
    sequence.setAlgorithm(11);
    sequence.setPower(16);  // High power = dense

    initAlgorithm();
    int gateCount = 0;
    for (int i = 0; i < 100; i++) {
        tick(i);
        if (_gateOutput) gateCount++;
    }

    expectTrue(gateCount > 10, "high power fires more gates");
}

CASE("staccato_gates") {
    // Verify 25-50% gate length
    sequence.setAlgorithm(11);
    initAlgorithm();

    for (int i = 0; i < 100; i++) {
        auto result = generateStep(i * divisor);
        if (result.velocity > 0) {
            expectTrue(result.gateRatio >= 25 && result.gateRatio <= 50, "staccato gates");
        }
    }
}

} // UNIT_TEST
```

---

## Part 9: Parameter Recommendations

### **Preset 1: "Classic Minimal"**
```
algorithm = 11 (MINIMAL)
flow = 8        (balanced silence)
ornament = 12   (fairly dense bursts)
power = 12      (most notes fire)
loopLength = 0  (infinite evolution)
glide = 0%      (no slides, pure staccato)
trill = 0%      (no trills)
gateLength = 50% (neutral)
gateOffset = 0% (quantized)
divisor = 1/16  (standard minimal techno)
useScale = true
scale = Minor
rootNote = C
octave = 0
transpose = 0
```

### **Preset 2: "Ultra-Minimal (Microhouse)"**
```
algorithm = 11
flow = 12       (longer silences)
ornament = 4    (very sparse bursts)
power = 8       (medium gate density)
loopLength = 16 (repeatable 16-step phrase)
glide = 0%
trill = 0%
gateLength = 25% (ultra-staccato clicks)
gateOffset = 0%
divisor = 1/32  (rapid clicks)
useScale = true
scale = Major
rootNote = C
octave = 0
transpose = 0
```

### **Preset 3: "Glitchy Minimal"**
```
algorithm = 11
flow = 4        (rapid cycling)
ornament = 16   (dense bursts)
power = 16      (all notes fire)
loopLength = 0  (infinite)
glide = 30%     (occasional slides)
trill = 20%     (rare triplet rolls)
gateLength = 40% (slightly longer)
gateOffset = 0%
divisor = 1/16
useScale = false (chromatic)
scale = -1
rootNote = -1
octave = +1     (higher register)
transpose = 0
```

---

## Summary

### **MINIMAL Algorithm uses all 13 parameters**:

| Parameter | Usage | Importance |
|-----------|-------|-----------|
| flow | Silence duration + RNG seed | ⭐⭐⭐⭐⭐ |
| ornament | Click density + RNG seed | ⭐⭐⭐⭐⭐ |
| power | Gate firing density (cooldown) | ⭐⭐⭐⭐☆ |
| glide | Slide probability (reduced) | ⭐⭐☆☆☆ |
| trill | Trill probability (reduced) | ⭐⭐☆☆☆ |
| **loopLength** | 0-29 | 0=Infinite, 1-29=Fixed step loop |
| **start** | 0-16 | Start delay in steps |
| gateLength | Gate duration multiplier | ⭐⭐⭐☆☆ |
| gateOffset | Gate timing offset | ⭐☆☆☆☆ |
| scale | Scale selection & quantization | ⭐⭐⭐⭐☆ |
| rootNote | Tonic pitch | ⭐⭐⭐☆☆ |
| octave | Pitch range | ⭐⭐⭐☆☆ |
| transpose | Key transposition | ⭐⭐☆☆☆ |
| divisor | Step rate | ⭐⭐⭐⭐☆ |

**Total integration**: 13/13 parameters used ✅ (removed redundant `useScale`)

---

## Next Steps

1. Implement initAlgorithm() case for MINIMAL
2. Implement generateStep() case for MINIMAL
3. Add 7 state variables to TuesdayTrackEngine.h
4. Write unit tests (6 test cases)
5. Test on hardware
6. Document in Tuesday-Track-Flow.md
7. Repeat process for DRILL and FUNK algorithms

---

**End of MINIMAL Signal Flow Design**
