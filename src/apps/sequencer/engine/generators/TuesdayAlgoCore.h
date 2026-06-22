#pragma once

#include "core/utils/Random.h"
#include "TuringRegister.h"

#include <cstdint>
#include <algorithm>

struct AlgoParams {
    uint32_t seed = 0;
    int algorithm = 0;       // 0-14
    int flow = 8;            // 0-16
    int ornament = 8;        // 0-16
    int glide = 0;           // 0-100
    int power = 8;           // 0-16
    int stepTrill = 0;       // 0-100
    int gateLength = 50;     // 0-100
};

struct AlgoContext {
    int stepIndex;
    int rotatedStep;
    int ornament;
    int subdivisions;
    int stepsPerBeat;
    bool isBeatStart;
    uint32_t divisor;
    int tpb;
    int loopLength;
    int effectiveLoopLength;
};

struct AlgoResult {
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

class TuesdayAlgoCore {
public:
    TuesdayAlgoCore();

    void init(const AlgoParams &params);
    void reseed();
    AlgoResult generate(int algorithm, const AlgoContext &ctx);

private:
    // Per-algorithm state structs
    struct TestState {
        uint8_t mode, sweepSpeed, accent, velocity;
        int16_t note;
    };

    struct TritranceState {
        int16_t b1, b2, b3;
    };

    struct StomperState {
        uint8_t mode, countDown, lowNote, highNote[2], lastOctave;
        int16_t lastNote;
    };

    struct AphexState {
        uint8_t track1_pattern[4], track2_pattern[3], track3_pattern[5];
        uint8_t pos1, pos2, pos3;
    };

    struct AutechreState {
        int8_t pattern[8];
        uint8_t rule_index, rule_sequence[8];
        int rule_timer;
    };

    struct StepwaveState {
        int8_t direction, current_step, chromatic_offset;
        uint8_t step_count;
        bool is_stepped;
    };

    struct MarkovState {
        int16_t history1, history3;
        uint8_t matrix[8][8][2];
    };

    struct ChipArp1State {
        uint32_t chordSeed, rngSeed;
        uint8_t base, dir;
    };

    struct ChipArp2State {
        uint32_t rngSeed;
        uint8_t chordScaler, offset, len, timeMult, deadTime, idx, dir, chordLen;
    };

    struct WobbleState {
        uint32_t phase, phaseSpeed, phase2, phaseSpeed2;
        uint8_t lastWasHigh;
    };

    struct ScalewalkerState {
        int8_t pos;
    };

    struct WindowState {
        uint32_t slowPhase, fastPhase;
        uint8_t noteMemory, noteHistory, ghostThreshold, phaseRatio;
    };

    struct MinimalState {
        uint8_t burstLength, silenceLength, clickDensity;
        uint8_t burstTimer, silenceTimer, noteIndex;
        uint8_t mode;
    };

    struct BlakeState {
        uint8_t motif[4];
        uint32_t breathPhase;
        uint8_t breathPattern, breathCycleLength, subBassCountdown;
    };

    struct GanzState {
        uint32_t phaseA, phaseB, phaseC;
        uint8_t noteHistory[3];
        uint8_t selectMode, phraseSkipCount, velocitySample, skipDecimator;
    };

    union AlgorithmState {
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
    };

    // Algorithm dispatch
    AlgoResult generateTest(const AlgoContext &ctx);
    AlgoResult generateTritrance(const AlgoContext &ctx);
    AlgoResult generateStomper(const AlgoContext &ctx);
    AlgoResult generateMarkov(const AlgoContext &ctx);
    AlgoResult generateChipArp1(const AlgoContext &ctx);
    AlgoResult generateChipArp2(const AlgoContext &ctx);
    AlgoResult generateWobble(const AlgoContext &ctx);
    AlgoResult generateScalewalker(const AlgoContext &ctx);
    AlgoResult generateWindow(const AlgoContext &ctx);
    AlgoResult generateMinimal(const AlgoContext &ctx);
    AlgoResult generateGanz(const AlgoContext &ctx);
    AlgoResult generateBlake(const AlgoContext &ctx);
    AlgoResult generateAphex(const AlgoContext &ctx);
    AlgoResult generateAutechre(const AlgoContext &ctx);
    AlgoResult generateStepwave(const AlgoContext &ctx);
    AlgoResult generateTuring(const AlgoContext &ctx);

    // State
    AlgoParams _params;
    int _cachedFlow;
    int _cachedOrnament;
    Random _rng;
    Random _extraRng;
    AlgorithmState _algoState;
    TuringRegister _turing;
};
