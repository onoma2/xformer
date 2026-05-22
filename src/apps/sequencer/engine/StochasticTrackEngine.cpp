#include "StochasticTrackEngine.h"
#include "StochasticGenerator.h"
#include "StochasticCache.h"
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

// Phase 14B engine cache lives inside StochasticTrackEngine as a member
// (`_cache`). The previous file-scope `gStochasticCaches[8]` was 3.6 KB of
// CCMRAM that paid 8× the per-track cost regardless of which tracks were
// stochastic. Folding it into the engine costs only the per-slot growth on
// the Container<...> slot size (~56 B × 8 = 448 B) since the container is
// already sized to its largest member (per PROJECT.md "Engine gate").
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

// Knob 0..99 → λ ∈ [1, ~79] linear; knob 100 is the off-sentinel ("never
// regenerate"). Phase 12 had dropped this sentinel in favor of routing the
// "never" case through the deferred Lock feature, but Lock hasn't shipped
// yet, so without the sentinel users had no way to disable patience-driven
// regen. Restored 2026-05-22 (hardware feedback on feat/stochastic-seed-log).
static inline float patienceLambda(int patience) {
    int p = patience < 0 ? 0 : (patience > 100 ? 100 : patience);
    return 1.0f + (p * 79.0f) / 100.0f;
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
static const uint32_t kMinAudibleGateTicks = 6;   // ~16 ms @ 192 PPQN, 120 BPM

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
    // Phase 12: count the reset-measure boundary toward patience ONLY when it
    // preempts a natural wrap that hasn't already fired. Natural wraps run
    // before reset boundaries in normal playback (often one tick apart), so
    // unconditional roll here would double-count and accelerate patience.
    // If `_patternIndex` is already `first()`, the natural wrap just ran and
    // already rolled — skip. If it's mid-cycle, reset is truncating an unfired
    // cycle — count it.
    bool preempted = (_patternIndex != seq.first());
    _patternIndex = seq.first();
    _relativeTick = 0;
    _eventElapsed = 0;
    _eventDuration = 0;
    if (preempted) {
        rollPatience();
    }
}

void StochasticTrackEngine::reset() {
    _patternIndex = stochasticTrack().sequence(pattern()).first();
    _relativeTick = 0;
    _sleepRemaining = 0;
    _loopCycleCount = 0;
    _loopCycleCountMelody = 0;
    _lastAppliedTilt = 0;
    _lastAppliedSize = 0;
    _lastAppliedFirst = 0;
    _lastAppliedLast = 0;
    _lastDegree = -1;
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
    _lastEventValid = false;
    clearDirectHistory();
    _gateQueue.clear();
    _cvQueue.clear();
    _rng = Random(0x12345678 + _track.trackIndex());
    changePattern();
}

void StochasticTrackEngine::restart() {
    _patternIndex = stochasticTrack().sequence(pattern()).first();
    _relativeTick = 0;
    _lastDegree = -1;
    _lastFreeStepIndex = -1;
    _eventElapsed = 0;
    _eventDuration = 0;
    _lastEventValid = false;
    clearDirectHistory();
    _rng = Random(0x12345678 + _track.trackIndex());
}

TrackEngine::TickResult StochasticTrackEngine::tick(uint32_t tick) {
    ASSERT(_sequence != nullptr, "invalid sequence");
    const auto &sequence = this->sequence();

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

    // Event-driven step advancement. The current event's durationTicks (set in
    // triggerStep, from the LUT slot × divisor) controls when the NEXT event
    // fires. Same path for Duration-Tickets mode and Live/Loop mode — only the
    // duration source differs (weighted ticket pick vs noteDuration+variation).
    _eventElapsed++;
    if (_eventDuration == 0 || _eventElapsed >= _eventDuration) {
        _eventElapsed = 0;
        // Phase 11: tickets always combine — sleep gate no longer mode-dependent.
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

    const auto &scale = sequence.selectedScale(_model.project().scale());
    int rootNote = sequence.selectedRootNote(_model.project().rootNote());

    // Patch 2: Loop source-tape regen if either domain is marked invalid.
    // Same seeds, same order, same valid semantics as the inlined block was.
    ensureLoopSources(scale, rootNote);

    // Phase 12 fix + 2026-05-22 follow-up: snap _patternIndex into the active
    // window before reading. If the user shrinks Last or raises First while
    // playback is past the new window, the engine would otherwise play one
    // stale off-window event with stale rank/mutation state.
    //
    // The snap also has to flush the gate/CV queues. Previous triggerSteps
    // pushed events with absolute ticks for indices that are now outside the
    // window (e.g. user shrinks last from 31 → 10 mid-cycle with queued events
    // for slot 25, 31). `tick()` drains those by absolute tick regardless of
    // _patternIndex, so without flushing the user would still hear slot 31's
    // gate/CV fire after the snap repositioned playback to slot 0. Same
    // discipline as changePattern() — clear queues, force gate low.
    if (_patternIndex < sequence.first() || _patternIndex > sequence.last()) {
        _patternIndex = sequence.first();
        _gateQueue.clear();
        _cvQueue.clear();
        _gateOutput = false;
        _activity = false;
    }
    int readIndex = _patternIndex;
    if (sequence.rotate() != 0) {
        int windowSize = sequence.last() - sequence.first() + 1;
        if (windowSize > 0) {
            int offset = (_patternIndex - sequence.first() + sequence.rotate()) % windowSize;
            if (offset < 0) offset += windowSize;
            readIndex = sequence.first() + offset;
        }
    }

    // Lock evaluated-cache replay path removed 2026-05-22 (crash on 2+ tracks
    // due to heap-stack collision; see PROJECT.md heap section and
    // .tasks/stochastic-track-port/LOCK-DESIGN-DEFERRED.md). The model-side
    // `track.lock()` flag is intentionally not consulted here — Lock is a UI
    // placeholder until the new design lands.
    float finalCv = 0.f;
    uint32_t durationTicks = divisor;
    bool isRest = false;
    bool isLegato = false;
    bool isSlide = false;

    {
        // Source loop tape is guaranteed fresh at this point — ensureLoopSources
        // at the top of triggerStep handled any invalid Loop domain regen.
        const auto &event = sequence.events()[readIndex];
        StochasticSourceEvent eval;
        eval.clear();

        // Phase 12 Repeat: per-event Bernoulli that bypasses generation and
        // reuses the previously-emitted event verbatim. Works in both Live and
        // Loop mode — freezes/clusters at the playback layer.
        int repeatProb = int(sequence.repeatProb());
        bool useRepeat = (repeatProb > 0 && _lastEventValid &&
                          int(_rng.nextRange(100)) < repeatProb);
        if (useRepeat) {
            eval = _lastEvent;
            // _lastDegree stays as-is — frozen on the previous pitch.
        } else {
            // Keep rhythm and melody writes domain-preserving; direct packed bitfield
            // assignment can clear sibling flags such as rhythmValid.
            if (sequence.rhythmMode() == StochasticSourceMode::Live) {
                auto rhythm = StochasticGenerator::generateRhythmEvent(sequence, track, _rng);
                eval.mergeRhythmFrom(rhythm);
                writeLiveRhythmShadow(readIndex, rhythm);
            } else {
                eval.mergeRhythmFrom(event);
            }

            if (sequence.melodyMode() == StochasticSourceMode::Live) {
                auto melody = StochasticGenerator::generateMelodyEvent(sequence, track, scale, rootNote, _lastDegree, _rng);
                eval.mergeMelodyFrom(melody);
                _lastDegree = int(eval.degree()) + int(eval.octave()) * scale.notesPerOctave();
                writeLiveMelodyShadow(readIndex, melody);
            } else {
                eval.mergeMelodyFrom(event);
                _lastDegree = int(eval.degree()) + int(eval.octave()) * scale.notesPerOctave();
            }

            _lastEvent = eval;
            _lastEventValid = true;
        }

        int activeNotes = scale.notesPerOctave();

        // Phase 14B Patch C step 2 (2026-05-22): parent-cell playback fields
        // (durationIndex, degree, octave, rest, slide, legato) sourced from
        // the engine cache when not under Repeat. The cache is built from the
        // event tape and tracked across all event-writes (refreshCache), so
        // these reads are byte-equivalent to the legacy eval-tape reads —
        // just routed through the deterministic cache. Repeat path stays on
        // eval (i.e. _lastEvent) because the audible material is the
        // repeated event, not the cell at readIndex (same discipline as the
        // mask-rank fix).
        int  pDurIdx = eval.durationIndex();
        int  pDegree = int(eval.degree());
        int  pOctave = int(eval.octave());
        bool pRest   = bool(eval.rest());
        bool pSlide  = bool(eval.slide());
        bool pLegato = bool(eval.legato());
        if (!useRepeat) {
            uint8_t cIdx = (readIndex >= 0 && readIndex < int(stochastic_cache::kMaxEventSlots))
                         ? _cache.parentCacheIdx[readIndex]
                         : uint8_t(0xff);
            if (cIdx < stochastic_cache::kCellCap) {
                const auto &cell = _cache.cells[cIdx];
                const auto &aux  = _cache.aux[cIdx];
                pDurIdx = int(cell.gateLen());  // cell.gateLen field holds durSlot (encodeGateLenAsDurSlot)
                pDegree = int(cell.degree());
                pOctave = int(cell.octave());
                pRest   = !aux.audible();
                pSlide  = cell.slide();
                pLegato = cell.legato();
            }
            // else: cache unprimed (engine reset, no refresh yet) — fall back
            // to event-tape values already loaded above.
        }

        // Duration LUT is divisor-relative: ticks = (divisor * num) / den.
        // Variation is applied at generation time in generateRhythmEvent — each
        // event's durationIndex is already varied when stored, so we just
        // read the (possibly cache-sourced) durationIndex directly.
        auto frac = getDurationFraction(pDurIdx);
        uint32_t mult = (uint64_t(divisor) * frac.num) / frac.den;
        if (mult < 1) mult = 1;
        _lastDurationIndex = pDurIdx;
        durationTicks = mult;
        _eventDuration = durationTicks;

        int note = pDegree + (pOctave + _jumpRegister + track.octave()) * activeNotes + track.transpose();
        finalCv = scale.noteToVolts(note) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);

        // V5 Mask: deterministic playback thinning after source read/evaluation.
        // Phase 12: ranks are assigned over the active window [first..last]
        // (0..windowSize-1), so the mask threshold must use windowSize, not
        // sequence.size(), to keep "% audible plays" semantics correct.
        //
        // Phase 14B Patch B: rank source moved from event.densityRank() (which
        // mixed in random noise) to the engine cache's deterministic rank,
        // derived from keyed RNG salt + tilt. The cache is rebuilt after every
        // generator write so this lookup stays in sync with the audible content.
        // parentCacheIdx maps event-slot index -> cache cell index in O(1).
        //
        // Codex review 2026-05-22 finding: Repeat replays _lastEvent, so the
        // audible material is no longer the event at readIndex. The cache
        // rank for readIndex would filter unrelated content. _lastEvent
        // carries its own densityRank (copied at capture time), so when
        // useRepeat is true the legacy rank field is the right source — it
        // travels with the repeated event. This is the one place where the
        // cache-as-rank-source path has to defer to the event's own rank.
        uint32_t windowSize = std::max(1, int(sequence.last()) - int(sequence.first()) + 1);
        uint32_t cacheRank = stochastic_cache::selectMaskRank(
            useRepeat, eval.densityRank(), readIndex, _cache);
        bool maskPass = sequence.mask() >= 100 ||
            ((cacheRank * 100) < (uint32_t(sequence.mask()) * windowSize));
        isRest = pRest || !maskPass || !eval.rhythmValid() || !eval.melodyValid();
        isLegato = pLegato;
        isSlide = pSlide;

        // Evaluate Children
        StochasticGenerator::EvaluatedChild evalChildren[kMaxChildren];
        if (!isRest) {
            StochasticGenerator::evaluateChildren(evalChildren, sequence, eval, track, scale, rootNote, note, durationTicks, _rng);
        } else {
            for (int i = 0; i < kMaxChildren; ++i) evalChildren[i].valid = false;
        }

        // captureLockedParent removed 2026-05-22 — lock cache deferred.
        uint8_t childCount = 0;
        for (int i = 0; i < kMaxChildren; ++i) if (evalChildren[i].valid) childCount++;
        recordDirectHistory(finalCv, isRest, !isRest, childCount);

        if (!isRest) {
            // DBG_STO("%u CVpush ON p=%d cv=%.3f", tick, _patternIndex, finalCv);
            _cvQueue.push({ tick, finalCv, isSlide });
            _gateQueue.push({ tick, true });

            if (!isLegato && childCount == 0) {
                uint32_t gateLen = pickGateLength(durationTicks, sequence.gateLength(), _rng);
                // DBG_STO("%u GATEpush LIVE OFF p=%d", tick + gateLen, _patternIndex);
                _gateQueue.push({ tick + gateLen, false });
            }
            _activity = true;

            for (int i = 0; i < kMaxChildren; ++i) {
                if (evalChildren[i].valid) {
                    uint32_t childTick = tick + evalChildren[i].tickOffset;
                    uint32_t lowTick = childTick > tick + 2 ? childTick - 2 : tick;

                    int childNote = evalChildren[i].note;
                    if (sequence.burstPitch() == StochasticBurstPitch::Generate) {
                        childNote += (_jumpRegister + track.octave()) * activeNotes + track.transpose();
                    }
                    float childCv = scale.noteToVolts(childNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);

                    _gateQueue.push({ lowTick, false });
                    _cvQueue.push({ childTick, childCv, isSlide });
                    _gateQueue.push({ childTick, true });
                    _gateQueue.push({ childTick + evalChildren[i].gateTicks, false });
                    // DBG_STO("%u GATEpush LIVE child on=%u off=%u", tick, childTick, childTick + evalChildren[i].gateTicks);
                }
            }
        } else {
            // DBG_STO("%u CVpush REST p=%d cv=%.3f mode=%s", tick, _patternIndex, finalCv,
            //     track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always ? "Always" : "Gate");
            _gateQueue.push({ tick, false });
            if (track.cvUpdateMode() == StochasticTrack::CvUpdateMode::Always) _cvQueue.push({ tick, finalCv, false });
            _activity = false;
        }
    }

    _patternIndex++;
    if (_patternIndex > sequence.last()) {
        _patternIndex = sequence.first();
        _patternCycleEnded = true;

        {
            // Lock cache removed 2026-05-22 — `lockActive` gate dropped; the
            // jump/sleep/patience/mutate cycle-end hooks always run.
            // Proteus-style octave walk — random ±1 each fire, reflected at the
            // ±kJumpMaxRange bounds. Non-destructive: _jumpRegister is applied
            // as a playback offset only; stored events are never modified, so
            // turning jump off snaps the sequence back to its captured pitches.
            if (sequence.jump() > 0 && int(_rng.nextRange(100)) < sequence.jump()) {
                static const int kJumpMaxRange = 2;   // ±2 octaves
                int direction = (_rng.nextRange(2) == 0) ? -1 : +1;
                if (_jumpRegister + direction > kJumpMaxRange) direction = -1;
                else if (_jumpRegister + direction < -kJumpMaxRange) direction = +1;
                _jumpRegister += direction;
            }

            // Phase 12 musicality fix: round (+5) instead of floor so knob=2
            // already produces 1 sleep event instead of the dead 0..2 zone.
            if (sequence.sleep() > 0) _sleepRemaining = (sequence.sleep() * 4 + 5) / 10;

            // Phase 12 fix: roll patience BEFORE mutation. If patience invalidates
            // a Loop domain, the next triggerStep will regenerate the whole event
            // buffer for that domain — overwriting any mutation we'd just write.
            // Doing patience first lets the mutation skip a domain that's about
            // to be regenerated, avoiding wasted writes that audio never observes.
            // Also now counts the reset-measure boundary (see resetMeasure()) toward
            // the same per-domain counters.
            rollPatience();

            // Domain-aware bipolar mutation. Sign selects algorithm:
            //   mutate > 0 → Marbles permutation (swap two existing events)
            //   mutate < 0 → Proteus destructive (regenerate one event)
            //   mutate = 0 → lock (no mutation)
            // Magnitude is the per-loop probability.
            // Phase 12: only apply to domains that are still valid after the
            // patience roll above; an invalidated domain is about to be wiped.
            int mutateAmount = sequence.mutate();
            int mutateMag = mutateAmount < 0 ? -mutateAmount : mutateAmount;
            if (mutateMag > 0 && int(_rng.nextRange(100)) < mutateMag) {
                bool destructive = (mutateAmount < 0);
                bool rhythmEligible = (sequence.rhythmMode() == StochasticSourceMode::Loop && sequence.rhythmValid());
                bool melodyEligible = (sequence.melodyMode() == StochasticSourceMode::Loop && sequence.melodyValid());
                auto applyRhythm = [&] {
                    if (destructive) StochasticGenerator::mutateRhythmOne(sequence, track, _rng, mutateMag);
                    else             StochasticGenerator::permuteRhythmOne(sequence, _rng);
                };
                auto applyMelody = [&] {
                    if (destructive) StochasticGenerator::mutateMelodyOne(sequence, track, scale, rootNote, _rng, mutateMag);
                    else             StochasticGenerator::permuteMelodyOne(sequence, _rng);
                };
                if (rhythmEligible && melodyEligible) {
                    if (_rng.nextRange(2) == 0) applyRhythm();
                    else                         applyMelody();
                } else if (rhythmEligible) {
                    applyRhythm();
                } else if (melodyEligible) {
                    applyMelody();
                }
                // Mutation just wrote into events; sync cache so the next
                // cycle's mask filter sees the new content (Codex adversarial
                // review finding #1, 2026-05-22).
                refreshCache();
            }

            // Phase 12 Mask+Tilt: re-rank on Tilt / Size / First / Last change.
            // Ranks are now window-local, so any change to the active playback
            // window invalidates them. Content untouched.
            if (int8_t(sequence.tilt()) != _lastAppliedTilt ||
                uint8_t(sequence.size()) != _lastAppliedSize ||
                uint8_t(sequence.first()) != _lastAppliedFirst ||
                uint8_t(sequence.last())  != _lastAppliedLast) {
                if (sequence.rhythmMode() == StochasticSourceMode::Loop) {
                    StochasticGenerator::generateMaskRanks(sequence, sequence.size(),
                        sequence.tilt(), sequence.rhythmSeed() ^ 0xdeadbeef);
                }
                // Cache rank also depends on tilt + window; the legacy refresh
                // above only updates event.densityRank() (used as fallback).
                // refreshCache() rebuilds cells from the (possibly resized)
                // window and re-applies the live tilt to per-cell ranks.
                refreshCache();
                _lastAppliedTilt  = int8_t(sequence.tilt());
                _lastAppliedSize  = uint8_t(sequence.size());
                _lastAppliedFirst = uint8_t(sequence.first());
                _lastAppliedLast  = uint8_t(sequence.last());
            }
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

void StochasticTrackEngine::renewRhythm() {
    auto &seq = sequence();
    auto &trk = stochasticTrack();
    StochasticGenerator::generateRhythm(seq, trk, _rng.next());
    seq.setRhythmValid(true);
    _loopCycleCount = 0;
    refreshCache();
}

void StochasticTrackEngine::renewMelody() {
    auto &seq = sequence();
    auto &trk = stochasticTrack();
    const auto &scale = seq.selectedScale(_model.project().scale());
    int rootNote = seq.selectedRootNote(_model.project().rootNote());
    StochasticGenerator::generateMelody(seq, trk, scale, rootNote, _rng.next());
    seq.setMelodyValid(true);
    _loopCycleCountMelody = 0;
    refreshCache();
}

void StochasticTrackEngine::refreshLoopSources() {
    auto &seq = sequence();
    auto &trk = stochasticTrack();
    const auto &scale = seq.selectedScale(_model.project().scale());
    int rootNote = seq.selectedRootNote(_model.project().rootNote());

    // Always regenerate the stored event buffer, regardless of source mode.
    // Loop mode will play the new pattern. Live mode keeps generating per-tick
    // but the loop tape display now reflects the fresh content too. Setting
    // valid=true prevents the engine's own re-roll path from firing on the
    // very next tick.
    StochasticGenerator::generateRhythm(seq, trk, _rng.next());
    StochasticGenerator::generateMelody(seq, trk, scale, rootNote, _rng.next());
    seq.setRhythmValid(true);
    seq.setMelodyValid(true);
    _loopCycleCount = 0;
    _loopCycleCountMelody = 0;
    refreshCache();
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
        regenerated = true;
    }
    if (regenerated) refreshCache();
}

void StochasticTrackEngine::writeLiveRhythmShadow(int readIndex, const StochasticSourceEvent &rhythm) {
    // Live-mode writeback into the source loop tape so the loop-tape UI
    // visualizes recent activity. Mid-audio write — see docs/stoch-review.md
    // finding #1; Patch 3 may relocate to engine-owned shadow if needed.
    sequence().events()[readIndex].mergeRhythmFrom(rhythm);
    // Cache stays in sync with the tape so the mask filter sees the live event.
    refreshCache();
}

void StochasticTrackEngine::writeLiveMelodyShadow(int readIndex, const StochasticSourceEvent &melody) {
    sequence().events()[readIndex].mergeMelodyFrom(melody);
    refreshCache();
}

void StochasticTrackEngine::syncWindowEdit() {
    // Called from UI edit paths after setFirst / setLast / setSize on this
    // track's sequence. Brings engine playback state in line with the new
    // window NOW — without waiting for the next event-boundary trigger.
    //
    // Without this, queued gate/CV events (with absolute ticks for slots that
    // may be outside the new window) and an in-flight _eventDuration (set
    // when the old window was active) keep playback honoring the old
    // boundaries until the current event finishes.
    auto &seq = sequence();
    int first = seq.first();
    int last  = seq.last();
    if (last < first) last = first;

    // Snap _patternIndex into the new window if it's now outside.
    if (_patternIndex < first || _patternIndex > last) {
        _patternIndex = first;
        // Force the next tick to trigger a step (don't wait for the now-stale
        // _eventDuration to elapse — its remaining ticks belonged to the old
        // window's event).
        _eventElapsed = _eventDuration;
    }

    // Queued events were scheduled at absolute ticks for slots that may not
    // exist in the new window. Drop them all; force gate off so the user
    // doesn't hear a stuck gate from the old window.
    _gateQueue.clear();
    _cvQueue.clear();
    _gateOutput = false;
    _activity = false;

    // Cache rank ordering was built for the old window — windowSize feeds the
    // mask threshold and the rank distribution was assigned over the old
    // window's slot set. Rebuild for the new window.
    refreshCache();
}

void StochasticTrackEngine::refreshCache() {
    auto &seq = sequence();
    // Match the divisor computation in tick(): base divisor scaled by the
    // sequence's clockMultiplier and PPQN conversion. clockMultiplier is in
    // hundredths of one (so 100 == 1.0×).
    float clockMult = seq.clockMultiplier() * 0.01f;
    if (clockMult <= 0.f) clockMult = 1.f;
    uint32_t divisor = uint32_t(seq.divisor()) * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    divisor = std::max<uint32_t>(1u, uint32_t(std::lround(divisor / clockMult)));
    // rhythmSeed drives deterministic rank salt — same seed → same rank order.
    stochastic_cache::regenerateCacheFromEvents(_cache, seq, divisor, seq.rhythmSeed());
    // tilt feeds rank weights; recomputeCacheRanks already ran inside
    // regenerateCacheFromEvents with tilt=0, so re-run with live tilt.
    stochastic_cache::recomputeCacheRanks(_cache, seq.rhythmSeed(), int(seq.tilt()));
}

// captureLockedParent removed 2026-05-22 — see header comment near the
// triggerStep declaration and `.tasks/stochastic-track-port/LOCK-DESIGN-DEFERRED.md`.

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
    if (children > kMaxChildren) children = kMaxChildren;
    int cvSixteenths = int(cv * 16.f);
    if (cvSixteenths < -128) cvSixteenths = -128;
    if (cvSixteenths > 127) cvSixteenths = 127;
    gDirectHistory[trackIndex][0].cvSixteenths = int8_t(cvSixteenths);
    gDirectHistory[trackIndex][0].children = children;
    gDirectHistory[trackIndex][0].flags = (rest ? 1 : 0) | (gate ? 2 : 0);
    if (gDirectHistoryCount[trackIndex] < kDirectHistoryMax) gDirectHistoryCount[trackIndex]++;
}
