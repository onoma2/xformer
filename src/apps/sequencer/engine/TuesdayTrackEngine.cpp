#include "TuesdayTrackEngine.h"

#include "Engine.h"
#include "model/Scale.h"

#include <cmath>
#include <algorithm>

// Initialize algorithm state based on Flow (seed1) and Ornament (seed2)
//
// DUAL RNG SYSTEM (Tuesday Spec Law 2):
// - _rng (Main RNG, seeded from Flow): Controls FLOW decisions
//   * Timing, mode transitions, gesture state machines, big structural changes
//   * Example: When to mutate, which phrase to play, beat-locked decisions
//
// - _extraRng (Aux RNG, seeded from Ornament): Controls ORNAMENT details
//   * Pitch variations, velocity, articulation, micro-timing
//   * Example: Which note in a scale, accent probability, slide amount
//
// This separation prevents unwanted correlation (e.g., slides always sync with accents).
// Salt value (0x9e3779b9) ensures distinct sequences even when Flow == Ornament.
void TuesdayTrackEngine::initAlgorithm() {
    const auto &_tuesdayTrackRef = tuesdayTrack();
    const auto &sequence = _tuesdayTrackRef.sequence(pattern());
    int flow = sequence.flow();
    int ornament = sequence.ornament();
    int algorithm = sequence.algorithm();

    // Generate seeds
    // Salt the Extra seed to ensure it's distinct even if Flow == Ornament
    uint32_t flowSeed = (flow - 1) << 4;
    uint32_t ornamentSeed = (ornament - 1) << 4;
    
    _uiRng = Random(flow * 37 + ornament * 101);

    _cachedFlow = flow;
    _cachedOrnament = ornament;
    _cachedAlgorithm = algorithm;
    _cachedLoopLength = sequence.loopLength();

    switch (algorithm) {
    case 0: // TEST
        _algoState.test.mode = (flow - 1) >> 3;
        _algoState.test.sweepSpeed = ((flow - 1) & 0x3);
        _algoState.test.accent = (ornament - 1) >> 3;
        _algoState.test.velocity = ((ornament - 1) << 4);
        _algoState.test.note = 0;

        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);
        break;

    case 1: // TRITRANCE
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _algoState.tritrance.b1 = (_rng.next() & 0x7);
        _algoState.tritrance.b2 = (_rng.next() & 0x7);

        _algoState.tritrance.b3 = (_extraRng.next() & 0x15);
        if (_algoState.tritrance.b3 >= 7) _algoState.tritrance.b3 -= 7; else _algoState.tritrance.b3 = 0;
        _algoState.tritrance.b3 -= 4;
        break;

    case 2: // STOMPER
        // FIX: Corrected RNG assignment to match Tuesday spec (Law 2)
        // Original C had R=seed2, Extra=seed1 (swapped), which violated the spec.
        // Correct: Flow → main RNG (gesture state machine), Ornament → extra RNG (velocity/timing)
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _algoState.stomper.mode = (_extraRng.next() % 7) * 2;
        _algoState.stomper.countDown = 0;
        _algoState.stomper.lowNote = _rng.next() % 3;
        _algoState.stomper.lastNote = _algoState.stomper.lowNote;
        _algoState.stomper.lastOctave = 0;
        _algoState.stomper.highNote[0] = _rng.next() % 7;
        _algoState.stomper.highNote[1] = _rng.next() % 5;
        break;

    case 6: // MARKOV
        _rng = Random(flowSeed);
        // Init History
        _algoState.markov.history1 = (_rng.next() & 0x7);
        _algoState.markov.history3 = (_rng.next() & 0x7);
        // Init Matrix
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                _algoState.markov.matrix[i][j][0] = (_rng.next() % 8);
                _algoState.markov.matrix[i][j][1] = (_rng.next() % 8);
            }
        }
        break;

    case 7: // CHIPARP 1
        _rng = Random(flowSeed);
        _algoState.chiparp1.chordSeed = _rng.next();
        _algoState.chiparp1.rngSeed = _algoState.chiparp1.chordSeed;
        _algoState.chiparp1.base = _rng.next() % 3;
        _algoState.chiparp1.dir = (_rng.next() >> 7) % 2;
        break;

    case 8: // CHIPARP 2
        _rng = Random(flowSeed);
        _algoState.chiparp2.rngSeed = _rng.next();
        _algoState.chiparp2.chordScaler = (_rng.next() % 3) + 2;
        _algoState.chiparp2.offset = (_rng.next() % 5);
        _algoState.chiparp2.len = ((_rng.next() & 0x3) + 1) * 2;
        _algoState.chiparp2.timeMult = _rng.nextBinary() ? (_rng.nextBinary() ? 1 : 0) : 0;
        _algoState.chiparp2.deadTime = 0;
        _algoState.chiparp2.idx = 0;
        _algoState.chiparp2.dir = _rng.nextBinary() ? (_rng.nextBinary() ? 1 : 0) : 0;
        _algoState.chiparp2.chordLen = 3 + (flow >> 2);
        break;

    case 9: // WOBBLE
        // FIX: Corrected RNG assignment to match Tuesday spec (Law 2)
        // Original C had R=seed2, Extra=seed1 (swapped), which violated the spec.
        // Correct: Flow → main RNG (phase selection), Ornament → extra RNG (velocity)
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _algoState.wobble.phase = 0;
        // Speed based on pattern length (dynamically updated in generateStep)
        // Original: 0xFFFFFFFF / Length
        _algoState.wobble.phaseSpeed = 0x08000000; // Slow (will be overridden)
        _algoState.wobble.phase2 = 0;
        _algoState.wobble.lastWasHigh = 0;
        _algoState.wobble.phaseSpeed2 = 0x02000000; // Slower (will be overridden)
        break;

    case 10: // SCALEWALKER
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);
        _algoState.scalewalker.pos = 0;
        break;

    case 3: // APHEX (Mapped from 18)
    case 18:
        _rng = Random(flowSeed);
        for (int i = 0; i < 4; ++i) _algoState.aphex.track1_pattern[i] = _rng.next() % 12;
        for (int i = 0; i < 3; ++i) _algoState.aphex.track2_pattern[i] = _rng.next() % 3;
        for (int i = 0; i < 5; ++i) _algoState.aphex.track3_pattern[i] = (_rng.next() % 8 == 0) ? (_rng.next() % 5) : 0;

        _algoState.aphex.pos1 = (ornament * 1) % 4;
        _algoState.aphex.pos2 = (ornament * 2) % 3;
        _algoState.aphex.pos3 = (ornament * 3) % 5;
        break;

    case 4: // AUTECHRE (Mapped from 19)
    case 19:
        // Seed pattern with variation based on Flow
        _rng = Random(flowSeed);
        for (int i = 0; i < 8; ++i) {
            // 50% chance of root, 25% +1oct, 25% +2oct
            int r = _rng.next() % 4;
            if (r == 0) _algoState.autechre.pattern[i] = 12; // +1 oct
            else if (r == 1) _algoState.autechre.pattern[i] = 24; // +2 oct
            else _algoState.autechre.pattern[i] = 0; // root

            // Add some melodic movement
            if (_rng.nextBinary()) _algoState.autechre.pattern[i] += (_rng.next() % 5) * 2; // 0, 2, 4, 6, 8
        }
        _algoState.autechre.rule_timer = 8 + (flow * 4);

        _rng = Random(ornamentSeed);
        for (int i = 0; i < 8; ++i) _algoState.autechre.rule_sequence[i] = _rng.next() % 5;
        _algoState.autechre.rule_index = 0;
        break;

    case 5: // STEPWAVE (Mapped from 20)
    case 20:
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        // Direction is controlled by flow parameter in generateStep()
        // Ornament controls timing mode (rapid vs spread)
        _algoState.stepwave.direction = 0;  // Will be set by flow in generateStep()
        _algoState.stepwave.step_count = 3 + (_rng.next() % 5);
        _algoState.stepwave.current_step = 0;
        _algoState.stepwave.chromatic_offset = 0;
        _algoState.stepwave.is_stepped = true;
        break;

    default:
        // Fallback to TEST
        _algoState.test.mode = (flow - 1) >> 3;
        _algoState.test.sweepSpeed = ((flow - 1) & 0x3);
        _algoState.test.accent = (ornament - 1) >> 3;
        _algoState.test.velocity = ((ornament - 1) << 4);
        _algoState.test.note = 0;
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);
        break;
    }
}

void TuesdayTrackEngine::reset() {
    _cachedAlgorithm = -1;
    _cachedFlow = -1;
    _cachedOrnament = -1;
    _cachedLoopLength = -1;

    _stepIndex = 0;
    _displayStep = -1;
    _gateLength = 0;
    _gateTicks = 0;
    _coolDown = 0;
    _coolDownMax = 0;
    _microCoolDown = 0;
    _microCoolDownMax = 0;
    _gatePercent = 75;
    _gateOffset = 0;
    _slide = 0;
    _cvTarget = 0.f;
    _cvCurrent = 0.f;
    _cvDelta = 0.f;
    _slideCountDown = 0;

    _microGateQueue.clear();
    _tieActive = false;

    _retriggerArmed = false;
    _retriggerCount = 0;
    _retriggerPeriod = 0;
    _retriggerLength = 0;
    _retriggerTimer = 0;
    _isTrillNote = false;
    _trillCvTarget = 0.f;
    _ratchetInterval = 0;

    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;
    _lastGatedCv = 0.f;

    // Zero-initialize union
    memset(&_algoState, 0, sizeof(_algoState));

    initAlgorithm();
}

void TuesdayTrackEngine::restart() {
    reset();
}

void TuesdayTrackEngine::reseed() {
    // Reset step to beginning
    _stepIndex = 0;
    _coolDown = 0;

    // Advance the RNGs to get new seeds
    uint32_t newSeed1 = _rng.next();
    uint32_t newSeed2 = _extraRng.next();

    // Re-seed with these new values
    // This simulates "turning the knobs" to a new random position
    // We need to temporarily override the cached Flow/Ornament effectively
    // But initAlgorithm reads from sequence. 
    // Ideally, 'reseed' implies we want variation *within* the current parameters?
    // Or does it mean "Scramble"?
    // For now, we'll just re-init the RNGs directly to simulate a fresh start
    _rng = Random(newSeed1);
    _extraRng = Random(newSeed2);
    
    // We should probably re-run init logic that depends on RNG
    // But without changing the 'flow/ornament' variables which are read from sequence.
    // A full initAlgorithm call would reset seeds based on Sequence.
    // So we need to manually re-init state vars here if we want true random reseed.
    
    const auto &sequence = tuesdayTrack().sequence(pattern());
    int algorithm = sequence.algorithm();
    int flow = sequence.flow();
    int ornament = sequence.ornament();

    // Reinitialize algorithm state with new random seeds
    // Pattern matches initAlgorithm() but uses already-reseeded RNGs
    switch (algorithm) {
    case 0: // TEST
        _algoState.test.note = 0;
        break;

    case 1: // TRITRANCE
        _algoState.tritrance.b1 = (_rng.next() & 0x7);
        _algoState.tritrance.b2 = (_rng.next() & 0x7);
        _algoState.tritrance.b3 = (_extraRng.next() & 0x15);
        if (_algoState.tritrance.b3 >= 7) _algoState.tritrance.b3 -= 7; else _algoState.tritrance.b3 = 0;
        _algoState.tritrance.b3 -= 4;
        break;

    case 2: // STOMPER
        _algoState.stomper.mode = (_extraRng.next() % 7) * 2;
        _algoState.stomper.countDown = 0;
        _algoState.stomper.lowNote = _rng.next() % 3;
        _algoState.stomper.lastNote = _algoState.stomper.lowNote;
        _algoState.stomper.lastOctave = 0;
        _algoState.stomper.highNote[0] = _rng.next() % 7;
        _algoState.stomper.highNote[1] = _rng.next() % 5;
        break;

    case 3: // APHEX (Mapped from 18)
    case 18:
        for (int i = 0; i < 4; ++i) _algoState.aphex.track1_pattern[i] = _rng.next() % 12;
        for (int i = 0; i < 3; ++i) _algoState.aphex.track2_pattern[i] = _rng.next() % 3;
        for (int i = 0; i < 5; ++i) _algoState.aphex.track3_pattern[i] = (_rng.next() % 8 == 0) ? (_rng.next() % 5) : 0;
        _algoState.aphex.pos1 = (ornament * 1) % 4;
        _algoState.aphex.pos2 = (ornament * 2) % 3;
        _algoState.aphex.pos3 = (ornament * 3) % 5;
        break;

    case 4: // AUTECHRE (Mapped from 19)
    case 19: {
        for (int i = 0; i < 8; ++i) {
            int r = _rng.next() % 4;
            if (r == 0) _algoState.autechre.pattern[i] = 12; // +1 oct
            else if (r == 1) _algoState.autechre.pattern[i] = 24; // +2 oct
            else _algoState.autechre.pattern[i] = 0; // root
            if (_rng.nextBinary()) _algoState.autechre.pattern[i] += (_rng.next() % 5) * 2;
        }
        _algoState.autechre.rule_timer = 8 + (flow * 4);
        // Reseed _rng temporarily for rule_sequence (matches initAlgorithm pattern)
        uint32_t tempSeed = _extraRng.next();
        Random tempRng(tempSeed);
        for (int i = 0; i < 8; ++i) _algoState.autechre.rule_sequence[i] = tempRng.next() % 5;
        _algoState.autechre.rule_index = 0;
    } break;

    case 5: // STEPWAVE (Mapped from 20)
    case 20:
        _algoState.stepwave.direction = 0;
        _algoState.stepwave.step_count = 3 + (_rng.next() % 5);
        _algoState.stepwave.current_step = 0;
        _algoState.stepwave.chromatic_offset = 0;
        _algoState.stepwave.is_stepped = true;
        break;

    case 6: // MARKOV
        _algoState.markov.history1 = (_rng.next() & 0x7);
        _algoState.markov.history3 = (_rng.next() & 0x7);
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                _algoState.markov.matrix[i][j][0] = (_rng.next() % 8);
                _algoState.markov.matrix[i][j][1] = (_rng.next() % 8);
            }
        }
        break;

    case 7: // CHIPARP1
        _algoState.chiparp1.chordSeed = _rng.next();
        _algoState.chiparp1.rngSeed = _algoState.chiparp1.chordSeed;
        _algoState.chiparp1.base = _rng.next() % 3;
        _algoState.chiparp1.dir = (_rng.next() >> 7) % 2;
        break;

    case 8: // CHIPARP2
        _algoState.chiparp2.rngSeed = _rng.next();
        _algoState.chiparp2.chordScaler = (_rng.next() % 3) + 2;
        _algoState.chiparp2.offset = (_rng.next() % 5);
        _algoState.chiparp2.len = ((_rng.next() & 0x3) + 1) * 2;
        _algoState.chiparp2.timeMult = _rng.nextBinary() ? (_rng.nextBinary() ? 1 : 0) : 0;
        _algoState.chiparp2.deadTime = 0;
        _algoState.chiparp2.idx = 0;
        _algoState.chiparp2.dir = _rng.nextBinary() ? (_rng.nextBinary() ? 1 : 0) : 0;
        _algoState.chiparp2.chordLen = 3 + (flow >> 2);
        break;

    case 9: // WOBBLE
        _algoState.wobble.phase = 0;
        _algoState.wobble.phaseSpeed = 0x08000000;
        _algoState.wobble.phase2 = 0;
        _algoState.wobble.lastWasHigh = 0;
        _algoState.wobble.phaseSpeed2 = 0x02000000;
        break;

    case 10: // SCALEWALKER
        _algoState.scalewalker.pos = 0;
        break;
    }
}

TuesdayTrackEngine::GenerationContext TuesdayTrackEngine::calculateContext(uint32_t tick) const {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    GenerationContext ctx;

    ctx.divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);
    ctx.stepsPerBeat = (ctx.divisor > 0) ? (CONFIG_PPQN / ctx.divisor) : 0;
    ctx.isBeatStart = (ctx.stepsPerBeat > 0) && ((_stepIndex % ctx.stepsPerBeat) == 0);

    uint32_t stepTicks = ctx.divisor;
    ctx.tpb = std::max(1, (int)((192 + (stepTicks/2)) / stepTicks));

    ctx.loopLength = sequence.actualLoopLength();
    ctx.effectiveLoopLength = (ctx.loopLength > 0) ? ctx.loopLength : 32;

    int rotate = (ctx.loopLength > 0) ? sequence.rotate() : 0;
    ctx.rotatedStep = (ctx.loopLength > 0)
        ? ((_stepIndex + rotate + ctx.loopLength) % ctx.loopLength)
        : _stepIndex;

    ctx.ornament = sequence.ornament();
    ctx.subdivisions = 4;
    if (ctx.ornament >= 5 && ctx.ornament <= 8) ctx.subdivisions = 3;
    else if (ctx.ornament >= 9 && ctx.ornament <= 12) ctx.subdivisions = 5;
    else if (ctx.ornament >= 13) ctx.subdivisions = 7;

    return ctx;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateTest(const GenerationContext &ctx) {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    TuesdayTickResult result;
    result.velocity = 255;
    result.gateRatio = 75;

    if (_rng.nextRange(100) < sequence.glide()) {
        result.slide = true;
    }

    switch (_algoState.test.mode) {
    case 0: // OCTSWEEPS
        result.octave = (_stepIndex % 5);
        result.note = 0;
        break;
    case 1: // SCALEWALKER
    default:
        result.octave = 0;
        result.note = _algoState.test.note;
        _algoState.test.note = (_algoState.test.note + 1) % 12;
        break;
    }
    result.velocity = _algoState.test.velocity;
    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateTritrance(const GenerationContext &ctx) {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    TuesdayTickResult result;
    result.velocity = 255;

    // Gate Length Logic (Probabilistic)
    int gateLenRnd = _rng.nextRange(100);
    if (gateLenRnd < 40) result.gateRatio = 50 + (_rng.nextRange(4) * 12);
    else if (gateLenRnd < 70) result.gateRatio = 100 + (_rng.nextRange(4) * 25);
    else result.gateRatio = 200 + (_rng.nextRange(9) * 25);

    // Slide Logic
    if (_rng.nextRange(100) < sequence.glide()) {
        result.slide = true;
    }

    // Note: TriTrance uses modulo 3, which is its own rhythmic grid (Polyrhythm).
    // It does NOT use the Beat (TPB) concept, so we leave it as is.
    int phase = (_stepIndex + _algoState.tritrance.b2) % 3;
    switch (phase) {
    case 0:
        if (_extraRng.nextBinary() && _extraRng.nextBinary()) {
            _algoState.tritrance.b3 = (_extraRng.next() & 0x15);
            if (_algoState.tritrance.b3 >= 7) _algoState.tritrance.b3 -= 7; else _algoState.tritrance.b3 = 0;
            _algoState.tritrance.b3 -= 4;
        }
        result.octave = 0;
        result.note = _algoState.tritrance.b3 + 4;
        // Phase 0: Tight/Human (0-10%)
        result.gateOffset = clamp(int(10 - _rng.nextRange(10)), 0, 100);
        break;
    case 1:
        result.octave = 1;
        result.note = _algoState.tritrance.b3 + 4;
        if (_rng.nextBinary()) _algoState.tritrance.b2 = (_rng.next() & 0x7);
        // Phase 1: Heavy Swing (25-35%)
        result.gateOffset = clamp(int(25 + _rng.nextRange(10)), 0, 100);
        break;
    case 2:
        result.octave = 2;
        result.note = _algoState.tritrance.b1;
        result.velocity = 255; // Accent phase
        result.accent = true;
        if (_rng.nextBinary()) _algoState.tritrance.b1 = ((_rng.next() >> 5) & 0x7);
        // Phase 2: Max Drag (40-50%)
        result.gateOffset = clamp(int(40 + _rng.nextRange(10)), 0, 100);

        // TRILL PATTERN: Grace notes leading to accent
        // stepTrill UI parameter will control probability
        result.trillCount = 3;  // Request triplet feel
        // Grace notes pattern: [grace, grace, MAIN]
        result.noteOffsets[0] = -2;  // Grace 2 semitones below
        result.noteOffsets[1] = -1;  // Grace 1 semitone below
        result.noteOffsets[2] = 0;   // Main accent note
        break;
    }

    // Velocity variation (unless accented above)
    if (!result.accent) {
        result.velocity = (_rng.nextRange(256) / 2);
    }

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateMarkov(const GenerationContext &ctx) {
    TuesdayTickResult result;
    result.velocity = 255;
    result.gateRatio = 75;

    int idx = _rng.nextBinary() ? 1 : 0;
    int newNote = _algoState.markov.matrix[_algoState.markov.history1][_algoState.markov.history3][idx];

    _algoState.markov.history1 = _algoState.markov.history3;
    _algoState.markov.history3 = newNote;

    result.note = newNote; // 0-7 scale degrees
    result.octave = _rng.nextBinary();

    // Random Slide/Length
    if (_rng.nextRange(100) < 50) result.gateRatio = 100 + (_rng.nextRange(4) * 25);
    else result.gateRatio = 75;

    if (_rng.nextBinary() && _rng.nextBinary()) result.slide = true;

    // 100% accent probability per original Tuesday
    result.accent = true;

    result.velocity = (_rng.nextRange(256) / 2) + 40;

    // Markov Timing (Push/Pull)
    if (_rng.nextBinary()) {
        result.gateOffset = clamp(int(10 - _rng.nextRange(10)), 0, 100);
    } else {
        result.gateOffset = clamp(int(15 + _rng.nextRange(10)), 0, 100);
    }

    // TRILL PATTERN: Occasional flams for humanization/texture
    // stepTrill UI parameter will control probability (~8% base chance scaled by stepTrill)
    if (_rng.nextRange(256) < 20) {  // ~8% chance to enable flam opportunity
        result.trillCount = 2;  // Request double-hit flam
        result.noteOffsets[0] = 0;  // First hit
        result.noteOffsets[1] = 0;  // Same note (flam effect)
        result.gateRatio = 50;  // Shorter gates for flam
    }

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateWobble(const GenerationContext &ctx) {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    TuesdayTickResult result;
    result.velocity = 255;

    // Update PhaseSpeed based on Effective Loop Length
    // Original: 0xffffffff / Length
    // We update it dynamically here to allow breathing with Loop Length changes
    _algoState.wobble.phaseSpeed = 0xFFFFFFFF / std::max(1, ctx.effectiveLoopLength);
    _algoState.wobble.phaseSpeed2 = 0xCFFFFFFF / std::max(1, ctx.effectiveLoopLength / 4);

    _algoState.wobble.phase += _algoState.wobble.phaseSpeed;
    _algoState.wobble.phase2 += _algoState.wobble.phaseSpeed2;

    // PercChance(R, seed2). seed2=ornament.
    // Map ornament 0-16 to 0-255 threshold
    if (_rng.nextRange(256) >= (sequence.ornament() * 16)) {
        // Use Phase 2
        // FIX: Wrap 5-bit phase (0-31) to scale degrees and overflow to octaves
        int rawPhase = (_algoState.wobble.phase2 >> 27) & 0x1F;
        result.note = rawPhase % 7;  // Wrap to scale degrees 0-6
        result.octave = 1 + (rawPhase / 7);  // Base octave + overflow

        if (_algoState.wobble.lastWasHigh == 0) {
            if (_rng.nextRange(256) >= 200) result.slide = true;
        }
        _algoState.wobble.lastWasHigh = 1;
    } else {
        // Use Phase 1
        // FIX: Wrap 5-bit phase (0-31) to scale degrees and overflow to octaves
        int rawPhase = (_algoState.wobble.phase >> 27) & 0x1F;
        result.note = rawPhase % 7;  // Wrap to scale degrees 0-6
        result.octave = (rawPhase / 7);  // Overflow octaves

        if (_algoState.wobble.lastWasHigh == 1) {
            if (_rng.nextRange(256) >= 200) result.slide = true;
        }
        _algoState.wobble.lastWasHigh = 0;
    }

    result.velocity = (_extraRng.nextRange(256) / 4);
    if (_rng.nextRange(256) >= 50) result.accent = true;

    // Wobble Phase Offset (0-30%)
    result.gateOffset = clamp(int(((_algoState.wobble.phase + _algoState.wobble.phase2) >> 28) % 30), 0, 100);

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateStomper(const GenerationContext &ctx) {
    TuesdayTickResult result;
    result.velocity = 255;
    result.gateRatio = 75;

    int accentoffs = 0;
    uint8_t veloffset = 0;

    if (_algoState.stomper.countDown > 0) {
        // Rest period (Countdown)
        result.gateRatio = _algoState.stomper.countDown * 25;
        result.velocity = 0; // Silent
        _algoState.stomper.countDown--;
        result.note = _algoState.stomper.lastNote;
        result.octave = _algoState.stomper.lastOctave;
    } else {
        // Active period
        if (_algoState.stomper.mode >= 14) {
            _algoState.stomper.mode = (_extraRng.next() % 7) * 2;
        }

        // FIX: Original C had PercChance(R, 100) = 100% probability (always true)
        // Randomize notes every tick for chaotic character (Pattern C: Range-Biased Random)
        _algoState.stomper.lowNote = _rng.next() % 3;
        _algoState.stomper.highNote[0] = _rng.next() % 7;
        _algoState.stomper.highNote[1] = _rng.next() % 5;

        veloffset = 100; // Base velocity when active
        int maxticklen = 2;

        switch (_algoState.stomper.mode) {
        case 10: // SLIDEDOWN1
            result.octave = 1;
            result.note = _algoState.stomper.highNote[_rng.next() % 2];
            _algoState.stomper.mode++;
            break;
        case 11: // SLIDEDOWN2
            result.octave = 0;
            result.note = _algoState.stomper.lowNote;
            result.slide = true;
            if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
            _algoState.stomper.mode = 14;
            break;
        case 12: // SLIDEUP1
            result.octave = 0;
            result.note = _algoState.stomper.lowNote;
            _algoState.stomper.mode++;
            break;
        case 13: // SLIDEUP2
            result.octave = 1;
            result.note = _algoState.stomper.highNote[_rng.next() % 2];
            result.slide = true;
            if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
            _algoState.stomper.mode = 14;
            break;
        case 4: case 5: // LOW
            accentoffs = 100;
            result.octave = 0;
            result.note = _algoState.stomper.lowNote;
            if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
            _algoState.stomper.mode = 14;
            break;
        case 0: case 1: // PAUSE
            result.octave = _algoState.stomper.lastOctave;
            result.note = _algoState.stomper.lastNote;
            veloffset = 0; // Quiet
            if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
            _algoState.stomper.mode = 14;
            break;
        case 6: case 7: // HIGH
        case 8: case 9: // HILOW
        case 2: case 3: // LOWHI
            // Simplified mapping for brevity, adhering to logic
            if (_algoState.stomper.mode == 6 || _algoState.stomper.mode == 7 || _algoState.stomper.mode == 9 || _algoState.stomper.mode == 2) accentoffs = 100;

            if (_algoState.stomper.mode == 8 || _algoState.stomper.mode == 3) {
                result.octave = 1;
                result.note = _algoState.stomper.highNote[_rng.next() % 2];
                if (_algoState.stomper.mode == 8) _algoState.stomper.mode++; else if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
                if (_algoState.stomper.mode == 3) _algoState.stomper.mode = 14;
            } else {
                // NOTE: Original C bug faithfully preserved:
                // STOMPER_HIGH cases (6,7) play low octave/low note instead of high octave/high note
                // Original C line 101: case STOMPER_HIGH1: NOTE(0, PS->Stomper.LowNote);
                // This gives STOMPER its characteristic "heavy" sound
                 result.octave = 0;
                 result.note = _algoState.stomper.lowNote;
                 if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
                 _algoState.stomper.mode = 14;
            }
            break;
        }
        _algoState.stomper.lastNote = result.note;
        _algoState.stomper.lastOctave = result.octave;

        // Calculate velocity
        result.velocity = (_extraRng.nextRange(256) / 4) + veloffset;
        // Accent logic: PercChance(50 + accentoffs)
        if (_rng.nextRange(256) >= (50 + accentoffs)) {
             result.accent = true;
        }

        result.beatSpread = 18; // Base spread
    }

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateChipArp1(const GenerationContext &ctx) {
    TuesdayTickResult result;
    result.velocity = 255;
    result.gateRatio = 75;

    // Use TPB for beat-sync reset (replaces hardcoded % 4)
    int chordpos = _stepIndex % ctx.tpb;

    // TRILL PATTERN: Chord stabs on beat starts
    // stepTrill UI parameter will control probability (like Glide)
    if (chordpos == 0) {
        result.trillCount = 3;  // Request 3-gate chord stab
        // Chord intervals: root, third, fifth
        result.noteOffsets[0] = 0;  // root
        result.noteOffsets[1] = 2;  // third
        result.noteOffsets[2] = 4;  // fifth
    }

    if (chordpos == 0) {
        result.accent = true;
        _algoState.chiparp1.rngSeed = _algoState.chiparp1.chordSeed;
        if (_rng.nextRange(256) >= 0x20) _algoState.chiparp1.base = _rng.next() % 3;
        if (_rng.nextRange(256) >= 0xF0) _algoState.chiparp1.dir = (_rng.next() >> 7) % 2;
    }

    if (_algoState.chiparp1.dir == 1) chordpos = ctx.tpb - chordpos - 1;

    Random chipRng(_algoState.chiparp1.rngSeed);

    if (chipRng.nextRange(256) >= 0x20) result.accent = true;

    if (chipRng.nextRange(256) >= 0x80) {
        result.slide = true;
    }

    if (chipRng.nextRange(256) >= 0xD0) {
        result.velocity = 0; // Note Off
    } else {
        result.note = (chordpos * 2) + _algoState.chiparp1.base;
        result.octave = 0;
        int rnd = (chipRng.next() >> 7) % 3;
        result.gateRatio = (4 + 6 * rnd) * (100/6); // approx mapping
    }

    // FIX: Add extra velocity on first step of PATTERN (not global step)
    // Original used GENERIC.b2 to track pattern position
    int extraVel = (ctx.rotatedStep == 0) ? 127 : 0;
    result.velocity = (_rng.nextRange(256) / 2) + extraVel;

    // Pattern Offset
    result.gateOffset = clamp(int(_stepIndex % 7), 0, 100);

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateChipArp2(const GenerationContext &ctx) {
    TuesdayTickResult result;
    result.velocity = 255;
    result.gateRatio = 75;
    result.octave = 0;

    if (_algoState.chiparp2.deadTime > 0) {
        _algoState.chiparp2.deadTime--;
        result.velocity = 0; // Note Off
    } else {
        int deadTimeAdd = 0;
        if (_algoState.chiparp2.idx == _algoState.chiparp2.chordLen) {
            _algoState.chiparp2.idx = 0;
            _algoState.chiparp2.len--;
            result.accent = true;

            // TRILL PATTERN: Arpeggio runs on accent hits
            // stepTrill UI parameter will control probability
            result.trillCount = 4;  // Request 4-gate arpeggio
            // Ascending arpeggio pattern
            for (int i = 0; i < 4; i++) {
                result.noteOffsets[i] = i * _algoState.chiparp2.chordScaler;
            }

            if (_rng.nextRange(256) >= 200) deadTimeAdd = 1 + (_rng.next() % 3);

            if (_algoState.chiparp2.len == 0) {
                _algoState.chiparp2.rngSeed = _rng.next();
                _algoState.chiparp2.chordScaler = (_rng.next() % 3) + 2;
                _algoState.chiparp2.offset = (_rng.next() % 5);
                _algoState.chiparp2.len = ((_rng.next() & 0x3) + 1) * 2;
                if (_rng.nextBinary()) {
                    _algoState.chiparp2.dir = _rng.nextBinary();
                }
            }
        }

        int scaleidx = (_algoState.chiparp2.idx % _algoState.chiparp2.chordLen);
        if (_algoState.chiparp2.dir) scaleidx = _algoState.chiparp2.chordLen - scaleidx - 1;

        result.note = (scaleidx * _algoState.chiparp2.chordScaler) + _algoState.chiparp2.offset;
        _algoState.chiparp2.idx++;

        _algoState.chiparp2.deadTime = _algoState.chiparp2.timeMult;
        if (_algoState.chiparp2.deadTime > 0) {
            result.gateRatio = (1 + _algoState.chiparp2.timeMult) * 100;
        }
        _algoState.chiparp2.deadTime += deadTimeAdd;
    }

    Random chip2Rng(_algoState.chiparp2.rngSeed);
    result.velocity = chip2Rng.nextRange(256);

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateAphex(const GenerationContext &ctx) {
    TuesdayTickResult result;
    result.velocity = 255;

    // Polyrhythmic Time Warping
    // Subdivisions control the time base for the Modifier Track (Track 2)
    uint32_t modifierStep = _stepIndex;
    if (ctx.subdivisions != 4) {
        modifierStep = (_stepIndex * ctx.subdivisions) / 4;
    }

    // Track 1: Main Melody (Straight)
    // Use the standard position counters but update them based on warped steps?
    // No, simpler: Use the calculated steps directly to look up pattern.
    // But Aphex uses stateful position counters (_aphex_pos1).
    // We must advance the state differently.
    // Let's apply the warping to the *increment* decision.

    // Actually, since we run live every step, we can't skip updates.
    // We must index into the pattern using the warped step.

    // Re-calculate positions based on warped indices
    // This makes the algo stateless (good for loop points!)
    // Use rotatedStep for pattern indexing to enable rotation parameter
    int pos1 = ctx.rotatedStep % 4; // Track 1 is always straight 4/4
    int pos2 = modifierStep % 3; // Track 2 is warped (intentionally uses unrotated time)
    int pos3 = ctx.rotatedStep % 5; // Track 3 is straight 5/4 (natural poly)

    result.note = _algoState.aphex.track1_pattern[pos1];
    result.octave = 0;
    result.gateRatio = 75;
    result.velocity = 180;

    uint8_t modifier = _algoState.aphex.track2_pattern[pos2];
    if (modifier == 1) {
        result.gateRatio = 25;
        result.velocity = 80;
        result.beatSpread = 60;
    } else if (modifier == 2) {
        result.gateRatio = 100;
        result.slide = true;
    }

    uint8_t bass = _algoState.aphex.track3_pattern[pos3];
    if (bass > 0) {
        result.note = bass;
        result.octave = -1;
        result.gateRatio = 90;
        result.velocity = 255;
        result.accent = true;
    }

    // Collision logic needs warped pos2
    int polyCollision = ((pos1 * 7) ^ (pos2 * 5) ^ (pos3 * 3)) % 12;
    if (polyCollision > 9) {
        result.beatSpread = 100;
        result.velocity = 255;
    }

    // Polyrhythm Offset
    int polyRhythmFactor = (pos1 + pos2 + pos3) % 12;
    result.gateOffset = clamp(polyRhythmFactor * 8, 0, 100);

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateAutechre(const GenerationContext &ctx) {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    TuesdayTickResult result;
    result.velocity = 255;

    // Polyrhythmic Time Warping (Fast Tuplets)
    int flow = sequence.flow();

    // Signal polyrhythm on beat starts ONLY (don't mask intermediate steps)
    if (ctx.isBeatStart && ctx.subdivisions != 4) {
        // Vary beatSpread based on flow (timing window control)
        // flow 0-16 → beatSpread 20-100 (low flow = narrow window, high flow = wide spread)
        result.beatSpread = 20 + (flow * 5);
        result.polyCount = ctx.subdivisions;  // Pass tuplet count to FX layer
        result.isSpatial = true;          // Polyrhythm mode (spread in time)

        // Generate note offsets from pattern (micro-melody)
        // Use pattern values as intervals, cycling through pattern if needed
        for (int i = 0; i < ctx.subdivisions && i < 8; i++) {
            int patternIdx = (ctx.rotatedStep + i) % 8;
            int patternNote = _algoState.autechre.pattern[patternIdx] % 12;
            // Convert to interval relative to base note (first pattern value)
            int baseNote = _algoState.autechre.pattern[ctx.rotatedStep % 8] % 12;
            result.noteOffsets[i] = patternNote - baseNote;
        }
    }

    // Algorithm logic (ALWAYS run, no masking)
    // Use rotatedStep for pattern indexing to enable rotation parameter
    int patternVal = _algoState.autechre.pattern[ctx.rotatedStep % 8];
    result.note = patternVal % 12;
    result.octave = patternVal / 12;

    // Random Jitter (0-10%)
    result.gateOffset = clamp(int(_rng.nextRange(11)), 0, 100);

    _algoState.autechre.rule_timer--;

    result.velocity = 160;  // NO MASKING
    result.gateRatio = 75;
    // ... (Transformation Logic) ...
    if (_algoState.autechre.rule_timer <= 0) {
        uint8_t rule = _algoState.autechre.rule_sequence[_algoState.autechre.rule_index];
        int intensity = sequence.power() / 2;
        if (result.velocity > 0) {
            result.velocity = 255;
            result.accent = true;
        }
        if (rule == 0) { int8_t t = _algoState.autechre.pattern[7]; for(int i=7; i>0; --i) _algoState.autechre.pattern[i]=_algoState.autechre.pattern[i-1]; _algoState.autechre.pattern[0]=t; }
        else if (rule == 1) { for(int i=0;i<4;++i) std::swap(_algoState.autechre.pattern[i], _algoState.autechre.pattern[7-i]); }
        else if (rule == 2) { for(int i=0;i<8;++i) { int o=_algoState.autechre.pattern[i]/12; int n=_algoState.autechre.pattern[i]%12; n=(6-(n-6)+12)%12; _algoState.autechre.pattern[i]=n+o*12; } }
        else if (rule == 3) { for(int i=0;i<8;i+=2) std::swap(_algoState.autechre.pattern[i], _algoState.autechre.pattern[i+1]); }
        else if (rule == 4) { for(int i=0;i<8;++i) { int o=_algoState.autechre.pattern[i]/12; int n=(_algoState.autechre.pattern[i]%12+intensity)%12; _algoState.autechre.pattern[i]=n+o*12; } }

        // FIX: Use cached flow to prevent timer drift if user tweaks knob mid-pattern
        _algoState.autechre.rule_timer = 8 + (_cachedFlow * 4);
        _algoState.autechre.rule_index = (_algoState.autechre.rule_index + 1) % 8;
    }

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateStepwave(const GenerationContext &ctx) {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    TuesdayTickResult result;
    result.velocity = 255;

    int flow = sequence.flow();
    int ornament = sequence.ornament();

    // Signal micro-sequencing on beat starts (scale-based stepping with glide)
    if (ctx.isBeatStart && ctx.subdivisions != 4) {
        // Vary beatSpread based on flow (timing window control)
        result.beatSpread = 20 + (flow * 5);
        result.polyCount = ctx.subdivisions;

        // ORNAMENT controls timing mode:
        // 0-7: Rapid (1-beat window) - fast chromatic-style runs within scale
        // 8: Neutral transition point
        // 9-16: Spread (4-beat window) - polyrhythmic patterns across beat
        result.isSpatial = (ornament >= 9);

        // Update direction based on flow using probability
        int downwardProbability = (16 - flow) * 6;  // Flow 0: ~96%, Flow 8: ~48%, Flow 16: ~0%
        downwardProbability = clamp(downwardProbability, 10, 90); // Ensure some randomness at extremes

        if (_rng.nextRange(100) < downwardProbability) {
            _algoState.stepwave.direction = -1; // Downward
        } else {
            _algoState.stepwave.direction = 1;  // Upward
        }

        _algoState.stepwave.step_count = ctx.subdivisions;

        // Generate scale-degree offsets for micro-sequencing
        // These are intervals in scale degrees, respecting the selected scale
        for (int i = 0; i < ctx.subdivisions && i < 8; i++) {
            result.noteOffsets[i] = i * _algoState.stepwave.direction;  // Scale-degree intervals
        }

        // Enable glide for smooth transitions between micro-sequence notes
        result.slide = true;
    }

    // Base note generation (scale-based walking) with probabilistic direction
    int dir;
    int downwardProbability = (16 - flow) * 6;  // Flow 0: ~96%, Flow 8: ~48%, Flow 16: ~0%
    downwardProbability = clamp(downwardProbability, 10, 90); // Ensure some randomness at extremes

    if (_rng.nextRange(100) < downwardProbability) {
        dir = -1; // Downward
    } else {
        dir = 1;  // Upward
    }

    int stepSize = 2 + (_stepIndex % 2);

    result.note = (_stepIndex * dir * stepSize) % 7;
    if (result.note < 0) result.note += 7;

    // Always generate velocity (NO MASKING)
    result.velocity = 200;
    if (_rng.nextRange(100) < (20 + flow * 3)) {
        result.octave = 2;
        result.velocity = 255;
        result.accent = true;
    } else {
        result.octave = 0;
    }

    if (_rng.nextRange(100) < sequence.glide()) {
        result.slide = true;
        result.gateRatio = 100;
    } else {
        result.gateRatio = 75;
    }

    // Micro Jitter (0-6%)
    result.gateOffset = clamp(int(_rng.nextRange(7)), 0, 100);

    return result;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateScalewalker(const GenerationContext &ctx) {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    TuesdayTickResult result;
    result.velocity = 255;

    int flow = sequence.flow();

    // 1. Direction
    int direction = (flow <= 7) ? -1 : 1;  // Always walk scale (remove direction=0)

    // 2. Base note is current walker position
    result.note = _algoState.scalewalker.pos;
    result.octave = 0;
    result.velocity = 100 + (sequence.power() * 10);  // 100-260 range
    result.gateRatio = 75;

    // 3. Polyrhythm: N gates spread across 1 beat
    // Calculate which of N gates should fire THIS step
    bool shouldFirePolyGate = false;
    if (ctx.subdivisions != 4) {
        // Determine if THIS step is one of the N poly gates
        // Using beat position: which of N subdivisions are we at?
        int beatPosition = _stepIndex % ctx.stepsPerBeat;
        int gateIndex = (beatPosition * ctx.subdivisions) / ctx.stepsPerBeat;
        int nextGateIndex = ((beatPosition + 1) * ctx.subdivisions) / ctx.stepsPerBeat;

        if (gateIndex != nextGateIndex) {
            // This step crosses a poly gate boundary - fire it
            shouldFirePolyGate = true;
            result.polyCount = 1;  // Just this one poly gate
            result.beatSpread = 100;
            result.isSpatial = true;
            result.slide = true;

            // Note offset for this specific gate
            result.noteOffsets[0] = gateIndex * direction;
        }
    }

    // 4. stepTrill: INDEPENDENT subdivision (works on ANY step)
    int stepTrillCount = 1;
    if (sequence.stepTrill() > 0) {
        stepTrillCount = 1 + (sequence.stepTrill() * 3) / 100;
        stepTrillCount = clamp(stepTrillCount, 1, 4);
    }

    // Decide what to do based on poly gate and stepTrill
    if (shouldFirePolyGate) {
        // Poly gate step: subdivide if stepTrill active
        if (stepTrillCount > 1) {
            int baseOffset = result.noteOffsets[0];
            result.polyCount = stepTrillCount;
            result.beatSpread = 0;
            result.isSpatial = false;  // Rapid fire

            // All gates walk based on direction
            for (int i = 0; i < stepTrillCount && i < 8; i++) {
                result.noteOffsets[i] = baseOffset + (i * direction);
            }

            result.slide = sequence.glide() > 50;
        }
        // else: keep polyCount=1 from poly gate setup above
    } else if (stepTrillCount > 1) {
        // Non-poly step with stepTrill: fire rapid burst
        result.polyCount = stepTrillCount;
        result.beatSpread = 0;
        result.isSpatial = false;

        // All gates walk based on direction (from current walker position)
        for (int i = 0; i < stepTrillCount && i < 8; i++) {
            result.noteOffsets[i] = i * direction;
        }

        result.slide = sequence.glide() > 50;
    } else if (ctx.subdivisions == 4) {
        // No poly, no stepTrill: regular single gate via normal path
        result.polyCount = 0;  // Use normal gate system (not micro-queue)
        result.beatSpread = 0;
        result.isSpatial = false;
        result.noteOffsets[0] = 0;
    } else {
        // Poly mode but not a poly step and no stepTrill: MUTE
        result.polyCount = 0;
        result.velocity = 0;
    }

    // Advance walker every step (not just on beat starts)
    _algoState.scalewalker.pos = (_algoState.scalewalker.pos + direction + 7) % 7;

    return result;
}

float TuesdayTrackEngine::scaleToVolts(int noteIndex, int octave) const {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    const auto &project = _model.project();

    // 1. Resolve which Scale to use
    int trackScaleIdx = sequence.scale();
    const Scale &scale = (trackScaleIdx < 0)
        ? project.selectedScale()   // -1 = use project scale
        : Scale::get(trackScaleIdx); // 0+ = use specific scale (0 = Semitones/chromatic)

    // 2. Resolve Root Note
    int trackRoot = sequence.rootNote();
    int rootNote = (trackRoot < 0) ? project.rootNote() : trackRoot;

    // 3. Quantization is always applied
    // Scale 0 (Semitones) naturally provides chromatic quantization (all 12 notes)
    bool quantize = true;

    float voltage = 0.f;

    if (quantize) {
        // SCALE MODE: noteIndex is Scale Degree
        int totalDegree = noteIndex + (octave * scale.notesPerOctave());
        
        // Apply Transpose (in Degrees)
        totalDegree += sequence.transpose();

        // Convert to Volts
        voltage = scale.noteToVolts(totalDegree);

        // Add Root Note offset
        voltage += (rootNote * (1.f / 12.f));
    } else {
        // CHROMATIC MODE: noteIndex is Semitone
        int totalSemitones = noteIndex + (octave * 12);
        
        // Apply Transpose (in Semitones)
        totalSemitones += sequence.transpose();

        // 1V/Oct
        voltage = totalSemitones * (1.f / 12.f);
    }

    // 4. Apply Track Octave Offset
    voltage += sequence.octave();

    return voltage;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateStep(uint32_t tick) {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    int algorithm = sequence.algorithm();

    // Calculate shared context for all algorithms
    GenerationContext ctx = calculateContext(tick);

    // Map legacy/alternate algorithms to supported set
    int algo = algorithm;
    if (algorithm == 18) algo = 3; // APHEX
    if (algorithm == 19) algo = 4; // AUTECHRE
    if (algorithm == 20) algo = 5; // STEPWAVE

    // Dispatch to algorithm-specific helper
    switch (algo) {
    case 0:  return generateTest(ctx);
    case 1:  return generateTritrance(ctx);
    case 2:  return generateStomper(ctx);
    case 3:  return generateAphex(ctx);
    case 4:  return generateAutechre(ctx);
    case 5:  return generateStepwave(ctx);
    case 6:  return generateMarkov(ctx);
    case 7:  return generateChipArp1(ctx);
    case 8:  return generateChipArp2(ctx);
    case 9:  return generateWobble(ctx);
    case 10: return generateScalewalker(ctx);
    default: return generateTest(ctx);
    }
}

TrackEngine::TickResult TuesdayTrackEngine::tick(uint32_t tick) {
    if (mute()) { _gateOutput=false; _cvOutput=0.f; _activity=false; return TickResult::NoUpdate; }
    
    const auto &sequence = tuesdayTrack().sequence(pattern());
    // int power = sequence.power(); 
    // Power check moved inside stepTrigger to allow for Skew modulation

    // Parameter Change Check
    bool paramsChanged = (_cachedAlgorithm != sequence.algorithm() ||
                         _cachedFlow != sequence.flow() ||
                         _cachedOrnament != sequence.ornament() ||
                         _cachedLoopLength != sequence.loopLength());
    if (paramsChanged) {
        initAlgorithm();
    }

    // Time Calculation
    uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);

    // Apply Start Delay (Time Shift)
    uint32_t startTicks = sequence.start() * divisor;
    if (tick < startTicks) {
        _gateOutput = false;
        _activity = false;
        return TickResult::NoUpdate;
    }
    tick -= startTicks;

    int loopLength = sequence.actualLoopLength();
    uint32_t resetDivisor = (loopLength > 0) ? (loopLength * divisor) : 0;
    
    uint32_t relativeTick = (resetDivisor == 0) ? tick : tick % resetDivisor;
    
    // Finite Loop Reset
    if (resetDivisor > 0 && relativeTick == 0) {
        initAlgorithm(); // Reset RNGs to loop start state
        _stepIndex = 0;
    }

    bool stepTrigger = (relativeTick % divisor == 0);

    // --- MICRO-GATE QUEUE PROCESSING ---

    // Process scheduled gates from micro-queue
    while (!_microGateQueue.empty() && tick >= _microGateQueue.front().tick) {
        auto event = _microGateQueue.front();
        _microGateQueue.pop();

        _gateOutput = event.gate;
        _activity = event.gate;

        // Update CV from micro-gate queue (always apply when gate fires)
        // Micro-gates store their target CV and should always use it
        if (event.gate) {
            _cvOutput = event.cvTarget;
        }
    }

    // --- ENGINE UPDATE LOGIC ---
    
    // Handle existing gate timer
    if (_gateTicks > 0) {
        _gateTicks--;
        if (_gateTicks == 0) {
             // Gate OFF
             _gateOutput = false;
             // If in retrigger cycle, this is the gap between ratchets
             // Logic handled in update()
        }
    }
    
    // Handle CV Slide
    if (_slideCountDown > 0) {
        _cvCurrent += _cvDelta;
        _slideCountDown--;
        if (_slideCountDown == 0) _cvCurrent = _cvTarget;
        _cvOutput = _cvCurrent;
    }

    if (stepTrigger) {
        // Advance Step
        uint32_t calculatedStep = relativeTick / divisor;
        _stepIndex = calculatedStep;
        _displayStep = _stepIndex;

        // 1. GENERATE (The Brain)
        TuesdayTickResult result = generateStep(tick);
        
        // Apply Algorithm's Timing Offset with User Scaler
        // Scaler Model:
        // - User Knob 0%   -> Force 0 (Strict/Quantized)
        // - User Knob 50%  -> 1x (Original Algo Timing)
        // - User Knob 100% -> 2x (Exaggerated Swing)
        int userScaler = sequence.gateOffset();
        _gateOffset = clamp((int)((result.gateOffset * userScaler * 2) / 100), 0, 100);

        // TRILL PROCESSING (follows Glide pattern)
        // Algorithm sets trillCount (1-4) to request micro-gate subdivision
        // UI stepTrill parameter (0-100) controls probability of firing
        if (result.trillCount > 1 && sequence.stepTrill() > 0) {
            int chance = sequence.stepTrill();  // 0-100
            if (_uiRng.nextRange(100) < static_cast<uint32_t>(chance)) {
                // Fire the trill: activate micro-gate system
                result.polyCount = result.trillCount;
                result.isSpatial = false;  // Rapid fire trill mode
                result.beatSpread = 0;
                // noteOffsets[] already set by algorithm
            }
            // else: don't fire trill, keep polyCount = 0 (single gate)
        }

        // 2. FX LAYER (Post-Processing)

        // Calculate power/cooldown ONCE per step (used by both micro-gate and normal gate paths)
        int power = sequence.power();
        int loopLength = sequence.actualLoopLength();
        int skew = sequence.skew();

        if (loopLength > 0 && skew != 0) {
            int currentPos = _stepIndex % loopLength;
            int offset = -skew + ((2 * skew * currentPos) / std::max(1, loopLength - 1));
            power = std::max(0, std::min(16, power + offset));
        }

        int baseCooldown = 17 - power;
        if (baseCooldown < 1) baseCooldown = 1;
        _coolDownMax = baseCooldown;
        _microCoolDownMax = baseCooldown;  // Same mapping for microgate cooldown

        // Decrement cooldown once per step (before any gate decisions)
        if (_coolDown > 0) {
            _coolDown--;
            if (_coolDown > _coolDownMax) _coolDown = _coolDownMax;
        }

        // Decrement microgate cooldown once per step
        if (_microCoolDown > 0) {
            _microCoolDown--;
            if (_microCoolDown > _microCoolDownMax) _microCoolDown = _microCoolDownMax;
        }

        // A. Polyrhythm / Trill Detection
        _retriggerArmed = false;

        if (result.polyCount > 0) {
            // MICRO-SEQUENCING MODE: Distribute N gates with independent pitches
            int tupleN = result.polyCount;

            // Simplified: All gates fire within their own step
            // Poly spread is handled by which steps fire (algorithm layer)
            uint32_t windowTicks = divisor;
            uint32_t spacing = windowTicks / tupleN;  // Even distribution

            // Gate length: Use algorithm's gateRatio
            uint32_t baseGateLen = (spacing * result.gateRatio) / 100;
            uint32_t userGate = sequence.gateLength();
            uint32_t gateLen = (baseGateLen * userGate) / 50;
            if (gateLen < 1) gateLen = 1;

            // Gate offset (applies to FIRST gate only)
            uint32_t gateOffsetTicks = (divisor * _gateOffset) / 100;

            // Schedule N gate ON/OFF pairs with independent pitches
            // Apply gate offset to the base tick, then space gates evenly from there
            uint32_t baseTick = tick + gateOffsetTicks;

            // STEP-LEVEL POWER CHECK (applies to ALL micro-gates in this step)
            // Cooldown decrements once per STEP, not once per micro-gate
            bool stepAllowed = false;

            if (result.velocity == 0 || power == 0) {
                stepAllowed = false;
            } else if (result.accent) {
                stepAllowed = true;
            } else if (_coolDown == 0) {
                stepAllowed = true;
                _coolDown = _coolDownMax;
            } else {
                int velDensity = result.velocity / 16;
                if (velDensity >= _coolDown) {
                    stepAllowed = true;
                    _coolDown = _coolDownMax;
                }
            }

            // Schedule micro-gates if step is allowed
            if (stepAllowed) {
                int velDensity = result.velocity / 16;

                for (int i = 0; i < tupleN; i++) {
                    // Microgate-level cooldown check (Option 1.5)
                    // Each microgate checks independently with 2x harder threshold
                    bool microgateAllowed = false;

                    if (_microCoolDown == 0) {
                        // Cooldown expired, microgate fires
                        microgateAllowed = true;
                        _microCoolDown = _microCoolDownMax;
                    } else {
                        // Velocity override: threshold is 2x harder than step-level
                        int microThreshold = _microCoolDown * 2;
                        if (velDensity >= microThreshold) {
                            microgateAllowed = true;
                            _microCoolDown = _microCoolDownMax;
                        }
                    }

                    if (microgateAllowed) {
                        uint32_t offset = (i * spacing);

                        // Calculate voltage for THIS gate using note offset
                        int noteWithOffset = result.note + result.noteOffsets[i];
                        float volts = scaleToVolts(noteWithOffset, result.octave);

                        _microGateQueue.push({ baseTick + offset, true, volts });
                        _microGateQueue.push({ baseTick + offset + gateLen, false, volts });
                    }
                }
            }

            _retriggerArmed = true;  // Skip normal gate logic
        }
        else if (result.beatSpread > 0) {
            // TRILL MODE: Original probabilistic ratchet (unchanged)
            int chance = (result.beatSpread * sequence.trill()) / 100;
            if (_uiRng.nextRange(100) < static_cast<uint32_t>(chance)) {
                _retriggerArmed = true;
                _retriggerCount = 2;
                _retriggerPeriod = divisor / 3;
                _retriggerLength = _retriggerPeriod / 2;
                _ratchetInterval = 0;
            }
        }

        // B. Glide
        _slide = 0;
        if (result.slide && sequence.glide() > 0) {
             _slide = 1 + (sequence.glide() / 30); // Map 0-100 to 1-4 range approx
        }

        // C. Density (Power Gating)
        // Power/cooldown already calculated and decremented above
        // Cooldown now decrements once per step regardless of micro-gate vs normal gate path
        
        // Gate Decision:
        // 1. Must satisfy Cooldown (Density)
        // 2. Velocity helps overcome Cooldown (Original Tuesday Logic)
        // Original: if (vel >= cooldown)
        // We map velocity 0-255 to roughly 0-16 range to compare with cooldown?
        // Or just use raw velocity if cooldown was velocity-based?
        // In C++ port, cooldown is a counter.
        // Let's implement the "Beat the Clock" logic:
        // If cooldown is high, we need high velocity to trigger.
        // If we trigger, we reset cooldown.
        
        bool densityGate = false;
        bool accentActive = false;  // Track accent for gate length extension

        // 0. Explicit Mute from Algorithm (e.g. Polyrhythm Masking) OR Power 0
        if (result.velocity == 0 || power == 0) {
            densityGate = false;
        }
        // 1. ACCENT: Always fire + extend gate length (mapped from original GATE_ACCENT output)
        else if (result.accent) {
            densityGate = true;
            accentActive = true;  // Will extend gate length later
        }
        // 2. Timer Expired (Power)
        else if (_coolDown == 0) {
            densityGate = true;
        }
        // 3. High Velocity Override
        else {
            int velDensity = result.velocity / 16;
            if (velDensity >= _coolDown) {
                densityGate = true;
            }
        }
        
        if (densityGate) {
             // Reset pressure
             _coolDown = _coolDownMax;

             // D. Gate Length (Scaler)
             uint32_t gateOffsetTicks = (divisor * _gateOffset) / 100;
             uint32_t baseLen = (divisor * result.gateRatio) / 100;
             uint32_t userGate = sequence.gateLength();
             uint32_t gateLengthTicks = (baseLen * userGate) / 50;
             if (gateLengthTicks < 1) gateLengthTicks = 1;

             // Accent: Extend gate length by 50% (per original Tuesday's GATE_ACCENT mapping)
             if (accentActive) {
                 gateLengthTicks = (gateLengthTicks * 3) / 2;
             }

             // Calculate CV voltage
             float volts = scaleToVolts(result.note, result.octave);

             // Schedule gates in micro-queue (not immediate)
             // Skip if retrigger/polyrhythm will handle it
             if (!_retriggerArmed) {
                 _microGateQueue.push({ tick + gateOffsetTicks, true, volts });
                 _microGateQueue.push({ tick + gateOffsetTicks + gateLengthTicks, false, volts });
                 _activity = true;
             }

             // Handle CV Slide
             if (_slide > 0) {
                 _cvTarget = volts;
                 int slideTicks = _slide * 12;
                 _cvDelta = (_cvTarget - _cvCurrent) / slideTicks;
                 _slideCountDown = slideTicks;
             } else {
                 _cvCurrent = volts;
                 _cvOutput = volts;
                 _slideCountDown = 0;
             }

             // Handle Trill Start CV
             if (_retriggerArmed) {
                 _gateTicks = _retriggerLength;
                 _trillCvTarget = volts;
             }

        } else {
             // Ghost note / Rest
             // Update CV anyway if Free mode?
             // BUT: Don't update on explicitly muted steps (velocity=0 from polyrhythm suppression)
             if (sequence.cvUpdateMode() == TuesdaySequence::Free && result.velocity > 0) {
                  float volts = scaleToVolts(result.note, result.octave);
                  _cvCurrent = volts;
                  _cvOutput = volts;
             }
        }
        
        return TickResult::CvUpdate | TickResult::GateUpdate;
    }

    return TickResult::NoUpdate;
}

void TuesdayTrackEngine::update(float dt) {
    // CV Slide Update
    if (_slideCountDown > 0) {
        _slideCountDown--;
        _cvCurrent += _cvDelta;
        if (!_retriggerArmed) _cvOutput = _cvCurrent;
    }

    // Retrigger Logic
    if (_retriggerArmed) {
        if (_gateOutput) {
            // Waiting for ON phase to end
            if (_gateTicks > 0) _gateTicks--;
            else {
                _gateOutput = false; // Gap
                _gateTicks = _retriggerPeriod - _retriggerLength; // Gap length
            }
        } else {
            // Waiting for OFF phase to end (Gap)
            if (_gateTicks > 0) _gateTicks--;
            else {
                if (_retriggerCount > 0) {
                    _retriggerCount--;
                    _gateOutput = true;
                    _gateTicks = _retriggerLength;
                    
                    // Apply Trill Interval
                    if (_ratchetInterval != 0) {
                        // Arp/Step mode
                         // We need to know current note to offset it.
                         // This is tricky without persistent note state.
                         // Simplification: just add voltage
                         _cvOutput += (_ratchetInterval / 12.f);
                    } else {
                        // Standard Trill: Toggle
                        // Need to know interval (e.g. +2 semitones)
                        // For now, just keep same voltage (Ratchet) or simple offset
                        // Original code added +2/12v.
                         _cvOutput += (2.f / 12.f); 
                    }
                } else {
                    _retriggerArmed = false;
                }
            }
        }
    }
    
    _activity = _gateOutput || _retriggerArmed;
}