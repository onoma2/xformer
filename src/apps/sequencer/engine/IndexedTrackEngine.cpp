#include "IndexedTrackEngine.h"

#include "Engine.h"

#include <algorithm>
#include <cmath>

namespace {

// Constants matching IndexedTrackEngine class constants
constexpr uint16_t MAX_DURATION = 65535;
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
inline void applyGateLengthModulation(float modValue, uint16_t &gatePercent) {
    if (gatePercent == IndexedSequence::GateLengthTrigger) {
        return; // Preserve trigger sentinel
    }
    int newGate = static_cast<int>(gatePercent + modValue);
    gatePercent = clamp(newGate, 0, int(IndexedSequence::GateLengthTrigger));
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
    uint16_t &duration,
    uint16_t &gatePercent,
    int8_t &note)
{
    // Calculate modulation values from each route
    float modA = 0.0f;
    float modB = 0.0f;

    if (aActive && routeA.targetParam == target) {
        if (target == IndexedSequence::ModTarget::Duration) {
            modA = cvA * (routeA.amount * 0.01f); // Percentage for duration
        } else {
            modA = cvA * routeA.amount; // Direct value for gate/note
        }
    }

    if (bActive && routeB.targetParam == target) {
        if (target == IndexedSequence::ModTarget::Duration) {
            modB = cvB * (routeB.amount * 0.01f); // Percentage for duration
        } else {
            modB = cvB * routeB.amount; // Direct value for gate/note
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
            applyGateLengthModulation(finalMod, gatePercent);
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
    _stepTimer = 0;
    _gateTimer = 0;
    _effectiveStepDuration = 0;
    _cvOutput = 0.0f;
    _running = true;
    _activity = false;

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

    // 1. Handle Gate (counts down independently of step progress)
    if (_gateTimer > 0) {
        _gateTimer--;
    }

    // 2. Get current step duration
    const uint16_t stepDuration = static_cast<uint16_t>(_effectiveStepDuration);

    // If current step has zero duration, don't advance timer (wait for next tick)
    // Note: advanceStep() already skips zero-duration steps, so this should rarely happen
    // except when user edits the current step to zero while playing
    if (stepDuration == 0) {
        return TickResult::NoUpdate;
    }

    // 3. Handle Duration (the accumulator)
    _stepTimer++;

    // 4. Check for Step Transition
    if (_stepTimer >= stepDuration) {
        advanceStep();  // This automatically skips zero-duration steps
        triggerStep();   // Trigger the new step we just advanced to
    }

    return TickResult::NoUpdate;
}

void IndexedTrackEngine::update(float dt) {
    // Update slew/smoothing if needed in the future
    // For now, CV output is direct (no slew)
}

void IndexedTrackEngine::advanceStep() {
    const int activeLength = _sequence->activeLength();
    if (activeLength <= 0) {
        return;
    }

    if (!_sequence->loop() && _stepsRemaining <= 0) {
        _running = false;
        _stepTimer = 0;
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
            _stepTimer = 0;
            return;
        }
    }

    _stepTimer = 0;
}

void IndexedTrackEngine::triggerStep() {
    int effectiveStepIndex = (_currentStepIndex + _sequence->firstStep()) % _sequence->activeLength();
    const auto &step = _sequence->step(effectiveStepIndex);

    // Get base values from step
    uint16_t baseDuration = step.duration();
    uint16_t baseGatePercent = step.gateLength();
    int8_t baseNote = step.noteIndex();

    // Apply route modulation (unified for both combined and sequential modes)
    const auto &routeA = _sequence->routeA();
    const auto &routeB = _sequence->routeB();
    const bool aActive = routeA.enabled && stepMatchesRouteGroups(step, routeA);
    const bool bActive = routeB.enabled && stepMatchesRouteGroups(step, routeB);

    if (aActive || bActive) {
        const float cvA = _sequence->routedIndexedA();
        const float cvB = _sequence->routedIndexedB();
        const auto combineMode = _sequence->routeCombineMode();

        // Check if routes should be combined
        const bool shouldCombine = combineMode != IndexedSequence::RouteCombineMode::AtoB &&
                                   aActive && bActive &&
                                   routeA.targetParam == routeB.targetParam;

        // Apply modulation for each possible target parameter
        applyStepModulation(IndexedSequence::ModTarget::Duration, cvA, cvB,
                           routeA, routeB, aActive, bActive, shouldCombine, combineMode,
                           baseDuration, baseGatePercent, baseNote);

        applyStepModulation(IndexedSequence::ModTarget::GateLength, cvA, cvB,
                           routeA, routeB, aActive, bActive, shouldCombine, combineMode,
                           baseDuration, baseGatePercent, baseNote);

        applyStepModulation(IndexedSequence::ModTarget::NoteIndex, cvA, cvB,
                           routeA, routeB, aActive, bActive, shouldCombine, combineMode,
                           baseDuration, baseGatePercent, baseNote);
    }

    // Calculate gate duration in ticks
    uint32_t gateTicks = 0;
    if (baseGatePercent == IndexedSequence::GateLengthTrigger) {
        gateTicks = TRIGGER_PULSE_TICKS;
    } else {
        // gateLength is stored as percentage (0-100)
        // Convert to ticks: (duration * percentage / 100)
        gateTicks = (baseDuration * baseGatePercent) / 100;
        if (gateTicks == 0 && baseGatePercent > 0) {
            gateTicks = 1;  // Minimum 1 tick gate if non-zero percentage
        }
    }

    // Set gate timer
    _gateTimer = gateTicks;
    _effectiveStepDuration = baseDuration;

    // Calculate CV output (direct Scale lookup, no octave/modulo math)
    if (_indexedTrack.cvUpdateMode() == IndexedTrack::CvUpdateMode::Always || gateTicks > 0) {
        _cvOutput = noteIndexToVoltage(baseNote);
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
    // Drop gate when transport is stopped
    return _engine.state().running() && !mute() && _gateTimer > 0;
}
