#include "NoteTrackEngine.h"

#include "Engine.h"
#include "Groove.h"
#include "Slide.h"
#include "SequenceUtils.h"

#include "core/Debug.h"
#include "core/utils/Random.h"
#include "core/math/Math.h"

#include "model/Scale.h"
#include "model/HarmonyEngine.h"

#include <cmath>

static Random rng;

// evaluate if step gate is active
static bool evalStepGate(const NoteSequence::Step &step, int probabilityBias) {
    int probability = clamp(step.gateProbability() + probabilityBias, -1, NoteSequence::GateProbability::Max);
    return step.gate() && int(rng.nextRange(NoteSequence::GateProbability::Range)) <= probability;
}

// evaluate step condition
static bool evalStepCondition(const NoteSequence::Step &step, int iteration, bool fill, bool &prevCondition) {
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
static int evalStepRetrigger(const NoteSequence::Step &step, int probabilityBias) {
    int probability = clamp(step.retriggerProbability() + probabilityBias, -1, NoteSequence::RetriggerProbability::Max);
    return int(rng.nextRange(NoteSequence::RetriggerProbability::Range)) <= probability ? step.retrigger() + 1 : 1;
}

// evaluate step length
static int evalStepLength(const NoteSequence::Step &step, int lengthBias) {
    int length = NoteSequence::Length::clamp(step.length() + lengthBias) + 1;
    int probability = step.lengthVariationProbability();
    if (int(rng.nextRange(NoteSequence::LengthVariationProbability::Range)) <= probability) {
        int offset = step.lengthVariationRange() == 0 ? 0 : rng.nextRange(std::abs(step.lengthVariationRange()) + 1);
        if (step.lengthVariationRange() < 0) {
            offset = -offset;
        }
        length = clamp(length + offset, 0, NoteSequence::Length::Range);
    }
    return length;
}

// evaluate transposition
static int evalTransposition(const Scale &scale, int octave, int transpose) {
    return octave * scale.notesPerOctave() + transpose;
}

// evaluate note voltage
static float evalStepNote(const NoteSequence::Step &step, int probabilityBias, const Scale &scale, int rootNote, int octave, int transpose, const NoteSequence &sequence, bool useVariation = true) {
    int note = step.note() + evalTransposition(scale, octave, transpose);

    // Apply accumulator modulation if enabled
    if (sequence.accumulator().enabled()) {
        int accumulatorValue = sequence.accumulator().currentValue();

        // Check accumulator mode
        if (sequence.accumulator().mode() == Accumulator::Track) {
            // TRACK mode: Apply to ALL steps
            note += accumulatorValue;
        } else {
            // STAGE mode: Only apply to steps with triggers enabled
            if (step.isAccumulatorTrigger()) {
                note += accumulatorValue;
            }
        }
    }

    int probability = clamp(step.noteVariationProbability() + probabilityBias, -1, NoteSequence::NoteVariationProbability::Max);
    if (useVariation && int(rng.nextRange(NoteSequence::NoteVariationProbability::Range)) <= probability) {
        int offset = step.noteVariationRange() == 0 ? 0 : rng.nextRange(std::abs(step.noteVariationRange()) + 1);
        if (step.noteVariationRange() < 0) {
            offset = -offset;
        }
        note = NoteSequence::Note::clamp(note + offset);
    }
    return scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
}

void NoteTrackEngine::reset() {
    _lastFreeStepIndex = -1;
    _reReneX = 0;
    _reReneY = 0;
    _reReneLastXTick = -1;
    _reReneLastYTick = -1;
    _sequenceState.reset();
    _currentStep = -1;
    _prevCondition = false;
    _pulseCounter = 0;
    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;
    _cvOutputTarget = 0.f;
    _slideActive = false;
    _gateQueue.clear();
    _cvQueue.clear();
    _recordHistory.clear();

    // Reset accumulator to minValue
    if (_sequence) {
        const_cast<Accumulator&>(_sequence->accumulator()).reset();
    }
    if (_fillSequence) {
        const_cast<Accumulator&>(_fillSequence->accumulator()).reset();
    }

    changePattern();
}

void NoteTrackEngine::restart() {
    _lastFreeStepIndex = -1;
    _sequenceState.reset();
    _currentStep = -1;
    _pulseCounter = 0;
    _reReneX = 0;
    _reReneY = 0;
    _reReneLastXTick = -1;
    _reReneLastYTick = -1;
}

TrackEngine::TickResult NoteTrackEngine::tick(uint32_t tick) {
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
        float clockMult = sequence.clockMultiplier() * 0.01f;
        uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
        divisor = std::max<uint32_t>(1, std::lround(divisor / clockMult));
        uint32_t resetDivisor = sequence.resetMeasure() * _engine.measureDivisor();
        uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;

        // handle reset measure
        if (relativeTick == 0) {
            reset();
        }

        // advance sequence
        switch (_noteTrack.playMode()) {
        case Types::PlayMode::Aligned:
            if (relativeTick % divisor == 0) {
                // Pulse count logic: Get current step's pulse count from sequence state
                int currentStepIndex = _sequenceState.step();
                int stepPulseCount = sequence.step(currentStepIndex).pulseCount();

                // Increment pulse counter
                _pulseCounter++;

                // Fire the current step BEFORE advancing
                recordStep(tick, divisor);
                triggerStep(tick, divisor);

                // Only advance to next step when all pulses for current step are complete
                bool shouldAdvanceStep = (_pulseCounter > stepPulseCount);

                if (shouldAdvanceStep) {
                    _pulseCounter = 0;
                    _sequenceState.advanceAligned(relativeTick / divisor, sequence.runMode(), sequence.firstStep(), sequence.lastStep(), rng);
                }
            }
            break;
        case Types::PlayMode::Free:
        {
            double tickPos = _engine.clock().tickPosition();
            double baseTick = resetDivisor == 0 ? tickPos : std::fmod(tickPos, double(resetDivisor));
            if (baseTick < 0.0) {
                baseTick = 0.0;
            }
            int stepIndex = int(std::floor(baseTick / divisor));
            relativeTick = static_cast<uint32_t>(baseTick);

            if (sequence.mode() == NoteSequence::Mode::ReRene) {
                uint32_t divisorX = divisor;
                uint32_t divisorY = sequence.divisorY() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
                divisorY = std::max<uint32_t>(1, std::lround(divisorY / clockMult));
                uint32_t stepDivisor = std::min(divisorX, divisorY);
                int firstStep = sequence.firstStep();
                int lastStep = sequence.lastStep();

                auto isAllowed = [firstStep, lastStep](int x, int y) {
                    int index = x + (y * 8);
                    return index >= firstStep && index <= lastStep;
                };

                auto seekX = [&](int &x, int y) {
                    int next = x;
                    for (int i = 0; i < 8; ++i) {
                        next = (next + 1) & 7;
                        if (isAllowed(next, y)) {
                            x = next;
                            return true;
                        }
                    }
                    return false;
                };

                auto seekY = [&](int x, int &y) {
                    int next = y;
                    for (int i = 0; i < 8; ++i) {
                        next = (next + 1) & 7;
                        if (isAllowed(x, next)) {
                            y = next;
                            return true;
                        }
                    }
                    return false;
                };

                int xTickIndex = int(std::floor(baseTick / divisorX));
                int yTickIndex = int(std::floor(baseTick / divisorY));

                if (_reReneLastXTick < 0) {
                    _reReneLastXTick = xTickIndex;
                    _reReneX = xTickIndex & 7;
                }
                if (_reReneLastYTick < 0) {
                    _reReneLastYTick = yTickIndex;
                    _reReneY = yTickIndex & 7;
                }

                bool stepped = false;
                while (_reReneLastXTick < xTickIndex) {
                    ++_reReneLastXTick;
                    stepped |= seekX(_reReneX, _reReneY);
                }
                while (_reReneLastYTick < yTickIndex) {
                    ++_reReneLastYTick;
                    stepped |= seekY(_reReneX, _reReneY);
                }

                if (stepped) {
                    stepIndex = _reReneX + (_reReneY * 8);
                    _lastFreeStepIndex = stepIndex;
                    _pulseCounter = 0;
                    _sequenceState.setStep(stepIndex, firstStep, lastStep);
                    _pulseCounter++;

                    recordStep(tick, stepDivisor);
                    triggerStep(tick, stepDivisor);
                }
                divisor = stepDivisor;
            } else if (stepIndex != _lastFreeStepIndex) {
                _lastFreeStepIndex = stepIndex;
                // Pulse count logic: Get current step's pulse count from sequence state
                int currentStepIndex = _sequenceState.step();
                int stepPulseCount = sequence.step(currentStepIndex).pulseCount();

                // Increment pulse counter
                _pulseCounter++;

                // Fire the current step BEFORE advancing
                recordStep(tick, divisor);
                triggerStep(tick, divisor);

                // Only advance to next step when all pulses for current step are complete
                bool shouldAdvanceStep = (_pulseCounter > stepPulseCount);

                if (shouldAdvanceStep) {
                    _pulseCounter = 0;
                    _sequenceState.advanceFree(sequence.runMode(), sequence.firstStep(), sequence.lastStep(), rng);
                }
            }
            break;
        }
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
        auto event = _gateQueue.front();
        _gateQueue.pop();

        if (!_monitorOverrideActive) {
            result |= TickResult::GateUpdate;
            _activity = event.gate;
            _gateOutput = (!mute() || fill()) && _activity;
            midiOutputEngine.sendGate(_track.trackIndex(), _gateOutput);
        }

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
        // SPREAD MODE (flag=1): Handle accumulator and CV for retriggers
        if (event.shouldTickAccumulator) {
            // Lookup sequence by ID (0=main, 1=fill)
            const NoteSequence* targetSeq = nullptr;
            if (event.sequenceId == NoteTrackEngine::MainSequenceId && _sequence) {
                targetSeq = _sequence;
            } else if (event.sequenceId == NoteTrackEngine::FillSequenceId && _fillSequence) {
                targetSeq = _fillSequence;
            }

            // Validate sequence and tick accumulator
            if (targetSeq &&
                targetSeq->accumulator().enabled() &&
                targetSeq->accumulator().triggerMode() == Accumulator::Retrigger) {
                const_cast<Accumulator&>(targetSeq->accumulator()).tick();

                // Use pre-calculated CV if available (for per-retrigger CV variation)
                if (event.gate && event.cvTarget != 0.f && !_monitorOverrideActive) {
                    result |= TickResult::CvUpdate;
                    _cvOutputTarget = event.cvTarget;
                    // No slide on retriggers
                    _slideActive = false;
                    if (!mute() || _noteTrack.cvUpdateMode() == NoteTrack::CvUpdateMode::Always) {
                        midiOutputEngine.sendCv(_track.trackIndex(), _cvOutputTarget);
                        midiOutputEngine.sendSlide(_track.trackIndex(), _slideActive);
                    }
                }
            }
        }
#endif
    }

    while (!_cvQueue.empty() && tick >= _cvQueue.front().tick) {
        if (!mute() || _noteTrack.cvUpdateMode() == NoteTrack::CvUpdateMode::Always) {
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

void NoteTrackEngine::update(float dt) {
    bool running = _engine.state().running();
    bool recording = _engine.state().recording();

    const auto &sequence = *_sequence;
    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());
    int octave = _noteTrack.octave();
    int transpose = _noteTrack.transpose();

    // enable/disable step recording mode
    if (recording && _model.project().recordMode() == Types::RecordMode::StepRecord) {
        if (!_stepRecorder.enabled()) {
            _stepRecorder.start(sequence);
        }
    } else {
        if (_stepRecorder.enabled()) {
            _stepRecorder.stop();
        }
    }

    // helper to send gate/cv from monitoring to midi output engine
    auto sendToMidiOutputEngine = [this] (bool gate, float cv = 0.f) {
        auto &midiOutputEngine = _engine.midiOutputEngine();
        midiOutputEngine.sendGate(_track.trackIndex(), gate);
        if (gate) {
            midiOutputEngine.sendCv(_track.trackIndex(), cv);
            midiOutputEngine.sendSlide(_track.trackIndex(), false);
        }
    };

    // set monitor override
    auto setOverride = [&] (float cv) {
        _cvOutputTarget = cv;
        _activity = _gateOutput = true;
        _monitorOverrideActive = true;
        // pass through to midi engine
        sendToMidiOutputEngine(true, cv);
    };

    // clear monitor override
    auto clearOverride = [&] () {
        if (_monitorOverrideActive) {
            _activity = _gateOutput = false;
            _monitorOverrideActive = false;
            sendToMidiOutputEngine(false);
        }
    };

    // check for step monitoring
    bool stepMonitoring = (!running && _monitorStepIndex >= 0);

    // check for live monitoring
    auto monitorMode = _model.project().monitorMode();
    bool liveMonitoring =
        (monitorMode == Types::MonitorMode::Always) ||
        (monitorMode == Types::MonitorMode::Stopped && !running);

    if (stepMonitoring) {
        const auto &step = sequence.step(_monitorStepIndex);
        setOverride(evalStepNote(step, 0, scale, rootNote, octave, transpose, sequence, false));
    } else if (liveMonitoring && _recordHistory.isNoteActive()) {
        int note = noteFromMidiNote(_recordHistory.activeNote()) + evalTransposition(scale, octave, transpose);
        setOverride(scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f));
    } else {
        clearOverride();
    }

    if (_slideActive && _noteTrack.slideTime() > 0) {
        _cvOutput = Slide::applySlide(_cvOutput, _cvOutputTarget, _noteTrack.slideTime(), dt);
    } else {
        _cvOutput = _cvOutputTarget;
    }
}

void NoteTrackEngine::changePattern() {
    _sequence = &_noteTrack.sequence(pattern());
    _fillSequence = &_noteTrack.sequence(std::min(pattern() + 1, CONFIG_PATTERN_COUNT - 1));

#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
    // SPREAD MODE (flag=1): Clear gate queue on pattern change to prevent stale accumulator ticks
    // Old gates might have shouldTickAccumulator=true pointing to old pattern's sequences
    _gateQueue.clear();
    _cvQueue.clear();
#endif
}

void NoteTrackEngine::monitorMidi(uint32_t tick, const MidiMessage &message) {
    _recordHistory.write(tick, message);

    if (_engine.recording() && _model.project().recordMode() == Types::RecordMode::StepRecord) {
        _stepRecorder.process(message, *_sequence, [this] (int midiNote) { return noteFromMidiNote(midiNote); });
    }
}

void NoteTrackEngine::clearMidiMonitoring() {
    _recordHistory.clear();
}

void NoteTrackEngine::setMonitorStep(int index) {
    _monitorStepIndex = (index >= 0 && index < CONFIG_STEP_COUNT) ? index : -1;

    // in step record mode, select step to start recording recording from
    if (_engine.recording() && _model.project().recordMode() == Types::RecordMode::StepRecord &&
        index >= _sequence->firstStep() && index <= _sequence->lastStep()) {
        _stepRecorder.setStepIndex(index);
    }
}

void NoteTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    int octave = _noteTrack.octave();
    int transpose = _noteTrack.transpose();
    int rotate = _noteTrack.rotate();
    bool fillStep = fill() && (rng.nextRange(100) < uint32_t(fillAmount()));
    bool useFillGates = fillStep && _noteTrack.fillMode() == NoteTrack::FillMode::Gates;
    bool useFillSequence = fillStep && _noteTrack.fillMode() == NoteTrack::FillMode::NextPattern;
    bool useFillCondition = fillStep && _noteTrack.fillMode() == NoteTrack::FillMode::Condition;

    const auto &sequence = *_sequence;
    const auto &evalSequence = useFillSequence ? *_fillSequence : *_sequence;
    _currentStep = SequenceUtils::rotateStep(_sequenceState.step(), sequence.firstStep(), sequence.lastStep(), rotate);
    const auto &step = evalSequence.step(_currentStep);

    // STEP mode: Tick accumulator once per step (first pulse only)
    if (step.isAccumulatorTrigger() && _pulseCounter == 1) {
        const auto &targetSequence = useFillSequence ? *_fillSequence : sequence; // Use the same sequence as evalSequence
        if (targetSequence.accumulator().enabled() &&
            targetSequence.accumulator().triggerMode() == Accumulator::Step) {
            // Get per-step accumulator value (0=OFF, 1=S(global), -7 to +7=override)
            int stepValue = step.accumulatorStepValue();
            auto &accumulator = const_cast<Accumulator&>(targetSequence.accumulator());

            if (stepValue == 1) {
                // Value 1 = S (use global stepValue)
                accumulator.tick();
            } else if (stepValue != 0) {
                // Override: handle signed values (-7 to +7)
                uint8_t savedStepValue = accumulator.stepValue();
                Accumulator::Direction savedDirection = accumulator.direction();

                if (stepValue < 0) {
                    // Negative: flip direction and use absolute value
                    Accumulator::Direction flipped = (savedDirection == Accumulator::Up) ? Accumulator::Down : Accumulator::Up;
                    accumulator.setDirection(flipped);
                    accumulator.setStepValue(-stepValue);  // abs value
                } else {
                    // Positive: use value directly
                    accumulator.setStepValue(stepValue);
                }

                accumulator.tick();
                accumulator.setStepValue(savedStepValue);
                accumulator.setDirection(savedDirection);
            }
            // stepValue == 0 handled by isAccumulatorTrigger() check above
        }
    }

    uint32_t gateOffset = (divisor * step.gateOffset()) / (NoteSequence::GateOffset::Max + 1);

    bool stepGate = evalStepGate(step, _noteTrack.gateProbabilityBias()) || useFillGates;
    if (stepGate) {
        stepGate = evalStepCondition(step, _sequenceState.iteration(), useFillCondition, _prevCondition);
    }

    if (stepGate) {
        // Gate mode logic: Determine if gate should fire on this pulse
        bool shouldFireGate = false;
        int gateMode = step.gateMode();
        int pulseCount = step.pulseCount();

        switch (gateMode) {
        case 0: // ALL - Fire gates on every pulse (default, backward compatible)
            shouldFireGate = true;
            break;
        case 1: // FIRST - Fire gate only on first pulse
            shouldFireGate = (_pulseCounter == 1);
            break;
        case 2: // HOLD - Fire ONE long gate on first pulse
            shouldFireGate = (_pulseCounter == 1);
            break;
        case 3: // FIRSTLAST - Fire gates on first and last pulse
            shouldFireGate = (_pulseCounter == 1) || (_pulseCounter == (pulseCount + 1));
            break;
        default: // Safety fallback - treat unknown as ALL mode
            shouldFireGate = true;
            break;
        }

        if (shouldFireGate) {
            // GATE mode: Tick accumulator per gate pulse
            if (step.isAccumulatorTrigger()) {
                const auto &targetSequence = useFillSequence ? *_fillSequence : sequence;
                if (targetSequence.accumulator().enabled() &&
                    targetSequence.accumulator().triggerMode() == Accumulator::Gate) {
                    // Get per-step accumulator value (0=OFF, 1=S(global), -7 to +7=override)
                    int stepValue = step.accumulatorStepValue();
                    auto &accumulator = const_cast<Accumulator&>(targetSequence.accumulator());

                    if (stepValue == 1) {
                        accumulator.tick();
                    } else if (stepValue != 0) {
                        // Override: handle signed values (-7 to +7)
                        uint8_t savedStepValue = accumulator.stepValue();
                        Accumulator::Direction savedDirection = accumulator.direction();

                        if (stepValue < 0) {
                            // Negative: flip direction and use absolute value
                            Accumulator::Direction flipped = (savedDirection == Accumulator::Up) ? Accumulator::Down : Accumulator::Up;
                            accumulator.setDirection(flipped);
                            accumulator.setStepValue(-stepValue);
                        } else {
                            // Positive: use value directly
                            accumulator.setStepValue(stepValue);
                        }

                        accumulator.tick();
                        accumulator.setStepValue(savedStepValue);
                        accumulator.setDirection(savedDirection);
                    }
                }
            }

            uint32_t stepLength = (divisor * evalStepLength(step, _noteTrack.lengthBias())) / NoteSequence::Length::Range;

            // HOLD mode: extend gate length to cover all pulses
            if (gateMode == 2) {
                stepLength = divisor * (pulseCount + 1);
            }

            int stepRetrigger = evalStepRetrigger(step, _noteTrack.retriggerProbabilityBias());
            if (stepRetrigger > 1) {
#if !CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
                // BURST MODE (flag=0): Tick accumulator for each retrigger subdivision (all at once)
                if (step.isAccumulatorTrigger()) {
                    const auto &targetSequence = useFillSequence ? *_fillSequence : sequence;
                    if (targetSequence.accumulator().enabled() &&
                        targetSequence.accumulator().triggerMode() == Accumulator::Retrigger) {
                        // Get per-step accumulator value (0=OFF, 1=S(global), -7 to +7=override)
                        int stepValue = step.accumulatorStepValue();
                        auto &accumulator = const_cast<Accumulator&>(targetSequence.accumulator());
                        int tickCount = stepRetrigger;

                        if (stepValue == 1) {
                            // Value 1 = S (use global stepValue)
                            for (int i = 0; i < tickCount; ++i) {
                                accumulator.tick();
                            }
                        } else if (stepValue != 0) {
                            // Override: handle signed values (-7 to +7)
                            uint8_t savedStepValue = accumulator.stepValue();
                            Accumulator::Direction savedDirection = accumulator.direction();

                            if (stepValue < 0) {
                                // Negative: flip direction and use absolute value
                                Accumulator::Direction flipped = (savedDirection == Accumulator::Up) ? Accumulator::Down : Accumulator::Up;
                                accumulator.setDirection(flipped);
                                accumulator.setStepValue(-stepValue);
                            } else {
                                // Positive: use value directly
                                accumulator.setStepValue(stepValue);
                            }

                            for (int i = 0; i < tickCount; ++i) {
                                accumulator.tick();
                            }

                            accumulator.setStepValue(savedStepValue);
                            accumulator.setDirection(savedDirection);
                        }
                    }
                }
#endif

                uint32_t retriggerLength = divisor / stepRetrigger;
                uint32_t retriggerOffset = 0;
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
                // SPREAD MODE (flag=1): Determine if gates should tick accumulator
                const auto &targetSequence = useFillSequence ? *_fillSequence : sequence;
                bool shouldTickAccum = (
                    step.isAccumulatorTrigger() &&
                    targetSequence.accumulator().enabled() &&
                    targetSequence.accumulator().triggerMode() == Accumulator::Retrigger
                );
                uint8_t seqId = useFillSequence ? NoteTrackEngine::FillSequenceId : NoteTrackEngine::MainSequenceId;

                // Pre-calculate base CV for accumulator-driven retriggers
                const auto &scale = evalSequence.selectedScale(_model.project().scale());
                int rootNote = evalSequence.selectedRootNote(_model.project().rootNote());
                float baseCv = 0.f;
                int retrigIndex = 0;

                if (shouldTickAccum) {
                    // Evaluate base note
                    float baseNote = evalStepNote(step, _noteTrack.noteProbabilityBias(), scale, rootNote, octave, transpose, evalSequence);
                    // Apply harmony if follower
                    baseCv = applyHarmony(baseNote, step, evalSequence, scale, octave, transpose);
                }
#endif

                while (stepRetrigger-- > 0 && retriggerOffset <= stepLength) {
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
                    // SPREAD MODE: Calculate CV for this retrigger
                    float retrigCv = 0.f;
                    if (shouldTickAccum) {
                        // Simulate accumulator value after N ticks
                        int accumOffset = 0;
                        Accumulator tempAccum = targetSequence.accumulator();
                        for (int i = 0; i < retrigIndex; i++) {
                            tempAccum.tick();
                        }
                        accumOffset = tempAccum.currentValue();

                        // Add accumulator offset to base CV
                        retrigCv = baseCv + scale.noteToVolts(accumOffset);
                    }

                    // Schedule gates with metadata (tick accumulator when gate fires)
                    _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + retriggerOffset, swing()), true, shouldTickAccum, seqId, retrigCv });
                    _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + retriggerOffset + retriggerLength / 2, swing()), false, false, seqId, 0.f });
#else
                    // BURST MODE: Schedule gates without metadata (accumulator already ticked)
                    _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + retriggerOffset, swing()), true });
                    _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + retriggerOffset + retriggerLength / 2, swing()), false });
#endif
                    retriggerOffset += retriggerLength;
#if CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
                    retrigIndex++;
#endif
                }
            } else {
#if !CONFIG_EXPERIMENTAL_SPREAD_RTRIG_TICKS
                // BURST MODE (flag=0): Tick for retrigger=1 (no subdivisions, immediate tick)
                if (step.isAccumulatorTrigger()) {
                    const auto &targetSequence = useFillSequence ? *_fillSequence : sequence;
                    if (targetSequence.accumulator().enabled() &&
                        targetSequence.accumulator().triggerMode() == Accumulator::Retrigger) {
                        const_cast<Accumulator&>(targetSequence.accumulator()).tick();
                    }
                }

                _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset, swing()), true });
                _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + stepLength, swing()), false });
#else
                // SPREAD MODE (flag=1): Schedule gates with metadata for retrigger=1
                const auto &targetSequence = useFillSequence ? *_fillSequence : sequence;
                bool shouldTickAccum = (
                    step.isAccumulatorTrigger() &&
                    targetSequence.accumulator().enabled() &&
                    targetSequence.accumulator().triggerMode() == Accumulator::Retrigger
                );
                uint8_t seqId = useFillSequence ? NoteTrackEngine::FillSequenceId : NoteTrackEngine::MainSequenceId;

                // Calculate CV for single retrigger
                float retrigCv = 0.f;
                if (shouldTickAccum) {
                    const auto &scale = evalSequence.selectedScale(_model.project().scale());
                    int rootNote = evalSequence.selectedRootNote(_model.project().rootNote());
                    // Evaluate base note
                    float baseNote = evalStepNote(step, _noteTrack.noteProbabilityBias(), scale, rootNote, octave, transpose, evalSequence);
                    // Apply harmony if follower
                    retrigCv = applyHarmony(baseNote, step, evalSequence, scale, octave, transpose);
                    // For first retrigger, use current accumulator value (before tick)
                    retrigCv += scale.noteToVolts(targetSequence.accumulator().currentValue());
                }

                _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset, swing()), true, shouldTickAccum, seqId, retrigCv });
                _gateQueue.pushReplace({ Groove::applySwing(tick + gateOffset + stepLength, swing()), false, false, seqId, 0.f });
#endif
            }
        }
    }

    if (stepGate || _noteTrack.cvUpdateMode() == NoteTrack::CvUpdateMode::Always) {
        const auto &scale = evalSequence.selectedScale(_model.project().scale());
        int rootNote = evalSequence.selectedRootNote(_model.project().rootNote());

        // Evaluate base note (without harmony)
        float baseNote = evalStepNote(step, _noteTrack.noteProbabilityBias(), scale, rootNote, octave, transpose, evalSequence);

        // Apply harmony if this track is a follower (has engine-level access)
        float finalNote = applyHarmony(baseNote, step, evalSequence, scale, octave, transpose);

        _cvQueue.push({ Groove::applySwing(tick + gateOffset, swing()), finalNote, step.slide() });
    }
}

void NoteTrackEngine::recordStep(uint32_t tick, uint32_t divisor) {
    if (!_engine.state().recording() || _model.project().recordMode() == Types::RecordMode::StepRecord || _sequenceState.prevStep() < 0) {
        return;
    }

    bool stepWritten = false;

    auto writeStep = [this, divisor, &stepWritten] (int stepIndex, int note, int lengthTicks) {
        auto &step = _sequence->step(stepIndex);
        int length = (lengthTicks * NoteSequence::Length::Range) / divisor;

        step.setGate(true);
        step.setGateProbability(NoteSequence::GateProbability::Max);
        step.setRetrigger(0);
        step.setRetriggerProbability(NoteSequence::RetriggerProbability::Max);
        step.setLength(length);
        step.setLengthVariationRange(0);
        step.setLengthVariationProbability(NoteSequence::LengthVariationProbability::Max);
        step.setNote(noteFromMidiNote(note));
        step.setNoteVariationRange(0);
        step.setNoteVariationProbability(NoteSequence::NoteVariationProbability::Max);
        step.setCondition(Types::Condition::Off);

        stepWritten = true;
    };

    auto clearStep = [this] (int stepIndex) {
        auto &step = _sequence->step(stepIndex);

        step.clear();
    };

    uint32_t stepStart = tick - divisor;
    uint32_t stepEnd = tick;
    uint32_t margin = divisor / 2;

    for (size_t i = 0; i < _recordHistory.size(); ++i) {
        if (_recordHistory[i].type != RecordHistory::Type::NoteOn) {
            continue;
        }

        int note = _recordHistory[i].note;
        uint32_t noteStart = _recordHistory[i].tick;
        uint32_t noteEnd = i + 1 < _recordHistory.size() ? _recordHistory[i + 1].tick : tick;

        if (noteStart >= stepStart - margin && noteStart < stepStart + margin) {
            // note on during step start phase
            if (noteEnd >= stepEnd) {
                // note hold during step
                int length = std::min(noteEnd, stepEnd) - stepStart;
                writeStep(_sequenceState.prevStep(), note, length);
            } else {
                // note released during step
                int length = noteEnd - noteStart;
                writeStep(_sequenceState.prevStep(), note, length);
            }
        } else if (noteStart < stepStart && noteEnd > stepStart) {
            // note on during previous step
            int length = std::min(noteEnd, stepEnd) - stepStart;
            writeStep(_sequenceState.prevStep(), note, length);
        }
    }

    if (isSelected() && !stepWritten && _model.project().recordMode() == Types::RecordMode::Overwrite) {
        clearStep(_sequenceState.prevStep());
    }
}

int NoteTrackEngine::noteFromMidiNote(uint8_t midiNote) const {
    const auto &scale = _sequence->selectedScale(_model.project().scale());
    int rootNote = _sequence->selectedRootNote(_model.project().rootNote());

    if (scale.isChromatic()) {
        return scale.noteFromVolts((midiNote - 60 - rootNote) * (1.f / 12.f));
    } else {
        return scale.noteFromVolts((midiNote - 60) * (1.f / 12.f));
    }
}

float NoteTrackEngine::applyHarmony(float baseNote, const NoteSequence::Step &step, const NoteSequence &sequence, const Scale &scale, int octave, int transpose) {
    // Check per-step harmony role override first
    int harmonyRoleOverride = step.harmonyRoleOverride();
    NoteSequence::HarmonyRole harmonyRole;

    if (harmonyRoleOverride == 0) {
        // UseSequence: use sequence-level role
        harmonyRole = sequence.harmonyRole();
    } else if (harmonyRoleOverride >= 1 && harmonyRoleOverride <= 4) {
        // Map override values to follower roles: 1=Root, 2=3rd, 3=5th, 4=7th
        harmonyRole = static_cast<NoteSequence::HarmonyRole>(harmonyRoleOverride + 1);
    } else {
        // harmonyRoleOverride == 5: Off (no harmony)
        harmonyRole = NoteSequence::HarmonyOff;
    }

    // If not a follower, return base note unchanged
    if (harmonyRole == NoteSequence::HarmonyOff || harmonyRole == NoteSequence::HarmonyMaster) {
        return baseNote;
    }

    // Get master track index
    int masterTrackIndex = sequence.masterTrackIndex();

    // CRITICAL FIX 1: Self-reference check
    if (masterTrackIndex == sequence.trackIndex()) {
        return baseNote; // Can't follow self
    }

    // Get master track
    const auto &masterTrack = _model.project().track(masterTrackIndex);

    // CRITICAL FIX 2: Track type validation
    if (masterTrack.trackMode() != Track::TrackMode::Note) {
        return baseNote; // Master must be a Note track
    }

    // PATTERN FIX: Get master's ACTIVE sequence from the engine
    const TrackEngine &masterTrackEngine = _engine.trackEngine(masterTrackIndex);
    if (masterTrackEngine.trackMode() != Track::TrackMode::Note) {
        return baseNote; // Safety check
    }

    const NoteTrackEngine &masterNoteEngine = static_cast<const NoteTrackEngine&>(masterTrackEngine);
    const NoteSequence &masterSequence = masterNoteEngine.sequence();

    // Get master's current step (use master's playback position)
    int masterStepIndex = masterNoteEngine.currentStep();

    // Validate master step index
    if (masterStepIndex < masterSequence.firstStep() || masterStepIndex > masterSequence.lastStep()) {
        // Master not playing or out of range - use follower's step as fallback
        masterStepIndex = clamp(_currentStep, masterSequence.firstStep(), masterSequence.lastStep());
    }

    const auto &masterStep = masterSequence.step(masterStepIndex);
    int masterNote = masterStep.note();

    // Convert to MIDI note number (middle C = 60)
    // Note values are -64 to +63, where 0 = middle C
    int midiNote = masterNote + 60;

    // Get scale degree (0-6 for 7-note scales)
    // For simplicity, use note modulo 7 as scale degree
    int scaleDegree = ((masterNote % 7) + 7) % 7;

    // Get harmony mode from follower's harmonyScale setting
    HarmonyEngine::Mode harmonyMode = static_cast<HarmonyEngine::Mode>(sequence.harmonyScale());

    // Check master step for per-step inversion/voicing overrides
    int inversionValue = masterStep.inversionOverride();
    int voicingValue = masterStep.voicingOverride();

    // Use master step overrides if set, otherwise use master's sequence-level settings
    int inversion = (inversionValue == 0) ? masterSequence.harmonyInversion() : (inversionValue - 1);
    int voicing = (voicingValue == 0) ? masterSequence.harmonyVoicing() : (voicingValue - 1);

    // Create a local HarmonyEngine for harmonization
    HarmonyEngine harmonyEngine;
    harmonyEngine.setMode(harmonyMode);
    harmonyEngine.setInversion(inversion);
    harmonyEngine.setVoicing(static_cast<HarmonyEngine::Voicing>(voicing));
    harmonyEngine.setTranspose(sequence.harmonyTranspose());
    auto chord = harmonyEngine.harmonize(midiNote, scaleDegree);

    // Extract the appropriate chord tone based on follower role
    int harmonizedMidi = midiNote;
    switch (harmonyRole) {
    case NoteSequence::HarmonyFollowerRoot:
        harmonizedMidi = chord.root;
        break;
    case NoteSequence::HarmonyFollower3rd:
        harmonizedMidi = chord.third;
        break;
    case NoteSequence::HarmonyFollower5th:
        harmonizedMidi = chord.fifth;
        break;
    case NoteSequence::HarmonyFollower7th:
        harmonizedMidi = chord.seventh;
        break;
    default:
        break;
    }

    // Convert back to note value and apply transposition
    int harmonizedNote = (harmonizedMidi - 60) + evalTransposition(scale, octave, transpose);

    // Convert note to voltage
    return scale.noteToVolts(harmonizedNote);
}
