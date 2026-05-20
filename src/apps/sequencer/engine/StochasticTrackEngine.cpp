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
#include <cstdio>

// Gate scheduling trace — disabled for release
//#define DBG_STO_ENABLE
#ifdef DBG_STO_ENABLE
#define DBG_STO(fmt, ...) printf("[STO] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_STO(fmt, ...)
#endif

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

void StochasticTrackEngine::resetMeasure() {
    _patternIndex = _stochasticTrack.sequence(pattern()).first();
    _relativeTick = 0;
    _eventElapsed = 0;
    _eventDuration = 0;
}

void StochasticTrackEngine::reset() {
    _patternIndex = _stochasticTrack.sequence(pattern()).first();
    _relativeTick = 0;
    _sleepRemaining = 0;
    _loopCycleCount = 0;
    _lastDegree = -1;
    _jumpRegister = 0;
    _lastFreeStepIndex = -1;
    _patternCycleEnded = false;
    _eventElapsed = 0;
    _eventDuration = 0;
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
    _patternIndex = _stochasticTrack.sequence(pattern()).first();
    _relativeTick = 0;
    _lastDegree = -1;
    _lastFreeStepIndex = -1;
    _eventElapsed = 0;
    _eventDuration = 0;
    _rng = Random(0x12345678 + _track.trackIndex());
}

TrackEngine::TickResult StochasticTrackEngine::tick(uint32_t tick) {
    ASSERT(_sequence != nullptr, "invalid sequence");
    const auto &sequence = *_sequence;

    // Divisor with clock multiplier
    float clockMult = sequence.clockMultiplier() * 0.01f;
    uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    divisor = std::max<uint32_t>(1, std::lround(divisor / clockMult));

    // Reset measure
    uint32_t resetDivisor = sequence.resetMeasure() * _engine.measureDivisor();
    uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;
    if (resetDivisor > 0 && relativeTick == 0) {
        resetMeasure();
    }

    // Dispatch by play mode or Duration Tickets event timing
    if (sequence.durationTicketsActive()) {
        _eventElapsed++;
        if (_eventDuration == 0 || _eventElapsed >= _eventDuration) {
            _eventElapsed = 0;
            DBG_STO("%u TRIG(T) p=%d", tick, _patternIndex);
            triggerStep(tick, divisor);
        }
    } else {
        switch (_stochasticTrack.playMode()) {
        case Types::PlayMode::Free: {
        double tickPos = _engine.clock().tickPosition();
        double baseTick = resetDivisor == 0 ? tickPos : std::fmod(tickPos, double(resetDivisor));
        if (baseTick < 0.0) baseTick = 0.0;
        int stepIndex = int(std::floor(baseTick / divisor));
        if (stepIndex != _lastFreeStepIndex) {
            _lastFreeStepIndex = stepIndex;
            if (_sleepRemaining > 0) {
                _sleepRemaining--;
            } else {
                DBG_STO("%u TRIG(F) p=%d", tick, _patternIndex);
                triggerStep(tick, divisor);
            }
        }
        break;
    }
    case Types::PlayMode::Aligned:
    default:
        _relativeTick = resetDivisor == 0 ? tick % divisor : relativeTick % divisor;
        if (_relativeTick == 0) {
            if (_sleepRemaining > 0) {
                _sleepRemaining--;
            } else {
                DBG_STO("%u TRIG(A) p=%d", tick, _patternIndex);
                triggerStep(tick, divisor);
            }
        }
        break;
    }
    }

    // Process gate queue
    TickResult result = TickResult::NoUpdate;
    while (!_gateQueue.empty() && tick >= _gateQueue.front().tick) {
        auto &ev = _gateQueue.front();
        _gateOutput = ev.gate;
        _accentOutput = ev.accent;
        _gateQueue.pop();
        DBG_STO("%u GATE pop gate=%d", tick, _gateOutput);
        result |= TickResult::GateUpdate;
    }

    // Process CV queue
    while (!_cvQueue.empty() && tick >= _cvQueue.front().tick) {
        auto &ev = _cvQueue.front();
        DBG_STO("%u CVpop cv=%.3f", tick, ev.cv);
        _cvOutputTarget = ev.cv;
        _slideActive = ev.slide;
        _cvQueue.pop();
        result |= TickResult::CvUpdate;
    }

    return result;
}

void StochasticTrackEngine::stop() {
    _gateQueue.clear();
    _cvQueue.clear();
    _gateOutput = false;
    _accentOutput = false;
    _activity = false;
}

void StochasticTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    auto &sequence = const_cast<StochasticSequence&>(*_sequence);
    auto &track = const_cast<StochasticTrack&>(_stochasticTrack);

    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());

    int readIndex = _patternIndex;
    if (sequence.rotate() != 0) {
        int windowSize = sequence.last() - sequence.first() + 1;
        if (windowSize > 0) {
            int offset = (_patternIndex - sequence.first() + sequence.rotate()) % windowSize;
            if (offset < 0) offset += windowSize;
            readIndex = sequence.first() + offset;
        }
    }

    const bool lockActive = track.lock() && _lockedParents != nullptr;
    bool locked = lockActive && _lockedParents[readIndex].valid;

    float finalCv = 0.f;
    uint32_t durationTicks = divisor;
    bool isRest = false;
    bool isLegato = false;
    bool isSlide = false;
    bool isAccent = false;

    if (locked) {
        finalCv = _lockedParents[readIndex].cv;
        durationTicks = _lockedParents[readIndex].durationTicks;
        _eventDuration = durationTicks;
        isRest = _lockedParents[readIndex].rest;
        isLegato = _lockedParents[readIndex].legato;
        isSlide = _lockedParents[readIndex].slide;
        isAccent = _lockedParents[readIndex].accent;

        if (!isRest) {
            DBG_STO("%u GATEpush LOCKED ON p=%d", tick, _patternIndex);
            _cvQueue.push({ tick, finalCv, isSlide });
            _gateQueue.push({ tick, true, isAccent });

            bool hasChildren = false;
            for (int i = 0; i < 4; ++i) if (_lockedParents[readIndex].children[i].valid) hasChildren = true;

            if (!isLegato && !hasChildren) {
                uint32_t gateLen = (durationTicks * 50) / 100;
                DBG_STO("%u GATEpush LOCKED OFF p=%d", tick + gateLen, _patternIndex);
                _gateQueue.push({ tick + gateLen, false, false });
            }
            _activity = true;

            for (int i = 0; i < 4; ++i) {
                auto &child = _lockedParents[readIndex].children[i];
                if (child.valid) {
                    uint32_t childTick = tick + child.tickOffset;
                    uint32_t lowTick = childTick > tick + 2 ? childTick - 2 : tick;
                    _gateQueue.push({ lowTick, false, false });
                    _cvQueue.push({ childTick, child.cv, child.slide });
                    _gateQueue.push({ childTick, true, child.accent });
                    _gateQueue.push({ childTick + child.gateTicks, false, false });
                    DBG_STO("%u GATEpush LOCKED child on=%u off=%u", tick, childTick, childTick + child.gateTicks);
                }
            }
        } else {
            DBG_STO("%u GATEpush LOCKED REST p=%d", tick, _patternIndex);
            _gateQueue.push({ tick, false, false });
            if (track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always) _cvQueue.push({ tick, finalCv, false });
            _activity = false;
        }
    } else {
        // Source Evaluation
        if (sequence.rhythmMode() == StochasticSourceMode::Loop && !sequence.rhythmValid()) {
            StochasticGenerator::generateRhythm(sequence, track, _rng.next());
        }
        if (sequence.melodyMode() == StochasticSourceMode::Loop && !sequence.melodyValid()) {
            StochasticGenerator::generateMelody(sequence, track, scale, rootNote, _rng.next());
        }

        const auto &event = sequence.events()[readIndex];
        StochasticSourceEvent eval;
        eval.clear();

        // Keep rhythm and melody writes domain-preserving; direct packed bitfield
        // assignment can clear sibling flags such as rhythmValid.
        if (sequence.rhythmMode() == StochasticSourceMode::Live) {
            auto rhythm = StochasticGenerator::generateRhythmEvent(sequence, track, _rng);
            eval.mergeRhythmFrom(rhythm);
        } else {
            eval.mergeRhythmFrom(event);
        }

        if (sequence.melodyMode() == StochasticSourceMode::Live) {
            auto melody = StochasticGenerator::generateMelodyEvent(sequence, track, scale, rootNote, _lastDegree, _rng);
            eval.mergeMelodyFrom(melody);
            _lastDegree = int(eval.degree()) + int(eval.octave()) * scale.notesPerOctave();
        } else {
            eval.mergeMelodyFrom(event);
            _lastDegree = int(eval.degree()) + int(eval.octave()) * scale.notesPerOctave();
        }

        int activeNotes = scale.notesPerOctave();

        uint32_t mult = getDurationMultiplier(eval.durationIndex());
        _lastDurationIndex = eval.durationIndex();
        if (!sequence.durationTicketsActive() && sequence.variation() != 0) {
            int variationRoll = _rng.nextRange(100);
            if (variationRoll < std::abs(sequence.variation())) {
                 if (sequence.variation() > 0) {
                     mult *= 2;
                 } else {
                     mult = std::max(uint32_t(6), mult / 2);
                 }
            }
        }
        durationTicks = mult;

        if (!sequence.durationTicketsActive()) {
            durationTicks = (divisor * sequence.rate()) / 100;
            if (durationTicks < 1) durationTicks = 1;
        }
        _eventDuration = durationTicks;

        int note = int(eval.degree()) + (int(eval.octave()) + _jumpRegister + track.octave()) * activeNotes + track.transpose();
        finalCv = scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);

        // V5 Mask: deterministic playback thinning after source read/evaluation
        uint32_t maskSize = std::max(1, sequence.size());
        bool maskPass = sequence.mask() >= 100 ||
            ((uint32_t(eval.densityRank()) * 100) < (uint32_t(sequence.mask()) * maskSize));
        isRest = bool(eval.rest()) || !maskPass || !eval.rhythmValid() || !eval.melodyValid();
        isLegato = bool(eval.legato());
        isSlide = bool(eval.slide());
        isAccent = bool(eval.accent());

        // Evaluate Children
        StochasticGenerator::EvaluatedChild evalChildren[4];
        if (!isRest) {
            StochasticGenerator::evaluateChildren(evalChildren, sequence, eval, track, scale, rootNote, note, durationTicks, _rng);
        } else {
            for (int i = 0; i < 4; ++i) evalChildren[i].valid = false;
        }

        if (_lockedParents) {
            _lockedParents[readIndex].valid = true;
            _lockedParents[readIndex].cv = finalCv;
            _lockedParents[readIndex].durationTicks = durationTicks;
            _lockedParents[readIndex].rest = isRest;
            _lockedParents[readIndex].legato = isLegato;
            _lockedParents[readIndex].slide = isSlide;
            _lockedParents[readIndex].accent = isAccent;
            for (int i = 0; i < 4; ++i) {
                if (evalChildren[i].valid) {
                    _lockedParents[readIndex].children[i].valid = true;

                    int childNote = evalChildren[i].note;
                    if (sequence.burstPitch() == StochasticBurstPitch::Generate) {
                        // Apply pitch transforms to generated burst pitch
                        childNote += (_jumpRegister + track.octave()) * activeNotes + track.transpose();
                    }

                    _lockedParents[readIndex].children[i].cv = scale.noteToVolts(childNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
                    _lockedParents[readIndex].children[i].tickOffset = evalChildren[i].tickOffset;
                    _lockedParents[readIndex].children[i].gateTicks = evalChildren[i].gateTicks;
                    _lockedParents[readIndex].children[i].slide = isSlide;
                    _lockedParents[readIndex].children[i].accent = isAccent;
                } else {
                    _lockedParents[readIndex].children[i].valid = false;
                }
            }
        }

        if (!isRest) {
            DBG_STO("%u CVpush ON p=%d cv=%.3f", tick, _patternIndex, finalCv);
            _cvQueue.push({ tick, finalCv, isSlide });
            _gateQueue.push({ tick, true, isAccent });

            bool hasChildren = false;
            for (int i = 0; i < 4; ++i) if (evalChildren[i].valid) hasChildren = true;

            if (!isLegato && !hasChildren) {
                uint32_t gateLen = (durationTicks * 50) / 100;
                DBG_STO("%u GATEpush LIVE OFF p=%d", tick + gateLen, _patternIndex);
                _gateQueue.push({ tick + gateLen, false, false });
            }
            _activity = true;

            for (int i = 0; i < 4; ++i) {
                if (evalChildren[i].valid) {
                    uint32_t childTick = tick + evalChildren[i].tickOffset;
                    uint32_t lowTick = childTick > tick + 2 ? childTick - 2 : tick;

                    int childNote = evalChildren[i].note;
                    if (sequence.burstPitch() == StochasticBurstPitch::Generate) {
                        childNote += (_jumpRegister + track.octave()) * activeNotes + track.transpose();
                    }
                    float childCv = scale.noteToVolts(childNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);

                    _gateQueue.push({ lowTick, false, false });
                    _cvQueue.push({ childTick, childCv, isSlide });
                    _gateQueue.push({ childTick, true, isAccent });
                    _gateQueue.push({ childTick + evalChildren[i].gateTicks, false, false });
                    DBG_STO("%u GATEpush LIVE child on=%u off=%u", tick, childTick, childTick + evalChildren[i].gateTicks);
                }
            }
        } else {
            DBG_STO("%u CVpush REST p=%d cv=%.3f mode=%s", tick, _patternIndex, finalCv,
                track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always ? "Always" : "Gate");
            _gateQueue.push({ tick, false, false });
            if (track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always) _cvQueue.push({ tick, finalCv, false });
            _activity = false;
        }
    }

    _patternIndex++;
    if (_patternIndex > sequence.last()) {
        _patternIndex = sequence.first();
        _patternCycleEnded = true;

        if (!lockActive) {
            // Evaluate Loop-level Jump Register
            if (sequence.jump() > 0 && int(_rng.nextRange(100)) < sequence.jump()) {
                if (_jumpRegister == 0) {
                    _jumpRegister = _rng.nextRange(2) == 0 ? -1 : 1;
                } else {
                    _jumpRegister = 0; // Return to 0
                }
            }

            if (sequence.sleep() > 0) _sleepRemaining = (sequence.sleep() * 4) / 10;

            // Domain-aware mutation
            if (sequence.mutate() > 0 && int(_rng.nextRange(100)) < sequence.mutate()) {
                if (sequence.rhythmMode() == StochasticSourceMode::Loop && sequence.melodyMode() == StochasticSourceMode::Loop) {
                    if (_rng.nextRange(2) == 0) StochasticGenerator::mutateRhythmOne(sequence, track, _rng);
                    else StochasticGenerator::mutateMelodyOne(sequence, track, scale, rootNote, _rng);
                } else if (sequence.rhythmMode() == StochasticSourceMode::Loop) {
                    StochasticGenerator::mutateRhythmOne(sequence, track, _rng);
                } else if (sequence.melodyMode() == StochasticSourceMode::Loop) {
                    StochasticGenerator::mutateMelodyOne(sequence, track, scale, rootNote, _rng);
                }
            }

            // V5 Patience: exponential loop-count mapping
            if (sequence.patience() < 100) {
                _loopCycleCount++;
                uint32_t loopsBeforeRefresh;
                if (sequence.patience() == 0) {
                    loopsBeforeRefresh = 1;
                } else {
                    int bucket = clamp(sequence.patience() / 14, 0, 7);
                    loopsBeforeRefresh = uint32_t(1) << bucket;
                }
                if (_loopCycleCount >= loopsBeforeRefresh) {
                    if (sequence.rhythmMode() == StochasticSourceMode::Loop) sequence.setRhythmValid(false);
                    if (sequence.melodyMode() == StochasticSourceMode::Loop) sequence.setMelodyValid(false);
                    _loopCycleCount = 0;
                }
            }
        }
    }
}

void StochasticTrackEngine::refreshLoopSources() {
    auto &sequence = const_cast<StochasticSequence&>(*_sequence);
    auto &track = const_cast<StochasticTrack&>(_stochasticTrack);
    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());

    if (sequence.rhythmMode() == StochasticSourceMode::Loop) {
        sequence.setRhythmValid(false);
    }
    if (sequence.melodyMode() == StochasticSourceMode::Loop) {
        sequence.setMelodyValid(false);
    }
    _loopCycleCount = 0;
}

void StochasticTrackEngine::update(float dt) {
    if (_slideActive && _stochasticTrack.slideTime() > 0) {
        float slideDt = dt * 1000.f / (float(_stochasticTrack.slideTime()) + 1.f);
        _cvOutput += (_cvOutputTarget - _cvOutput) * std::min(1.f, slideDt);
    } else {
        _cvOutput = _cvOutputTarget;
    }
}
