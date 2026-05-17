#include "StochasticTrackEngine.h"

#include "Engine.h"
#include "Groove.h"
#include "SequenceState.h"
#include "Slide.h"
#include "SequenceUtils.h"

#include "core/Debug.h"
#include "core/utils/Random.h"
#include "core/math/Math.h"

#include "model/StochasticSequence.h"
#include "model/Scale.h"

#include <cmath>

// evaluate if step gate is active
static bool evalStepGate(const StochasticSequence::Step &step, int probabilityBias, Random &rng) {
    int probability = clamp(step.gateProbability() + probabilityBias, -1, StochasticSequence::GateProbability::Max);
    if (probability == 0) {
        return false;
    }
    return step.gate() && int(rng.nextRange(StochasticSequence::GateProbability::Range)) <= probability;
}

// evaluate step condition
static bool evalStepCondition(const StochasticSequence::Step &step, int iteration, bool fill, bool &prevCondition) {
    auto condition = step.condition();
    switch (condition) {
    case Types::Condition::Off:                                         return true;
    case Types::Condition::Fill:        prevCondition = fill;           return prevCondition;
    case Types::Condition::NotFill:     prevCondition = !fill;          return prevCondition;
    case Types::Condition::Pre:                                         return prevCondition;
    case Types::Condition::NotPre:                                      return !prevCondition;
    case Types::Condition::First:       prevCondition = iteration == 0; return prevCondition;
    case Types::Condition::NotFirst:    prevCondition = iteration != 0; return prevCondition;
    default:
        int index = int(condition);
        if (index >= int(Types::Condition::Loop) && index < int(Types::Condition::Last)) {
            auto loop = Types::conditionLoop(condition);
            prevCondition = iteration % loop.base == loop.offset;
            if (loop.invert) prevCondition = !prevCondition;
            return prevCondition;
        }
    }
    return true;
}

// evaluate step retrigger count
static int evalStepRetrigger(const StochasticSequence::Step &step, int probabilityBias, Random &rng) {
    int probability = clamp(step.retriggerProbability() + probabilityBias, -1, StochasticSequence::RetriggerProbability::Max);
    return int(rng.nextRange(StochasticSequence::RetriggerProbability::Range)) <= probability ? step.retrigger() + 1 : 1;
}

// evaluate step length
static int evalStepLength(const StochasticSequence::Step &step, int lengthBias, Random &rng) {
    int length = StochasticSequence::Length::clamp(step.length() + lengthBias) + 1;
    int probability = step.lengthVariationProbability();
    if (int(rng.nextRange(StochasticSequence::LengthVariationProbability::Range)) <= probability) {
        int offset = step.lengthVariationRange() == 0 ? 0 : rng.nextRange(std::abs(step.lengthVariationRange()) + 1);
        if (step.lengthVariationRange() < 0) {
            offset = -offset;
        }
        length = clamp(length + offset, 0, StochasticSequence::Length::Range);
    }
    return length;
}

// Beta distribution approximation for Marbles shaping
static float betaDistributionSample(float x, float spread) {
    float normalizedSpread = clamp(spread, 0.0f, 1.0f);
    if (normalizedSpread == 0.5f) return x;
    
    if (normalizedSpread < 0.5f) {
        // Concentrate towards center
        float p = 1.0f + (0.5f - normalizedSpread) * 4.0f;
        if (x < 0.5f) {
            return 0.5f * std::pow(2.0f * x, p);
        } else {
            return 1.0f - 0.5f * std::pow(2.0f * (1.0f - x), p);
        }
    } else {
        // Push to edges
        float p = 1.0f + (normalizedSpread - 0.5f) * 4.0f;
        if (x < 0.5f) {
            return 0.5f * (1.0f - std::pow(1.0f - 2.0f * x, p));
        } else {
            return 0.5f + 0.5f * std::pow(2.0f * x - 1.0f, p);
        }
    }
}

static float evalStepNote(const StochasticSequence::Step &step, const StochasticTrack &track, const Scale &scale, int rootNote, int octave, int transpose, int &lastDegree, Random &rng) {
    int activeNotes = scale.notesPerOctave();
    int noteVarProb = clamp(step.noteVariationProbability() + track.noteBias(), -1, StochasticSequence::NoteVariationProbability::Max);
    
    int degree = 0;

    // 1. Build candidate pool from active scale degrees within range
    int allowedDegrees[CONFIG_USER_SCALE_SIZE];
    int weights[CONFIG_USER_SCALE_SIZE];
    int allowedCount = 0;
    
    for (int i = 0; i < activeNotes; ++i) {
        if (i >= track.minDegree() && i <= track.maxDegree()) {
            if (track.degreeTicket(i) >= 0) {
                allowedDegrees[allowedCount] = i;
                weights[allowedCount] = track.degreeTicket(i);
                allowedCount++;
            }
        }
    }

    if (allowedCount == 0 || (noteVarProb > 0 && int(rng.nextRange(StochasticSequence::NoteVariationProbability::Range)) > noteVarProb)) {
        // Fallback to step note if pool is empty or variation roll fails
        degree = step.note();
    } else {
        // Apply mask rotation to the weights within the allowed pool
        // Excluded degrees stay excluded (they aren't in the pool), 
        // and we rotate the weights of the included degrees through their positions.
        if (track.maskRotation() != 0) {
            int originalWeights[CONFIG_USER_SCALE_SIZE];
            for (int i = 0; i < allowedCount; ++i) originalWeights[i] = weights[i];
            for (int i = 0; i < allowedCount; ++i) {
                int srcIdx = (i - track.maskRotation()) % allowedCount;
                if (srcIdx < 0) srcIdx += allowedCount;
                weights[i] = originalWeights[srcIdx];
            }
        }

        if (track.marblesMode() == StochasticTrack::MarblesMode::On) {
            // Marbles shaping
            float u = rng.nextFloat();
            float shaped = betaDistributionSample(u, track.marblesSpread() / 100.0f);
            float biased = shaped + (track.marblesBias() / 100.0f - 0.5f);
            int bucket = clamp(int(clamp(biased, 0.0f, 1.0f) * allowedCount), 0, allowedCount - 1);
            degree = allowedDegrees[bucket];
        } else {
            // PWT raffling with Linearity
            int totalTickets = 0;
            for (int i = 0; i < allowedCount; ++i) {
                int w = weights[i];
                if (track.linearity() > 0 && lastDegree >= 0) {
                    int dist = std::abs(allowedDegrees[i] - lastDegree);
                    float penalty = 1.0f - (dist / float(activeNotes)) * (track.linearity() / 100.0f);
                    w = int(w * std::max(0.1f, penalty));
                }
                weights[i] = w;
                totalTickets += w;
            }

            if (totalTickets > 0) {
                int roll = rng.nextRange(totalTickets);
                int sum = 0;
                for (int i = 0; i < allowedCount; ++i) {
                    sum += weights[i];
                    if (roll < sum) {
                        degree = allowedDegrees[i];
                        break;
                    }
                }
            } else {
                degree = allowedDegrees[rng.nextRange(allowedCount)];
            }
        }

        // Apply degree rotation within the pool
        if (track.degreeRotation() != 0) {
            int currentIdx = 0;
            for (int i = 0; i < allowedCount; ++i) {
                if (allowedDegrees[i] == degree) {
                    currentIdx = i;
                    break;
                }
            }
            int rotatedIdx = (currentIdx + track.degreeRotation()) % allowedCount;
            if (rotatedIdx < 0) rotatedIdx += allowedCount;
            degree = allowedDegrees[rotatedIdx];
        }
    }

    lastDegree = degree;
    
    // Apply octave jump
    int octProb = clamp(step.noteOctaveProbability(), -1, StochasticSequence::NoteOctaveProbability::Max);
    int selectedOctave = octave;
    if (octProb > 0 && int(rng.nextRange(StochasticSequence::NoteOctaveProbability::Range)) <= octProb) {
        selectedOctave += step.noteOctave();
    }

    int finalNote = degree + selectedOctave * activeNotes + transpose;
    return scale.noteToVolts(finalNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
}

StochasticTrackEngine::StochasticTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
    TrackEngine(engine, model, track, linkedTrackEngine),
    _stochasticTrack(track.stochasticTrack()),
    _rng(0x12345678 + track.trackIndex())
{
    initLockedSteps();
    reset();
}

StochasticTrackEngine::~StochasticTrackEngine() {
    freeLockedSteps();
}

void StochasticTrackEngine::initLockedSteps() {
    if (!_lockedSteps) {
        _lockedSteps = new LockedStep[CONFIG_STEP_COUNT];
    }
}

void StochasticTrackEngine::freeLockedSteps() {
    if (_lockedSteps) {
        delete[] _lockedSteps;
        _lockedSteps = nullptr;
    }
}

void StochasticTrackEngine::reset() {
    _freeRelativeTick = 0xFFFFFFFF;
    _sequenceState.reset();
    _currentStep = -1;
    _index = -1;
    _prevCondition = false;
    _activity = false;
    _gateOutput = false;
    _slideActive = false;
    _cvOutput = 0.f;
    _cvOutputTarget = 0.f;
    _gateQueue.clear();
    _cvQueue.clear();
    _recordHistory.clear();
    _lockedStepCount = 0;
    _skips = 0;
    _lastDegree = -1;
    _rng = Random(0x12345678 + _track.trackIndex()); // Re-seed for determinism
    changePattern();
}

void StochasticTrackEngine::restart() {
    _freeRelativeTick = 0xFFFFFFFF;
    _sequenceState.reset();
    _currentStep = -1;
    _rng = Random(0x12345678 + _track.trackIndex()); // Re-seed for determinism
}

TrackEngine::TickResult StochasticTrackEngine::tick(uint32_t tick) {
    ASSERT(_sequence != nullptr, "invalid sequence");
    const auto &sequence = *_sequence;
    const auto *linkData = _linkedTrackEngine ? _linkedTrackEngine->linkData() : nullptr;

    if (linkData) {
        _linkData = *linkData;
        _sequenceState = *linkData->sequenceState;

        if (linkData->relativeTick % linkData->divisor == 0) {
            recordStep(tick, linkData->divisor);
            triggerStep(tick, linkData->divisor);
        }
    } else {
        uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
        uint32_t resetDivisor = sequence.resetMeasure() * _engine.measureDivisor();
        uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;

        if (relativeTick == 0) {
            reset();
        }

        // advance sequence
        switch (sequence.playMode()) {
        case Types::PlayMode::Aligned: {
            if (relativeTick % divisor == 0) {
                _sequenceState.advanceAligned(relativeTick / divisor, sequence.runMode(), sequence.firstStep(), sequence.lastStep(), _rng);
                triggerStep(tick, divisor);
            }
        }
            break;
        case Types::PlayMode::Free: {
            double tickPos = _engine.clock().tickPosition();
            double baseTick = resetDivisor == 0 ? tickPos : std::fmod(tickPos, double(resetDivisor));
            if (baseTick < 0.0) baseTick = 0.0;
            uint32_t currentRelativeTick = static_cast<uint32_t>(baseTick);
            
            if (currentRelativeTick % divisor == 0 && currentRelativeTick != _freeRelativeTick) {
                _sequenceState.advanceAligned(currentRelativeTick / divisor, sequence.runMode(), sequence.firstStep(), sequence.lastStep(), _rng);
                triggerStep(tick, divisor);
                _freeRelativeTick = currentRelativeTick;
            }
        }
            break;
        case Types::PlayMode::Last:
            break;
        }

        _linkData.divisor = divisor;
        _linkData.relativeTick = relativeTick;
        _linkData.sequenceState = &_sequenceState;
    }

    auto &midiOutputEngine = _engine.midiOutputEngine();
    TickResult result = TickResult::NoUpdate;

    while (!_gateQueue.empty() && tick >= _gateQueue.front().tick) {
        if (!_monitorOverrideActive) {
            result |= TickResult::GateUpdate;
            _activity = _gateQueue.front().gate;
            _gateOutput = (!mute() || fill()) && _activity;
            midiOutputEngine.sendGate(_track.trackIndex(), _gateOutput);
        }
        _gateQueue.pop();
    }

    while (!_cvQueue.empty() && tick >= _cvQueue.front().tick) {
        if (!mute() || _stochasticTrack.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always) {
            if (!_monitorOverrideActive) {
                result |= TickResult::CvUpdate;
                _cvOutputTarget = _cvQueue.front().cv;
                _slideActive = _cvQueue.front().slide;
                midiOutputEngine.sendCv(_track.trackIndex(), _cvOutputTarget);
                midiOutputEngine.sendSlide(_track.trackIndex(), _slideActive);
            }
        }
        _cvQueue.pop();
    }

    return result;
}

void StochasticTrackEngine::update(float dt) {
    bool running = _engine.state().running();
    const auto &sequence = *_sequence;
    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());

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

    bool stepMonitoring = (!running && _monitorStepIndex >= 0);

    auto monitorMode = _model.project().monitorMode();
    bool liveMonitoring =
        (monitorMode == Types::MonitorMode::Always) ||
        (monitorMode == Types::MonitorMode::Stopped && !running);

    if (stepMonitoring) {
        const auto &step = sequence.step(_monitorStepIndex);
        int lastDegreeStub = -1;
        setOverride(evalStepNote(step, _stochasticTrack, scale, rootNote, _stochasticTrack.octave(), _stochasticTrack.transpose(), lastDegreeStub, _rng));
    } else if (liveMonitoring && _recordHistory.isNoteActive()) {
        int note = noteFromMidiNote(_recordHistory.activeNote()) + _stochasticTrack.octave() * scale.notesPerOctave() + _stochasticTrack.transpose();
        setOverride(scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f));
    } else {
        clearOverride();
    }

    if (_slideActive && _stochasticTrack.slideTime() > 0) {
        _cvOutput = Slide::applySlide(_cvOutput, _cvOutputTarget, _stochasticTrack.slideTime(), dt);
    } else {
        _cvOutput = _cvOutputTarget;
    }
}

void StochasticTrackEngine::changePattern() {
    _sequence = &_stochasticTrack.sequence(pattern());
    _fillSequence = &_stochasticTrack.sequence(std::min(pattern() + 1, CONFIG_PATTERN_COUNT - 1));
}

void StochasticTrackEngine::monitorMidi(uint32_t tick, const MidiMessage &message) {
    _recordHistory.write(tick, message);
}

void StochasticTrackEngine::clearMidiMonitoring() {
    _recordHistory.clear();
}

void StochasticTrackEngine::setMonitorStep(int index) {
    _monitorStepIndex = (index >= 0 && index < CONFIG_STEP_COUNT) ? index : -1;
}

void StochasticTrackEngine::triggerStep(uint32_t tick, uint32_t divisor, bool forNextStep) {
    auto &sequence = *_sequence;
    _index = _sequenceState.step();
    _currentStep = _index;

    auto step = sequence.step(_index);
    bool stepGate = evalStepGate(step, _stochasticTrack.gateBias(), _rng);

    if (stepGate) {
        stepGate = evalStepCondition(step, _sequenceState.iteration(), fill(), _prevCondition);
    }

    float noteValue = 0.f;
    uint32_t stepLength = 0;
    int stepRetrigger = 1;

    if (stepGate) {
        const auto &scale = sequence.selectedScale(_model.project().scale());
        int rootNote = sequence.selectedRootNote(_model.project().rootNote());
        
        noteValue = evalStepNote(step, _stochasticTrack, scale, rootNote, _stochasticTrack.octave(), _stochasticTrack.transpose(), _lastDegree, _rng);
        stepLength = (divisor * evalStepLength(step, _stochasticTrack.lengthBias(), _rng)) / StochasticSequence::Length::Range;
        stepRetrigger = evalStepRetrigger(step, _stochasticTrack.retriggerBias(), _rng);

        int gateOffset = ((int) divisor * step.gateOffset()) / (StochasticSequence::GateOffset::Max + 1);
        uint32_t stepTick = tick + gateOffset;
        
        if (stepRetrigger > 1) {
            uint32_t retriggerLength = divisor / stepRetrigger;
            uint32_t retriggerOffset = 0;
            while (stepRetrigger-- > 0 && retriggerOffset <= stepLength) {
                _gateQueue.pushReplace({ Groove::applySwing(stepTick + retriggerOffset, swing()), true });
                _gateQueue.pushReplace({ Groove::applySwing(stepTick + retriggerOffset + retriggerLength / 2, swing()), false });
                retriggerOffset += retriggerLength;
            }
        } else {
            _gateQueue.pushReplace({ Groove::applySwing(stepTick, swing()), true });
            _gateQueue.pushReplace({ Groove::applySwing(stepTick + stepLength, swing()), false });
        }

        _cvQueue.push({ Groove::applySwing(stepTick, swing()), noteValue, step.slide() });
    }
}

void StochasticTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    triggerStep(tick, divisor, false);
}

void StochasticTrackEngine::recordStep(uint32_t tick, uint32_t divisor) {
}

int StochasticTrackEngine::noteFromMidiNote(uint8_t midiNote) const {
    const auto &scale = _sequence->selectedScale(_model.project().scale());
    int rootNote = _sequence->selectedRootNote(_model.project().rootNote());
    float octaveVolts = scale.noteToVolts(scale.notesPerOctave()) - scale.noteToVolts(0);
    float semitoneVolts = octaveVolts * (1.f / 12.f);
    float volts = (int(midiNote) - 60) * semitoneVolts;

    if (scale.isChromatic()) {
        volts -= rootNote * semitoneVolts;
    }

    return scale.noteFromVolts(volts);
}

float StochasticTrackEngine::sequenceProgress() const {
    return _currentStep < 0 ? 0.f : float(_currentStep - _sequence->firstStep()) / std::max(1, _sequence->lastStep() - _sequence->firstStep());
}
