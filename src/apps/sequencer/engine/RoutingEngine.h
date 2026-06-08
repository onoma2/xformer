#pragma once

#include "Config.h"

#include "MidiPort.h"

#include "model/Model.h"

#include "core/midi/MidiMessage.h"

#include <array>

#include <cstdint>

class Engine;

class RoutingEngine {
public:
    RoutingEngine(Engine &engine, Model &model);

    void update();

    bool receiveMidi(MidiPort port, const MidiMessage &message);

    float routeSource(int index) const;

    // Output group rotation (spec 018/019): one mask + amount per frame, applied to the
    // masked group's jacks in the engine output loop. Mask 0 = no rotation this frame.
    uint8_t gateRotateMask() const { return _gateRotateMask; }
    int gateRotateAmount() const { return _gateRotateAmount; }
    uint8_t cvRotateMask() const { return _cvRotateMask; }
    int cvRotateAmount() const { return _cvRotateAmount; }

    struct RouteState {
        Routing::Target target = Routing::Target::None;
        uint8_t tracks = 0;

        // Rising-edge state for Trigger-kind targets: one "was high" bit per
        // track (bit 0 for global triggers). Unifies the former per-target
        // latches (Reset / PlayToggle / RecordToggle / TapTempo).
        uint8_t gateMask = 0;
    };

private:
    void updateSources();
    void updateSinks();

    // Resolve a Source to a normalized [0,1] value, reading live engine I/O. CV-domain
    // sources normalize through the route's own cvSource range (scaleSource has no own
    // config). MIDI is excluded (set asynchronously in receiveMidi) -> returns 0.
    float resolveSourceValue(const Routing::Route &route, Routing::Source source) const;

    void writeEngineTarget(Routing::Target target, float normalized, RouteState &routeState);

    Engine &_engine;
    Routing &_routing;
    Project &_project;

    std::array<float, CONFIG_ROUTE_COUNT> _sourceValues;

    std::array<RouteState, CONFIG_ROUTE_COUNT> _routeStates;
    uint8_t _gateRotateMask = 0;
    int _gateRotateAmount = 0;
    uint8_t _cvRotateMask = 0;
    int _cvRotateAmount = 0;
};
