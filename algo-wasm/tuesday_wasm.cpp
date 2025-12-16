#include <cstdint>
#include <cstring>
#include <vector>
#include <algorithm>

// Minimal clamp (std::clamp without C++17 <algorithm> dependency on some targets)
template<typename T>
T tClamp(T v, T lo, T hi) {
    return std::max(lo, std::min(hi, v));
}

// Tuesday RNG (matches firmware)
class Random {
public:
    explicit Random(uint32_t seed = 0) : _state(seed) {}

    uint32_t next() {
        _state = _state * 1664525L + 1013904223L;
        return _state;
    }

    bool nextBinary() { return next() < 0x80000000; }
    uint32_t nextRange(uint32_t range) { return next() / (0xffffffff / range); }

private:
    uint32_t _state;
};

// Simplified scale: C major (7-note) only.
struct ScaleMajor {
    static float noteToVolts(int degree) {
        static const int notes[] = {0, 2, 4, 5, 7, 9, 11};
        const int count = 7;
        int octave = 0;
        if (degree < 0) {
            // allow negative degrees
            int o = (-degree + (count - 1)) / count;
            octave = -o;
            degree += o * count;
        }
        octave += degree / count;
        int note = notes[(degree % count + count) % count];
        int semitones = octave * 12 + note;
        return semitones * (1.f / 12.f);
    }
};

// Input parameter bundle (approximated)
struct SequenceParams {
    int algorithm = 0;
    int flow = 8;
    int ornament = 8;
    int power = 16;
    int glide = 0;
    int stepTrill = 0;
    int trill = 0;
    int gateLength = 50; // percent scaler
    int gateOffset = 50; // percent scaler
    int divisor = 24;    // 192 PPQN / 24 = 1/16th step
    int loopLength = 32;
    int rotate = 0;
    int maskParameter = 0;
    int timeMode = 0;
    int maskProgression = 0;
    int scale = 1;   // ignored, locked to major
    int rootNote = 0;
    int transpose = 0;
    int octave = 0;
    int skew = 0;
};

// Exported per-step summary (tick units = PPQN ticks; 192 PPQN baseline).
struct ExportedStep {
    uint32_t stepIndex;
    uint32_t tickOn;
    uint32_t tickOff;
    float cv;
    int note;
    int octave;
    uint8_t velocity;
    uint8_t accent;
    uint8_t slide;
    uint8_t gateRatio;
    uint8_t gateOffset;
    uint8_t polyCount;
    uint8_t microCount;
    uint32_t microTicks[8];
    float microCv[8];
    int8_t noteOffsets[8];
};

// Tuesday simulator (trimmed to algorithm brain + simplified gate logic).
class TuesdaySim {
public:
    TuesdaySim() { reset(); }

    void reset() {
        _params = {};
        _stepIndex = 0;
        _gateTicks = 0;
        _coolDown = 0;
        _microCoolDown = 0;
        memset(&_algoState, 0, sizeof(_algoState));
        initAlgorithm();
    }

    void setParam(int key, int value) {
        switch (key) {
        case 0: _params.algorithm = value; break;
        case 1: _params.flow = value; break;
        case 2: _params.ornament = value; break;
        case 3: _params.power = value; break;
        case 4: _params.glide = value; break;
        case 5: _params.stepTrill = value; break;
        case 6: _params.trill = value; break;
        case 7: _params.gateLength = value; break;
        case 8: _params.gateOffset = value; break;
        case 9: _params.divisor = value; break;
        case 10: _params.loopLength = value; break;
        case 11: _params.rotate = value; break;
        case 12: _params.maskParameter = value; break;
        case 13: _params.timeMode = value; break;
        case 14: _params.maskProgression = value; break;
        case 15: _params.scale = value; break;
        case 16: _params.rootNote = value; break;
        case 17: _params.transpose = value; break;
        case 18: _params.octave = value; break;
        case 19: _params.skew = value; break;
        default: break;
        }
    }

    ExportedStep runOneStep(uint32_t tickBase) {
        // Calculate context
        GenerationContext ctx = calculateContext();

        // Generate algorithm result
        TuesdayTickResult result = generateStep(ctx);

        // Power/cooldown mapping
        int power = tClamp(_params.power, 0, 16);
        int baseCooldown = 17 - power;
        if (baseCooldown < 1) baseCooldown = 1;
        _coolDownMax = baseCooldown;
        _microCoolDownMax = baseCooldown;

        // Decrement cooldowns (once per step)
        if (_coolDown > 0) _coolDown--;
        if (_microCoolDown > 0) _microCoolDown--;

        // Density gate decision
        bool densityGate = false;
        if (result.velocity > 0 && power > 0) {
            if (result.accent) {
                densityGate = true;
            } else if (_coolDown == 0) {
                densityGate = true;
            } else {
                int velDensity = result.velocity / 16;
                densityGate = velDensity >= _coolDown;
            }
        }

        // Reset cooldown when gate fires
        if (densityGate) {
            _coolDown = _coolDownMax;
        }

        // Gate length/offset
        uint32_t gateOffsetTicks = (ctx.divisor * result.gateOffset) / 100;
        // User scaler (0-100) mapped to 0..2x in firmware; we approximate 0..2x
        gateOffsetTicks = (gateOffsetTicks * tClamp(_params.gateOffset, 0, 100)) / 50;

        uint32_t baseLen = (ctx.divisor * result.gateRatio) / 100;
        uint32_t gateLengthTicks = (baseLen * tClamp(_params.gateLength, 1, 100)) / 50;
        if (gateLengthTicks < 1) gateLengthTicks = 1;
        if (result.accent) {
            gateLengthTicks = (gateLengthTicks * 3) / 2;
        }

        ExportedStep out = {};
        out.stepIndex = _stepIndex;
        out.note = result.note;
        out.octave = result.octave;
        out.velocity = result.velocity;
        out.accent = result.accent ? 1 : 0;
        out.slide = result.slide ? 1 : 0;
        out.gateRatio = static_cast<uint8_t>(tClamp<int>(result.gateRatio, 0, 200));
        out.gateOffset = static_cast<uint8_t>(tClamp<int>(result.gateOffset, 0, 100));
        out.polyCount = result.polyCount;
        memcpy(out.noteOffsets, result.noteOffsets, sizeof(out.noteOffsets));

        // Voltage (quantized)
        float volts = scaleToVolts(result.note, result.octave);
        out.cv = volts + _params.octave;

        uint32_t stepTick = tickBase + (_stepIndex * ctx.divisor);
        out.tickOn = stepTick + gateOffsetTicks;
        out.tickOff = out.tickOn + gateLengthTicks;

        if (densityGate) {
            if (result.polyCount > 0) {
                int tupleN = result.polyCount;
                uint32_t spacing = ctx.divisor / tupleN;
                int velDensity = result.velocity / 16;

                for (int i = 0; i < tupleN && i < 8; i++) {
                    bool microAllowed = false;
                    if (_microCoolDown == 0) {
                        microAllowed = true;
                        _microCoolDown = _microCoolDownMax;
                    } else if (velDensity >= _microCoolDown * 2) {
                        microAllowed = true;
                        _microCoolDown = _microCoolDownMax;
                    }
                    if (microAllowed) {
                        uint32_t offset = i * spacing;
                        out.microTicks[out.microCount] = stepTick + offset;
                        int noteWithOffset = result.note + result.noteOffsets[i];
                        out.microCv[out.microCount] = scaleToVolts(noteWithOffset, result.octave) + _params.octave;
                        out.microCount++;
                    }
                }
            }
        }

        _stepIndex = (_stepIndex + 1) % std::max(1, _params.loopLength);
        return out;
    }

private:
    struct GenerationContext {
        uint32_t divisor;
        int tpb, loopLength, effectiveLoopLength, rotatedStep;
        int ornament, subdivisions, stepsPerBeat;
        bool isBeatStart;
    };

    struct TuesdayTickResult {
        int note = 0;
        int octave = 0;
        uint8_t velocity = 0;
        bool accent = false;
        bool slide = false;
        uint16_t gateRatio = 75;
        uint8_t gateOffset = 0;
        uint8_t trillCount = 1;
        uint8_t beatSpread = 0;
        uint8_t polyCount = 0;
        int8_t noteOffsets[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        bool isSpatial = true;
    };

    struct MicroGate { uint32_t tick; bool gate; float cvTarget; };

    // Algorithm state structs (copied from firmware)
    struct TestState { uint8_t mode, sweepSpeed, accent, velocity; int16_t note; };
    struct TritranceState { int b1, b2, b3; };
    struct StomperState { int mode, countDown, lowNote, lastNote, lastOctave, highNote[2]; };
    struct AphexState { int track1_pattern[4], track2_pattern[3], track3_pattern[5]; int pos1, pos2, pos3; };
    struct AutechreState { int pattern[8], rule_timer, rule_sequence[8], rule_index; };
    struct StepwaveState { int direction, step_count, current_step, chromatic_offset; bool is_stepped; };
    struct MarkovState { int history1, history3, matrix[8][8][2]; };
    struct ChipArp1State { int chordSeed, rngSeed, base, dir; };
    struct ChipArp2State { int rngSeed, chordScaler, offset, len, timeMult, deadTime, idx, dir, chordLen; };
    struct WobbleState { uint32_t phase, phaseSpeed, phase2, phaseSpeed2; int lastWasHigh; };
    struct ScalewalkerState { int pos; };
    struct WindowState { uint32_t slowPhase, fastPhase; int noteMemory, noteHistory, ghostThreshold, phaseRatio; };
    struct MinimalState { int burstLength, silenceLength, clickDensity, mode, silenceTimer, burstTimer, noteIndex; };
    struct BlakeState { int motif[4]; uint32_t breathPhase; int breathPattern, breathCycleLength, subBassCountdown; };
    struct GanzState { uint32_t phaseA, phaseB, phaseC; int noteHistory[3]; int selectMode; int skipDecimator; int phraseSkipCount; int velocitySample; };

    union AlgoState {
        TestState test;
        TritranceState tritrance;
        StomperState stomper;
        AphexState aphex;
        AutechreState autechre;
        StepwaveState stepwave;
        MarkovState markov;
        ChipArp1State chiparp1;
        ChipArp2State chiparp2;
        WobbleState wobble;
        ScalewalkerState scalewalker;
        WindowState window;
        MinimalState minimal;
        BlakeState blake;
        GanzState ganz;
    } _algoState;

    SequenceParams _params;
    Random _rng;
    Random _extraRng;
    Random _uiRng;
    int _stepIndex = 0;
    uint32_t _gateTicks = 0;
    int _coolDown = 0;
    int _coolDownMax = 0;
    int _microCoolDown = 0;
    int _microCoolDownMax = 0;

    void initAlgorithm() {
        int flow = _params.flow;
        int ornament = _params.ornament;
        int algorithm = _params.algorithm;

        uint32_t flowSeed = (flow - 1) << 4;
        uint32_t ornamentSeed = (ornament - 1) << 4;

        _uiRng = Random(flow * 37 + ornament * 101);

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
        case 6: // MARKOV
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
            _rng = Random(flowSeed);
            _extraRng = Random(ornamentSeed + 0x9e3779b9);
            _algoState.wobble.phase = 0;
            _algoState.wobble.phaseSpeed = 0x08000000;
            _algoState.wobble.phase2 = 0;
            _algoState.wobble.lastWasHigh = 0;
            _algoState.wobble.phaseSpeed2 = 0x02000000;
            break;
        case 10: // SCALEWALKER
            _rng = Random(flowSeed);
            _extraRng = Random(ornamentSeed + 0x9e3779b9);
            _algoState.scalewalker.pos = 0;
            break;
        case 11: // WINDOW
            _rng = Random(flowSeed);
            _extraRng = Random(ornamentSeed + 0x9e3779b9);
            _algoState.window.slowPhase = _rng.next() << 16;
            _algoState.window.fastPhase = _rng.next() << 16;
            _algoState.window.noteMemory = _rng.next() & 0x7;
            _algoState.window.noteHistory = _rng.next() & 0x7;
            _algoState.window.ghostThreshold = _rng.next() & 0x1f;
            _algoState.window.phaseRatio = 3 + (_rng.next() & 0x3);
            break;
        case 12: // MINIMAL
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
        case 13: // GANZ
            _rng = Random(flowSeed);
            _extraRng = Random(ornamentSeed + 0x9e3779b9);
            _algoState.ganz.phaseA = _rng.next() << 16;
            _algoState.ganz.phaseB = _rng.next() << 16;
            _algoState.ganz.phaseC = _rng.next() << 16;
            for (int i = 0; i < 3; i++) _algoState.ganz.noteHistory[i] = _rng.next() % 7;
            _algoState.ganz.selectMode = _rng.next() % 4;
            _algoState.ganz.skipDecimator = flow >> 2;
            _algoState.ganz.phraseSkipCount = 0;
            _algoState.ganz.velocitySample = 128 + (_extraRng.next() & 0x7F);
            break;
        case 14: // BLAKE
            _rng = Random(flowSeed);
            _extraRng = Random(ornamentSeed + 0x9e3779b9);
            for (int i = 0; i < 4; i++) _algoState.blake.motif[i] = _rng.next() % 7;
            _algoState.blake.breathPhase = _rng.next() << 16;
            _algoState.blake.breathPattern = (flow >> 2) % 4;
            _algoState.blake.breathCycleLength = 4 + ((ornament >> 2) % 4);
            _algoState.blake.subBassCountdown = 0;
            break;
        case 3: // APHEX (mapped from 18)
        case 18:
            _rng = Random(flowSeed);
            for (int i = 0; i < 4; ++i) _algoState.aphex.track1_pattern[i] = _rng.next() % 12;
            for (int i = 0; i < 3; ++i) _algoState.aphex.track2_pattern[i] = _rng.next() % 3;
            for (int i = 0; i < 5; ++i) _algoState.aphex.track3_pattern[i] = (_rng.next() % 8 == 0) ? (_rng.next() % 5) : 0;
            _algoState.aphex.pos1 = (ornament * 1) % 4;
            _algoState.aphex.pos2 = (ornament * 2) % 3;
            _algoState.aphex.pos3 = (ornament * 3) % 5;
            break;
        case 4: // AUTECHRE (mapped from 19)
        case 19:
            _rng = Random(flowSeed);
            for (int i = 0; i < 8; ++i) {
                int r = _rng.next() % 4;
                if (r == 0) _algoState.autechre.pattern[i] = 12;
                else if (r == 1) _algoState.autechre.pattern[i] = 24;
                else _algoState.autechre.pattern[i] = 0;
                if (_rng.nextBinary()) _algoState.autechre.pattern[i] += (_rng.next() % 5) * 2;
            }
            _algoState.autechre.rule_timer = 8 + (flow * 4);
            _rng = Random(ornamentSeed);
            for (int i = 0; i < 8; ++i) _algoState.autechre.rule_sequence[i] = _rng.next() % 5;
            _algoState.autechre.rule_index = 0;
            break;
        case 5: // STEPWAVE (mapped from 20)
        case 20:
            _rng = Random(flowSeed);
            _extraRng = Random(ornamentSeed + 0x9e3779b9);
            _algoState.stepwave.direction = 0;
            _algoState.stepwave.step_count = 3 + (_rng.next() % 5);
            _algoState.stepwave.current_step = 0;
            _algoState.stepwave.chromatic_offset = 0;
            _algoState.stepwave.is_stepped = true;
            break;
        default:
            _rng = Random(flowSeed);
            _extraRng = Random(ornamentSeed + 0x9e3779b9);
            break;
        }
    }

    GenerationContext calculateContext() const {
        GenerationContext ctx{};
        ctx.divisor = _params.divisor;           // ticks per step
        ctx.stepsPerBeat = (ctx.divisor > 0) ? (192 / ctx.divisor) : 0;
        ctx.isBeatStart = (ctx.stepsPerBeat > 0) && ((_stepIndex % ctx.stepsPerBeat) == 0);
        ctx.tpb = std::max(1, 192 / (int)ctx.divisor);
        ctx.loopLength = _params.loopLength;
        ctx.effectiveLoopLength = (ctx.loopLength > 0) ? ctx.loopLength : 32;
        int rotate = (_params.loopLength > 0) ? _params.rotate : 0;
        ctx.rotatedStep = (_params.loopLength > 0)
            ? ((_stepIndex + rotate + ctx.loopLength) % ctx.loopLength)
            : _stepIndex;
        ctx.ornament = _params.ornament;
        ctx.subdivisions = 4;
        if (ctx.ornament >= 5 && ctx.ornament <= 8) ctx.subdivisions = 3;
        else if (ctx.ornament >= 9 && ctx.ornament <= 12) ctx.subdivisions = 5;
        else if (ctx.ornament >= 13) ctx.subdivisions = 7;
        return ctx;
    }

    TuesdayTickResult generateStep(const GenerationContext &ctx) {
        int algorithm = _params.algorithm;
        int algo = algorithm;
        if (algorithm == 18) algo = 3;
        if (algorithm == 19) algo = 4;
        if (algorithm == 20) algo = 5;
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
        case 11: return generateWindow(ctx);
        case 12: return generateMinimal(ctx);
        case 13: return generateGanz(ctx);
        case 14: return generateBlake(ctx);
        default: return generateTest(ctx);
        }
    }

    // --- Algorithm generators (lifted/adapted) ---

    TuesdayTickResult generateTest(const GenerationContext &ctx) {
        TuesdayTickResult result;
        result.velocity = 255;
        result.gateRatio = 75;
        if (_rng.nextRange(100) < static_cast<uint32_t>(_params.glide)) result.slide = true;
        switch (_algoState.test.mode) {
        case 0: result.octave = (_stepIndex % 5); result.note = 0; break;
        default: result.octave = 0; result.note = _algoState.test.note; _algoState.test.note = (_algoState.test.note + 1) % 12; break;
        }
        result.velocity = _algoState.test.velocity;
        return result;
    }

    TuesdayTickResult generateTritrance(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 255;
        int gateLenRnd = _rng.nextRange(100);
        if (gateLenRnd < 40) result.gateRatio = 50 + (_rng.nextRange(4) * 12);
        else if (gateLenRnd < 70) result.gateRatio = 100 + (_rng.nextRange(4) * 25);
        else result.gateRatio = 200 + (_rng.nextRange(9) * 25);
        if (_rng.nextRange(100) < static_cast<uint32_t>(_params.glide)) result.slide = true;
        int phase = (_stepIndex + _algoState.tritrance.b2) % 3;
        switch (phase) {
        case 0:
            if (_extraRng.nextBinary() && _extraRng.nextBinary()) {
                _algoState.tritrance.b3 = (_extraRng.next() & 0x15);
                if (_algoState.tritrance.b3 >= 7) _algoState.tritrance.b3 -= 7; else _algoState.tritrance.b3 = 0;
                _algoState.tritrance.b3 -= 4;
            }
            result.octave = 0; result.note = _algoState.tritrance.b3 + 4;
            result.gateOffset = tClamp<int>(10 - _rng.nextRange(10), 0, 100);
            break;
        case 1:
            result.octave = 1; result.note = _algoState.tritrance.b3 + 4;
            if (_rng.nextBinary()) _algoState.tritrance.b2 = (_rng.next() & 0x7);
            result.gateOffset = tClamp<int>(25 + _rng.nextRange(10), 0, 100);
            break;
        case 2:
            result.octave = 2; result.note = _algoState.tritrance.b1;
            result.velocity = 255; result.accent = true;
            if (_rng.nextBinary()) _algoState.tritrance.b1 = ((_rng.next() >> 5) & 0x7);
            result.gateOffset = tClamp<int>(40 + _rng.nextRange(10), 0, 100);
            result.trillCount = 3;
            result.noteOffsets[0] = -2; result.noteOffsets[1] = -1; result.noteOffsets[2] = 0;
            break;
        }
        if (!result.accent) result.velocity = (_rng.nextRange(256) / 2);
        return result;
    }

    TuesdayTickResult generateMarkov(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 255; result.gateRatio = 75;
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
        if (_rng.nextBinary()) result.gateOffset = tClamp<int>(10 - _rng.nextRange(10), 0, 100);
        else result.gateOffset = tClamp<int>(15 + _rng.nextRange(10), 0, 100);
        if (_rng.nextRange(256) < 20) {
            result.trillCount = 2;
            result.noteOffsets[0] = 0; result.noteOffsets[1] = 0;
            result.gateRatio = 50;
        }
        return result;
    }

    TuesdayTickResult generateWobble(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 255;
        _algoState.wobble.phaseSpeed = 0xFFFFFFFF / std::max(1, ctx.effectiveLoopLength);
        _algoState.wobble.phaseSpeed2 = 0xCFFFFFFF / std::max(1, ctx.effectiveLoopLength / 4);
        _algoState.wobble.phase += _algoState.wobble.phaseSpeed;
        _algoState.wobble.phase2 += _algoState.wobble.phaseSpeed2;
        if (_rng.nextRange(256) >= static_cast<uint32_t>(_params.ornament * 16)) {
            int rawPhase = (_algoState.wobble.phase2 >> 27) & 0x1F;
            result.note = rawPhase % 7;
            result.octave = 1 + (rawPhase / 7);
            if (_algoState.wobble.lastWasHigh == 0) if (_rng.nextRange(256) >= 200) result.slide = true;
            _algoState.wobble.lastWasHigh = 1;
        } else {
            int rawPhase = (_algoState.wobble.phase >> 27) & 0x1F;
            result.note = rawPhase % 7;
            result.octave = (rawPhase / 7);
            if (_algoState.wobble.lastWasHigh == 1) if (_rng.nextRange(256) >= 200) result.slide = true;
            _algoState.wobble.lastWasHigh = 0;
        }
        result.velocity = (_extraRng.nextRange(256) / 4);
        if (_rng.nextRange(256) >= 50) result.accent = true;
        result.gateOffset = tClamp<int>(((_algoState.wobble.phase + _algoState.wobble.phase2) >> 28) % 30, 0, 100);
        return result;
    }

    TuesdayTickResult generateStomper(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 255; result.gateRatio = 75;
        int accentoffs = 0; uint8_t veloffset = 0;
        if (_algoState.stomper.countDown > 0) {
            result.gateRatio = _algoState.stomper.countDown * 25;
            result.velocity = 0;
            _algoState.stomper.countDown--;
            result.note = _algoState.stomper.lastNote;
            result.octave = _algoState.stomper.lastOctave;
        } else {
            if (_algoState.stomper.mode >= 14) _algoState.stomper.mode = (_extraRng.next() % 7) * 2;
            _algoState.stomper.lowNote = _rng.next() % 3;
            _algoState.stomper.highNote[0] = _rng.next() % 7;
            _algoState.stomper.highNote[1] = _rng.next() % 5;
            veloffset = 100; int maxticklen = 2;
            switch (_algoState.stomper.mode) {
            case 10: result.octave = 1; result.note = _algoState.stomper.highNote[_rng.next() % 2]; _algoState.stomper.mode++; break;
            case 11: result.octave = 0; result.note = _algoState.stomper.lowNote; result.slide = true; if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen; _algoState.stomper.mode = 14; break;
            case 12: result.octave = 0; result.note = _algoState.stomper.lowNote; _algoState.stomper.mode++; break;
            case 13: result.octave = 1; result.note = _algoState.stomper.highNote[_rng.next() % 2]; result.slide = true; if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen; _algoState.stomper.mode = 14; break;
            case 4: case 5: result.octave = 0; result.note = _algoState.stomper.lowNote; if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen; _algoState.stomper.mode = 14; accentoffs = 100; break;
            case 0: case 1: result.octave = _algoState.stomper.lastOctave; result.note = _algoState.stomper.lastNote; veloffset = 0; if (_extraRng.nextBinary()) _algoState.stomper.countDown = _extraRng.next() % maxticklen; _algoState.stomper.mode = 14; break;
            default: result.octave = (_algoState.stomper.mode % 2) ? 1 : 0; result.note = _algoState.stomper.highNote[_rng.next() % 2]; _algoState.stomper.mode = 14; break;
            }
            _algoState.stomper.lastNote = result.note;
            _algoState.stomper.lastOctave = result.octave;
        }
        result.velocity = veloffset;
        if (accentoffs > 0) { result.velocity += accentoffs; result.accent = true; }
        return result;
    }

    TuesdayTickResult generateChipArp1(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 255;
        Random rng(_algoState.chiparp1.rngSeed);
        int steps[4] = {0, 2, 4, 7};
        int pos = (_stepIndex + _algoState.chiparp1.base) % 4;
        result.note = steps[pos];
        result.octave = (_algoState.chiparp1.chordSeed >> pos) & 0x1;
        result.gateRatio = 60 + (_algoState.chiparp1.dir ? 30 : 0);
        result.gateOffset = (_algoState.chiparp1.dir ? 10 : 0);
        result.slide = rng.nextBinary();
        result.accent = rng.nextBinary();
        return result;
    }

    TuesdayTickResult generateChipArp2(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 200 + (_rng.nextRange(56));
        if (_algoState.chiparp2.deadTime > 0) {
            _algoState.chiparp2.deadTime--;
            result.velocity = 0;
            return result;
        }
        int baseChord[4] = {0, 2, 4, 7};
        int chordLen = std::min(4, _algoState.chiparp2.chordLen);
        int idx = _algoState.chiparp2.idx % chordLen;
        int dir = _algoState.chiparp2.dir ? 1 : -1;
        result.note = baseChord[(idx + chordLen) % chordLen] + _algoState.chiparp2.offset;
        result.octave = _algoState.chiparp2.timeMult ? 1 : 0;
        result.gateRatio = 50 + (_algoState.chiparp2.timeMult ? 30 : 0);
        result.slide = _rng.nextBinary();
        _algoState.chiparp2.idx += dir;
        if (_algoState.chiparp2.idx < 0) _algoState.chiparp2.idx += chordLen;
        if (_algoState.chiparp2.idx >= chordLen) _algoState.chiparp2.idx -= chordLen;
        if (_rng.nextRange(100) < 10) _algoState.chiparp2.deadTime = 1;
        return result;
    }

    TuesdayTickResult generateAphex(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 180; result.gateRatio = 75;
        int phrase = _stepIndex % 3;
        if (phrase == 0) { result.note = _algoState.aphex.track1_pattern[_algoState.aphex.pos1]; if (++_algoState.aphex.pos1 >= 4) _algoState.aphex.pos1 = 0; }
        else if (phrase == 1) { result.note = _algoState.aphex.track2_pattern[_algoState.aphex.pos2]; if (++_algoState.aphex.pos2 >= 3) _algoState.aphex.pos2 = 0; }
        else { result.note = _algoState.aphex.track3_pattern[_algoState.aphex.pos3]; if (++_algoState.aphex.pos3 >= 5) _algoState.aphex.pos3 = 0; }
        result.octave = phrase % 2;
        if (_extraRng.nextRange(100) < 15) { result.velocity = 80; result.gateRatio = 40; }
        return result;
    }

    TuesdayTickResult generateAutechre(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 160; result.gateRatio = 60;
        int idx = _stepIndex % 8;
        int note = _algoState.autechre.pattern[idx];
        result.note = note % 12;
        result.octave = note / 12;
        result.gateOffset = (_stepIndex % 3 == 0) ? 20 : 5;
        if (--_algoState.autechre.rule_timer <= 0) {
            _algoState.autechre.rule_timer = 8 + (_params.flow * 4);
            _algoState.autechre.pattern[idx] = (_algoState.autechre.pattern[idx] + _algoState.autechre.rule_sequence[_algoState.autechre.rule_index]) % 24;
            _algoState.autechre.rule_index = (_algoState.autechre.rule_index + 1) % 8;
        }
        return result;
    }

    TuesdayTickResult generateStepwave(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 220; result.gateRatio = 80;
        if (_algoState.stepwave.direction == 0) _algoState.stepwave.direction = (_params.flow % 2) ? 1 : -1;
        result.note = _algoState.stepwave.current_step;
        result.octave = (_algoState.stepwave.current_step / 4) % 2;
        result.slide = (_params.glide > 50);
        if (_params.ornament > 8) result.trillCount = 2;
        _algoState.stepwave.current_step += _algoState.stepwave.direction;
        if (_algoState.stepwave.current_step >= _algoState.stepwave.step_count || _algoState.stepwave.current_step < 0) {
            _algoState.stepwave.direction *= -1;
            _algoState.stepwave.current_step = tClamp(_algoState.stepwave.current_step, 0, _algoState.stepwave.step_count - 1);
        }
        return result;
    }

    TuesdayTickResult generateScalewalker(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 140 + (_params.power * 7);
        result.note = (_algoState.scalewalker.pos % 7);
        result.octave = (_algoState.scalewalker.pos / 7) % 3;
        result.slide = (_params.glide > 20);
        result.gateRatio = 60 + ((_algoState.scalewalker.pos % 4) * 10);
        _algoState.scalewalker.pos = (_algoState.scalewalker.pos + ((_rng.next() % 3) - 1)) % 21;
        if (_algoState.scalewalker.pos < 0) _algoState.scalewalker.pos += 21;
        return result;
    }

    TuesdayTickResult generateWindow(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 200;
        _algoState.window.slowPhase += (1u << 26);
        _algoState.window.fastPhase += (1u << (24 + (_algoState.window.phaseRatio & 0x3)));
        int window = (_algoState.window.slowPhase >> 27) & 0x7;
        int note = (_algoState.window.fastPhase >> 29) & 0x7;
        if (window < 2) result.velocity = 0;
        result.note = note;
        result.octave = (window > 4) ? 1 : 0;
        if (_rng.nextRange(32) < static_cast<uint32_t>(_algoState.window.ghostThreshold)) result.velocity = 0;
        result.gateOffset = (_algoState.window.fastPhase >> 27) & 0x1F;
        result.gateRatio = 60 + ((_algoState.window.slowPhase >> 28) & 0x1F);
        return result;
    }

    TuesdayTickResult generateMinimal(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 0; result.gateRatio = 40; result.note = 0; result.octave = 0;
        if (_algoState.minimal.mode == 0) { // silence
            if (--_algoState.minimal.silenceTimer <= 0) {
                _algoState.minimal.mode = 1;
                _algoState.minimal.burstTimer = _algoState.minimal.burstLength;
            }
        } else { // burst
            result.velocity = _algoState.minimal.clickDensity;
            result.note = _algoState.minimal.noteIndex % 7;
            result.octave = (_algoState.minimal.noteIndex / 7) % 2;
            _algoState.minimal.noteIndex++;
            if (--_algoState.minimal.burstTimer <= 0) {
                _algoState.minimal.mode = 0;
                _algoState.minimal.silenceTimer = _algoState.minimal.silenceLength;
            }
        }
        return result;
    }

    TuesdayTickResult generateBlake(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = 180;
        int phrase = _stepIndex % 4;
        result.note = _algoState.blake.motif[phrase];
        result.octave = phrase / 2;
        _algoState.blake.breathPhase += 0x1000000;
        if ((_algoState.blake.breathPhase >> 28) & 1) result.slide = true;
        if (_algoState.blake.subBassCountdown > 0) _algoState.blake.subBassCountdown--;
        if (_rng.nextRange(100) < 5 && _algoState.blake.subBassCountdown == 0) {
            result.note -= 7; result.octave = 0; result.accent = true; _algoState.blake.subBassCountdown = 8;
        }
        return result;
    }

    TuesdayTickResult generateGanz(const GenerationContext &ctx) {
        TuesdayTickResult result; result.velocity = _algoState.ganz.velocitySample;
        uint32_t oldPhaseA = _algoState.ganz.phaseA;
        _algoState.ganz.phaseA += 0x08000000;
        _algoState.ganz.phaseB += 0x04000000;
        _algoState.ganz.phaseC += 0x02000000;
        int tupletPos = (_algoState.ganz.phaseA >> 30) & 0x3;
        int candidates[3] = {_algoState.ganz.noteHistory[0], _algoState.ganz.noteHistory[1], _algoState.ganz.noteHistory[2]};
        int selectedNote = candidates[_algoState.ganz.selectMode % 3];
        if (tupletPos == 0 && _algoState.ganz.phraseSkipCount == 0) {
            _algoState.ganz.phraseSkipCount = _algoState.ganz.skipDecimator;
        } else if (_algoState.ganz.phraseSkipCount > 0) {
            _algoState.ganz.phraseSkipCount--;
        }
        _algoState.ganz.noteHistory[2] = _algoState.ganz.noteHistory[1];
        _algoState.ganz.noteHistory[1] = _algoState.ganz.noteHistory[0];
        _algoState.ganz.noteHistory[0] = selectedNote;
        result.note = selectedNote;
        result.octave = (_algoState.ganz.phaseC >> 29) % 3;
        if (_stepIndex % 8 == 0) _algoState.ganz.velocitySample = 64 + (_extraRng.next() & 0xBF);
        result.velocity = _algoState.ganz.velocitySample;
        bool isAccent = (_algoState.ganz.phaseA < oldPhaseA);
        if (isAccent) {
            result.accent = true; result.velocity = 255; result.gateRatio = 125; result.slide = true;
        } else {
            result.gateRatio = 60 + (tupletPos * 15);
            result.slide = ((_extraRng.next() % 100) < static_cast<uint32_t>(_params.ornament * 6));
        }
        result.gateOffset = ((_algoState.ganz.phaseB >> 26) & 0x1F) % 20;
        return result;
    }

    float scaleToVolts(int noteIndex, int octave) const {
        int totalDegree = noteIndex + (octave * 7);
        totalDegree += _params.transpose;
        float voltage = ScaleMajor::noteToVolts(totalDegree);
        voltage += (_params.rootNote * (1.f / 12.f));
        voltage += _params.octave;
        return voltage;
    }
};

// ----------------- C API exports for wasm -----------------

static TuesdaySim g_sim;
static std::vector<ExportedStep> g_steps;

extern "C" {

void wasm_init() {
    g_sim.reset();
}

void wasm_reset() {
    g_sim.reset();
}

// key matches setParam switch (documented in README)
void wasm_set_param(int key, int value) {
    g_sim.setParam(key, value);
}

int wasm_run_steps(int count) {
    g_steps.clear();
    g_steps.reserve(count);
    uint32_t tickBase = 0;
    for (int i = 0; i < count; ++i) {
        g_steps.push_back(g_sim.runOneStep(tickBase));
    }
    return static_cast<int>(g_steps.size());
}

int wasm_get_steps_len() {
    return static_cast<int>(g_steps.size());
}

ExportedStep *wasm_get_steps_ptr() {
    return g_steps.empty() ? nullptr : g_steps.data();
}

} // extern "C"
