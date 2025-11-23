# Gate Offset and Engine Fixes Summary

This document outlines a series of fixes and feature refinements implemented in the `TuesdayTrackEngine`. The work addressed core bugs related to note generation and completely redesigned the `gateOffset` parameter into a more musical and flexible control.

## 1. Core Bugs Fixed

### `Power` Parameter Bug

- **Symptom:** Setting `Power = 16` resulted in silence, the opposite of the intended "maximum density" behavior.
- **Cause:** The formula for an internal event timer (`_ambient_event_timer`) was flawed. At high power settings, it incorrectly calculated a very long wait time between notes instead of a short one.
- **Fix:** The calculation was corrected to create a proper inverse relationship, where higher `Power` values now correctly result in shorter wait times and denser patterns.

### Missing Gate Timing Logic

- **Symptom:** The `gateOffset` parameter had no audible effect, regardless of its setting. Notes always played on the beat.
- **Cause:** This was the primary issue. The `TuesdayTrackEngine::update()` function was empty. It completely lacked the essential time-keeping logic required to handle a delayed gate. The engine would decide *that* a note should be delayed, but the code to perform the countdown for the delay and then open the gate did not exist.
- **Fix:** The missing logic was implemented.
    1.  New state variables (`_pendingGateOffsetTicks`, `_gateLengthTicks`) were added to the engine to track timing.
    2.  The `tick()` function was updated to set up these timers when a note is generated.
    3.  The `update()` function was filled with the countdown logic that processes these timers, opens the gate after the offset delay, and closes it after the correct length.

## 2. New "Tri-State Groove" Feature for `gateOffset`

Based on feedback, the `gateOffset` parameter was redesigned from a simple switch into a powerful three-mode "Groove Morph" control.

- **Mode 1: `gateOffset = 0%` ("Force On-Beat")**
  - All notes are forced to play exactly on the beat with zero offset. This disables all algorithmic groove and randomness for perfectly straight timing.

- **Mode 2: `gateOffset = 1-99%` ("Probabilistic Groove")**
  - The sequence uses the algorithm's natural, internal groove as its baseline rhythm.
  - The `gateOffset` value introduces a percentage chance for each note to have its timing overridden by a new, fully random offset. This adds variation and surprise while retaining the algorithm's core character.

- **Mode 3: `gateOffset = 100%` ("Pure Chaos")**
  - The algorithm's internal groove is completely ignored. The timing of every single note is fully randomized, creating chaotic, unpredictable rhythms.

## 3. Architectural Improvements

### Dedicated UI Random Number Generator

- **Problem:** Initial attempts to add probability to `gateOffset` used the algorithm's main Random Number Generator (`_rng`), which broke the determinism of the pattern generation and caused instability.
- **Solution:** A new, separate `_uiRng` was created. This generator is used exclusively for UI-driven probabilistic events, cleanly separating them from the core algorithms and preventing state corruption.

### State Management and Initialization

- **Problem:** Several compilation errors occurred due to missing member variable declarations (`_cached...`) and initializations.
- **Solution:** All cached variables were correctly declared in the header file and are now initialized in the `reset()` function to ensure stable and predictable state management.
