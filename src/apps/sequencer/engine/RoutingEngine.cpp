#include "RoutingEngine.h"

#include "model/RouteParam.h"
#include "model/RouteFork.h"

#include "RouteShellTrigger.h"
#include "GateRotation.h"
#include "Engine.h"
#include "MidiUtils.h"
#include "core/math/Math.h"

// Per-shaper state reset: zero-init then set non-zero initial values.
// Must match the original TrackState() constructor defaults for each shaper.
void RoutingEngine::resetState(RouteState::TrackStateUnion &st, Routing::Shaper shaper) {
    // Zero-init the entire union (all bytes to 0)
    st = RouteState::TrackStateUnion{};

    // Set per-shaper non-zero initial values (these differ from zero-init)
    switch (shaper) {
    case Routing::Shaper::Location:
        st.locationState.location = 0.5f;
        break;
    case Routing::Shaper::Activity:
        st.activityState.activityPrev = 0.5f;
        break;
    case Routing::Shaper::ProgressiveDivider:
        st.progDivState.progThreshold = 1.f;
        break;
    default:
        // Envelope, FrequencyFollower: all fields correctly default from zero-init
        break;
    }
}

Routing::Shaper RoutingEngine::effectiveShaper(const Routing::Route &route, int trackIndex) {
    auto target = route.target();
    if (Routing::isBusTarget(target)) {
        return route.shaper(0);
    } else if (Routing::isPerTrackTarget(target)) {
        return route.shaper(trackIndex);
    } else {
        return Routing::Shaper::None;
    }
}

void RoutingEngine::resetRouteShaperState(RouteState &routeState, const Routing::Route &route) {
    auto target = route.target();
    if (Routing::isBusTarget(target)) {
        // Bus targets use only track 0
        resetState(routeState.shaperState[0], route.shaper(0));
        // Tracks 1-7 get None (no state needed)
        for (int i = 1; i < CONFIG_TRACK_COUNT; ++i) {
            resetState(routeState.shaperState[i], Routing::Shaper::None);
        }
    } else if (Routing::isPerTrackTarget(target)) {
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            resetState(routeState.shaperState[i], route.shaper(i));
        }
    } else {
        // Engine/other targets: no shaper state needed
        for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
            resetState(routeState.shaperState[i], Routing::Shaper::None);
        }
    }
}

// Apply per-track bias/depth to the normalized source (0..1) before the route window is applied.
static inline float applyBiasDepthToSource(float srcNormalized, const Routing::Route &route, int trackIndex) {
    float depth = route.depthPct(trackIndex) * 0.01f;
    float bias = route.biasPct(trackIndex) * 0.01f;
    float shaped = 0.5f + (srcNormalized - 0.5f) * depth + bias;
    return clamp(shaped, 0.f, 1.f);
}

// Target-agnostic waveslicer: fold around 0.5 in normalized source space with a fixed ±0.5 jump.
static inline float applyCreaseSource(float normalized, float bias) {
    // threshold moves inversely to bias by 30%
    float threshold = 0.5f + (bias * -0.3f);
    constexpr float creaseAmount = 0.5f;
    float creased = normalized + (normalized <= threshold ? creaseAmount : -creaseAmount);
    return clamp(creased, 0.f, 1.f);
}

static inline float applyLocation(float srcNormalized, float &state) {
    // target ~4s rail-to-rail at 1 kHz: 0.5 span / (4000 ticks) ≈ 0.000125
    constexpr float kRate = 0.000125f;
    state = clamp(state + (srcNormalized - 0.5f) * kRate, 0.f, 1.f);
    return state;
}

static inline float applyEnvelope(float srcNormalized, float &envState) {
    float rect = fabsf(srcNormalized - 0.5f) * 2.f; // 0..1
    constexpr float attackCoeff = 1.0f;
    // release with tau ~2s at 1 kHz: 1 - exp(-1/2000) ≈ 0.0005
    constexpr float releaseCoeff = 0.0005f;
    float coeff = rect > envState ? attackCoeff : releaseCoeff;
    envState += (rect - envState) * coeff;
    return clamp(envState, 0.f, 1.f);
}

static inline float applyTriangleFold(float srcNormalized, float bias) {
    // center point moves with bias by 50%
    float center = 0.5f + (bias * 0.5f);
    float x = 2.f * (srcNormalized - center); // -1..1
    float folded = (x > 0.f) ? 1.f - 2.f * fabsf(x - 0.5f) : -1.f + 2.f * fabsf(x + 0.5f);
    return clamp(0.5f + 0.5f * folded, 0.f, 1.f);
}

static inline float applyFrequencyFollower(float srcNormalized, RoutingEngine::RouteState::FreqFollowState &st) {
    bool signNow = srcNormalized > 0.5f;
    if (signNow != st.freqSign) {
        // Tuned for 1s LFO: reaches 1.0 in 14 crossings = 7s build time
        st.freqAcc = std::min(1.f, st.freqAcc + 0.10f);
        st.freqSign = signNow;
    }
    // leak with tau ~10s at 1 kHz: exp(-1/10000) ≈ 0.9999
    st.freqAcc *= 0.9999f;
    if (st.freqAcc >= 0.999f) {
        if (++st.ffHold > 3000) { // ~3s at 1 kHz (1x max LFO period)
            // Slew back to zero over ~7s instead of instant reset
            constexpr float fadeCoeff = 0.00015f; // tau ~7s
            st.freqAcc += (0.f - st.freqAcc) * fadeCoeff;

            // Exit fade when close to target
            if (st.freqAcc < 0.01f) {
                st.freqAcc = 0.f;
                st.ffHold = 0;
            }
        }
    } else {
        st.ffHold = 0;
    }
    return st.freqAcc;
}

static inline float applyActivity(float srcNormalized, RoutingEngine::RouteState::ActivityState &st) {
    float delta = fabsf(srcNormalized - st.activityPrev);
    // decay with tau ~2s at 1 kHz: exp(-1/2000) ≈ 0.9995 (tuned for 1-3s LFOs)
    constexpr float decay = 0.9995f;
    constexpr float gain = 0.05f; // Higher sensitivity for slow LFO movement
    st.activityLevel = st.activityLevel * decay + delta * gain;
    bool signNow = srcNormalized > 0.5f;
    if (signNow != st.activitySign) {
        st.activityLevel = 1.f;
        st.activitySign = signNow;
    }
    if (st.activityLevel >= 0.999f) {
        if (++st.actHold > 6000) { // ~6s at 1 kHz (2x max LFO period)
            // Slew back to zero over ~3s instead of instant reset
            constexpr float fadeCoeff = 0.00033f; // tau ~3s
            st.activityLevel += (0.f - st.activityLevel) * fadeCoeff;

            // Exit fade when close to target
            if (st.activityLevel < 0.01f) {
                st.activityLevel = 0.f;
                st.actHold = 0;
            }
        }
    } else {
        st.actHold = 0;
    }
    st.activityPrev = srcNormalized;
    return clamp(st.activityLevel, 0.f, 1.f);
}

float RoutingEngine::applyProgressiveDivider(float srcNormalized, RouteState::ProgDivState &st) {
    bool signNow = srcNormalized > 0.5f;
    if (signNow != st.progSign) {
        st.progCount += 1.f;
        st.progSign = signNow;
    }

    if (st.progCount >= st.progThreshold) {
        st.progOut = st.progOut > 0.5f ? 0.f : 1.f;
        st.progCount = 0.f;
        constexpr float growth = 1.25f;
        constexpr float add = 0.f;
        constexpr float thresholdMax = 128.f;
        st.progThreshold = std::min(st.progThreshold * growth + add, thresholdMax);
    } else {
        // recover threshold: tau ~1s at 1 kHz → decay ≈ 0.999
        constexpr float decay = 0.999f;
        if (st.progThreshold > 1.f) {
            st.progThreshold = std::max(1.f, st.progThreshold * decay);
        }
    }
    if (st.progThreshold >= 127.f) {
        if (++st.progHold > 2000) { // ~2s at 1 kHz before resetting
            st.progThreshold = 1.f;
            st.progHold = 0;
        }
    } else {
        st.progHold = 0;
    }

    // Slew the binary gate output over ~1s for smooth transitions
    constexpr float gateSlew = 0.001f; // tau ~1s at 1 kHz
    st.progOutSlewed += (st.progOut - st.progOutSlewed) * gateSlew;

    return st.progOutSlewed;
}

// Helper: apply a shaper to a TrackStateUnion, reading/writing the correct union member.
static inline float applyShaper(Routing::Shaper shaper, float shapedSource, float biasNormalized,
                                 RoutingEngine::RouteState::TrackStateUnion &st) {
    switch (shaper) {
    case Routing::Shaper::None:
        break;
    case Routing::Shaper::Crease:
        shapedSource = applyCreaseSource(shapedSource, biasNormalized);
        break;
    case Routing::Shaper::Location:
        shapedSource = applyLocation(shapedSource, st.locationState.location);
        break;
    case Routing::Shaper::Envelope:
        shapedSource = applyEnvelope(shapedSource, st.envelopeState.envelope);
        break;
    case Routing::Shaper::TriangleFold:
        shapedSource = applyTriangleFold(shapedSource, biasNormalized);
        break;
    case Routing::Shaper::FrequencyFollower:
        shapedSource = applyFrequencyFollower(shapedSource, st.freqFollowState);
        break;
    case Routing::Shaper::Activity:
        shapedSource = applyActivity(shapedSource, st.activityState);
        break;
    case Routing::Shaper::ProgressiveDivider:
        shapedSource = RoutingEngine::applyProgressiveDivider(shapedSource, st.progDivState);
        break;
    case Routing::Shaper::VcaNext:
        // VcaNext needs neighbor source value — handled in caller
        break;
    case Routing::Shaper::Last:
        break;
    }
    return shapedSource;
}

// for allowing direct mapping
static_assert(int(MidiPort::Midi) == int(Types::MidiPort::Midi), "invalid mapping");
static_assert(int(MidiPort::UsbMidi) == int(Types::MidiPort::UsbMidi), "invalid mapping");

RoutingEngine::RoutingEngine(Engine &engine, Model &model) :
    _engine(engine),
    _routing(model.project().routing()),
    _project(model.project())
{
    _cvRotateValues.fill(0.f);
    _cvRotateInterp.fill(false);
}

float RoutingEngine::routeSource(int index) const {
    if (index < 0 || index >= CONFIG_ROUTE_COUNT) {
        return 0.f;
    }
    return clamp(_sourceValues[index], 0.f, 1.f);
}

bool RoutingEngine::cvRotateInterpolated(int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return false;
    }
    return _cvRotateInterp[trackIndex];
}

float RoutingEngine::cvRotateValue(int trackIndex) const {
    if (trackIndex < 0 || trackIndex >= CONFIG_TRACK_COUNT) {
        return 0.f;
    }
    return _cvRotateValues[trackIndex];
}

void RoutingEngine::resetShaperState() {
    for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
        const auto &route = _routing.route(routeIndex);
        auto &routeState = _routeStates[routeIndex];
        resetRouteShaperState(routeState, route);
    }
}

void RoutingEngine::update() {
    updateSources();
    updateSinks();
}

bool RoutingEngine::receiveMidi(MidiPort port, const MidiMessage &message) {
    bool consumed = false;

    for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
        const auto &route = _routing.route(routeIndex);
        if (route.active() &&
            route.source() == Routing::Source::Midi &&
            MidiUtils::matchSource(port, message, route.midiSource().source())
        ) {
            const auto &midiSource = route.midiSource();
            auto &sourceValue = _sourceValues[routeIndex];
            switch (midiSource.event()) {
            case Routing::MidiSource::Event::ControlAbsolute:
                if (message.controlNumber() == midiSource.controlNumber()) {
                    sourceValue = message.controlValue() * (1.f / 127.f);
                    consumed = true;
                }
                break;
            case Routing::MidiSource::Event::ControlRelative:
                if (message.controlNumber() == midiSource.controlNumber()) {
                    int value = message.controlValue();
                    value = value >= 64 ? 64 - value : value;
                    sourceValue = clamp(sourceValue + value * (1.f / 127.f), 0.f, 1.f);
                    consumed = true;
                }
                break;
            case Routing::MidiSource::Event::PitchBend:
                if (message.isPitchBend()) {
                    sourceValue = (message.pitchBend() + 0x2000) * (1.f / 16383.f);
                    consumed = true;
                }
                break;
            case Routing::MidiSource::Event::NoteMomentary:
                if (message.isNoteOn() && message.note() == midiSource.note()) {
                    sourceValue = 1.f;
                    consumed = true;
                } else if (message.isNoteOff() && message.note() == midiSource.note()) {
                    sourceValue = 0.f;
                    consumed = true;
                }
                break;
            case Routing::MidiSource::Event::NoteToggle:
                if (message.isNoteOn() && message.note() == midiSource.note()) {
                    sourceValue = sourceValue < 0.5f ? 1.f : 0.f;
                    consumed = true;
                }
                break;
            case Routing::MidiSource::Event::NoteVelocity:
                if (message.isNoteOn() && message.note() == midiSource.note()) {
                    sourceValue = message.velocity() * (1.f / 127.f);
                    consumed = true;
                }
                break;
            case Routing::MidiSource::Event::NoteRange:
                if (message.isNoteOn() && message.note() >= midiSource.note() && message.note() < midiSource.note() + midiSource.noteRange()) {
                    sourceValue = (message.note() - midiSource.note()) / float(midiSource.noteRange() - 1);
                    consumed = true;
                }
                break;
            case Routing::MidiSource::Event::Last:
                break;
            }
        }
    }

    return consumed;
}

void RoutingEngine::updateSources() {
    for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
        const auto &route = _routing.route(routeIndex);
        if (route.active()) {
            auto &sourceValue = _sourceValues[routeIndex];
            switch (route.source()) {
            case Routing::Source::None:
                sourceValue = 0.f;
                break;
            case Routing::Source::CvIn1:
            case Routing::Source::CvIn2:
            case Routing::Source::CvIn3:
            case Routing::Source::CvIn4: {
                const auto &range = Types::voltageRangeInfo(route.cvSource().range());
                int index = int(route.source()) - int(Routing::Source::CvIn1);
                sourceValue = range.normalize(_engine.cvInput().channel(index));
                break;
            }
            case Routing::Source::CvOut1:
            case Routing::Source::CvOut2:
            case Routing::Source::CvOut3:
            case Routing::Source::CvOut4:
            case Routing::Source::CvOut5:
            case Routing::Source::CvOut6:
            case Routing::Source::CvOut7:
            case Routing::Source::CvOut8: {
                const auto &range = Types::voltageRangeInfo(route.cvSource().range());
                int index = int(route.source()) - int(Routing::Source::CvOut1);
                sourceValue = range.normalize(_engine.cvOutput().channel(index));
                break;
            }
            case Routing::Source::BusCv1:
            case Routing::Source::BusCv2:
            case Routing::Source::BusCv3:
            case Routing::Source::BusCv4: {
                const auto &range = Types::voltageRangeInfo(route.cvSource().range());
                int index = int(route.source()) - int(Routing::Source::BusCv1);
                sourceValue = range.normalize(_engine.busCv(index));
                break;
            }
            case Routing::Source::GateOut1:
            case Routing::Source::GateOut2:
            case Routing::Source::GateOut3:
            case Routing::Source::GateOut4:
            case Routing::Source::GateOut5:
            case Routing::Source::GateOut6:
            case Routing::Source::GateOut7:
            case Routing::Source::GateOut8: {
                int index = int(route.source()) - int(Routing::Source::GateOut1);
                sourceValue = (_engine.gateOutput() & (1 << index)) ? 1.f : 0.f;
                break;
            }
            case Routing::Source::Midi:
                // handled in receiveMidi
                break;
            case Routing::Source::Mod1:
            case Routing::Source::Mod2:
            case Routing::Source::Mod3:
            case Routing::Source::Mod4:
            case Routing::Source::Mod5:
            case Routing::Source::Mod6:
            case Routing::Source::Mod7:
            case Routing::Source::Mod8: {
                int index = int(route.source()) - int(Routing::Source::Mod1);
                sourceValue = _engine.modulatorEngine().currentValue(index) / 127.f;
                break;
            }
            case Routing::Source::Last:
                break;
            }
        }
    }
}

void RoutingEngine::updateSinks() {
    _cvRotateValues.fill(0.f);
    _cvRotateInterp.fill(false);
    _gateRotateMask = 0;
    _gateRotateAmount = 0;
    Routing::clearRouteOverrides();

    for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
        const auto &route = _routing.route(routeIndex);
        auto &routeState = _routeStates[routeIndex];

        bool routeChanged = route.target() != routeState.target || route.tracks() != routeState.tracks;
        // Check for shaper changes on all relevant tracks
        if (!routeChanged && Routing::isPerTrackTarget(route.target())) {
            for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
                if (route.shaper(i) != routeState.shaper[i]) {
                    routeChanged = true;
                    break;
                }
            }
        }
        // Bus targets also use shaper(0) — detect shaper changes there too
        if (!routeChanged && Routing::isBusTarget(route.target())) {
            if (route.shaper(0) != routeState.shaper[0]) {
                routeChanged = true;
            }
        }

        if (routeChanged) {
            // disable previous routing
            Routing::setRouted(routeState.target, routeState.tracks, false);
            // re-arm trigger edge state (Reset / PlayToggle / RecordToggle / TapTempo)
            routeState.gateMask = 0;
            // reset shaper state to correct initial values for the new route config
            resetRouteShaperState(routeState, route);
        }

        if (route.active()) {
            // A sourceless route is inert (spec §3: nothing active before a source
            // is committed). Overrides are cleared each frame, so skipping leaves the
            // target unmodulated instead of Modulate centering None=0 to a -full-scale delta.
            if (route.source() == Routing::Source::None) {
                continue;
            }

            auto target = route.target();

            if (Routing::isBusTarget(target)) {
                // Unified base-0 model: signed depthPct + combine, no min/max window.
                float volts = RouteFork::busDelta(_sourceValues[routeIndex], route.shaper(0),
                                                  route.depthPct(0), route.combine());
                int busIndex = int(target) - int(Routing::Target::BusCv1);
                _engine.setBusCv(busIndex, volts, Engine::BusWriterRouting);
            } else if (target == Routing::Target::GateOutputRotate) {
                // Group rotation (spec 018): route-level, not per-track. The track mask is
                // the group; one amount rotates it. Single group — lowest-index route wins.
                if (_gateRotateMask == 0) {
                    _gateRotateMask = route.tracks();
                    _gateRotateAmount = gateRotationFromSource(_sourceValues[routeIndex], route.depthPct(0), route.combine());
                }
            } else if (Routing::isPerTrackTarget(target)) {
                uint8_t tracks = route.tracks();

                float routeSpan = route.max() - route.min();
                for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                    if (tracks & (1 << trackIndex)) {
                        // Apply fork: migrated Note/PhaseFlux per-track params take the
                        // bias-free override path; old writeTarget is skipped for them.
                        uint8_t paramKey;
                        RouteParam::Range pRange;
                        if (RouteFork::migrated(_project.track(trackIndex).trackMode(), target, paramKey, pRange)) {
                            float delta = RouteFork::computeDelta(_sourceValues[routeIndex],
                                                                 route.shaper(trackIndex), route.depthPct(trackIndex), pRange,
                                                                 route.combine());
                            Routing::writeRouteOverride(paramKey, trackIndex, delta);
                            continue;
                        }

                        // Base-less shell triggers: consume the raw source (no bias/depth/
                        // window/shaper). Reset = rising edge; Run = threshold toggle.
                        if (target == Routing::Target::Reset) {
                            if (RouteParam::gateRisingEdge(routeState.gateMask, trackIndex, _sourceValues[routeIndex])) {
                                _engine.trackEngine(trackIndex).reset();
                            }
                            continue;
                        }
                        if (target == Routing::Target::Run) {
                            _project.track(trackIndex).setRunGate(routeRunGate(_sourceValues[routeIndex]), true);
                            continue;
                        }

                        float shapedSource = applyBiasDepthToSource(_sourceValues[routeIndex], route, trackIndex);
                        float biasNormalized = route.biasPct(trackIndex) * 0.01f;
                        float shaperOut = shapedSource;
                        auto shaper = route.shaper(trackIndex);
                        auto &st = routeState.shaperState[trackIndex];
                        switch (shaper) {
                        case Routing::Shaper::None:
                            break;
                        case Routing::Shaper::Crease:
                            shaperOut = applyCreaseSource(shapedSource, biasNormalized);
                            break;
                        case Routing::Shaper::Location:
                            shaperOut = applyLocation(shapedSource, st.locationState.location);
                            break;
                        case Routing::Shaper::Envelope:
                            shaperOut = applyEnvelope(shapedSource, st.envelopeState.envelope);
                            break;
                        case Routing::Shaper::TriangleFold:
                            shaperOut = applyTriangleFold(shapedSource, biasNormalized);
                            break;
                        case Routing::Shaper::FrequencyFollower:
                            shaperOut = applyFrequencyFollower(shapedSource, st.freqFollowState);
                            break;
                        case Routing::Shaper::Activity:
                            shaperOut = applyActivity(shapedSource, st.activityState);
                            break;
                        case Routing::Shaper::ProgressiveDivider:
                            shaperOut = applyProgressiveDivider(shapedSource, st.progDivState);
                            break;
                        case Routing::Shaper::VcaNext: {
                            int nextRouteIndex = (routeIndex + 1) % CONFIG_ROUTE_COUNT;
                            float neighbor = _sourceValues[nextRouteIndex];
                            // VCA: Center-referenced amplitude modulation
                            shaperOut = 0.5f + (shapedSource - 0.5f) * neighbor;
                            break;
                        }
                        case Routing::Shaper::Last:
                            break;
                        }
                        float routed = route.min() + shaperOut * routeSpan;

                        // Remaining legacy-branch targets: CvOutputRotate / GateOutputRotate
                        // (Run/Reset handled above on the clean path).
                        _routing.writeTarget(target, (1 << trackIndex), routed);

                        if (target == Routing::Target::CvOutputRotate) {
                            _cvRotateValues[trackIndex] = Routing::denormalizeTargetValue(target, routed);
                            _cvRotateInterp[trackIndex] = route.cvRotateInterpolate();
                        }
                    }
                }
            } else if (Routing::isEngineTarget(target)) {
                float baseValue = route.min() + _sourceValues[routeIndex] * (route.max() - route.min());
                writeEngineTarget(target, baseValue, routeState);
            } else if (Routing::isProjectTarget(target)) {
                // Global (Tempo/Swing/CVR): base-anchored override at GlobalTrack,
                // skipping old absolute writeTarget. No track dimension -> slot 0
                // carries the route's shaper/depth.
                uint8_t gKey; RouteParam::Range gRange;
                if (RouteFork::migratedGlobal(target, gKey, gRange)) {
                    float delta = RouteFork::computeDelta(_sourceValues[routeIndex],
                                                          route.shaper(0), route.depthPct(0), gRange,
                                                          route.combine());
                    Routing::writeRouteOverride(gKey, Routing::GlobalTrack, delta);
                }
            } else {
                float baseValue = route.min() + _sourceValues[routeIndex] * (route.max() - route.min());
                _routing.writeTarget(target, route.tracks(), baseValue);
            }
        } else {
            // Inactive route: reset shaper state using the route's configured shapers
            // so that re-activating starts from correct initial values
            resetRouteShaperState(routeState, route);
        }

        if (routeChanged) {
            // enable new routing
            Routing::setRouted(route.target(), route.tracks(), true);
            // save state
            routeState.target = route.target();
            routeState.tracks = route.tracks();
            if (Routing::isPerTrackTarget(route.target())) {
                for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
                    routeState.shaper[i] = route.shaper(i);
                }
            } else {
                routeState.shaper.fill(Routing::Shaper::None);
            }
        }
    }
}

void RoutingEngine::writeEngineTarget(Routing::Target target, float normalized, RouteState &routeState) {
    bool active = normalized > 0.5f;

    switch (target) {
    case Routing::Target::Play:
        // level-following: state mirrors the routed value
        if (active != _engine.clockRunning()) {
            _engine.togglePlay();
        }
        break;
    case Routing::Target::Record:
        // level-following
        if (active != _engine.recording()) {
            _engine.toggleRecording();
        }
        break;
    case Routing::Target::PlayToggle:
        if (RouteParam::gateRisingEdge(routeState.gateMask, 0, normalized)) {
            _engine.togglePlay();
        }
        break;
    case Routing::Target::RecordToggle:
        if (RouteParam::gateRisingEdge(routeState.gateMask, 0, normalized)) {
            _engine.toggleRecording();
        }
        break;
    case Routing::Target::TapTempo:
        if (RouteParam::gateRisingEdge(routeState.gateMask, 0, normalized)) {
            _engine.tapTempoTap();
        }
        break;
    default:
        break;
    }
}
