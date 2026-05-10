# FractalTrack: Smart Mutation Engine with Evolution

## Design Philosophy

FractalTrack is NOT purely random mutation. Inspired by **Hallucigenia (VCV Rack)** and **Bloom v2**, it combines:
- **Parent-child inheritance** from NoteTrack (scale, root, rhythm)
- **Memory-based mutation** where history influences future
- **Rules-based evolution** with selection pressure
- **Deterministic seeds** for reproducibility

The result: Mutations get *smarter* over time, not purely random.

---

## Hallucigenia Integration (Core Enhancement)

### Source: XTRTN Hallucigenia for VCV Rack

**Key Principles:**
- Mutations have memory of past results (**PREFERRED** - see below)
- Selection pressure favors "successful" mutations (**PREFERRED**)
- Rules constrain mutation types based on context
- Evolution depth controls how much history affects future

### ⚠️ Important: Actual Hallucigenia vs FractalTrack Design

The **actual Hallucigenia module** (Darwinism.cpp source) uses simpler rules:
- **No mutation history** - each mutation is independent
- **Mutation rate** - X% chance each step mutates when triggered
- **Save/Load** - stores entire sequences (16 polyphonic slots)
- **Trigger modes** - Manual, Clocked, CV, or Continuous

**FractalTrack uses history-based mutation (PREFERRED approach):**
- MutationHistory buffer tracks recent mutation results
- SelectionPressure biases future mutations toward successful past results
- EvolutionDepth controls how much history influences future
- This creates "smarter" mutations that learn from patterns

### Hallucigenia Features Added to FractalTrack

| Feature | Description | Implementation |
|---------|-------------|----------------|
| **MutationHistory** | Buffer of last 16 mutation results | Circular buffer with success scoring |
| **SelectionPressure** | Bias toward mutations that worked | Weighted random based on past success |
| **RuleSet** | Constraints on mutation types | Scale-aware, position-aware, context-aware |
| **EvolutionDepth** | 0-100% how much history influences future | User parameter controlling bias strength |
| **SuccessCriteria** | What makes a mutation "good"? | Note in scale, gate fired, velocity in range |

### Hallucigenia-style Simple Mode (Alternative)

For cases where history is not desired, a simpler **Hallucigenia mode** can be enabled:

```cpp
struct HallucigeniaModeRules {
    float mutationRate;      // 0-100%, probability each step mutates
    float density;            // 0-100%, gate survival probability
    bool sampleAndHold;      // Hold note vs continuous
    
    enum class TriggerMode {
        Manual,       // Button press
        Clocked,     // Every N clock pulses
        CV,          // External trigger
        Continuous   // Every step (default)
    };
    TriggerMode triggerMode;
    int clockDivisor;
    
    // What mutates:
    bool mutateNote;         // Randomize note value
    bool mutateGate;         // Randomize gate on/off
    bool mutateVelocity;     // Randomize velocity
};
```

**Toggle**: `MEMORY: ON/OFF` in UI
- ON = History-based mutation (PREFERRED, default)
- OFF = Hallucigenia-style independent mutation

---

## Architecture

### Data Flow with Evolution

```
┌─────────────────────────────────────────────────────────────────┐
│                    NoteTrack (PARENT)                           │
│   note=60, vel=100, gate=ON, len=0.5, scale=major, root=C       │
└─────────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────────┐
│                   FractalTrack (CHILD)                          │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ 1. PARENT DATA                                           │    │
│  │    baseNote=60, baseVel=100, baseGate=ON, baseLen=0.5   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ 2. MUTATION HISTORY CHECK                                 │    │
│  │    recentSuccesses = history.getRecent(8)                │    │
│  │    biasFactor = evolutionDepth * successRate            │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ 3. RULES CONSTRAINTS                                     │    │
│  │    - Scale degree? (don't jump to out-of-scale)         │    │
│  │    - Position in bar? (downbeat = stronger mutation?)    │    │
│  │    - Previous note? (avoid tritones unless intentional)   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ 4. BIASED MUTATION (Hallucigenia-style)                  │    │
│  │    pitchOffset = randomRange(pitchRange)                 │    │
│  │    pitchOffset *= biasFactor  // Lean toward history    │    │
│  │    velocity = clamp(baseVel + randomRange(velRange), 1, 127)    │
│  │    gateSurvives = random() < gateProb                   │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  ┌─────────────────────────────────────────────────────────┐    │
│  │ 5. RECORD TO HISTORY                                     │    │
│  │    history.push(result, evaluateSuccess())              │    │
│  └─────────────────────────────────────────────────────────┘    │
│                              │                                   │
│                              ▼                                   │
│  OUTPUT: mutatedNote=58, vel=85, gate=ON, len=0.4               │
└─────────────────────────────────────────────────────────────────┘
```

### Class Structure with Evolution

```cpp
class FractalTrackEngine {
    // Parent relationship
    NoteTrack* parentTrack;
    uint8_t parentTrackIndex;
    
    // Base mutation parameters
    float pitchRange;       // ±semitones (1-24)
    float velRange;         // ±velocity (1-64)
    float gateProb;        // 0-100%
    float lenRange;        // ±percentage (1-100%)
    
    // Hallucigenia-inspired evolution
    MutationHistory history;
    SelectionPressure pressure;
    RuleSet rules;
    uint8_t evolutionDepth;  // 0-100%
    bool memoryEnabled;
    
    // Branch system (from Bloom v2)
    uint32_t branchSeeds[8];
    uint8_t activeBranch;
    bool cvBranchSelect;
    
    // Seed for reproducibility
    uint32_t baseSeed;
    
public:
    void linkTo(NoteTrack* parent);
    void setEvolutionDepth(uint8_t percent);
    void setRules(RuleSet rules);
    void recordSuccess(uint8_t mutationIndex, bool success);
    void clearHistory();
    
private:
    int8_t biasedMutation(int8_t base, float range);
    bool evaluateSuccess(const Step& result);
    void applyRules(Step& step, const NoteTrack::Step& parent);
};
```

---

## Universal Parent Support

FractalTrack can read from multiple track modes as parent sources, not just NoteTrack.

### Supported Parent Track Modes

| Track Mode | linkData() | getFractalSourceData() | Notes |
|------------|------------|------------------------|-------|
| **NoteTrack** | ✅ Existing | Extend existing | Full support - scale, root, notes, velocity, gate |
| **CurveTrack** | ✅ Existing | Extend existing | Full support - CV values, gate, timing |
| **TuesdayTrack** | ❌ Add new | ❌ Add new | Algo-generated notes, velocity, accent |
| **IndexedTrack** | ❌ Add new | ❌ Add new | Indexed notes, duration, gate |
| **DiscreteMapTrack** | ❌ Add new | ❌ Add new | Stage index → note mapping, ramp phase |
| **TeletypeTrack** | ❌ N/A | ❌ N/A | **Out of scope** - too variable |

### FractalSourceInterface

New interface for uniform parent data access:

```cpp
// In TrackEngine.h
class FractalSourceInterface {
public:
    virtual ~FractalSourceInterface() {}
    
    // Get normalized data for FractalTrack
    virtual FractalSourceData getFractalSourceData() const = 0;
    
    // Check if this track has a sequence to read
    virtual bool hasSequence() const = 0;
    
    // Get scale info for mutation constraints
    virtual const Scale* getScale() const = 0;
    virtual int getRootNote() const = 0;
};
```

### FractalSourceData Structure

```cpp
struct FractalSourceData {
    // Pitch (normalized)
    int8_t noteIndex;        // -63 to +64
    uint8_t scaleDegree;    // 0-6 or 0-11
    float cvVoltage;        // -5V to +5V
    
    // Gate
    bool gate;               // true/false
    uint8_t velocity;        // 0-255
    uint8_t gateLength;     // 0-100%
    
    // Timing
    int currentStep;        // 0 to length-1
    int loopLength;         // pattern length
    float progress;         // 0.0 to 1.0
    
    // Config (for scale-aware mutations)
    int8_t scaleIndex;      // -1 = use project scale
    int8_t rootNote;        // -1 = use project root
    uint32_t divisor;       // step timing
};
```

### Per-Track Implementation Details

#### NoteTrackEngine → Full Support
```cpp
FractalSourceData getFractalSourceData() const override {
    const auto& step = _sequence->step(_currentStep);
    return {
        .noteIndex = step.note(),
        .cvVoltage = scaleToVolts(step.note(), _currentStep),
        .gate = step.gate(),
        .velocity = step.gate() ? 127 : 0,
        .currentStep = _currentStep,
        .loopLength = _sequence->lastStep() - _sequence->firstStep() + 1,
        .progress = sequenceProgress(),
        .scaleIndex = _sequence->scale(),
        .rootNote = _sequence->rootNote(),
        .divisor = _sequence->divisor()
    };
}
```

#### CurveTrackEngine → Full Support
```cpp
FractalSourceData getFractalSourceData() const override {
    return {
        .noteIndex = static_cast<int8_t>(_cvOutput * 12),  // CV → note
        .cvVoltage = _cvOutput,
        .gate = _gateOutput,
        .velocity = _gateOutput ? 127 : 0,
        .currentStep = _currentStep,
        .loopLength = _sequence->length(),
        .progress = sequenceProgress(),
        .scaleIndex = -1,  // Curve is not scale-based
        .rootNote = 0,
        .divisor = _sequence->divisor()
    };
}
```

#### TuesdayTrackEngine → Add Support
```cpp
// Needs: linkData() + current step data from algo
FractalSourceData getFractalSourceData() const override {
    // Get from TuesdaySequence's current step via _stepIndex
    // TuesdayTickResult has: note, octave, velocity, accent, slide
    return {
        .noteIndex = currentNote,
        .cvVoltage = scaleToVolts(currentNote, currentOctave),
        .gate = _gateOutput,
        .velocity = currentVelocity,
        .currentStep = _stepIndex,
        .loopLength = sequence().actualLoopLength(),
        .progress = sequenceProgress(),
        .scaleIndex = sequence().scale(),
        .rootNote = sequence().rootNote(),
        .divisor = sequence().divisor()
    };
}
```

#### IndexedTrackEngine → Add Support
```cpp
FractalSourceData getFractalSourceData() const override {
    const auto& step = _sequence->step(_currentStepIndex);
    return {
        .noteIndex = step.noteIndex(),
        .cvVoltage = noteIndexToVoltage(step.noteIndex()),
        .gate = _gateOutput,
        .velocity = _gateOutput ? 127 : 0,
        .currentStep = _currentStepIndex,
        .loopLength = _sequence->length(),
        .progress = sequenceProgress(),
        .scaleIndex = _indexedTrack.scale(),
        .rootNote = _indexedTrack.rootNote(),
        .divisor = _sequence->divisor()
    };
}
```

#### DiscreteMapTrackEngine → Add Support (Special Case)
```cpp
// DiscreteMap outputs stage index + ramp, not traditional note
FractalSourceData getFractalSourceData() const override {
    // Stage index becomes "note" for fractal mutation
    return {
        .noteIndex = _activeStage,  // Stage = "note" for DiscreteMap
        .cvVoltage = _cvOutput,
        .gate = gateOutput(0),
        .velocity = gateOutput(0) ? 127 : 0,
        .currentStep = _activeStage,
        .loopLength = DiscreteMapSequence::StageCount,
        .progress = _rampPhase,
        .scaleIndex = -1,  // Not scale-based
        .rootNote = 0,
        .divisor = _sequence->divisor()
    };
}
```

### UI: Parent Track Selection

```
FRACTAL TRACK
├── LINK         1           (parent Track 1-8)
│                   │
│                   └── Shows track type icon:
│                       [N] Note   [C] Curve
│                       [A] Algo   [I] Indexed
│                       [D] Disc
├── PARENT INFO              (auto-displayed based on type)
│   ├── TRACK    1-NOTE       (track # + type)
│   ├── STEP     4/16         (current/total)
│   ├── NOTE     C4           (current note if applicable)
│   └── GATE     ON           (gate state)
└── ...
```

### Excluded: TeletypeTrack

TeletypeTrack is **out of scope** for Fractal parent because:
- Output is script-defined, highly variable
- No predictable sequence structure
- External control via Teletype scripts
- Would require special-case handling that complicates design

---

## Mutation Types with Evolution

### 1. Pitch Mutation (with memory)

**Basic:** Random semitone offset within ±pitchRange

**With Hallucigenia:**
```cpp
int8_t biasedMutation(int8_t base, float range) {
    // Get bias from history
    float bias = 0.0f;
    if (memoryEnabled && evolutionDepth > 0) {
        auto recent = history.getRecentPitchOffsets(4);
        bias = pressure.calculateBias(recent);
    }
    
    // Random base mutation
    int8_t offset = random.range(-range, range);
    
    // Apply bias (lean toward successful patterns)
    // evolutionDepth controls strength: 0 = ignore history, 100 = full bias
    offset = lerp(offset, offset * bias, evolutionDepth / 100.0f);
    
    return clamp(base + offset, 0, 127);
}
```

### 2. Velocity Mutation (with memory)

**Basic:** Random velocity offset within ±velRange

**With Hallucigenia:**
```cpp
// Track which velocity offsets led to "musical" results
// (not too quiet, not too loud for the context)
velocity = clamp(parent.vel + biasedOffset(velRange, &history.velOffsets), 1, 127);
```

### 3. Gate Mutation (with probability memory)

**Basic:** Each gate survives with gateProb probability

**With Hallucigenia:**
```cpp
// Gate survival can be influenced by position
// Downbeat gates get higher survival probability
float positionBonus = (stepIndex % 4 == 0) ? 0.2f : 0.0f;
float survivalChance = gateProb + positionBonus;

// Bias toward keeping gates that worked with similar neighbors
if (history.recentGatesMatch(neighborPattern)) {
    survivalChance *= (1.0f + evolutionDepth / 200.0f);
}
```

### 4. Length Mutation (with swing detection)

**Basic:** Percentage variation of step length

**With Hallucigenia:**
```cpp
// Detect if length variation creates swing feel
// If previous mutation created good swing, bias toward similar lengths
float lenOffset = random.range(-lenRange, lenRange);
if (history.swingDetected()) {
    lenOffset *= pressure.swingBias();  // Lean into swing pattern
}
```

---

## MutationHistory Structure

```cpp
struct MutationRecord {
    int8_t pitchOffset;     // -24 to +24
    int8_t velOffset;       // -64 to +64
    bool gateSurvived;     // true/false
    int8_t lenOffset;       // -100 to +100 (percentage)
    uint8_t stepIndex;      // Position in sequence
    bool success;           // User-feedback or automatic evaluation
    uint32_t timestamp;     // For decay/aging
};

class MutationHistory {
    static constexpr uint8_t MAX_RECORDS = 16;
    MutationRecord records[MAX_RECORDS];
    uint8_t head = 0;
    uint8_t count = 0;
    
public:
    void push(const MutationRecord& record);
    MutationRecord getRecent(uint8_t n) const;
    int8_t getAveragePitchOffset() const;
    float getSuccessRate() const;
    void decay(float factor);  // Age old records
    void clear();
};
```

---

## SelectionPressure Structure

```cpp
class SelectionPressure {
    // Track success rate of different mutation types
    float pitchSuccessRate = 0.5f;
    float gateSuccessRate = 0.5f;
    float lengthSuccessRate = 0.5f;
    
public:
    // Called after each mutation to record success
    void record(uint8_t mutationType, bool success);
    
    // Calculate bias factor for future mutations
    // Returns 0.0-2.0: 1.0 = neutral, >1.0 = favor this type, <1.0 = avoid
    float calculateBias(uint8_t mutationType) {
        switch (mutationType) {
            case PITCH: return pitchSuccessRate * 2.0f;
            case GATE:  return gateSuccessRate * 2.0f;
            case LEN:   return lengthSuccessRate * 2.0f;
            default:    return 1.0f;
        }
    }
    
    // Decay old data (mutations fade from memory over time)
    void decay(float factor = 0.95f) {
        pitchSuccessRate = lerp(pitchSuccessRate, 0.5f, factor);
        gateSuccessRate = lerp(gateSuccessRate, 0.5f, factor);
        lengthSuccessRate = lerp(lengthSuccessRate, 0.5f, factor);
    }
};
```

---

## RuleSet for Constraints

```cpp
struct RuleSet {
    // Scale-aware mutations (don't jump out of scale)
    bool scaleConstrained = true;
    
    // Position-aware mutations (downbeats = stronger)
    bool positionAware = true;
    
    // Prevent large leaps (max interval)
    uint8_t maxLeapSemitones = 12;
    
    // Octave constraints (don't jump more than ±2 octaves)
    int8_t maxOctaveJump = 2;
    
    // Repeat prevention (don't repeat same mutation twice)
    bool preventRepeats = true;
    
    // Neighbor awareness (similar context → similar mutation)
    bool neighborAware = true;
};

void applyRules(Step& step, const NoteTrack::Step& parent, const RuleSet& rules) {
    // Scale constraint
    if (rules.scaleConstrained) {
        const Scale& scale = parentTrack->getScale();
        step.note = scale.snapToScale(step.note);
    }
    
    // Leap prevention
    if (rules.maxLeapSemitones > 0) {
        int interval = abs(step.note - parent.note);
        if (interval > rules.maxLeapSemitones) {
            step.note = parent.note + (step.note > parent.note ? 
                         rules.maxLeapSemitones : -rules.maxLeapSemitones);
        }
    }
    
    // Position bonus (downbeats can have more extreme mutations)
    if (rules.positionAware && (stepIndex % 4 == 0)) {
        // Downbeat = allow stronger mutations
    }
}
```

---

## Extended Mutation Rules (Inspired by Generative Systems Research)

Beyond basic constraints, these rules enable more sophisticated child-parent mutation behaviors:

### 1. Markov Chain Transitions

**Concept:** Probability of transitioning from one note to another based on historical patterns.

```
Parent Note:  C → D → E → G
Child inherits transition probabilities:
- C to D: 30%
- C to E: 50%  
- C to G: 20%
- D to E: 40%
- D to G: 60%
```

**Implementation:**
```cpp
struct MarkovTransitionRules {
    // Matrix of note-to-note transition probabilities
    // Based on parent's note sequence history
    float transitionMatrix[12][12];  // semitone → semitone
    
    // Decay older transitions over time
    bool trackHistory;
    uint8_t historyDepth;  // How many steps to remember
    
    // Mode: NOTE (semitone), DEGREE (scale degree), INTERVAL (direction)
    enum class Mode { NOTE, DEGREE, INTERVAL };
    Mode mode;
};
```

### 2. L-System Growth Rules

**Concept:** Apply rewriting rules to parent sequence to generate child variations.

```
Axiom: Parent sequence
Rule 1: A → BC (expand note A into B+C)
Rule 2: B → A (contract B back to A)
Production depth: 1-4 iterations
```

**Implementation:**
```cpp
struct LSystemRules {
    // Production rules
    struct Rule {
        uint8_t source;    // Note/degree to match
        uint8_t target1;    // Replace with this
        uint8_t target2;    // Optionally replace with this
        float probability; // Rule application probability
    };
    
    Rule rules[8];
    uint8_t ruleCount;
    uint8_t maxDepth;      // Recursion depth (1-4)
    bool randomize;        // Apply rules probabilistically
};
```

### 3. Cellular Automata Neighbors

**Concept:** Each step's mutation depends on neighboring steps' values (like Conway's Game of Life for notes).

```
Rule example: "Survive if 2-3 neighbors are active, born if exactly 3 neighbors active"
Applied to: Gate on/off, velocity ranges, note density
```

**Implementation:**
```cpp
struct CellularAutomataRules {
    uint8_t ruleNumber;        // 0-255, standard CA rule
    enum class CellType { 
        GATE,           // Gate on/off
        VELOCITY_BAND,   // Velocity in ranges
        NOTE_DENSITY     // Note spread
    };
    CellType cellType;
    
    uint8_t neighborhood;  // 3 (neighbors), 5 (wider context)
    bool wrapAround;       // Sequence wraps at boundaries
    
    // Pattern state
    uint8_t currentState[16];  // For 16-step windows
};
```

### 4. Fibonacci/Melodic Sequence Rules

**Concept:** Use Fibonacci ratios to constrain melodic intervals.

```
Common Fibonacci intervals: 1, 2, 3, 5, 8, 13 (semitones)
Rule: Allowed intervals = Fibonacci sequence
Deviation: Allow non-Fibonacci but weight them lower
```

**Implementation:**
```cpp
struct FibonacciRules {
    int8_t allowedIntervals[6];  // {1, 2, 3, 5, 8, 13}
    uint8_t intervalCount;
    
    enum class Strictness {
        STRICT,    // Only Fibonacci allowed
        WEIGHTED,  // Fibonacci preferred, others possible
        LOOSE      // Any interval, Fibonacci-biased selection
    };
    Strictness strictness;
    
    // Direction bias
    float ascendWeight;   // Prefer ascending intervals
    float descendWeight;  // Prefer descending intervals
};
```

### 5. Gravitation/Attraction Rules

**Concept:** Notes are "attracted" toward or "repelled" from certain target notes (like gravity wells).

```
Common targets: Root note, fifth, current scale degree
Attraction strength: Distance-based falloff
Repulsion for: Tritone, out-of-scale notes
```

**Implementation:**
```cpp
struct AttractionRules {
    // Gravity wells (attractors)
    struct Attractor {
        int8_t note;           // Target note (-1 = parent note)
        float strength;         // 0.0-1.0
        float falloff;         // Distance falloff rate
        uint8_t radius;         // Max influence distance
    };
    Attractor attractors[4];
    uint8_t attractorCount;
    
    // Repulsors (avoid these)
    int8_t repulsors[4];      // Notes to avoid
    float repulsionStrength;
};
```

### 6. Perlin Noise Field Rules

**Concept:** Apply smooth continuous noise to parameter mutation (creates organic, natural-feeling variations).

```
Use Perlin noise instead of pure random for:
- Pitch drift over time
- Velocity waves
- Gate probability oscillation
- Length variation curves
```

**Implementation:**
```cpp
struct PerlinNoiseRules {
    float frequency;           // Noise frequency (steps per cycle)
    float amplitude;          // Max mutation amount
    float offset;             // Phase offset
    
    enum class Parameter {
        PITCH,
        VELOCITY,
        GATE_PROB,
        LENGTH
    };
    Parameter parameter;
    
    uint32_t seed;            // For reproducible noise
};
```

### 7. Echo/Delay Memory Rules

**Concept:** Child "remembers" parent mutations and echoes them with attenuation.

```
Parent: Note 60 mutated → 62
Child:  Echoes 62 → may mutate toward 62 (diminishing over steps)
Echo decay: Each repetition weaker
Echo delay: 1-4 steps lag
```

**Implementation:**
```cpp
struct EchoMemoryRules {
    bool enabled;
    uint8_t echoCount;       // How many echoes
    float decayRate;         // 0.0-1.0, attenuation per echo
    uint8_t delaySteps;       // Steps between echoes
    
    enum class Target {
        PITCH,               // Echo pitch mutations
        GATE,               // Echo gate patterns
        VELOCITY,            // Echo velocity curves
        ALL                  // Echo everything
    };
    Target target;
};
```

### 8. Symmetry/Mirror Rules

**Concept:** Apply palindrome or mirror transformations to parent sequence.

```
Mirror types:
- Horizontal: Note values mirrored around center
- Vertical: Sequence reversed
- Point: Start/end meet in middle
- Rotary: Rotate by fixed amount then mirror
```

**Implementation:**
```cpp
struct SymmetryRules {
    enum class MirrorType {
        NONE,
        HORIZONTAL,    // Flip pitch contour
        VERTICAL,      // Reverse sequence
        POINT,         // Palindrome (ABCBA)
        ROTARY         // Rotate then mirror
    };
    MirrorType type;
    
    uint8_t rotationOffset;   // For rotary: 0-15 steps
    float symmetryStrength;   // 0.0 = no mirror, 1.0 = full mirror
    
    bool preserveRoot;         // Keep root note in place
};
```

### 9. Tension/Resolution Rules

**Concept:** Track harmonic tension and bias mutations toward/away from resolution.

```
Tension indicators: 
- Dissonant intervals (tritone, minor 2nd)
- Unstable scale degrees (4th, 7th)
- High density of notes

Resolution bias: Mutations tend toward stable notes (1, 3, 5)
```

**Implementation:**
```cpp
struct TensionRules {
    // Tensions scores for each note/interval
    float tensionTable[12];  // 0.0 = stable, 1.0 = tense
    
    float tensionThreshold;   // Start biasing when tension exceeds this
    float tensionBias;       // How strongly to resolve (0.0-1.0)
    
    // Target notes when resolving
    int8_t resolutionTargets[4];  // Typically: root, 3rd, 5th
};
```

---

## UI: Extended Rule Selection

```
EVOLUTION
├── DEPTH    50%          (history influence: 0-100%)
├── MEMORY   ON           (enable mutation memory)
├── RULES    SCALE        (CONSTRAINED/LOOSE/NONE)
│               │
│               └── Expanded RULES menu:
│                   SCALE     - Scale-aware only (default)
│                   MARKOV   - Transition probabilities
│                   LSYSTEM  - L-System growth rules
│                   CA       - Cellular automata
│                   FIBONACCI- Fibonacci intervals
│                   ATTRACT  - Gravity well attraction
│                   PERLIN   - Smooth noise fields
│                   ECHO     - Echo memory
│                   SYMMETRY - Mirror transforms
│                   TENSION  - Resolution bias
│                   COMBINE  - Layer multiple rules
├── MAX.LEAP 12           (max semitone jump)
├── SWING    ON           (detect/enhance swing)
└── ...
```
```

---

## UI Parameters

```
FRACTAL TRACK (linked to NoteTrack 1)
├── LINK         1           (parent NoteTrack 1-8)
├── MUTATION
│   ├── PITCH    ±7          (semitone mutation range)
│   ├── VEL      ±20         (velocity mutation range)
│   ├── GATE     75%         (gate survival probability)
│   ├── LEN      ±25%        (length variation percentage)
│   └── MODE     INTERP      (PITCH/INTERP/GATE/BOTH)
├── EVOLUTION                          [NEW - Hallucigenia]
│   ├── DEPTH    50%          (history influence: 0-100%)
│   ├── MEMORY   ON           (enable mutation memory)
│   ├── RULES    SCALE        (CONSTRAINED/LOOSE/NONE)
│   ├── MAX.LEAP 12           (max semitone jump)
│   ├── SWING    ON           (detect/enhance swing)
│   ├── RESET    [*]          (clear mutation history)
│   └── DECAY    50%          (how fast history fades)
├── BRANCHES                        [NEW - Bloom v2 style]
│   ├── SLOT    0            (branch 0-7)
│   ├── SAVE    [*]          (save current state to branch)
│   ├── LOAD    [*]          (load from branch)
│   ├── COPY    [*]          (copy branch to another)
│   ├── CLEAR   [*]          (clear branch)
│   └── CV.SEL  OFF          (CV input for branch select)
├── BLOOM STYLE                     [NEW - Bloom v2 features]
│   ├── RATCHET OFF          (per-step subdivisions 1-8)
│   ├── R.PROB  50%          (ratchet probability)
│   ├── SLEW    0%           (portamento amount per step)
│   ├── TRILL   OFF          (ornamental rapid notes)
│   └── T.PROB  25%          (trill probability)
└── SEED       12345         (base mutation seed)
```

---

## Comparison Table

| Feature | Bloom v2 | Hallucigenia (VCV) | FractalTrack (XFormer) |
|---------|----------|-------------------|----------------------|
| Parent-child | 3 channels share seed | Module I/O relationships | NoteTrack link |
| Memory | Limited (branches) | Full mutation history | Full history + bias |
| Selection | Manual branch select | Automatic pressure | Automatic + depth control |
| Rules | None | User-defined | Scale-aware, position-aware |
| Determinism | Reseed button | Seed per mutation | Full seed control |
| Branches | 8 saveable | N/A | 8 saveable |
| Per-step mods | Ratchet, mute, slew, trill | N/A | All planned |

---

## Implementation Priority

### Phase 1: Core Evolution (Hallucigenia)
1. [x] MutationHistory buffer (16 records)
2. [x] SelectionPressure tracking
3. [x] Bias-based mutation functions
4. [x] EvolutionDepth parameter

### Phase 2: RuleSet Constraints
1. [x] ScaleConstrained mutations
2. [x] MaxLeapSemitones limit
3. [x] Position-aware bonuses
4. [x] Neighbor awareness

### Phase 3: Bloom v2 Per-Step Features
1. [ ] Per-step ratchet (1-8 subdivisions)
2. [ ] Per-step mute probability
3. [ ] Per-step slew
4. [ ] Per-step trill
5. [ ] Branch system (8 saveable states)

### Phase 4: UI Integration
1. [ ] Evolution page in FractalTrack UI
2. [ ] History visualization
3. [ ] Branch save/load UI
4. [ ] CV branch selection input

---

## Integration Plan: Minimum Touchpoints & Separation of Concerns

FractalTrack follows the **TuesdayTrack pattern** — a new TrackMode with its own model/engine/UI layers, reusing all existing infrastructure (routing, serialization, track management, pattern system).

### What Reuses Existing Infrastructure (ZERO new code)

| Concern | Reused From | How It Works |
|---------|-------------|--------------|
| **Track container** | `Track.h` union + `Container<>` | `Container<NoteTrack, ..., FractalTrack>` stores the mode data |
| **Pattern system** | `CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT` | FractalTrack gets its own sequence array, same pattern count as Tuesday |
| **Track linking** | `Track::_linkTrack`, `TrackEngine::_linkedTrackEngine` | Reuses existing link mechanism; FractalTrack links to parent for source data |
| **Routing** | `Routable<T>`, `Routing::Target`, `Routing::isRouted()` | Fractal params can be CV-routed just like Tuesday params |
| **Serialization** | `VersionedSerializedWriter/Reader` | Same write/read pattern as TuesdaySequence |
| **Engine lifecycle** | `Engine::updateTrackEngines()`, `TrackEngine` base class | Same creation/destruction pattern, same `tick()/update()` contract |
| **UI pages** | `PageManager`, `BasePage` | Same page lifecycle, same key/encoder/led handling |
| **Track setup** | `TrackPage` + ListModels | FractalTrack gets its own ListModel for setup parameters |
| **Accumulator** | Existing accumulator system | Can be reused for modulating mutation parameters |
| **Clipboard** | `ClipBoard` class | Pattern copy/paste works the same way |

### What Needs NEW Code (8 files, ~200 lines each)

#### 1. `model/FractalTrack.h` — Track-level container (like TuesdayTrack.h)

```cpp
#include "FractalSequence.h"

class FractalTrack {
public:
    using FractalSequenceArray = std::array<FractalSequence, 
        CONFIG_PATTERN_COUNT + CONFIG_SNAPSHOT_COUNT>;
    
    Types::PlayMode playMode() const { ... }
    const FractalSequenceArray &sequences() const { return _sequences; }
    
    void clear();
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);
    
private:
    void setTrackIndex(int trackIndex) { 
        _trackIndex = trackIndex;
        for (auto &seq : _sequences) seq.setTrackIndex(trackIndex);
    }
    
    int8_t _trackIndex = -1;
    FractalSequenceArray _sequences;
    Types::PlayMode _playMode = Types::PlayMode::Aligned;
    friend class Track;
};
```

**Scope**: ~80 lines. Minimal — just stores sequences.

#### 2. `model/FractalSequence.h` — Per-pattern params (like TuesdaySequence.h)

```cpp
class FractalSequence {
public:
    // === REUSED from TuesdaySequence pattern ===
    // These params use Routable<T> for CV routing, 
    // same as how TuesdaySequence uses Routable for algorithm/flow/etc
    
    // Parent link
    int parentTrack() const;
    void setParentTrack(int track);  // 0-7, which track to read from
    
    // Mutation ranges (all Routable for CV modulation)
    Routable<int8_t> _pitchRange;      // ±1-24 semitones
    Routable<int8_t> _velRange;        // ±1-127 velocity
    Routable<uint8_t> _gateProb;       // 0-100% gate survival
    Routable<int8_t> _lenRange;        // ±1-100% length variation
    
    // Evolution system (Hallucigenia-inspired, memory-based)
    uint8_t _evolutionDepth;           // 0-100%
    bool _memoryEnabled;               // enable mutation history
    uint8_t _ruleType;                 // SCALE/MARKOV/LSYSTEM/CA/...
    
    // === REUSED from TuesdaySequence exactly ===
    int8_t _scale = -1;                // -1 = project scale
    int8_t _rootNote = -1;             // -1 = project root
    Routable<uint16_t> _divisor;       // clock divisor
    Routable<uint8_t> _clockMultiplier;
    uint8_t _resetMeasure = 8;
    Routable<int8_t> _octave;
    Routable<int8_t> _transpose;
    Routable<uint8_t> _gateLength;
    Routable<uint8_t> _gateOffset;
    Routable<int8_t> _rotate;
    
    // === Bloom v2 per-step features ===
    uint8_t _ratchetCount;             // 0-8
    uint8_t _ratchetProb;              // 0-100%
    uint8_t _slewAmount;               // 0-100%
    bool _trillEnabled;
    uint8_t _trillProb;                // 0-100%
    
    // === Branch system ===
    uint32_t _branchSeeds[8];
    uint8_t _activeBranch;
    bool _cvBranchSelect;
    
    void clear();
    void write(VersionedSerializedWriter &writer) const;
    void read(VersionedSerializedReader &reader);
    
private:
    void setTrackIndex(int index) { _trackIndex = index; }
    int8_t _trackIndex = -1;
    friend class FractalTrack;
};
```

**Scope**: ~120 lines. All params follow the Routable pattern from TuesdaySequence.

#### 3. `engine/FractalTrackEngine.h` — Generation engine (like TuesdayTrackEngine.h)

```cpp
class FractalTrackEngine : public TrackEngine {
public:
    FractalTrackEngine(Engine &engine, const Model &model, Track &track, 
                       const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _fractalTrack(track.fractalTrack())
    { reset(); }
    
    virtual Track::TrackMode trackMode() const override { 
        return Track::TrackMode::Fractal; 
    }
    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;
    
    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return _gateOutput; }
    virtual float cvOutput(int index) const override { return _cvOutput; }
    
    // === REUSED: same linkData() pattern as NoteTrackEngine ===
    // Returns FractalSourceData from parent track
    virtual const TrackLinkData *linkData() const override { return &_linkData; }
    
private:
    FractalTrack &_fractalTrack;
    TrackLinkData _linkData;
    
    // Parent data source
    FractalSourceData _parentSourceData;
    const TrackEngine *_parentEngine = nullptr;
    
    // Mutation state
    MutationHistory _history;
    SelectionPressure _pressure;
    uint8_t _currentRule;
    
    // Output
    bool _activity = false;
    bool _gateOutput = false;
    float _cvOutput = 0.f;
};
```

**Scope**: ~100 lines. Reuses TrackEngine base class, linkData(), and TickResult pattern.

#### 4. `engine/FractalTrackEngine.cpp` — Core generation logic

```cpp
// Three main functions:

TrackEngine::TickResult FractalTrackEngine::tick(uint32_t tick) {
    // 1. REUSED: same muting/pattern/link pattern as other engines
    if (mute()) { ... return TickResult::NoUpdate; }
    
    // 2. Read from parent track via FractalSourceInterface
    updateParentSource();
    
    // 3. Apply mutation rules based on current rule type
    mutateWithRules(_parentSourceData);
    
    // 4. Output CV/gate (REUSED: same voltage scaling as TuesdayTrack)
    _cvOutput = scaleToVolts(mutatedNote, octave);
    _gateOutput = mutatedGate;
}

FractalSourceData FractalTrackEngine::updateParentSource() {
    // Resolve parent engine via Engine's trackEngine array
    _parentEngine = &_engine.trackEngine(parentTrackIndex);
    
    // Each track engine implements FractalSourceInterface
    return _parentEngine->getFractalSourceData();
}
```

**Scope**: ~300 lines. Core logic is new, but output/scaling reuses TuesdayTrack patterns.

#### 5. `Track.h` — Add FractalTrack union member + TrackMode entry

```cpp
// Add to enum:
enum class TrackMode : uint8_t {
    Note, Curve, MidiCv, Tuesday, DiscreteMap, Indexed, Teletype,
    Fractal,  // <-- NEW
    Last
};

// Add to Container:
Container<NoteTrack, CurveTrack, MidiCvTrack, TuesdayTrack, 
          DiscreteMapTrack, IndexedTrack, TeletypeTrack, FractalTrack> _container;

// Add to union:
union {
    ...
    FractalTrack *fractal;
} _track;

// Add accessor:
const FractalTrack &fractalTrack() const { ... }
FractalTrack &fractalTrack() { ... }

// Add to trackModeSerialize: case Fractal: return 7;
// Add to setContainerTrackIndex: case Fractal: _container.as<FractalTrack>().setTrackIndex(...)
// Add to initContainer: case Fractal: _track.fractal = _container.create<FractalTrack>()
```

**Scope**: ~30 lines of additions.

#### 6. `Routing.h` — Add Fractal routing targets

```cpp
// Add after TuesdayLast:
FractalFirst,
ParentTrack = FractalFirst,     // CV-routable parent selection
PitchRange,
VelocityRange,
GateProbability,
LengthRange,
EvolutionDepth,
MemoryEnabled,
RuleType,
RatchetCount,
RatchetProb,
SlewAmount,
TrillEnabled,
TrillProb,
ActiveBranch,
FractalLast = ActiveBranch;
```

**Scope**: ~20 lines. Same pattern as TuesdayFirst/TuesdayLast.

#### 7. `ui/pages/FractalTrackPage.h` — UI for FractalTrack

Follows **TuesdayEditPage** pattern: multi-page BasePage with F1-F4 param slots.

```
Pages: LINK | MUTATION | EVOLUTION | BRANCHES | BLOOM | HISTORY
Params per page: 4 (F1-F4 slots)
Encoder: scroll sections
```

**Scope**: ~300 lines. Reuses BasePage, Canvas, StringBuilder patterns.

#### 8. `ui/model/FractalTrackListModel.h` — TrackPage list model

```cpp
class FractalTrackListModel : public RoutableListModel {
    // REUSED: same pattern as TuesdayTrackListModel
    // Lists: PlayMode, Link, Divisor, ResetMeasure, Scale, RootNote
    
    virtual int rows() const override;
    virtual void edit(int row, int value, bool shift) override;
};
```

**Scope**: ~50 lines. Trivial list model.

### Files Modified (additions only, no behavioral changes to existing code)

| File | Lines Added | What Changes |
|------|------------|--------------|
| `model/Track.h` | ~30 | Add Fractal to TrackMode enum, Container, union |
| `model/Routing.h` | ~20 | Add FractalFirst..FractalLast routing targets |
| `engine/Engine.cpp` | ~3 | Add `case TrackMode::Fractal` to creation switch |
| `model/Project.h` | ~2 | If FractalTrack needs per-project defaults |
| `ui/Pages.h` | ~3 | Register FractalTrackPage in page list |
| `ui/pages/TopPage.cpp` | ~2 | Route to FractalTrackPage on track mode |
| `ui/pages/TrackPage.cpp` | ~3 | Instantiate FractalTrackListModel |
| `ui/Ui.h` | ~2 | Include FractalTrackPage header |

### New Files (self-contained, no external dependencies beyond existing patterns)

| New File | Lines | Purpose |
|----------|-------|---------|
| `model/FractalTrack.h` | ~80 | Track-level container |
| `model/FractalSequence.h` | ~120 | Per-pattern parameters |
| `engine/FractalTrackEngine.h` | ~100 | Engine class definition |
| `engine/FractalTrackEngine.cpp` | ~300 | Mutation + generation logic |
| `ui/pages/FractalTrackPage.h` | ~300 | Multi-section UI page |
| `ui/model/FractalTrackListModel.h` | ~50 | Track setup list |

### Total: ~1000 lines new, ~80 lines modified

### Separation of Concerns Summary

```
┌──────────────────────────────────────────────────────────────────────┐
│  REUSED INFRASTRUCTURE (no changes)                                  │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ Container<> / union            Track::TrackMode management     │  │
│  │ TrackEngine base class          tick/update/reset contract      │  │
│  │ Clock system                    Divisor/multiplier system       │  │
│  │ Routing/Routable<T>             CV modulation of params         │  │
│  │ Serialization                   VersionedSerializedWriter/Reader│  │
│  │ Pattern system                  CONFIG_PATTERN_COUNT etc.       │  │
│  │ Accumulator                     Parameter modulation            │  │
│  │ PageManager / BasePage          Page lifecycle + input handling │  │
│  │ Canvas / StringBuilder          Drawing + printing              │  │
│  │ Leds / KeyEvent                 LED + button system             │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘

┌──────────────────────────────────────────────────────────────────────┐
│  NEW CODE (self-contained per concern)                               │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ MODEL LAYER                                                    │  │
│  │  FractalSequence.h  → Stores per-pattern mutation parameters   │  │
│  │  FractalTrack.h     → Stores sequence array + play mode        │  │
│  │ ENGINE LAYER                                                   │  │
│  │  FractalTrackEngine → Reads parent, applies rules, outputs CV  │  │
│  │ HISTORY & RULES (embedded in engine)                           │  │
│  │  MutationHistory    → Circular buffer of mutation results      │  │
│  │  SelectionPressure  → Success-based bias for future mutations   │  │
│  │  RuleSet variants   → MARKOV/LSYSTEM/CA/FIBONACCI/...          │  │
│  │ UI LAYER                                                       │  │
│  │  FractalTrackPage   → Section-based param editing              │  │
│  │  FractalTrackListModel → TrackPage integration                  │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## References

- **Hallucigenia**: https://library.vcvrack.com/Extratone/Darwinism
- **Bloom v2**: https://www.qubitelectronix.com/shop/p/bloom-v2
- **Bloom v2 Manual**: https://static1.squarespace.com/static/.../Bloom-v2-qs-v1-0.pdf
- **ModWiggler Thread**: https://www.modwiggler.com/forum/viewtopic.php?t=293326
