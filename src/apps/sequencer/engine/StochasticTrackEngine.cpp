#include "StochasticTrackEngine.h"
#include "StochasticGenerator.h"
#include "StochasticCache.h"
#include "KeyedRng.h"
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

namespace {
struct PackedDirectHistoryEvent {
    int8_t cvSixteenths = 0;
    uint8_t children = 0;
    uint8_t flags = 0; // bit0 rest, bit1 gate
};
static_assert(sizeof(PackedDirectHistoryEvent::cvSixteenths) == 1, "Direct history CV must stay 8-bit");
static_assert(sizeof(PackedDirectHistoryEvent) <= 4, "Direct history event must stay UI-sized");

PackedDirectHistoryEvent gDirectHistory[CONFIG_TRACK_COUNT][StochasticTrackEngine::kDirectHistoryMax] = {};
uint8_t gDirectHistoryCount[CONFIG_TRACK_COUNT] = {};

int directHistoryTrackIndex(const Track &track) {
    int index = track.trackIndex();
    if (index < 0) index = 0;
    if (index >= CONFIG_TRACK_COUNT) index = CONFIG_TRACK_COUNT - 1;
    return index;
}

// The engine cache lives inside StochasticTrackEngine as a member (`_stepCache`),
// not in a file-scope per-track array, so memory is only paid for tracks
// that actually run in Stochastic mode.
}

// Gate scheduling trace — disabled for release
//#define DBG_STO_ENABLE
#ifdef DBG_STO_ENABLE
#define DBG_STO(fmt, ...) printf("[STO] " fmt "\n", ##__VA_ARGS__)
#else
#define DBG_STO(fmt, ...)
#endif

// Forward decl for use by patienceProbability below.
static float poissonCdf(int k, float lambda);

// Knob 0..99 → λ ∈ [1, 128] on a log2 curve; knob 100 is the off-sentinel
// ("never regenerate"). Musical time scales logarithmically — every ~14
// knob units doubles the expected wait, so the perceptually-useful short-
// to-medium loop region (1, 2, 4, 8, 16 loops) is spread across the lower
// 60% of the knob instead of squeezed into the bottom sliver.
static inline float patienceLambda(int patience) {
    int p = patience < 0 ? 0 : (patience > 100 ? 100 : patience);
    return std::exp2(float(p) * 7.0f / 99.0f);
}

float StochasticTrackEngine::patienceProbability(uint32_t loops, int patience) {
    if (patience >= 100) return 0.f;   // off sentinel — never regenerate
    return poissonCdf(int(loops), patienceLambda(patience));
}

float StochasticTrackEngine::patienceMeter(uint32_t loops, int patience) {
    if (patience >= 100) return 0.f;   // off sentinel — UI shows no boredom progress
    float lambda = patienceLambda(patience);
    float meter = float(loops) / lambda;
    if (meter > 1.0f) meter = 1.0f;
    return meter;
}

// Gate length picker. Knob `spread` 0..100 controls deviation around the
// hardcoded 50% center. spread=0 → exact 50% every event; spread=100 → triangular
// distribution across the allowed range, still peak-centered at 50%. Floored at
// 10% of duration AND at kMinAudibleGateTicks (audible-trigger guarantee for
// very short events where even 50% would be too brief).
using stochastic_cache::kMinAudibleGateTicks;   // ~16 ms @ 192 PPQN, 120 BPM

static uint32_t pickGateLength(uint32_t durationTicks, int spread, Random &rng) {
    int pct = 50;
    if (spread > 0) {
        int r1 = int(rng.nextRange(101));
        int r2 = int(rng.nextRange(101));
        int tri = (r1 + r2) / 2;                              // triangular, peak 50
        pct = 50 + ((tri - 50) * spread) / 100;
    }
    if (pct < 10) pct = 10;
    if (pct > 100) pct = 100;
    uint32_t ticks = (durationTicks * uint32_t(pct)) / 100;
    if (ticks < kMinAudibleGateTicks) ticks = kMinAudibleGateTicks;
    if (ticks > durationTicks) ticks = durationTicks;          // never exceed event
    return ticks;
}

// Poisson CDF F(k; λ) = Σᵢ₌₀ᵏ e^(-λ) λⁱ / i!. Iterative, one exp() + k mults.
// Used by patience: probability of "time to regenerate" given k loops survived.
static float poissonCdf(int k, float lambda) {
    if (k < 0) return 0.0f;
    if (lambda <= 0.0f) return 1.0f;
    if (k > 200) k = 200;  // guard against runaway loops
    float term = std::exp(-lambda);
    float sum = term;
    for (int i = 1; i <= k; ++i) {
        term *= lambda / float(i);
        sum += term;
    }
    return sum > 1.0f ? 1.0f : sum;
}

StochasticTrackEngine::StochasticTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
    TrackEngine(engine, model, track, linkedTrackEngine),
    _stochasticTrack(track.stochasticTrack()),
    _rng(0x12345678 + track.trackIndex())
{
    reset();
}

uint8_t StochasticTrackEngine::directHistoryCount() const {
    return gDirectHistoryCount[directHistoryTrackIndex(_track)];
}

StochasticTrackEngine::DirectHistoryEvent StochasticTrackEngine::directHistoryEvent(int age) const {
    int trackIndex = directHistoryTrackIndex(_track);
    uint8_t count = gDirectHistoryCount[trackIndex];
    if (age < 0) age = 0;
    if (age >= count) age = count > 0 ? count - 1 : 0;
    const auto &packed = gDirectHistory[trackIndex][age];
    DirectHistoryEvent event;
    event.cv = float(packed.cvSixteenths) * 0.0625f;
    event.children = packed.children;
    event.rest = (packed.flags & 1) != 0;
    event.gate = (packed.flags & 2) != 0;
    return event;
}


void StochasticTrackEngine::resetMeasure() {
    auto &seq = stochasticTrack().sequence(pattern());
    // Count the reset-measure boundary toward all cycle-end hooks ONLY when
    // it preempts a natural wrap that hasn't already fired. Natural wraps
    // run before reset boundaries in normal playback (often one tick apart),
    // so unconditional roll here would double-count.
    // 2026-05-24: bundled all four hooks (jump, sleep, patience, mutate) so
    // they stay in lockstep — previously only patience advanced on resets.
    bool preempted = (_currentStep != seq.first());
    _currentStep = seq.first();
    _relativeTick = 0;
    _eventElapsed = 0;
    _eventDuration = 0;
    if (preempted) {
        rollCycleEndHooks();
    }
}

void StochasticTrackEngine::reset() {
    _currentStep = stochasticTrack().sequence(pattern()).first();
    _relativeTick = 0;
    _sleepRemaining = 0;
    _loopCycleCount = 0;
    _loopCycleCountMelody = 0;
    _lastAppliedSize = 0;
    _lastAppliedFirst = 0;
    _lastDegree = -1;
    _pitchState = {};
    _jumpRegister = 0;
    _lastFreeStepIndex = -1;
    _patternCycleEnded = false;
    _eventElapsed = 0;
    _eventDuration = 0;
    _activity = false;
    _gateOutput = false;
    _slideActive = false;
    _cvOutput = 0.f;
    _cvOutputTarget = 0.f;
    _lastStepContentValid = false;
    clearDirectHistory();
    _gateQueue.clear();
    _cvQueue.clear();
    _rng = Random(0x12345678 + _track.trackIndex());
    changePattern();
}

void StochasticTrackEngine::restart() {
    _currentStep = stochasticTrack().sequence(pattern()).first();
    _relativeTick = 0;
    _lastDegree = -1;
    _pitchState = {};
    _lastFreeStepIndex = -1;
    _eventElapsed = 0;
    _eventDuration = 0;
    _lastStepContentValid = false;
    clearDirectHistory();
    _rng = Random(0x12345678 + _track.trackIndex());
}

TrackEngine::TickResult StochasticTrackEngine::tick(uint32_t tick) {
    ASSERT(_sequence != nullptr, "invalid sequence");
    const auto &sequence = this->sequence();

    uint32_t divisor = effectiveDivisor();

    // Reset measure
    uint32_t resetDivisor = sequence.resetMeasure() * _engine.measureDivisor();
    uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;
    if (resetDivisor > 0 && relativeTick == 0) {
        resetMeasure();
    }

    // Event-driven step advancement. The current event's durationTicks (set in
    // triggerStep, from the LUT entry × divisor) controls when the NEXT event
    // fires. Same path for Duration-Tickets mode and Live/Loop mode — only the
    // duration source differs (weighted ticket pick vs noteDuration+variation).
    _eventElapsed++;
    if (_eventDuration == 0 || _eventElapsed >= _eventDuration) {
        _eventElapsed = 0;
        if (_sleepRemaining > 0) {
            _sleepRemaining--;
        } else {
            triggerStep(tick, divisor);
        }
    }

    // Process gate queue
    TickResult result = TickResult::NoUpdate;
    while (!_gateQueue.empty() && tick >= _gateQueue.front().tick) {
        auto &ev = _gateQueue.front();
        _gateOutput = ev.gate;
        _gateQueue.pop();
        // DBG_STO("%u GATE pop gate=%d", tick, _gateOutput);
        result |= TickResult::GateUpdate;
    }

    // Process CV queue
    while (!_cvQueue.empty() && tick >= _cvQueue.front().tick) {
        auto &ev = _cvQueue.front();
        // DBG_STO("%u CVpop cv=%.3f", tick, ev.cv);
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
    _activity = false;
}

void StochasticTrackEngine::triggerStep(uint32_t tick, uint32_t divisor) {
    auto &sequence = this->sequence();
    auto &track = stochasticTrack();

    // Routed-CV invalidation. The UI edit path calls notifyStochasticShapingEdit
    // which triggers a refresh, but routing writes the sequence directly per
    // tick with no UI notify. Snapshot the cache-shaping knobs; if any moved
    // since last trigger, flag a refresh. Cheap (int compares per field) and
    // avoids rebuilding every tick.
    {
        uint8_t cur[kShapingKnobCount];
        readShapingKnobs(cur);
        bool anyKnobMoved = !_shapingSnapshotValid;
        for (size_t i = 0; !anyKnobMoved && i < kShapingKnobCount; ++i) {
            if (cur[i] != _shapingSnapshot[i]) anyKnobMoved = true;
        }
        if (anyKnobMoved) {
            _stepCacheRefreshPending = true;
            for (size_t i = 0; i < kShapingKnobCount; ++i) _shapingSnapshot[i] = cur[i];
            _shapingSnapshotValid = true;
        }
    }

    // Coalesce cache refreshes: Live rhythm + melody writes from the previous
    // trigger may have flagged a refresh. Pay at most one rebuild per trigger,
    // before any cache read downstream.
    if (_stepCacheRefreshPending) {
        refreshStepCache();
        _stepCacheRefreshPending = false;
    }

    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());

    // Regenerate Loop sources for any domain marked invalid (e.g. patience
    // just fired, NewR/NewM was pressed).
    ensureLoopSources(scale, rootNote);

    // Snap _currentStep into the active window before reading. If the user
    // shrinks Size mid-cycle past the current playback position, the engine
    // would otherwise play one stale off-window event with stale state.
    // Flush gate/CV queues too — they hold absolute-tick events for steps
    // that are now outside the window, and tick() drains them regardless of
    // _currentStep.
    if (_currentStep < sequence.first() || _currentStep > sequence.last()) {
        _currentStep = sequence.first();
        _gateQueue.clear();
        _cvQueue.clear();
        _gateOutput = false;
        _activity = false;
    }
    int stepIndex = _currentStep;
    if (sequence.rotate() != 0) {
        int windowSize = sequence.last() - sequence.first() + 1;
        if (windowSize > 0) {
            int offset = (_currentStep - sequence.first() + sequence.rotate()) % windowSize;
            if (offset < 0) offset += windowSize;
            stepIndex = sequence.first() + offset;
        }
    }

    float finalCv = 0.f;
    uint32_t durationTicks = divisor;
    bool isRest = false;
    bool isLegato = false;
    bool isSlide = false;

    {
        // Source loop is guaranteed fresh here — ensureLoopSources ran above.
        const auto &event = sequence.steps()[stepIndex];
        StochasticStepContent eval;
        eval.clear();

        // Repeat: per-trigger Bernoulli that replays the previously-emitted
        // event verbatim. Works in both Live and Loop — playback-layer freeze.
        int repeatProb = int(sequence.repeatProb());
        bool useRepeat = (repeatProb > 0 && _lastStepContentValid &&
                          int(_rng.nextRange(100)) < repeatProb);
        if (useRepeat) {
            eval = _lastStepContent;
            // _lastDegree stays as-is — frozen on the previous pitch.
        } else {
            // Keep rhythm and melody writes domain-preserving; direct packed bitfield
            // assignment can clear sibling flags such as rhythmValid.
            if (sequence.rhythmMode() == StochasticSourceMode::Live) {
                auto rhythm = StochasticGenerator::generateRhythmEvent(sequence, track, _rng);
                eval.mergeRhythmFrom(rhythm);
                writeLiveRhythmShadow(stepIndex, rhythm);
            } else {
                eval.mergeRhythmFrom(event);
            }

            if (sequence.melodyMode() == StochasticSourceMode::Live) {
                auto melody = StochasticGenerator::generateMelodyEvent(sequence, track, scale, rootNote, _pitchState, _rng);
                eval.mergeMelodyFrom(melody);
                _lastDegree = _pitchState.lastDegree;
                writeLiveMelodyShadow(stepIndex, melody);
            } else {
                eval.mergeMelodyFrom(event);
                _lastDegree = int(eval.degree()) + int(eval.octave()) * scale.notesPerOctave();
            }

            _lastStepContent = eval;
            _lastStepContentValid = true;
        }

        int activeNotes = scale.notesPerOctave();

        // Playback fields come from the cache cell for the non-Repeat path.
        // Repeat stays on eval (i.e. _lastStepContent) because the audible material
        // is the captured event, not the cell at stepIndex.
        int  pDurIdx = eval.durationIndex();  // repeat path / fallback
        int  pDegree = int(eval.degree());
        int  pOctave = int(eval.octave());
        bool pRest   = bool(eval.rest());
        bool pSlide  = bool(eval.slide());
        bool pLegato = bool(eval.legato());
        uint32_t cellDurFromCache = 0;       // 0 → use legacy durationIndex path
        uint8_t  cellGateFrac     = 32;      // default 50% if no cache
        if (!useRepeat && stepIndex >= 0 && stepIndex < int(_stepCache.count)) {
            const auto &cell = _stepCache.runtimeSteps[stepIndex];
            cellDurFromCache = cell.durationTicks();
            if (cellDurFromCache == 0) cellDurFromCache = 1;
            cellGateFrac = cell.gateLen();
            pDegree = int(cell.degree());
            pOctave = int(cell.octave());
            pRest   = !cell.audible();
            pSlide  = cell.slide();
            pLegato = cell.legato();
            // Legato + Slide are cache-owned. Propagate the cache's picks
            // into _lastStepContent so a subsequent Repeat replays this
            // step's decisions, not whatever stale bits lived on the stored
            // content.
            _lastStepContent.setLegato(pLegato);
            _lastStepContent.setSlide(pSlide);
        }
        // else: cache unprimed or out-of-window → fall back to stored
        // durationIndex via the LUT path below.

        // durationTicks: non-Repeat with primed cache uses the cached value;
        // Repeat / unprimed falls back to a LUT pick from eval.durationIndex().
        uint32_t mult;
        if (cellDurFromCache > 0) {
            mult = cellDurFromCache;
        } else {
            auto frac = getDurationFraction(pDurIdx);
            mult = (uint64_t(divisor) * frac.num) / frac.den;
            if (mult < 1) mult = 1;
        }
        // Feel scale is computed per trigger so a routed Feel CV takes effect
        // immediately without a cache refresh.
        uint32_t feelScaleQ16 = stochastic_cache::computeFeelScaleQ16(
            int(sequence.feel()), uint32_t(_stepCache.cycleTicks), uint32_t(CONFIG_PPQN));
        mult = stochastic_cache::applyFeelScale(mult, feelScaleQ16);
        if (mult < 1) mult = 1;
        _lastDurationIndex = pDurIdx;
        durationTicks = mult;
        _eventDuration = durationTicks;

        int note = pDegree + (pOctave + _jumpRegister + track.octave()) * activeNotes + track.transpose();
        finalCv = scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);

        // MaskR + TiltR — LoopR-only. In LiveR every event stores rank=0
        // (Live bypasses generateMaskRanks), so TiltR's duration axis would
        // collapse to a salt-only cut. Both knobs are inert in LiveR.
        // TiltR is 0..100 with center 50 = pure salt cut; <50 cuts longs,
        // >50 cuts shorts. Signed magnitude reconstructed below.
        bool maskRhythmPass = true;
        if (sequence.rhythmMode() == StochasticSourceMode::Loop && sequence.maskRhythm() < 100) {
            const uint32_t patternSize = std::max<uint32_t>(1, sequence.size());
            const uint32_t denom = patternSize > 1 ? (patternSize - 1) : 1;
            const uint32_t storedRank = (useRepeat ? uint32_t(eval.densityRank())
                                                   : uint32_t(sequence.steps()[stepIndex].densityRank()));
            const uint32_t rankPctMilli = std::min<uint32_t>(1000, (storedRank * 1000) / denom);
            const uint32_t saltHash = keyed_rng::cellSeed(sequence.rhythmSeed(), uint32_t(stepIndex));
            const uint32_t saltPctMilli = ((saltHash >> 24) * 1000) / 255;
            const int tiltSigned = int(sequence.tiltRhythm()) - 50;   // -50..+50
            const uint32_t tiltMag = uint32_t(std::abs(tiltSigned)) * 2;   // 0..100
            const uint32_t effectiveMilli =
                (tiltMag * rankPctMilli + (100 - tiltMag) * saltPctMilli) / 100;
            const uint32_t maskMilli = uint32_t(sequence.maskRhythm()) * 10;
            // knob<50 = low-pass (long notes survive): rank 0 is longest, so
            // pass when effective < mask. knob>50 = high-pass (short survives):
            // pass when (1000-effective) < mask, i.e. high rank = short.
            if (tiltSigned <= 0) {
                maskRhythmPass = effectiveMilli < maskMilli;
            } else {
                maskRhythmPass = (1000 - effectiveMilli) < maskMilli;
            }
        }

        // MaskM + TiltM — LoopM-only. Pitch-centrality filter: each played
        // step's degree gets a centrality score; MaskM thresholds it. TiltM
        // is unipolar inversion magnitude — 0 keeps high-centrality (tonal),
        // 100 keeps low-centrality (anti-tonal). LiveM bypasses (inert).
        bool maskMelodyPass = true;
        if (sequence.melodyMode() == StochasticSourceMode::Loop && sequence.maskMelody() < 100) {
            const int N = activeNotes > 0 ? activeNotes : 1;
            int degInOct = pDegree % N;
            if (degInOct < 0) degInOct += N;
            const uint32_t centralityMilli = std::min<uint32_t>(1000,
                uint32_t(stochasticPitchCentrality(degInOct, N)) * 1000u /
                uint32_t(kStochasticPitchCentralityMax));
            const uint32_t tiltMag = uint32_t(sequence.tiltMelody());   // 0..100
            const uint32_t effectiveMilli =
                ((100 - tiltMag) * centralityMilli + tiltMag * (1000 - centralityMilli)) / 100;
            const uint32_t maskMilli = uint32_t(sequence.maskMelody()) * 10;
            maskMelodyPass = effectiveMilli >= (1000 - maskMilli);
        }

        isRest = pRest || !maskRhythmPass || !maskMelodyPass || !eval.rhythmValid() || !eval.melodyValid();
        isLegato = pLegato;
        isSlide = pSlide;

        // Non-Repeat bursts: cluster cells are normal steps with their own
        // duration; they play through the normal triggerStep loop, no extra
        // child-walk. Only Repeat needs the child-array path because it
        // replays a captured _lastStepContent rather than the step.
        StochasticGenerator::EvaluatedBurstNote evalBursts[kMaxBurst];
        for (int i = 0; i < kMaxBurst; ++i) evalBursts[i].valid = false;
        uint8_t clusterTailCount = 0;

        if (useRepeat && !isRest) {
            StochasticGenerator::evaluateBurst(evalBursts, sequence, eval, track, scale, rootNote, note, durationTicks, _rng);
            for (int i = 0; i < kMaxBurst; ++i) if (evalBursts[i].valid) clusterTailCount++;
        }

        recordDirectHistory(finalCv, isRest, !isRest, clusterTailCount);

        if (!isRest) {
            // DBG_STO("%u CVpush ON p=%d cv=%.3f", tick, _currentStep, finalCv);
            _cvQueue.push({ tick, finalCv, isSlide });
            _gateQueue.push({ tick, true });

            if (!isLegato && clusterTailCount == 0) {
                // Non-Repeat reads the cache-baked gate fraction. Repeat
                // uses pickGateLength at runtime because _lastStepContent carries
                // no cache-baked gate fraction.
                uint32_t gateLen;
                if (useRepeat) {
                    gateLen = pickGateLength(durationTicks, sequence.gateLength(), _rng);
                } else {
                    gateLen = (durationTicks * uint32_t(cellGateFrac)) / 64u;
                    if (gateLen < kMinAudibleGateTicks) gateLen = kMinAudibleGateTicks;
                    if (gateLen > durationTicks) gateLen = durationTicks;
                }
                _gateQueue.push({ tick + gateLen, false });
            }
            _activity = true;

            if (useRepeat) {
                // Legacy burst playback for Repeat — bursts come from
                // _lastStepContent (Repeat replays the captured event, not the
                // window). The flat-cell cache path doesn't apply here.
                for (int i = 0; i < kMaxBurst; ++i) {
                    if (evalBursts[i].valid) {
                        uint32_t childTick = tick + evalBursts[i].tickOffset;
                        uint32_t lowTick = childTick > tick + 2 ? childTick - 2 : tick;

                        int childNote = evalBursts[i].note;
                        if (burstHoldIsRoll(sequence.burstHold())) {
                            childNote += (_jumpRegister + track.octave()) * activeNotes + track.transpose();
                        }
                        float childCv = scale.noteToVolts(childNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);

                        _gateQueue.push({ lowTick, false });
                        _cvQueue.push({ childTick, childCv, isSlide });
                        _gateQueue.push({ childTick, true });
                        _gateQueue.push({ childTick + evalBursts[i].gateTicks, false });
                    }
                }
            }
            // Phase 16 (2026-05-23): non-Repeat burst playback is implicit —
            // cluster cells are normal event steps with their own duration;
            // each gets a triggerStep call. No cache-walk needed here.
        } else {
            // DBG_STO("%u CVpush REST p=%d cv=%.3f mode=%s", tick, _currentStep, finalCv,
            //     track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always ? "Always" : "Gate");
            _gateQueue.push({ tick, false });
            if (track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always) _cvQueue.push({ tick, finalCv, false });
            _activity = false;
        }
    }

    _currentStep++;
    if (_currentStep > sequence.last()) {
        _currentStep = sequence.first();
        _patternCycleEnded = true;

        // Jump / Sleep / Patience / Mutate all advance on the same boundary —
        // bundled into rollCycleEndHooks() so reset-measure preempt and
        // natural pattern wrap stay in lockstep (2026-05-24).
        rollCycleEndHooks();

        // Size edit → extend the cache walk to cover newly-active steps.
        // Step-keyed cache (2026-05-24): cells are keyed by step index, so
        // First does NOT alter cell content and does not force a rebuild —
        // moving the window plays the same steps in their same (cluster-aware)
        // shape, just from a different starting point.
        if (uint8_t(sequence.size()) != _lastAppliedSize) {
            refreshStepCache();
            _lastAppliedSize  = uint8_t(sequence.size());
            _lastAppliedFirst = uint8_t(sequence.first());
        }
    }
}

void StochasticTrackEngine::rollPatience() {
    auto &sequence = this->sequence();
    // Knob 100 is the off-sentinel: don't advance the counter and don't roll.
    // Counter stays at 0 so the UI tension meter sits at empty; flipping the
    // knob below 100 starts fresh (counter from 0) rather than picking up
    // accumulated time. Restored 2026-05-22 — Lock feature hasn't shipped to
    // own the "never regenerate" semantics.
    int patR = int(sequence.patienceRhythm());
    if (sequence.rhythmMode() == StochasticSourceMode::Loop && patR < 100) {
        _loopCycleCount++;
        float pR = StochasticTrackEngine::patienceProbability(_loopCycleCount, patR);
        if (_rng.nextRange(10000) < uint32_t(pR * 10000.0f)) {
            sequence.setRhythmValid(false);
            _loopCycleCount = 0;
        }
    } else if (patR >= 100) {
        _loopCycleCount = 0;
    }
    int patM = int(sequence.patienceMelody());
    if (sequence.melodyMode() == StochasticSourceMode::Loop && patM < 100) {
        _loopCycleCountMelody++;
        float pM = StochasticTrackEngine::patienceProbability(_loopCycleCountMelody, patM);
        if (_rng.nextRange(10000) < uint32_t(pM * 10000.0f)) {
            sequence.setMelodyValid(false);
            _loopCycleCountMelody = 0;
        }
    } else if (patM >= 100) {
        _loopCycleCountMelody = 0;
    }
}

void StochasticTrackEngine::rollCycleEndHooks() {
    auto &sequence = this->sequence();
    auto &track = stochasticTrack();
    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());

    // Proteus-style octave walk — random ±1 each fire, reflected at the
    // ±kJumpMaxRange bounds. Non-destructive: _jumpRegister is applied as a
    // playback offset only; stored events are never modified.
    if (sequence.jump() > 0 && int(_rng.nextRange(100)) < sequence.jump()) {
        static const int kJumpMaxRange = 2;   // ±2 octaves
        int direction = (_rng.nextRange(2) == 0) ? -1 : +1;
        if (_jumpRegister + direction > kJumpMaxRange) direction = -1;
        else if (_jumpRegister + direction < -kJumpMaxRange) direction = +1;
        _jumpRegister += direction;
    }

    // Round (+5) instead of floor so knob=2 already produces 1 sleep event
    // instead of the dead 0..2 zone.
    if (sequence.sleep() > 0) _sleepRemaining = (sequence.sleep() * 4 + 5) / 10;

    // Patience first — if it invalidates a Loop domain, the next triggerStep
    // regenerates that domain, overwriting any mutation we'd write below.
    rollPatience();

    // Destructive mutation. Mutate knob is probability 0..100 of one event
    // inside [first..last] being re-rolled per cycle-end. Domain chosen at
    // random when both are Loop and valid.
    const int mutateAmount = sequence.mutate();
    if (mutateAmount > 0 && int(_rng.nextRange(100)) < mutateAmount) {
        bool rhythmEligible = (sequence.rhythmMode() == StochasticSourceMode::Loop && sequence.rhythmValid());
        bool melodyEligible = (sequence.melodyMode() == StochasticSourceMode::Loop && sequence.melodyValid());
        auto applyRhythm = [&] {
            StochasticGenerator::mutateRhythmOne(sequence, track, _rng);
        };
        auto applyMelody = [&] {
            StochasticGenerator::mutateMelodyOne(sequence, track, scale, rootNote, _rng);
        };
        if (rhythmEligible && melodyEligible) {
            if (_rng.nextRange(2) == 0) applyRhythm();
            else                         applyMelody();
        } else if (rhythmEligible) {
            applyRhythm();
        } else if (melodyEligible) {
            applyMelody();
        }
        refreshStepCache();
    }
}

void StochasticTrackEngine::renewRhythm() {
    auto &seq = sequence();
    auto &trk = stochasticTrack();
    StochasticGenerator::generateRhythm(seq, trk, _rng.next());
    seq.setRhythmValid(true);
    _loopCycleCount = 0;
    refreshStepCache();
}

void StochasticTrackEngine::renewMelody() {
    auto &seq = sequence();
    auto &trk = stochasticTrack();
    const auto &scale = seq.selectedScale(_model.project().scale());
    int rootNote = seq.selectedRootNote(_model.project().rootNote());
    StochasticGenerator::generateMelody(seq, trk, scale, rootNote, _rng.next());
    seq.setMelodyValid(true);
    _loopCycleCountMelody = 0;
    // Fresh melody = no inherited octave drift (2026-05-24).
    _jumpRegister = 0;
    refreshStepCache();
}

void StochasticTrackEngine::refreshLoopSources() {
    auto &seq = sequence();
    auto &trk = stochasticTrack();
    const auto &scale = seq.selectedScale(_model.project().scale());
    int rootNote = seq.selectedRootNote(_model.project().rootNote());

    // Regenerate both stored event domains. Loop plays the new pattern; Live
    // keeps generating per-trigger but the events-array display reflects the
    // fresh content. valid=true keeps the engine's auto-regen path quiet on
    // the next tick.
    StochasticGenerator::generateRhythm(seq, trk, _rng.next());
    StochasticGenerator::generateMelody(seq, trk, scale, rootNote, _rng.next());
    seq.setRhythmValid(true);
    seq.setMelodyValid(true);
    _loopCycleCount = 0;
    _loopCycleCountMelody = 0;
    // Both domains rewritten = fresh material; clear inherited octave drift.
    _jumpRegister = 0;
    refreshStepCache();
}

void StochasticTrackEngine::update(float dt) {
    const auto &trk = stochasticTrack();
    if (_slideActive && trk.slideTime() > 0) {
        float slideDt = dt * 1000.f / (float(trk.slideTime()) + 1.f);
        _cvOutput += (_cvOutputTarget - _cvOutput) * std::min(1.f, slideDt);
    } else {
        _cvOutput = _cvOutputTarget;
    }
}

// ============================================================================
// Patch 2 — Helper bodies. No behavior change vs the inlined originals.
// ============================================================================

void StochasticTrackEngine::ensureLoopSources(const Scale &scale, int rootNote) {
    auto &seq = sequence();
    auto &trk = stochasticTrack();
    bool regenerated = false;
    // Same seeds, same order, same valid semantics as the previously-inlined
    // block at the top of triggerStep's non-locked branch.
    if (seq.rhythmMode() == StochasticSourceMode::Loop && !seq.rhythmValid()) {
        StochasticGenerator::generateRhythm(seq, trk, _rng.next());
        regenerated = true;
    }
    if (seq.melodyMode() == StochasticSourceMode::Loop && !seq.melodyValid()) {
        StochasticGenerator::generateMelody(seq, trk, scale, rootNote, _rng.next());
        // Patience invalidated melody = fresh material; clear octave drift.
        _jumpRegister = 0;
        regenerated = true;
    }
    if (regenerated) refreshStepCache();
}

void StochasticTrackEngine::writeLiveRhythmShadow(int stepIndex, const StochasticStepContent &rhythm) {
    // Live writeback into the events array so the events display reflects
    // the recently played event. Flag a coalesced cache refresh so we only
    // rebuild once even if both rhythm and melody write in the same trigger.
    sequence().steps()[stepIndex].mergeRhythmFrom(rhythm);
    _stepCacheRefreshPending = true;
}

void StochasticTrackEngine::writeLiveMelodyShadow(int stepIndex, const StochasticStepContent &melody) {
    sequence().steps()[stepIndex].mergeMelodyFrom(melody);
    _stepCacheRefreshPending = true;
}

void StochasticTrackEngine::syncWindowEdit() {
    // Called from UI edit paths after setFirst / setLast / setSize on this
    // track's sequence. Brings engine playback state in line with the new
    // window NOW — without waiting for the next event-boundary trigger.
    //
    // Without this, queued gate/CV events (with absolute ticks for steps that
    // may be outside the new window) and an in-flight _eventDuration (set
    // when the old window was active) keep playback honoring the old
    // boundaries until the current event finishes.
    auto &seq = sequence();
    int first = seq.first();
    int last  = seq.last();
    if (last < first) last = first;

    // Snap _currentStep into the new window if it's now outside.
    if (_currentStep < first || _currentStep > last) {
        _currentStep = first;
        // Force the next tick to trigger a step (don't wait for the now-stale
        // _eventDuration to elapse — its remaining ticks belonged to the old
        // window's event).
        _eventElapsed = _eventDuration;
    }

    // Queued events were scheduled at absolute ticks for steps that may not
    // exist in the new window. Drop them all; force gate off so the user
    // doesn't hear a stuck gate from the old window.
    _gateQueue.clear();
    _cvQueue.clear();
    _gateOutput = false;
    _activity = false;

    // Cache rank ordering was built for the old window — windowSize feeds the
    // mask threshold and the rank distribution was assigned over the old
    // window's step set. Rebuild for the new window.
    refreshStepCache();
}

void StochasticTrackEngine::refreshStepCache() {
    auto &seq = sequence();
    auto &trk = stochasticTrack();
    uint32_t divisor = effectiveDivisor();
    // Scale + rootNote are passed so the cache can bake melody-domain picks
    // (anchor pitches in Loop melody, cluster-tail Generate-mode pitches).
    const auto &scale = seq.selectedScale(_model.project().scale());
    int rootNote = seq.selectedRootNote(_model.project().rootNote());
    stochastic_cache::rebuildStepCache(
        _stepCache, seq, divisor, seq.rhythmSeed(), &scale, &trk, rootNote);
}

uint32_t StochasticTrackEngine::effectiveDivisor() const {
    const auto &seq = sequence();
    float clockMult = seq.clockMultiplier() * 0.01f;
    if (clockMult <= 0.f) clockMult = 1.f;
    uint32_t divisor = uint32_t(seq.divisor()) * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    return std::max<uint32_t>(1u, uint32_t(std::lround(divisor / clockMult)));
}

void StochasticTrackEngine::readShapingKnobs(uint8_t out[kShapingKnobCount]) const {
    const auto &seq = sequence();
    out[size_t(ShapingKnob::NoteDuration)]  = uint8_t(seq.noteDuration());
    out[size_t(ShapingKnob::Variation)]     = uint8_t(seq.variation());
    out[size_t(ShapingKnob::Burst)]         = uint8_t(seq.burst());
    out[size_t(ShapingKnob::GateLength)]    = uint8_t(seq.gateLength());
    out[size_t(ShapingKnob::Complexity)]    = uint8_t(seq.complexity());
    out[size_t(ShapingKnob::Contour)]       = uint8_t(int8_t(seq.contour()));
    out[size_t(ShapingKnob::MarblesBias)]   = uint8_t(seq.marblesBias());
    out[size_t(ShapingKnob::MarblesSpread)] = uint8_t(seq.marblesSpread());
    out[size_t(ShapingKnob::BurstCount)]    = uint8_t(seq.burstCount());
    out[size_t(ShapingKnob::BurstRate)]     = uint8_t(seq.burstRate());
    out[size_t(ShapingKnob::BurstHold)]     = uint8_t(seq.burstHold());
    out[size_t(ShapingKnob::Range)]         = uint8_t(seq.range());
}

void StochasticTrackEngine::clearDirectHistory() {
    int trackIndex = directHistoryTrackIndex(_track);
    gDirectHistoryCount[trackIndex] = 0;
    for (int i = 0; i < kDirectHistoryMax; ++i) {
        gDirectHistory[trackIndex][i] = {};
    }
}

void StochasticTrackEngine::recordDirectHistory(float cv, bool rest, bool gate, uint8_t children) {
    int trackIndex = directHistoryTrackIndex(_track);
    for (int i = kDirectHistoryMax - 1; i > 0; --i) {
        gDirectHistory[trackIndex][i] = gDirectHistory[trackIndex][i - 1];
    }
    if (children > kMaxBurst) children = kMaxBurst;
    int cvSixteenths = int(cv * 16.f);
    if (cvSixteenths < -128) cvSixteenths = -128;
    if (cvSixteenths > 127) cvSixteenths = 127;
    gDirectHistory[trackIndex][0].cvSixteenths = int8_t(cvSixteenths);
    gDirectHistory[trackIndex][0].children = children;
    gDirectHistory[trackIndex][0].flags = (rest ? 1 : 0) | (gate ? 2 : 0);
    if (gDirectHistoryCount[trackIndex] < kDirectHistoryMax) gDirectHistoryCount[trackIndex]++;
}
