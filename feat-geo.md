Here is a complete architectural plan to implement the **Just Friends Geode Engine** in software, using 6 internal virtual voices summed to a single physical CV output.

### System Architecture

The system consists of three distinct layers of abstraction:

1. **The Geode Sequencer (Macro Time):** Handles the clock, phase wrapping, and trigger generation.
2. **The Physics Engine (Dynamics):** Calculates the *Velocity* (Amplitude) of each trigger based on the active Mode (Transient, Sustain, Cycle).
3. **The Manifold Generator (Micro Voltage):** Generates the actual slope (Function Generator) for each voice, scaled by the `INTONE` parameter.

---

### Layer 1: The Geode Sequencer (Time)

This layer manages the "Measure" and determines *when* a voice should fire.

**Global State:**

* `MasterPhasor`: A float  representing one musical measure (4 beats).
* `Run_CV`: A global input  that modifies physics.

**Per-Voice State (for 6 Voices):**

* `Divs`: The speed multiplier relative to the measure.
* `Repeats`: The countdown of how many envelopes remain in the current burst.
* `VoicePhase`: The local phase  for the specific polyrhythm.

**Logic:**
Every sample (or tick), update `MasterPhasor`.
For each Voice  from 0 to 5:

1. Increment `VoicePhase[i]` by `Delta_Master * Divs[i]`.
2. **Trigger Event:** If `VoicePhase[i]` wraps past :
* Check if `Repeats[i] > 0` (or if infinite).
* If yes, fire a **Trigger** to Layer 2.
* Decrement `Repeats[i]`.



---

### Layer 2: The Physics Engine (Dynamics)

This is where the "Mode" switch lives. It calculates the **Target Level (Velocity)** for the slope generator. It does *not* generate the slope shape itself, only the destination voltage.

**Inputs:**

* `Mode Switch`: Transient / Sustain / Cycle.
* `Burst Progress`: How far along the burst we are (e.g., Repeat 1 of 8 vs Repeat 8 of 8).
* `Run_CV`: Modifies the physics constant.

**The 3 Modes Logic:**

#### A. Transient Mode (Rhythmic Accent)

* **Goal:** Machine-gun repeats with rhythmic emphasis.
* **Math:**
* If `Run_CV` is low (), `Target_Level = 1.0` (constant).
* If `Run_CV` is high, apply a Euclidean-style modulo mask.
* `Mask = (Steps_Played % (int)(Run_CV * 8) == 0)`
* If Mask is true, `Target_Level = 1.0`; else `Target_Level = 0.3`.



#### B. Sustain Mode (Gravity / Decay)

* **Goal:** A bouncing ball or decaying echo. The burst starts loud and fades to silence.
* **Math:**
* Calculate a damping factor based on `Run_CV`.
* `Damping = 0.05 + (Run_CV * 0.2)`
* `Target_Level = pow(1.0 - Damping, Steps_Played)`
* *Result:* First hit is 1.0, 5th hit is 0.6, 10th hit is 0.2, etc.



#### C. Cycle Mode (Undulation)

* **Goal:** Amplitude Modulation (Tremolo) over the duration of the burst.
* **Math:**
* Calculate an LFO phase based on `Run_CV` (speed) and `Burst_Progress`.
* `LFO_Freq = map(Run_CV, 0.0, 1.0, 1.0, 4.0) * TWO_PI`
* `Target_Level = 0.5 + 0.5 * sin(Burst_Progress * LFO_Freq)`



---

### Layer 3: The Manifold Generator (Voltage & Intone)

This generates the actual CV slope. This is where the 6 voices become distinct "instruments."

**Global Parameters:**

* `TIME`: Base Rise/Fall time.
* `INTONE`: The spread of time across voices.
* `RAMP`: Balance between Rise and Fall (Attack/Decay skew).
* `CURVE`: Shape (Log/Lin/Exp).

**The Intone Math (Crucial):**
You must scale the speed of each voice relative to the base `TIME`.


* If `INTONE` is 1.0, all 6 voices have the exact same envelope duration.
* If `INTONE` is 1.5, Voice 6 is much faster (snappier) than Voice 1.

**Function Generator Logic (Per Voice):**

1. **State:** `CurrentVoltage`, `State` (Idle, Rising, Falling).
2. **On Trigger (from Layer 2):**
* Set `State = Rising`.
* Set `GoalVoltage = Target_Level` (calculated in Layer 2).


3. **Process:**
* Slew `CurrentVoltage` towards `GoalVoltage` using the `Speed_i` calculated above.
* Apply `CURVE` shaping to the output.



---

### Layer 4: The Mixer (Output)

Finally, sum the 6 internal streams to your single physical output.

* **Result:** A single CV output that contains the "constructive interference" of 6 independent polymetric rhythms and 6 independent slope gradients.

### Implementation Checklist

1. [ ] **Class `GeodeVoice**`: Member variables for phase, repeats, divisions.
2. [ ] **Class `SlopeGen**`: Standard AR envelope logic with variable Rate and Target.
3. [ ] **`MasterClock`**: A simple accumulator that wraps at 1.0.
4. [ ] **`PhysicsSolver`**: A function `float get_velocity(mode, run, step_index)` that returns the target level.
5. [ ] **Main Audio Loop**:
* Tick Master Clock.
* Loop 0..5 Voices -> Check Phase -> (If Wrap) -> Call PhysicsSolver -> Trigger SlopeGen.
* Update SlopeGen with `INTONE` scaled rates.
* Sum outputs -> Write to DAC.
# Feature: Just Friends "Geode" Engine

This feature implements a virtual control voltage generation engine modeled after the "Geode" concept—inspired by the Mannequins Just Friends module—within the Performer/Teletype environment.

It simulates 6 internal "voices" that generate envelopes/LFOs based on polyrhythmic relationships to the main clock, shaping them with "Physics" (Transient, Sustain, Cycle modes) and mixing them to a single output.

## Functional Description

The Geode Engine operates as a subsystem of the `TeletypeTrackEngine`. It does not consume physical track resources but offers a complex modulation source accessible via Teletype scripts. The output is a control signal (CV) intended to modulate other parameters (pitch, filter, etc.) or be output directly to a CV jack.

### 1. The Geode Sequencer (Time)
*   **Driven by Transport:** The engine is phase-locked to Performer's transport.
*   **6 Voices:** Each voice has a `DIV` (Divisor) and `REP` (Repeats) setting.
*   **Polyrhythms:** Voice 1 might fire every measure (Div 1), while Voice 6 fires 7 times a measure (Div 7).
*   **Phasor Logic:** `VoicePhase = (MasterMeasurePhase * Divisor) % 1.0`. A trigger is generated when the phase wraps.

### 2. The Physics Engine (Dynamics)
Determines the *amplitude/velocity* of each trigger based on the global `MODE` and `RUN` parameter.

*   **Mode 0: Transient (Rhythmic Accent)**
    *   High `RUN` values apply a Euclidean-style mask, silencing steps to create rhythms.
*   **Mode 1: Sustain (Gravity)**
    *   Triggers decay in amplitude over the course of a burst. `RUN` sets the decay rate (gravity).
*   **Mode 2: Cycle (Undulation)**
    *   Amplitude modulates via a sine LFO over the burst. `RUN` sets the LFO speed.

### 3. The Manifold (Shaping)
Generates the actual CV envelope (Attack/Decay) for each voice.

*   **Global Params:**
    *   **TIME:** Base duration of the envelopes.
    *   **INTONE:** Spreads the time across voices. High intone makes high-index voices faster (snappy) and low-index voices slower (swells).
    *   **RAMP:** Skews Attack vs. Decay.
    *   **CURVE:** Bends the slope (Log to Exp).

### 4. Output
*   **Summed Mix:** All 6 active voices are summed to a single normalized output (0.0 - 1.0).
*   **Teletype Access:** Scripts can read this value (`GEO.VAL`) and assign it to physical CV outputs (`CV 1 GEO.VAL`).

---
Here is the comprehensive implementation plan for the **Just Friends Geode Engine** in software.

This plan distinguishes between **Real-Time Performance Parameters** (the knobs you tweak while playing) and **Configuration Parameters** (the rhythmic structure you program).

---

### I. The User Interface (Parameters)

You need to expose two distinct sets of controls to the user.

#### A. The Global Performance Controls (7 Parameters)

These control the "Timbre" and "Physics" of the entire system. They affect all 6 voices simultaneously.

1. **TIME (`0.0` to `1.0`)**
* **Label:** Time / Rate
* **Function:** Sets the fundamental duration of the envelopes. Low values = fast clicks; High values = long swells.


2. **INTONE (`-1.0` to `1.0`)**
* **Label:** Intone / Spread
* **Function:** Spreads the *Time* parameter across the 6 voices.
* `0.0`: All voices have identical speed.
* `+1.0`: Voice 6 is significantly faster than Voice 1 (High frequency percussion vs Low frequency swells).
* `-1.0`: Voice 6 is significantly slower than Voice 1.




3. **RAMP (`0.0` to `1.0`)**
* **Label:** Ramp / Skew
* **Function:** Adjusts the Attack/Decay ratio.
* `0.0`: Immediate Attack, Long Decay (Percussive).
* `0.5`: Triangle wave (Equal Attack/Decay).
* `1.0`: Long Attack, Immediate Decay (Reverse).




4. **CURVE (`-1.0` to `1.0`)**
* **Label:** Curve / Shape
* **Function:** Bends the envelope slope.
* `-1.0`: Logarithmic (Snappy/Plucky).
* `0.0`: Linear.
* `1.0`: Exponential (Smooth/Round).




5. **RUN (`0.0` to `1.0`)**
* **Label:** Run / Dynamics
* **Function:** The "Macro" control for the physics engine. Its behavior changes based on the *Mode*.


6. **MODE (`Enum: 0, 1, 2`)**
* **Label:** Transient / Sustain / Cycle
* **Function:** Selects the physics simulation logic (see Section III).


7. **TEMPO (`40` to `250` BPM)**
* **Label:** BPM / Clock
* **Function:** Sets the speed of the master phasor (the "Measure").



#### B. The Sequence Configuration (12 Parameters)

These are the "Scripted" parameters. In a software GUI, this could be a grid of 6 rows, or a text command line.

**For Each Voice (1 through 6):**

1. **DIVS (`Integer: 1 to 64`)**: The polyrhythmic subdivision. "How many notes fit in one measure?"
2. **REPEATS (`Integer: -1 to 255`)**: The burst length. "How many notes fire when the burst starts?" (`-1` = Infinite/Looping).

---

### II. The DSP Architecture (Signal Flow)

This is how you wire the inputs to the output.

#### Step 1: The Master Clock

You need a global accumulator (Phasor) that ramps from `0.0` to `1.0` over the course of one musical measure (4 beats).

```cpp
// Called every sample
float phase_step = (Tempo / 60.0f * 4.0f) / SampleRate;
master_phasor += phase_step;
if (master_phasor >= 1.0f) master_phasor -= 1.0f;

```

#### Step 2: The 6 Geode Voices (Trigger Logic)

Iterate through 6 instances of a `Voice` class. Each voice watches the `master_phasor` but moves at its own speed determined by its **DIVS** parameter.

* **Logic:** `VoicePhase = MasterPhasor * VoiceDivs`
* **Trigger Condition:** When `VoicePhase` wraps (goes from >0.99 to 0.0), trigger the envelope **IF** repeats are remaining.

#### Step 3: The Physics Engine (Velocity Calculation)

When a voice triggers, calculate its **Target Velocity** (how loud this specific note should be). This is where **MODE** and **RUN** interact.

* **Transient Mode:** `Run` adds "accents" (silences some steps to make rhythms).
* *Formula:* `Velocity = (StepIndex % map(Run, 1, 8) == 0) ? 1.0 : 0.2;`


* **Sustain Mode:** `Run` increases "Gravity" (decay).
* *Formula:* `Velocity = pow(1.0 - Run, StepIndex);` (Steps get quieter).


* **Cycle Mode:** `Run` sets LFO Speed (undulation).
* *Formula:* `Velocity = 0.5 + 0.5 * sin(StepIndex * Run * Constant);`



#### Step 4: The Manifold (Slope Generation)

Now, generate the actual envelope signal. This is where **TIME** and **INTONE** apply.

* **Base Rate:** `Rate = map(TIME)`
* **Intone Scaling:** You must calculate a unique rate for each voice index ().
* *Formula:* `VoiceRate[i] = BaseRate * pow(2.0, INTONE * (i - 2.5));`
* *Result:* This spreads the voices apart in frequency/speed.


* **Shape:** Apply **RAMP** (skew rise vs fall) and **CURVE** (shaping function) to the linear output of the slope generator.

#### Step 5: The Mixer

Sum all 6 voices to your single output.

* **Formula:** `Output = (Voice0 + Voice1 + Voice2 + Voice3 + Voice4 + Voice5) / 6.0;`
* *Note:* Dividing by 6 ensures you never clip, but the output might be quiet if only one voice is playing. You may want to add a "Makeup Gain" or soft clipper here.

---

### III. Summary of "The Experience"

If you implement this plan, here is what the user experience will be:

1. **Setup:** The user sets Voice 1 to `Divs: 4` (Kick) and Voice 6 to `Divs: 7` (Polyrhythmic Hat).
2. **Play:** They hear a rhythmic pattern.
3. **Perform:**
* Turning **INTONE** separates the sounds: Voice 1 becomes a long bass throb, Voice 6 becomes a tight click.
* Turning **RUN** in **Sustain Mode** makes the pattern act like a bouncing ball, fading out and restarting.
* Turning **RAMP** transforms the sound from percussive (drum-like) to bowing (violin-like).


4. **Result:** A complex, shifting voltage terrain generated by the interference of the 6 voices.

## Implementation Plan

### Phase 1: Core Engine (C++)

**File:** `src/apps/sequencer/engine/GeodeEngine.h` / `.cpp`

A standalone class handling the DSP logic. It reuses Performer's `Curve` math for shaping.

```cpp
class GeodeEngine {
public:
    void update(float dt, float measurePhase);
    // ... setters for Time, Intone, Ramp, Curve, Run, Mode
    // ... setters for Divisor, Repeats
    float output() const;
};
```

### Phase 2: Host Integration

**File:** `src/apps/sequencer/engine/TeletypeTrackEngine.h` / `.cpp`

*   Add `GeodeEngine _geodeEngine;` member.
*   In `update(dt)`, call `_geodeEngine.update(dt, measureFraction())`.
*   Expose helper methods for the Teletype Bridge to set Geode parameters.

### Phase 3: Bridge & Ops (C Interface)

**File:** `src/apps/sequencer/engine/TeletypeBridge.h` / `.cpp`
*   Add C-compatible exports: `tele_geo_set_time`, `tele_geo_get_val`, etc.

**File:** `teletype/src/ops/geode.c` / `.h`
*   Implement the new Teletype operations.

**New Operations:**

| Op | Description |
| :--- | :--- |
| `GEO.TIME x` | Set base time (0-16383). |
| `GEO.TONE x` | Set intone/spread (-16383 to 16383). |
| `GEO.RAMP x` | Set ramp/skew (0-16383). |
| `GEO.CURV x` | Set shape curve (-16383 to 16383). |
| `GEO.RUN x` | Set physics run param (0-16383). |
| `GEO.MODE x` | Set mode (0=Trans, 1=Sust, 2=Cyc). |
| `GEO.DIV i x` | Set voice `i` divisor to `x`. |
| `GEO.REP i x` | Set voice `i` repeats to `x` (-1=inf). |
| `GEO.VAL` | Get current mixed output value. |
| `GEO.READ i` | Get current value of voice `i`. |

**File:** `teletype/src/table.c`
*   Register the new ops in the dispatch table.

### Phase 4: Integration
*   Build the simulator.
*   Verify functionality with a test script.

## Reused Components
*   **Engine Transport:** `_engine.measureFraction()` for sample-accurate syncing.
*   **Curve Library:** `model/Curve` for envelope shaping.
*   **Bridge Architecture:** Existing `TeletypeBridge` pattern for C/C++ interop.
