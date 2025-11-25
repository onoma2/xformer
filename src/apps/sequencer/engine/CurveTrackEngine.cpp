#include "CurveTrackEngine.h"

#include "Engine.h"
#include "Groove.h"
#include "Slide.h"
#include "SequenceUtils.h"

#include "core/Debug.h"
#include "core/utils/Random.h"
#include "core/math/Math.h"

#include "model/Curve.h"
#include "model/Types.h"

static Random rng;

static float applyDjFilter(float input, float &lpfState, float control) {
    // 1. Dead zone
    if (control > -0.02f && control < 0.02f) {
        return input;
    }

    float alpha;
    if (control < 0.f) { // LPF Mode (knob left)
        // More turn (-> -1.0) = more filtering = lower alpha
        alpha = 1.f - std::abs(control);
    } else { // HPF Mode (knob right)
        // More turn (-> +1.0) = more of a filtering effect.
        // We remap the knob's travel to an effective alpha range that feels more linear.
        alpha = 0.1f + std::abs(control) * 0.85f;
    }
    // The clamp and square can be applied to both to shape the response
    alpha = clamp(alpha * alpha, 0.005f, 0.95f); // Clamp max to avoid instabilities

    // Update the internal LPF state
    lpfState = lpfState + alpha * (input - lpfState);

    // 2. Correct LPF/HPF mapping
    if (control < 0.f) { // LPF
        return lpfState;
    } else { // HPF
        return input - lpfState;
    }
}

static float applyWavefolder(float input, float fold, float gain, float symmetry) {
    // map from [0, 1] to [-1, 1]
    float bipolar_input = (input * 2.f) - 1.f;
    // apply symmetry
    float biased_input = bipolar_input + symmetry;
    // apply gain
    float gained_input = biased_input * gain;
    // apply folding using sine function. fold parameter controls frequency.
    // map fold from [0, 1] to a range of number of folds, e.g. 1 to 9
    float fold_count = 1.f + fold * 8.f;
    float folded_output = sinf(gained_input * M_PI * fold_count);
    // map back from [-1, 1] to [0, 1]
    return (folded_output + 1.f) * 0.5f;
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
    _shapeVariation = false;
    _fillMode = CurveTrack::FillMode::None;
    _activity = false;
    _gateOutput = false;
    _lpfState = 0.f;

    _recorder.reset();
    _gateQueue.clear();

    changePattern();
}

void CurveTrackEngine::restart() {
    _sequenceState.reset();
    _currentStep = -1;
    _currentStepFraction = 0.f;
    _phasedStep = -1;
    _phasedStepFraction = 0.f;
    _lpfState = 0.f;
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

        if (relativeTick % divisor == 0) {
            // advance sequence
            switch (_curveTrack.playMode()) {
            case Types::PlayMode::Aligned:
                _sequenceState.advanceAligned(relativeTick / divisor, sequence.runMode(), sequence.firstStep(), sequence.lastStep(), rng);
                triggerStep(tick, divisor);
                break;
            case Types::PlayMode::Free:
                _sequenceState.advanceFree(sequence.runMode(), sequence.firstStep(), sequence.lastStep(), rng);
                triggerStep(tick, divisor);
                break;
            case Types::PlayMode::Last:
                break;
            }
        }

        updateOutput(relativeTick, divisor);

        _linkData.divisor = divisor;
        _linkData.relativeTick = relativeTick;
        _linkData.sequenceState = &_sequenceState;
    }

    TickResult result = TickResult::NoUpdate;

    while (!_gateQueue.empty() && tick >= _gateQueue.front().tick) {
        result |= TickResult::GateUpdate;
        _activity = _gateQueue.front().gate;
        _gateOutput = (!mute() || fill()) && _activity;
        _gateQueue.pop();

        _engine.midiOutputEngine().sendGate(_track.trackIndex(), _gateOutput);
    }

    return result;
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

    if (_curveTrack.slideTime() > 0) {
        _cvOutput = Slide::applySlide(_cvOutput, _cvOutputTarget + offset, _curveTrack.slideTime(), dt);
    } else {
        _cvOutput = _cvOutputTarget + offset;
    }
}

void CurveTrackEngine::changePattern() {
    _sequence = &_curveTrack.sequence(pattern());
    _fillSequence = &_curveTrack.sequence(std::min(pattern() + 1, CONFIG_PATTERN_COUNT - 1));
}

void CurveTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    int rotate = _curveTrack.rotate();
    int shapeProbabilityBias = _curveTrack.shapeProbabilityBias();
    int gateProbabilityBias = _curveTrack.gateProbabilityBias();

    const auto &sequence = *_sequence;
    _currentStep = SequenceUtils::rotateStep(_sequenceState.step(), sequence.firstStep(), sequence.lastStep(), rotate);
    const auto &step = sequence.step(_currentStep);

    _shapeVariation = evalShapeVariation(step, shapeProbabilityBias);

    bool fillStep = fill() && (rng.nextRange(100) < uint32_t(fillAmount()));
    _fillMode = fillStep ? _curveTrack.fillMode() : CurveTrack::FillMode::None;

    // Trigger gate pattern
    int gate = step.gate();
    for (int i = 0; i < 4; ++i) {
        if (gate & (1 << i) && evalGate(step, gateProbabilityBias)) {
            uint32_t gateStart = (divisor * i) / 4;
            uint32_t gateLength = divisor / 8;
            _gateQueue.pushReplace({ Groove::applySwing(tick + gateStart, swing()), true });
            _gateQueue.pushReplace({ Groove::applySwing(tick + gateStart + gateLength, swing()), false });
        }
    }
}

void CurveTrackEngine::updateOutput(uint32_t relativeTick, uint32_t divisor) {
    if (_sequenceState.step() < 0) {
        return;
    }

    const auto &sequence = *_sequence;
    const auto &range = Types::voltageRangeInfo(sequence.range());

    _currentStepFraction = float(relativeTick % divisor) / divisor;

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

        // 1. Get the original normalized shape value
        float shapeValue = evalStepShape(step, _shapeVariation || fillVariation, fillInvert, lookupFraction);

        // 2. Get wavefolder parameters from the track
        float fold_linear = _curveTrack.wavefolderFold();
        // Apply an exponential curve for more fine-grained control at low values
        float fold = fold_linear * fold_linear;

        // 3. Apply wavefolder if enabled (fold > 0)
        if (fold > 0.f) {
            float gain = _curveTrack.wavefolderGain();
            float symmetry = _curveTrack.wavefolderSymmetry();
            shapeValue = applyWavefolder(shapeValue, fold, gain, symmetry);
        }

        // 4. Denormalize the final value to voltage
        float value = range.denormalize(shapeValue);

        // 5. Apply DJ Filter
        float filterControl = _curveTrack.djFilter();
        if (filterControl != 0.f) {
            value = applyDjFilter(value, _lpfState, filterControl);
        }

        _cvOutputTarget = value;
    }

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
