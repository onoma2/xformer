#pragma once

#include "TrackEngine.h"

#include "model/Track.h"

#include "core/utils/Random.h"
#include "SortedQueue.h"

class TuesdayTrackEngine : public TrackEngine {
public:
    TuesdayTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine)
    {
        reset();
    }

    virtual Track::TrackMode trackMode() const override { return Track::TrackMode::Tuesday; }

    virtual void reset() override;
    virtual void restart() override;
    virtual TickResult tick(uint32_t tick) override;
    virtual void update(float dt) override;

    virtual bool activity() const override { return _activity; }
    virtual bool gateOutput(int index) const override { return _gateOutput; }
    virtual float cvOutput(int index) const override { return _cvOutput; }

    const TuesdayTrack &tuesdayTrack() const {
        return _track.tuesdayTrack();
    }
    virtual float sequenceProgress() const override {
        int loopLength = tuesdayTrack().sequence(pattern()).actualLoopLength();
        if (loopLength <= 0) return 0.f;  // Infinite loop shows no progress
        // Use modulo to ensure step is always within bounds
        uint32_t step = _stepIndex % loopLength;
        return float(step) / float(loopLength);
    }

    // Reseed the algorithm (called from UI via Shift+F5)
    void reseed();

    // Current step index for UI display
    int currentStep() const { return _displayStep; }

private:
    void initAlgorithm();
    
    // The "Contract": Abstract step result from the generation engine
    struct TuesdayTickResult {
        int note = 0;           // Scale Degree
        int octave = 0;         // Octave Offset
        uint8_t velocity = 0;   // 0-255 (For Density Gating)
        bool accent = false;
        bool slide = false;

        // TIMING UNIT CONVERSION (Original C → C++):
        // Original C: maxsubticklength (fixed units: 4-22 subticks at TUESDAY_SUBTICKRES=6)
        // C++: gateRatio (percentage: 0-200% of step divisor)
        // At 192 PPQN with 48 PPQN steps: divisor=4 ticks, 75% = 3 ticks ≈ 6 subticks
        // This percentage-based approach scales properly with variable tempo/divisor.
        uint16_t gateRatio = 75; // 0-200% (Relative Duration)
        uint8_t gateOffset = 0;  // 0-100% (Timing Offset)

        // Trill Control (follows Glide pattern)
        // Algorithm sets trillCount (1-4) to request micro-gate subdivision
        // UI stepTrill parameter (0-100) controls probability of firing
        uint8_t trillCount = 1;  // 1-4 gates (algorithm's desired subdivision)

        uint8_t beatSpread = 0;  // 0-100% → 1x-5x timing window multiplier
        uint8_t polyCount = 0;   // Number of subdivisions (0=none, 3/5/7=tuplet)

        // Micro-sequencing: Note offsets for each micro-gate (in semitones/scale degrees)
        int8_t noteOffsets[8] = {0, 0, 0, 0, 0, 0, 0, 0};

        // Spatial vs Temporal distribution
        // true = polyrhythm mode (gates spread evenly across 4-beat window)
        // false = trill mode (gates fired rapidly in succession)
        bool isSpatial = true;
    };

    // Micro-gate scheduling system (inspired by NoteTrackEngine)
    struct MicroGate {
        uint32_t tick;      // Absolute tick when gate should fire
        bool gate;          // true=ON, false=OFF
        float cvTarget;     // CV voltage for this gate (for trill intervals)
    };

    struct MicroGateCompare {
        bool operator()(const MicroGate &a, const MicroGate &b) const {
            return a.tick < b.tick;
        }
    };

    // Generation context (shared across algorithms)
    struct GenerationContext {
        uint32_t divisor;
        int tpb, loopLength, effectiveLoopLength, rotatedStep;
        int ornament, subdivisions, stepsPerBeat;
        bool isBeatStart;
    };

    // Unified Generation Engine
    TuesdayTickResult generateStep(uint32_t tick);

    // Context calculation
    GenerationContext calculateContext(uint32_t tick) const;

    // Algorithm generators
    TuesdayTickResult generateTest(const GenerationContext &ctx);
    TuesdayTickResult generateTritrance(const GenerationContext &ctx);
    TuesdayTickResult generateStomper(const GenerationContext &ctx);
    TuesdayTickResult generateAphex(const GenerationContext &ctx);
    TuesdayTickResult generateAutechre(const GenerationContext &ctx);
    TuesdayTickResult generateStepwave(const GenerationContext &ctx);
    TuesdayTickResult generateMarkov(const GenerationContext &ctx);
    TuesdayTickResult generateChipArp1(const GenerationContext &ctx);
    TuesdayTickResult generateChipArp2(const GenerationContext &ctx);
    TuesdayTickResult generateWobble(const GenerationContext &ctx);
    TuesdayTickResult generateScalewalker(const GenerationContext &ctx);
    TuesdayTickResult generateWindow(const GenerationContext &ctx);
    TuesdayTickResult generateMinimal(const GenerationContext &ctx);
    TuesdayTickResult generateBlake(const GenerationContext &ctx);
    TuesdayTickResult generateGanz(const GenerationContext &ctx);

    // The "Pipeline": Converts abstract algorithm steps into quantized voltage
    float scaleToVolts(int noteIndex, int octave) const;

    // Dual RNG system (matches original Tuesday)
    // _rng is seeded from seed1 (Flow) or seed2 (Ornament) depending on algorithm
    // _extraRng is seeded from the other parameter
    Random _rng;
    Random _extraRng;
    Random _uiRng;

    // Cached properties
    int _cachedAlgorithm;
    int _cachedFlow;
    int _cachedOrnament;
    int _cachedLoopLength;

    // Persisted state
    int _stepIndex = 0;
    int _displayStep = -1;  // Step currently being displayed (set before processing)
    uint32_t _gateLength = 0;
    uint32_t _gateTicks = 0;

    // Cooldown system (Power parameter controls note density)
    // Higher power = shorter cooldown = more notes
    // Lower power = longer cooldown = sparser patterns
    int _coolDown = 0;
    int _coolDownMax = 0;

    // Microgate-level cooldown (Option 1.5: Nested cooldown)
    // Same mapping as step cooldown, but applies to individual microgates
    // Threshold is 2x harder (velDensity must be >= microCoolDown * 2)
    int _microCoolDown = 0;
    int _microCoolDownMax = 0;

    // Gate length (as fraction of divisor, 0-100%)
    int _gatePercent = 75;  // Default 75% gate length

    // Gate offset from step start (as fraction of divisor, 0-100%)
    uint8_t _gateOffset = 0;  // Default 0% gate offset

    // Micro-gate queue for precise timing
    // Size 32: supports 7-tuplet (14 entries) with headroom for multiple beats
    SortedQueue<MicroGate, 32, MicroGateCompare> _microGateQueue;
    bool _tieActive = false; // Tracks if the current gate is tied from previous

    // Retrigger/Trill State
    int _retriggerCount = 0;
    uint32_t _retriggerPeriod = 0;
    uint32_t _retriggerLength = 0;
    uint32_t _retriggerTimer = 0;
    bool _isTrillNote = false;
    float _trillCvTarget = 0.f;
    bool _retriggerArmed = false;
    
    // Polyrhythmic State
    int _polySubStep = 0;
    bool _polyAlgoActive = false;
    
    // Micro-Sequencing / Ratcheting
    int8_t _ratchetInterval = 0; // Semitones/Degrees shift per ratchet

    // Slide/portamento state
    int _slide = 0;           // Slide amount (0=instant, 1-3=glide)
    float _cvTarget = 0.f;    // Target CV value
    float _cvCurrent = 0.f;   // Current CV value (for glide)
    float _cvDelta = 0.f;     // CV change per tick
    int _slideCountDown = 0;  // Ticks remaining in slide

    // Algorithm-specific state structures (POD)
    struct TestState {
        uint8_t mode, sweepSpeed, accent, velocity;
        int16_t note;
    };  // 6 bytes

    struct TritranceState {
        int16_t b1, b2, b3;
    };  // 6 bytes

    struct StomperState {
        uint8_t mode, countDown, lowNote, highNote[2], lastOctave;
        int16_t lastNote;
    };  // 10 bytes

    struct AphexState {
        uint8_t track1_pattern[4], track2_pattern[3], track3_pattern[5];
        uint8_t pos1, pos2, pos3;
    };  // 15 bytes

    struct AutechreState {
        int8_t pattern[8];
        uint8_t rule_index, rule_sequence[8];
        int rule_timer;
    };  // 21 bytes

    struct StepwaveState {
        int8_t direction, current_step, chromatic_offset;
        uint8_t step_count;
        bool is_stepped;
    };  // 5 bytes

    struct MarkovState {
        int16_t history1, history3;
        uint8_t matrix[8][8][2];
    };  // 132 bytes

    struct ChipArp1State {
        uint32_t chordSeed, rngSeed;  // Store seed, reconstruct Random
        uint8_t base, dir;
    };  // 10 bytes

    struct ChipArp2State {
        uint32_t rngSeed;
        uint8_t chordScaler, offset, len, timeMult, deadTime, idx, dir, chordLen;
    };  // 13 bytes

    struct WobbleState {
        uint32_t phase, phaseSpeed, phase2, phaseSpeed2;
        uint8_t lastWasHigh;
    };  // 17 bytes

    struct ScalewalkerState {
        int8_t pos;
    };  // 1 byte

    struct WindowState {
        uint32_t slowPhase;      // Slow cycle phasor (anchor notes)
        uint32_t fastPhase;      // Fast cycle phasor (texture)
        uint8_t noteMemory;      // Current Markov note (0-7)
        uint8_t noteHistory;     // Previous Markov note (0-7)
        uint8_t ghostThreshold;  // Ghost note threshold 0-31 (mutable)
        uint8_t phaseRatio;      // Polyrhythm ratio 3-6 (mutable)
    };  // 14 bytes

    struct MinimalState {
        uint8_t burstLength;     // 2-8 steps per burst
        uint8_t silenceLength;   // 4-16 steps per silence
        uint8_t clickDensity;    // 0-255 note probability
        uint8_t burstTimer;      // Countdown in current burst
        uint8_t silenceTimer;    // Countdown in current silence
        uint8_t noteIndex;       // Current note position 0-20
        uint8_t mode;            // 0=SILENCE, 1=BURST
    };  // 7 bytes

    struct BlakeState {
        uint8_t motif[4];           // 4-note melodic core (0-6 scale degrees)
        uint32_t breathPhase;       // 32-bit phasor for 8-bar breath LFO
        uint8_t breathPattern;      // 0-3: articulation variant index
        uint8_t breathCycleLength;  // 4-7 steps (mutable)
        uint8_t subBassCountdown;   // Stomper-style drop counter
    };  // 13 bytes

    struct GanzState {
        uint32_t phaseA;            // 1x speed phasor (base rhythm)
        uint32_t phaseB;            // 5x speed phasor (micro-rhythm)
        uint32_t phaseC;            // 7x speed phasor (texture layer)
        uint8_t noteHistory[3];     // 3-note melodic memory (0-6)
        uint8_t selectMode;         // 0-3: note selection mode (mutable)
        uint8_t phraseSkipCount;    // Active skip counter
        uint8_t velocitySample;     // Sample-and-hold velocity
        uint8_t skipDecimator;      // Controls skip frequency
    };  // 20 bytes

    // Union for algorithm state (only one active)
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

    AlgorithmState _algoState;

    // Output state
    bool _activity = false;
    bool _gateOutput = false;
    float _cvOutput = 0.f;

    // Gated CV mode state - tracks last CV value when gate fired
    float _lastGatedCv = 0.f;

    // Prime masking state variables
    int _primeMaskCounter = 0;  // Counter for consecutive ticks to allow
    int _primeMaskState = 1;    // 0=mask, 1=allow
    int _cachedPrimePattern = -1; // Cached value to detect changes
    int _cachedPrimeParam = -1;   // Cached value to detect changes
    int _cachedTimeMode = -1;     // Cached value to detect time mode changes
    uint32_t _timeModeStartTick = 0; // Tick when current time mode interval started
    int _currentMaskArrayIndex = 0;  // Current index in the mask array
    int _cachedMaskProgression = -1; // Cached value for progression changes
};