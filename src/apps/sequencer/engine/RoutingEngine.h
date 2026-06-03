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

    void resetShaperState();
    float routeSource(int index) const;
    bool cvRotateInterpolated(int trackIndex) const;
    float cvRotateValue(int trackIndex) const;

    struct RouteState {
        Routing::Target target = Routing::Target::None;
        uint8_t tracks = 0;
        std::array<Routing::Shaper, CONFIG_TRACK_COUNT> shaper{};

        // Per-shaper state structs — stored in a union per track slot.
        // The active variant is determined by route.shaper(trackIndex) at access time.
        // No default member initializers inside union — resetState() sets per-shaper defaults.

        struct LocationState {
            float location;
        };

        struct EnvelopeState {
            float envelope;
        };

        struct FreqFollowState {
            float freqAcc;
            bool freqSign;
            uint16_t ffHold;
        };

        struct ActivityState {
            float activityPrev;
            float activityLevel;
            bool activitySign;
            uint16_t actHold;
        };

        struct ProgDivState {
            float progCount;
            float progThreshold;
            bool progSign;
            float progOut;
            float progOutSlewed;
            uint16_t progHold;
        };

        union TrackStateUnion {
            LocationState locationState;
            EnvelopeState envelopeState;
            FreqFollowState freqFollowState;
            ActivityState activityState;
            ProgDivState progDivState;
        };

        std::array<TrackStateUnion, CONFIG_TRACK_COUNT> shaperState;

        // Rising-edge state for Trigger-kind targets: one "was high" bit per
        // track (bit 0 for global triggers). Unifies the former per-target
        // latches (Reset / PlayToggle / RecordToggle / TapTempo).
        uint8_t gateMask = 0;
    };

    // Reset a single TrackStateUnion to the correct initial values for a given shaper.
    static void resetState(RouteState::TrackStateUnion &st, Routing::Shaper shaper);

    // Determine the effective shaper for a given route and track index.
    // Bus targets use shaper(0), engine/other targets use None.
    static Routing::Shaper effectiveShaper(const Routing::Route &route, int trackIndex);

    // Reset all shaperState entries for a route using the route's current configuration.
    static void resetRouteShaperState(RouteState &routeState, const Routing::Route &route);

    // Apply progressive divider shaper (public for use by applyShaper helper)
    static float applyProgressiveDivider(float srcNormalized, RouteState::ProgDivState &st);

private:
    void updateSources();
    void updateSinks();

    void writeEngineTarget(Routing::Target target, float normalized, RouteState &routeState);

    Engine &_engine;
    Routing &_routing;
    Project &_project;

    std::array<float, CONFIG_ROUTE_COUNT> _sourceValues;

    std::array<RouteState, CONFIG_ROUTE_COUNT> _routeStates;
    std::array<float, CONFIG_TRACK_COUNT> _cvRotateValues{};
    std::array<bool, CONFIG_TRACK_COUNT> _cvRotateInterp{};
};
