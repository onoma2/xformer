#pragma once

#include "TrackEngine.h"

#include "model/Track.h"

#include "core/utils/Random.h"

class TuesdayTrackEngine : public TrackEngine {
public:
    TuesdayTrackEngine(Engine &engine, const Model &model, Track &track, const TrackEngine *linkedTrackEngine) :
        TrackEngine(engine, model, track, linkedTrackEngine),
        _tuesdayTrack(track.tuesdayTrack())
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
    virtual float sequenceProgress() const override {
        int loopLength = _tuesdayTrack.actualLoopLength();
        if (loopLength <= 0) return 0.f;  // Infinite loop shows no progress
        // Use modulo to ensure step is always within bounds
        uint32_t step = _stepIndex % loopLength;
        return float(step) / float(loopLength);
    }

    // Reseed the algorithm (called from UI via Shift+F5)
    void reseed();

    // Current step index for UI display
    int currentStep() const { return _displayStep; }

    // Generate pattern buffer for finite loops
    void generateBuffer();

private:
    void initAlgorithm();

    // Structure for pre-generated step data
    struct BufferedStep {
        int8_t note;
        int8_t octave;
        uint8_t gatePercent;
        uint8_t slide;
        uint8_t gateOffset;
    };

    // Pattern buffer for finite loops (128 steps)
    static const int BUFFER_SIZE = 128;
    BufferedStep _buffer[BUFFER_SIZE];
    bool _bufferValid = false;

    const TuesdayTrack &_tuesdayTrack;

public:
    // Public type alias for testing
    using PublicBufferedStep = BufferedStep;

    // Public method to access buffer for testing purposes
    PublicBufferedStep getBufferAt(int index) const {
        if (index >= 0 && index < BUFFER_SIZE) {
            return _buffer[index];
        }
        return PublicBufferedStep{}; // Return default if out of bounds
    }

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

    // State for gate offset processing
    uint32_t _gateLengthTicks = 0;
    uint32_t _pendingGateOffsetTicks = 0;
    bool _pendingGateActivation = false;

    // Retrigger/Trill State
    int _retriggerCount = 0;
    uint32_t _retriggerPeriod = 0;
    uint32_t _retriggerLength = 0;
    uint32_t _retriggerTimer = 0;
    bool _isTrillNote = false;
    float _trillCvTarget = 0.f;
    bool _retriggerArmed = false;

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

    // GOACID algorithm state
    uint8_t _goaB1 = 0;  // Pattern transpose flag 1
    uint8_t _goaB2 = 0;  // Pattern transpose flag 2

    // SNH (Sample & Hold) algorithm state
    uint32_t _snhPhase = 0;
    uint32_t _snhPhaseSpeed = 0;
    uint8_t _snhLastVal = 0;
    int32_t _snhTarget = 0;
    int32_t _snhCurrent = 0;
    int32_t _snhCurrentDelta = 0;

    // WOBBLE algorithm state
    uint32_t _wobblePhase = 0;
    uint32_t _wobblePhaseSpeed = 0;
    uint32_t _wobblePhase2 = 0;
    uint32_t _wobblePhaseSpeed2 = 0;
    uint8_t _wobbleLastWasHigh = 0;

    // TECHNO algorithm state
    uint8_t _technoKickPattern = 0;
    uint8_t _technoHatPattern = 0;
    uint8_t _technoBassNote = 0;

    // FUNK algorithm state
    uint8_t _funkPattern = 0;
    uint8_t _funkSyncopation = 0;
    uint8_t _funkGhostProb = 0;

    // DRONE algorithm state
    uint8_t _droneBaseNote = 0;
    uint8_t _droneInterval = 0;
    uint8_t _droneSpeed = 1;  // Safe default to avoid division by zero

    // PHASE algorithm state
    uint32_t _phaseAccum = 0;
    uint32_t _phaseSpeed = 0;
    uint8_t _phasePattern[8] = {0};
    uint8_t _phaseLength = 4;  // Safe default to avoid division by zero

    // RAGA algorithm state
    uint8_t _ragaScale[7] = {0};
    uint8_t _ragaDirection = 0;
    uint8_t _ragaPosition = 0;
    uint8_t _ragaOrnament = 0;

    // AMBIENT algorithm state (13) - Harmonic Drone & Event Scheduler
    int8_t _ambient_root_note;
    int8_t _ambient_drone_notes[3]; // The notes of the drone chord
    int _ambient_event_timer;       // Countdown to the next sparse event
    uint8_t _ambient_event_type;    // 0=none, 1=single note, 2=arpeggio
    uint8_t _ambient_event_step;    // Position within the current event

    // ACID algorithm state (14)
    uint8_t _acidSequence[8] = {0};
    uint8_t _acidPosition = 0;
    uint8_t _acidAccentPattern = 0;
    uint8_t _acidOctaveMask = 0;
    int8_t _acidLastNote = 0;
    uint8_t _acidSlideTarget = 0;
    uint8_t _acidStepCount = 0;

    // DRILL algorithm state (15)
    uint8_t _drillHiHatPattern = 0;
    uint8_t _drillSlideTarget = 0;
    uint8_t _drillTripletMode = 0;
    uint8_t _drillRollCount = 0;
    uint8_t _drillLastNote = 0;
    uint8_t _drillStepInBar = 0;
    uint8_t _drillSubdivision = 1;

    // MINIMAL algorithm state (16)
    uint8_t _minimalBurstLength = 0;
    uint8_t _minimalSilenceLength = 0;
    uint8_t _minimalClickDensity = 0;
    uint8_t _minimalBurstTimer = 0;
    uint8_t _minimalSilenceTimer = 0;
    uint8_t _minimalNoteIndex = 0;
    uint8_t _minimalMode = 0;

    // KRAFT algorithm state (17)
    uint8_t _kraftSequence[8] = {0};
    uint8_t _kraftPosition = 0;
    uint8_t _kraftLockTimer = 0;
    uint8_t _kraftTranspose = 0;
    uint8_t _kraftTranspCount = 0;
    int8_t _kraftBaseNote = 0;
    uint8_t _kraftGhostMask = 0;

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


    // Output state
    bool _activity = false;
    bool _gateOutput = false;
    float _cvOutput = 0.f;

    // Gated CV mode state - tracks last CV value when gate fired
    float _lastGatedCv = 0.f;
};
