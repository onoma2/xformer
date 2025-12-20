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

    struct RouteState {
        Routing::Target target = Routing::Target::None;
        uint8_t tracks = 0;
        std::array<Routing::Shaper, CONFIG_TRACK_COUNT> shaper{};
        struct TrackState {
            float location = 0.5f;
            float envelope = 0.f;
            float freqAcc = 0.f;
            bool freqSign = false;
            float activityPrev = 0.5f;
            float activityLevel = 0.f;
            bool activitySign = false;
            uint16_t ffHold = 0;
            uint16_t actHold = 0;
            float progCount = 0.f;
            float progThreshold = 1.f;
            bool progSign = false;
            float progOut = 0.f;
            float progOutSlewed = 0.f;
            uint16_t progHold = 0;
        };
        std::array<TrackState, CONFIG_TRACK_COUNT> shaperState;
    };

private:
    void updateSources();
    void updateSinks();

    void writeEngineTarget(Routing::Target target, float normalized);

    Engine &_engine;
    Routing &_routing;

    std::array<float, CONFIG_ROUTE_COUNT> _sourceValues;

    std::array<RouteState, CONFIG_ROUTE_COUNT> _routeStates;

    uint8_t _lastPlayToggleActive = false;
    uint8_t _lastRecordToggleActive = false;

    static float applyProgressiveDivider(float srcNormalized, RouteState::TrackState &st);
};
