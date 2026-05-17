#include "TuesdayAlgoCore.h"

#include "core/math/Math.h"

#include <cstring>

TuesdayAlgoCore::TuesdayAlgoCore() {
    _cachedFlow = -1;
    _cachedOrnament = -1;
}

void TuesdayAlgoCore::init(const AlgoParams &params) {
    _params = params;

    int flow = params.flow;
    int ornament = params.ornament;
    int algorithm = params.algorithm;
    uint32_t seed = params.seed;

    uint32_t flowSeed = ((flow - 1) << 4) ^ ((seed >> 0) & 0xFF);
    uint32_t ornamentSeed = ((ornament - 1) << 4) ^ ((seed >> 8) & 0xFF);

    _cachedFlow = flow;
    _cachedOrnament = ornament;

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

    case 3: // MARKOV
        _rng = Random(flowSeed);
        _algoState.markov.history1 = (_rng.next() & 0x7);
        _algoState.markov.history3 = (_rng.next() & 0x7);
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                _algoState.markov.matrix[i][j][0] = (_rng.next() % 8);
                _algoState.markov.matrix[i][j][1] = (_rng.next() % 8);
            }
        }
        break;

    case 4: // CHIPARP 1
        _rng = Random(flowSeed);
        _algoState.chiparp1.chordSeed = _rng.next();
        _algoState.chiparp1.rngSeed = _algoState.chiparp1.chordSeed;
        _algoState.chiparp1.base = _rng.next() % 3;
        _algoState.chiparp1.dir = (_rng.next() >> 7) % 2;
        break;

    case 5: // CHIPARP 2
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

    case 6: // WOBBLE
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _algoState.wobble.phase = 0;
        _algoState.wobble.phaseSpeed = 0x08000000;
        _algoState.wobble.phase2 = 0;
        _algoState.wobble.lastWasHigh = 0;
        _algoState.wobble.phaseSpeed2 = 0x02000000;
        break;

    case 7: // SCALEWALKER
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);
        _algoState.scalewalker.pos = 0;
        break;

    case 8: // WINDOW
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _algoState.window.slowPhase = _rng.next() << 16;
        _algoState.window.fastPhase = _rng.next() << 16;
        _algoState.window.noteMemory = _rng.next() & 0x7;
        _algoState.window.noteHistory = _rng.next() & 0x7;
        _algoState.window.ghostThreshold = _rng.next() & 0x1f;
        _algoState.window.phaseRatio = 3 + (_rng.next() & 0x3);
        break;

    case 9: // MINIMAL
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _algoState.minimal.burstLength = 2 + (_rng.next() % 7);
        _algoState.minimal.silenceLength = 4 + (flow % 13);
        _algoState.minimal.clickDensity = ornament * 16;

        _algoState.minimal.mode = 0;
        _algoState.minimal.silenceTimer = _algoState.minimal.silenceLength;
        _algoState.minimal.burstTimer = 0;
        _algoState.minimal.noteIndex = 0;
        break;

    case 10: // GANZ
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _algoState.ganz.phaseA = _rng.next() << 16;
        _algoState.ganz.phaseB = _rng.next() << 16;
        _algoState.ganz.phaseC = _rng.next() << 16;

        for (int i = 0; i < 3; i++) {
            _algoState.ganz.noteHistory[i] = _rng.next() % 7;
        }

        _algoState.ganz.selectMode = _rng.next() % 4;
        _algoState.ganz.skipDecimator = flow >> 2;
        _algoState.ganz.phraseSkipCount = 0;
        _algoState.ganz.velocitySample = 128 + (_extraRng.next() & 0x7F);
        break;

    case 11: // BLAKE
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        for (int i = 0; i < 4; i++) {
            _algoState.blake.motif[i] = _rng.next() % 7;
        }

        _algoState.blake.breathPhase = _rng.next() << 16;
        _algoState.blake.breathPattern = (flow >> 2) % 4;
        _algoState.blake.breathCycleLength = 4 + ((ornament >> 2) % 4);
        _algoState.blake.subBassCountdown = 0;
        break;

    case 12: // APHEX
        _rng = Random(flowSeed);
        for (int i = 0; i < 4; ++i) _algoState.aphex.track1_pattern[i] = _rng.next() % 12;
        for (int i = 0; i < 3; ++i) _algoState.aphex.track2_pattern[i] = _rng.next() % 3;
        for (int i = 0; i < 5; ++i) _algoState.aphex.track3_pattern[i] = (_rng.next() % 8 == 0) ? (_rng.next() % 5) : 0;

        _algoState.aphex.pos1 = (ornament * 1) % 4;
        _algoState.aphex.pos2 = (ornament * 2) % 3;
        _algoState.aphex.pos3 = (ornament * 3) % 5;
        break;

    case 13: { // AUTECHRE
        _rng = Random(flowSeed);
        for (int i = 0; i < 8; ++i) {
            int r = _rng.next() % 4;
            if (r == 0) _algoState.autechre.pattern[i] = 12;
            else if (r == 1) _algoState.autechre.pattern[i] = 24;
            else _algoState.autechre.pattern[i] = 0;

            if (_rng.nextBinary()) _algoState.autechre.pattern[i] += (_rng.next() % 5) * 2;
        }
        _algoState.autechre.rule_timer = 8 + (flow * 4);

        uint32_t tempSeed = _extraRng.next();
        Random tempRng(tempSeed);
        for (int i = 0; i < 8; ++i) _algoState.autechre.rule_sequence[i] = tempRng.next() % 5;
        _algoState.autechre.rule_index = 0;
        break;
    }

    case 14: { // STEPWAVE
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _algoState.stepwave.direction = 0;
        _algoState.stepwave.step_count = 3 + (_rng.next() % 5);
        _algoState.stepwave.current_step = 0;
        _algoState.stepwave.chromatic_offset = 0;
        _algoState.stepwave.is_stepped = true;
        break;
    }

    default: {
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
}

void TuesdayAlgoCore::reseed() {
    uint32_t newSeed1 = _rng.next();
    uint32_t newSeed2 = _extraRng.next();
    _rng = Random(newSeed1);
    _extraRng = Random(newSeed2);

    int algorithm = _params.algorithm;
    int flow = _params.flow;
    int ornament = _params.ornament;

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

    case 3: // MARKOV
        _algoState.markov.history1 = (_rng.next() & 0x7);
        _algoState.markov.history3 = (_rng.next() & 0x7);
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                _algoState.markov.matrix[i][j][0] = (_rng.next() % 8);
                _algoState.markov.matrix[i][j][1] = (_rng.next() % 8);
            }
        }
        break;

    case 4: // CHIPARP1
        _algoState.chiparp1.chordSeed = _rng.next();
        _algoState.chiparp1.rngSeed = _algoState.chiparp1.chordSeed;
        _algoState.chiparp1.base = _rng.next() % 3;
        _algoState.chiparp1.dir = (_rng.next() >> 7) % 2;
        break;

    case 5: // CHIPARP2
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

    case 6: // WOBBLE
        _algoState.wobble.phase = 0;
        _algoState.wobble.phaseSpeed = 0x08000000;
        _algoState.wobble.phase2 = 0;
        _algoState.wobble.lastWasHigh = 0;
        _algoState.wobble.phaseSpeed2 = 0x02000000;
        break;

    case 7: // SCALEWALKER
        _algoState.scalewalker.pos = 0;
        break;

    case 8: // WINDOW
        _algoState.window.slowPhase = _rng.next() << 16;
        _algoState.window.fastPhase = _rng.next() << 16;
        _algoState.window.noteMemory = _rng.next() & 0x7;
        _algoState.window.noteHistory = _rng.next() & 0x7;
        _algoState.window.ghostThreshold = _rng.next() & 0x1f;
        _algoState.window.phaseRatio = 3 + (_rng.next() & 0x3);
        break;

    case 9: // MINIMAL
        _algoState.minimal.burstLength = 2 + (_rng.next() % 7);
        _algoState.minimal.silenceLength = 4 + (_cachedFlow % 13);
        _algoState.minimal.clickDensity = _cachedOrnament * 16;
        _algoState.minimal.mode = 0;
        _algoState.minimal.silenceTimer = _algoState.minimal.silenceLength;
        _algoState.minimal.burstTimer = 0;
        _algoState.minimal.noteIndex = 0;
        break;

    case 10: // GANZ
        _algoState.ganz.phaseA = _rng.next() << 16;
        _algoState.ganz.phaseB = _rng.next() << 16;
        _algoState.ganz.phaseC = _rng.next() << 16;
        for (int i = 0; i < 3; i++) {
            _algoState.ganz.noteHistory[i] = _rng.next() % 7;
        }
        _algoState.ganz.selectMode = _rng.next() % 4;
        _algoState.ganz.skipDecimator = _cachedFlow >> 2;
        _algoState.ganz.phraseSkipCount = 0;
        _algoState.ganz.velocitySample = 128 + (_extraRng.next() & 0x7F);
        break;

    case 11: // BLAKE
        for (int i = 0; i < 4; i++) {
            _algoState.blake.motif[i] = _rng.next() % 7;
        }
        _algoState.blake.breathPhase = _rng.next() << 16;
        _algoState.blake.breathPattern = (_cachedFlow >> 2) % 4;
        _algoState.blake.breathCycleLength = 4 + ((_cachedOrnament >> 2) % 4);
        _algoState.blake.subBassCountdown = 0;
        break;

    case 12: // APHEX
        for (int i = 0; i < 4; ++i) _algoState.aphex.track1_pattern[i] = _rng.next() % 12;
        for (int i = 0; i < 3; ++i) _algoState.aphex.track2_pattern[i] = _rng.next() % 3;
        for (int i = 0; i < 5; ++i) _algoState.aphex.track3_pattern[i] = (_rng.next() % 8 == 0) ? (_rng.next() % 5) : 0;
        _algoState.aphex.pos1 = (ornament * 1) % 4;
        _algoState.aphex.pos2 = (ornament * 2) % 3;
        _algoState.aphex.pos3 = (ornament * 3) % 5;
        break;

    case 13: { // AUTECHRE
        for (int i = 0; i < 8; ++i) {
            int r = _rng.next() % 4;
            if (r == 0) _algoState.autechre.pattern[i] = 12;
            else if (r == 1) _algoState.autechre.pattern[i] = 24;
            else _algoState.autechre.pattern[i] = 0;
            if (_rng.nextBinary()) _algoState.autechre.pattern[i] += (_rng.next() % 5) * 2;
        }
        _algoState.autechre.rule_timer = 8 + (flow * 4);
        uint32_t tempSeed = _extraRng.next();
        Random tempRng(tempSeed);
        for (int i = 0; i < 8; ++i) _algoState.autechre.rule_sequence[i] = tempRng.next() % 5;
        _algoState.autechre.rule_index = 0;
        break;
    }

    case 14: { // STEPWAVE
        _algoState.stepwave.direction = 0;
        _algoState.stepwave.step_count = 3 + (_rng.next() % 5);
        _algoState.stepwave.current_step = 0;
        _algoState.stepwave.chromatic_offset = 0;
        _algoState.stepwave.is_stepped = true;
        break;
    }
    }
}

AlgoResult TuesdayAlgoCore::generate(int algorithm, const AlgoContext &ctx) {
    switch (algorithm) {
    case 0:  return generateTest(ctx);
    case 1:  return generateTritrance(ctx);
    case 2:  return generateStomper(ctx);
    case 3:  return generateMarkov(ctx);
    case 4:  return generateChipArp1(ctx);
    case 5:  return generateChipArp2(ctx);
    case 6:  return generateWobble(ctx);
    case 7:  return generateScalewalker(ctx);
    case 8:  return generateWindow(ctx);
    case 9:  return generateMinimal(ctx);
    case 10: return generateGanz(ctx);
    case 11: return generateBlake(ctx);
    case 12: return generateAphex(ctx);
    case 13: return generateAutechre(ctx);
    case 14: return generateStepwave(ctx);
    default: return generateTest(ctx);
    }
}

// --- Algorithm generators (adapted from TuesdayTrackEngine) ---

AlgoResult TuesdayAlgoCore::generateTest(const AlgoContext &ctx) {
    AlgoResult result;
    result.velocity = 255;
    result.gateRatio = 75;

    if (int(_rng.nextRange(100)) < _params.glide) {
        result.slide = true;
    }

    switch (_algoState.test.mode) {
    case 0: // OCTSWEEPS
        result.octave = (ctx.stepIndex % 5);
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

AlgoResult TuesdayAlgoCore::generateTritrance(const AlgoContext &ctx) {
    AlgoResult result;
    result.velocity = 255;

    int gateLenRnd = _rng.nextRange(100);
    if (gateLenRnd < 40) result.gateRatio = 50 + (_rng.nextRange(4) * 12);
    else if (gateLenRnd < 70) result.gateRatio = 100 + (_rng.nextRange(4) * 25);
    else result.gateRatio = 200 + (_rng.nextRange(9) * 25);

    if (int(_rng.nextRange(100)) < _params.glide) {
        result.slide = true;
    }

    int phase = (ctx.stepIndex + _algoState.tritrance.b2) % 3;
    switch (phase) {
    case 0:
        if (_extraRng.nextBinary() && _extraRng.nextBinary()) {
            _algoState.tritrance.b3 = (_extraRng.next() & 0x15);
            if (_algoState.tritrance.b3 >= 7) _algoState.tritrance.b3 -= 7; else _algoState.tritrance.b3 = 0;
            _algoState.tritrance.b3 -= 4;
        }
        result.octave = 0;
        result.note = _algoState.tritrance.b3 + 4;
        result.gateOffset = clamp(int(10 - _rng.nextRange(10)), 0, 100);
        break;
    case 1:
        result.octave = 1;
        result.note = _algoState.tritrance.b3 + 4;
        if (_rng.nextBinary()) _algoState.tritrance.b2 = (_rng.next() & 0x7);
        result.gateOffset = clamp(int(25 + _rng.nextRange(10)), 0, 100);
        break;
    case 2:
        result.octave = 2;
        result.note = _algoState.tritrance.b1;
        result.velocity = 255;
        result.accent = true;
        if (_rng.nextBinary()) _algoState.tritrance.b1 = ((_rng.next() >> 5) & 0x7);
        result.gateOffset = clamp(int(40 + _rng.nextRange(10)), 0, 100);
        result.trillCount = 3;
        result.noteOffsets[0] = -2;
        result.noteOffsets[1] = -1;
        result.noteOffsets[2] = 0;
        break;
    }

    if (!result.accent) {
        result.velocity = (_rng.nextRange(256) / 2);
    }

    return result;
}

AlgoResult TuesdayAlgoCore::generateMarkov(const AlgoContext &ctx) {
    (void)ctx;
    AlgoResult result;
    result.velocity = 255;
    result.gateRatio = 75;

    int idx = _rng.nextBinary() ? 1 : 0;
    int newNote = _algoState.markov.matrix[_algoState.markov.history1][_algoState.markov.history3][idx];

    _algoState.markov.history1 = _algoState.markov.history3;
    _algoState.markov.history3 = newNote;

    result.note = newNote;
    result.octave = _rng.nextBinary();

    if (_rng.nextRange(100) < 50) result.gateRatio = 100 + (_rng.nextRange(4) * 25);
    else result.gateRatio = 75;

    if (_rng.nextBinary() && _rng.nextBinary()) result.slide = true;

    result.accent = true;

    result.velocity = (_rng.nextRange(256) / 2) + 40;

    if (_rng.nextBinary()) {
        result.gateOffset = clamp(int(10 - _rng.nextRange(10)), 0, 100);
    } else {
        result.gateOffset = clamp(int(15 + _rng.nextRange(10)), 0, 100);
    }

    if (_rng.nextRange(256) < 20) {
        result.trillCount = 2;
        result.noteOffsets[0] = 0;
        result.noteOffsets[1] = 0;
        result.gateRatio = 50;
    }

    return result;
}

AlgoResult TuesdayAlgoCore::generateWobble(const AlgoContext &ctx) {
    AlgoResult result;
    result.velocity = 255;

    _algoState.wobble.phaseSpeed = 0xFFFFFFFF / std::max(1, ctx.effectiveLoopLength);
    _algoState.wobble.phaseSpeed2 = 0xCFFFFFFF / std::max(1, ctx.effectiveLoopLength / 4);

    _algoState.wobble.phase += _algoState.wobble.phaseSpeed;
    _algoState.wobble.phase2 += _algoState.wobble.phaseSpeed2;

    if (int(_rng.nextRange(256)) >= (_params.ornament * 16)) {
        int rawPhase = (_algoState.wobble.phase2 >> 27) & 0x1F;
        result.note = rawPhase % 7;
        result.octave = 1 + (rawPhase / 7);

        if (_algoState.wobble.lastWasHigh == 0) {
            if (_rng.nextRange(256) >= 200) result.slide = true;
        }
        _algoState.wobble.lastWasHigh = 1;
    } else {
        int rawPhase = (_algoState.wobble.phase >> 27) & 0x1F;
        result.note = rawPhase % 7;
        result.octave = (rawPhase / 7);

        if (_algoState.wobble.lastWasHigh == 1) {
            if (_rng.nextRange(256) >= 200) result.slide = true;
        }
        _algoState.wobble.lastWasHigh = 0;
    }

    result.velocity = (_extraRng.nextRange(256) / 4);
    if (_rng.nextRange(256) >= 50) result.accent = true;

    result.gateOffset = clamp(int(((_algoState.wobble.phase + _algoState.wobble.phase2) >> 28) % 30), 0, 100);

    return result;
}

AlgoResult TuesdayAlgoCore::generateStomper(const AlgoContext &ctx) {
    (void)ctx;
    AlgoResult result;
    result.velocity = 255;
    result.gateRatio = 75;

    uint8_t veloffset = 0;

    if (_algoState.stomper.countDown > 0) {
        result.gateRatio = _algoState.stomper.countDown * 25;
        result.velocity = 0;
        _algoState.stomper.countDown--;
        result.note = _algoState.stomper.lastNote;
        result.octave = _algoState.stomper.lastOctave;
    } else {
        if (_algoState.stomper.mode >= 14) {
            _algoState.stomper.mode = (_extraRng.next() % 7) * 2;
        }

        _algoState.stomper.lowNote = _rng.next() % 3;
        _algoState.stomper.highNote[0] = _rng.next() % 7;
        _algoState.stomper.highNote[1] = _rng.next() % 5;

        veloffset = 100;
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
            result.octave = 0;
            result.note = _algoState.stomper.lowNote;
            if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
            _algoState.stomper.mode = 14;
            break;
        case 0: case 1: // PAUSE
            result.octave = _algoState.stomper.lastOctave;
            result.note = _algoState.stomper.lastNote;
            veloffset = 0;
            if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
            _algoState.stomper.mode = 14;
            break;
        case 6: case 7: // HIGH
        case 8: case 9: // HILOW
        case 2: case 3: // LOWHI
            if (_algoState.stomper.mode == 8 || _algoState.stomper.mode == 3) {
                result.octave = 1;
                result.note = _algoState.stomper.highNote[_rng.next() % 2];
                if (_algoState.stomper.mode == 8) _algoState.stomper.mode++;
                else if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
                if (_algoState.stomper.mode == 3) _algoState.stomper.mode = 14;
            } else {
                result.octave = 0;
                result.note = _algoState.stomper.lowNote;
                if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen;
                _algoState.stomper.mode = 14;
            }
            break;
        }
        _algoState.stomper.lastNote = result.note;
        _algoState.stomper.lastOctave = result.octave;

        result.velocity = (_extraRng.nextRange(256) / 4) + veloffset;
        if (int(_rng.nextRange(256)) >= 50) {
            result.accent = true;
        }

        result.beatSpread = 18;
    }

    return result;
}

AlgoResult TuesdayAlgoCore::generateChipArp1(const AlgoContext &ctx) {
    AlgoResult result;
    result.velocity = 255;
    result.gateRatio = 75;

    int chordpos = ctx.stepIndex % ctx.tpb;

    if (chordpos == 0) {
        result.trillCount = 3;
        result.noteOffsets[0] = 0;
        result.noteOffsets[1] = 2;
        result.noteOffsets[2] = 4;
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
        result.velocity = 0;
    } else {
        result.note = (chordpos * 2) + _algoState.chiparp1.base;
        result.octave = 0;
        int rnd = (chipRng.next() >> 7) % 3;
        result.gateRatio = (4 + 6 * rnd) * (100/6);
    }

    int extraVel = (ctx.rotatedStep == 0) ? 127 : 0;
    result.velocity = (_rng.nextRange(256) / 2) + extraVel;

    result.gateOffset = clamp(int(ctx.stepIndex % 7), 0, 100);

    return result;
}

AlgoResult TuesdayAlgoCore::generateChipArp2(const AlgoContext &ctx) {
    (void)ctx;
    AlgoResult result;
    result.velocity = 255;
    result.gateRatio = 75;
    result.octave = 0;

    if (_algoState.chiparp2.deadTime > 0) {
        _algoState.chiparp2.deadTime--;
        result.velocity = 0;
    } else {
        int deadTimeAdd = 0;
        if (_algoState.chiparp2.idx == _algoState.chiparp2.chordLen) {
            _algoState.chiparp2.idx = 0;
            _algoState.chiparp2.len--;
            result.accent = true;

            result.trillCount = 4;
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

AlgoResult TuesdayAlgoCore::generateAphex(const AlgoContext &ctx) {
    AlgoResult result;
    result.velocity = 255;

    uint32_t modifierStep = ctx.stepIndex;
    if (ctx.subdivisions != 4) {
        modifierStep = (ctx.stepIndex * ctx.subdivisions) / 4;
    }

    int pos1 = ctx.rotatedStep % 4;
    int pos2 = modifierStep % 3;
    int pos3 = ctx.rotatedStep % 5;

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

    int polyCollision = ((pos1 * 7) ^ (pos2 * 5) ^ (pos3 * 3)) % 12;
    if (polyCollision > 9) {
        result.beatSpread = 100;
        result.velocity = 255;
    }

    int polyRhythmFactor = (pos1 + pos2 + pos3) % 12;
    result.gateOffset = clamp(polyRhythmFactor * 8, 0, 100);

    return result;
}

AlgoResult TuesdayAlgoCore::generateAutechre(const AlgoContext &ctx) {
    AlgoResult result;
    result.velocity = 255;

    int flow = _params.flow;

    if (ctx.isBeatStart && ctx.subdivisions != 4) {
        result.beatSpread = 20 + (flow * 5);
        result.polyCount = ctx.subdivisions;
        result.isSpatial = true;

        for (int i = 0; i < ctx.subdivisions && i < 8; i++) {
            int patternIdx = (ctx.rotatedStep + i) % 8;
            int patternNote = _algoState.autechre.pattern[patternIdx] % 12;
            int baseNote = _algoState.autechre.pattern[ctx.rotatedStep % 8] % 12;
            result.noteOffsets[i] = patternNote - baseNote;
        }
    }

    int patternVal = _algoState.autechre.pattern[ctx.rotatedStep % 8];
    result.note = patternVal % 12;
    result.octave = patternVal / 12;

    result.gateOffset = clamp(int(_rng.nextRange(11)), 0, 100);

    _algoState.autechre.rule_timer--;

    result.velocity = 160;
    result.gateRatio = 75;

    if (_algoState.autechre.rule_timer <= 0) {
        uint8_t rule = _algoState.autechre.rule_sequence[_algoState.autechre.rule_index];
        int intensity = _params.power / 2;
        if (result.velocity > 0) {
            result.velocity = 255;
            result.accent = true;
        }
        if (rule == 0) { int8_t t = _algoState.autechre.pattern[7]; for(int i=7; i>0; --i) _algoState.autechre.pattern[i]=_algoState.autechre.pattern[i-1]; _algoState.autechre.pattern[0]=t; }
        else if (rule == 1) { for(int i=0;i<4;++i) std::swap(_algoState.autechre.pattern[i], _algoState.autechre.pattern[7-i]); }
        else if (rule == 2) { for(int i=0;i<8;++i) { int o=_algoState.autechre.pattern[i]/12; int n=_algoState.autechre.pattern[i]%12; n=(6-(n-6)+12)%12; _algoState.autechre.pattern[i]=n+o*12; } }
        else if (rule == 3) { for(int i=0;i<8;i+=2) std::swap(_algoState.autechre.pattern[i], _algoState.autechre.pattern[i+1]); }
        else if (rule == 4) { for(int i=0;i<8;++i) { int o=_algoState.autechre.pattern[i]/12; int n=(_algoState.autechre.pattern[i]%12+intensity)%12; _algoState.autechre.pattern[i]=n+o*12; } }

        _algoState.autechre.rule_timer = 8 + (_cachedFlow * 4);
        _algoState.autechre.rule_index = (_algoState.autechre.rule_index + 1) % 8;
    }

    return result;
}

AlgoResult TuesdayAlgoCore::generateStepwave(const AlgoContext &ctx) {
    AlgoResult result;
    result.velocity = 255;

    int flow = _params.flow;
    int ornament = _params.ornament;

    if (ctx.isBeatStart && ctx.subdivisions != 4) {
        result.beatSpread = 20 + (flow * 5);
        result.polyCount = ctx.subdivisions;

        result.isSpatial = (ornament >= 9);

        int downwardProbability = (16 - flow) * 6;
        downwardProbability = clamp(downwardProbability, 10, 90);

        if (int(_rng.nextRange(100)) < downwardProbability) {
            _algoState.stepwave.direction = -1;
        } else {
            _algoState.stepwave.direction = 1;
        }

        _algoState.stepwave.step_count = ctx.subdivisions;

        for (int i = 0; i < ctx.subdivisions && i < 8; i++) {
            result.noteOffsets[i] = i * _algoState.stepwave.direction;
        }

        result.slide = true;
    }

    int dir;
    int downwardProbability = (16 - flow) * 6;
    downwardProbability = clamp(downwardProbability, 10, 90);

    if (int(_rng.nextRange(100)) < downwardProbability) {
        dir = -1;
    } else {
        dir = 1;
    }

    int stepSize = 2 + (ctx.stepIndex % 2);

    result.note = (ctx.stepIndex * dir * stepSize) % 7;
    if (result.note < 0) result.note += 7;

    result.velocity = 200;
    if (int(_rng.nextRange(100)) < (20 + flow * 3)) {
        result.octave = 2;
        result.velocity = 255;
        result.accent = true;
    } else {
        result.octave = 0;
    }

    if (int(_rng.nextRange(100)) < _params.glide) {
        result.slide = true;
        result.gateRatio = 100;
    } else {
        result.gateRatio = 75;
    }

    result.gateOffset = clamp(int(_rng.nextRange(7)), 0, 100);

    return result;
}

AlgoResult TuesdayAlgoCore::generateScalewalker(const AlgoContext &ctx) {
    AlgoResult result;
    result.velocity = 255;

    int flow = _params.flow;

    int direction = (flow <= 7) ? -1 : 1;

    result.note = _algoState.scalewalker.pos;
    result.octave = 0;
    result.velocity = 100 + (_params.power * 10);
    result.gateRatio = 75;

    bool shouldFirePolyGate = false;
    if (ctx.subdivisions != 4) {
        int beatPosition = ctx.stepIndex % ctx.stepsPerBeat;
        int gateIndex = (beatPosition * ctx.subdivisions) / ctx.stepsPerBeat;
        int nextGateIndex = ((beatPosition + 1) * ctx.subdivisions) / ctx.stepsPerBeat;

        if (gateIndex != nextGateIndex) {
            shouldFirePolyGate = true;
            result.polyCount = 1;
            result.beatSpread = 100;
            result.isSpatial = true;
            result.slide = true;

            result.noteOffsets[0] = gateIndex * direction;
        }
    }

    int stepTrillCount = 1;
    if (_params.stepTrill > 0) {
        stepTrillCount = 1 + (_params.stepTrill * 3) / 100;
        stepTrillCount = clamp(stepTrillCount, 1, 4);
    }

    if (shouldFirePolyGate) {
        if (stepTrillCount > 1) {
            int baseOffset = result.noteOffsets[0];
            result.polyCount = stepTrillCount;
            result.beatSpread = 0;
            result.isSpatial = false;

            for (int i = 0; i < stepTrillCount && i < 8; i++) {
                result.noteOffsets[i] = baseOffset + (i * direction);
            }

            result.slide = _params.glide > 50;
        }
    } else if (stepTrillCount > 1) {
        result.polyCount = stepTrillCount;
        result.beatSpread = 0;
        result.isSpatial = false;

        for (int i = 0; i < stepTrillCount && i < 8; i++) {
            result.noteOffsets[i] = i * direction;
        }

        result.slide = _params.glide > 50;
    } else if (ctx.subdivisions == 4) {
        result.polyCount = 0;
        result.beatSpread = 0;
        result.isSpatial = false;
        result.noteOffsets[0] = 0;
    } else {
        result.polyCount = 0;
        result.velocity = 0;
    }

    _algoState.scalewalker.pos = (_algoState.scalewalker.pos + direction + 7) % 7;

    return result;
}

AlgoResult TuesdayAlgoCore::generateMinimal(const AlgoContext &ctx) {
    (void)ctx;
    AlgoResult result;
    auto &state = _algoState.minimal;

    if (state.mode == 0) {
        if (state.silenceTimer > 0) {
            state.silenceTimer--;
        }

        if (state.silenceTimer == 0) {
            state.mode = 1;
            state.burstTimer = state.burstLength;
            state.burstLength = 2 + (_rng.next() % 7);
        }
    } else {
        if (state.burstTimer > 0) {
            state.burstTimer--;
        }

        if (state.burstTimer == 0) {
            state.mode = 0;
            state.silenceTimer = state.silenceLength;
            state.silenceLength = 4 + (_params.flow % 13);
        }
    }

    if (state.mode == 0) {
        result.velocity = 0;
        result.note = 0;
        result.octave = 0;
        result.gateRatio = 25;
    } else {
        bool shouldPlay = true;
        if (state.clickDensity > 0) {
            uint8_t rng = _rng.next() & 0xFF;
            shouldPlay = (rng < state.clickDensity);
        }

        if (shouldPlay) {
            result.note = state.noteIndex % 7;
            result.octave = (state.noteIndex / 7) % 3;

            result.velocity = 128 + (_extraRng.next() & 0x7F);

            int gatePercent = 25 + (_params.gateLength / 4);
            result.gateRatio = clamp(gatePercent, 25, 50);

            int glideProb = _params.glide / 5;
            int trillProb = _params.stepTrill / 10;

            result.slide = int(_extraRng.next() % 100) < glideProb;

            if (int(_extraRng.next() % 100) < trillProb) {
                result.trillCount = 2 + (_extraRng.next() % 3);
            } else {
                result.trillCount = 1;
            }

            result.gateOffset = _extraRng.next() % 6;

            state.noteIndex = (state.noteIndex + 1) % 21;
        } else {
            result.velocity = 0;
            result.note = 0;
            result.octave = 0;
            result.gateRatio = 25;
        }
    }

    return result;
}

AlgoResult TuesdayAlgoCore::generateWindow(const AlgoContext &ctx) {
    AlgoResult result;
    auto &state = _algoState.window;

    uint32_t slowIncrement = (ctx.effectiveLoopLength > 0)
        ? (0xFFFFFFFF / ctx.effectiveLoopLength)
        : 0x08000000;

    uint32_t fastDivisor = std::max(1, ctx.effectiveLoopLength / state.phaseRatio);
    uint32_t fastIncrement = (ctx.effectiveLoopLength > 0)
        ? (0xFFFFFFFF / fastDivisor)
        : 0x20000000;

    uint32_t oldSlowPhase = state.slowPhase;
    state.slowPhase += slowIncrement;
    state.fastPhase += fastIncrement;

    if (ctx.stepIndex % 96 == 0 && ctx.stepIndex > 0) {
        state.phaseRatio = 3 + (_rng.next() & 0x3);
    }
    if (ctx.stepIndex % 64 == 0 && ctx.stepIndex > 0) {
        state.ghostThreshold = _rng.next() & 0x1f;
    }

    bool isAccent = (state.slowPhase < oldSlowPhase);
    bool isGhost = ((state.fastPhase >> 27) & 0x1F) > state.ghostThreshold;
    bool voiceA = (state.fastPhase >> 28) & 1;

    if (isAccent) {
        result.note = state.noteMemory;
        result.octave = 2 + (_extraRng.next() % 2);
        result.velocity = 100 + (_extraRng.next() & 0x3F);
        result.gateRatio = 150 + (_extraRng.next() % 26);
        result.slide = true;
    } else if (isGhost) {
        result.note = (state.fastPhase >> 25) & 0x7;
        result.octave = 0;
        result.velocity = 5 + (_extraRng.next() & 0x1F);
        result.gateRatio = 50 + (_extraRng.next() % 26);
        result.slide = false;
    } else if (voiceA) {
        Random markovRng(_extraRng.next() + state.noteHistory + state.noteMemory);

        int step = (markovRng.next() % 3) - 1;
        int newNote = (state.noteMemory + step + 7) % 7;

        state.noteHistory = state.noteMemory;
        state.noteMemory = newNote;

        result.note = newNote;
        result.octave = 0;
        result.velocity = 40 + (_extraRng.next() % 86);
        result.gateRatio = 75;

        result.slide = (_extraRng.next() % 100) < 25;
    } else {
        result.note = (state.fastPhase >> 25) & 0x7;
        result.octave = 0;
        result.velocity = 40 + (_extraRng.next() % 86);
        result.gateRatio = 75;
        result.slide = false;
    }

    return result;
}

AlgoResult TuesdayAlgoCore::generateBlake(const AlgoContext &ctx) {
    AlgoResult result;
    auto &state = _algoState.blake;

    uint32_t breathIncrement = (ctx.effectiveLoopLength > 0)
        ? (0xFFFFFFFF / (ctx.effectiveLoopLength * 8))
        : 0x02000000;
    state.breathPhase += breathIncrement;

    if (ctx.stepIndex % 96 == 0 && ctx.stepIndex > 0) {
        state.breathPattern = _rng.next() % 4;
    }
    if (ctx.stepIndex % 64 == 0 && ctx.stepIndex > 0) {
        state.breathCycleLength = 4 + (_rng.next() % 4);
    }

    int breathPos = (ctx.stepIndex % state.breathCycleLength);

    enum ArticulationType { REAL, GHOST, WHISPER };
    ArticulationType articulation = REAL;

    switch (state.breathPattern) {
    case 0:
        articulation = (breathPos == 3) ? GHOST : REAL;
        break;
    case 1:
        if (breathPos == 1) articulation = WHISPER;
        else if (breathPos == 3) articulation = GHOST;
        else articulation = REAL;
        break;
    case 2:
        if (breathPos == 0) articulation = REAL;
        else if (breathPos == 2) articulation = WHISPER;
        else articulation = GHOST;
        break;
    case 3:
        if (breathPos < 2) articulation = REAL;
        else if (breathPos == 2) articulation = GHOST;
        else articulation = WHISPER;
        break;
    }

    int motifIdx = ctx.stepIndex % 4;
    result.note = state.motif[motifIdx];
    result.octave = 0;

    if (state.subBassCountdown > 0) {
        state.subBassCountdown--;
        result.octave = -1;
        result.note = 0;
        result.velocity = 255;
        result.accent = true;
        result.gateRatio = 100;
        result.slide = (state.subBassCountdown == 0);
        return result;
    }

    if (ctx.isBeatStart) {
        int dropChance = _params.power * 6;
        if ((_rng.next() % 100) < static_cast<uint32_t>(dropChance)) {
            state.subBassCountdown = 2 + (_rng.next() % 3);
        }
    }

    switch (articulation) {
    case REAL:
        {
            int breathBias = (state.breathPhase >> 24) & 0xFF;
            result.velocity = 128 + (breathBias / 2);
            result.gateRatio = 75;
            if ((_extraRng.next() % 100) < 25) {
                result.octave = 1;
            }
        }
        break;

    case GHOST:
        result.octave = 2;
        result.velocity = 20 + (_extraRng.next() & 0x0F);
        result.gateRatio = 40;
        break;

    case WHISPER:
        result.velocity = 0;
        result.gateRatio = 25;
        break;
    }

    result.gateOffset = _extraRng.next() % 5;

    return result;
}

AlgoResult TuesdayAlgoCore::generateGanz(const AlgoContext &ctx) {
    AlgoResult result;
    auto &state = _algoState.ganz;

    uint32_t incrementA = (ctx.effectiveLoopLength > 0)
        ? (0xFFFFFFFF / ctx.effectiveLoopLength)
        : 0x08000000;

    uint32_t oldPhaseA = state.phaseA;
    state.phaseA += incrementA;
    state.phaseB += (incrementA * 5);
    state.phaseC += (incrementA * 7);

    if (ctx.stepIndex % 96 == 0 && ctx.stepIndex > 0) {
        state.selectMode = _rng.next() % 4;
    }

    int tupletPos = (state.phaseB >> 27) % 5;
    bool shouldPlay = (tupletPos <= 2);

    if (!shouldPlay) {
        result.velocity = 0;
        result.gateRatio = 25;
        return result;
    }

    if (state.phraseSkipCount > 0) {
        state.phraseSkipCount--;
        result.velocity = 0;
        return result;
    }

    if ((state.phaseC >> 28) < (state.skipDecimator * 2)) {
        int skipLength = 1 + (_rng.next() % 4);
        state.phraseSkipCount = skipLength;
        result.velocity = 0;
        return result;
    }

    int selectedNote = 0;

    switch (state.selectMode) {
    case 0: // MARKOV WALK
        {
            int step = (_extraRng.next() % 3) - 1;
            selectedNote = (state.noteHistory[0] + step + 7) % 7;
        }
        break;

    case 1: // PHASOR DIRECT
        selectedNote = (state.phaseA >> 25) % 7;
        break;

    case 2: // HISTORY CYCLE
        selectedNote = state.noteHistory[ctx.stepIndex % 3];
        break;

    case 3: // XOR CHAOS
        {
            int xor_val = ((state.phaseA >> 24) ^ (state.phaseC >> 24)) & 0xFF;
            selectedNote = xor_val % 7;
        }
        break;
    }

    state.noteHistory[2] = state.noteHistory[1];
    state.noteHistory[1] = state.noteHistory[0];
    state.noteHistory[0] = selectedNote;

    result.note = selectedNote;

    result.octave = (state.phaseC >> 29) % 3;

    if (ctx.stepIndex % 8 == 0) {
        state.velocitySample = 64 + (_extraRng.next() & 0xBF);
    }
    result.velocity = state.velocitySample;

    bool isAccent = (state.phaseA < oldPhaseA);

    if (isAccent) {
        result.accent = true;
        result.velocity = 255;
        result.gateRatio = 125;
        result.slide = true;
    } else {
        result.gateRatio = 60 + (tupletPos * 15);
        result.slide = ((_extraRng.next() % 100) < static_cast<uint32_t>(_params.ornament * 6));
    }

    result.gateOffset = ((state.phaseB >> 26) & 0x1F) % 20;

    return result;
}
