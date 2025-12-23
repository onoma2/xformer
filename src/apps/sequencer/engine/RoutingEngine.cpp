#include "RoutingEngine.h"

#include "Engine.h"
#include "MidiUtils.h"
#include "core/math/Math.h"

// Apply per-track bias/depth to the normalized source (0..1) before the route window is applied.
static inline float applyBiasDepthToSource(float srcNormalized, const Routing::Route &route, int trackIndex) {
    float depth = route.depthPct(trackIndex) * 0.01f;
    float bias = route.biasPct(trackIndex) * 0.01f;
    float shaped = 0.5f + (srcNormalized - 0.5f) * depth + bias;
    return clamp(shaped, 0.f, 1.f);
}

// Target-agnostic waveslicer: fold around 0.5 in normalized source space with a fixed ±0.5 jump.
static inline float applyCreaseSource(float normalized) {
    constexpr float creaseAmount = 0.5f;
    float creased = normalized + (normalized <= 0.5f ? creaseAmount : -creaseAmount);
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

static inline float applyTriangleFold(float srcNormalized) {
    float x = 2.f * (srcNormalized - 0.5f); // -1..1
    float folded = (x > 0.f) ? 1.f - 2.f * fabsf(x - 0.5f) : -1.f + 2.f * fabsf(x + 0.5f);
    return clamp(0.5f + 0.5f * folded, 0.f, 1.f);
}

static inline float applyFrequencyFollower(float srcNormalized, RoutingEngine::RouteState::TrackState &st) {
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

static inline float applyActivity(float srcNormalized, RoutingEngine::RouteState::TrackState &st) {
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

float RoutingEngine::applyProgressiveDivider(float srcNormalized, RouteState::TrackState &st) {
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

// for allowing direct mapping
static_assert(int(MidiPort::Midi) == int(Types::MidiPort::Midi), "invalid mapping");
static_assert(int(MidiPort::UsbMidi) == int(Types::MidiPort::UsbMidi), "invalid mapping");

RoutingEngine::RoutingEngine(Engine &engine, Model &model) :
    _engine(engine),
    _routing(model.project().routing())
{
    _lastResetActive.fill(false);
}

void RoutingEngine::resetShaperState() {
    for (auto &routeState : _routeStates) {
        for (auto &st : routeState.shaperState) {
            st = RouteState::TrackState();
        }
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
            case Routing::Source::Midi:
                // handled in receiveMidi
                break;
            case Routing::Source::Last:
                break;
            }
        }
    }
}

void RoutingEngine::updateSinks() {
    for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
        const auto &route = _routing.route(routeIndex);
        auto &routeState = _routeStates[routeIndex];

        bool routeChanged = route.target() != routeState.target || route.tracks() != routeState.tracks;
        if (!routeChanged && Routing::isPerTrackTarget(route.target())) {
            for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
                if (route.shaper(i) != routeState.shaper[i]) {
                    routeChanged = true;
                    break;
                }
            }
        }

        if (routeChanged) {
            // disable previous routing
            Routing::setRouted(routeState.target, routeState.tracks, false);
            // reset last state for play/record toggle
            if (routeState.target == Routing::Target::PlayToggle) {
                _lastPlayToggleActive = false;
            }
            if (routeState.target == Routing::Target::RecordToggle) {
                _lastRecordToggleActive = false;
            }
            if (routeState.target == Routing::Target::Reset) {
                for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
                    if (routeState.tracks & (1 << i)) {
                        _lastResetActive[i] = false;
                    }
                }
            }
            // reset shaper state
            for (auto &st : routeState.shaperState) {
                st = RouteState::TrackState();
            }
        }

        if (route.active()) {
            auto target = route.target();

            if (Routing::isPerTrackTarget(target)) {
                uint8_t tracks = route.tracks();

                float routeSpan = route.max() - route.min();
                for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                    if (tracks & (1 << trackIndex)) {
                        float shapedSource = applyBiasDepthToSource(_sourceValues[routeIndex], route, trackIndex);
                        // select shaper
                        float shaperOut = shapedSource;
                        auto shaper = route.shaper(trackIndex);
                        auto &st = routeState.shaperState[trackIndex];
                        switch (shaper) {
                        case Routing::Shaper::None:
                            break;
                        case Routing::Shaper::Crease:
                            shaperOut = applyCreaseSource(shapedSource);
                            break;
                        case Routing::Shaper::Location:
                            shaperOut = applyLocation(shapedSource, st.location);
                            break;
                        case Routing::Shaper::Envelope:
                            shaperOut = applyEnvelope(shapedSource, st.envelope);
                            break;
                        case Routing::Shaper::TriangleFold:
                            shaperOut = applyTriangleFold(shapedSource);
                            break;
                        case Routing::Shaper::FrequencyFollower:
                            shaperOut = applyFrequencyFollower(shapedSource, st);
                            break;
                        case Routing::Shaper::Activity:
                            shaperOut = applyActivity(shapedSource, st);
                            break;
                        case Routing::Shaper::ProgressiveDivider:
                            shaperOut = applyProgressiveDivider(shapedSource, st);
                            break;
                        case Routing::Shaper::Last:
                            break;
                        }
                        if (route.creaseEnabled(trackIndex) && shaper != Routing::Shaper::Crease) {
                            shaperOut = applyCreaseSource(shaperOut);
                        }
                        float routed = route.min() + shaperOut * routeSpan;

                        // Special handling for Reset target
                        if (target == Routing::Target::Reset) {
                            bool active = routed > 0.5f;
                            if (active != _lastResetActive[trackIndex]) {
                                if (active) {
                                    // Rising edge detected - reset this track
                                    _engine.trackEngine(trackIndex).reset();
                                }
                                _lastResetActive[trackIndex] = active;
                            }
                        } else {
                            // Normal target handling
                            _routing.writeTarget(target, (1 << trackIndex), routed);
                        }
                    }
                }
            } else if (Routing::isEngineTarget(target)) {
                float baseValue = route.min() + _sourceValues[routeIndex] * (route.max() - route.min());
                writeEngineTarget(target, baseValue);
            } else {
                float baseValue = route.min() + _sourceValues[routeIndex] * (route.max() - route.min());
                _routing.writeTarget(target, route.tracks(), baseValue);
            }
        } else {
            for (auto &st : routeState.shaperState) {
                st = RouteState::TrackState();
            }
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

void RoutingEngine::writeEngineTarget(Routing::Target target, float normalized) {
    bool active = normalized > 0.5f;

    switch (target) {
    case Routing::Target::Play:
        if (active != _engine.clockRunning()) {
            _engine.togglePlay();
        }
        break;
    case Routing::Target::PlayToggle:
        if (active != _lastPlayToggleActive) {
            if (active) {
                _engine.togglePlay();
            }
            _lastPlayToggleActive = active;
        }
        break;
    case Routing::Target::Record:
        if (active != _engine.recording()) {
            _engine.toggleRecording();
        }
        break;
    case Routing::Target::RecordToggle:
        if (active != _lastRecordToggleActive) {
            if (active) {
                _engine.toggleRecording();
            }
            _lastRecordToggleActive = active;
        }
        break;
    case Routing::Target::TapTempo:
        {
            static bool lastActive = false;
            if (active != lastActive) {
                if (active) {
                    _engine.tapTempoTap();
                }
                lastActive = active;
            }
        }
        break;
    default:
        break;
    }
}
