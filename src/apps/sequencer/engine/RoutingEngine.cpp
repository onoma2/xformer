#include "RoutingEngine.h"

#include "model/RouteParam.h"
#include "model/RouteResolve.h"

#include "RouteShellTrigger.h"
#include "GateRotation.h"
#include "Engine.h"
#include "MidiUtils.h"
#include "core/math/Math.h"


// for allowing direct mapping
static_assert(int(MidiPort::Midi) == int(Types::MidiPort::Midi), "invalid mapping");
static_assert(int(MidiPort::UsbMidi) == int(Types::MidiPort::UsbMidi), "invalid mapping");

RoutingEngine::RoutingEngine(Engine &engine, Model &model) :
    _engine(engine),
    _routing(model.project().routing()),
    _project(model.project())
{
}

float RoutingEngine::routeSource(int index) const {
    if (index < 0 || index >= CONFIG_ROUTE_COUNT) {
        return 0.f;
    }
    return clamp(_sourceValues[index], 0.f, 1.f);
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

float RoutingEngine::resolveSourceValue(const Routing::Route &route, Routing::Source source) const {
    switch (source) {
    case Routing::Source::None:
        return 0.f;
    case Routing::Source::CvIn1:
    case Routing::Source::CvIn2:
    case Routing::Source::CvIn3:
    case Routing::Source::CvIn4: {
        const auto &range = Types::voltageRangeInfo(route.cvSource().range());
        int index = int(source) - int(Routing::Source::CvIn1);
        return range.normalize(_engine.cvInput().channel(index));
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
        int index = int(source) - int(Routing::Source::CvOut1);
        return range.normalize(_engine.cvOutput().channel(index));
    }
    case Routing::Source::BusCv1:
    case Routing::Source::BusCv2:
    case Routing::Source::BusCv3:
    case Routing::Source::BusCv4: {
        const auto &range = Types::voltageRangeInfo(route.cvSource().range());
        int index = int(source) - int(Routing::Source::BusCv1);
        return range.normalize(_engine.busCv(index));
    }
    case Routing::Source::GateOut1:
    case Routing::Source::GateOut2:
    case Routing::Source::GateOut3:
    case Routing::Source::GateOut4:
    case Routing::Source::GateOut5:
    case Routing::Source::GateOut6:
    case Routing::Source::GateOut7:
    case Routing::Source::GateOut8: {
        int index = int(source) - int(Routing::Source::GateOut1);
        return (_engine.gateOutput() & (1 << index)) ? 1.f : 0.f;
    }
    case Routing::Source::Midi:
        // set asynchronously in receiveMidi; not resolvable synchronously here
        return 0.f;
    case Routing::Source::Mod1:
    case Routing::Source::Mod2:
    case Routing::Source::Mod3:
    case Routing::Source::Mod4:
    case Routing::Source::Mod5:
    case Routing::Source::Mod6:
    case Routing::Source::Mod7:
    case Routing::Source::Mod8: {
        int index = int(source) - int(Routing::Source::Mod1);
        return _engine.modulatorEngine().currentValue(index) / 127.f;
    }
    case Routing::Source::Last:
        return 0.f;
    }
    return 0.f;
}

void RoutingEngine::updateSources() {
    for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
        const auto &route = _routing.route(routeIndex);
        if (route.active() && route.source() != Routing::Source::Midi) {
            // MIDI is left as set in receiveMidi; everything else resolves from live I/O.
            _sourceValues[routeIndex] = resolveSourceValue(route, route.source());
        }
    }
}

void RoutingEngine::updateSinks() {
    _gateRotateMask = 0;
    _gateRotateAmount = 0;
    _cvRotateMask = 0;
    _cvRotateAmount = 0;
    Routing::clearRouteOverrides();

    for (int routeIndex = 0; routeIndex < CONFIG_ROUTE_COUNT; ++routeIndex) {
        const auto &route = _routing.route(routeIndex);
        auto &routeState = _routeStates[routeIndex];

        bool routeChanged = route.target() != routeState.target || route.tracks() != routeState.tracks;

        if (routeChanged) {
            // disable previous routing
            Routing::setRouted(routeState.target, routeState.tracks, false);
            // re-arm trigger edge state (Reset / PlayToggle / RecordToggle / TapTempo)
            routeState.gateMask = 0;
        }

        if (route.active()) {
            // A sourceless route is inert (spec §3: nothing active before a source
            // is committed). Overrides are cleared each frame, so skipping leaves the
            // target unmodulated instead of Modulate centering None=0 to a -full-scale delta.
            if (route.source() == Routing::Source::None) {
                continue;
            }

            auto target = route.target();

            // scaleSource is a dynamic gain on the route's modulation depth (None -> 1.0
            // identity). Resolved like any source; bypass to 1.0 if it would self-reference
            // the route's own bus (the only within-frame feedback path).
            float scaleValue = 1.f;
            auto scaleSrc = route.scaleSource();
            if (scaleSrc != Routing::Source::None && !Routing::isBusSelfRoute(scaleSrc, target)) {
                scaleValue = clamp(resolveSourceValue(route, scaleSrc), 0.f, 1.f);
            }

            if (Routing::isBusTarget(target)) {
                // Unified base-0 model: signed depthPct + combine, no min/max window.
                float volts = RouteResolve::busDelta(_sourceValues[routeIndex], route.shaper(0),
                                                  route.depthPct(0), route.combine(), scaleValue);
                int busIndex = int(target) - int(Routing::Target::BusCv1);
                _engine.setBusCv(busIndex, volts, Engine::BusWriterRouting);
            } else if (target == Routing::Target::GateOutputRotate) {
                // Group rotation (spec 018): route-level, not per-track. The track mask is
                // the group; one amount (from the first masked track's depth) rotates it.
                // Single group — lowest-index route wins.
                if (_gateRotateMask == 0) {
                    _gateRotateMask = route.tracks();
                    _gateRotateAmount = gateRotationFromSource(_sourceValues[routeIndex], route.depthPct(firstMaskedSlot(route.tracks())), route.combine());
                }
            } else if (target == Routing::Target::CvOutputRotate) {
                // Discrete CV group rotation (spec 019), mirror of gate.
                if (_cvRotateMask == 0) {
                    _cvRotateMask = route.tracks();
                    _cvRotateAmount = gateRotationFromSource(_sourceValues[routeIndex], route.depthPct(firstMaskedSlot(route.tracks())), route.combine());
                }
            } else if (Routing::isPerTrackTarget(target)) {
                uint8_t tracks = route.tracks();
                for (int trackIndex = 0; trackIndex < CONFIG_TRACK_COUNT; ++trackIndex) {
                    if (tracks & (1 << trackIndex)) {
                        // Per-track override params take the bias-free override path; the
                        // legacy writeTarget fallthrough is skipped for them.
                        uint8_t paramKey;
                        RouteParam::Range pRange;
                        if (RouteResolve::overrideParam(_project.track(trackIndex).trackMode(), target, paramKey, pRange)) {
                            float delta = RouteResolve::computeDelta(_sourceValues[routeIndex],
                                                                 route.shaper(trackIndex), route.depthPct(trackIndex), pRange,
                                                                 route.combine(), scaleValue);
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
                        // Anything else is a mismatched (target, mode) pair (the matrix won't
                        // create one) — no-op. The legacy bias/window/shaper branch is gone.
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
                if (RouteResolve::overrideParamGlobal(target, gKey, gRange)) {
                    float delta = RouteResolve::computeDelta(_sourceValues[routeIndex],
                                                          route.shaper(0), route.depthPct(0), gRange,
                                                          route.combine(), scaleValue);
                    Routing::writeRouteOverride(gKey, Routing::GlobalTrack, delta);
                }
            } else {
                float baseValue = route.min() + _sourceValues[routeIndex] * (route.max() - route.min());
                _routing.writeTarget(target, route.tracks(), baseValue);
            }
        }

        if (routeChanged) {
            // enable new routing
            Routing::setRouted(route.target(), route.tracks(), true);
            // save state
            routeState.target = route.target();
            routeState.tracks = route.tracks();
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
