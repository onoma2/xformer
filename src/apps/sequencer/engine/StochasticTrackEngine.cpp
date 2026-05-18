#include "StochasticTrackEngine.h"
#include "StochasticGenerator.h"
#include "Engine.h"

#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"
#include "model/Model.h"
#include "model/Scale.h"

#include "core/utils/Random.h"
#include "core/math/Math.h"

#include <cmath>
#include <algorithm>

static uint32_t getDurationMultiplier(int rate) {
    if (rate < 10) return 16;
    if (rate < 20) return 8;
    if (rate < 30) return 4;
    if (rate < 40) return 2;
    if (rate < 60) return 1;
    return 1; // Default
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
    if (!_lockedParents) {
        _lockedParents = new (std::nothrow) LockedParentEvent[CONFIG_STEP_COUNT];
        if (_lockedParents) {
            for (int i = 0; i < CONFIG_STEP_COUNT; ++i) {
                _lockedParents[i].valid = false;
                for (int j = 0; j < 4; ++j) _lockedParents[i].children[j].valid = false;
            }
        }
    }
}

void StochasticTrackEngine::freeLockedSteps() {
    if (_lockedParents) {
        delete[] _lockedParents;
        _lockedParents = nullptr;
    }
}

void StochasticTrackEngine::reset() {
    _sequenceState.reset();
    _patternIndex = _stochasticTrack.sequence(_model.project().selectedPatternIndex()).first();
    _relativeTick = 0;
    _sleepRemaining = 0;
    _boredomCounter = 0;
    _jumpOctave = 0;
    _patternCycleEnded = false;
    _prevCondition = false;
    _activity = false;
    _gateOutput = false;
    _accentOutput = false;
    _slideActive = false;
    _cvOutput = 0.f;
    _cvOutputTarget = 0.f;
    _gateQueue.clear();
    _cvQueue.clear();
    _rng = Random(0x12345678 + _track.trackIndex());
    changePattern();
}

void StochasticTrackEngine::restart() {
    _sequenceState.reset();
    _patternIndex = _stochasticTrack.sequence(_model.project().selectedPatternIndex()).first();
    _relativeTick = 0;
    _rng = Random(0x12345678 + _track.trackIndex());
}

TrackEngine::TickResult StochasticTrackEngine::tick(uint32_t tick) {
    ASSERT(_sequence != nullptr, "invalid sequence");
    const auto &sequence = *_sequence;

    uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    uint32_t resetMeasure = sequence.resetMeasure();
    if (resetMeasure > 0) {
        uint32_t resetDivisor = resetMeasure * _engine.measureDivisor();
        uint32_t relativeTick = tick % resetDivisor;
        if (relativeTick == 0) {
            reset();
        }
        _relativeTick = relativeTick % divisor;
    }

    if (_relativeTick == 0) {
        if (_sleepRemaining > 0) {
            _sleepRemaining--;
        } else {
            triggerStep(tick, divisor);
        }
    }

    // Process Queues
    while (!_gateQueue.empty() && tick >= _gateQueue.front().tick) {
        _gateOutput = _gateQueue.front().gate;
        _gateQueue.pop();
    }

    while (!_cvQueue.empty() && tick >= _cvQueue.front().tick) {
        _cvOutputTarget = _cvQueue.front().cv;
        _slideActive = _cvQueue.front().slide;
        _cvQueue.pop();
    }

    if (resetMeasure == 0) {
        _relativeTick = (_relativeTick + 1) % divisor;
    }
    
    return (CvUpdate | GateUpdate);
}

void StochasticTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    auto &sequence = const_cast<StochasticSequence&>(*_sequence);
    auto &track = const_cast<StochasticTrack&>(_stochasticTrack);

    // 1. Consistency check: rebuild shared buffer if it doesn't match current pattern context
    int patternIndex = _model.project().selectedPatternIndex();
    if (track.mode() == StochasticMode::Dice) {
        if (!sequence.patternValid() || track.activePatternIndex() != patternIndex || track.activeSeed() != sequence.seed()) {
            StochasticGenerator::generatePattern(sequence, track, sequence.selectedScale(_model.project().scale()), sequence.selectedRootNote(_model.project().rootNote()), sequence.seed());
            track.setActivePatternIndex(patternIndex);
            track.setActiveSeed(sequence.seed());
        }
    }

    // 2. Fetch Event
    StochasticParentEvent event;
    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());

    StochasticChildHit evaluationChildren[4];
    bool useSharedChildren = true;

    if (track.mode() == StochasticMode::Realtime) {
        int lastDegree = -1; 
        event = StochasticGenerator::generateParentEvent(track, scale, rootNote, lastDegree, _rng);
        
        // Isolate realtime children
        useSharedChildren = false;
        int count = event.d1.childCount;
        int childLast = event.d0.degree;
        int burstRate = track.burstRate();
        for (int c = 0; c < count; ++c) {
            auto &child = evaluationChildren[c];
            child.clear();
            if (track.burstPitch() == StochasticBurstPitch::Parent) {
                child.degree = event.d0.degree;
                child.octave = event.d0.octave;
            } else {
                int absoluteDegree = StochasticGenerator::generateDegree(track, scale, childLast, _rng);
                int activeNotes = scale.notesPerOctave();
                child.degree = absoluteDegree % activeNotes;
                child.octave = absoluteDegree / activeNotes;
            }
            int spacing = 255 / (count + 1);
            if (burstRate > 0) spacing = std::max(8, spacing - (spacing * burstRate) / 100);
            child.offset = (c + 1) * spacing;
            child.length = 128;
            child.gate = true;
        }
    } else {
        event = track.events()[_patternIndex];
    }

    // 3. Evaluated Lock check
    bool locked = track.lock() && _lockedParents[_patternIndex].valid;
    
    float finalCv = 0.f;
    uint32_t durationTicks = divisor;
    bool isRest = false;
    bool isLegato = false;
    bool isSlide = false;
    bool isAccent = false;

    if (locked) {
        finalCv = _lockedParents[_patternIndex].cv;
        durationTicks = _lockedParents[_patternIndex].durationTicks;
        isRest = _lockedParents[_patternIndex].rest;
        isLegato = _lockedParents[_patternIndex].legato;
        isSlide = _lockedParents[_patternIndex].slide;
        isAccent = _lockedParents[_patternIndex].accent;

        // Schedule Replay (Parent)
        if (!isRest) {
            _cvQueue.push({ tick, finalCv, isSlide });
            uint32_t gateLen = (durationTicks * 50) / 100;
            _gateQueue.push({ tick, true });
            if (!isLegato) _gateQueue.push({ tick + gateLen, false });
            _activity = true;
        } else {
            _gateQueue.push({ tick, false });
            if (track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always) _cvQueue.push({ tick, finalCv, false });
            _activity = false;
        }

        // Replay Children
        for (int i = 0; i < 4; ++i) {
            auto &child = _lockedParents[_patternIndex].children[i];
            if (child.valid) {
                uint32_t childTick = tick + child.tickOffset;
                _cvQueue.push({ childTick, child.cv, child.slide });
                _gateQueue.push({ childTick, true });
                _gateQueue.push({ childTick + 10, false }); 
            }
        }
    } else {
        int activeNotes = scale.notesPerOctave();
        
        // Rhythm: Rate / Variation
        uint32_t mult = getDurationMultiplier(track.rate());
        if (track.variation() > 0 && int(_rng.nextRange(100)) < track.variation()) {
             // Pick neighbor multiplier
             mult = (_rng.nextRange(2) == 0) ? mult * 2 : std::max(uint32_t(1), uint32_t(mult / 2));
        }
        durationTicks = divisor * mult;

        int note = int(event.d0.degree) + (int(event.d0.octave) + track.octave() + _jumpOctave) * activeNotes + track.transpose();
        finalCv = scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
        
        bool densityPass = (uint32_t(event.d1.densityRank) * 100) < (uint32_t(track.density()) * sequence.size());
        isRest = bool(event.d1.rest) || !densityPass || !event.d1.valid;
        isLegato = bool(event.d1.legato);
        isSlide = bool(event.d1.slide);
        isAccent = bool(event.d1.accent);

        if (_lockedParents) {
            _lockedParents[_patternIndex].valid = true;
            _lockedParents[_patternIndex].cv = finalCv;
            _lockedParents[_patternIndex].durationTicks = durationTicks;
            _lockedParents[_patternIndex].rest = isRest;
            _lockedParents[_patternIndex].legato = isLegato;
            _lockedParents[_patternIndex].slide = isSlide;
            _lockedParents[_patternIndex].accent = isAccent;
            for (int i = 0; i < 4; ++i) _lockedParents[_patternIndex].children[i].valid = false;
        }

        if (!isRest) {
            _cvQueue.push({ tick, finalCv, isSlide });
            uint32_t gateLen = (durationTicks * 50) / 100;
            _gateQueue.push({ tick, true });
            if (!isLegato) _gateQueue.push({ tick + gateLen, false });
            _activity = true;

            // Schedule & Lock Children
            for (int c = 0; c < int(event.d1.childCount); ++c) {
                const auto &child = useSharedChildren ? track.children()[event.d1.childFirst + c] : evaluationChildren[c];
                uint32_t childOffset = (durationTicks * child.offset) / 256;
                uint32_t childTick = tick + childOffset;
                
                int childNote = int(child.degree) + (int(child.octave) + track.octave() + _jumpOctave) * activeNotes + track.transpose();
                float childCv = scale.noteToVolts(childNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);

                _cvQueue.push({ childTick, childCv, bool(child.slide) });
                _gateQueue.push({ childTick, true });
                _gateQueue.push({ childTick + 10, false });

                if (_lockedParents) {
                    auto &lc = _lockedParents[_patternIndex].children[c];
                    lc.valid = true;
                    lc.cv = childCv;
                    lc.tickOffset = childOffset;
                    lc.slide = bool(child.slide);
                    lc.accent = bool(child.accent);
                }
            }
        } else {
            _gateQueue.push({ tick, false });
            if (track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always) _cvQueue.push({ tick, finalCv, false });
            _activity = false;
        }
    }

    // Pattern Advancement
    _patternIndex++;
    if (_patternIndex > sequence.last()) {
        _patternIndex = sequence.first();
        _patternCycleEnded = true;
        
        if (!track.lock()) {
            if (track.sleep() > 0) _sleepRemaining = (track.sleep() * 4) / 10; 
            
            if (track.mutate() > 0 && int(_rng.nextRange(100)) < track.mutate()) {
                StochasticGenerator::mutateOne(sequence, track, scale, rootNote, _rng);
            }
            
            if (track.patience() < 100) {
                _boredomCounter++;
                if (_boredomCounter > (uint32_t(track.patience()) * 10)) {
                    sequence.setPatternValid(false);
                    _boredomCounter = 0;
                }
            }
            _jumpOctave = StochasticGenerator::generateJumpOctave(track, _jumpOctave, _rng);
        }
    }
}

void StochasticTrackEngine::update(float dt) {
    if (_slideActive && _stochasticTrack.slideTime() > 0) {
        float slideDt = dt * 1000.f / (float(_stochasticTrack.slideTime()) + 1.f);
        _cvOutput += (_cvOutputTarget - _cvOutput) * std::min(1.f, slideDt);
    } else {
        _cvOutput = _cvOutputTarget;
    }
}
