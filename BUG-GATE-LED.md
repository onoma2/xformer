# Feature Plan: Tuesday Track Gate LED

## Objective

The `TuesdayTrack` mode currently does not provide visual feedback for gate events. This plan outlines how to implement LED blinking on gate triggers for `TuesdayTrack`, mimicking the existing behavior of `NoteTrack`.

## Analysis of NoteTrackEngine

The `NoteTrackEngine` provides the blueprint for this feature:

1.  **`_activity` Flag:** It uses a private `bool _activity` flag to represent the gate's state. This is exposed to the UI via a public `activity()` method.

2.  **Gate Queue:** The engine pushes gate-on (`gate = true`) and gate-off (`gate = false`) events onto a queue.

3.  **State Update:** In its `tick()` function, as the engine processes these events, it updates the activity state with `_activity = event.gate;`. This makes the `_activity` flag high for the exact duration of the gate.

## Implementation Plan for TuesdayTrackEngine

The `TuesdayTrackEngine` uses a different mechanism for gates (a `_gateTicks` countdown timer), but we can adapt the same principle.

1.  **Add `_activity` member to `TuesdayTrackEngine.h`:**
    *   Add a private boolean member: `bool _activity = false;`
    *   Add the public getter: `virtual bool activity() const override { return _activity; }`

2.  **Modify `TuesdayTrackEngine.cpp`:**
    *   **Initialization:** In the `reset()` method, add `_activity = false;` to ensure it is reset properly.

    *   **Gate-On Logic:** In the `tick()` function, locate the `if (gateTriggered)` block where a new gate is fired. Inside this block, add:
        ```cpp
        _activity = true;
        ```

    *   **Gate-Off Logic:** In the `tick()` function, find the section that handles the gate timer countdown. Inside the `if (_gateTicks == 0)` block where the gate output is turned off, add:
        ```cpp
        _activity = false;
        ```

This plan will synchronize the `_activity` flag with the `TuesdayTrackEngine`'s gate output, providing the necessary mechanism for the UI to display gate events.
