# Advanced Algorithm Designs: Aphex, Ambient, Autechre

This document outlines concrete implementation designs for new, musically effective versions of the Aphex, Ambient, and Autechre algorithms. These designs are based on the principles of purpose-driven, constrained generation, as detailed in `GOOD-ALGO.md`.

The code snippets provided are conceptual and intended for insertion into the `switch` statements within `TuesdayTrackEngine.h` and `TuesdayTrackEngine.cpp`.

---

## Core Engine Mechanics to Consider

Before implementing, it's crucial to understand how the `TuesdayTrackEngine`'s `generateBuffer` function operates.

### 1. The 256-Step "Warmup Loop"
Many algorithms are generative and need time to evolve from a simple starting point into a more complex state. The engine accommodates this by first running a 256-step "warmup" loop.

*   **Purpose:** To advance the algorithm's internal state to a "mature" point without recording any output.
*   **Mechanism:** This loop exclusively updates the algorithm's state variables (e.g., `_markovHistory`, `_ambientLastNote`) and consumes random numbers. It ensures that when the "real" generation begins, the pattern feels like a slice from a continuously evolving sequence.
*   **Implementation Rule:** Any new algorithm with an evolving state **must** have a corresponding `case` in the warmup loop that mirrors the RNG consumption and state changes of its main generation logic to ensure deterministic behavior.

### 2. `Flow` and `Ornament` as Seeds
In this engine, `Flow` and `Ornament` are not direct controls. They are **seeds** for two independent random number generators (`_rng` and `_extraRng`), set once in the `initAlgorithm()` function.

*   **Deterministic Randomness:** For a given algorithm, the same `Flow` and `Ornament` values will *always* produce the exact same pattern. Changing either value by 1 re-seeds the generator and creates a completely new sequence.
*   **Independent Control:** This two-RNG system is powerful. It allows `Flow` to control one aspect of generation (e.g., rhythm) while `Ornament` controls another (e.g., pitch), or vice-versa. This should be leveraged in the algorithm design.

### 3. Gate and Glide Hierarchy
The engine uses a two-layer system for gates and slides: the algorithm proposes a value, and the user's global `glide` setting can override it.

*   **Gate (`gatePercent`):** This is part of an algorithm's core identity. It should be set explicitly inside the generation logic to define the algorithm's intrinsic rhythmic character (e.g., short and plucky, or long and sustained).
*   **Slide (`slide`):** The user's `glide` parameter (0-100%) acts as a master probability control. The common pattern `if (glide > 0 && _rng.nextRange(100) < glide)` allows the user to probabilistically force slides on top of any algorithm's output, adding a valuable layer of performance expression.

---

## 1. Aphex - The "Polyrhythmic Event Sequencer"

### Description

This algorithm emulates Aphex Twin's method of layering multiple, independent rhythmic and melodic ideas that fall in and out of phase. It functions as a "sequencer-within-a-sequencer" where the complexity emerges from the interplay of simple, looping patterns.

*   **Track 1:** A 4-step melodic/acid pattern.
*   **Track 2:** A 3-step polymetric accent/stutter pattern that modifies Track 1.
*   **Track 3:** A 5-step bass or noise pattern that can override the main output.
*   **`Flow`:** Controls the melodic content of the patterns.
*   **`Ornament`:** Controls the starting phase of the loops, creating instant rhythmic variation.
*   **`Power`:** Controls the overall gate density of the combined output.

### Code Structure

#### **1. State Variables (`TuesdayTrackEngine.h`)**

```cpp
// For APHEX - Polyrhythmic Event Sequencer
uint8_t _aphex_track1_pattern[4]; // 4-step melodic pattern
uint8_t _aphex_track2_pattern[3]; // 3-step modifier pattern (0=off, 1=stutter, 2=slide)
uint8_t _aphex_track3_pattern[5]; // 5-step bass/override pattern
uint8_t _aphex_pos1, _aphex_pos2, _aphex_pos3;
```

#### **2. Initialization (`TuesdayTrackEngine.cpp` -> `initAlgorithm`)**

```cpp
case 18: // APHEX - Polyrhythmic Event Sequencer
{
    // 1. Seed the patterns based on Flow, using the main RNG
    _rng = Random((_tuesdayTrack.flow() - 1) << 4);
    for (int i = 0; i < 4; ++i) _aphex_track1_pattern[i] = _rng.next() % 12;
    for (int i = 0; i < 3; ++i) _aphex_track2_pattern[i] = _rng.next() % 3; // 0, 1, or 2
    for (int i = 0; i < 5; ++i) _aphex_track3_pattern[i] = (_rng.next() % 8 == 0) ? (_rng.next() % 5) : 0; // Sparse bass notes

    // 2. Set initial phase based on Ornament
    int ornament = _tuesdayTrack.ornament();
    _aphex_pos1 = (ornament * 1) % 4;
    _aphex_pos2 = (ornament * 2) % 3;
    _aphex_pos3 = (ornament * 3) % 5;
    break;
}
```

#### **3. Generation (`TuesdayTrackEngine.cpp` -> `generateBuffer`)**

```cpp
case 18: // APHEX - Polyrhythmic Event Sequencer
{
    // --- Main melody from Track 1 ---
    note = _aphex_track1_pattern[_aphex_pos1];
    octave = 0;
    gatePercent = 75;
    slide = 0;

    // --- Modification from Track 2 ---
    uint8_t modifier = _aphex_track2_pattern[_aphex_pos2];
    if (modifier == 1) { // Stutter
        gatePercent = 20;
    } else if (modifier == 2) { // Slide
        slide = 1;
    }

    // --- Override from Track 3 ---
    uint8_t bass_note = _aphex_track3_pattern[_aphex_pos3];
    if (bass_note > 0) {
        note = bass_note;
        octave = -1; // Deep bass
        gatePercent = 90;
        slide = 0;
    }

    // --- Advance all tracks ---
    _aphex_pos1 = (_aphex_pos1 + 1) % 4;
    _aphex_pos2 = (_aphex_pos2 + 1) % 3;
    _aphex_pos3 = (_aphex_pos3 + 1) % 5;
}
break;
```

### Implementation Guidance

*   **Warmup Loop:** This algorithm's state is purely its position counters. The warmup loop should simply advance these counters 256 times to ensure the starting phase of the generated buffer reflects the `Ornament` setting correctly. No RNG consumption is needed in the warmup's generation logic.
*   **Randomness:** `Flow` is used intelligently here to seed all the generative content at once. `Ornament` is used not for randomness, but as a deterministic control for the initial phase relationship between the tracks, which is a powerful performance feature.
*   **Gate/Glide:** The algorithm proposes its own `gatePercent` and `slide` values based on its internal logic (stutters, slides, bass hits). This is good. The global `glide` override will still function as expected, allowing the user to add extra slides on top of the programmed ones.

---

## 2. Ambient - The "Harmonic Drone & Event Scheduler"

### Description

This algorithm emulates the common ambient technique of a stable, underlying drone against which sparse, harmonically-related events occur. This creates a sense of atmosphere and musicality through harmonic coherence.

*   **Layer 1 (Drone):** A near-static chord (e.g., root, 5th, 9th) that provides a continuous harmonic bed. The gate is always held open.
*   **Layer 2 (Events):** A sparse "event scheduler" that, after a long countdown, plays a single note or a short arpeggio. These events use notes from the same scale as the drone, ensuring they always sound musically related.
*   **`Flow`:** Determines the root note and chord of the drone.
*   **`Ornament`:** Seeds the random generator that decides the type of sparse event (e.g., single note vs. arpeggio).
*   **`Power`:** Controls the average time between sparse events.

### Code Structure

#### **1. State Variables (`TuesdayTrackEngine.h`)**

```cpp
// For AMBIENT - Harmonic Drone & Event Scheduler
int8_t _ambient_root_note;
int8_t _ambient_drone_notes[3]; // The notes of the drone chord
int _ambient_event_timer;       // Countdown to the next sparse event
uint8_t _ambient_event_type;    // 0=none, 1=single note, 2=arpeggio
uint8_t _ambient_event_step;    // Position within the current event
```

#### **2. Initialization (`TuesdayTrackEngine.cpp` -> `initAlgorithm`)**

```cpp
case 13: // AMBIENT - Harmonic Drone & Event Scheduler
{
    // 1. Set up the drone chord based on Flow. Not random, but deterministic.
    _ambient_root_note = (_tuesdayTrack.flow() - 1) % 12;
    _ambient_drone_notes[0] = _ambient_root_note;
    _ambient_drone_notes[1] = (_ambient_root_note + 7) % 12; // Perfect 5th
    _ambient_drone_notes[2] = (_ambient_root_note + 16) % 12; // A Major 9th (as a 2nd)

    // 2. Init event state using Ornament for randomness
    _extraRng = Random((_tuesdayTrack.ornament() - 1) << 4);
    _ambient_event_timer = 16 + (_extraRng.next() % 48); // First event in 16-64 steps
    _ambient_event_type = 0;
    _ambient_event_step = 0;
    break;
}
```

#### **3. Generation (`TuesdayTrackEngine.cpp` -> `generateBuffer`)**

```cpp
case 13: // AMBIENT - Harmonic Drone & Event Scheduler
{
    // --- Handle ongoing events first ---
    if (_ambient_event_type == 1) { // Single Note Event
        note = (_ambient_root_note + 12) % 12; // Octave up
        octave = 1;
        gatePercent = 150;
        _ambient_event_type = 0; // Event is one step long
    } else if (_ambient_event_type == 2) { // Arpeggio Event
        note = _ambient_drone_notes[_ambient_event_step];
        octave = 0;
        gatePercent = 50;
        _ambient_event_step++;
        if (_ambient_event_step >= 3) {
            _ambient_event_type = 0; // End of event
        }
    } else {
        // --- Default Drone Behavior ---
        note = _ambient_drone_notes[ (step / 4) % 3 ]; // Slowly cycle through drone notes
        octave = 0;
        gatePercent = 1600; // Hold gate for 16 steps
        _ambient_event_timer--;
    }

    // --- Check if it's time for a new event ---
    if (_ambient_event_timer <= 0) {
        _ambient_event_type = 1 + (_extraRng.next() % 2); // 1 or 2
        _ambient_event_step = 0;
        // Reset timer based on Power
        int power = _tuesdayTrack.power();
        _ambient_event_timer = 16 + (power > 0 ? 256 / power : 256);
    }
    
    slide = 0;
}
break;
```

### Implementation Guidance

*   **Warmup Loop:** Critical for this algorithm. The warmup loop must accurately simulate the countdown of `_ambient_event_timer` and the consumption of `_extraRng` when the timer expires. This ensures that the generated buffer starts with the correct event state, making the pattern feel like it has been playing for a while.
*   **Randomness:** `Flow` is used as a direct, deterministic control for harmony, which is good. `Ornament` provides the seed for the `_extraRng`, which is then used to decide *when* and *what* events happen. This is an excellent separation of concerns.
*   **Gate/Glide:** The algorithm's identity is its long `gatePercent = 1600` drone. The sparse events propose their own shorter gates. Setting `slide = 0` internally makes sense for this style. The user can still use the global `glide` setting to add slides if they wish, which provides a useful expressive override.

---

## 3. Autechre - The "Algorithmic Transformation Engine"

### Description

This algorithm emulates Autechre's focus on **process and transformation**. It starts with a simple, looping musical pattern (the "axiom") and continuously applies a sequence of deterministic transformation rules to it. The complexity is emergent, arising from the logical (but often unexpected) evolution of the pattern.

*   **The Pattern:** A simple, 8-step, non-random sequence of notes.
*   **The Rules:** A predefined list of deterministic transformations (e.g., `REVERSE`, `ROTATE`, `INVERT_INTERVALS`).
*   **`Flow`:** Controls the average time between rule applications.
*   **`Ornament`:** Determines the sequence of rules that will be applied.
*   **`Power`:** Influences the intensity of the transformation (e.g., how many steps to rotate by).

### Code Structure

#### **1. State Variables (`TuesdayTrackEngine.h`)**

```cpp
// For AUTECHRE - Algorithmic Transformation Engine
int8_t _autechre_pattern[8];    // The 8-step pattern being transformed
uint8_t _autechre_rule_index;   // Which transformation rule to apply next
int _autechre_rule_timer;       // How long to wait before applying the next rule
uint8_t _autechre_rule_sequence[8]; // The sequence of rules to apply
```

#### **2. Initialization (`TuesdayTrackEngine.cpp` -> `initAlgorithm`)**

```cpp
case 19: // AUTECHRE - Algorithmic Transformation Engine
{
    // 1. Start with a dead-simple, non-random pattern
    for (int i = 0; i < 8; ++i) _autechre_pattern[i] = (i % 4 == 0) ? 0 : 7; // C, G, G, G, C, G, G, G

    // 2. Set rule timing based on Flow
    _autechre_rule_timer = 8 + (_tuesdayTrack.flow() * 4);

    // 3. Create a sequence of rules based on Ornament
    _rng = Random((_tuesdayTrack.ornament() - 1) << 4);
    for (int i = 0; i < 8; ++i) _autechre_rule_sequence[i] = _rng.next() % 5; // 5 different rules
    _autechre_rule_index = 0;
    break;
}
```

#### **3. Generation (`TuesdayTrackEngine.cpp` -> `generateBuffer`)**

```cpp
case 19: // AUTECHRE - Algorithmic Transformation Engine
{
    // --- Always play the current state of the pattern ---
    note = _autechre_pattern[step % 8];
    octave = 0;
    gatePercent = 75;
    slide = 0;

    // --- Countdown to the next transformation ---
    _autechre_rule_timer--;
    if (_autechre_rule_timer <= 0) {
        uint8_t current_rule = _autechre_rule_sequence[_autechre_rule_index];
        int intensity = _tuesdayTrack.power() / 2; // Power: 0-8

        // --- Apply a deterministic transformation rule ---
        switch (current_rule) {
            case 0: // ROTATE
                {
                    int8_t temp = _autechre_pattern[7];
                    for (int i = 7; i > 0; --i) _autechre_pattern[i] = _autechre_pattern[i-1];
                    _autechre_pattern[0] = temp;
                }
                break;
            case 1: // REVERSE
                for (int i = 0; i < 4; ++i) {
                    int8_t temp = _autechre_pattern[i];
                    _autechre_pattern[i] = _autechre_pattern[7-i];
                    _autechre_pattern[7-i] = temp;
                }
                break;
            case 2: // INVERT around a pivot (e.g., note 6)
                for (int i = 0; i < 8; ++i) {
                    int interval = _autechre_pattern[i] - 6;
                    _autechre_pattern[i] = (6 - interval + 12) % 12;
                }
                break;
            case 3: // SWAP adjacent notes
                for (int i = 0; i < 8; i += 2) {
                    int8_t temp = _autechre_pattern[i];
                    _autechre_pattern[i] = _autechre_pattern[i+1];
                    _autechre_pattern[i+1] = temp;
                }
                break;
            case 4: // ADD intensity
                 for (int i = 0; i < 8; ++i) _autechre_pattern[i] = (_autechre_pattern[i] + intensity) % 12;
                break;
        }

        // --- Reset for next rule ---
        _autechre_rule_timer = 8 + (_tuesdayTrack.flow() * 4);
        _autechre_rule_index = (_autechre_rule_index + 1) % 8;
    }
}
break;
```
### Implementation Guidance

*   **Warmup Loop:** This is the most important part to get right. The warmup loop **must** perform the exact same transformations on the `_autechre_pattern` as the main loop. It needs to count down the `_autechre_rule_timer` and apply the correct rule from the `_autechre_rule_sequence` when the timer expires. This will ensure the buffered pattern starts in its fully evolved state.
*   **Randomness:** The use of `Ornament` to seed the *sequence of rules* is perfect. It means the user can discover an interesting evolutionary path and recall it deterministically. `Flow` and `Power` are used as direct, non-random controls for timing and intensity, giving the user fine-grained control over the process.
*   **Gate/Glide:** The algorithm sets a default `gatePercent` and no slide. This is appropriate, as the complexity comes from the note transformations. The global `glide` setting can still be used by the performer to add slides manually, which is a good separation of concerns.
