# Plan: Tides Track Type Implementation

This document outlines the plan to implement a new `Tides` track type in the Performer firmware. This track type will function as a complex LFO generator, inspired by Mutable Instruments' Tides v2, with its output sent directly to the track's assigned CV and Gate channels.

## 1. Core Goal

The objective is to create a new, first-class track mode, `Track::Mode::Tides`. When a track is set to this mode, it will not play notes but will instead run a "Tides2-Lite" oscillator. This provides users with up to 8 complex, syncable LFOs that can be used for intricate modulation of external Eurorack modules.

## 2. Architecture Overview

The implementation will be integrated into the existing Model-Engine-UI architecture.

### 2.1. Model Layer

-   **`TidesTrack.h`**: A new class, `TidesTrack`, will be created to serve as the data model for the LFO. It will store all user-editable parameters:
    -   `enabled`: (bool)
    -   `frequency`: (float) - Can be free-running (Hz) or clock-synced (divisions).
    -   `shape`: (float) - Controls waveform morphing.
    -   `slope`: (float) - Controls waveform symmetry.
    -   `smoothness`: (float) - Controls harmonic content (implemented as a filter/crossfade).
    -   `measureSync`: (bool) - Enables phase reset on the first beat of a measure.
    -   `gateOutput`: (enum) - `Off`, `Rise`, `Fall`.

-   **`Track.h`**: The main track model will be updated to incorporate `TidesTrack`.
    -   `Track::Mode::Tides` will be added to the `TrackMode` enum.
    -   `TidesTrack` will be added to the `Container` and the `_track` union.
    -   A `tidesTrack()` accessor will be added.
    -   All `switch` statements will be updated to handle the `Tides` case.

### 2.2. Engine Layer

-   **`TidesTrackEngine.h/.cpp`**: A new engine class will be created specifically for this track mode.
    -   It will contain the "Tides2-Lite" oscillator, optimized for CV generation (wavetable-based, 1kHz update rate).
    -   It will be a `Clock::Listener` to receive measure reset events for the `measureSync` feature.
    -   It will read parameters from its `TidesTrack` model.
    -   It will compute and write CV values to the track's assigned DAC channel.
    -   It will compute and write Gate values (high/low) to the track's assigned Gate output.

-   **`Engine.cpp`**: The main `TrackEngineContainer` will be modified to instantiate `TidesTrackEngine` for any track set to `Tides` mode.

### 2.3. UI Layer

-   A new set of pages will be designed and implemented to allow the user to visualize the LFO waveform and edit its parameters (`frequency`, `shape`, `slope`, etc.) on the hardware display.

## 3. Test-Driven Development (TDD) Workflow

The implementation will strictly follow the TDD methodology.

1.  **Create `TidesTrack.h`**: Define the data model class.
2.  **Create `src/tests/unit/sequencer/TestTidesTrack.cpp`**: Write and pass tests for the `TidesTrack` model, verifying default values, setters, getters, clamping, and serialization.
3.  **Modify `Track.h`**: Integrate the `TidesTrack` into the main `Track` model as described above.
4.  **Update `TestTrack.cpp`** (or equivalent): Add tests to verify that a `Track` can be successfully switched to `Tides` mode and that its data can be accessed correctly.
5.  **Create `TidesTrackEngine.h/.cpp`**.
6.  **Create `src/tests/unit/sequencer/TestTidesTrackEngine.cpp`**: Write and pass tests for the engine logic:
    -   Verify correct waveform generation based on model parameters.
    -   Verify phase reset when a `measureSync` event is received.
    -   Verify correct gate output for `Rise` and `Fall` modes.
7.  **Integrate with Main Engine**: Modify the `Engine` to use the new `TidesTrackEngine`.
8.  **Update Build System**: Add all new files (`.h`, `.cpp`, `test.cpp`) to `CMakeLists.txt`.
9.  **Implement UI**: Develop the user interface pages for the `Tides` mode.
