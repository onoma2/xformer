#include "CurveTrackEngine.h"

#include "Engine.h"
#include "Groove.h"
#include "Slide.h"
#include "SequenceUtils.h"

#include "core/Debug.h"
#include "core/utils/Random.h"
#include "core/math/Math.h"

#include <cmath>
#include <algorithm>

#include "model/Curve.h"
#include "model/Types.h"

static Random rng;

static float applyDjFilter(float input, float &lpfState, float control, float resonance) {
    // 1. Dead zone
    if (control > -0.02f && control < 0.02f) {
        return input;
    }

    float alpha;
    if (control < 0.f) { // LPF Mode (knob left)
        alpha = 1.f - std::abs(control);
    } else { // HPF Mode (knob right)
        alpha = 0.1f + std::abs(control) * 0.85f;
    }
    alpha = clamp(alpha * alpha, 0.005f, 0.95f);

    // Resonance logic removed for now (was dependent on linearToLogarithmicFeedback)

    float feedback_input = input;

    // Update the internal LPF state
    lpfState = lpfState + alpha * (feedback_input - lpfState);

    // Apply hard limiting to lpfState to prevent internal state from growing without bounds
    lpfState = std::max(-6.0f, std::min(6.0f, lpfState)); // Allow slightly more than output range internally

    // 2. Correct LPF/HPF mapping
    if (control < 0.f) { // LPF
        return lpfState;
    } else { // HPF
        return input - lpfState;
    }
}

static float applyWavefolder(float input, float fold, float gain) {
    // map from [0, 1] to [-1, 1]
    float bipolar_input = (input * 2.f) - 1.f;
    // apply gain
    float gained_input = bipolar_input * gain;
    // apply folding using sine function. fold parameter controls frequency.
    // map fold from [0, 1] to a range of number of folds, e.g. 1 to 9
    float fold_count = 1.f + fold * 8.f;
    float folded_output = sinf(gained_input * M_PI * fold_count);
    // map back from [-1, 1] to [0, 1]
    return (folded_output + 1.f) * 0.5f;
}

// LFO-appropriate limiter function to ensure max 5V output
static float applyLfoLimiting(float input, float resonance) {
    // Simple hard clamp for now
    return std::max(-5.0f, std::min(5.0f, input));
}

static float evalStepShape(const CurveSequence::Step &step, bool variation, bool invert, float fraction) {
    auto function = Curve::function(Curve::Type(variation ? step.shapeVariation() : step.shape()));
    float value = function(fraction);
    if (invert) {
        value = 1.f - value;
    }
    float min = float(step.min()) / CurveSequence::Min::Max;
    float max = float(step.max()) / CurveSequence::Max::Max;
    return min + value * (max - min);
}

static bool evalShapeVariation(const CurveSequence::Step &step, int probabilityBias) {
    int probability = clamp(step.shapeVariationProbability() + probabilityBias, 0, 8);
    return int(rng.nextRange(8)) < probability;
}

static bool evalGate(const CurveSequence::Step &step, int probabilityBias) {
    int probability = clamp(step.gateProbability() + probabilityBias, -1, CurveSequence::GateProbability::Max);
    return int(rng.nextRange(CurveSequence::GateProbability::Range)) <= probability;
}

void CurveTrackEngine::reset() {
    _sequenceState.reset();
    _currentStep = -1;
    _currentStepFraction = 0.f;
    _phasedStep = -1;
    _phasedStepFraction = 0.f;
    _freePhase = 0.0;
    _shapeVariation = false;
    _fillMode = CurveTrack::FillMode::None;
    _activity = false;
    _gateOutput = false;
    _lpfState = 0.f;
    _feedbackState = 0.f;

    _chaosValue = 0.f;
    _chaosPhase = 0.f;
    _latoocarfian.reset();
    _lorenz.reset();

    _recorder.reset();
    _gateQueue.clear();

    _prevCvOutput = 0.f;
    _wasRising = false;
    _wasFalling = false;
    _gateTimer = 0;

    _prevCvOutputTarget = 0.f;
    _tickPhase = 0.f;

    changePattern();
}

void CurveTrackEngine::restart() {
    _sequenceState.reset();
    _currentStep = -1;
    _currentStepFraction = 0.f;
    _phasedStep = -1;
    _phasedStepFraction = 0.f;
    _freePhase = 0.0;
    _lpfState = 0.f;
    _feedbackState = 0.f;

    _chaosValue = 0.f;
    _chaosPhase = 0.f;
    _latoocarfian.reset();
    _lorenz.reset();

    _prevCvOutput = 0.f;
    _wasRising = false;
    _wasFalling = false;
    _gateTimer = 0;

    _prevCvOutputTarget = 0.f;
    _tickPhase = 0.f;
}

TrackEngine::TickResult CurveTrackEngine::tick(uint32_t tick) {
    ASSERT(_sequence != nullptr, "invalid sequence");
    const auto &sequence = *_sequence;
    const auto *linkData = _linkedTrackEngine ? _linkedTrackEngine->linkData() : nullptr;

    if (linkData) {
        _linkData = *linkData;
        _sequenceState = *linkData->sequenceState;

        updateRecording(linkData->relativeTick, linkData->divisor);

        if (linkData->relativeTick % linkData->divisor == 0) {
            triggerStep(tick, linkData->divisor);
        }

        updateOutput(linkData->relativeTick, linkData->divisor);
    } else {
        uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
        uint32_t resetDivisor = sequence.resetMeasure() * _engine.measureDivisor();
        uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;

        // handle reset measure
        if (relativeTick == 0) {
            reset();
        }

        updateRecording(relativeTick, divisor);

        if (_curveTrack.playMode() == Types::PlayMode::Free) {
            // Free mode: use phase accumulator with curveRate modulation

            // Trigger initial step if sequence hasn't started
            if (_sequenceState.step() < 0) {
                _sequenceState.advanceFree(sequence.runMode(), sequence.firstStep(), sequence.lastStep(), rng);
                triggerStep(tick, divisor);
            }

            float curveRate = _curveTrack.curveRate();  // 0.0-4.0
            float baseSpeed = 1.0 / divisor;             // Speed for 1x rate
            double speed = baseSpeed * curveRate;

            // Option 1: Enforce minimum sample density (8 ticks per step minimum)
            const float minTicksPerStep = 8.0f;
            const float maxSpeedForQuality = 1.0f / minTicksPerStep;  // 0.125
            speed = std::min(speed, static_cast<double>(maxSpeedForQuality));

            // Apply Nyquist clamping: max 0.5 phase per tick
            speed = std::min(speed, 0.5);

            // Increment phase
            _freePhase += speed;

            // Check for step boundary crossing
            if (_freePhase >= 1.0) {
                _freePhase -= 1.0;  // Wrap phase
                _sequenceState.advanceFree(sequence.runMode(), sequence.firstStep(), sequence.lastStep(), rng);
                triggerStep(tick, divisor);
            }
        } else if (relativeTick % divisor == 0) {
            // Aligned or Last mode: use grid-locked timing
            switch (_curveTrack.playMode()) {
            case Types::PlayMode::Aligned:
                _sequenceState.advanceAligned(relativeTick / divisor, sequence.runMode(), sequence.firstStep(), sequence.lastStep(), rng);
                triggerStep(tick, divisor);
                break;
            case Types::PlayMode::Last:
                break;
            default:
                break;
            }
        }

        // Option 2: Save previous target and reset tick phase for 1kHz interpolation
        _prevCvOutputTarget = _cvOutputTarget;
        _tickPhase = 0.0f;

        updateOutput(relativeTick, divisor);

        _linkData.divisor = divisor;
        _linkData.relativeTick = relativeTick;
        _linkData.sequenceState = &_sequenceState;
    }

    return TickResult::NoUpdate;
}

void CurveTrackEngine::update(float dt) {
    bool running = _engine.state().running();
    bool recording = isRecording();

    const auto &sequence = *_sequence;
    const auto &range = Types::voltageRangeInfo(_sequence->range());

    // override due to monitoring or recording
    if (!running && !recording && _monitorStepIndex >= 0) {
        // step monitoring (first priority)
        const auto &step = sequence.step(_monitorStepIndex);
        float min = float(step.min()) / CurveSequence::Min::Max;
        float max = float(step.max()) / CurveSequence::Max::Max;
        _cvOutput = _cvOutputTarget = range.denormalize(_monitorStepLevel == MonitorLevel::Min ? min : max);
        // pass through to midi engine
        auto &midiOutputEngine = _engine.midiOutputEngine();
        midiOutputEngine.sendCv(_track.trackIndex(), _cvOutput);
    } else if (recording) {
        updateRecordValue();
        _cvOutput = _cvOutputTarget = range.denormalize(_recordValue);
    }

    float offset = mute() ? 0.f : _curveTrack.offsetVolts();

    // Option 2: 1kHz interpolation between tick samples
    float interpolatedTarget = _cvOutputTarget;
    if (running && _engine.clock().isRunning()) {
        // Update tick phase (fraction through current tick)
        float tickDuration = _engine.clock().tickDuration();
        if (tickDuration > 0.f) {
            _tickPhase += dt / tickDuration;
            _tickPhase = std::min(_tickPhase, 1.0f);
        }

        // Linear interpolation between previous and current tick samples
        interpolatedTarget = _prevCvOutputTarget + (_cvOutputTarget - _prevCvOutputTarget) * _tickPhase;
    }

    if (_curveTrack.slideTime() > 0) {
        _cvOutput = Slide::applySlide(_cvOutput, interpolatedTarget + offset, _curveTrack.slideTime(), dt);
    } else {
        _cvOutput = interpolatedTarget + offset;
    }

    // Update Chaos
    if (_sequence != nullptr) {
        float p1 = _sequence->chaosParam1() / 100.f;
        float p2 = _sequence->chaosParam2() / 100.f;

        if (_sequence->chaosAlgo() == CurveSequence::ChaosAlgorithm::Latoocarfian) {
            float rate = _sequence->chaosHz();
            _chaosPhase += rate * dt;
            if (_chaosPhase >= 1.f) {
                _chaosPhase -= 1.f;
                // Map params to chaotic regions (approx 0.5 to 3.0)
                float a = 0.5f + p1 * 2.5f;
                float b = 0.5f + p1 * 2.5f;
                float c = 0.5f + p2 * 2.5f;
                float d = 0.5f + p2 * 2.5f;
                _chaosValue = _latoocarfian.next(a, b, c, d);
            }
        } else if (_sequence->chaosAlgo() == CurveSequence::ChaosAlgorithm::Lorenz) {
            // Lorenz runs at full update rate (1ms) for smoothness
            // Use unified Hz calculation for consistent speed control
            float speed = _sequence->chaosHz();
            // Map P1 to Rho (Rayleigh number) - 10.0 to 50.0
            float rho = 10.0f + p1 * 40.0f;
            // Map P2 to Beta (Geometric factor) - 0.5 to 4.0
            float beta = 0.5f + p2 * 3.5f;

            _chaosValue = _lorenz.next(dt * speed, rho, beta);
        }
    }
}

void CurveTrackEngine::changePattern() {
    _sequence = &_curveTrack.sequence(pattern());
    _fillSequence = &_curveTrack.sequence(std::min(pattern() + 1, CONFIG_PATTERN_COUNT - 1));
}

void CurveTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    int rotate = _curveTrack.rotate();
    int shapeProbabilityBias = _curveTrack.shapeProbabilityBias();

    const auto &sequence = *_sequence;
    _currentStep = SequenceUtils::rotateStep(_sequenceState.step(), sequence.firstStep(), sequence.lastStep(), rotate);
    const auto &step = sequence.step(_currentStep);

    _shapeVariation = evalShapeVariation(step, shapeProbabilityBias);

    bool fillStep = fill() && (rng.nextRange(100) < uint32_t(fillAmount()));
    _fillMode = fillStep ? _curveTrack.fillMode() : CurveTrack::FillMode::None;

    // Legacy gate pattern generation removed.
    // Gates are now generated dynamically in updateOutput based on curve slope/events.
}

void CurveTrackEngine::updateOutput(uint32_t relativeTick, uint32_t divisor) {
    if (_sequenceState.step() < 0) {
        return;
    }

    const auto &sequence = *_sequence;
    const auto &range = Types::voltageRangeInfo(sequence.range());

    // Calculate step fraction based on play mode
    if (_curveTrack.playMode() == Types::PlayMode::Free) {
        // Free mode: use phase accumulator for smooth FM
        _currentStepFraction = float(_freePhase);
    } else {
        // Aligned mode: use grid-locked timing
        _currentStepFraction = float(relativeTick % divisor) / divisor;
    }

    // True Reverse: If playing backwards, invert the step fraction so shapes play in reverse
    if (_sequenceState.direction() < 0) {
        _currentStepFraction = 1.0f - _currentStepFraction;
    }

    if (mute() || (isRecording() && _curveTrack.globalPhase() > 0.f)) {
        switch (_curveTrack.muteMode()) {
        case CurveTrack::MuteMode::LastValue:
            // keep value
            break;
        case CurveTrack::MuteMode::Zero:
            _cvOutputTarget = 0.f;
            break;
        case CurveTrack::MuteMode::Min:
            _cvOutputTarget = range.lo;
            break;
        case CurveTrack::MuteMode::Max:
            _cvOutputTarget = range.hi;
            break;
        case CurveTrack::MuteMode::Last:
            break;
        }
        if (isRecording() && _curveTrack.globalPhase() > 0.f) {
            // pass through recorded value
            updateRecordValue();
            _cvOutputTarget = range.denormalize(_recordValue);
        }
        _phasedStep = _currentStep;
        _phasedStepFraction = _currentStepFraction;
        
        // Mute should silence the gate output
        _gateOutput = false;
        _activity = false;
    } else {
        bool fillVariation = _fillMode == CurveTrack::FillMode::Variation;
        bool fillNextPattern = _fillMode == CurveTrack::FillMode::NextPattern;
        bool fillInvert = _fillMode == CurveTrack::FillMode::Invert;

        const auto &evalSequence = fillNextPattern ? *_fillSequence : sequence;

        int firstStep = evalSequence.firstStep();
        int lastStep = evalSequence.lastStep();
        int sequenceLength = lastStep - firstStep + 1;

        float globalPhase = _curveTrack.globalPhase();

        int lookupStep;
        float lookupFraction;

        if (globalPhase > 0.f && sequenceLength > 0) {
            float currentGlobalPos = float(_sequenceState.step() - firstStep) + _currentStepFraction;
            float phasedGlobalPos = fmod(currentGlobalPos + globalPhase * sequenceLength, float(sequenceLength));

            lookupStep = firstStep + int(phasedGlobalPos);
            lookupFraction = fmod(phasedGlobalPos, 1.0f);
        } else {
            // No phase offset
            lookupStep = _currentStep;
            lookupFraction = _currentStepFraction;
        }

        _phasedStep = lookupStep;
        _phasedStepFraction = lookupFraction;

        const auto &step = evalSequence.step(lookupStep);

        float shapeValue = evalStepShape(step, _shapeVariation || fillVariation, fillInvert, lookupFraction);
        float value = shapeValue;

        // Apply Chaos (Crossfade Mix)
        // Treats Chaos as a separate signal source and crossfades between the "Clean Shape" and "Pure Chaos".
        // Pros: Guaranteed to stay within valid ranges. At 100% Amount, you hear pure Chaos.
        if (evalSequence.chaosAmount() > 0) {
            float chaosAmount = evalSequence.chaosAmount() / 100.f;
            // Map Chaos (-1..1) to 0..1 to mix with Shape
            float normalizedChaos = (_chaosValue + 1.f) * 0.5f;

            // Apply range offset based on chaos range setting
            switch (evalSequence.chaosRange()) {
            case CurveSequence::ChaosRange::Below:
                normalizedChaos -= 0.25f;  // Wiggle around bottom quarter
                break;
            case CurveSequence::ChaosRange::Above:
                normalizedChaos += 0.25f;  // Wiggle around top quarter
                break;
            case CurveSequence::ChaosRange::Mid:
            case CurveSequence::ChaosRange::Last:
                break;  // No offset (wiggle around center)
            }

            value = value * (1.f - chaosAmount) + normalizedChaos * chaosAmount;
        }

        // Store pure phased shape value (before chaos and effects) for the final dry/wet crossfade
        float originalValue = range.denormalize(shapeValue);

        // 2. Get wavefolder and feedback parameters
        float fold = evalSequence.wavefolderFold();

        // 3. Prepare wavefolder input
        float folderInput = value;

        // 5. Apply wavefolder if enabled
        if (fold > 0.f) {
            float uiGain = evalSequence.wavefolderGain();
            // Map UI gain from 0.0-2.0 to internal gain range 1.0-5.0
            // 0.0 (standard) -> 1.0, 1.0 (extra) -> 3.0, 2.0 (extreme) -> 5.0
            float gain = 1.0f + (uiGain * 2.0f);
            // Apply exponential curve to fold control for better resolution
            float fold_exp = fold * fold;
            folderInput = applyWavefolder(folderInput, fold_exp, gain);
        }

        // 6. Denormalize to voltage
        float voltage = range.denormalize(folderInput);

        // 7. Apply DJ Filter
        float filterControl = evalSequence.djFilter();
        // The filter is always active to calculate state, but only applied if control is not 0
        // Resonance removed for now
        voltage = applyDjFilter(voltage, _lpfState, filterControl, 0.0f);

        // Store the processed signal (before crossfade)
        float processedSignal = voltage;

        // 7. Apply crossfade between original phased shape and processed signal
        float xFade = evalSequence.xFade();
        voltage = originalValue * (1.0f - xFade) + voltage * xFade;

        // 8. Apply LFO-appropriate limiting to ensure max 5V output
        // Resonance parameter removed from limiting for now
        processedSignal = applyLfoLimiting(processedSignal, 0.0f);

        // 9. Update feedback state for next tick (from processed signal before crossfading)
        // Apply additional limiting to feedback state to prevent runaway feedback
        _feedbackState = std::max(-4.0f, std::min(4.0f, processedSignal)); // Keep feedback state within ±4V

        // 10. Apply final hard limiting to ensure output never exceeds ±5V
        _cvOutputTarget = std::max(-5.0f, std::min(5.0f, voltage));

        // --- GATE LOGIC ---
        int gateMask = step.gate();
        int gateParam = step.gateProbability(); // 0-7
        bool gateHigh = false;

        float current = _cvOutputTarget;
        float slope = current - _prevCvOutput;
        
        // Threshold for slope detection to avoid noise triggering
        const float slopeThresh = 0.0001f;
        bool isRising = slope > slopeThresh;
        bool isFalling = slope < -slopeThresh;

        if (gateMask != 0) {
            // EVENT MODE
            bool trigger = false;

            // Zero Crossings
            if ((gateMask & CurveSequence::ZeroRise) && (_prevCvOutput <= 0.f && current > 0.f)) trigger = true;
            if ((gateMask & CurveSequence::ZeroFall) && (_prevCvOutput >= 0.f && current < 0.f)) trigger = true;

            // Peak/Trough - includes flat slopes as end points
            // Peak: end of rise (rising → flat OR rising → falling)
            if ((gateMask & CurveSequence::Peak) && _wasRising && !isRising) trigger = true;
            // Trough: end of fall (falling → flat OR falling → rising)
            if ((gateMask & CurveSequence::Trough) && _wasFalling && !isFalling) trigger = true;

            if (trigger) {
                // Use exponential trigger length: 1, 2, 4, 8, 16, 32, 64, 128 ticks
                _gateTimer = step.gateTriggerLength();
            }
        } else {
            // ADVANCED MODE (Mask == 0)
            using Mode = CurveSequence::AdvancedGateMode;
            Mode mode = Mode(gateParam);
            
            switch (mode) {
            case Mode::RisingSlope: gateHigh = isRising; break;
            case Mode::FallingSlope: gateHigh = isFalling; break;
            case Mode::AnySlope: gateHigh = (isRising || isFalling); break;
            case Mode::Compare25: gateHigh = current > range.denormalize(0.25f); break;
            case Mode::Compare50: gateHigh = current > range.denormalize(0.50f); break;
            case Mode::Compare75: gateHigh = current > range.denormalize(0.75f); break;
            case Mode::Window: 
                gateHigh = (current > range.denormalize(0.25f) && current < range.denormalize(0.75f)); 
                break;
            case Mode::Off: break;
            }
        }
        
        if (_gateTimer > 0) {
            gateHigh = true;
            _gateTimer--;
        }
        
        _gateOutput = gateHigh;
        _activity = gateHigh;

        // Update history state for next tick
        if (isRising) {
            _wasRising = true;
            _wasFalling = false;
        } else if (isFalling) {
            _wasRising = false;
            _wasFalling = true;
        }
        // If flat, keep previous state unchanged
    }

    _prevCvOutput = _cvOutputTarget;
    
    bool finalGate = (!mute() || fill()) && _gateOutput;
    _engine.midiOutputEngine().sendGate(_track.trackIndex(), finalGate);
    _engine.midiOutputEngine().sendCv(_track.trackIndex(), _cvOutputTarget);
}

bool CurveTrackEngine::isRecording() const {
    return
        _engine.state().recording() &&
        _model.project().curveCvInput() != Types::CurveCvInput::Off &&
        _model.project().selectedTrackIndex() == _track.trackIndex();
}

void CurveTrackEngine::updateRecordValue() {
    auto &sequence = *_sequence;
    const auto &range = Types::voltageRangeInfo(sequence.range());
    auto curveCvInput = _model.project().curveCvInput();

    switch (curveCvInput) {
    case Types::CurveCvInput::Cv1:
    case Types::CurveCvInput::Cv2:
    case Types::CurveCvInput::Cv3:
    case Types::CurveCvInput::Cv4:
        _recordValue = range.normalize(_engine.cvInput().channel(int(curveCvInput) - int(Types::CurveCvInput::Cv1)));
        break;
    default:
        _recordValue = 0.f;
        break;
    }
}

void CurveTrackEngine::updateRecording(uint32_t relativeTick, uint32_t divisor) {
    if (!isRecording()) {
        _recorder.reset();
        return;
    }

    updateRecordValue();

    if (_recorder.write(relativeTick, divisor, _recordValue) && _sequenceState.step() >= 0) {
        auto &sequence = *_sequence;
        int rotate = _curveTrack.rotate();
        auto &step = sequence.step(SequenceUtils::rotateStep(_sequenceState.step(), sequence.firstStep(), sequence.lastStep(), rotate));
        auto match = _recorder.matchCurve();
        step.setShape(match.type);
        step.setMinNormalized(match.min);
        step.setMaxNormalized(match.max);
    }
}
