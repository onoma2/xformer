#include "IndexedTrackEngine.h"

#include "Engine.h"

#include <algorithm>
#include <cmath>

float IndexedTrackEngine::routedSync() const {
    // Reuse DMap sync target for external resets
    return _indexedTrack.routedSync();
}

void IndexedTrackEngine::reset() {
    _sequence = &_indexedTrack.sequence(pattern());

    _currentStepIndex = 0;
    _stepTimer = 0;
    _gateTimer = 0;
    _effectiveStepDuration = 0;
    _cvOutput = 0.0f;
    _running = true;
    _activity = false;
    _prevSync = routedSync();
    primeNextStep();
}

void IndexedTrackEngine::changePattern() {
    _sequence = &_indexedTrack.sequence(pattern());
    // Keep playback position when changing patterns
    // (could optionally reset to step 0 here)
}

void IndexedTrackEngine::restart() {
    _currentStepIndex = 0;
    _stepTimer = 0;
    _gateTimer = 0;
    _effectiveStepDuration = 0;
    _running = true;
    _prevSync = routedSync();
    primeNextStep();
}

TrackEngine::TickResult IndexedTrackEngine::tick(uint32_t tick) {
    _sequence = &_indexedTrack.sequence(pattern());

    // Sync handling (Off / ResetMeasure / External)
    switch (_sequence->syncMode()) {
    case IndexedSequence::SyncMode::ResetMeasure: {
        uint32_t resetDivisor = _sequence->resetMeasure() * _engine.measureDivisor();
        if (resetDivisor > 0 && (tick % resetDivisor) == 0) {
            _currentStepIndex = 0;
            _stepTimer = 0;
            _gateTimer = 0;
            _cvOutput = 0.0f;
            _running = true;
            _activity = false;
            primeNextStep();
        }
        break;
    }
    case IndexedSequence::SyncMode::External: {
        float syncVal = routedSync();
        if (_prevSync <= 0.f && syncVal > 0.f) {
            // External sync reset detected
            _currentStepIndex = 0;
            _stepTimer = 0;
            _gateTimer = 0;
            _cvOutput = 0.0f;
            _running = true;
            _activity = false;
            primeNextStep();
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

    // Safety counter to prevent infinite loops when all steps have zero duration
    int skipCounter = 0;
    const int maxSkips = _sequence->activeLength();

    STEP_BEGIN:

    if (_pendingTrigger) {
        triggerStep();
        _pendingTrigger = false;
    }

    // 1. Handle Gate (counts down independently of step progress)
    if (_gateTimer > 0) {
        _gateTimer--;
        if (_gateTimer == 0) {
            // Gate off (handled by gateOutput() check)
        }
    }

    // 2. Check current step duration BEFORE incrementing timer
    uint16_t stepDuration = static_cast<uint16_t>(_effectiveStepDuration);

    // If step has zero duration, skip it immediately (without incrementing timer)
    if (stepDuration == 0) {
        skipCounter++;
        if (skipCounter >= maxSkips) {
            // All steps have zero duration - stop playback to prevent infinite loop
            // _running = false; // Do not stop running, just wait for next tick (user might edit duration)
            return TickResult::NoUpdate;
        }
        advanceStep();
        primeNextStep(); // ensure next real step fires immediately
        goto STEP_BEGIN;  // Retry with next step
    }

    // 3. Handle Duration (the accumulator) - only for valid steps
    _stepTimer++;

    // 4. Check for Step Transition
    if (_stepTimer >= stepDuration) {
        advanceStep();

        // Trigger the new step we just advanced to
        triggerStep();
    }

    return TickResult::NoUpdate;
}

void IndexedTrackEngine::update(float dt) {
    // Update slew/smoothing if needed in the future
    // For now, CV output is direct (no slew)
}

void IndexedTrackEngine::advanceStep() {
    _currentStepIndex++;

    // Handle loop/once mode
    if (_currentStepIndex >= _sequence->activeLength()) {
        if (_sequence->loop()) {
            _currentStepIndex = 0;  // Loop back to start
        } else {
            _currentStepIndex = _sequence->activeLength() - 1;  // Stay on last step
            _running = false;  // Stop playback
        }
    }

    // Reset step timer for new step
    _stepTimer = 0;
}

void IndexedTrackEngine::triggerStep() {
    int effectiveStepIndex = (_currentStepIndex + _sequence->firstStep()) % _sequence->activeLength();
    const auto &step = _sequence->step(effectiveStepIndex);

    // Get base values from step
    uint16_t baseDuration = step.duration();
    uint16_t baseGatePercent = step.gateLength();
    int8_t baseNote = step.noteIndex();

    const auto &routeA = _sequence->routeA();
    const auto &routeB = _sequence->routeB();
    auto combineMode = _sequence->routeCombineMode();

    auto matchesGroup = [&step] (const IndexedSequence::RouteConfig &cfg) {
        auto targetGroups = cfg.targetGroups;
        return targetGroups == IndexedSequence::TargetGroupsAll ||
            (targetGroups == IndexedSequence::TargetGroupsUngrouped && step.groupMask() == 0) ||
            (step.groupMask() & targetGroups);
    };

    bool combineRoutes = combineMode != IndexedSequence::RouteCombineMode::AtoB &&
        routeA.enabled && routeB.enabled &&
        routeA.targetParam == routeB.targetParam;

    if (combineRoutes) {
        bool aActive = matchesGroup(routeA);
        bool bActive = matchesGroup(routeB);
        if (aActive || bActive) {
            float cvA = _sequence->routedIndexedA();
            float cvB = _sequence->routedIndexedB();

            auto combineMod = [combineMode] (float a, float b) {
                switch (combineMode) {
                case IndexedSequence::RouteCombineMode::Mux: return 0.5f * (a + b);
                case IndexedSequence::RouteCombineMode::Min: return std::min(a, b);
                case IndexedSequence::RouteCombineMode::Max: return std::max(a, b);
                case IndexedSequence::RouteCombineMode::AtoB:
                case IndexedSequence::RouteCombineMode::Last:
                    break;
                }
                return a;
            };

            auto resolveMod = [&] (float modA, bool modAActive, float modB, bool modBActive) {
                if (modAActive && modBActive) {
                    return combineMod(modA, modB);
                }
                return modAActive ? modA : (modBActive ? modB : 0.0f);
            };

            switch (routeA.targetParam) {
            case IndexedSequence::ModTarget::Duration: {
                float modA = aActive ? cvA * (routeA.amount * 0.01f) : 0.0f;
                float modB = bActive ? cvB * (routeB.amount * 0.01f) : 0.0f;
                float mod = resolveMod(modA, aActive, modB, bActive);
                float modded = static_cast<float>(baseDuration) * (1.0f + mod);
                int newDuration = static_cast<int>(std::lround(modded));
                baseDuration = clamp(newDuration, 0, 65535);
                break;
            }
            case IndexedSequence::ModTarget::GateLength: {
                if (baseGatePercent != IndexedSequence::GateLengthTrigger) {
                    float modA = aActive ? cvA * routeA.amount : 0.0f;
                    float modB = bActive ? cvB * routeB.amount : 0.0f;
                    float mod = resolveMod(modA, aActive, modB, bActive);
                    int newGate = static_cast<int>(baseGatePercent + mod);
                    baseGatePercent = clamp(newGate, 0, int(IndexedSequence::GateLengthTrigger));
                }
                break;
            }
            case IndexedSequence::ModTarget::NoteIndex: {
                float modA = aActive ? cvA * routeA.amount : 0.0f;
                float modB = bActive ? cvB * routeB.amount : 0.0f;
                float mod = resolveMod(modA, aActive, modB, bActive);
                int newNote = static_cast<int>(baseNote + mod);
                baseNote = clamp(newNote, -63, 64);
                break;
            }
            case IndexedSequence::ModTarget::Last:
                break;
            }
        }
    } else {
        // Apply Route A modulation (if enabled and step is in target groups)
        if (routeA.enabled && matchesGroup(routeA)) {
            float cvA = _sequence->routedIndexedA();
            applyModulation(cvA, routeA, baseDuration, baseGatePercent, baseNote);
        }

        // Apply Route B modulation (if enabled and step is in target groups)
        if (routeB.enabled && matchesGroup(routeB)) {
            float cvB = _sequence->routedIndexedB();
            applyModulation(cvB, routeB, baseDuration, baseGatePercent, baseNote);
        }
    }

    // Calculate gate duration in ticks
    uint32_t gateTicks = 0;
    if (baseGatePercent == IndexedSequence::GateLengthTrigger) {
        gateTicks = 3; // Fixed short trigger pulse
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

void IndexedTrackEngine::applyModulation(
    float cv,
    const IndexedSequence::RouteConfig &cfg,
    uint16_t &duration,
    uint16_t &gate,
    int8_t &note
) {
    // cv is typically -5V to +5V, normalized to -1 to +1 for percentage-based modulation
    // For now, assume routing engine provides normalized values

    switch (cfg.targetParam) {
        case IndexedSequence::ModTarget::Duration: {
            // Scale duration by percentage of base duration
            float amountPct = cfg.amount * 0.01f;
            float factor = 1.0f + (cv * amountPct);
            float modded = static_cast<float>(duration) * factor;
            int maxDuration = 65535;
            int newDuration = static_cast<int>(std::lround(modded));
            duration = clamp(newDuration, 0, maxDuration);
            break;
        }

        case IndexedSequence::ModTarget::GateLength: {
            if (gate == IndexedSequence::GateLengthTrigger) {
                // Preserve trigger gate length sentinel
                break;
            }
            // Additive percentage modulation
            int modAmount = static_cast<int>(cv * cfg.amount);
            int newGate = static_cast<int>(gate) + modAmount;
            gate = clamp(newGate, 0, 100);
            break;
        }

        case IndexedSequence::ModTarget::NoteIndex: {
            // Transpose (additive semitones)
            int modAmount = static_cast<int>(cv * cfg.amount);
            int newNote = static_cast<int>(note) + modAmount;
            note = clamp(newNote, -63, 64);
            break;
        }

        case IndexedSequence::ModTarget::Last:
            break;
    }
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
