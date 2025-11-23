# Feature Plan: Re-trigger and Trill State Machine

## 1. Goal

The primary goal is to add high-resolution, musically expressive re-triggers and trills to the `TuesdayTrackEngine`. This will allow algorithms like `Drill` and `Techno` to produce their signature fast rolls and hi-hat patterns, and will enable other melodic algorithms to be enhanced with ornamental trills.

## 2. High-Level Design

The feature will be implemented as a self-contained **state machine** inside the `TuesdayTrackEngine::update()` function.

This design was chosen to **encapsulate all complexity within the `TuesdayTrackEngine`**. It does not require any risky changes to the main `Engine` loop or the core architecture. The `TuesdayTrackEngine` becomes solely responsible for generating its own re-triggers, treating it as a special internal capability.

- The `tick()` function will remain responsible for discrete, per-step decisions. When a trill is desired, it will "arm" the state machine.
- The `update()` function, which runs continuously, will execute the state machine's logic, firing multiple short gate events in quick succession within a single step's timeframe.

## 3. UI Control (`trill` parameter)

A new user-facing parameter will be added to the `TuesdayTrack` model:

- **`trill` (0-100%):** This knob will control the intensity of the trill/re-trigger effect using a **"Scaled Probability"** model.

This model allows for creative collaboration between the algorithm's intent and the user's control.

#### "Scaled Probability" Logic

The final chance of a trill occurring is calculated as:
`Final Chance = (Algorithm's Base Chance * Your Trill Setting) / 100`

This results in three distinct behaviors:

- **`trill = 0%`:** The `Final Chance` is always 0. No trills or re-triggers will be generated. This is the master "off" switch.
- **`trill = 1-99%`:** The user's setting acts as a multiplier on the algorithm's built-in probability. If an algorithm has a 50% chance of creating a roll and you set `trill` to 50%, the final chance becomes 25%. This allows you to "tame" or reduce the frequency of the effect.
- **`trill = 100%`:** You are fully enabling the algorithm's intended behavior. Trills will occur with the exact probability defined by the algorithm's designer.

## 4. Implementation Details

### Model Changes (`TuesdayTrack.h`)

- A new `uint8_t _trill` member variable will be added to the `TuesdayTrack` model class.
- Corresponding getter (`trill()`), setter (`setTrill()`), and UI helper functions (`editTrill()`, `printTrill()`) will be added, mirroring the existing `glide` and `gateOffset` parameters.

### Engine State Changes (`TuesdayTrackEngine.h`)

The following private member variables will be added to `TuesdayTrackEngine` to manage the state machine:

- `int _retriggerCount`: How many re-triggers are left in the current burst.
- `uint32_t _retriggerPeriod`: The time in ticks between the start of each re-triggered note.
- `uint32_t _retriggerLength`: The gate length in ticks of each re-triggered note.
- `uint32_t _retriggerTimer`: A master countdown timer for all re-trigger events.
- `bool _isTrillNote`: A flag to alternate between the primary and trill note CVs.
- `float _trillCvTarget`: The CV value for the alternate trill note.
- `bool _retriggerArmed`: A flag to signal the `update()` function that a re-trigger sequence should begin.

### Engine Logic Changes (`TuesdayTrackEngine.cpp`)

- **`tick()` function:**
  - For algorithms that support this feature, they will define a *base probability* for a trill on a given step, as well as the desired *alternate trill note*.
  - The `tick()` function will use the "Scaled Probability" formula to determine if a trill should happen.
  - If a trill is triggered, it will set `_retriggerArmed = true` and populate the other state variables (`_retriggerCount`, `_trillCvTarget`, etc.) to arm the state machine.

- **`update()` function:**
  - This function will be refactored to contain the main state machine logic.
  - It will check if `_retriggerArmed` is true and take over the gate generation.
  - It will use `_retriggerTimer` to handle the precise timing of multiple gate-on and gate-off events within a single step.
  - On each re-triggered gate, it will use `_isTrillNote` to decide whether to output the primary `_cvTarget` or the alternate `_trillCvTarget`, creating the trill effect.

## 5. Target Algorithms

This feature is designed to be extensible. The initial implementation will target the **`Drill`** algorithm. Following that, other strong candidates for this feature include:

- `Techno` (for hi-hat rolls)
- `Minimal` (for glitchy micro-rhythms)
- `Acid` (for classic 303-style trills)
- `Tritrance` (for melodic embellishments)
- `Stomper` / `Funk` (for rhythmic fills)
