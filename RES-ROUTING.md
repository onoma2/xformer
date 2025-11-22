# Routing Capabilities in PER|FORMER Firmware

This document summarizes the routing capabilities, potential extensions for Tuesday (Algo) track parameters, and architectural implications for the Model-Engine-UI layers. It also clarifies the current status of internal LFOs.

## 1. Routing Capabilities Overview

The PER|FORMER firmware's routing system allows modulation of various parameters (Targets) using different internal and external signals (Sources). A `Route` specifies a `Source`, a `Target`, an `Amount` (intensity/polarity), and a `Range` (min/max values).

### Available Sources (`Routing::Source`)

These sources are defined in `src/apps/sequencer/model/Routing.h`:

*   **LFOs (1-4):** Four internal low-frequency oscillators.
*   **CV Inputs (1-4):** Voltages from the four CV input jacks.
*   **MIDI Messages:** Note, Velocity, Aftertouch, Pitch Bend, Modulation Wheel, MIDI CC.
*   **Track Outputs:** The final output value of another track.

### Available Targets (`Routing::Target`)

These targets are defined in `src/apps/sequencer/model/Routing.h`:

**Global Parameters:**
*   `Tempo`
*   `Swing`

**Note Track Parameters (per track):**
*   `SlideTime`
*   `Octave`
*   `Transpose`
*   `Rotate`
*   `GateProbability`
*   `GateOffset`
*   `RetriggerProbability`
*   `Length`
*   `NoteProbability`

**Curve Track Parameters (per track):**
*   `Shape`
*   `Min/Max`
*   `Phase`
*   `Rotate`
*   `ShapeProbabilityBias`

**Sequence Parameters (per track/pattern):**
*   `FirstStep`
*   `LastStep`
*   `RunMode`
*   `Divisor`
*   `Scale`
*   `RootNote`

**Engine/PlayState Parameters:**
*   `Play`, `PlayToggle`, `Record`, `RecordToggle`, `TapTempo`
*   `Mute`, `Fill`, `FillAmount`, `Pattern`

### Example: Modulating Note Track `Rotate` with LFO 1

To use LFO 1 to continuously shift the starting point of the sequence on Track 1:

1.  **UI Setup (Conceptual):** In a Routing setup interface, you would define a route:
    *   `Source`: LFO 1
    *   `Target`: Note Rotate 1 (for Track 1)
    *   `Amount`: e.g., 100% for full modulation range.
2.  **Engine Processing:** On each clock tick, the `RoutingEngine` would:
    *   Retrieve the current value of LFO 1.
    *   Calculate the modulated `Rotate` value based on the route's `Amount` and `Range`.
    *   Apply this value to the `Rotate` parameter of the `NoteTrack`'s model object for Track 1.
3.  **Model Interaction:** The `Rotate` parameter in `NoteTrack` is of type `Routable<int8_t>`. This `Routable` wrapper handles the base value (user-set) and the incoming modulation offset, combining them for the final effective value.

## 2. Exposing Tuesday (Algo) Track Parameters (`flow`, `ornament`)

Currently, parameters like `flow` and `ornament` within the `TuesdayTrack` are **not routable**. They are defined as simple `int` or `int8_t` members in `src/apps/sequencer/model/TuesdayTrack.h`, not as `Routable<>` types.

To make them routable, the following modifications are required:

1.  **Model Layer Changes:**
    *   **`src/apps/sequencer/model/TuesdayTrack.h`:**
        *   Change `int8_t _flow;` to `Routable<int8_t> _flow;`
        *   Change `int8_t _ornament;` to `Routable<int8_t> _ornament;`
        *   Add corresponding `setFlow(value, routed)` and `setOrnament(value, routed)` methods that interact with the `Routable` members.
    *   **`src/apps/sequencer/model/Routing.h`:**
        *   Add new `Target` enum entries for each parameter for all 8 tracks, e.g., `TuesdayFlow1` to `TuesdayFlow8`, `TuesdayOrnament1` to `TuesdayOrnament8`.
        *   Update the `targetNames` array in `src/apps/sequencer/model/Routing.cpp` to provide display names for these new targets.

2.  **Engine Layer Changes:**
    *   **`src/apps/sequencer/engine/RoutingEngine.cpp`:**
        *   Modify the `RoutingEngine::updateSinks()` method (specifically the `_routing.writeTarget` call, which dispatches to `_project.track().tuesdayTrack().writeRouted`).
        *   The `TuesdayTrack::writeRouted` method would need to be implemented to update the `_flow` and `_ornament` `Routable` members based on the incoming `Target` and value.

3.  **UI Layer Changes:**
    *   **`src/apps/sequencer/ui/model/RouteListModel.cpp`:**
        *   Modify `RouteListModel::build()` to include the new `TuesdayFlow` and `TuesdayOrnament` targets in the list of selectable destinations when the track mode is `Tuesday`.

## 3. Implications for Model-Engine-UI Layers

Extending routing to new parameters affects all three architectural layers, highlighting their distinct roles:

*   **Model:** This layer defines the data structures and the available parameters. Changes here update the fundamental data representation to support modulation (e.g., changing `int` to `Routable<>`, adding new `Target` enums). It dictates *what* can be routed.
*   **Engine:** This layer handles the real-time processing and application of logic. The `RoutingEngine`'s role is to calculate modulation values from sources and apply them to the respective parameters in the model. Changes here involve implementing *how* modulation is applied.
*   **UI:** This layer is responsible for presenting information to the user and capturing their input. Changes here involve making new routable parameters visible and selectable in the user interface. It defines *how* the user interacts with the routing system.

## 4. Current Status of Internal LFOs

While `Routing::Source` includes `Lfo1` through `Lfo4`, **these internal LFOs are currently not implemented in the engine, nor are they exposed or configurable in the UI.**

*   **Engine:** The `RoutingEngine::updateSources()` method in `src/apps/sequencer/engine/RoutingEngine.cpp` does not contain any logic to generate LFO signals or to retrieve values for `Routing::Source::LfoX`. Therefore, even if an LFO is selected as a source, it will not produce any modulation.
*   **UI:** There are no UI elements or pages that allow users to configure LFO parameters such as waveform, rate, or depth.

**Implementation for LFOs would involve:**

1.  **LFO Class:** Creating a dedicated `Lfo` class (e.g., in `core/`) to manage waveform generation, frequency, and other parameters.
2.  **Engine Integration:** Adding instances of the `Lfo` class to the main `Engine` (e.g., `src/apps/sequencer/engine/Engine.h`) and updating them regularly (e.g., in `Engine::update()`).
3.  **RoutingEngine Update:** Modifying `RoutingEngine::updateSources()` to read the current output value from these `Engine`-managed `Lfo` instances and assign it to the `_sourceValues` array for the corresponding LFO routes.
4.  **UI Exposure (Optional but Recommended):** Developing UI pages and model components to allow users to configure the LFO parameters.

This concludes the thorough analysis of the routing system.
