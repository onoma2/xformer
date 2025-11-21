# Bug Report: CV and Glide Failure in Tuesday Track Finite Loops

## Symptom

A user reported that in the `Tuesday` track mode, the **Gated/Free CV Update** (`cvUpdateMode`) and **Track Glide Override** (`glide`) parameters only appear to work for the first few algorithms. Further testing confirms that these features fail for all algorithms when they are running in their default finite-loop (buffered) mode.

## Root Cause Analysis

The bug is located in the `TuesdayTrackEngine::tick()` function in `src/apps/sequencer/engine/TuesdayTrackEngine.cpp`. It is not a flaw in the individual algorithms, but rather a critical oversight in the engine's handling of buffered pattern playback.

The logical flow is as follows:
1.  At the start of the step-trigger block, a `noteVoltage` variable is declared and initialized to `0.f`.
2.  The code then checks if the track is running in a finite loop (`loopLength > 0`).
3.  If it is, it correctly reads the pre-generated `note`, `octave`, and `slide` values from the `_buffer`.
4.  **The Bug:** After reading these values, the code **fails to calculate the `noteVoltage`** from the buffered `note` and `octave`. The `noteVoltage` variable retains its stale `0.f` value.
5.  When execution reaches the end of the function where `cvUpdateMode` and `glide` are handled, this incorrect `0.f` voltage is used as the target CV.

This causes the system to either update the CV to 0V or attempt to glide to 0V, which is not the intended behavior and breaks the functionality of these parameters.

The reason the bug might not have been immediately obvious is that the code path for *infinite loops* (`loopLength == 0`) is correct. In that mode, `noteVoltage` is calculated properly within each algorithm's `case` block.

## Proposed Fix

The fix is to add the missing `noteVoltage` calculation immediately after the data is read from the `_buffer` within the finite-loop code block.

```cpp
// In TuesdayTrackEngine::tick()

if (loopLength > 0) {
    // ...
    if (effectiveStep < BUFFER_SIZE) {
        note = _buffer[effectiveStep].note;
        octave = _buffer[effectiveStep].octave;
        _gatePercent = _buffer[effectiveStep].gatePercent;
        _slide = _buffer[effectiveStep].slide;
        shouldGate = true;
        // BUG FIX: Calculate noteVoltage from buffered data
        noteVoltage = (note + (octave * 12)) / 12.0f;
    }
} else {
    // ... infinite loop logic (which is correct)
}
```
This ensures that the correct target voltage is always available for the CV and glide logic, regardless of whether the track is in buffered or live-generation mode.
