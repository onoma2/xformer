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

    // Unified Generation Engine
    TuesdayTickResult generateStep(uint32_t tick);

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

    // TEST algorithm state
    uint8_t _testMode = 0;      // 0=OCTSWEEPS, 1=SCALEWALKER
    uint8_t _testSweepSpeed = 0;
    uint8_t _testAccent = 0;
    uint8_t _testVelocity = 0;
    int16_t _testNote = 0;

    // TRITRANCE algorithm state
    int16_t _triB1 = 0;  // High note
    int16_t _triB2 = 0;  // Phase offset
    int16_t _triB3 = 0;  // Note offset

    // STOMPER algorithm state
    uint8_t _stomperMode = 0;
    uint8_t _stomperCountDown = 0;
    uint8_t _stomperLowNote = 0;
    uint8_t _stomperHighNote[2] = {0, 0};
    int16_t _stomperLastNote = 0;
    uint8_t _stomperLastOctave = 0;

    // MARKOV algorithm state
    int16_t _markovHistory1 = 0;
    int16_t _markovHistory3 = 0;
    uint8_t _markovMatrix[8][8][2];

    // CHIPARP algorithm state
    uint32_t _chipChordSeed = 0;
    Random _chipRng;
    uint8_t _chipBase = 0;
    uint8_t _chipDir = 0;
    
    // CHIPARP2 algorithm state
    uint8_t _chip2ChordScaler = 0;
    uint8_t _chip2Offset = 0;
    uint8_t _chip2Len = 0;
    uint8_t _chip2TimeMult = 0;
    uint8_t _chip2DeadTime = 0;
    uint8_t _chip2Idx = 0;
    uint8_t _chip2Dir = 0;
    uint8_t _chip2ChordLen = 0;
    Random _chip2Rng;

    // WOBBLE algorithm state
    uint32_t _wobblePhase = 0;
    uint32_t _wobblePhaseSpeed = 0;
    uint32_t _wobblePhase2 = 0;
    uint32_t _wobblePhaseSpeed2 = 0;
    uint8_t _wobbleLastWasHigh = 0;

    // For APHEX - Polyrhythmic Event Sequencer
    uint8_t _aphex_track1_pattern[4]; // 4-step melodic pattern
    uint8_t _aphex_track2_pattern[3]; // 3-step modifier pattern (0=off, 1=stutter, 2=slide)
    uint8_t _aphex_track3_pattern[5]; // 5-step bass/override pattern
    uint8_t _aphex_pos1, _aphex_pos2, _aphex_pos3;

    // For AUTECHRE - Algorithmic Transformation Engine
    int8_t _autechre_pattern[8];    // The 8-step pattern being transformed
    uint8_t _autechre_rule_index;   // Which transformation rule to apply next
    int _autechre_rule_timer;       // How long to wait before applying the next rule
    uint8_t _autechre_rule_sequence[8]; // The sequence of rules to apply

    // STEPWAVE algorithm state (20) - Chromatic stepping trill
    int8_t _stepwave_direction;     // -1=down, 0=random, +1=up
    uint8_t _stepwave_step_count;   // Number of trill substeps (3-7)
    int8_t _stepwave_current_step;  // Current position in trill (0 to step_count-1)
    int8_t _stepwave_chromatic_offset; // Running chromatic offset from base note
    bool _stepwave_is_stepped;      // true=stepped, false=slide

    // SCALEWALKER algorithm state (10) - Scale degree walker with subdivisions
    int8_t _scalewalker_pos;        // Current position in scale (0-6)

    // Output state
    bool _activity = false;
    bool _gateOutput = false;
    float _cvOutput = 0.f;

    // Gated CV mode state - tracks last CV value when gate fired
    float _lastGatedCv = 0.f;
};