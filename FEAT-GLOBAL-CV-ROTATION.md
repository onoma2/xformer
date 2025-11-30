# Feature: Selective Output Rotation (CV & Gate)

## Concept
Allow dynamic rotation of track-to-output assignments within a **user-defined subset** of tracks. This enables complex signal routing (e.g., rotating a chord voicing across 3 oscillators) while leaving other tracks (e.g., drums on track 4-8) static.

## Status
**Implemented.**

## Architecture
The "Active Rotation Pool" is defined dynamically by the **Routing Mask**.
*   If a Track is targeted by the Rotation Route, it joins the pool.
*   If a Track is NOT targeted (or has 0 rotation), it remains static on its assigned output.

## Algorithm: "The Modulated Pool"

1.  **Setup (Engine Update):**
    *   Scan all Physical Outputs.
    *   Identify the **Source Track** for each output.
    *   Check if that Source Track has a non-zero **Rotation Value** (modulated via Routing).
    *   Build a list of **Pool Indices** (Outputs that are "in the game").
    
2.  **Execution:**
    *   For each Output in the Pool:
        *   Calculate its position in the pool list.
        *   Apply the rotation offset (derived from the track's rotation parameter).
        *   Find the **New Source Track** from the rotated position in the pool.
    *   For Outputs NOT in the Pool:
        *   Use the static assignment from Layout.

## Implementation Details

### 1. Model (`Track.h`, `Routing.h`, `ProjectVersion.h`)
*   `ProjectVersion.h`: Added `Version47`.
*   `Routing.h`: Added `Target::CvOutputRotate` and `Target::GateOutputRotate` to `Target` enum.
*   `Track.h`: Added `Routable<int8_t> _cvOutputRotate` and `Routable<int8_t> _gateOutputRotate` to `Track` class, with accessors `cvOutputRotate()`, `gateOutputRotate()`, `isCvOutputRotated()`, `isGateOutputRotated()`.
*   `Track.cpp`: Updated `clear()`, `write()`, `read()` to handle the new parameters and versioning.
*   `Routing.cpp`: Added `TargetInfo` entries for the new targets (`minDef=0`, `maxDef=8`, `min=-8`, `max=8`). Updated `writeTarget` to correctly dispatch `CvOutputRotate` and `GateOutputRotate` to the `Track` object.

### 2. Engine (`Engine.cpp`)
*   `Engine::updateTrackOutputs()`: Implemented the rotation logic:
    *   Iterates through `CONFIG_CHANNEL_COUNT` (8) physical outputs.
    *   Builds `cvPool` and `gatePool` arrays of physical output indices. An output `i` is added to the pool if the track it's assigned to (`_project.cvOutputTrack(i)`) is configured for rotation (`_project.track(trackIndex).isCvOutputRotated()`).
    *   For each output `i` in the pool, calculates a `rotatedIndex` by applying the track's rotation value (retrieved from `_project.track(originalTrack).cvOutputRotate()`) modulo `poolSize`.
    *   Dispatches the CV/Gate value from the track at `poolTracks[rotatedIndex]` to the physical output `i`.
    *   Outputs not in the pool (or `poolSize <= 1`) remain static.

## Resource Cost
*   **CPU:** Low. Involves a few loops (max 8 iterations) and integer arithmetic. Happens once per engine update (1ms).
*   **RAM:** Very low. A few stack arrays (size 8).

## Usage
1.  **Layout Page:** Assign tracks to physical CV/Gate outputs as desired.
2.  **Routing Page:**
    *   Create a new route.
    *   Set **Target** to `CV Out Rot` or `Gate Out Rot`.
    *   Set **Source** (e.g., `CV In 1`, a slow LFO).
    *   Set **Min/Max** for the rotation range (defaults to 0-8, can be adjusted to -8 to 8).
    *   **Crucially:** Select the **Tracks** that you want to participate in the rotation using the track mask (e.g., Track 1, 2, 3).
    *   **Commit** the route.

When the modulation source changes, the selected tracks' outputs will rotate amongst themselves, leaving unselected tracks' outputs static.