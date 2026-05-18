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
    // Maps 0..100 to discrete multipliers (1 bar down to 1/128th)
    if (rate < 10) return 192 * 4;  // 1 Bar
    if (rate < 20) return 192 * 2;  // 1/2
    if (rate < 30) return 192;      // 1/4
    if (rate < 40) return 96;       // 1/8
    if (rate < 50) return 48;       // 1/16
    if (rate < 60) return 24;       // 1/32
    if (rate < 70) return 12;       // 1/64
    if (rate < 80) return 6;        // 1/128
    return 6;
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

    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());

    // 1. Read Index (Rotate) - Must be computed before Lock check
    int readIndex = _patternIndex;
    if (track.rotate() != 0) {
        int windowSize = sequence.last() - sequence.first() + 1;
        int offset = (_patternIndex - sequence.first() + track.rotate()) % windowSize;
        if (offset < 0) offset += windowSize;
        readIndex = sequence.first() + offset;
    }

    // 2. Evaluated Lock check (Primary Control Flow)
    // Replay immediately without consuming RNG or generating source if locked and valid for the rotated slot.
    bool locked = track.lock() && _lockedParents[readIndex].valid;
    
    float finalCv = 0.f;
    uint32_t durationTicks = divisor;
    bool isRest = false;
    bool isLegato = false;
    bool isSlide = false;
    bool isAccent = false;

    if (locked) {
        finalCv = _lockedParents[readIndex].cv;
        durationTicks = _lockedParents[readIndex].durationTicks;
        isRest = _lockedParents[readIndex].rest;
        isLegato = _lockedParents[readIndex].legato;
        isSlide = _lockedParents[readIndex].slide;
        isAccent = _lockedParents[readIndex].accent;

        // Schedule Replay (Parent)
        if (!isRest) {
            _cvQueue.push({ tick, finalCv, isSlide });
            
            bool hasChildren = false;
            for (int i = 0; i < 4; ++i) if (_lockedParents[readIndex].children[i].valid) hasChildren = true;

            uint32_t gateLen = (durationTicks * 50) / 100;
            if (hasChildren) {
                // Shorten parent gate to ensure child retriggers are audible
                gateLen = std::min(gateLen, uint32_t(10));
            }

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
            auto &child = _lockedParents[readIndex].children[i];
            if (child.valid) {
                uint32_t childTick = tick + child.tickOffset;
                // Force a brief low before each child high to ensure audibility, safe from underflow
                uint32_t lowTick = childTick > tick + 2 ? childTick - 2 : tick;
                _gateQueue.push({ lowTick, false });
                _cvQueue.push({ childTick, child.cv, child.slide });
                _gateQueue.push({ childTick, true });
                _gateQueue.push({ childTick + child.gateTicks, false }); 
            }
        }
    } else {
        // 3. Dice Mode: Generate/Verify shared buffer (Only if not locked)
        int patternIndex = _model.project().selectedPatternIndex();
        if (track.mode() == StochasticMode::Dice) {
            if (!sequence.patternValid() || track.activePatternIndex() != patternIndex || track.activeSeed() != sequence.seed()) {
                StochasticGenerator::generatePattern(sequence, track, scale, rootNote, sequence.seed());
                track.setActivePatternIndex(patternIndex);
                track.setActiveSeed(sequence.seed());
            }
        }

        // 4. Fetch Event
        StochasticParentEvent event;

        StochasticChildHit evaluationChildren[4];
        bool useSharedChildren = true;

        if (track.mode() == StochasticMode::Realtime) {
            int lastDegree = -1; 
            event = StochasticGenerator::generateParentEvent(track, scale, rootNote, lastDegree, _rng);
            
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
            event = track.events()[readIndex];
        }

        int activeNotes = scale.notesPerOctave();
        
        // Rhythm: Rate / Variation
        uint32_t mult = getDurationMultiplier(track.rate());
        if (track.variation() != 0) {
            int variationRoll = _rng.nextRange(100);
            if (variationRoll < std::abs(track.variation())) {
                 if (track.variation() > 0) {
                     mult *= 2; 
                 } else {
                     mult = std::max(uint32_t(6), mult / 2);
                 }
            }
        }
        durationTicks = mult;

        // Evaluate Jump: probability of octave register jump inside generated pitch behavior
        int jumpOffset = 0;
        if (track.jump() > 0 && int(_rng.nextRange(100)) < track.jump()) {
            jumpOffset = _rng.nextRange(2) == 0 ? -1 : 1;
        }

        int note = int(event.d0.degree) + (int(event.d0.octave) + jumpOffset + track.octave()) * activeNotes + track.transpose();
        finalCv = scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
        
        bool densityPass = (uint32_t(event.d1.densityRank) * 100) < (uint32_t(track.density()) * sequence.size());
        isRest = bool(event.d1.rest) || !densityPass || !event.d1.valid;
        isLegato = bool(event.d1.legato);
        isSlide = bool(event.d1.slide);
        isAccent = bool(event.d1.accent);

        if (_lockedParents) {
            _lockedParents[readIndex].valid = true; 
            _lockedParents[readIndex].cv = finalCv;
            _lockedParents[readIndex].durationTicks = durationTicks;
            _lockedParents[readIndex].rest = isRest;
            _lockedParents[readIndex].legato = isLegato;
            _lockedParents[readIndex].slide = isSlide;
            _lockedParents[readIndex].accent = isAccent;
            for (int i = 0; i < 4; ++i) _lockedParents[readIndex].children[i].valid = false;
        }

        if (!isRest) {
            _cvQueue.push({ tick, finalCv, isSlide });
            
            uint32_t gateLen = (durationTicks * 50) / 100;
            if (event.d1.childCount > 0) {
                gateLen = std::min(gateLen, uint32_t(10));
            }

            _gateQueue.push({ tick, true });
            if (!isLegato) _gateQueue.push({ tick + gateLen, false });
            _activity = true;

            // Schedule & Lock Children
            for (int c = 0; c < int(event.d1.childCount); ++c) {
                const auto &child = useSharedChildren ? track.children()[event.d1.childFirst + c] : evaluationChildren[c];
                uint32_t childOffset = (durationTicks * child.offset) / 256;
                uint32_t childTick = tick + childOffset;
                uint32_t childGateTicks = (durationTicks * child.length) / (256 * (int(event.d1.childCount) + 1));
                
                int childNote = int(child.degree) + (int(child.octave) + track.octave()) * activeNotes + track.transpose();
                float childCv = scale.noteToVolts(childNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);

                // Child CV update is unconditional on gated hits
                uint32_t lowTick = childTick > tick + 2 ? childTick - 2 : tick;
                _gateQueue.push({ lowTick, false }); // Force low
                _cvQueue.push({ childTick, childCv, bool(child.slide) });
                _gateQueue.push({ childTick, true });
                _gateQueue.push({ childTick + childGateTicks, false });

                if (_lockedParents) {
                    auto &lc = _lockedParents[readIndex].children[c];
                    lc.valid = true;
                    lc.cv = childCv;
                    lc.tickOffset = childOffset;
                    lc.gateTicks = childGateTicks;
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

    // 5. Pattern Advancement
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
