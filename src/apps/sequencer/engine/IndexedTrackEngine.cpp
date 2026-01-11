#include "IndexedTrackEngine.h"

#include "Engine.h"
#include "Slide.h"

#include <algorithm>
#include <cmath>

namespace {

// Constants matching IndexedTrackEngine class constants
constexpr uint16_t MAX_DURATION = IndexedSequence::MaxDuration;
constexpr int8_t MIN_NOTE_INDEX = -63;
constexpr int8_t MAX_NOTE_INDEX = 64;

// Helper: Check if step matches route target groups
inline bool stepMatchesRouteGroups(const IndexedSequence::Step &step, const IndexedSequence::RouteConfig &cfg) {
    uint8_t targetGroups = cfg.targetGroups;
    return targetGroups == IndexedSequence::TargetGroupsAll ||
           (targetGroups == IndexedSequence::TargetGroupsUngrouped && step.groupMask() == 0) ||
           (step.groupMask() & targetGroups);
}

// Helper: Combine two modulation values based on mode
inline float combineModulation(float a, float b, IndexedSequence::RouteCombineMode mode) {
    switch (mode) {
    case IndexedSequence::RouteCombineMode::Mux: return 0.5f * (a + b);
    case IndexedSequence::RouteCombineMode::Min: return std::min(a, b);
    case IndexedSequence::RouteCombineMode::Max: return std::max(a, b);
    case IndexedSequence::RouteCombineMode::AtoB:
    case IndexedSequence::RouteCombineMode::Last:
        break;
    }
    return a;
}

// Helper: Resolve modulation from two routes (A and B)
inline float resolveModulation(float modA, bool modAActive, float modB, bool modBActive,
                                IndexedSequence::RouteCombineMode mode) {
    if (modAActive && modBActive) {
        return combineModulation(modA, modB, mode);
    }
    return modAActive ? modA : (modBActive ? modB : 0.0f);
}

// Helper: Apply duration modulation (multiplicative, percentage-based)
inline void applyDurationModulation(float modValue, uint16_t &duration) {
    float modded = static_cast<float>(duration) * (1.0f + modValue);
    int newDuration = static_cast<int>(std::lround(modded));
    duration = clamp(newDuration, 0, int(MAX_DURATION));
}

// Helper: Apply gate length modulation (additive)
inline void applyGateLengthModulation(float modValue, uint16_t &gateValue, uint16_t durationTicks) {
    int ticks = int(IndexedSequence::gateTicks(gateValue, durationTicks));
    int newTicks = ticks + int(std::lround(modValue));
    gateValue = IndexedSequence::gateEncodeTicksForDuration(newTicks, durationTicks);
}

// Helper: Apply note index modulation (additive, semitone transpose)
inline void applyNoteModulation(float modValue, int8_t &note) {
    int newNote = static_cast<int>(note + modValue);
    note = clamp(newNote, int(MIN_NOTE_INDEX), int(MAX_NOTE_INDEX));
}

// Unified modulation application: handles both combined and sequential routing
inline void applyStepModulation(
    IndexedSequence::ModTarget target,
    float cvA, float cvB,
    const IndexedSequence::RouteConfig &routeA,
    const IndexedSequence::RouteConfig &routeB,
    bool aActive, bool bActive,
    bool combineMode,
    IndexedSequence::RouteCombineMode combineType,
    int scaleSize,
    uint16_t &duration,
    uint16_t &gateValue,
    int8_t &note)
{
    // Calculate modulation values from each route
    float modA = 0.0f;
    float modB = 0.0f;

    if (aActive && routeA.targetParam == target) {
        if (target == IndexedSequence::ModTarget::Duration) {
            modA = cvA * (routeA.amount * 0.01f); // Percentage for duration
        } else if (target == IndexedSequence::ModTarget::NoteIndex) {
            modA = cvA * (routeA.amount * 0.01f * scaleSize); // 100% = 1 octave in current scale
        } else {
            modA = cvA * routeA.amount; // Direct value for gate ticks
        }
    }

    if (bActive && routeB.targetParam == target) {
        if (target == IndexedSequence::ModTarget::Duration) {
            modB = cvB * (routeB.amount * 0.01f); // Percentage for duration
        } else if (target == IndexedSequence::ModTarget::NoteIndex) {
            modB = cvB * (routeB.amount * 0.01f * scaleSize); // 100% = 1 octave in current scale
        } else {
            modB = cvB * routeB.amount; // Direct value for gate ticks
        }
    }

    // Resolve final modulation (combine or sequential)
    float finalMod = 0.0f;
    if (combineMode && aActive && bActive) {
        finalMod = resolveModulation(modA, true, modB, true, combineType);
    } else {
        // Sequential: add both (if targeting same param, only one will be non-zero)
        finalMod = modA + modB;
    }

    // Apply modulation to appropriate parameter
    if (finalMod != 0.0f) {
        switch (target) {
        case IndexedSequence::ModTarget::Duration:
            applyDurationModulation(finalMod, duration);
            break;
        case IndexedSequence::ModTarget::GateLength:
            applyGateLengthModulation(finalMod, gateValue, duration);
            break;
        case IndexedSequence::ModTarget::NoteIndex:
            applyNoteModulation(finalMod, note);
            break;
        case IndexedSequence::ModTarget::Last:
            break;
        }
    }
}

} // anonymous namespace

float IndexedTrackEngine::routedSync() const {
    // Reuse DMap sync target for external resets
    return _indexedTrack.routedSync();
}

void IndexedTrackEngine::resetSequenceState() {
    _sequenceState.reset();
    _stepsRemaining = std::max(0, _sequence->activeLength() - 1);
    _currentStepIndex = 0;
    _stepTimer = 0.0;
    _gateTimer = 0;
    _effectiveStepDuration = 0;
    _cvOutput = 0.0f;
    _cvOutputTarget = 0.0f;
    _running = true;
    _activity = false;
    _slideActive = false;
    _lastFreeTickPos = _engine.clock().tickPosition();

    if (_sequence->activeLength() > 0) {
        _sequenceState.advanceFree(_sequence->runMode(), 0, _sequence->activeLength() - 1, _rng);
        _currentStepIndex = _sequenceState.step();
    }
    primeNextStep();
}

void IndexedTrackEngine::reset() {
    int currentPattern = pattern();
    _sequence = &_indexedTrack.sequence(currentPattern);
    _cachedPattern = currentPattern;

    _prevSync = routedSync();
    resetSequenceState();
}

void IndexedTrackEngine::changePattern() {
    int currentPattern = pattern();
    _sequence = &_indexedTrack.sequence(currentPattern);
    _cachedPattern = currentPattern;
    // Keep playback position when changing patterns
    // (could optionally reset to step 0 here)
}

void IndexedTrackEngine::restart() {
    _prevSync = routedSync();
    resetSequenceState();
}

TrackEngine::TickResult IndexedTrackEngine::tick(uint32_t tick) {
    // Only update sequence pointer when pattern actually changes
    int currentPattern = pattern();
    if (currentPattern != _cachedPattern) {
        _sequence = &_indexedTrack.sequence(currentPattern);
        _cachedPattern = currentPattern;
    }

    // Sync handling (Off / ResetMeasure / External)
    switch (_sequence->syncMode()) {
    case IndexedSequence::SyncMode::ResetMeasure: {
        uint32_t resetDivisor = _sequence->resetMeasure() * _engine.measureDivisor();
        if (resetDivisor > 0 && (tick % resetDivisor) == 0) {
            resetSequenceState();
        }
        break;
    }
    case IndexedSequence::SyncMode::External: {
        float syncVal = routedSync();
        if (_prevSync <= 0.f && syncVal > 0.f) {
            // External sync reset detected
            resetSequenceState();
        }
        _prevSync = syncVal;
        break;
    }
    case IndexedSequence::SyncMode::Off:
    case IndexedSequence::SyncMode::Last:
        break;
    }

    if (!_running) {
        return TickResult::NoUpdate;
    }

    // Fire pending trigger (set during reset/sync)
    if (_pendingTrigger) {
        triggerStep();
        _pendingTrigger = false;
    }

    // 1. Handle Gate (counts down independently of step progress in both modes)
    if (_gateTimer > 0) {
        _gateTimer--;
        // Send MIDI gate OFF when timer expires
        if (_gateTimer == 0) {
            _engine.midiOutputEngine().sendGate(_track.trackIndex(), false);
        }
    }

    // 2. Step Advancement (mode-specific)
    switch (_indexedTrack.playMode()) {
    case Types::PlayMode::Aligned:
    case Types::PlayMode::Free: {
        // Duration-based advancement using wallclock-derived tick position.
        // ResetMeasure/external sync are handled above, so this stays phase-locked.
        const uint16_t stepDuration = static_cast<uint16_t>(_effectiveStepDuration);

        // Zero duration steps: rapid-fire through immediately
        if (stepDuration == 0) {
            advanceStep();  // This skips zero-duration steps automatically
            triggerStep();
            // Next tick will handle the new step
            return TickResult::NoUpdate;
        }

        double tickPos = _engine.clock().tickPosition();
        double deltaTicks = tickPos - _lastFreeTickPos;
        if (deltaTicks < 0.0) {
            deltaTicks = 0.0;
        }
        _lastFreeTickPos = tickPos;

        if (_effectiveStepDuration > 0) {
            const double maxDeltaTicks = _effectiveStepDuration * 2.0;
            if (deltaTicks > maxDeltaTicks) {
                deltaTicks = maxDeltaTicks;
            }
        }

        _stepTimer += deltaTicks;

        // Check for step transition
        if (_stepTimer >= stepDuration) {
            advanceStep();  // This automatically skips zero-duration steps
            triggerStep();   // Trigger the new step we just advanced to
        }
        break;
    }
    case Types::PlayMode::Last:
        break;
    }

    return TickResult::NoUpdate;
}

void IndexedTrackEngine::update(float dt) {
    bool running = _engine.state().running();
    bool stepMonitoring = (!running && _monitorStepIndex >= 0);

    auto sendToMidiOutputEngine = [this] (bool gate, float cv = 0.f) {
        auto &midiOutputEngine = _engine.midiOutputEngine();
        midiOutputEngine.sendGate(_track.trackIndex(), gate);
        if (gate) {
            midiOutputEngine.sendCv(_track.trackIndex(), cv);
            midiOutputEngine.sendSlide(_track.trackIndex(), false);
        }
    };

    auto setOverride = [&] (float cv) {
        _cvOutputTarget = cv;
        _cvOutput = cv;
        _slideActive = false;
        _activity = _gateOutput = true;
        _monitorOverrideActive = true;
        sendToMidiOutputEngine(true, cv);
    };

    auto clearOverride = [&] () {
        if (_monitorOverrideActive) {
            _activity = _gateOutput = false;
            _monitorOverrideActive = false;
            sendToMidiOutputEngine(false);
        }
    };

    if (stepMonitoring) {
        const auto &step = _sequence->step(_monitorStepIndex);
        setOverride(noteIndexToVoltage(step.noteIndex()));
    } else {
        clearOverride();
    }

    if (_slideActive && _indexedTrack.slideTime() > 0) {
        _cvOutput = Slide::applySlide(_cvOutput, _cvOutputTarget, _indexedTrack.slideTime(), dt);
    } else {
        _cvOutput = _cvOutputTarget;
    }
}

void IndexedTrackEngine::advanceStep() {
    const int activeLength = _sequence->activeLength();
    if (activeLength <= 0) {
        return;
    }

    if (!_sequence->loop() && _stepsRemaining <= 0) {
        _running = false;
        _stepTimer = 0.0;
        return;
    }

    int skipCounter = 0;
    while (skipCounter < activeLength) {
        _sequenceState.advanceFree(_sequence->runMode(), 0, activeLength - 1, _rng);
        _currentStepIndex = _sequenceState.step();

        if (!_sequence->loop()) {
            _stepsRemaining--;
        }

        int effectiveIndex = (_currentStepIndex + _sequence->firstStep()) % activeLength;
        uint16_t duration = _sequence->step(effectiveIndex).duration();
        if (duration > 0) {
            break;
        }

        skipCounter++;
        if (!_sequence->loop() && _stepsRemaining <= 0) {
            _running = false;
            _stepTimer = 0.0;
            return;
        }
    }

    _stepTimer = 0.0;
}

void IndexedTrackEngine::triggerStep() {
    int effectiveStepIndex = (_currentStepIndex + _sequence->firstStep()) % _sequence->activeLength();
    const auto &step = _sequence->step(effectiveStepIndex);

    // Get base values from step
    uint16_t baseDuration = step.duration();
    uint16_t baseGateValue = step.gateLength();
    int8_t baseNote = step.noteIndex();

    // Apply route modulation (unified for both combined and sequential modes)
    const auto &routeA = _sequence->routeA();
    const auto &routeB = _sequence->routeB();
    const bool aActive = routeA.source != IndexedSequence::RouteSource::Off && stepMatchesRouteGroups(step, routeA);
    const bool bActive = routeB.source != IndexedSequence::RouteSource::Off && stepMatchesRouteGroups(step, routeB);

    if (aActive || bActive) {
        const float cvA = _sequence->routedIndexedA();
        const float cvB = _sequence->routedIndexedB();
        const auto combineMode = _sequence->routeCombineMode();

        // Get CV value based on route source
        const float routeAValue = (routeA.source == IndexedSequence::RouteSource::A) ? cvA : cvB;
        const float routeBValue = (routeB.source == IndexedSequence::RouteSource::A) ? cvA : cvB;

        // Get scale size for note index routing (100% = 1 octave)
        const Scale &scale = _sequence->selectedScale(_model.project().selectedScale());
        int scaleSize = scale.notesPerOctave();

        // Check if routes should be combined
        const bool shouldCombine = combineMode != IndexedSequence::RouteCombineMode::AtoB &&
                                   aActive && bActive &&
                                   routeA.targetParam == routeB.targetParam;

        // Apply modulation for each possible target parameter
        applyStepModulation(IndexedSequence::ModTarget::Duration, routeAValue, routeBValue,
                           routeA, routeB, aActive, bActive, shouldCombine, combineMode,
                           scaleSize, baseDuration, baseGateValue, baseNote);

        applyStepModulation(IndexedSequence::ModTarget::GateLength, routeAValue, routeBValue,
                           routeA, routeB, aActive, bActive, shouldCombine, combineMode,
                           scaleSize, baseDuration, baseGateValue, baseNote);

        applyStepModulation(IndexedSequence::ModTarget::NoteIndex, routeAValue, routeBValue,
                           routeA, routeB, aActive, bActive, shouldCombine, combineMode,
                           scaleSize, baseDuration, baseGateValue, baseNote);
    }

    // Calculate gate duration in ticks
    uint16_t effectiveDuration = baseDuration;
    if (baseDuration > 0) {
        float clockMult = _sequence->clockMultiplier() * 0.01f;
        uint32_t scaledDuration = std::lround(baseDuration / clockMult);
        scaledDuration = std::max<uint32_t>(1, scaledDuration);
        scaledDuration = std::min<uint32_t>(scaledDuration, 65535u);
        effectiveDuration = static_cast<uint16_t>(scaledDuration);
    }

    uint32_t gateTicks = IndexedSequence::gateTicks(baseGateValue, effectiveDuration);
    if (effectiveDuration > 0) {
        gateTicks = std::min<uint32_t>(gateTicks, effectiveDuration);
    }

    // Set gate timer
    _gateTimer = gateTicks;
    _effectiveStepDuration = effectiveDuration;

    // Send MIDI gate ON/OFF
    bool finalGate = (!mute() || fill()) && (gateTicks > 0);
    _engine.midiOutputEngine().sendGate(_track.trackIndex(), finalGate);

    // Calculate CV output (direct Scale lookup, no octave/modulo math)
    if (_indexedTrack.cvUpdateMode() == IndexedTrack::CvUpdateMode::Always || gateTicks > 0) {
        _cvOutputTarget = noteIndexToVoltage(baseNote);
        _slideActive = step.slide();

        // Send MIDI CV and slide (respect CvUpdateMode)
        if (!mute() || _indexedTrack.cvUpdateMode() == IndexedTrack::CvUpdateMode::Always) {
            _engine.midiOutputEngine().sendCv(_track.trackIndex(), _cvOutputTarget);
            _engine.midiOutputEngine().sendSlide(_track.trackIndex(), _slideActive);
        }

        if (!_slideActive || _indexedTrack.slideTime() == 0) {
            _cvOutput = _cvOutputTarget;
        }
    } else {
        _slideActive = false;
    }

    // Activity indicator
    _activity = true;
}

float IndexedTrackEngine::noteIndexToVoltage(int8_t noteIndex) const {
    const Scale &scale = _sequence->selectedScale(_model.project().selectedScale());
    int rootNote = _sequence->rootNote();
    if (rootNote < 0) {
        rootNote = _model.project().rootNote();
    }

    int shift = _indexedTrack.octave() * scale.notesPerOctave() + _indexedTrack.transpose();
    float volts = scale.noteToVolts(noteIndex + shift);

    // Apply root note offset (only for chromatic scales)
    if (scale.isChromatic()) {
        volts += rootNote * (1.f / 12.f);
    }

    return volts;
}

float IndexedTrackEngine::sequenceProgress() const {
    if (!_sequence || _sequence->activeLength() == 0) {
        return 0.0f;
    }

    // Progress is current step position + sub-step position
    float stepProgress = static_cast<float>(_currentStepIndex) / _sequence->activeLength();

    // Add sub-step progress within current step
    if (_effectiveStepDuration > 0) {
        float subStepProgress = static_cast<float>(_stepTimer) / _effectiveStepDuration;
        stepProgress += subStepProgress / _sequence->activeLength();
    }

    return clamp(stepProgress, 0.0f, 1.0f);
}

bool IndexedTrackEngine::gateOutput(int index) const {
    if (_monitorOverrideActive) {
        return _gateOutput;
    }
    // Drop gate when transport is stopped
    return _engine.state().running() && !mute() && _gateTimer > 0;
}

void IndexedTrackEngine::setMonitorStep(int index) {
    if (index >= 0 && index < _sequence->activeLength()) {
        _monitorStepIndex = index;
    } else {
        _monitorStepIndex = -1;
    }
}
