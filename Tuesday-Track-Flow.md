# Tuesday Track Flow: A Complete Guide (ELI12)

*Explaining the TuesdayTrack system like you're twelve years old*

---

## 🎯 **The Big Picture: What is TuesdayTrack?**

Imagine a **musical robot** that creates melodies for you. You tell it:
- "Be random like this much" (Flow)
- "Add spice like this much" (Ornament)
- "Use this recipe" (Algorithm)

The robot then generates notes that play on your synthesizer. TuesdayTrack is that robot!

---

## 🏗️ **The Three-Layer Cake Architecture**

Think of TuesdayTrack like a three-layer cake, where each layer has a specific job:

```
┌─────────────────────────────────────┐
│         UI LAYER (Top)              │  ← You see and touch this
│  Pages, Knobs, Screens              │
├─────────────────────────────────────┤
│       ENGINE LAYER (Middle)         │  ← The brain that makes music
│  TuesdayTrackEngine                 │
├─────────────────────────────────────┤
│       MODEL LAYER (Bottom)          │  ← Where settings are stored
│  TuesdayTrack, TuesdaySequence      │
└─────────────────────────────────────┘
```

### 📦 **Layer 1: MODEL (The Storage Box)**

**What it does:** Stores all your settings

**Key Classes:**
- `TuesdayTrack` - Your track container (holds 16 patterns)
- `TuesdaySequence` - One pattern's settings

**What it stores:**
```cpp
TuesdaySequence {
    algorithm    // Which recipe (0-20)
    flow         // Randomness amount (1-16)
    ornament     // Spice amount (1-16)
    power        // How many notes (1-16)
    divisor      // Note speed (1/4, 1/8, 1/16, etc.)
    loopLength   // Pattern length (0=infinite, 1-64 steps)
    rotate       // Pattern offset (0-63)
    scale        // Which musical scale (-1=project, 0-15=specific)
    rootNote     // Starting note (-1=project, 0-11=C to B)
    transpose    // Pitch shift (-24 to +24)
    octave       // Octave shift (-4 to +4)
    glide        // Portamento amount (0-100%)
    trill        // Ratchet probability (0-100%)
    gateLength   // Note length (0-100)
    cvUpdateMode // When CV changes (Gated/Free)
}
```

**Think of it like:** A recipe card box where you write down all your settings

---

### 🧠 **Layer 2: ENGINE (The Brain)**

**What it does:** Uses the settings to generate actual notes

**Key Class:** `TuesdayTrackEngine`

**The Magic Process:**

```
┌──────────────────────────────────────────────────────┐
│              EVERY TICK (48 times per beat)          │
├──────────────────────────────────────────────────────┤
│ 1. Check: Is it time for a new step?                │
│    ↓ YES                                              │
│ 2. Generate: Run the algorithm's recipe             │
│    → Algorithm produces TuesdayTickResult            │
│    ↓                                                  │
│ 3. FX Layer: Add polyrhythm/trill effects          │
│    ↓                                                  │
│ 4. Density: Should this note actually play?         │
│    (Power parameter gates notes via cooldown)       │
│    ↓                                                  │
│ 5. Schedule: Put note in micro-gate queue          │
│    ↓                                                  │
│ 6. Output: Send CV and Gate signals                │
└──────────────────────────────────────────────────────┘
```

**The Contract (TuesdayTickResult):**

Every algorithm must fill out this form:

```cpp
struct TuesdayTickResult {
    int note;           // Which scale degree? (0-6 usually)
    int octave;         // Which octave? (-2 to +5)
    uint8_t velocity;   // How loud? (0-255)
    bool accent;        // Special loud note? (yes/no)
    bool slide;         // Glide to next note? (yes/no)
    uint16_t gateRatio; // How long? (0-200% of step)
    uint8_t gateOffset; // When to trigger? (0-100% delay)
    uint8_t chaos;      // Trill chance? (0-100%)
    uint8_t polyCount;  // How many tuplet notes? (0,3,5,7)
    int8_t noteOffsets[8]; // Tuplet pitch offsets
    bool isSpatial;     // Spread out or rapid-fire?
};
```

**Think of it like:** A chef who reads your recipe card and cooks the meal

---

### 🎨 **Layer 3: UI (The Control Panel)**

**What it does:** Shows you what's happening and lets you change things

**Key Classes:**
- `TuesdayEditPage` - Where you edit settings
- `TuesdaySequencePage` - Where you see the pattern playing
- `TuesdayTrackListModel` - List of all your tracks

**What you see:**
- Current algorithm name
- Flow/Ornament knobs
- Power/Speed controls
- Current step indicator
- Scale/Root settings

**Think of it like:** The dashboard of your musical robot

---

## 🎲 **The Dual Random Number System**

This is **super important** for how Tuesday works!

### **Why Two Random Generators?**

Imagine you're throwing dice to make decisions:
- **Red Die (Main RNG)** - For BIG decisions
  - "Should I change the melody now?"
  - "What phrase should I play?"
  - "Is it time to switch modes?"

- **Blue Die (Extra RNG)** - For SMALL details
  - "Which exact note?"
  - "How loud?"
  - "Should I add a slide?"

**Why separate?** So that when you change **Flow** (red die), it doesn't mess up the **Ornament** details (blue die). They stay independent!

### **How Seeds Work:**

```
Flow = 5, Ornament = 12

↓

flowSeed = (5-1) << 4 = 64
ornamentSeed = (12-1) << 4 = 176

↓

_rng = Random(64)           ← Red die
_extraRng = Random(176 + magic_number)  ← Blue die
                    ↑
            Salt to keep them different
```

**Think of it like:** Having two bags of marbles that never get mixed up

---

## 🎵 **The 14 Algorithms Explained**

Each algorithm is a different "recipe" for making music. Let's go through them!

---

### **Algorithm 0: TEST** 🧪

**Purpose:** Testing and debugging scales

**What it does:**
- **Mode 0 (OCTSWEEPS):** Plays root note in different octaves (0,1,2,3,4)
- **Mode 1 (SCALEWALKER):** Walks through all notes in the scale 0→11

**When to use:** When you want to check if your scale sounds right

**Flow:** Controls test mode
**Ornament:** Controls velocity and accents

**Think of it like:** Testing each key on a piano to make sure it works

---

### **Algorithm 1: TRITRANCE** 🌀

**Purpose:** Hypnotic trance patterns with 3-step polyrhythm

**What it does:**
Creates a repeating 3-phase pattern:
1. **Phase 0:** Base note (might mutate)
2. **Phase 1:** Same note, one octave up
3. **Phase 2:** High note with accent (BOOM!)

**The Magic:** Phase 0 has a 25% chance to change the melody!

```
Step:  0   1   2   0   1   2   0   1   2
Phase: ●...●...●...●...●...●...●...●...●
Note:  C → C → E → C → C → E → D → D → E
       ↑           ↑           ↑
     Maybe     Same      MUTATED!
```

**Flow:** Controls phase offset (shifts the 3-step rhythm)
**Ornament:** Controls which notes get chosen (pitch mutations)

**Mutation Pattern:** Sparse (25% chance per cycle)
- Uses `_extraRng.nextBinary() && _extraRng.nextBinary()` = 50% × 50% = 25%

**Think of it like:** A DJ loop that occasionally changes the sample

---

### **Algorithm 2: STOMPER** 🦶

**Purpose:** Heavy techno kicks and acid-style gestures

**What it does:**
State machine with 7 gesture modes:
1. **PAUSE:** Rest (silent)
2. **LOWHI:** Low → High (rising)
3. **LOW:** Just low notes (heavy)
4. **HIGH:** Just low notes (bug preserved for character!)
5. **SLIDEDOWN:** High → Low with slide
6. **SLIDEUP:** Low → High with slide
7. **HILOW:** High → Low (falling)

**The Chaos Factor:** Notes randomize EVERY tick (100% mutation!)

```
Mode:  SLIDEDOWN → PAUSE → LOW → SLIDEUP
Note:  G ↘ C      (rest)   C      C ↗ E
Time:  ——— ••• —— ••••••• —— ——— ••• ——
```

**Flow:** Controls gesture timing and note choices
**Ornament:** Controls which gesture mode and rests

**Mutation Pattern:** Chaotic (100% - notes always change)

**Special Note:** HIGH mode plays LOW notes due to original C bug - kept for "heavy" character!

**Think of it like:** A drummer who makes up patterns on the fly

---

### **Algorithm 3: APHEX** 🎹
*(Original ID: 18, remapped to 3)*

**Purpose:** Complex polyrhythmic layering (3 simultaneous patterns)

**What it does:**
Three independent loops playing at once:
- **Track 1 (4 steps):** Main melody - straight time
- **Track 2 (3 steps):** Modifiers (stutter/slide) - **warped by Ornament parameter!**
- **Track 3 (5 steps):** Bass notes - natural polyrhythm

**Time Warping Magic:**
```
Ornament=4 (straight) → Track 2 runs at normal speed
Ornament=5 (triplets) → Track 2 runs at 3:4 ratio (time warp!)
```

**Flow:** Generates the melodic patterns (what notes?)
**Ornament:** Controls subdivisions (triplets/quintuplets warp Track 2 timing)

**Collision Detection:** When all 3 tracks align → chaos boost!

**Think of it like:** Three musicians playing different time signatures together

---

### **Algorithm 4: AUTECHRE** 🔄
*(Original ID: 19, remapped to 4)*

**Purpose:** Algorithmic pattern transformation (evolving sequences)

**What it does:**
1. Starts with an 8-note pattern
2. Every N beats, applies a transformation rule:
   - **Rule 0:** Rotate pattern left
   - **Rule 1:** Reverse pattern
   - **Rule 2:** Invert intervals
   - **Rule 3:** Swap pairs
   - **Rule 4:** Transpose by intensity

**The Evolution:**
```
Start:   C  E  G  C  E  G  C  E
Rule 0:  E  G  C  E  G  C  E  C  (rotated)
Rule 1:  C  E  C  G  E  C  G  E  (reversed)
```

**Flow:** Controls how often transformations happen (timer)
**Ornament:** Generates the rule sequence and fast tuplets

**Polyrhythm Feature:** Can trigger 3/5/7-tuplets on beat starts

**Think of it like:** A puzzle that rearranges itself while you solve it

---

### **Algorithm 5: STEPWAVE** 🌊
*(Original ID: 20, remapped to 5)*

**Purpose:** Chromatic trills (rapid pitch slides)

**What it does:**
- **Base:** Scale-degree walker (moves through scale)
- **Trill:** Rapid chromatic steps (bypasses scale!)

**Trill Example:**
```
Base Note: C (scale degree 0)
Trill: C → C# → D → D# → E  (chromatic!)
       ▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔▔
       5 gates in one beat
```

**Flow:** Controls direction (-1=down, 0=random, +1=up)
**Ornament:** Controls subdivision count (3/5/7 tuplets)

**Special:** This is the ONLY algorithm that intentionally breaks scale quantization!

**Think of it like:** Sliding your finger up/down a keyboard really fast

---

### **Algorithm 6: MARKOV** 🎰

**Purpose:** Probabilistic melody generation (Markov chain)

**What it does:**
Uses a transition matrix to choose next notes:

```
Current State: (LastNote1, LastNote3)
↓
Look up matrix[LastNote1][LastNote3][coin_flip]
↓
Next Note!
```

**Example:**
```
History: (C, E)
Matrix[0][4] = {G, A}  ← Two possible next notes
Coin flip: Heads → G
Next: G

History: (E, G)
Matrix[4][7] = {C, D}
Coin flip: Tails → D
Next: D
```

**Flow:** Generates the transition matrix (probabilities)
**Ornament:** Not used (seeded at init)

**Mutation Pattern:** State evolves via Markov chain

**Think of it like:** A choose-your-own-adventure book where dice decide the path

---

### **Algorithm 7: CHIPARP 1** 🎮

**Purpose:** Chiptune arpeggios with beat-locked resets

**What it does:**
1. Every beat (TPB): Reset RNG to same seed → chord pattern repeats
2. Within beat: Arpeggiate through chord notes
3. Random direction (up/down)

```
Beat 1: C → E → G → C (up)
Beat 2: C → E → G → C (same seed, same pattern!)
Beat 3: C → G → E → C (flow changed direction)
```

**Flow:** Controls chord base notes and direction changes
**Ornament:** Embedded in chord seed

**Pattern:** Deterministic within beat, mutates between beats

**Think of it like:** A video game arpeggiator from 1985

---

### **Algorithm 8: CHIPARP 2** 🕹️

**Purpose:** Extended chiptune with variable chord lengths

**What it does:**
- Cycles through chord (3-7 notes)
- After each chord cycle, might rest (dead time)
- Occasionally changes chord completely

```
Chord: C E G (len=3)
Play:  C → E → G → C → E → G → [REST] → New Chord!
       ↑———————————↑   ↑———————————↑
       Cycle 1         Cycle 2
```

**Flow:** Controls chord progressions and cycle length
**Ornament:** Not directly used (more flow-based)

**Think of it like:** CHIPARP 1's bigger brother with more variety

---

### **Algorithm 9: WOBBLE** 🌀

**Purpose:** LFO-driven pitch sweeps (wobble bass)

**What it does:**
Two phase accumulators sweep through pitch ranges:
- **Phase 1 (slow):** Low notes
- **Phase 2 (faster):** High notes

**Phase Math:**
```
_wobblePhase += _wobblePhaseSpeed
noteValue = (phase >> 27) & 0x1F  ← Extract top 5 bits (0-31)
scaleDegree = noteValue % 7       ← Wrap to scale
octaveOffset = noteValue / 7      ← Overflow to octaves
```

**Flow:** Controls phase selection probability
**Ornament:** Controls how often high notes play

**Speed:** Automatically adjusts to loop length (breathes with pattern)

**Think of it like:** A slow-motion sine wave choosing your notes

---

### **Algorithm 10: SCALEWALKER** 🚶

**Purpose:** Deterministic scale walking with polyrhythmic gates

**What it does:**
Walks through scale degrees in a direction:
- **Flow ≤ 7:** Walk down
- **Flow = 8:** Random
- **Flow ≥ 9:** Walk up

**Polyrhythm Feature:**
On beat starts, can trigger tuplets (3/5/7 gates) that walk sequentially:

```
Main Step: D (degree 2)
Tuplet (5 gates): D → E → F → G → A
                  2   3   4   5   6
                  ↑_______________↑
                  5 gates spread across beat
```

**Flow:** Controls walk direction
**Ornament:** Controls tuplet count (3/5/7)

**Mutation Pattern:** None! (Linear walker - Pattern D)

**Think of it like:** Climbing stairs, with occasional quick runs

---

## 🎛️ **Parameter Deep Dive**

### **Flow (1-16)** 🌊

**What it controls:**
- RNG seed for main decisions
- Timing mutations (when patterns change)
- Direction/mode transitions
- Transformation timers (AUTECHRE)
- Phase offsets (TRITRANCE)

**Musical Impact:**
- **Low Flow (1-4):** Stable, predictable patterns
- **Mid Flow (7-10):** Balanced variation
- **High Flow (13-16):** Chaotic, constantly evolving

**Think of it like:** How much you shake the dice

---

### **Ornament (1-16)** 🎨

**What it controls:**
- RNG seed for details
- Pitch variations within patterns
- Velocity/accent probability
- Slide/glide amounts
- Polyrhythm subdivisions (3/5/7)

**Musical Impact:**
- **Low Ornament (1-4):** Simple, straight timing (4/4)
- **Ornament 5-8:** Triplet feel (3:4)
- **Ornament 9-12:** Quintuplets (5:4)
- **Ornament 13-16:** Septuplets (7:4)

**Think of it like:** What color paint you use

---

### **Power (1-16)** ⚡

**How it works:** Cooldown timer system

```
Power = 16 → Cooldown = 1  → Every step plays
Power = 8  → Cooldown = 9  → ~50% notes play
Power = 1  → Cooldown = 16 → Sparse notes
```

**Density Gating:**
```cpp
if (velocity / 16 >= cooldown) {
    Note plays!
    cooldown = max
} else {
    Note skipped
    cooldown--
}
```

**Musical Impact:**
- **Low Power:** Sparse, minimal (Minimal Techno)
- **Mid Power:** Balanced groove
- **High Power:** Dense, driving (Acid Techno)

**Think of it like:** How many notes per measure

---

### **Divisor (Speed)** ⏱️

**What it is:** Note duration in clock divisions

**Common Values:**
- `Divisor=1` → Whole notes (192 ticks)
- `Divisor=2` → Half notes (96 ticks)
- `Divisor=4` → Quarter notes (48 ticks)
- `Divisor=8` → Eighth notes (24 ticks)
- `Divisor=16` → Sixteenth notes (12 ticks)

**At 120 BPM:**
- Sixteenth = 125ms
- Eighth = 250ms
- Quarter = 500ms

**Think of it like:** How fast the pattern runs (like a metronome speed)

---

### **Loop Length (0-64)** 🔁

**What it is:** Pattern length in steps

**Special Values:**
- `0` → Infinite loop (no reset)
- `1-64` → Finite loop (resets RNG at loop boundary)

**Loop Reset Behavior:**
```
Tick 0:    initAlgorithm() → Reset RNG seeds
Tick 191:  Last step of 16-step loop
Tick 192:  initAlgorithm() → RESET! Back to start
```

**Musical Impact:**
- **Infinite:** Continuous evolution, never repeats
- **Finite:** Predictable loops, good for songs

**Think of it like:** How many measures before the loop restarts

---

### **Rotate (0-63)** 🔄

**What it does:** Shifts pattern start point

```
Original:  C D E F G A B C
Rotate +2: E F G A B C C D
           ↑
           New start
```

**Implementation:**
```cpp
rotatedStep = (_stepIndex + rotate + loopLength) % loopLength
```

**Musical Impact:** Shifts accents and phrase positions

**Think of it like:** Starting a song from the chorus instead of verse

---

### **Scale (-1 to 15)** 🎼

**What it is:** Which musical scale to quantize notes to

**Special Value:**
- `-1` → Use Project scale (global)
- `0-15` → Specific scale

**Available Scales:**
1. Major (Ionian)
2. Minor (Aeolian)
3. Dorian
4. Blues
5. Pentatonic Major
6. Chromatic (12-tone)
7. Major Triad
8. Minor Triad
9. Egyptian
10. Phrygian
11. Locrian
12. Octatonic
13. Melodic Minor
14. Pentatonic Minor
15-16. Custom

**Scale Quantization:**
```cpp
noteIndex = 3  (scale degree)
scale = Minor {0,2,3,5,7,8,10}  (7 notes)
actualNote = scale[3] = 5 semitones = F
```

**Think of it like:** Which set of piano keys you're allowed to use

---

### **Root Note (-1 to 11)** 🎵

**What it is:** Starting pitch of the scale

**Values:**
- `-1` → Use Project root (global)
- `0` = C
- `1` = C#
- `2` = D
- ...
- `11` = B

**Example:**
```
Scale: Major (C D E F G A B)
Root: 2 (D)
Result: D Major (D E F# G A B C#)
```

**Think of it like:** Which letter the alphabet starts from

---

### **Transpose (-24 to +24)** 🎹

**What it does:** Shifts all notes up/down by semitones (chromatic) or degrees (scale)

**Mode:**
- **Quantized (scale on):** Degrees
  - `transpose = +2` → Up 2 scale steps (C → E in C Major)
- **Chromatic (scale off):** Semitones
  - `transpose = +2` → Up 2 semitones (C → D)

**Think of it like:** Pressing all keys 2 steps to the right

---

### **Octave (-4 to +4)** 🎚️

**What it does:** Shifts all notes by whole octaves (12 semitones)

**Example:**
```
Note: C4
Octave: +1
Result: C5 (+12 semitones, +1V in CV)
```

**Think of it like:** Playing the same notes on a higher/lower piano

---

### **Glide (0-100%)** 🌊

**What it does:** Portamento/slide amount between notes

**Implementation:**
```cpp
if (algorithmSlide && glide > 0) {
    slideTime = 1 + (glide / 30);  // 1-4 range
    cvDelta = (targetCV - currentCV) / slideTime;
    // Smooth slide over multiple ticks
}
```

**Musical Impact:**
- `0%` → Instant pitch changes (stepped)
- `50%` → Medium slide (smooth)
- `100%` → Long slide (slinky)

**Think of it like:** How smoothly you move between notes (like a trombone)

---

### **Trill (0-100%)** 🎲

**What it does:** Probability of ratcheting (multiple fast gates)

**Implementation:**
```cpp
if (algorithm.chaos > 0) {
    int chance = (chaos * trill) / 100;
    if (random(100) < chance) {
        // Fire 2-3 rapid gates instead of 1
    }
}
```

**Example:**
```
Trill=100%, Chaos=50
→ 50% chance of: ♩ → ♬♬ (one note becomes two)
```

**Think of it like:** How often notes stutter/repeat really fast

---

### **Gate Length (1-100)** 📏

**What it does:** Note duration as % of step length

**Implementation:**
```cpp
baseGateLen = (divisor * algorithmGateRatio) / 100;
finalGateLen = (baseGateLen * userGateLength) / 50;
```

**Musical Impact:**
- **Low (10-30):** Staccato (short, plucky)
- **Mid (50):** Normal (balanced)
- **High (80-100):** Legato (long, smooth)
- **>100:** Ties (overlaps next note)

**Think of it like:** How long you hold down a piano key

---

### **CV Update Mode** 📡

**What it is:** When CV output changes

**Modes:**

1. **Gated (default):**
   ```
   CV only changes when Gate is HIGH
   ●——— ●——— ●———
   ↑    ↑    ↑
   CV changes here
   ```

2. **Free:**
   ```
   CV changes every step (even on rests)
   ●——— •——— ●———
   ↑    ↑    ↑
   CV always updates
   ```

**Use Cases:**
- **Gated:** Normal sequencing (pitch per gate)
- **Free:** LFO-like modulation (continuous pitch changes)

**Think of it like:** Whether the note-picker moves during rests

---

## 🔄 **The Complete Data Flow**

Let's trace a single note from settings to output:

```
┌─────────────────────────────────────────────────────┐
│ USER TWEAKS FLOW KNOB TO 12                         │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ MODEL: TuesdaySequence.setFlow(12)                  │
│ Storage updated, cached value invalidated           │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ ENGINE: tick() detects parameter change             │
│ if (_cachedFlow != sequence.flow()) {               │
│     initAlgorithm();  // Re-seed RNGs               │
│ }                                                    │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ INIT: Regenerate algorithm state                    │
│ flowSeed = (12-1) << 4 = 176                        │
│ _rng = Random(176);                                 │
│ _extraRng = Random(ornamentSeed + salt);            │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ TICK: Clock sends tick=192 (beat boundary)          │
│ stepTrigger = (tick % divisor == 0)  → true         │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ GENERATE: Call generateStep(192)                    │
│ switch(algorithm) {                                 │
│   case 1: // TRITRANCE                              │
│     phase = (_stepIndex + _triB2) % 3;              │
│     if (phase == 0) {                               │
│       if (_extraRng.nextBinary() &&                 │
│           _extraRng.nextBinary()) {                 │
│         _triB3 = newNote;  // 25% mutation!         │
│       }                                             │
│       result.note = _triB3 + 4;                     │
│       result.octave = 0;                            │
│     }                                               │
│     ...                                             │
│ }                                                   │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ RESULT: TuesdayTickResult                           │
│ {                                                   │
│   note = 7,        // Scale degree G                │
│   octave = 0,                                       │
│   velocity = 180,                                   │
│   accent = false,                                   │
│   slide = false,                                    │
│   gateRatio = 75,  // 75% gate length               │
│   ...                                               │
│ }                                                   │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ FX LAYER: Check for polyrhythm/trill                │
│ if (ornament >= 5 && isBeatStart) {                 │
│   // Trigger tuplet (skipped in this example)      │
│ }                                                   │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ DENSITY GATING: Should note actually play?          │
│ velDensity = velocity / 16 = 180 / 16 = 11          │
│ if (velDensity >= _coolDown) {                      │
│   densityGate = true;  // Note plays!               │
│   _coolDown = _coolDownMax;  // Reset timer         │
│ }                                                   │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ QUANTIZATION: Convert to voltage                    │
│ volts = scaleToVolts(7, 0)                          │
│   → scale = Minor {0,2,3,5,7,8,10}                  │
│   → note[7 % 7] = 10 semitones = Bb                 │
│   → octave offset = 0                               │
│   → root = C → No shift                             │
│   → transpose = 0 → No shift                        │
│   → final = 10 semitones = 0.833V                   │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ GATE SCHEDULING: Put in micro-queue                 │
│ gateOffsetTicks = (divisor * 0) / 100 = 0           │
│ gateLengthTicks = (divisor * 75 * 50) / 100 / 50    │
│                 = (48 * 75 * 50) / 5000             │
│                 = 36 ticks                          │
│                                                     │
│ _microGateQueue.push({                              │
│   tick: 192 + 0,      // Gate ON now                │
│   gate: true,                                       │
│   cvTarget: 0.833V                                  │
│ });                                                 │
│                                                     │
│ _microGateQueue.push({                              │
│   tick: 192 + 36,     // Gate OFF in 36 ticks       │
│   gate: false,                                      │
│   cvTarget: 0.833V                                  │
│ });                                                 │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ OUTPUT (tick 192): Process queue                    │
│ _gateOutput = true;     → Hardware gate HIGH        │
│ _cvOutput = 0.833V;     → DAC outputs 0.833V        │
│ _activity = true;       → LED lights up             │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ OUTPUT (tick 228): Process queue                    │
│ _gateOutput = false;    → Hardware gate LOW         │
│ _cvOutput = 0.833V;     → CV holds (gated mode)     │
│ _activity = false;      → LED dims                  │
└────────────────┬────────────────────────────────────┘
                 ↓
┌─────────────────────────────────────────────────────┐
│ 🎵 SYNTHESIZER PLAYS Bb NOTE FOR 36 TICKS! 🎵       │
└─────────────────────────────────────────────────────┘
```

---

## 🎚️ **The Micro-Gate Queue System**

### **Why We Need It:**

Old way (immediate gates):
```
Problem: Can't schedule precise polyrhythm timing!

Tick 0:  Generate step → Gate ON immediately
Tick 12: Oops, need 3 gates within this step!
         But we already fired the gate... 😢
```

New way (micro-gate queue):
```
Solution: Schedule ALL gates in advance!

Tick 0:  Generate step
         → Queue: Gate ON at tick 0
         → Queue: Gate OFF at tick 9
         → Queue: Gate ON at tick 12
         → Queue: Gate OFF at tick 21
         → Queue: Gate ON at tick 24
         → Queue: Gate OFF at tick 33

Tick 0:  Process queue → Gate ON
Tick 9:  Process queue → Gate OFF
Tick 12: Process queue → Gate ON (tuplet 2!)
...
```

### **How Polyrhythms Use It:**

**Example: 5-tuplet at 120 BPM**

```
Beat duration: 48 ticks (quarter note)
Tuplet count: 5 gates
Spacing: 48 / 5 = 9.6 ticks per gate
Gate length: 6 ticks (60% of spacing)

Schedule:
┌─────────────────────────────────────────┐
│ Tick  | Gate | CV                       │
├───────┼──────┼──────────────────────────┤
│ 0     | ON   | C  (base note)           │
│ 6     | OFF  | C                        │
│ 9.6   | ON   | D  (offset +1 degree)    │
│ 15.6  | OFF  | D                        │
│ 19.2  | ON   | E  (offset +2 degrees)   │
│ 25.2  | OFF  | E                        │
│ 28.8  | ON   | F  (offset +3 degrees)   │
│ 34.8  | OFF  | F                        │
│ 38.4  | ON   | G  (offset +4 degrees)   │
│ 44.4  | OFF  | G                        │
└───────┴──────┴──────────────────────────┘

Result: C→D→E→F→G played evenly across one beat!
```

**Think of it like:** A calendar app that reminds you exactly when to do things

---

## 🎯 **Polyrhythm vs Trill: The Difference**

### **Polyrhythm (isSpatial = true)**

**What:** Multiple gates spread evenly across 4 beats
**When:** APHEX, AUTECHRE, SCALEWALKER
**Pitch:** Scale degrees (quantized)
**Purpose:** Polymetric layers

```
4-beat window:
●———————•———————•———————•———————
│       │       │       │
5-tuplet spread:
●——•——•——•——•——
C  D  E  F  G

Gates are SPREAD OUT
```

### **Trill (isSpatial = false)**

**What:** Multiple gates fired rapidly in succession
**When:** STEPWAVE (chromatic climbing)
**Pitch:** Semitones (chromatic bypass!)
**Purpose:** Ornamental trills

```
1-beat window:
●———————————————
│
5-tuplet rapid:
●●●●●
C C# D D# E

Gates are BUNCHED UP
```

**Think of it like:**
- **Polyrhythm:** 5 people clapping in a circle
- **Trill:** 1 person clapping really fast

---

## 🔧 **Technical Deep Dives**

### **The Cooldown Density System**

**How notes get filtered:**

```cpp
// Power = 8 (50% density)
_coolDownMax = 17 - power = 9

// Timer counts down:
Step 0: coolDown=9, velocity=200 → 200/16=12 >= 9 → PLAY! Reset to 9
Step 1: coolDown=9, velocity=100 → 100/16=6 < 9  → SKIP! Decrement to 8
Step 2: coolDown=8, velocity=120 → 120/16=7 < 8  → SKIP! Decrement to 7
Step 3: coolDown=7, velocity=150 → 150/16=9 >= 7 → PLAY! Reset to 9
Step 4: coolDown=9, velocity=80  → 80/16=5 < 9   → SKIP! Decrement to 8
...

Result: Notes "beat the clock" based on velocity!
```

**Why this is cool:**
- High velocity notes always play (accents)
- Low velocity notes might skip (dynamics)
- Creates organic-feeling density variations

---

### **The Loop Reset System**

**Finite loops reset RNG at loop boundary:**

```cpp
uint32_t resetDivisor = (loopLength > 0) ?
    (loopLength * divisor) : 0;

uint32_t relativeTick = (resetDivisor == 0) ?
    tick : tick % resetDivisor;

// Loop boundary reset
if (resetDivisor > 0 && relativeTick == 0) {
    initAlgorithm();  // RNGs → same seeds → pattern repeats!
    _stepIndex = 0;
}
```

**Example:**
```
Loop length: 16 steps
Divisor: 3 ticks/step
Reset every: 16 × 3 = 48 ticks

Tick 0:   Init → Pattern A
Tick 47:  Last step of loop
Tick 48:  Init → Pattern A (SAME!)
Tick 96:  Init → Pattern A (SAME!)
```

**Infinite loops never reset:**
```
Loop length: 0 (infinite)
resetDivisor: 0
relativeTick: tick (keeps counting up)

Pattern evolves forever, never repeats!
```

---

### **Scale Quantization Pipeline**

**From algorithm note to voltage:**

```cpp
float scaleToVolts(int noteIndex, int octave) {
    // 1. Resolve scale
    int scaleIdx = sequence.scale();
    const Scale &scale = (scaleIdx < 0) ?
        project.selectedScale() : Scale::get(scaleIdx);

    // 2. Resolve root
    int root = (sequence.rootNote() < 0) ?
        project.rootNote() : sequence.rootNote();

    // 3. Convert scale degree to semitones
    int totalDegree = noteIndex + (octave * scale.notesPerOctave());

    // 4. Apply transpose (in degrees for scale mode)
    totalDegree += sequence.transpose();

    // 5. Look up semitones from scale table
    float voltage = scale.noteToVolts(totalDegree);

    // 6. Add root offset
    voltage += (root / 12.0f);

    // 7. Apply octave offset
    voltage += sequence.octave();

    return voltage;
}
```

**Example:**
```
Input: note=3, octave=1

Scale: Minor {0, 2, 3, 5, 7, 8, 10}
Root: 2 (D)
Transpose: 0
Octave shift: 0

Step 1: totalDegree = 3 + (1 × 7) = 10
Step 2: totalDegree = 10 + 0 = 10
Step 3: semitones = scale[10 % 7] + (10/7) × 12
                  = scale[3] + 1 × 12
                  = 5 + 12
                  = 17 semitones
Step 4: voltage = 17 / 12 = 1.417V
Step 5: voltage = 1.417 + (2/12) = 1.583V
Step 6: voltage = 1.583 + 0 = 1.583V

Output: 1.583V (D# in octave 2)
```

---

## 🎨 **UI Integration Points**

### **How the UI Talks to the Engine:**

```
┌─────────────────────────────────────────┐
│ TuesdayEditPage (UI)                    │
├─────────────────────────────────────────┤
│ User turns Flow knob                    │
│ ↓                                       │
│ sequence.setFlow(newValue)              │
│ ↓                                       │
│ Model updated                           │
│ ↓                                       │
│ Engine detects change on next tick()    │
│ ↓                                       │
│ initAlgorithm() re-seeds                │
└─────────────────────────────────────────┘
```

### **How the Engine Talks to the UI:**

```
┌─────────────────────────────────────────┐
│ TuesdayTrackEngine                      │
├─────────────────────────────────────────┤
│ _displayStep = _stepIndex               │
│ ↓                                       │
│ TuesdaySequencePage::paint()            │
│ ↓                                       │
│ int currentStep = engine.currentStep()  │
│ ↓                                       │
│ Draw indicator at step position         │
└─────────────────────────────────────────┘
```

### **Activity LED:**

```cpp
bool activity() const override {
    return _activity;
}

// Updated in tick():
_activity = _gateOutput || _retriggerArmed;
```

---

## 🐛 **Common Gotchas & Solutions**

### **Gotcha 1: "My pattern changed when I adjusted power!"**

**Why:** Parameter changes invalidate cached values → `initAlgorithm()` re-seeds

**Solution:**
```cpp
// Only re-init if ALGORITHM/FLOW/ORNAMENT change
bool paramsChanged = (_cachedAlgorithm != sequence.algorithm() ||
                     _cachedFlow != sequence.flow() ||
                     _cachedOrnament != sequence.ornament());
// Power/GateLength don't re-init!
```

---

### **Gotcha 2: "My tuplets sound sloppy!"**

**Why:** Floating-point gate timing gets truncated

**Solution:** Micro-gate queue uses integer tick scheduling:
```cpp
uint32_t spacing = windowTicks / tupleN;  // Integer division
for (int i = 0; i < tupleN; i++) {
    uint32_t offset = i * spacing;  // Exact tick offsets
    _microGateQueue.push({tick + offset, true, volts});
}
```

---

### **Gotcha 3: "WOBBLE plays wrong notes!"**

**Why:** Phase accumulator overflows scale size

**Solution (FIXED!):**
```cpp
// OLD BUG:
result.note = (_wobblePhase >> 27) & 0x1F;  // 0-31!

// NEW FIX:
int rawPhase = (_wobblePhase >> 27) & 0x1F;
result.note = rawPhase % 7;      // Wrap to scale
result.octave = rawPhase / 7;    // Overflow
```

---

## 🎓 **Key Takeaways**

1. **Three-Layer Cake:** Model (storage) → Engine (brain) → UI (display)

2. **The Contract:** Every algorithm must fill `TuesdayTickResult` completely

3. **Dual RNG:** Flow (structure) vs Ornament (details) - never mix!

4. **Micro-Gate Queue:** Schedule gates in advance for precise polyrhythm

5. **Density Gating:** Power controls cooldown → velocity "beats the clock"

6. **Loop Reset:** Finite loops reset RNG → predictable patterns

7. **Polyrhythm vs Trill:** Spread vs rapid, quantized vs chromatic

8. **Scale Quantization:** Algorithm → degree → semitone → voltage

9. **Parameter Changes:** Only Flow/Ornament/Algorithm trigger re-init

10. **Mutation Rates:** Sparse (5-15%) vs Chaotic (100%) define character

---

## 🎯 **Quick Reference: Algorithm Cheat Sheet**

| ID | Algorithm | Pattern | Flow | Ornament | Character |
|----|-----------|---------|------|----------|-----------|
| 0  | TEST | D: Debug | Test mode | Velocity | Diagnostic |
| 1  | TRITRANCE | A: Sparse | Phase shift | Pitch mutation | Hypnotic trance |
| 2  | STOMPER | C: Chaotic | Note selection | Gesture mode | Heavy techno |
| 3  | APHEX | A: Poly | Melody | Time warp | Complex poly |
| 4  | AUTECHRE | A: Transform | Transform timer | Rules + tuplets | Evolving patterns |
| 5  | STEPWAVE | C: Chromatic | Direction | Tuplet count | Chromatic trills |
| 6  | MARKOV | A: Markov | Matrix gen | (unused) | Probabilistic |
| 7  | CHIPARP1 | B: Gesture | Chord/direction | (embedded) | Chiptune arp |
| 8  | CHIPARP2 | B: Gesture | Chord progress | (embedded) | Extended chip |
| 9  | WOBBLE | C: LFO | Phase select | High note freq | Wobble bass |
| 10 | SCALEWALKER | D: Linear | Direction | Tuplet count | Walking poly |

**Pattern Key:**
- **A:** Sparse Mutation (5-15%)
- **B:** Gesture State Machine (phrase-based)
- **C:** Range-Biased Random (stateless chaos)
- **D:** Linear Walker (deterministic)

---

## 🎬 **Conclusion**

TuesdayTrack is like a **musical robot with multiple personalities** (algorithms). You control its randomness (Flow), detail (Ornament), and energy (Power), and it generates notes that follow your chosen musical recipe.

The system uses:
- **Smart separation** (Model/Engine/UI)
- **Precise timing** (micro-gate queue)
- **Dual randomness** (Flow/Ornament RNGs)
- **Musical intelligence** (scale quantization, density gating)

To create endless variations of patterns that stay musical but never get boring!

**Remember:** It's not just about random notes - it's about **controlled chaos** that makes interesting music! 🎵

---

*Generated: 2025-12-04*
*Version: Post-Bug-Fix (v1.1)*
*Spec Compliance: 92/100* ✅
