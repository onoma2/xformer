#include "PhaseFluxTrackEngine.h"

#include "AccumulatorOps.h"
#include "Engine.h"

#include "model/Curve.h"
#include "model/Scale.h"
#include "model/StochasticTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

// PhaseFlux trigger-path instrumentation. Logs every rebuildSchedule + every
// pulse fire to stderr. Comment the define to silence.
#define DBG_PFX_ENABLE
#ifdef DBG_PFX_ENABLE
#define DBG_PFX(fmt, ...) std::fprintf(stderr, "[PFX] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_PFX(fmt, ...) do { } while (0)
#endif

namespace {

// §6.3 mask table. LSB = pulse 0. A set bit MUTES that pulse.
//   Off, 1in2, 1in3, 1in4, 2in3, 2in4, 3in4, 1in8
constexpr uint8_t kMaskTable[8] = {
    0x00, 0xAA, 0x49, 0x11, 0xB6, 0x55, 0x77, 0x01,
};

inline float pitchRangeToOctaves(PhaseFluxSequence::PitchRangeType v) {
    switch (v) {
    case PhaseFluxSequence::PitchRangeType::Half:  return 0.5f;
    case PhaseFluxSequence::PitchRangeType::One:   return 1.0f;
    case PhaseFluxSequence::PitchRangeType::Two:   return 2.0f;
    case PhaseFluxSequence::PitchRangeType::Three: return 3.0f;
    }
    return 1.0f;
}

inline Curve::Type temporalCurveLut(PhaseFluxSequence::TemporalCurveType v) {
    // Bounce = ExpDown3x per spec §16 implementer punch list — Phase C may
    // swap if hardware audition prefers ExpDown2x or a bespoke LUT.
    switch (v) {
    case PhaseFluxSequence::TemporalCurveType::Linear: return Curve::RampUp;
    case PhaseFluxSequence::TemporalCurveType::Bell:   return Curve::Bell;
    case PhaseFluxSequence::TemporalCurveType::Bounce: return Curve::ExpDown3x;
    }
    return Curve::RampUp;
}

inline Curve::Type pitchCurveLut(PhaseFluxSequence::PitchCurveType v) {
    switch (v) {
    case PhaseFluxSequence::PitchCurveType::Ramp:     return Curve::RampUp;
    case PhaseFluxSequence::PitchCurveType::Bell:     return Curve::Bell;
    case PhaseFluxSequence::PitchCurveType::Triangle: return Curve::Triangle;
    case PhaseFluxSequence::PitchCurveType::Bounce:   return Curve::ExpDown3x;
    }
    return Curve::RampUp;
}

inline float clamp01(float v) {
    return std::max(0.f, std::min(1.f, v));
}

inline float applyPowerBend(float x, int knob) {
    if (knob == 0) return x;
    return PhaseFluxMath::powerBend(clamp01(x), PhaseFluxMath::powerBendKnobToParam(knob));
}

inline int repeatMultiplier(PhaseFluxSequence::RepeatType v) {
    return PhaseFluxMath::repeatMultiplier(v);
}

inline bool isInWindow(float phi, PhaseFluxSequence::WindowType v) {
    return PhaseFluxMath::isInWindow(phi, v);
}

} // namespace

void PhaseFluxTrackEngine::advanceCounter(int &counter, int8_t &pendulumDir,
                                          const AccumulatorConfig &cfg, int step) {
    if (step == 0) return;

    // §13.9 polarity-to-bounds mapping. Uni: bounds depend on step sign;
    // Bi: bounds span both limits regardless of step sign.
    int absMin, absMax;
    if (cfg.polarity() == AccumulatorConfig::Polarity::Uni) {
        if (step > 0) { absMin = 0;                       absMax = +int(cfg.posLim()); }
        else          { absMin = -int(cfg.negLim());      absMax = 0;                  }
    } else {
        absMin = -int(cfg.negLim());
        absMax = +int(cfg.posLim());
    }

    const int direction = step > 0 ? +1 : -1;
    const int magnitude = std::abs(step);

    switch (cfg.order()) {
    case AccumulatorConfig::Order::Wrap:
        AccumulatorOps::tickWrap(counter, direction, absMin, absMax, magnitude);
        break;
    case AccumulatorConfig::Order::Pendulum: {
        // Signed step respected — first move follows the cell's configured drift sign.
        // Diverges from NoteTrack (unsigned step + separate Direction enum).
        int pdir = int(pendulumDir);
        AccumulatorOps::tickPendulum(counter, pdir, absMin, absMax, step);
        pendulumDir = int8_t(pdir);
        break;
    }
    case AccumulatorConfig::Order::Hold:
        AccumulatorOps::tickHold(counter, direction, absMin, absMax, magnitude);
        break;
    case AccumulatorConfig::Order::RTZ:
        // RTZ ops only checks bounds; advance manually first.
        counter += step;
        AccumulatorOps::tickRTZ(counter, absMin, absMax);
        break;
    }
}

void PhaseFluxTrackEngine::reset() {
    _resetTickOffset = 0;
    _firstTickAfterReset = true;
    _layoutDirty = true;
    _pulseFired = 0;
    _slotIdx = 0;
    _prevSlotIdx = -1;
    _activeCell = 0;
    _stagePhase = 0.f;
    _pitchPhase = 0.f;
    _gateState = false;
    _gateTimer = 0;
    _cvOutput = 0.f;
    _activity = false;
    _activityTimer = 0;
    _scheduleCount = 0;
    for (int i = 0; i < kStageCount; ++i) {
        _noteAccumCounter[i] = 0;
        _noteAccumDir[i] = 1;
        _pulseAccumCounter[i] = 0;
        _pulseAccumDir[i] = 1;
    }
    _cycleCount = 0;
    _prevCycleTick = 0;
    for (int i = 0; i <= kStageCount; ++i) {
        _cumulativeTicks[i] = 0;
    }
    _cycleTicks = 0;
    _sequence = &_phaseFluxTrack.sequence(pattern());
}

void PhaseFluxTrackEngine::restart() {
    // §11.1 light reset — _resetTickOffset re-anchored on next tick,
    // pulse-fire + gate cleared, accumulator counters preserved per §7.1.
    _firstTickAfterReset = true;
    _pulseFired = 0;
    _gateState = false;
    _gateTimer = 0;
    _prevSlotIdx = -1;
    _prevCycleTick = 0;
    _scheduleCount = 0;
}

void PhaseFluxTrackEngine::stop() {
    _gateState = false;
    _gateTimer = 0;
}

void PhaseFluxTrackEngine::changePattern() {
    // Engine::updatePlayState fires this every measure boundary, not only on real
    // pattern switches — clearing accumulators here would kill drift each measure.
    // Real pattern switches are caught by the seq-pointer rebind in tick().
}

bool PhaseFluxTrackEngine::detectLayoutChange() {
    if (!_sequence) return false;
    uint16_t div = uint16_t(_sequence->divisor());
    uint8_t mult = uint8_t(_sequence->clockMultiplier());
    if (div != _cachedDivisor || mult != _cachedClockMult) return true;
    for (int i = 0; i < kStageCount; ++i) {
        const auto &s = _sequence->stage(i);
        if (uint8_t(s.stageDivisor()) != _cachedStageDivisor[i]) return true;
        if (uint8_t(s.stageLen()) != _cachedStageLen[i]) return true;
        if (s.skip() != _cachedSkip[i]) return true;
    }
    return false;
}

void PhaseFluxTrackEngine::rebuildCumulativeTable() {
    int stageDivisorTicksArr[kStageCount];
    int stageLenArr[kStageCount];
    bool skipArr[kStageCount];
    for (int i = 0; i < kStageCount; ++i) {
        const auto &s = _sequence->stage(i);
        stageDivisorTicksArr[i] = PhaseFluxMath::stageDivisorTicks(s.stageDivisor());
        stageLenArr[i] = s.stageLen();
        skipArr[i] = s.skip();
        _cachedStageDivisor[i] = uint8_t(s.stageDivisor());
        _cachedStageLen[i] = uint8_t(s.stageLen());
        _cachedSkip[i] = s.skip();
    }
    _cachedDivisor = uint16_t(_sequence->divisor());
    _cachedClockMult = uint8_t(_sequence->clockMultiplier());

    _cycleTicks = PhaseFluxMath::computeCumulativeTicks(
        stageDivisorTicksArr, stageLenArr, skipArr,
        _sequence->divisor(),
        int(_engine.measureDivisor()),
        _sequence->clockMultiplier(),
        _cumulativeTicks);

    // §3.4 — held gate from old layout ends; pulse-fire memory cleared.
    // _prevSlotIdx reset forces rebuildSchedule on next tick even if same slot.
    _gateTimer = 0;
    _gateState = false;
    _pulseFired = 0;
    _scheduleCount = 0;
    _prevSlotIdx = -1;
    _prevCycleTick = 0;
    _layoutDirty = false;

#ifdef DBG_PFX_ENABLE
    DBG_PFX("layout: cycleTicks=%d divisor=%d clockMult=%d",
            _cycleTicks, _sequence->divisor(), _sequence->clockMultiplier());
    int activeCount = 0;
    for (int i = 0; i < kStageCount; ++i) {
        const auto &s = _sequence->stage(i);
        if (s.skip()) continue;
        ++activeCount;
        DBG_PFX("  stage[%d] ACTIVE pulseCount=%d tRep=%d tWin=%d pRep=%d pWin=%d stageLen=%d",
                i, s.pulseCount(),
                int(s.temporalRepeat()), int(s.temporalWindow()),
                int(s.pitchRepeat()),    int(s.pitchWindow()),
                s.stageLen());
    }
    DBG_PFX("  total active stages = %d", activeCount);
#endif
}

void PhaseFluxTrackEngine::rebuildSchedule(int slotDurationTicks) {
    _scheduleCount = 0;
    if (slotDurationTicks <= 0) return;

    const auto &stage = _sequence->stage(_activeCell);

    // §13.8 — clamp on the OUTPUT, not the counter: counter walks past 0..16
    // so wrap/pendulum bounds stay independent of pulse-count clipping.
    const auto &pulseCfg = _sequence->pulseAccumConfig();
    const int pulseCounterIdx = (pulseCfg.scope() == AccumulatorConfig::Scope::Local)
        ? _activeCell : 0;
    // Counter already stores accumulated delta (each tick adds step), so apply
    // directly — multiplying by step here would be quadratic growth.
    const int pulseAccOffset = _pulseAccumCounter[pulseCounterIdx];
    int pulseCount = stage.pulseCount() + pulseAccOffset;
    if (pulseCount < 0) pulseCount = 0;
    if (pulseCount > kMaxPulses) pulseCount = kMaxPulses;

    const Scale &scale = _sequence->selectedScale(_model.project().scale());
    const int rootNote = _sequence->selectedRootNote(_model.project().rootNote());
    const int octave = _phaseFluxTrack.octave();
    const int transpose = _phaseFluxTrack.transpose();

    const Curve::Type tempCurveType = temporalCurveLut(stage.temporalCurve());
    const bool tFlipV = stage.temporalFlipV();
    const bool tFlipH = stage.temporalFlipH();
    const int phaseShift = stage.phaseShift();
    const int maskByte = kMaskTable[int(stage.mask())];
    const int maskShift = stage.maskShift();

    // Pitch source: Cell mode = active stage; Global mode = stage[0]'s curve.
    const bool isGlobalPitch =
        _sequence->pitchMode() == PhaseFluxSequence::PitchMode::Global;
    const auto &pitchStage = isGlobalPitch ? _sequence->stage(0) : stage;
    const Curve::Type pitchCurveType = pitchCurveLut(pitchStage.pitchCurve());
    const bool pFlipV = pitchStage.pitchFlipV();
    const bool pFlipH = pitchStage.pitchFlipH();
    const int pitchWarpKnob = pitchStage.pitchWarp();
    const int pitchRespKnob = pitchStage.pitchResponse();

    // Global mode advance projection: pitchPhase at slot start + per-tick rate.
    const float pitchAdvancePerTick = isGlobalPitch
        ? (float(PhaseFluxSequence::pitchRateNum(_sequence->pitchRate())) /
           float(PhaseFluxSequence::pitchRateDen(_sequence->pitchRate()))) /
              float(slotDurationTicks)
        : 0.f;

    const float rangeOctaves = pitchRangeToOctaves(stage.pitchRange());
    const int rangeDegrees = std::max(1, int(std::round(rangeOctaves * scale.notesPerOctave())));
    const auto &noteCfg = _sequence->noteAccumConfig();
    // Track scope shares counter [0]; Local scope uses per-cell index (§13.3).
    const int noteCounterIdx = (noteCfg.scope() == AccumulatorConfig::Scope::Local)
        ? _activeCell : 0;
    // Counter already stores accumulated degrees — apply directly.
    const int noteAccOffset = _noteAccumCounter[noteCounterIdx];
    const int baseDegree = stage.basePitch() + noteAccOffset
        + octave * scale.notesPerOctave() + transpose;

    // §6.1 + §6.2 — build per-pulse (triggerOffset, cv) pairs, then apply
    // §6.3 mask and §6.4 collision clamp.
    struct PulseCandidate {
        int origIdx;
        float triggerTime;
        float cv;
    };
    PulseCandidate cands[kMaxPulses];
    int candCount = 0;

    // §14.2 Window → Repeat (temporal). Repeat splits pulseCount into R
    // sub-sections (earlier subs get the extra pulse(s) when not divisible).
    const int tempR = repeatMultiplier(stage.temporalRepeat());
    const int subBaseSize = pulseCount / tempR;
    const int subRemainder = pulseCount % tempR;
    const int subOversizeBoundary = subRemainder * (subBaseSize + 1);

    // §14.2 Pitch Window + Repeat — Cell mode only (Global has its own rate).
    const int pitchR = isGlobalPitch ? 1 : repeatMultiplier(stage.pitchRepeat());
    const auto pitchWindowType = isGlobalPitch
        ? PhaseFluxSequence::WindowType::Off
        : stage.pitchWindow();

    for (int i = 0; i < pulseCount; ++i) {
        // §6.3 mask gates on the ORIGINAL pulse index — survives §6.4 reorder.
        bool muted = (maskByte >> ((i + maskShift) & 7)) & 1;
        if (muted) continue;

        // §14.2 Repeat — assign pulse i to a sub-section. Earlier subs hold
        // (subBaseSize + 1) pulses if there's a remainder; later subs hold
        // (subBaseSize).
        int subIdx;
        int localPulseIdx;
        int pulsesInThisSub;
        if (i < subOversizeBoundary) {
            subIdx = i / (subBaseSize + 1);
            localPulseIdx = i % (subBaseSize + 1);
            pulsesInThisSub = subBaseSize + 1;
        } else {
            const int j = i - subOversizeBoundary;
            subIdx = subRemainder + (subBaseSize > 0 ? j / subBaseSize : 0);
            localPulseIdx = (subBaseSize > 0) ? (j % subBaseSize) : 0;
            pulsesInThisSub = subBaseSize > 0 ? subBaseSize : 1;
        }
        const float t_raw_local = float(localPulseIdx) / float(pulsesInThisSub);

        // §14.2 temporal Window — drop pulse if outside the visible band.
        if (!isInWindow(t_raw_local, stage.temporalWindow())) continue;

        // §6.1 temporal pipeline on the local position, then map back to
        // global via the sub-section index.
        float t_warped = applyPowerBend(t_raw_local, stage.temporalWarp());
        float t_curved = Curve::eval(tempCurveType, t_warped);
        float t_flipped = tFlipV ? (1.f - t_curved) : t_curved;
        float t_final_local = applyPowerBend(t_flipped, stage.temporalResponse());
        float t_final_global = (float(subIdx) + t_final_local) / float(tempR);
        float t_shifted = t_final_global + (float(phaseShift) * 0.125f);
        t_shifted = std::fmod(t_shifted, 1.f);
        if (t_shifted < 0.f) t_shifted += 1.f;
        float triggerTime = t_shifted * float(slotDurationTicks);

        // §6.2 pitch pipeline. Cell mode φ = stagePhase-equivalent (today).
        // Global mode φ = pitchPhase projected to triggerTime — free-running.
        float phi;
        if (isGlobalPitch) {
            phi = _pitchPhase + triggerTime * pitchAdvancePerTick;
            phi -= std::floor(phi);
        } else {
            phi = (pulseCount == 1) ? 0.0f : t_shifted;
        }
        // §14.2 pitch Window — drop pulse contribution if outside band.
        if (!isInWindow(phi, pitchWindowType)) continue;
        // §14.2 pitch Repeat — fmod expands curve frequency by pitchR.
        float phi_repeated = (pitchR > 1) ? std::fmod(phi * float(pitchR), 1.f) : phi;
        float phi_warped = applyPowerBend(phi_repeated, pitchWarpKnob);
        float phi_input = pFlipH ? (1.f - phi_warped) : phi_warped;
        float p_curved = Curve::eval(pitchCurveType, phi_input);
        float p_flipped = pFlipV ? (1.f - p_curved) : p_curved;
        float p_final = applyPowerBend(p_flipped, pitchRespKnob);
        float offsetDegrees = 0.f;
        switch (stage.pitchDirection()) {
        case PhaseFluxSequence::PitchDirectionType::Up:
            offsetDegrees = p_final * float(rangeDegrees);
            break;
        case PhaseFluxSequence::PitchDirectionType::Down:
            offsetDegrees = -p_final * float(rangeDegrees);
            break;
        case PhaseFluxSequence::PitchDirectionType::Bipolar:
            offsetDegrees = (p_final - 0.5f) * float(rangeDegrees);
            break;
        }

        int degree = baseDegree + int(std::round(offsetDegrees));

        // §6.2.1 MaskMelody + TiltMelody — pitch-centrality filter ported from
        // StochasticTrackEngine (12-EDO tuned; see StochasticTypes.h).
        if (stage.maskMelody() < 100) {
            const int N = scale.notesPerOctave();
            if (N > 0) {
                int degInOct = ((degree % N) + N) % N;
                const uint32_t centralityMilli = std::min<uint32_t>(1000,
                    uint32_t(stochasticPitchCentrality(degInOct, N)) * 1000u /
                    uint32_t(kStochasticPitchCentralityMax));
                const uint32_t tiltMag = uint32_t(stage.tiltMelody());
                const uint32_t effectiveMilli =
                    ((100 - tiltMag) * centralityMilli + tiltMag * (1000 - centralityMilli)) / 100;
                const uint32_t maskMilli = uint32_t(stage.maskMelody()) * 10;
                if (effectiveMilli < (1000 - maskMilli)) continue;
            }
        }

        float cv = scale.noteToVolts(degree);
        if (scale.isChromatic()) cv += float(rootNote) * (1.f / 12.f);

        cands[candCount].origIdx = i;
        cands[candCount].triggerTime = triggerTime;
        cands[candCount].cv = cv;
        ++candCount;

        // Pulse-trigger advances fire AFTER candidate accepted (post-mask).
        // Affects the NEXT pulse / cell — Cirklon next-step semantics.
        if (stage.accumulatorTrigger() == PhaseFluxSequence::AccumulatorTriggerType::Pulse) {
            int step = stage.accumulatorStep();
            if (step != 0) {
                advanceCounter(_noteAccumCounter[noteCounterIdx],
                               _noteAccumDir[noteCounterIdx], noteCfg, step);
            }
        }
        if (stage.pulseAccumTrigger() == PhaseFluxSequence::AccumulatorTriggerType::Pulse) {
            int step = stage.pulseAccumStep();
            if (step != 0) {
                advanceCounter(_pulseAccumCounter[pulseCounterIdx],
                               _pulseAccumDir[pulseCounterIdx], pulseCfg, step);
            }
        }
    }

    // Insertion sort by trigger time — avoids std::sort inlining past array bounds.
    for (int i = 1; i < candCount; ++i) {
        PulseCandidate key = cands[i];
        int j = i - 1;
        while (j >= 0 && cands[j].triggerTime > key.triggerTime) {
            cands[j + 1] = cands[j];
            --j;
        }
        cands[j + 1] = key;
    }

    // §6.4 schedule — trigger overlap is allowed. NoteTrack precedent: the
    // runtime tick loop merges overlapping gates via _gateState=true retrigger,
    // so no need for "gap between pulses" or "previous gate must end first"
    // guards. Each candidate fires at its computed time; gate length follows
    // the per-stage gateLength rules below.
    if (slotDurationTicks < int(kMinPulseGateTicks)) return;

    for (int k = 0; k < candCount; ++k) {
        int t = int(cands[k].triggerTime);

        int nextT = (k + 1 < candCount) ? int(cands[k + 1].triggerTime) : slotDurationTicks;
        int pulsePeriod = nextT - t;
        if (pulsePeriod <= 0) continue;

        // §6.4 gate-length rules:
        //   gateLength == 0 → explicit silence, pulse dropped
        //   gateLength == 1 → always-audible-minimum sentinel; floor at kMinPulseGateTicks
        //   gateLength >= 2 → scale by percent, drop if computed gate < kMinPulseGateTicks
        int len = int(stage.gateLength());
        if (len == 0) continue;

        int gateTicks = (len * pulsePeriod) / 100;
        if (len == 1 && gateTicks < int(kMinPulseGateTicks)) {
            gateTicks = int(kMinPulseGateTicks);
        }
        if (gateTicks < int(kMinPulseGateTicks)) continue;

        if (_scheduleCount >= kMaxPulses) break;
        _schedule[_scheduleCount].triggerOffset = uint16_t(std::max(0, t));
        _schedule[_scheduleCount].gateTicks = uint16_t(gateTicks);
        _schedule[_scheduleCount].cv = cands[k].cv;
        ++_scheduleCount;
    }

#ifdef DBG_PFX_ENABLE
    {
        const auto &dbgStage = _sequence->stage(_activeCell);
        DBG_PFX("rebuild cell=%d slot=%d slotDur=%d pulses_base=%d effective=%d cands=%d scheduled=%d "
                "tWin=%d tRep=%d gateLen=%d mask=%d",
                _activeCell, _slotIdx, slotDurationTicks,
                dbgStage.pulseCount(), pulseCount, candCount, _scheduleCount,
                int(dbgStage.temporalWindow()), int(dbgStage.temporalRepeat()),
                int(dbgStage.gateLength()), int(dbgStage.mask()));
        for (int k = 0; k < _scheduleCount; ++k) {
            DBG_PFX("  fire[%d] offset=%u gate=%u cv=%.3f",
                    k,
                    unsigned(_schedule[k].triggerOffset),
                    unsigned(_schedule[k].gateTicks),
                    double(_schedule[k].cv));
        }
        if (candCount > _scheduleCount) {
            DBG_PFX("  DROPPED %d candidate(s) — gateLen too short or pulseCount full",
                    candCount - _scheduleCount);
        }
    }
#endif

    // §6.1.1 temporal flipH — reflect each scheduled pulse's [triggerOffset,
    // triggerOffset+gateTicks] interval around the slot midpoint. Preserves
    // gate duration; new triggerOffset = slotDur - oldEnd. Pulses retain
    // their CV pairing, so the pitch profile mirrors in time as well.
    // Re-sort by triggerOffset since order flips.
    if (tFlipH && _scheduleCount > 0) {
        for (int k = 0; k < _scheduleCount; ++k) {
            int endTick = int(_schedule[k].triggerOffset) + int(_schedule[k].gateTicks);
            int newOffset = slotDurationTicks - endTick;
            _schedule[k].triggerOffset = uint16_t(std::max(0, newOffset));
        }
        for (int i = 1; i < _scheduleCount; ++i) {
            ScheduledPulse key = _schedule[i];
            int j = i - 1;
            while (j >= 0 && _schedule[j].triggerOffset > key.triggerOffset) {
                _schedule[j + 1] = _schedule[j];
                --j;
            }
            _schedule[j + 1] = key;
        }
    }
}

TrackEngine::TickResult PhaseFluxTrackEngine::tick(uint32_t tick) {
    // Resolve sequence lazily — pattern() can change without changePattern().
    // This branch fires once on real pattern switches; §13.4 accumulator hard-reset
    // belongs here, not in changePattern() (which the engine calls every measure).
    PhaseFluxSequence *seq = &_phaseFluxTrack.sequence(pattern());
    if (seq != _sequence) {
        _sequence = seq;
        _layoutDirty = true;
        _prevSlotIdx = -1;
        _prevCycleTick = 0;
        for (int i = 0; i < kStageCount; ++i) {
            _noteAccumCounter[i] = 0;
            _noteAccumDir[i] = 1;
            _pulseAccumCounter[i] = 0;
            _pulseAccumDir[i] = 1;
        }
        _cycleCount = 0;
    }

    if (_firstTickAfterReset) {
        _resetTickOffset = tick;
        _firstTickAfterReset = false;
    }

    if (_layoutDirty || detectLayoutChange()) {
        rebuildCumulativeTable();
    }

    uint32_t relativeTick = tick - _resetTickOffset;

    // Sequence globalPhase shift (CurveTrack precedent — CurveTrackEngine.cpp:404).
    // Fraction 0..1 of the cycle, added to relativeTick mod cycleTicks.
    if (_cycleTicks > 0) {
        float phase = _sequence->globalPhase();
        if (phase > 0.f) {
            uint32_t shift = uint32_t(phase * float(_cycleTicks));
            relativeTick = (relativeTick + shift) % uint32_t(_cycleTicks);
        }
    }

    // §3.3 Reset Measure — light reset, preserves counters per §7.1.
    int resetMeasure = _sequence->resetMeasure();
    if (resetMeasure > 0) {
        uint32_t resetDivisor = uint32_t(resetMeasure) * _engine.measureDivisor();
        if (relativeTick % resetDivisor == 0 && relativeTick != 0) {
            _resetTickOffset = tick;
            relativeTick = 0;
            _pulseFired = 0;
            _gateTimer = 0;
            _gateState = false;
            _prevSlotIdx = -1;
            _prevCycleTick = 0;
        }
    }

    int slotIdx = 0, activeCell = 0;
    float stagePhase = 0.f;
    bool valid = PhaseFluxMath::deriveTickPosition(
        relativeTick, _cumulativeTicks, _cycleTicks,
        slotIdx, activeCell, stagePhase);

    if (!valid) {
        // §3.5 idle — hold gate low, freeze counters, preserve CV.
        if (_gateState) {
            _gateState = false;
            _gateTimer = 0;
            return GateUpdate;
        }
        return NoUpdate;
    }

    _slotIdx = slotIdx;
    _activeCell = activeCell;
    _stagePhase = stagePhase;

    // §13.4 cycle-wrap detection — timing-based per spec §13.10 Task 5.
    // Slot-based detection misses when stage 0 is skipped (zero-width slot)
    // or when only one slot is active (no slot transition occurs).
    uint32_t cycleTick = (_cycleTicks > 0) ? (relativeTick % uint32_t(_cycleTicks)) : 0;
    if (_cycleTicks > 0 && cycleTick < _prevCycleTick) {
        ++_cycleCount;
        // 1-active-stage case: _slotIdx never changes, so the slot-change branch
        // below never resets pulse-fired memory. Reset here on every cycle wrap.
        // Multi-stage case is unaffected — slot change also clears _pulseFired.
        _pulseFired = 0;
        const auto &noteCfg = _sequence->noteAccumConfig();
        if (noteCfg.reset() > 0 && _cycleCount % noteCfg.reset() == 0) {
            for (int i = 0; i < kStageCount; ++i) {
                _noteAccumCounter[i] = 0;
                _noteAccumDir[i] = 1;
            }
        }
        const auto &pulseCfg = _sequence->pulseAccumConfig();
        if (pulseCfg.reset() > 0 && _cycleCount % pulseCfg.reset() == 0) {
            for (int i = 0; i < kStageCount; ++i) {
                _pulseAccumCounter[i] = 0;
                _pulseAccumDir[i] = 1;
            }
        }
    }
    _prevCycleTick = cycleTick;

    TickResult result = NoUpdate;

    // §6.2.2 continuous pitch CV. cvUpdateMode == Always evaluates the pitch
    // pipeline every engine tick from _stagePhase, producing a smooth envelope
    // independent of pulse activity. Gate mode falls through unchanged — CV
    // only updates inside the pulse-fire loop below.
    if (_phaseFluxTrack.cvUpdateMode() == PhaseFluxTrack::CvUpdateMode::Always) {
        const auto &stage = _sequence->stage(_activeCell);
        if (!stage.skip()) {
            const Scale &scale = _sequence->selectedScale(_model.project().scale());
            const int rootNote = _sequence->selectedRootNote(_model.project().rootNote());
            const int octave = _phaseFluxTrack.octave();
            const int transpose = _phaseFluxTrack.transpose();
            const float rangeOctaves = pitchRangeToOctaves(stage.pitchRange());
            const int rangeDegrees = std::max(1, int(std::round(rangeOctaves * scale.notesPerOctave())));
            const auto &noteCfg = _sequence->noteAccumConfig();
            // Track scope shares counter [0]; Local scope uses per-cell index (§13.3).
            const int counterIdx = (noteCfg.scope() == AccumulatorConfig::Scope::Local)
                ? _activeCell : 0;
            // Counter already stores accumulated degrees — apply directly.
            const int noteAccOffset = _noteAccumCounter[counterIdx];
            const int baseDegree = stage.basePitch() + noteAccOffset
                + octave * scale.notesPerOctave() + transpose;

            const bool isGlobalPitch =
                _sequence->pitchMode() == PhaseFluxSequence::PitchMode::Global;
            const auto &pitchStage = isGlobalPitch ? _sequence->stage(0) : stage;
            const Curve::Type pitchCurveType = pitchCurveLut(pitchStage.pitchCurve());

            float phi = isGlobalPitch ? _pitchPhase : _stagePhase;
            // §14.2 pitch Window — Cell mode only; hold CV when phi is in
            // hidden band (engine doesn't update output).
            const auto pitchWindowType = isGlobalPitch
                ? PhaseFluxSequence::WindowType::Off
                : stage.pitchWindow();
            if (isInWindow(phi, pitchWindowType)) {
                // §14.2 pitch Repeat — Cell mode only; multiplies curve frequency.
                const int pitchR = isGlobalPitch ? 1 : repeatMultiplier(stage.pitchRepeat());
                float phi_repeated = (pitchR > 1) ? std::fmod(phi * float(pitchR), 1.f) : phi;
                float phi_warped = applyPowerBend(phi_repeated, pitchStage.pitchWarp());
                float phi_input = pitchStage.pitchFlipH() ? (1.f - phi_warped) : phi_warped;
                float p_curved = Curve::eval(pitchCurveType, phi_input);
                float p_flipped = pitchStage.pitchFlipV() ? (1.f - p_curved) : p_curved;
                float p_final = applyPowerBend(p_flipped, pitchStage.pitchResponse());

                float offsetDegrees = 0.f;
                switch (stage.pitchDirection()) {
                case PhaseFluxSequence::PitchDirectionType::Up:
                    offsetDegrees = p_final * float(rangeDegrees); break;
                case PhaseFluxSequence::PitchDirectionType::Down:
                    offsetDegrees = -p_final * float(rangeDegrees); break;
                case PhaseFluxSequence::PitchDirectionType::Bipolar:
                    offsetDegrees = (p_final - 0.5f) * float(rangeDegrees); break;
                }
                int degree = baseDegree + int(std::round(offsetDegrees));
                float cv = scale.noteToVolts(degree);
                if (scale.isChromatic()) cv += float(rootNote) * (1.f / 12.f);

                if (cv != _cvOutput) {
                    _cvOutput = cv;
                    result = result | CvUpdate;
                }
            }
        }
    }

    // Slot change: advance previous cell's accumulator, rebuild schedule.
    if (_slotIdx != _prevSlotIdx) {
        if (_prevSlotIdx >= 0) {
            // §13.4 Stage-trigger advance for the completing cell.
            const int completedCell = int(PhaseFluxMath::snakeOrder()[_prevSlotIdx]);
            const auto &completed = _sequence->stage(completedCell);

            // Note accumulator — Stage trigger only here; Pulse trigger advances in rebuildSchedule's per-pulse loop.
            if (completed.accumulatorTrigger() == PhaseFluxSequence::AccumulatorTriggerType::Stage) {
                const auto &cfg = _sequence->noteAccumConfig();
                const int idx = (cfg.scope() == AccumulatorConfig::Scope::Track) ? 0 : completedCell;
                advanceCounter(_noteAccumCounter[idx], _noteAccumDir[idx], cfg, completed.accumulatorStep());
            }

            // Pulse accumulator — Stage trigger only here; Pulse trigger advances in rebuildSchedule's per-pulse loop.
            if (completed.pulseAccumTrigger() == PhaseFluxSequence::AccumulatorTriggerType::Stage) {
                const auto &cfg = _sequence->pulseAccumConfig();
                const int idx = (cfg.scope() == AccumulatorConfig::Scope::Track) ? 0 : completedCell;
                advanceCounter(_pulseAccumCounter[idx], _pulseAccumDir[idx], cfg, completed.pulseAccumStep());
            }
        }
        _pulseFired = 0;
        _prevSlotIdx = _slotIdx;

        int slotDuration = _cumulativeTicks[_slotIdx + 1] - _cumulativeTicks[_slotIdx];
        rebuildSchedule(slotDuration);
    }

    int posInSlot = int(cycleTick) - _cumulativeTicks[_slotIdx];

    // Fire any scheduled pulses whose trigger has been reached this tick.
    for (int k = 0; k < _scheduleCount; ++k) {
        if (_pulseFired & (1 << k)) continue;
        if (posInSlot < int(_schedule[k].triggerOffset)) continue;

        _pulseFired |= uint8_t(1 << k);

        _cvOutput = _schedule[k].cv;
        _gateState = true;
        _gateTimer = _schedule[k].gateTicks;
        _activity = true;
        _activityTimer = kActivityPulseTicks;
        result = result | CvUpdate | GateUpdate;
        DBG_PFX("fire tick=%u cell=%d slot=%d k=%d posInSlot=%d offset=%u gate=%u cv=%.3f",
                unsigned(tick), _activeCell, _slotIdx, k, posInSlot,
                unsigned(_schedule[k].triggerOffset),
                unsigned(_schedule[k].gateTicks),
                double(_schedule[k].cv));
    }

    // Drop gate when timer expires.
    if (_gateTimer > 0) {
        --_gateTimer;
        if (_gateTimer == 0 && _gateState) {
            _gateState = false;
            result = result | GateUpdate;
        }
    }

    if (_activityTimer > 0) {
        --_activityTimer;
    }

    // Continuous pitchPhase accumulator. Advances every tick regardless of
    // pitchMode so mode flips are seamless (Global picks up wherever the
    // accumulator happens to be). Rate × tempo, where one cell = one unit.
    if (_slotIdx >= 0 && _slotIdx < kStageCount) {
        int slotTicks = _cumulativeTicks[_slotIdx + 1] - _cumulativeTicks[_slotIdx];
        if (slotTicks > 0) {
            float ratePerSlot =
                float(PhaseFluxSequence::pitchRateNum(_sequence->pitchRate())) /
                float(PhaseFluxSequence::pitchRateDen(_sequence->pitchRate()));
            _pitchPhase += ratePerSlot / float(slotTicks);
            _pitchPhase -= std::floor(_pitchPhase);
        }
    }

    return result;
}

void PhaseFluxTrackEngine::update(float dt) {
    (void)dt;
    // Slide / smoothing deferred to Phase C.
}
