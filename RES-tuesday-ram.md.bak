# Tuesday Algorithm RAM Optimization Research

## Analysis of Implemented Algorithms in `TuesdayTrackEngine`

### **Current Architecture**

*   **Structure:** The engine is a single monolithic class `TuesdayTrackEngine` that handles all algorithms.
*   **State:** Algorithm-specific state is stored as member variables in the class (`_triB1`, `_markovHistory1`, `_chipChordSeed`, etc.). This means every instance of `TuesdayTrackEngine` allocates memory for *all* algorithms, even though only one runs at a time.
*   **Logic:**
    *   `generateStep(uint32_t tick)` is the core function. It contains a large `switch` statement dispatching to the active algorithm code.
    *   It returns a `TuesdayTickResult` struct (abstract musical intent: note, velocity, gate length).
    *   The `tick()` function handles the timing, "density gating" (cooldown), micro-gate scheduling, and CV/Gate signal generation.
*   **RNG:** Uses two `Random` instances (`_rng`, `_extraRng`) seeded from Flow/Ornament, plus `_uiRng` for UI interactions.

### **Optimization Opportunities**

Here are three distinct optimization strategies, ranging from "Low Effort / High Impact" to "Architectural Refactor".

#### **Option 1: Memory Optimization (Unionize State)**
**Goal:** Reduce RAM usage per track.
**Why:** Currently, the class size grows linearly with every new algorithm added. Since only one algo is active per track, this is wasteful.
**How:** Group algorithm-specific state variables into structs, then put them in a `union`.

*   **Pros:** Immediate RAM savings. Scalable for adding many more algorithms. Minimal logic changes.
*   **Cons:** Requires careful management of initialization (constructors/destructors if structs are non-trivial, though these are mostly PODs). State is lost when switching algorithms (which is usually desired behavior anyway).

**Implementation Sketch:**
```cpp
// In TuesdayTrackEngine.h
struct TriTranceState { int16_t b1, b2, b3; };
struct StomperState { uint8_t mode, countDown, lowNote; uint8_t highNote[2]; int16_t lastNote; uint8_t lastOctave; };
// ... define structs for all algos ...

union AlgorithmState {
    TriTranceState tritrance;
    StomperState stomper;
    MarkovState markov;
    // ...
};

AlgorithmState _algoState;
```

#### **Option 2: Logic Optimization (Strategy Pattern / Dispatch Table)**
**Goal:** Clean up the massive `switch` statement in `generateStep`.
**Why:** The `generateStep` function is becoming a "god method." It's hard to read, maintain, and profile.
**How:**
    *   **Static Dispatch (Templates):** Too complex for this runtime-switchable context.
    *   **Function Pointers (Jump Table):** Create private static helper functions for each algorithm: `generateTriTrance`, `generateStomper`, etc. Store pointers in an array `AlgorithmGenerator generators[]`.
    *   **Polymorphism:** Make an abstract `AlgoGenerator` base class and subclasses.

**Evaluation:**
*   **Virtual methods (Polymorphism):** Adds vtable overhead and heap allocation (or placement new). Probably overkill and slightly slower.
*   **Function Pointers:** Very standard in C/embedded C++. Fast dispatch. Keeps `generateStep` clean.
*   **Private Helpers + Switch:** The simplest refactor. Keeps code in one file but breaks it into chunks. The compiler likely optimizes the switch to a jump table anyway.

**Recommendation:** Stick to **Private Helpers called by a Switch** for now. It balances readability with zero overhead (inlineable).

#### **Option 3: Compute Optimization (Common Sub-expressions)**
**Goal:** Reduce CPU cycles in the `tick` loop.
**Observations:**
*   `scaleToVolts` is called inside `tick` and micro-gate loops. It does scale lookups and float math.
*   `_rng.nextRange(100)` involves division/modulo.
*   **Redundant Calculations:** `divisor`, `loopLength`, `tpb` are re-calculated every tick in `generateStep`.

**How:**
*   **Cache more derivatives:** The `initAlgorithm` or `update` could pre-calculate `stepsPerBeat`, `divisor`, etc. if they only change when parameters change.
*   **Fast RNG:** `Random::nextRange` uses modulo. For power-of-2 ranges (like 256), use bitmasks (`& 0xFF`). The code already does `nextRange(256)` often; ensure `Random` optimizes this or switch to `next() & 0xFF`.
*   **Lookup Tables:** For things like `scaleToVolts`, if the scale is small, a LUT might help, but the current math is simple enough.

### **Proposal: The "Union + Helper" Refactor**

This is the most balanced approach for current tasks.

1.  **Refactor State:** Move all `_triB1`, `_stomperMode`, etc. into a `union` of structs inside `TuesdayTrackEngine.h`. This prepares the ground for adding new algos without bloating the class.
2.  **Refactor Logic:** Extract the logic for `TriTrance`, `Stomper`, etc. from `generateStep` into private methods `generateTriTrance()`, `generateStomper()`.

This keeps the codebase clean, memory-efficient, and easy to debug.
