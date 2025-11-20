# Tuesday Research Document

## Architecture Overview

Tuesday is an algorithmic random sequencer that generates 2 CV outputs and 6 gate outputs using pattern-based and algorithmic approaches. Each tick contains both note and velocity information, creating tightly coupled gate+pitch pairs.

## Algorithm Implementation Patterns

### Core Architecture
- **Tick-based system**: Each tick contains note, velocity, accent, slide, and other parameters
- **Integrated gate+pitch**: Direct relationship between velocity (gate trigger) and note (pitch)
- **Pattern-based**: Pre-generated sequences with algorithmic variations
- **Measure-based timing**: Music-theoretic approach with measure structure

## Three Major Algorithm Implementations

### 1. Markov Chain Algorithm (Algo_Markov)

#### Core Approach
Uses a 3rd-order Markov chain with probabilistic note transitions based on previous notes.

```cpp
// Markov algorithm implementation
class MarkovAlgorithm {
private:
    int matrix[8][8][2];  // 3D transition matrix
    int NoteHistory1, NoteHistory3;  // Previous note tracking
    
public:
    int getNextNote() {
        int idx = Tuesday_BoolChance(R);  // Random choice between transition paths
        int nextNote = matrix[NoteHistory1][NoteHistory3][idx];
        
        // Update history for next iteration
        NoteHistory1 = NoteHistory3;
        NoteHistory3 = nextNote;
        
        return nextNote;
    }
    
    Tick generateTick() {
        Tick tick;
        tick.note = ScaleToNote(getNextNote(), scale);
        tick.vel = Tuesday_Rand(R) / 2 + 40;  // Always generates note (no rests)
        tick.accent = true;  // 100% probability accent
        tick.octave = Tuesday_BoolChance(R) ? 1 : 0;  // Random octave
        
        return tick;
    }
};
```

#### Key Characteristics
- Probabilistic sequence with note-to-note dependency
- Maintains musical context through history
- Always generates notes (no rests)
- 100% accent probability for musical emphasis

### 2. Stomper Algorithm (Algo_Stomper)

#### Core Approach
State machine pattern-based algorithm with 14 different musical actions including slides, pauses, and rhythmic variations.

```cpp
// Stomper algorithm implementation
class StomperAlgorithm {
private:
    enum Action {
        ST_PAUSE = 0, ST_PAUSE2, ST_PAUSE3, ST_HIGH, ST_LOW, 
        ST_SLIDEUP, ST_SLIDEDOWN, ST_HIGH2, ST_LOW2, ST_UP,
        ST_DOWN, ST_UP2, ST_DOWN2, ST_LONG
    };
    
    Action LastMode;      // Current musical action
    int CountDown;        // Rest timer
    int HighNote[2];      // High note choices
    
public:
    Tick generateTick() {
        Tick tick;
        
        if (CountDown > 0) {
            CountDown--;
            tick.vel = 0;  // Rest during countdown
            return tick;
        }
        
        Action currentAction = static_cast<Action>(Tuesday_Rand(R) % 9);
        
        switch(currentAction) {
            case ST_PAUSE:
            case ST_PAUSE2:
            case ST_PAUSE3:
                CountDown = 2 + (Tuesday_Rand(R) % 4);  // Create rest
                tick.vel = 0;
                break;
                
            case ST_HIGH:
            case ST_HIGH2:
                tick.note = HighNote[Tuesday_Rand(R) % 2];
                tick.vel = Tuesday_Rand(R) / 2 + 40;
                break;
                
            case ST_LOW:
            case ST_LOW2:
                tick.note = 0;  // Low note
                tick.vel = Tuesday_Rand(R) / 2 + 40;
                break;
                
            case ST_SLIDEUP:
            case ST_SLIDEDOWN:
                // Implement pitch sliding behavior
                tick.slide = true;
                tick.note = calculateSlideNote();
                tick.vel = Tuesday_Rand(R) / 2 + 40;
                break;
                
            default:
                // Other actions...
                tick.note = Tuesday_Rand(R) % 8;
                tick.vel = Tuesday_Rand(R) / 2 + 40;
                break;
        }
        
        // Apply accent probability
        tick.accent = Tuesday_BoolChance(R);
        
        return tick;
    }
};
```

#### Key Characteristics
- State machine with explicit action states
- Includes rest/pause functionality through countdown
- Built-in slide capabilities
- Dual random generators for different aspects
- Rhythmic complexity with pattern variations

### 3. Scale Walker Algorithm (Algo_ScaleWalker)

#### Core Approach
Deterministic scale walking algorithm that sequentially steps through scale degrees.

```cpp
// ScaleWalker algorithm implementation
class ScaleWalkerAlgorithm {
private:
    int Current;      // Current scale degree position
    int WalkLen;      // Number of scale degrees to cycle through
    
public:
    void initialize() {
        // Calculate walk length based on seed values
        WalkLen = (Seed1 % 8) + 1;  // 1-8 scale degrees
        Current = 0;  // Start from first degree
    }
    
    Tick generateTick() {
        Tick tick;
        
        // Sequential progression through scale
        tick.note = Current;
        tick.vel = Tuesday_Rand(R) / 2 + 40;
        
        // Apply accent probability
        tick.accent = Tuesday_BoolChance(R);
        
        // Advance to next scale degree
        Current = (Current + 1) % WalkLen;
        
        return tick;
    }
    
    void reset() {
        Current = 0;  // Reset to beginning of scale walk
    }
};
```

#### Key Characteristics
- Deterministic sequential scale exploration
- Predictable behavior with minimal randomness
- Guaranteed cycle through specific number of scale degrees
- Simplest implementation with minimal state
- Always generates notes (no rests capability)

## Gate+Pitch Relationship Patterns

### Integrated Architecture
```cpp
// Core Tick structure showing gate+pitch coupling
struct Tuesday_Tick_t {
    signed char note;     // Note value (pitch)
    unsigned char vel;    // Velocity (gate trigger)
    bool accent;          // Accent gate
    bool slide;           // Glide gate
    unsigned char length; // Gate length
    signed char octave;   // Octave offset
};

// Gate generation based on velocity threshold
bool shouldTriggerGate(Tick* tick, int coolDown) {
    return tick->vel >= coolDown;
}
```

### Multi-Gate Output System
Tuesday generates 6 different gate types:
- GATE_GATE: Primary note trigger
- GATE_ACCENT: Accent gate
- GATE_BEAT: Measure beat
- GATE_LOOP: Pattern loop
- GATE_TICK: Individual tick
- GATE_CLOCK: Clock output

## Voltage Generation and Mapping

### Note to Voltage Conversion
```cpp
// Scale to note conversion
int ScaleToNote(int noteIndex, int scale) {
    // Maps semitone values to specific scale degrees
    // Supports various scales (major, minor, dorian, blues, etc.)
    return scalePattern[scale][noteIndex % scaleLength];
}
```

### DAC Voltage Output
```cpp
// Note value to voltage conversion
float noteToVoltage(int noteValue, float baseVoltage) {
    // Convert semitone offset to voltage (typically 1V/octave)
    return baseVoltage + (noteValue * 1.0f / 12.0f);  // 1V/octave standard
}
```

## Timing and Synchronization

### Measure-Based System
```cpp
// Timing structure
class TuesdayTimer {
private:
    int CurrentSubtick;      // Current position in measure
    int MaxSubtickLength;    // Length of measure in subticks
    bool ExternalClock;      // Clock source selection
    
public:
    void updateTiming() {
        CurrentSubtick = (CurrentSubtick + 1) % MaxSubtickLength;
        
        if (CurrentSubtick == 0) {
            // Start of new measure
            generateNewPattern();
        }
    }
};
```

## Key Design Patterns

### 1. Pattern-Based Generation
- Pre-generated sequences with algorithmic variations
- Dithering system for blending between pattern variations
- Measure-based structure for musical coherence

### 2. Integrated Gate+Pitch
- Direct coupling between velocity and pitch
- Each tick contains both gate and pitch information
- No decoupling possible between gate and pitch

### 3. Algorithmic State Management
- Different algorithms maintain different types of state
- Markov: Note history
- Stomper: Current action state and counters
- ScaleWalker: Current position in scale

### 4. Multi-Algo Approach
- Multiple algorithmic approaches in single module
- Each algorithm serves different musical purposes
- Algorithm selection via mode switching

## Code Snippets for Future Modules

### Generic Algorithm Base Class
```cpp
class AlgorithmBase {
public:
    virtual Tick generateTick() = 0;
    virtual void initialize() = 0;
    virtual void reset() = 0;
    virtual ~AlgorithmBase() = default;
};

// Algorithm manager
class AlgorithmManager {
private:
    std::vector<std::unique_ptr<AlgorithmBase>> algorithms;
    int currentAlgorithm;
    
public:
    void setCurrentAlgorithm(int algoIndex) {
        currentAlgorithm = algoIndex;
    }
    
    Tick generateNextTick() {
        return algorithms[currentAlgorithm]->generateTick();
    }
};
```

### State Machine Pattern Implementation
```cpp
template<typename StateType>
class StateMachine {
private:
    StateType currentState;
    int countdown;
    
public:
    void updateState() {
        if (countdown > 0) {
            countdown--;
            return;
        }
        
        // Determine next state based on current state
        currentState = getNextState(currentState);
        resetCountdown();
    }
    
    StateType getCurrentState() const { return currentState; }
    bool isCountdownActive() const { return countdown > 0; }
};
```

### Integrated Gate+Pitch Generator
```cpp
class GatePitchGenerator {
private:
    int coolDown;  // Velocity threshold for gate triggering
    
public:
    struct GatePitchPair {
        float voltage;  // Pitch voltage
        bool gate;      // Gate trigger
        bool accent;    // Accent gate
    };
    
    GatePitchPair generatePair(Tick tick) {
        GatePitchPair output;
        output.voltage = noteToVoltage(tick.note);
        output.gate = tick.vel >= coolDown;
        output.accent = tick.accent;
        
        return output;
    }
};
```