# Mutable Instruments Grids - Drum Pattern Implementation Research

This document summarizes the investigation into how drum patterns are implemented and scanned within the Mutable Instruments Grids module.

## 1. Overview of Drum Pattern Scanning

The drum pattern generation in Grids is based on a system that allows for a wide range of rhythmic variations through interpolation between a set of stored patterns. The core components involved are:

*   **`PatternGenerator` Class:** This static class (`grids/pattern_generator.h`, `grids/pattern_generator.cc`) manages the overall pattern generation, including clocking, step advancement, and evaluating drum or Euclidean patterns.
*   **Two Main Modes:** Grids operates in either `OUTPUT_MODE_DRUMS` or `OUTPUT_MODE_EUCLIDEAN`. This research focuses on the `DRUMS` mode.
*   **X and Y Parameters:** The `DrumsSettings` struct contains `x` and `y` coordinates, along with `randomness`, which define the selection and characteristics of the drum pattern. These typically correspond to the "Map X" and "Map Y" controls on the module.
*   **`Evaluate()` Function:** Driven by an internal or external clock, this function updates internal counters and then calls either `EvaluateDrums()` or `EvaluateEuclidean()` based on the current mode.
*   **`ReadDrumMap()` Function:** This crucial function (in `grids/pattern_generator.cc`) is responsible for retrieving and interpolating pattern data based on the `x` and `y` coordinates.

## 2. How Drum Maps are Organized and What They Contain

At the heart of the drum pattern system is a 2D map of pre-defined rhythmic patterns:

*   **`drum_map`:** A `5x5` array of pointers (`static const prog_uint8_t* drum_map[5][5]`) located in `grids/pattern_generator.cc`. Each element in this 2D array points to one of the 25 stored drum patterns.
*   **`node_` Arrays:** These are the actual drum patterns. There are **25** such arrays, named `node_0` through `node_24`, declared in `grids/resources.h` and their data defined in `grids/resources.cc`.
*   **Structure of a `node_` Array:** Each `node_` array is 96 bytes long (`NODE_x_SIZE = 96`). This size accommodates data for 3 instruments (`kNumParts = 3`) over 32 steps (`kStepsPerPattern = 32`), as `3 * 32 = 96`.
    *   The first 32 bytes typically correspond to Instrument 1 (e.g., Bass Drum).
    *   The next 32 bytes correspond to Instrument 2 (e.g., Snare Drum).
    *   The final 32 bytes correspond to Instrument 3 (e.g., Hi-Hat).
*   **Content of Each Byte:** Each byte (0-255) within a `node_` array represents a "level" or "intensity" for a specific instrument at a specific step. These values are not direct triggers but rather probabilities or thresholds that determine if a trigger will fire.

### Interpolation Process:

1.  **Coordinate Mapping:** The `x` and `y` parameters (0-255) from the `DrumsSettings` are scaled to determine an effective position within the `5x5` `drum_map`.
2.  **Bilinear Interpolation:** The `ReadDrumMap()` function performs bilinear interpolation between the four `node_` arrays that surround the calculated `(x, y)` position. This means it doesn't just pick one pattern but smoothly blends values from the four nearest patterns.
3.  **Level Retrieval:** For each of the three instruments and for each of the 32 steps, `ReadDrumMap()` returns an interpolated "level" (0-255).

### Trigger Generation Process (`EvaluateDrums()`):

1.  **Randomness Application:** For each instrument, a degree of randomness (derived from `settings_.options.drums.randomness`) can be applied to the interpolated level.
2.  **Density Thresholding:** The modified level is then compared against a `threshold` derived from the instrument's `density` setting.
3.  **Trigger Decision:** If the level exceeds the threshold, a trigger is generated for that instrument. A particularly high level can also result in an accent trigger.

The "scanning" of drum patterns, therefore, involves iterating through the 32 steps, and for each step, performing the dynamic interpolation and thresholding based on the module's controls.

## 3. Analysis of Actual Pattern Values

An analysis of all 2400 individual byte values across the 25 `node_` arrays in `grids/resources.cc` revealed a specific distribution, highlighting the design philosophy:

*   **Most Frequent Value: `0` (1493 occurrences / ~62% of data)**
    *   The overwhelming majority of values are `0`. This means that drum patterns are primarily sparse, with many steps designed to be silent. A `0` value indicates that no trigger will be generated for that step, unless density and randomness are extremely high.
*   **Second Most Frequent Value: `255` (97 occurrences / ~4% of data)**
    *   The next most common value is `255`. This represents a "definite hit" or a highly probable trigger. These are the core rhythmic anchors in the patterns, ensuring a strong beat even with low density settings.
*   **Intermediate Values (1-254):**
    *   All other values are significantly less frequent. These intermediate values provide the nuanced "probabilities" or "intensities" that allow the `density` knob to smoothly fill in or thin out the patterns. They create the subtle rhythmic variations and complexity that Grids is known for, transitioning effectively between silence and full triggers.

This distribution confirms that the Grids drum patterns are designed with a balance of strong, foundational beats, and a high degree of sparsity, enabling extensive user control over pattern density and complexity through the interpolation and thresholding mechanisms.

---

## 4. Key Implementation Details (Deep Dive Summary)

Further research into the `grids.cc` and `pattern_generator.cc` files revealed the following key implementation details crucial for replication.

### Clocking, Timing, and Swing

*   **ISR Driven:** All timing is handled by a high-frequency Timer 2 interrupt (`TIMER2_COMPA_vect`), ensuring precision.
*   **Internal 24 PPQN:** The `PatternGenerator` always runs at an internal resolution of 24 PPQN. User-selectable clock resolutions (4, 8, 24 PPQN) are handled by scaling incoming external/MIDI clock ticks *before* they are sent to the generator.
*   **Dual-Purpose "Fill" Knob:** The swing amount is directly proportional to the "Fill" knob's value (`randomness`). When swing is enabled, this knob controls swing, and the "fill" feature is disabled. Swing itself is created by applying a small, alternating positive/negative delay to the internal clock on every off-beat 16th note.
*   **Locked Tap Tempo:** When using Tap Tempo, the clock becomes "locked," ignoring the tempo knob and external clock inputs until it is explicitly unlocked from the settings menu.

### Randomness and Fills

*   **"Fill" via Level Boosting:** The fill effect is not generated by adding new triggers, but by randomly "boosting" the intensity (`level`) of existing steps. A random "perturbation" value is calculated once per pattern for each instrument and is added to the level read from the drum map. This makes it more likely for a step to cross the `density` threshold and fire a trigger.
*   **Efficient RNG:** The random source is a 16-bit Galois Linear Feedback Shift Register (LFSR), which is computationally very cheap and ideal for microcontrollers.

### User Interface and State Management

*   **Modal UI:** Grids uses a modal UI. A long press (~0.5s) on the TAP/RESET button enters a "settings" mode.
*   **Multiplexed Knobs:** In settings mode, the knobs are multiplexed. For example, moving the "BD Density" knob changes the global **Clock Resolution** setting.
*   **Minimal EEPROM Usage:** Only global configuration options (like swing, gate mode, clock resolution) are saved. They are packed into a single byte and stored in EEPROM. The pattern's state (X/Y, densities, fill) is **not** saved and is always determined by the knob positions on power-up.
*   **LEDs for Feedback:** The LEDs change function based on the mode. In settings mode, the top LED is solid, and the other three display the value of the currently selected setting.

---

## 5. Comparison to Other Algorithmic Drum Modules (Zularic Repetitor & Numeric Repetitor)

This section compares the implementation and design philosophy of Grids with two other popular algorithmic drum modules: the Noise Engineering Zularic Repetitor (ZR) and Numeric Repetitor (NR).

### Comparison: Zularic Repetitor (ZR) vs. Numeric Repetitor (NR)

| Feature | Zularic Repetitor (ZR) | Numeric Repetitor (NR) |
| :--- | :--- | :--- |
| **Core Concept** | **Phasing/Time-Shifting:** Creates polyrhythms by playing the same "mother" rhythm at different time offsets. | **Generative Arithmetic:** Creates new rhythms by performing binary integer multiplication on a "prime" rhythm. |
| **Source Patterns** | 30 "mother rhythms" from various musical traditions (African, Funk, etc.). | 32 "prime rhythms" algorithmically selected based on musical criteria (e.g., no consecutive beats). |
| **Main Knobs** | `Mother`: Selects the base pattern. `Child 1-3`: Control the time offset for each of the three child outputs. | `Prime`: Selects the base pattern. `Factor 1-3`: Select the integer to *multiply* the prime pattern with. |
| **Outputs** | 1 "Mother" output (original pattern) and 3 "Child" outputs (time-shifted versions of the mother pattern). | 1 "Prime" output (original pattern) and 3 "Product" outputs (results of the multiplication). |
| **Feel & Result** | Creates complex, evolving polyrhythms that are always related to a central "mother" pulse. The feel is often layered and syncopated. | Creates variations that can be both musically related (multiplication by powers of 2 causes simple rotation) and highly unexpected. The feel is more abstract and mathematical. |

### Inferred Algorithms

#### Zularic Repetitor (ZR) - Phasing Algorithm

The implementation is likely based on pattern lookup and rotation.

1.  **Storage:** A set of 30 patterns is stored, likely as 32-bit integers (e.g., `uint32_t patterns[30];`).
2.  **Clock:** A `step_counter` (0-31) is incremented by the beat clock.
3.  **Pattern Selection:** The `Mother` knob selects an index `mother_idx` into the `patterns` array.
4.  **Offset Selection:** The three `Child` knobs select integer offset values: `offset_1`, `offset_2`, `offset_3`.
5.  **Output Calculation:**
    *   **Mother Output:** The trigger for the current step is read directly from the selected pattern: `(patterns[mother_idx] >> step_counter) & 1`.
    *   **Child Outputs:** For each child, the `step_counter` is offset before reading from the *same* pattern. The modulo operator handles wraparound.
        *   `child_1_step = (step_counter + offset_1) % 32;`
        *   `child_1_output = (patterns[mother_idx] >> child_1_step) & 1;`
        *   *(...and so on for Child 2 and 3.)*

#### Numeric Repetitor (NR) - Multiplicative Algorithm

The implementation is likely based on integer arithmetic.

1.  **Storage:** A set of 32 "prime" patterns is stored as 16-bit integers (`uint16_t primes[32];`).
2.  **Clock:** A `step_counter` (0-15) is incremented by the beat clock.
3.  **Pattern Selection:** The `Prime` knob selects an index `prime_idx`.
4.  **Factor Selection:** The three `Factor` knobs select integer multipliers: `factor_1`, `factor_2`, `factor_3`.
5.  **Output Calculation:**
    *   **Prime Output:** The trigger for the current step is read directly: `(primes[prime_idx] >> step_counter) & 1`.
    *   **Product Outputs:** A new pattern is generated for each output through multiplication. The natural integer overflow is the "wrap around" mechanism.
        *   `product_1_pattern = primes[prime_idx] * factor_1;`
        *   `product_1_output = (product_1_pattern >> step_counter) & 1;`
        *   For Product 2 and 3, a bitwise AND is applied first, as detailed below.

##### A Note on NR's Masking (`&`)

For the Product 2 and 3 outputs, NR performs an additional step before multiplication. It uses a bitwise AND operation with a "mask" to erase parts of the prime rhythm. This is a form of controlled variation.

*   **The Masks in Binary:** The patterns are 16-bit.
    *   **Product 2 Mask (`0x0F0F`):** Becomes `0000111100001111`. This keeps only steps 0-3 and 8-11 of the original pattern.
    *   **Product 3 Mask (`0xF003`):** Becomes `1111000000000011`. This keeps only steps 0-1 and 12-15.
*   **The Musical Effect:** This allows the module to deconstruct a rhythm and create variations on its component parts. For example, Product 1 can play a variation of the full beat, while Product 2 plays a variation of just the middle "snare" section, and Product 3 plays a variation of just the "kick and clap" at the start and end of the bar. This creates a much richer and more compositional output from a single source pattern.
    *   `masked_pattern_2 = primes[prime_idx] & 0x0F0F;`
    *   `product_2_pattern = masked_pattern_2 * factor_2;`
    *   `product_2_output = (product_2_pattern >> step_counter) & 1;`

### Comparison to Mutable Instruments Grids

This is where the fundamental design philosophies become clear. All three are pattern-based drum sequencers, but they manipulate the patterns in completely different ways.

| Feature | Grids | Zularic Repetitor (ZR) | Numeric Repetitor (NR) |
| :--- | :--- | :--- | :--- |
| **Core Method** | **Interpolation** | **Phasing** | **Multiplication** |
| **How it Works** | It finds a "rhythm-space" *between* four neighboring patterns on a 2D map using bilinear interpolation. | It plays one "mother" pattern and three time-shifted copies of it simultaneously. | It takes one "prime" pattern and *generates* three new patterns by multiplying the original as a binary number. |
| **Control Paradigm** | **Exploration:** The user explores a continuous map of rhythms, smoothly morphing between them. The relationship between knob position and rhythm is not always direct. | **Layering/Offsetting:** The user has direct control over the phase relationship between four identical but time-shifted rhythms. | **Generative/Abstract:** The user provides a "seed" (prime) and a "multiplier" (factor) to a mathematical process. The results can be surprising and less intuitive. |
| **Source "DNA"** | 25 patterns derived from statistical analysis of common drum machine beats. | 30 patterns from various world music traditions. | 32 patterns algorithmically chosen for specific mathematical and "musical" properties. |
| **Flexibility** | High. The `Density` and `Fill` controls allow for significant variation on the interpolated pattern. | High, but within the confines of polyrhythms of a single source pattern. Excellent for creating complex but coherent layers. | Potentially infinite, but can be less "traditionally" musical. The masking on channels 2 and 3 adds further controlled variation. |

**In summary:**

*   **Grids is like a map:** You are exploring a landscape of rhythms and finding points between known landmarks.
*   **ZR is like a canon/round:** You are singing the same melody starting at different times.
*   **NR is like a number theory equation:** You are plugging numbers into a formula and seeing what rhythmic theorems it proves.

---

## 6. Determinism in Algorithmic Sequencers

A key question for performance and composition is whether these modules are "random" or simply "complex." If two identical modules are set up identically, will they produce the same result?

**The short answer is: Yes, all three modules are fully deterministic.** Their complexity comes from repeatable mathematics, not true randomness.

### Mutable Instruments Grids

*   **Why it's deterministic:** The interpolation and density thresholding are pure math. The "Fill" knob, while seeming random, uses a pseudo-random number generator (LFSR) that starts from the same state (seed) every single time the module is powered on.
*   **Conclusion:** Since the random number sequence is identical on every power cycle, two Grids modules with the same settings will produce the exact same output, including the "random" fills.

### Zularic Repetitor (ZR)

*   **Why it's deterministic:** The primary mode of operation is pattern lookup and time-shifting (phase). This is basic, repeatable arithmetic.
*   **Conclusion:** In its main mode, the ZR is 100% deterministic. The separate "random gate" mode likely uses a deterministic pseudo-random generator, similar to Grids.

### Numeric Repetitor (NR)

*   **Why it's deterministic:** This module's algorithm is based entirely on integer multiplication and bitwise logic. It does not use a random number generator at all.
*   **Conclusion:** The NR is the most clear-cut case. Its complexity is purely mathematical. `Pattern A * Factor B` will always equal `Product C`. Two NR modules will always behave identically.