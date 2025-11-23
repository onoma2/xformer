#include "TuesdayTrackEngine.h"

#include "Engine.h"
#include "model/Scale.h"

#include <cmath>
#include <algorithm>

// Initialize algorithm state based on Flow (seed1) and Ornament (seed2)
// This mirrors the original Tuesday Init functions
void TuesdayTrackEngine::initAlgorithm() {
    int flow = _tuesdayTrack.flow();
    int ornament = _tuesdayTrack.ornament();
    int algorithm = _tuesdayTrack.algorithm();

    _uiRng = Random(flow * 37 + ornament * 101);

    _cachedFlow = flow;
    _cachedOrnament = ornament;
    _cachedAlgorithm = algorithm;

    switch (algorithm) {
    case 0: // TEST
        // seed1 (flow) determines mode and sweep speed
        // seed2 (ornament) determines accent and velocity
        _testMode = (flow - 1) >> 3;  // 0 or 1 (1-8 = mode 0, 9-16 = mode 1)
        _testSweepSpeed = ((flow - 1) & 0x3);  // 0-3
        _testAccent = (ornament - 1) >> 3;  // 0 or 1
        _testVelocity = ((ornament - 1) << 4);  // 0-240
        _testNote = 0;
        break;

    case 1: // TRITRANCE
        // seed1 (flow) seeds main RNG for b1, b2
        // seed2 (ornament) seeds extra RNG for b3
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);

        _triB1 = (_rng.next() & 0x7);   // High note for case 2
        _triB2 = (_rng.next() & 0x7);   // Phase offset for mod 3

        // b3: note offset for octave 0/1
        _triB3 = (_extraRng.next() & 0x15);
        if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
        _triB3 -= 4;  // Range: -4 to +3
        break;

    case 2: // STOMPER
        // seed2 (ornament) seeds main RNG for note choices
        // seed1 (flow) seeds extra RNG for mode/pattern
        _rng = Random((ornament - 1) << 4);
        _extraRng = Random((flow - 1) << 4);

        _stomperMode = (_extraRng.next() % 7) * 2;  // Initial pattern mode
        _stomperCountDown = 0;
        _stomperLowNote = _rng.next() % 3;
        _stomperLastNote = _stomperLowNote;
        _stomperLastOctave = 0;
        _stomperHighNote[0] = _rng.next() % 7;
        _stomperHighNote[1] = _rng.next() % 5;
        break;

    case 3: // MARKOV
        // Both seeds contribute to matrix generation
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);

        _markovHistory1 = (_rng.next() & 0x7);
        _markovHistory3 = (_rng.next() & 0x7);

        // Generate 8x8x2 Markov transition matrix
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                _markovMatrix[i][j][0] = (_rng.next() % 8);
                _markovMatrix[i][j][1] = (_extraRng.next() % 8);
            }
        }
        break;

    case 4: // CHIPARP
        // Flow seeds main RNG, Ornament seeds chord RNG
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _chipChordSeed = _rng.next();
        _chipRng = Random(_chipChordSeed);
        _chipBase = _rng.next() % 3;
        _chipDir = _extraRng.nextBinary() ? 1 : 0;
        break;

    case 5: // GOACID
        // Flow seeds main RNG, Ornament seeds extra RNG for transpose
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _goaB1 = _extraRng.nextBinary() ? 1 : 0;
        _goaB2 = _extraRng.nextBinary() ? 1 : 0;
        break;

    case 6: // SNH (Sample & Hold)
        // Flow seeds main RNG, Ornament seeds extra RNG
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _snhPhase = 0;
        _snhPhaseSpeed = 0xffffffff / 16;  // Default speed based on 16 steps
        _snhLastVal = 0;
        _snhTarget = _snhCurrent = _rng.next() << 10;
        _snhCurrentDelta = 0;
        break;

    case 7: // WOBBLE
        // Flow seeds main RNG, Ornament seeds extra RNG
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _wobblePhase = 0;
        _wobblePhaseSpeed = 0xffffffff / 16;  // Default based on 16 steps
        _wobblePhase2 = 0;
        _wobblePhaseSpeed2 = 0xcfffffff / 4;  // Faster second oscillator
        _wobbleLastWasHigh = 0;
        break;

    case 8: // TECHNO
        // Flow seeds main RNG, Ornament seeds extra RNG
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _technoKickPattern = _rng.next() % 4;  // 4 kick variations
        _technoHatPattern = _extraRng.next() % 4;  // 4 hat variations
        _technoBassNote = _rng.next() % 5;  // Bass note 0-4
        break;

    case 9: // FUNK
        // Flow seeds main RNG, Ornament seeds extra RNG
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _funkPattern = _rng.next() % 8;  // 8 funk patterns
        _funkSyncopation = _extraRng.next() % 4;  // Syncopation level
        _funkGhostProb = 32 + (_extraRng.next() % 64);  // 32-96 ghost note probability
        break;

    case 10: // DRONE
        // Flow seeds main RNG, Ornament seeds extra RNG
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _droneBaseNote = _rng.next() % 12;  // Root note
        _droneInterval = _extraRng.next() % 4;  // 0=unison, 1=5th, 2=octave, 3=5th+octave
        _droneSpeed = 1 + (_rng.next() % 4);  // Change rate 1-4
        break;

    case 11: // PHASE
        // Flow seeds main RNG, Ornament seeds extra RNG
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _phaseAccum = 0;
        _phaseSpeed = 0x1000000 + (_extraRng.next() & 0xffffff);  // Slow phase drift
        _phaseLength = 3 + (_rng.next() % 6);  // Pattern length 3-8
        for (int i = 0; i < 8; i++) {
            _phasePattern[i] = _rng.next() % 8;  // Simple melodic cell
        }
        break;

    case 12: // RAGA
        // Flow seeds main RNG, Ornament seeds extra RNG
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        // Indian pentatonic-ish scales (sa re ga ma pa dha ni)
        {
            int scaleType = _rng.next() % 4;
            switch (scaleType) {
            case 0: // Bhairav-like (morning raga)
                _ragaScale[0] = 0; _ragaScale[1] = 1; _ragaScale[2] = 4;
                _ragaScale[3] = 5; _ragaScale[4] = 7; _ragaScale[5] = 8;
                _ragaScale[6] = 11;
                break;
            case 1: // Yaman-like (evening raga)
                _ragaScale[0] = 0; _ragaScale[1] = 2; _ragaScale[2] = 4;
                _ragaScale[3] = 6; _ragaScale[4] = 7; _ragaScale[5] = 9;
                _ragaScale[6] = 11;
                break;
            case 2: // Todi-like
                _ragaScale[0] = 0; _ragaScale[1] = 1; _ragaScale[2] = 3;
                _ragaScale[3] = 6; _ragaScale[4] = 7; _ragaScale[5] = 8;
                _ragaScale[6] = 11;
                break;
            default: // Kafi-like (Dorian)
                _ragaScale[0] = 0; _ragaScale[1] = 2; _ragaScale[2] = 3;
                _ragaScale[3] = 5; _ragaScale[4] = 7; _ragaScale[5] = 9;
                _ragaScale[6] = 10;
                break;
            }
        }
        _ragaDirection = 0;  // 0=ascending, 1=descending
        _ragaPosition = 0;
        _ragaOrnament = _extraRng.next() % 3;  // Ornament type
        break;

    case 13: // AMBIENT - Harmonic Drone & Event Scheduler
{
    // 1. Set up the drone chord based on Flow. Not random, but deterministic.
    _ambient_root_note = (_tuesdayTrack.flow() - 1) % 12;
    _ambient_drone_notes[0] = _ambient_root_note;
    _ambient_drone_notes[1] = (_ambient_root_note + 7) % 12; // Perfect 5th
    _ambient_drone_notes[2] = (_ambient_root_note + 16) % 12; // A Major 9th (as a 2nd)

    // 2. Init event state using Ornament for randomness
    _extraRng = Random((_tuesdayTrack.ornament() - 1) << 4);
    _ambient_event_timer = 16 + (_extraRng.next() % 48); // First event in 16-64 steps
    _ambient_event_type = 0;
    _ambient_event_step = 0;
    break;
}

    case 15: // DRILL - UK Drill hi-hat rolls and bass slides
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _drillHiHatPattern = 0b10101010;  // Basic hi-hat pattern
        _drillSlideTarget = _rng.next() % 12;
        _drillTripletMode = (ornament > 8) ? 1 : 0;  // High ornament = triplets
        _drillRollCount = 0;
        _drillLastNote = _rng.next() % 5;  // Low bass notes
        _drillStepInBar = 0;
        _drillSubdivision = 1;
        break;

    case 16: // MINIMAL - staccato bursts and silence
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        _minimalBurstLength = 2 + (_rng.next() % 7);  // 2-8 steps
        _minimalSilenceLength = 4 + (flow % 13);  // 4-16 steps
        _minimalClickDensity = ornament * 16;  // 0-255 scale
        _minimalBurstTimer = 0;
        _minimalSilenceTimer = _minimalSilenceLength;  // Start in silence
        _minimalNoteIndex = 0;
        _minimalMode = 0;  // 0=silence, 1=burst
        break;

    case 14: // ACID - 303-style patterns with slides
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        // Generate 8-step acid sequence
        for (int i = 0; i < 8; i++) {
            _acidSequence[i] = _rng.next() % 12;
        }
        _acidPosition = 0;
        _acidAccentPattern = _extraRng.next();  // Random accent pattern
        _acidOctaveMask = _extraRng.next() & 0x33;  // Sparse octave jumps
        _acidLastNote = _acidSequence[0];
        _acidSlideTarget = 0;
        _acidStepCount = 0;
        break;

    case 17: // KRAFT - precise mechanical sequences
        _rng = Random((flow - 1) << 4);
        _extraRng = Random((ornament - 1) << 4);
        // Generate repetitive mechanical pattern
        _kraftBaseNote = _rng.next() % 12;
        for (int i = 0; i < 8; i++) {
            // Kraftwerk patterns often alternate between 2-3 notes
            _kraftSequence[i] = (_kraftBaseNote + ((i % 2) ? 7 : 0)) % 12;
        }
        _kraftPosition = 0;
        _kraftLockTimer = 16 + (_rng.next() % 16);  // Lock for 16-32 steps
        _kraftTranspose = 0;
        _kraftTranspCount = 0;
        _kraftGhostMask = _extraRng.next() & 0x55;  // Every other step ghost
        break;

    case 18: // APHEX - Polyrhythmic Event Sequencer
{
    // 1. Seed the patterns based on Flow, using the main RNG
    _rng = Random((_tuesdayTrack.flow() - 1) << 4);
    for (int i = 0; i < 4; ++i) _aphex_track1_pattern[i] = _rng.next() % 12;
    for (int i = 0; i < 3; ++i) _aphex_track2_pattern[i] = _rng.next() % 3; // 0, 1, or 2
    for (int i = 0; i < 5; ++i) _aphex_track3_pattern[i] = (_rng.next() % 8 == 0) ? (_rng.next() % 5) : 0; // Sparse bass notes

    // 2. Set initial phase based on Ornament
    int ornament_val = _tuesdayTrack.ornament();
    _aphex_pos1 = (ornament_val * 1) % 4;
    _aphex_pos2 = (ornament_val * 2) % 3;
    _aphex_pos3 = (ornament_val * 3) % 5;
    break;
}

    case 19: // AUTECHRE - Algorithmic Transformation Engine
{
    // 1. Start with a dead-simple, non-random pattern
    _autechre_pattern[0] = 0;
    _autechre_pattern[1] = 0;
    _autechre_pattern[2] = 0;
    _autechre_pattern[3] = 0;
    _autechre_pattern[4] = 0;
    _autechre_pattern[5] = 0;
    _autechre_pattern[6] = 7;
    _autechre_pattern[7] = 0;
    // 2. Set rule timing based on Flow
    _autechre_rule_timer = 8 + (_tuesdayTrack.flow() * 4);

    // 3. Create a sequence of rules based on Ornament
    _rng = Random((_tuesdayTrack.ornament() - 1) << 4);
    for (int i = 0; i < 8; ++i) _autechre_rule_sequence[i] = _rng.next() % 5; // 5 different rules
    _autechre_rule_index = 0;
    break;
}

    default:
        break;
    }
}

void TuesdayTrackEngine::reset() {
    _cachedAlgorithm = -1;
    _cachedFlow = -1;
    _cachedOrnament = -1;
    _cachedLoopLength = -1;

    _stepIndex = 0;
    _displayStep = -1;  // No step displayed until first tick
    _gateLength = 0;
    _gateTicks = 0;
    _coolDown = 0;
    _coolDownMax = 0;
    _gatePercent = 75;
    _gateOffset = 0;
    _slide = 0;
    _cvTarget = 0.f;
    _cvCurrent = 0.f;
    _cvDelta = 0.f;
    _slideCountDown = 0;

    // Gate timing state
    _gateLengthTicks = 0;
    _pendingGateOffsetTicks = 0;
    _pendingGateActivation = false;

    // Retrigger/Trill State
    _retriggerArmed = false;
    _retriggerCount = 0;
    _retriggerPeriod = 0;
    _retriggerLength = 0;
    _retriggerTimer = 0;
    _isTrillNote = false;
    _trillCvTarget = 0.f;

    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;
    _lastGatedCv = 0.f;
    _bufferValid = false;

    // Re-initialize algorithm with current seeds
    initAlgorithm();
}

void TuesdayTrackEngine::restart() {
    reset();
}

void TuesdayTrackEngine::reseed() {
    // Reset step to beginning
    _stepIndex = 0;
    _coolDown = 0;

    // Generate new random seeds for a fresh pattern
    // Use current RNG state to generate new seeds, giving variety
    uint32_t newSeed1 = _rng.next();
    uint32_t newSeed2 = _extraRng.next();

    // Reinitialize RNGs with new random seeds
    _rng = Random(newSeed1);
    _extraRng = Random(newSeed2);

    // Reinitialize algorithm state with new RNG values
    int algorithm = _tuesdayTrack.algorithm();

    switch (algorithm) {
    case 0: // TEST
        _testMode = (_rng.next() & 0x1);
        _testSweepSpeed = (_rng.next() & 0x3);
        _testAccent = (_rng.next() & 0x1);
        _testVelocity = (_rng.next() & 0xF0);
        _testNote = 0;
        break;

    case 1: // TRITRANCE
        _triB1 = (_rng.next() & 0x7);
        _triB2 = (_rng.next() & 0x7);
        _triB3 = (_extraRng.next() & 0x15);
        if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
        _triB3 -= 4;
        break;

    case 2: // STOMPER
        _stomperMode = (_rng.next() % 7) * 2;
        _stomperCountDown = 0;
        _stomperLowNote = _rng.next() % 3;
        _stomperLastNote = _stomperLowNote;
        _stomperLastOctave = 0;
        _stomperHighNote[0] = _rng.next() % 7;
        _stomperHighNote[1] = _rng.next() % 5;
        break;

    case 3: // MARKOV
        _markovHistory1 = (_rng.next() & 0x7);
        _markovHistory3 = (_rng.next() & 0x7);
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                _markovMatrix[i][j][0] = (_rng.next() % 8);
                _markovMatrix[i][j][1] = (_extraRng.next() % 8);
            }
        }
        break;

    case 4: // CHIPARP
        _chipChordSeed = _rng.next();
        _chipRng = Random(_chipChordSeed);
        _chipBase = _rng.next() % 3;
        _chipDir = _extraRng.nextBinary() ? 1 : 0;
        break;

    case 5: // GOACID
        _goaB1 = _extraRng.nextBinary() ? 1 : 0;
        _goaB2 = _extraRng.nextBinary() ? 1 : 0;
        break;

    case 6: // SNH
        _snhPhase = 0;
        _snhLastVal = 0;
        _snhTarget = _snhCurrent = _rng.next() << 10;
        _snhCurrentDelta = 0;
        break;

    case 7: // WOBBLE
        _wobblePhase = 0;
        _wobblePhase2 = 0;
        _wobbleLastWasHigh = 0;
        break;

    case 8: // TECHNO
        _technoKickPattern = _rng.next() % 4;
        _technoHatPattern = _extraRng.next() % 4;
        _technoBassNote = _rng.next() % 5;
        break;

    case 9: // FUNK
        _funkPattern = _rng.next() % 8;
        _funkSyncopation = _extraRng.next() % 4;
        _funkGhostProb = 32 + (_extraRng.next() % 64);
        break;

    case 10: // DRONE
        _droneBaseNote = _rng.next() % 12;
        _droneInterval = _extraRng.next() % 4;
        _droneSpeed = 1 + (_rng.next() % 4);
        break;

    case 11: // PHASE
        _phaseAccum = 0;
        _phaseSpeed = 0x1000000 + (_extraRng.next() & 0xffffff);
        _phaseLength = 3 + (_rng.next() % 6);
        for (int i = 0; i < 8; i++) {
            _phasePattern[i] = _rng.next() % 8;
        }
        break;

    case 12: // RAGA
        {
            int scaleType = _rng.next() % 4;
            switch (scaleType) {
            case 0:
                _ragaScale[0] = 0; _ragaScale[1] = 1; _ragaScale[2] = 4;
                _ragaScale[3] = 5; _ragaScale[4] = 7; _ragaScale[5] = 8;
                _ragaScale[6] = 11;
                break;
            case 1:
                _ragaScale[0] = 0; _ragaScale[1] = 2; _ragaScale[2] = 4;
                _ragaScale[3] = 6; _ragaScale[4] = 7; _ragaScale[5] = 9;
                _ragaScale[6] = 11;
                break;
            case 2:
                _ragaScale[0] = 0; _ragaScale[1] = 1; _ragaScale[2] = 3;
                _ragaScale[3] = 6; _ragaScale[4] = 7; _ragaScale[5] = 8;
                _ragaScale[6] = 11;
                break;
            default:
                _ragaScale[0] = 0; _ragaScale[1] = 2; _ragaScale[2] = 3;
                _ragaScale[3] = 5; _ragaScale[4] = 7; _ragaScale[5] = 9;
                _ragaScale[6] = 10;
                break;
            }
        }
        _ragaDirection = 0;
        _ragaPosition = 0;
        _ragaOrnament = _extraRng.next() % 3;
        break;

    case 13: // AMBIENT
    case 18: // APHEX
    case 19: // AUTECH
        // These algorithms are fully deterministic from flow/ornament,
        // so reseed just re-initializes with the same parameters.
        initAlgorithm();
        break;

    case 15: // DRILL
        _drillHiHatPattern = 0b10101010 | (_rng.next() & 0x55);  // Vary pattern
        _drillSlideTarget = _rng.next() % 12;
        _drillTripletMode = _extraRng.nextBinary();
        _drillRollCount = 0;
        _drillLastNote = _rng.next() % 5;
        _drillStepInBar = 0;
        _drillSubdivision = 1;
        break;

    case 16: // MINIMAL
        _minimalBurstLength = 2 + (_rng.next() % 7);
        _minimalSilenceLength = 4 + (_rng.next() % 13);
        _minimalClickDensity = _extraRng.next();
        _minimalBurstTimer = 0;
        _minimalSilenceTimer = _minimalSilenceLength;
        _minimalNoteIndex = 0;
        _minimalMode = 0;
        break;

    case 14: // ACID
        for (int i = 0; i < 8; i++) {
            _acidSequence[i] = _rng.next() % 12;
        }
        _acidPosition = 0;
        _acidAccentPattern = _extraRng.next();
        _acidOctaveMask = _extraRng.next() & 0x33;
        _acidLastNote = _acidSequence[0];
        _acidSlideTarget = 0;
        _acidStepCount = 0;
        break;

    case 17: // KRAFT
        _kraftBaseNote = _rng.next() % 12;
        for (int i = 0; i < 8; i++) {
            _kraftSequence[i] = (_kraftBaseNote + ((i % 2) ? 7 : 0)) % 12;
        }
        _kraftPosition = 0;
        _kraftLockTimer = 16 + (_rng.next() % 16);
        _kraftTranspose = 0;
        _kraftTranspCount = 0;
        _kraftGhostMask = _extraRng.next() & 0x55;
        break;

    default:
        break;
    }
}

void TuesdayTrackEngine::generateBuffer() {
    // Initialize algorithm fresh to get deterministic pattern
    initAlgorithm();

    int algorithm = _tuesdayTrack.algorithm();
    int glide = _tuesdayTrack.glide();

    // Warmup phase: run algorithm for 256 steps to get mature pattern
    // This allows capturing evolved patterns instead of initial state
    // Must match exact RNG consumption pattern of buffer generation
    const int WARMUP_STEPS = 256;
    for (int step = 0; step < WARMUP_STEPS; step++) {
        switch (algorithm) {
        case 0: // TEST
            {
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    // consume RNG for slide
                }
                switch (_testMode) {
                case 1:  // SCALEWALKER
                    _testNote = (_testNote + 1) % 12;
                    break;
                }
            }
            break;

        case 1: // TRITRANCE
            {
                // Gate length - must match exact consumption pattern
                int gateLengthChoice = _rng.nextRange(100);
                if (gateLengthChoice < 40) {
                    _rng.nextRange(4);
                } else if (gateLengthChoice < 70) {
                    _rng.nextRange(4);
                } else {
                    _rng.nextRange(9);
                }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }

                int phase = (step + _triB2) % 3;
                switch (phase) {
                case 0:
                    if (_extraRng.nextBinary() && _extraRng.nextBinary()) {
                        _triB3 = (_extraRng.next() & 0x15);
                        if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
                        _triB3 -= 4;
                    }
                    break;
                case 1:
                    if (_rng.nextBinary()) {
                        _triB2 = (_rng.next() & 0x7);
                    }
                    break;
                case 2:
                    if (_rng.nextBinary()) {
                        _triB1 = ((_rng.next() >> 5) & 0x7);
                    }
                    break;
                }
            }
            break;

        case 2: // STOMPER - warmup state machine
            {
                if (_stomperCountDown > 0) {
                    _stomperCountDown--;
                } else {
                    if (_stomperMode >= 14) {
                        _stomperMode = (_extraRng.next() % 7) * 2;
                    }

                    _rng.nextRange(100);
                    _stomperLowNote = _rng.next() % 3;
                    _rng.nextRange(100);
                    _stomperHighNote[0] = _rng.next() % 7;
                    _rng.nextRange(100);
                    _stomperHighNote[1] = _rng.next() % 5;

                    int maxticklen = 2;

                    switch (_stomperMode) {
                    case 10:
                        _stomperLastNote = _stomperHighNote[_rng.next() % 2];
                        _stomperLastOctave = 1;
                        _stomperMode++;
                        break;
                    case 11:
                        _stomperLastNote = _stomperLowNote;
                        _stomperLastOctave = 0;
                        // Match RNG pattern: nextRange(100) then conditional nextRange(3)
                        if (glide > 0 && _rng.nextRange(100) < glide) {
                            _rng.nextRange(3);
                        }
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 12:
                        _stomperLastNote = _stomperLowNote;
                        _stomperLastOctave = 0;
                        _stomperMode++;
                        break;
                    case 13:
                        _stomperLastNote = _stomperHighNote[_rng.next() % 2];
                        _stomperLastOctave = 1;
                        // Match RNG pattern: nextRange(100) then conditional nextRange(3)
                        if (glide > 0 && _rng.nextRange(100) < glide) {
                            _rng.nextRange(3);
                        }
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 4:
                    case 5:
                        _stomperLastNote = _stomperLowNote;
                        _stomperLastOctave = 0;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 0:
                    case 1:
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 6:
                    case 7:
                        _stomperLastNote = _stomperLowNote;
                        _stomperLastOctave = 0;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 8:
                        _stomperLastNote = _stomperHighNote[_rng.next() % 2];
                        _stomperLastOctave = 1;
                        _stomperMode++;
                        break;
                    case 9:
                        _stomperLastNote = _stomperLowNote;
                        _stomperLastOctave = 0;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 2:
                        _stomperLastNote = _stomperLowNote;
                        _stomperLastOctave = 0;
                        _stomperMode++;
                        break;
                    case 3:
                        _stomperLastNote = _stomperHighNote[_rng.next() % 2];
                        _stomperLastOctave = 1;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    default:
                        break;
                    }
                }
            }
            break;

        case 3: // MARKOV
            {
                // Gate length - must match exact consumption pattern
                int gateLengthChoice = _rng.nextRange(100);
                if (gateLengthChoice < 40) {
                    _rng.nextRange(4);
                } else if (gateLengthChoice < 70) {
                    _rng.nextRange(4);
                } else {
                    _rng.nextRange(9);
                }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }

                int idx = _rng.nextBinary() ? 1 : 0;
                int note = _markovMatrix[_markovHistory1][_markovHistory3][idx];
                _markovHistory1 = _markovHistory3;
                _markovHistory3 = note;
                _rng.nextBinary();  // octave
            }
            break;

        case 4: // CHIPARP warmup
            {
                int chordpos = step % 4;  // TPB=4 default
                if (chordpos == 0) {
                    _chipRng = Random(_chipChordSeed);
                    if (_rng.nextRange(256) < 0x20) {
                        _chipBase = _rng.next() % 3;
                    }
                    if (_rng.nextRange(256) < 0xf0) {
                        _chipDir = _extraRng.nextBinary() ? 1 : 0;
                    }
                }
                _chipRng.nextRange(256);  // accent check
                _chipRng.nextRange(256);  // slide check
                _chipRng.nextRange(256);  // noteoff check
                _chipRng.nextRange(256);  // gate length
                _extraRng.nextRange(256);  // velocity

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 5: // GOACID warmup
            {
                _rng.nextRange(256);  // velocity
                _extraRng.nextBinary();  // accent
                _rng.next();  // note selection

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 6: // SNH warmup
            {
                _snhPhase += _snhPhaseSpeed;
                int v = _snhPhase >> 30;
                if (v != _snhLastVal) {
                    _snhLastVal = v;
                    _snhTarget = _rng.next() << 10;
                }
                int newdelta = (_snhTarget - _snhCurrent) / 100;
                _snhCurrentDelta += ((newdelta - _snhCurrentDelta) * 100) / 200;
                _snhCurrent += _snhCurrentDelta * 100;

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
                _extraRng.next();  // velocity
            }
            break;

        case 7: // WOBBLE warmup
            {
                _wobblePhase += _wobblePhaseSpeed;
                _wobblePhase2 += _wobblePhaseSpeed2;

                if (_rng.nextRange(256) >= 128) {  // PercChance analog
                    if (_wobbleLastWasHigh == 0) {
                        if (_rng.nextRange(256) >= 56) {
                            _rng.next();  // slide
                        }
                    }
                    _wobbleLastWasHigh = 1;
                } else {
                    if (_wobbleLastWasHigh == 1) {
                        if (_rng.nextRange(256) >= 56) {
                            _rng.next();  // slide
                        }
                    }
                    _wobbleLastWasHigh = 0;
                }
                _extraRng.next();  // velocity

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 8: // TECHNO warmup
            {
                _rng.next();  // pattern position
                _extraRng.next();  // hat variation
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 9: // FUNK warmup
            {
                _rng.next();  // pattern note
                _extraRng.nextRange(256);  // ghost check
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 10: // DRONE warmup
            {
                _rng.next();  // interval variation
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 11: // PHASE warmup
            {
                _phaseAccum += _phaseSpeed;
                _rng.next();  // consume for determinism
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 12: // RAGA warmup
            {
                _rng.next();  // note selection
                _extraRng.next();  // ornament
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 13: // AMBIENT warmup
            {
                if (_ambient_event_type == 1) {
                    _ambient_event_type = 0;
                } else if (_ambient_event_type == 2) {
                    _ambient_event_step++;
                    if (_ambient_event_step >= 3) {
                        _ambient_event_type = 0;
                    }
                } else {
                    _ambient_event_timer--;
                }

                if (_ambient_event_timer <= 0) {
                    _ambient_event_type = 1 + (_extraRng.next() % 2);
                    _ambient_event_step = 0;
                    int power = _tuesdayTrack.power();
                    _ambient_event_timer = 16 + (power > 0 ? 256 - (power * 15) : 256);
                }
            }
            break;

        case 15: // DRILL warmup
            {
                // Hi-hat pattern step
                _drillStepInBar = (_drillStepInBar + 1) % 8;

                // Consume RNG for pattern variation
                if (_rng.nextRange(16) < 4) {
                    _drillHiHatPattern ^= (1 << (_drillStepInBar % 8));  // Toggle bit
                }

                // Slide probability (flow controlled)
                if (_extraRng.nextRange(16) < 8) {
                    _drillSlideTarget = _rng.next() % 12;
                }

                // Glide check
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 16: // MINIMAL warmup
            {
                // Mode state machine: silence → burst → silence
                if (_minimalMode == 0) {
                    // Silence mode
                    if (_minimalSilenceTimer > 0) {
                        _minimalSilenceTimer--;
                        _rng.next();  // consume for determinism
                    } else {
                        _minimalMode = 1;  // Switch to burst
                        _minimalBurstTimer = _minimalBurstLength;
                        _minimalNoteIndex = 0;
                    }
                } else {
                    // Burst mode
                    if (_minimalBurstTimer > 0) {
                        _minimalBurstTimer--;
                        _minimalNoteIndex++;
                        _rng.next();  // note selection
                        _extraRng.next();  // glitch check
                    } else {
                        _minimalMode = 0;  // Switch to silence
                        _minimalSilenceTimer = _minimalSilenceLength;
                    }
                }

                // Glide check
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 14: // ACID warmup
            {
                // Advance through sequence
                _acidPosition = (_acidPosition + 1) % 8;
                _acidStepCount++;

                // Consume RNG for accent and octave checks
                _rng.next();  // note variation
                if (_acidAccentPattern & (1 << _acidPosition)) {
                    _extraRng.next();  // accent
                }
                if (_acidOctaveMask & (1 << _acidPosition)) {
                    _extraRng.next();  // octave
                }

                // Slide check
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }

                // Pattern mutation
                if (_rng.nextRange(128) < 2) {
                    _acidSequence[_rng.next() % 8] = _rng.next() % 12;
                }
            }
            break;

        case 17: // KRAFT warmup
            {
                // Advance through sequence
                _kraftPosition = (_kraftPosition + 1) % 8;

                // Lock timer countdown
                if (_kraftLockTimer > 0) {
                    _kraftLockTimer--;
                } else {
                    // Regenerate pattern when lock expires
                    _kraftLockTimer = 16 + (_rng.next() % 16);
                    _kraftBaseNote = (_kraftBaseNote + _rng.nextRange(5)) % 12;
                }

                // Transpose check
                if (_rng.nextRange(16) < 4) {
                    _kraftTranspose = _rng.next() % 12;
                    _kraftTranspCount++;
                }

                // Ghost note check
                _extraRng.next();

                // Glide check
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _rng.nextRange(3);
                }
            }
            break;

        case 18: // APHEX warmup
            {
                _aphex_pos1 = (_aphex_pos1 + 1) % 4;
                _aphex_pos2 = (_aphex_pos2 + 1) % 3;
                _aphex_pos3 = (_aphex_pos3 + 1) % 5;
            }
            break;

        case 19: // AUTECH warmup
            {
                _autechre_rule_timer--;
                if (_autechre_rule_timer <= 0) {
                    uint8_t current_rule = _autechre_rule_sequence[_autechre_rule_index];
                    int intensity = _tuesdayTrack.power() / 2; // Power: 0-8

                    switch (current_rule) {
                        case 0: // ROTATE
                            {
                                int8_t temp = _autechre_pattern[7];
                                for (int i = 7; i > 0; --i) _autechre_pattern[i] = _autechre_pattern[i-1];
                                _autechre_pattern[0] = temp;
                            }
                            break;
                        case 1: // REVERSE
                            for (int i = 0; i < 4; ++i) {
                                int8_t temp = _autechre_pattern[i];
                                _autechre_pattern[i] = _autechre_pattern[7-i];
                                _autechre_pattern[7-i] = temp;
                            }
                            break;
                        case 2: // INVERT around a pivot (e.g., note 6)
                            for (int i = 0; i < 8; ++i) {
                                int interval = _autechre_pattern[i] - 6;
                                _autechre_pattern[i] = (6 - interval + 12) % 12;
                            }
                            break;
                        case 3: // SWAP adjacent notes
                            for (int i = 0; i < 8; i += 2) {
                                int8_t temp = _autechre_pattern[i];
                                _autechre_pattern[i] = _autechre_pattern[i+1];
                                _autechre_pattern[i+1] = temp;
                            }
                            break;
                        case 4: // ADD intensity
                             for (int i = 0; i < 8; ++i) _autechre_pattern[i] = (_autechre_pattern[i] + intensity) % 12;
                            break;
                    }

                    _autechre_rule_timer = 8 + (_tuesdayTrack.flow() * 4);
                    _autechre_rule_index = (_autechre_rule_index + 1) % 8;
                }
            }
            break;

        default:
            break;
        }
    }

    // Generate 128 steps into buffer (now capturing mature pattern)
    for (int step = 0; step < BUFFER_SIZE; step++) {
        int note = 0;
        int octave = 0;
        uint8_t gatePercent = 75;
        uint8_t slide = 0;
        uint8_t gateOffset = 0;  // Initialize default value for gateOffset

        switch (algorithm) {
        case 0: // TEST
            {
                gatePercent = 75;
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = _testSweepSpeed + 1;
                }

                switch (_testMode) {
                case 0:  // OCTSWEEPS
                    octave = (step % 5);
                    note = 0;
                    // TEST: Apply timing variations based on mode
                    gateOffset = (step % 4) * 10;  // Simple periodic timing variations
                    break;
                case 1:  // SCALEWALKER
                default:
                    octave = 0;
                    note = _testNote;
                    _testNote = (_testNote + 1) % 12;
                    // TEST: Apply timing variations based on position in scale walk
                    gateOffset = (step % 3) * 20;  // Different timing for each step in triad
                    break;
                }
            }
            break;

        case 1: // TRITRANCE
            {
                int gateLengthChoice = _rng.nextRange(100);
                if (gateLengthChoice < 40) {
                    gatePercent = 50 + (_rng.nextRange(4) * 12);  // 50%, 62%, 74%, 86%
                } else if (gateLengthChoice < 70) {
                    gatePercent = 100 + (_rng.nextRange(4) * 25); // 100%, 125%, 150%, 175%
                } else {
                    gatePercent = 200 + (_rng.nextRange(9) * 25); // 200% to 400%
                }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }

                // TRITRANCE: Generate phase-based timing variations for swing feel
                int phase = (step + _triB2) % 3;
                switch (phase) {
                case 0:
                    // First phase: slightly early timing
                    gateOffset = 10 + (_rng.nextRange(15));  // 10-25% early
                    break;
                case 1:
                    // Second phase: on-time or slightly late
                    gateOffset = 45 + (_rng.nextRange(10));  // 45-55% timing
                    break;
                case 2:
                    // Third phase: swing delay
                    gateOffset = 60 + (_rng.nextRange(20));  // 60-80% delay
                    break;
                }
                switch (phase) {
                case 0:
                    if (_extraRng.nextBinary() && _extraRng.nextBinary()) {
                        _triB3 = (_extraRng.next() & 0x15);
                        if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
                        _triB3 -= 4;
                    }
                    octave = 0;
                    note = _triB3 + 4;
                    break;
                case 1:
                    octave = 1;
                    note = _triB3 + 4;
                    if (_rng.nextBinary()) {
                        _triB2 = (_rng.next() & 0x7);
                    }
                    break;
                case 2:
                    octave = 2;
                    note = _triB1;
                    if (_rng.nextBinary()) {
                        _triB1 = ((_rng.next() >> 5) & 0x7);
                    }
                    break;
                }

                if (note < 0) note = 0;
                if (note > 11) note = 11;
            }
            break;

        case 2: // STOMPER - buffer generation
            {
                gatePercent = 75;
                slide = 0;

                if (_stomperCountDown > 0) {
                    gatePercent = _stomperCountDown * 25;
                    _stomperCountDown--;
                    // Still generate a note but mark as rest
                    note = _stomperLastNote;
                    octave = _stomperLastOctave;
                } else {
                    if (_stomperMode >= 14) {
                        _stomperMode = (_extraRng.next() % 7) * 2;
                    }

                    _rng.nextRange(100);
                    _stomperLowNote = _rng.next() % 3;
                    _rng.nextRange(100);
                    _stomperHighNote[0] = _rng.next() % 7;
                    _rng.nextRange(100);
                    _stomperHighNote[1] = _rng.next() % 5;

                    int maxticklen = 2;

                    switch (_stomperMode) {
                    case 10:
                        octave = 1;
                        note = _stomperHighNote[_rng.next() % 2];
                        _stomperMode++;
                        break;
                    case 11:
                        octave = 0;
                        note = _stomperLowNote;
                        if (glide > 0 && _rng.nextRange(100) < glide) {
                            slide = (_rng.nextRange(3)) + 1;
                        }
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 12:
                        octave = 0;
                        note = _stomperLowNote;
                        _stomperMode++;
                        break;
                    case 13:
                        octave = 1;
                        note = _stomperHighNote[_rng.next() % 2];
                        if (glide > 0 && _rng.nextRange(100) < glide) {
                            slide = (_rng.nextRange(3)) + 1;
                        }
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 4:
                    case 5:
                        octave = 0;
                        note = _stomperLowNote;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 0:
                    case 1:
                        octave = _stomperLastOctave;
                        note = _stomperLastNote;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 6:
                    case 7:
                        octave = 0;
                        note = _stomperLowNote;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 8:
                        octave = 1;
                        note = _stomperHighNote[_rng.next() % 2];
                        _stomperMode++;
                        break;
                    case 9:
                        octave = 0;
                        note = _stomperLowNote;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    case 2:
                        octave = 0;
                        note = _stomperLowNote;
                        _stomperMode++;
                        break;
                    case 3:
                        octave = 1;
                        note = _stomperHighNote[_rng.next() % 2];
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;
                        break;
                    default:
                        octave = _stomperLastOctave;
                        note = _stomperLastNote;
                        break;
                    }

                    _stomperLastNote = note;
                    _stomperLastOctave = octave;
                }

                if (note < 0) note = 0;
                if (note > 11) note = 11;

                // STOMPER: Apply timing variations that complement the stomping rhythm
                // Different timing based on the current mode
                switch (_stomperMode) {
                case 0: case 1: case 2: case 3:
                    gateOffset = 5;   // Early timing for foundational notes
                    break;
                case 4: case 5: case 6: case 7:
                    gateOffset = 25;  // Mid-range timing for build-up
                    break;
                case 8: case 9: case 10: case 11:
                    gateOffset = 75;  // Later timing for climax
                    break;
                case 12: case 13:
                    gateOffset = 15;  // Moderate timing for transitions
                    break;
                case 14:
                default:
                    gateOffset = 0;   // On-beat timing for steady sections
                    break;
                }
            }
            break;

        case 3: // MARKOV
            {
                int gateLengthChoice = _rng.nextRange(100);
                if (gateLengthChoice < 40) {
                    gatePercent = 50 + (_rng.nextRange(4) * 12);
                } else if (gateLengthChoice < 70) {
                    gatePercent = 100 + (_rng.nextRange(4) * 25);
                } else {
                    gatePercent = 200 + (_rng.nextRange(9) * 25);
                }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }

                int idx = _rng.nextBinary() ? 1 : 0;
                note = _markovMatrix[_markovHistory1][_markovHistory3][idx];
                _markovHistory1 = _markovHistory3;
                _markovHistory3 = note;
                octave = _rng.nextBinary() ? 1 : 0;

                // MARKOV: Use transition probabilities to determine timing variations
                // Based on which transition was taken, generate different timing offsets
                // Transition from _markovHistory1 to _markovHistory3 may influence timing
                int transition_delta = abs(note - _markovHistory3);
                int history_factor = (_markovHistory1 + _markovHistory3) % 11;  // Keep in 0-10 range
                gateOffset = (transition_delta * 10) + (history_factor * 2);  // Scale factors for 0-100 range
                if (gateOffset > 100) gateOffset = 100;
            }
            break;

        case 4: // CHIPARP buffer generation
            {
                gatePercent = 75;
                int chordpos = step % 4;

                if (chordpos == 0) {
                    _chipRng = Random(_chipChordSeed);
                    if (_rng.nextRange(256) < 0x20) {
                        _chipBase = _rng.next() % 3;
                    }
                    if (_rng.nextRange(256) < 0xf0) {
                        _chipDir = _extraRng.nextBinary() ? 1 : 0;
                    }
                }

                int pos = chordpos;
                if (_chipDir == 1) {
                    pos = 3 - chordpos;
                }

                if (_chipRng.nextRange(256) < 0x20) {
                    // accent
                }
                // Algorithm's slide logic - only apply if glide > 0
                if (_chipRng.nextRange(256) < 0x80) {
                    int algoSlide = _chipRng.nextRange(256) % 3;
                    if (glide > 0) {
                        slide = algoSlide;
                    }
                }
                if (_chipRng.nextRange(256) >= 0xd0) {
                    note = 0;  // Note off (~19% chance)
                    gatePercent = 0;
                } else {
                    note = (pos * 2) + _chipBase;
                    gatePercent = 50 + 25 * (_chipRng.nextRange(256) % 3);
                }
                octave = 0;
                _extraRng.nextRange(256);  // velocity

                // Additional slide from glide parameter
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }

                // CHIPARP: Apply chiptune-style timing variations
                // Use the chord position to create arpeggio timing patterns
                if (pos == 0) {
                    gateOffset = 0;    // Root note on-beat
                } else if (pos == 1) {
                    gateOffset = 10;   // Second note slightly delayed
                } else if (pos == 2) {
                    gateOffset = 30;   // Third note with more delay
                } else {  // pos == 3
                    gateOffset = 50;   // Fourth note with even more delay for arpeggio feel
                }
            }
            break;

        case 5: // GOACID buffer generation
            {
                gatePercent = 75;
                _rng.nextRange(256);  // velocity (consumed but not used)
                bool accent = _extraRng.nextBinary();

                int randNote = _rng.next() % 8;
                // Original uses signed char: 0xf4=-12, 0xfe=-2, 0xf2=-14
                switch (randNote) {
                case 0:
                case 2:
                    note = 0; break;
                case 1: note = -12; break;  // 0xf4
                case 3: note = 1; break;
                case 4: note = 3; break;
                case 5: note = 7; break;
                case 6: note = 12; break;  // 0xc
                case 7: note = 13; break;  // 0xd
                }

                if (accent) {
                    switch (randNote) {
                    case 0:
                    case 3:
                    case 7: note = 0; break;
                    case 1: note = -12; break;  // 0xf4
                    case 2: note = -2; break;   // 0xfe
                    case 4: note = 3; break;
                    case 5: note = -14; break;  // 0xf2
                    case 6: note = 1; break;
                    }
                }

                // Apply pattern transpose
                if (_goaB1 && (step % 16) <= 7) {
                    note += 3;
                }
                if (_goaB2 && (step % 16) <= 7) {
                    note -= 5;
                }

                // Add +24 semitones (2 octaves) as in original
                note += 24;

                // Convert to note/octave
                while (note < 0) { note += 12; octave--; }
                while (note >= 12) { note -= 12; octave++; }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }

                // GOACID: Apply Goa/psytrance-style timing variations
                // Create rhythmic shifts that complement the acid patterns
                int stepInBar = step % 16;
                if (stepInBar == 0 || stepInBar == 8) {
                    gateOffset = 0;    // On strong beats (1st and 3rd beat), on-time
                } else if (stepInBar % 4 == 0) {
                    gateOffset = 10;   // On weak beats (2nd and 4th beat), slight delay
                } else {
                    gateOffset = 25 + (stepInBar % 5);  // Off-beat patterns with varying delays
                }
            }
            break;

        case 6: // SNH buffer generation
            {
                gatePercent = 75;
                _snhPhase += _snhPhaseSpeed;
                int v = _snhPhase >> 30;

                if (v != _snhLastVal) {
                    _snhLastVal = v;
                    _snhTarget = _rng.next() << 10;
                }
                int newdelta = (_snhTarget - _snhCurrent) / 100;
                _snhCurrentDelta += ((newdelta - _snhCurrentDelta) * 100) / 200;
                _snhCurrent += _snhCurrentDelta * 100;

                // Convert filtered value to note
                int absVal = _snhCurrent;
                if (absVal < 0) absVal = -absVal;
                note = (absVal >> 22) % 12;
                octave = 0;

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }
                _extraRng.next();  // velocity

                // SNH: Apply Sample & Hold timing variations
                // Based on the current phase position for randomness
                gateOffset = (_snhPhase >> 24) % 100;  // Use phase bits for random timing delay
            }
            break;

        case 7: // WOBBLE buffer generation
            {
                gatePercent = 75;
                _wobblePhase += _wobblePhaseSpeed;
                _wobblePhase2 += _wobblePhaseSpeed2;

                if (_rng.nextRange(256) >= 128) {  // High phase
                    int32_t pos2 = _wobblePhase2 >> 27;
                    note = pos2 % 8;
                    if (_wobbleLastWasHigh == 0) {
                        if (_rng.nextRange(256) >= 56) {
                            int algoSlide = _rng.next() % 3;
                            if (glide > 0) {
                                slide = algoSlide;
                            }
                        }
                    }
                    _wobbleLastWasHigh = 1;
                } else {  // Low phase
                    int32_t pos = _wobblePhase >> 27;
                    note = pos % 8;
                    if (_wobbleLastWasHigh == 1) {
                        if (_rng.nextRange(256) >= 56) {
                            int algoSlide = _rng.next() % 3;
                            if (glide > 0) {
                                slide = algoSlide;
                            }
                        }
                    }
                    _wobbleLastWasHigh = 0;
                }
                octave = 0;
                _extraRng.next();  // velocity

                // Additional slide from glide parameter
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }

                // WOBBLE: Apply wobble timing variations based on dual phase system
                // Use both phase positions to create complex timing shifts
                uint32_t phaseSum = (_wobblePhase + _wobblePhase2) >> 25;
                gateOffset = phaseSum % 100;  // Use combined phase for timing variation
            }
            break;

        case 8: // TECHNO buffer generation - Four-on-floor club
            {
                gatePercent = 75;
                int beatPos = step % 4;  // Position within beat
                int barPos = step % 16;  // Position within bar

                // Four-on-floor kick pattern variations
                bool isKick = false;
                switch (_technoKickPattern) {
                case 0: isKick = (beatPos == 0); break;  // Basic 4/4
                case 1: isKick = (beatPos == 0) || (barPos == 14); break;  // With pickup
                case 2: isKick = (beatPos == 0) || (barPos == 6); break;  // With offbeat
                case 3: isKick = (beatPos == 0) || (barPos == 3) || (barPos == 11); break;  // Syncopated
                }

                if (isKick) {
                    note = _technoBassNote;  // Bass note 0-4
                    octave = 0;
                    gatePercent = 80;
                } else {
                    // Hi-hat patterns on off-beats
                    bool isHat = false;
                    switch (_technoHatPattern) {
                    case 0: isHat = (beatPos == 2); break;  // Off-beat hats
                    case 1: isHat = (beatPos == 1) || (beatPos == 3); break;  // 8th notes
                    case 2: isHat = true; break;  // 16th notes
                    case 3: isHat = (beatPos != 0) && (_rng.next() % 3 != 0); break;  // Random 16ths
                    }
                    if (isHat) {
                        note = 7 + (_extraRng.next() % 3);  // Higher notes for hats
                        octave = 1;
                        gatePercent = 40;
                    } else {
                        note = 0;
                        octave = 0;
                        gatePercent = 0;
                    }
                }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }

                // TECHNO: Apply four-on-floor timing variations
                // Emphasize kick drum on beats, add slight timing shifts for groove
                if (isKick) {
                    gateOffset = (barPos == 0) ? 0 : 5;  // Kick on beat 1 is on-time, others slightly delayed
                } else if (_extraRng.next() % 2) {  // Use a generic condition since hat pattern is complex
                    gateOffset = 15 + (beatPos * 5);  // Hi-hats with rhythmic timing variations
                } else {
                    gateOffset = 25;  // Off-pattern elements with mid timing delay
                }
            }
            break;

        case 9: // FUNK buffer generation - Syncopated grooves
            {
                gatePercent = 75;
                int pos = step % 16;

                // Funk patterns with syncopation
                static const uint16_t funkPatterns[8] = {
                    0b1010010010100100,  // Basic funk
                    0b1001001010010010,  // Syncopated
                    0b1010100100101001,  // Displaced
                    0b1001010010100101,  // Complex
                    0b1010010100100101,  // Variation 1
                    0b1001001001010010,  // Variation 2
                    0b1010100101001010,  // Variation 3
                    0b1001010100100101,  // Variation 4
                };

                bool isNote = (funkPatterns[_funkPattern] >> (15 - pos)) & 1;

                if (isNote) {
                    // Note selection based on position
                    int noteChoice = _rng.next() % 8;
                    switch (_funkSyncopation) {
                    case 0: note = noteChoice % 5; break;  // Pentatonic-ish
                    case 1: note = (noteChoice % 3) * 2; break;  // Root/3rd/5th
                    case 2: note = noteChoice; break;  // Full range
                    case 3: note = (pos % 4 == 0) ? 0 : (noteChoice % 5) + 2; break;  // Root on beat
                    }
                    octave = (pos % 8 == 0) ? 0 : (_rng.nextBinary() ? 1 : 0);

                    // FUNK: Generate syncopated timing offsets to complement the rhythm
                    if (pos % 4 == 0) {
                        // On beats (1, 2, 3, 4) - often on-time
                        gateOffset = 40 + (_rng.nextRange(20));  // 40-60% timing
                    } else if (pos % 2 == 0) {
                        // On upbeats (e.g., "&" of 2, 4) - maybe slightly early or late
                        gateOffset = 20 + (_rng.nextRange(20));  // 20-40% timing
                    } else {
                        // On off-beats (e.g., "&" of 1, 3, or 16th notes) - syncopated feel
                        gateOffset = 50 + (_rng.nextRange(30));  // 50-80% timing
                    }

                    // Ghost notes (quieter)
                    if (_extraRng.nextRange(256) < _funkGhostProb && pos % 4 != 0) {
                        gatePercent = 35;  // Ghost note
                    } else {
                        gatePercent = 75;
                    }
                } else {
                    note = 0;
                    gatePercent = 0;
                    gateOffset = 0;  // No gate, no offset
                }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }
            }
            break;

        case 10: // DRONE buffer generation - Sustained textures
            {
                // Very long gates, slow movement
                int interval = 0;
                switch (_droneInterval) {
                case 0: interval = 0; break;   // Unison
                case 1: interval = 7; break;   // Perfect 5th
                case 2: interval = 12; break;  // Octave
                case 3: interval = 19; break;  // 5th + octave
                }

                // Slow change rate (guard against division by zero)
                int droneRate = (_droneSpeed > 0) ? (4 * _droneSpeed) : 4;
                if ((step % droneRate) == 0) {
                    // Occasional variation
                    if (_rng.next() % 4 == 0) {
                        interval += (_rng.nextBinary() ? 2 : -2);
                    }
                }

                note = (_droneBaseNote + interval) % 12;
                octave = (_droneBaseNote + interval) / 12;
                gatePercent = 400;  // Very long sustain

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = 3;  // Long slide for drones
                }

                // DRONE: Apply sustained timing variations matching the slow drone nature
                // Use drone rate and position to create subtle timing shifts
                int timingFactor = (step % droneRate);
                if (timingFactor < droneRate / 2) {
                    gateOffset = 0;      // Stable timing during first half of drone change
                } else {
                    gateOffset = 15;     // Slight delay during second half of drone change
                }
            }
            break;

        case 11: // PHASE buffer generation - Minimalist phasing
            {
                gatePercent = 75;
                _phaseAccum += _phaseSpeed;

                // Get pattern position with phase offset (guard against division by zero)
                int phaseLen = (_phaseLength > 0) ? _phaseLength : 4;
                int patternPos = (step + (_phaseAccum >> 28)) % phaseLen;
                note = _phasePattern[patternPos];
                octave = 0;

                // PHASE: Generate timing variations based on phase accumulator
                // Use fractional part of accumulator to create evolving rhythmic patterns
                uint32_t fractionalPhase = _phaseAccum & 0x0FFFFFFF;  // Get fractional part
                gateOffset = (fractionalPhase >> 24) & 0x7F;  // Extract upper bits as timing offset (0-127, capped by uint8_t)

                // Consume RNG for determinism
                _rng.next();

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }
            }
            break;

        case 12: // RAGA buffer generation - Indian classical melodies
            {
                gatePercent = 75;

                // Move through scale in characteristic ways
                int movement = _rng.next() % 8;
                switch (movement) {
                case 0:
                case 1:
                case 2:
                    // Continue in current direction
                    if (_ragaDirection == 0) {
                        _ragaPosition = (_ragaPosition + 1) % 7;
                        if (_ragaPosition == 6) _ragaDirection = 1;
                    } else {
                        _ragaPosition = (_ragaPosition + 6) % 7;  // -1 mod 7
                        if (_ragaPosition == 0) _ragaDirection = 0;
                    }
                    break;
                case 3:
                case 4:
                    // Skip a note
                    if (_ragaDirection == 0) {
                        _ragaPosition = (_ragaPosition + 2) % 7;
                    } else {
                        _ragaPosition = (_ragaPosition + 5) % 7;
                    }
                    break;
                case 5:
                    // Repeat
                    break;
                case 6:
                    // Jump to root or 5th
                    _ragaPosition = (_rng.nextBinary()) ? 0 : 4;
                    break;
                case 7:
                    // Change direction
                    _ragaDirection = 1 - _ragaDirection;
                    break;
                }

                note = _ragaScale[_ragaPosition];
                octave = (_ragaPosition > 4) ? 1 : 0;

                // Ornaments (gamaka-like slides)
                int ornamentChance = _extraRng.next() % 8;
                if (ornamentChance < _ragaOrnament && glide > 0) {
                    slide = 2;  // Characteristic slides
                } else if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }
            }
            break;

        case 13: // AMBIENT buffer generation
            {
                // --- Handle ongoing events first ---
                if (_ambient_event_type == 1) { // Single Note Event
                    note = (_ambient_root_note + 12) % 12; // Octave up
                    octave = 1;
                    gatePercent = 150;
                    _ambient_event_type = 0; // Event is one step long
                } else if (_ambient_event_type == 2) { // Arpeggio Event
                    note = _ambient_drone_notes[_ambient_event_step];
                    octave = 0;
                    gatePercent = 50;
                    _ambient_event_step++;
                    if (_ambient_event_step >= 3) {
                        _ambient_event_type = 0; // End of event
                    }
                } else {
                    // --- Default Drone Behavior ---
                    note = _ambient_drone_notes[ (step / 4) % 3 ]; // Slowly cycle through drone notes
                    octave = 0;
                    gatePercent = 1600; // Hold gate for 16 steps
                    _ambient_event_timer--;
                }

                // --- Check if it's time for a new event ---
                if (_ambient_event_timer <= 0) {
                    _ambient_event_type = 1 + (_extraRng.next() % 2); // 1 or 2
                    _ambient_event_step = 0;
                    // Reset timer based on Power
                    int power = _tuesdayTrack.power();
                    _ambient_event_timer = 16 + (power > 0 ? 256 / power : 256);

                    // AMBIENT: Generate random timing variations for organic feel
                    // More timing flexibility for sparse events
                    gateOffset = _rng.nextRange(50);  // Random timing (0-49%) for organic feel
                }

                slide = 0;
            }
            break;

        case 15: // DRILL buffer generation - UK Drill hi-hat rolls and bass slides
            {
                // Advance step in bar
                _drillStepInBar = (_drillStepInBar + 1) % 8;

                // Check hi-hat pattern
                bool hihatHit = (_drillHiHatPattern & (1 << _drillStepInBar)) != 0;

                if (hihatHit) {
                    // Hi-hat hit - high note, short gate
                    note = 7 + (_rng.next() % 5);  // High notes for hi-hat
                    octave = 1;
                    gatePercent = 25;  // Short staccato for hi-hat

                    // Check for roll (rapid repeats)
                    if (_extraRng.nextRange(16) < 4) {
                        _drillRollCount = 2 + (_rng.next() % 3);  // 2-4 repeats
                    }
                } else if (_drillRollCount > 0) {
                    // Continue roll
                    _drillRollCount--;
                    note = 7 + (_rng.next() % 5);
                    octave = 1;
                    gatePercent = 20;  // Very short for roll notes
                } else {
                    // Bass note - low octave
                    note = _drillLastNote;
                    octave = -1;  // Deep bass
                    gatePercent = 75;

                    // Occasional bass note change
                    if (_rng.nextRange(8) < 2) {
                        _drillLastNote = _rng.next() % 5;
                    }

                    // Slide to target
                    if (_extraRng.nextRange(16) < 8) {
                        slide = 2;  // Medium glide for bass slides
                        _drillSlideTarget = _rng.next() % 12;
                    } else if (glide > 0 && _rng.nextRange(100) < glide) {
                        slide = (_rng.nextRange(3)) + 1;
                    }

                    // DRILL: Apply UK drill timing variations with micro-syncopation
                    if (hihatHit) {
                        gateOffset = (_drillStepInBar == 0) ? 5 : 25;  // Strong beats (0) slightly early, others delayed for swing
                    } else if (_drillRollCount > 0) {
                        gateOffset = 10 + _drillRollCount * 5;  // Roll notes with progressive timing
                    } else {
                        gateOffset = 0;  // Bass notes on beat
                    }
                }
            }
            break;

        case 16: // MINIMAL buffer generation - staccato bursts and silence
            {
                // Mode state machine: silence → burst → silence
                if (_minimalMode == 0) {
                    // Silence mode - no gate
                    if (_minimalSilenceTimer > 0) {
                        _minimalSilenceTimer--;
                        gatePercent = 0;
                        note = 0;
                        octave = 0;
                        _rng.next();  // consume for determinism
                    } else {
                        // Switch to burst mode
                        _minimalMode = 1;
                        _minimalBurstTimer = _minimalBurstLength;
                        _minimalNoteIndex = 0;
                        // Generate first note of burst
                        note = _rng.next() % 12;
                        octave = 0;
                        gatePercent = 25;  // Short staccato gates
                    }
                } else {
                    // Burst mode - generate notes
                    if (_minimalBurstTimer > 0) {
                        _minimalBurstTimer--;
                        _minimalNoteIndex++;

                        // Generate note based on pattern
                        int baseNote = _rng.next() % 12;

                        // Glitch repeats based on ornament
                        if (_extraRng.nextRange(256) < _minimalClickDensity) {
                            // Glitch - repeat previous note or create click
                            note = baseNote;
                            gatePercent = 15;  // Very short click
                        } else {
                            note = baseNote;
                            gatePercent = 25;  // Normal staccato
                        }
                        octave = 0;
                    } else {
                        // Switch to silence mode
                        _minimalMode = 0;
                        _minimalSilenceTimer = _minimalSilenceLength;
                        gatePercent = 0;
                        note = 0;
                        octave = 0;
                    }
                }

                // Glide check
                if (gatePercent > 0 && glide > 0 && _rng.nextRange(100) < glide) {
                    slide = (_rng.nextRange(3)) + 1;
                }

                // MINIMAL: Apply timing variations that complement the burst/silence pattern
                if (_minimalMode == 0) {
                    gateOffset = 0;   // Silence mode - no gate offset
                } else {
                    // Burst mode - vary timing based on burst progression
                    gateOffset = (_minimalBurstTimer % 4) * 10;  // Cycle through timing variations during burst
                }
            }
            break;

        case 14: // ACID buffer generation - 303-style patterns
            {
                // Get note from sequence
                note = _acidSequence[_acidPosition];
                octave = 0;

                // Check for accent
                bool hasAccent = (_acidAccentPattern & (1 << _acidPosition)) != 0;
                gatePercent = hasAccent ? 95 : 65;  // Punchy 303 gates

                // Check for octave jump
                if (_acidOctaveMask & (1 << _acidPosition)) {
                    octave = 1;
                }

                // Slide based on flow
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    slide = 2;  // 303-style slide
                }

                // Advance position
                _acidPosition = (_acidPosition + 1) % 8;
                _acidLastNote = note;
                _acidStepCount++;

                // Occasional pattern mutation
                if (_rng.nextRange(128) < 2) {
                    int mutatePos = _rng.next() % 8;
                    _acidSequence[mutatePos] = _rng.next() % 12;
                }

                // Consume extra RNG
                _extraRng.next();

                // ACID: Apply acid-style timing variations that complement the 303 patterns
                if (hasAccent) {
                    gateOffset = 10;  // Accented notes slightly delayed
                } else if (_acidPosition % 2 == 0) {
                    gateOffset = 0;   // Even steps on-beat
                } else {
                    gateOffset = 20;  // Odd steps with slight delay
                }
            }
            break;

        case 17: // KRAFT buffer generation - precise mechanical sequences
            {
                // Get note from sequence with transpose
                note = (_kraftSequence[_kraftPosition] + _kraftTranspose) % 12;
                octave = 0;

                // Check for ghost note
                bool isGhost = (_kraftGhostMask & (1 << _kraftPosition)) != 0;
                gatePercent = isGhost ? 25 : 50;  // Precise, mechanical gates

                // Lock timer controls pattern stability
                if (_kraftLockTimer > 0) {
                    _kraftLockTimer--;
                } else {
                    // Pattern evolution when lock expires
                    _kraftLockTimer = 16 + (_rng.next() % 16);
                    _kraftBaseNote = (_kraftBaseNote + _rng.nextRange(5)) % 12;
                    // Regenerate pattern
                    for (int i = 0; i < 8; i++) {
                        _kraftSequence[i] = (_kraftBaseNote + ((i % 2) ? 7 : 0)) % 12;
                    }
                }

                // Transpose based on flow
                if (_rng.nextRange(16) < 4) {
                    _kraftTranspose = _rng.next() % 12;
                    _kraftTranspCount++;
                }

                // Advance position
                _kraftPosition = (_kraftPosition + 1) % 8;

                // Glide check (rare for mechanical feel)
                if (glide > 0 && _rng.nextRange(100) < glide / 2) {
                    slide = 1;  // Short slide
                }

                _extraRng.next();

                // KRAFT: Apply precise mechanical timing variations
                // Complement the precise, algorithmic nature of the sequence
                if (isGhost) {
                    gateOffset = 20 + (_kraftPosition * 5);  // Ghost notes with specific timing
                } else {
                    gateOffset = (_kraftPosition % 3) * 10;  // Pattern-based timing variations
                }
            }
            break;

        case 18: // APHEX buffer generation
            {
                // --- Main melody from Track 1 ---
                note = _aphex_track1_pattern[_aphex_pos1];
                octave = 0;
                gatePercent = 75;
                slide = 0;

                // --- Modification from Track 2 ---
                uint8_t modifier = _aphex_track2_pattern[_aphex_pos2];
                if (modifier == 1) { // Stutter
                    gatePercent = 20;
                } else if (modifier == 2) { // Slide
                    slide = 1;
                }

                // --- Override from Track 3 ---
                uint8_t bass_note = _aphex_track3_pattern[_aphex_pos3];
                if (bass_note > 0) {
                    note = bass_note;
                    octave = -1; // Deep bass
                    gatePercent = 90;
                    slide = 0;
                }

                // --- Advance all tracks ---
                _aphex_pos1 = (_aphex_pos1 + 1) % 4;
                _aphex_pos2 = (_aphex_pos2 + 1) % 3;
                _aphex_pos3 = (_aphex_pos3 + 1) % 5;

                // APHEX: Apply polyrhythmic timing variations
                // Use multi-track positioning to create complex timing variations
                int polyRhythmFactor = (_aphex_pos1 + _aphex_pos2 + _aphex_pos3) % 12;
                gateOffset = polyRhythmFactor * 8;  // Scale to 0-96% range
            }
            break;

        case 19: // AUTECHRE buffer generation
            {
                // --- Always play the current state of the pattern ---
                note = _autechre_pattern[step % 8];
                octave = 0;
                gatePercent = 75;
                slide = 0;

                // --- Countdown to the next transformation ---
                _autechre_rule_timer--;
                if (_autechre_rule_timer <= 0) {
                    uint8_t current_rule = _autechre_rule_sequence[_autechre_rule_index];
                    int intensity = _tuesdayTrack.power() / 2; // Power: 0-8

                    // --- Apply a deterministic transformation rule ---
                    switch (current_rule) {
                        case 0: // ROTATE
                            {
                                int8_t temp = _autechre_pattern[7];
                                for (int i = 7; i > 0; --i) _autechre_pattern[i] = _autechre_pattern[i-1];
                                _autechre_pattern[0] = temp;
                            }
                            break;
                        case 1: // REVERSE
                            for (int i = 0; i < 4; ++i) {
                                int8_t temp = _autechre_pattern[i];
                                _autechre_pattern[i] = _autechre_pattern[7-i];
                                _autechre_pattern[7-i] = temp;
                            }
                            break;
                        case 2: // INVERT around a pivot (e.g., note 6)
                            for (int i = 0; i < 8; ++i) {
                                int interval = _autechre_pattern[i] - 6;
                                _autechre_pattern[i] = (6 - interval + 12) % 12;
                            }
                            break;
                        case 3: // SWAP adjacent notes
                            for (int i = 0; i < 8; i += 2) {
                                int8_t temp = _autechre_pattern[i];
                                _autechre_pattern[i] = _autechre_pattern[i+1];
                                _autechre_pattern[i+1] = temp;
                            }
                            break;
                        case 4: // ADD intensity
                             for (int i = 0; i < 8; ++i) _autechre_pattern[i] = (_autechre_pattern[i] + intensity) % 12;
                            break;
                    }

                    // --- Reset for next rule ---
                    _autechre_rule_timer = 8 + (_tuesdayTrack.flow() * 4);
                    _autechre_rule_index = (_autechre_rule_index + 1) % 8;
                }

                // AUTECHRE: Apply algorithmic timing variations that complement the pattern transformations
                // Use the current rule index and timer to create evolving timing variations
                gateOffset = ((_autechre_rule_index * 10) + (_autechre_rule_timer % 7)) % 100;
            }
            break;

        default:
            note = 0;
            octave = 0;
            break;
        }

        _buffer[step].note = note;
        _buffer[step].octave = octave;
        _buffer[step].gatePercent = gatePercent;
        _buffer[step].slide = slide;
        _buffer[step].gateOffset = gateOffset;  // Assign the gateOffset value
    }

    _bufferValid = true;

    // Reinitialize algorithm for live playback
    initAlgorithm();
}

TrackEngine::TickResult TuesdayTrackEngine::tick(uint32_t tick) {
    // Check mute
    if (mute()) {
        _gateOutput = false;
        _cvOutput = 0.f;
        _activity = false;
        return TickResult::NoUpdate;
    }

    // Get parameters
    int power = _tuesdayTrack.power();
    int loopLength = _tuesdayTrack.actualLoopLength();
    int algorithm = _tuesdayTrack.algorithm();

    // Power = 0 means silent
    if (power == 0) {
        _gateOutput = false;
        _cvOutput = 0.f;
        _activity = false;
        return TickResult::NoUpdate;
    }

    // Check if parameters changed - if so, reinitialize and invalidate buffer
    // Note: Scan is NOT included here - it's a real-time playback parameter
    bool paramsChanged = (_cachedAlgorithm != _tuesdayTrack.algorithm() ||
                         _cachedFlow != _tuesdayTrack.flow() ||
                         _cachedOrnament != _tuesdayTrack.ornament() ||
                         _cachedLoopLength != _tuesdayTrack.loopLength());
    if (paramsChanged) {
        _cachedAlgorithm = _tuesdayTrack.algorithm();
        _cachedLoopLength = _tuesdayTrack.loopLength();
        initAlgorithm();
        _bufferValid = false;
    }

    // Generate buffer for finite loops if needed
    if (loopLength > 0 && !_bufferValid) {
        generateBuffer();
    }

    // Calculate base cooldown from power
    // Linear mapping: Power = number of notes per 16 steps
    // Use fixed reference length for consistent density regardless of loop length
    int effectiveLength = 16;  // Fixed reference for density calculations
    int baseCooldown = (power > 0) ? effectiveLength / power : effectiveLength;
    if (baseCooldown < 1) baseCooldown = 1;

    // Apply skew to cooldown based on loop position
    // Skew value determines what fraction of loop is at power 16:
    // Skew 8 = last 50% at power 16, Skew 4 = last 25% at power 16
    int skew = _tuesdayTrack.skew();
    if (skew != 0 && loopLength > 0) {
        // Calculate position in loop (0.0 to 1.0), clamped for safety
        float position = (float)_stepIndex / (float)loopLength;
        if (position > 1.0f) position = 1.0f;
        if (position < 0.0f) position = 0.0f;

        if (skew > 0) {
            // Build-up: last (skew/16) of loop at power 16
            // Skew 8 → switch at 0.5, Skew 4 → switch at 0.75
            float switchPoint = 1.0f - (float)skew / 16.0f;

            if (position < switchPoint) {
                // Before switch: use base power setting
                _coolDownMax = baseCooldown;
            } else {
                // After switch: full density (power 16)
                _coolDownMax = 1;
            }
        } else {
            // Fade-out: first (|skew|/16) of loop at power 16
            // Skew -8 → switch at 0.5, Skew -4 → switch at 0.25
            float switchPoint = (float)(-skew) / 16.0f;

            if (position < switchPoint) {
                // Before switch: full density (power 16)
                _coolDownMax = 1;
            } else {
                // After switch: base power setting
                _coolDownMax = baseCooldown;
            }
        }

        // Clamp to valid range
        if (_coolDownMax < 1) _coolDownMax = 1;
        if (_coolDownMax > 16) _coolDownMax = 16;
    } else {
        _coolDownMax = baseCooldown;
    }

    // Calculate step timing with clock sync
    // Use track divisor (converts from PPQN to actual ticks)
    uint32_t divisor = _tuesdayTrack.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);

    // Calculate reset divisor from resetMeasure parameter
    int resetMeasure = _tuesdayTrack.resetMeasure();
    uint32_t resetDivisor;

    if (loopLength > 0) {
        // Finite loop - calculate reset from loop duration
        // This ensures steps always align with loop boundaries
        uint32_t loopDuration = loopLength * divisor;

        if (resetMeasure > 0) {
            // If resetMeasure is set, round up to next complete loop cycle
            // to allow patterns to evolve over multiple loop cycles
            uint32_t measureReset = resetMeasure * _engine.measureDivisor();
            resetDivisor = ((measureReset + loopDuration - 1) / loopDuration) * loopDuration;
        } else {
            // No reset measure - just use loop duration
            resetDivisor = loopDuration;
        }
    } else {
        // Infinite loop - no reset, tick grows forever
        resetDivisor = 0;
    }

    uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;

    // Reset on measure boundary (only if resetMeasure is enabled and not infinite loop)
    if (resetDivisor > 0 && relativeTick == 0) {
        reset();
    }

    // Check if we're at a step boundary
    bool stepTrigger = (relativeTick % divisor == 0);

    // Handle gate timing
    if (_gateTicks > 0) {
        _gateTicks--;
        if (_gateTicks == 0) {
            _gateOutput = false;
            _activity = false;
        }
    }

    // Handle slide/portamento
    if (_slideCountDown > 0) {
        _cvCurrent += _cvDelta;
        _slideCountDown--;
        if (_slideCountDown == 0) {
            _cvCurrent = _cvTarget;  // Ensure we hit target exactly
        }
        _cvOutput = _cvCurrent;
    }

    if (stepTrigger) {
        // Calculate step from tick count (ensures sync with divisor)
        uint32_t calculatedStep = relativeTick / divisor;

        // For finite loops, wrap within loop length
        if (loopLength > 0) {
            // Check if we wrapped to beginning of loop - reinitialize algorithm
            uint32_t newStep = calculatedStep % loopLength;
            if (newStep < _stepIndex && _stepIndex > 0) {
                // Loop wrapped - reinitialize algorithm for deterministic repeats
                initAlgorithm();
            }
            _stepIndex = newStep;
        } else {
            _stepIndex = calculatedStep;
        }

        // Set display step for UI sync
        _displayStep = _stepIndex;

        // Generate output based on algorithm
        bool shouldGate = false;
        float noteVoltage = 0.f;
        int note = 0;
        int octave = 0;

        // Calculate effective step index
        // Scan: offsets where on the infinite tape we capture from (0-127)
        // Rotate: for finite loops, shifts start point within the captured loop
        int scan = _tuesdayTrack.scan();
        uint32_t effectiveStep = _stepIndex;
        if (loopLength > 0) {
            // Finite loop: rotate shifts within loop, then scan offsets into infinite tape
            int rot = _tuesdayTrack.rotate();
            // Handle negative rotation with proper modulo
            effectiveStep = ((_stepIndex + rot) % loopLength + loopLength) % loopLength;
            // Add scan offset to select which portion of infinite tape
            effectiveStep += scan;

            // Read from pre-generated buffer
            if (effectiveStep < BUFFER_SIZE) {
                note = _buffer[effectiveStep].note;
                octave = _buffer[effectiveStep].octave;
                _gatePercent = _buffer[effectiveStep].gatePercent;
                _slide = _buffer[effectiveStep].slide;

                // Retrieve gate offset from buffered data with potential global override
                // Similar to glide override: global parameter can override algorithmic value based on probability
                uint8_t bufferedGateOffset = _buffer[effectiveStep].gateOffset;
                uint8_t globalGateOffset = _tuesdayTrack.gateOffset();
                uint8_t finalOffset = 0;

                if (globalGateOffset == 0) {
                    // Mode 1: Force On-Beat
                    finalOffset = 0;
                } else if (globalGateOffset == 100) {
                    // Mode 3: Pure Random Offset
                    finalOffset = _uiRng.nextRange(101); // 0-100
                } else {
                    // Mode 2: Probabilistic blend of Algorithmic and Random
                    finalOffset = bufferedGateOffset; // Base is the algorithmic groove
                    // with a chance of being overridden by a random value
                    if (_uiRng.nextRange(100) < globalGateOffset) {
                        finalOffset = _uiRng.nextRange(101); // 0-100
                    }
                }
                _gateOffset = finalOffset;

                shouldGate = true;
                // Calculate noteVoltage from buffered data for CV/glide
                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
        } else {
            // Infinite loop: live generation with scan offset
            effectiveStep = _stepIndex + scan;

            switch (algorithm) {
        case 0: // TEST - Test patterns
            // Flow: Mode (OCTSWEEPS or SCALEWALKER) + SweepSpeed
            // Ornament: Accent + Velocity
            {
                shouldGate = true;  // Always gate in test mode
                _gatePercent = 75;  // Default gate length

                // Slide controlled by glide parameter
                int glide = _tuesdayTrack.glide();
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = _testSweepSpeed + 1;  // Use sweep speed as slide amount
                } else {
                    _slide = 0;
                }

                switch (_testMode) {
                case 0:  // OCTSWEEPS - sweep through octaves
                    octave = (effectiveStep % 5);  // 5 octaves
                    note = 0;
                    break;
                case 1:  // SCALEWALKER - walk through notes
                default:
                    octave = 0;
                    note = _testNote;
                    _testNote = (_testNote + 1) % 12;
                    break;
                }

                // CV: 1V/octave standard
                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 1: // TRITRANCE - German minimal style arpeggios
            // Flow: Seeds RNG for b1 (high note), b2 (phase offset)
            // Ornament: Seeds RNG for b3 (note offset)
            {
                shouldGate = true;

                // Random gate length - min 50%, up to 400%
                int gateLengthChoice = _rng.nextRange(100);
                if (gateLengthChoice < 40) {
                    _gatePercent = 50 + (_rng.nextRange(4) * 12);  // 50%, 62%, 74%, 86%
                } else if (gateLengthChoice < 70) {
                    _gatePercent = 100 + (_rng.nextRange(4) * 25); // 100%, 125%, 150%, 175%
                } else {
                    _gatePercent = 200 + (_rng.nextRange(9) * 25); // 200% to 400%
                }

                // Random slide controlled by glide parameter
                int glide = _tuesdayTrack.glide();
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;  // 1-3
                } else {
                    _slide = 0;
                }

                // Tritrance pattern based on step position mod 3
                int phase = (effectiveStep + _triB2) % 3;
                switch (phase) {
                case 0:
                    // Maybe change b3
                    if (_extraRng.nextBinary() && _extraRng.nextBinary()) {
                        _triB3 = (_extraRng.next() & 0x15);
                        if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
                        _triB3 -= 4;
                    }
                    octave = 0;
                    note = _triB3 + 4;  // Center around note 4
                    break;
                case 1:
                    octave = 1;
                    note = _triB3 + 4;
                    // Maybe change b2
                    if (_rng.nextBinary()) {
                        _triB2 = (_rng.next() & 0x7);
                    }
                    break;
                case 2:
                    octave = 2;
                    note = _triB1;
                    // Maybe change b1
                    if (_rng.nextBinary()) {
                        _triB1 = ((_rng.next() >> 5) & 0x7);
                    }
                    break;
                }

                // Clamp note to valid range
                if (note < 0) note = 0;
                if (note > 11) note = 11;

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 2: // STOMPER - Acid bass patterns with slides
            // Flow: Seeds RNG for pattern modes
            // Ornament: Seeds RNG for note choices
            {
                int accentoffs = 0;
                _gatePercent = 75;  // Default
                _slide = 0;  // Default no slide

                if (_stomperCountDown > 0) {
                    // Rest/note-off period - use countdown for gate length
                    shouldGate = false;
                    _gatePercent = _stomperCountDown * 25;  // Shorter gates during countdown
                    _stomperCountDown--;
                } else {
                    shouldGate = true;

                    // Generate new mode if needed
                    if (_stomperMode >= 14) {  // STOMPER_MAKENEW
                        _stomperMode = (_extraRng.next() % 7) * 2;
                    }

                    // Maybe change notes
                    if (_rng.nextRange(100) < 100) {
                        _stomperLowNote = _rng.next() % 3;
                    }
                    if (_rng.nextRange(100) < 100) {
                        _stomperHighNote[0] = _rng.next() % 7;
                    }
                    if (_rng.nextRange(100) < 100) {
                        _stomperHighNote[1] = _rng.next() % 5;
                    }

                    int maxticklen = 2;

                    // Pattern state machine
                    switch (_stomperMode) {
                    case 10:  // SLIDEDOWN1
                        octave = 1;
                        note = _stomperHighNote[_rng.next() % 2];
                        _stomperMode++;
                        break;
                    case 11:  // SLIDEDOWN2
                        octave = 0;
                        note = _stomperLowNote;
                        // Slide controlled by glide parameter
                        if (_tuesdayTrack.glide() > 0) {
                            _slide = (_rng.nextRange(3)) + 1;  // Slide 1-3
                        } else {
                            _slide = 0;
                        }
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;  // MAKENEW
                        break;
                    case 12:  // SLIDEUP1
                        octave = 0;
                        note = _stomperLowNote;
                        _stomperMode++;
                        break;
                    case 13:  // SLIDEUP2
                        octave = 1;
                        note = _stomperHighNote[_rng.next() % 2];
                        // Slide controlled by glide parameter
                        if (_tuesdayTrack.glide() > 0) {
                            _slide = (_rng.nextRange(3)) + 1;  // Slide 1-3
                        } else {
                            _slide = 0;
                        }
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;  // MAKENEW
                        break;
                    case 4:  // LOW1
                        accentoffs = 100;
                        // fall through
                    case 5:  // LOW2
                        octave = 0;
                        note = _stomperLowNote;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;  // MAKENEW
                        break;
                    case 0:  // PAUSE1
                    case 1:  // PAUSE2
                        octave = _stomperLastOctave;
                        note = _stomperLastNote;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;  // MAKENEW
                        break;
                    case 6:  // HIGH1
                        accentoffs = 100;
                        // fall through
                    case 7:  // HIGH2
                        octave = 0;
                        note = _stomperLowNote;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;  // MAKENEW
                        break;
                    case 8:  // HILOW1
                        octave = 1;
                        note = _stomperHighNote[_rng.next() % 2];
                        _stomperMode++;
                        break;
                    case 9:  // HILOW2
                        octave = 0;
                        note = _stomperLowNote;
                        accentoffs = 100;
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;  // MAKENEW
                        break;
                    case 2:  // LOWHI1
                        octave = 0;
                        note = _stomperLowNote;
                        accentoffs = 100;
                        _stomperMode++;
                        break;
                    case 3:  // LOWHI2
                        octave = 1;
                        note = _stomperHighNote[_rng.next() % 2];
                        if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        _stomperMode = 14;  // MAKENEW
                        break;
                    default:
                        octave = _stomperLastOctave;
                        note = _stomperLastNote;
                        break;
                    }

                    _stomperLastNote = note;
                    _stomperLastOctave = octave;
                }

                // Clamp
                if (note < 0) note = 0;
                if (note > 11) note = 11;

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 3: // MARKOV - Markov chain melody generation
            // Flow + Ornament: Seeds for generating the transition matrix
            {
                shouldGate = true;

                // Random gate length - min 50%, up to 400%
                int gateLengthChoice = _rng.nextRange(100);
                if (gateLengthChoice < 40) {
                    _gatePercent = 50 + (_rng.nextRange(4) * 12);  // 50%, 62%, 74%, 86%
                } else if (gateLengthChoice < 70) {
                    _gatePercent = 100 + (_rng.nextRange(4) * 25); // 100%, 125%, 150%, 175%
                } else {
                    _gatePercent = 200 + (_rng.nextRange(9) * 25); // 200% to 400%
                }

                // Random slide controlled by glide parameter
                int glide = _tuesdayTrack.glide();
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;  // 1-3
                } else {
                    _slide = 0;
                }

                // Select from Markov matrix based on history
                int idx = _rng.nextBinary() ? 1 : 0;
                note = _markovMatrix[_markovHistory1][_markovHistory3][idx];

                // Update history
                _markovHistory1 = _markovHistory3;
                _markovHistory3 = note;

                // Random octave (0 or 1)
                octave = _rng.nextBinary() ? 1 : 0;

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 4: // CHIPARP - Chiptune arpeggios
            {
                shouldGate = true;
                _gatePercent = 75;
                int glide = _tuesdayTrack.glide();

                int chordpos = effectiveStep % 4;

                if (chordpos == 0) {
                    _chipRng = Random(_chipChordSeed);
                    if (_rng.nextRange(256) < 0x20) {
                        _chipBase = _rng.next() % 3;
                    }
                    if (_rng.nextRange(256) < 0xf0) {
                        _chipDir = _extraRng.nextBinary() ? 1 : 0;
                    }
                }

                int pos = chordpos;
                if (_chipDir == 1) {
                    pos = 3 - chordpos;
                }

                if (_chipRng.nextRange(256) < 0x20) {
                    // accent - could affect velocity
                }
                // Algorithm's slide logic - only apply if glide > 0
                if (_chipRng.nextRange(256) < 0x80) {
                    int algoSlide = _chipRng.nextRange(256) % 3;
                    if (glide > 0) {
                        _slide = algoSlide;
                    } else {
                        _slide = 0;
                    }
                } else {
                    _slide = 0;
                }
                if (_chipRng.nextRange(256) >= 0xd0) {
                    shouldGate = false;  // Note off (~19% chance)
                    _gatePercent = 0;
                } else {
                    note = (pos * 2) + _chipBase;
                    _gatePercent = 50 + 25 * (_chipRng.nextRange(256) % 3);
                }
                octave = 0;
                _extraRng.nextRange(256);  // velocity

                // Additional slide from glide parameter
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 5: // GOACID - Goa/psytrance acid patterns
            {
                shouldGate = true;
                _gatePercent = 75;
                int glide = _tuesdayTrack.glide();

                _rng.nextRange(256);  // velocity (consumed but not used)
                bool accent = _extraRng.nextBinary();

                int randNote = _rng.next() % 8;
                // Original uses signed char: 0xf4=-12, 0xfe=-2, 0xf2=-14
                switch (randNote) {
                case 0:
                case 2:
                    note = 0; break;
                case 1: note = -12; break;  // 0xf4
                case 3: note = 1; break;
                case 4: note = 3; break;
                case 5: note = 7; break;
                case 6: note = 12; break;  // 0xc
                case 7: note = 13; break;  // 0xd
                }

                if (accent) {
                    switch (randNote) {
                    case 0:
                    case 3:
                    case 7: note = 0; break;
                    case 1: note = -12; break;  // 0xf4
                    case 2: note = -2; break;   // 0xfe
                    case 4: note = 3; break;
                    case 5: note = -14; break;  // 0xf2
                    case 6: note = 1; break;
                    }
                }

                // Apply pattern transpose
                if (_goaB1 && (effectiveStep % 16) <= 7) {
                    note += 3;
                }
                if (_goaB2 && (effectiveStep % 16) <= 7) {
                    note -= 5;
                }

                // Add +24 semitones (2 octaves) as in original
                note += 24;

                // Convert to note/octave
                while (note < 0) { note += 12; octave--; }
                while (note >= 12) { note -= 12; octave++; }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                } else {
                    _slide = 0;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 6: // SNH - Sample & Hold random walk
            {
                shouldGate = true;
                _gatePercent = 75;
                int glide = _tuesdayTrack.glide();

                _snhPhase += _snhPhaseSpeed;
                int v = _snhPhase >> 30;

                if (v != _snhLastVal) {
                    _snhLastVal = v;
                    _snhTarget = _rng.next() << 10;
                }
                int newdelta = (_snhTarget - _snhCurrent) / 100;
                _snhCurrentDelta += ((newdelta - _snhCurrentDelta) * 100) / 200;
                _snhCurrent += _snhCurrentDelta * 100;

                // Convert filtered value to note
                int absVal = _snhCurrent;
                if (absVal < 0) absVal = -absVal;
                note = (absVal >> 22) % 12;
                octave = 0;

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                } else {
                    _slide = 0;
                }
                _extraRng.next();  // velocity

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 7: // WOBBLE - Dual-phase LFO bass
            {
                shouldGate = true;
                _gatePercent = 75;
                int glide = _tuesdayTrack.glide();

                _wobblePhase += _wobblePhaseSpeed;
                _wobblePhase2 += _wobblePhaseSpeed2;

                if (_rng.nextRange(256) >= 128) {  // High phase
                    int32_t pos2 = _wobblePhase2 >> 27;
                    note = pos2 % 8;
                    if (_wobbleLastWasHigh == 0) {
                        if (_rng.nextRange(256) >= 56) {
                            int algoSlide = _rng.next() % 3;
                            if (glide > 0) {
                                _slide = algoSlide;
                            } else {
                                _slide = 0;
                            }
                        } else {
                            _slide = 0;
                        }
                    }
                    _wobbleLastWasHigh = 1;
                } else {  // Low phase
                    int32_t pos = _wobblePhase >> 27;
                    note = pos % 8;
                    if (_wobbleLastWasHigh == 1) {
                        if (_rng.nextRange(256) >= 56) {
                            int algoSlide = _rng.next() % 3;
                            if (glide > 0) {
                                _slide = algoSlide;
                            } else {
                                _slide = 0;
                            }
                        } else {
                            _slide = 0;
                        }
                    }
                    _wobbleLastWasHigh = 0;
                }
                octave = 0;
                _extraRng.next();  // velocity

                // Additional slide from glide parameter
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 8: // TECHNO - Four-on-floor club
            {
                shouldGate = true;
                _gatePercent = 75;
                int glide = _tuesdayTrack.glide();

                int beatPos = effectiveStep % 4;
                int barPos = effectiveStep % 16;

                bool isKick = false;
                switch (_technoKickPattern) {
                case 0: isKick = (beatPos == 0); break;
                case 1: isKick = (beatPos == 0) || (barPos == 14); break;
                case 2: isKick = (beatPos == 0) || (barPos == 6); break;
                case 3: isKick = (beatPos == 0) || (barPos == 3) || (barPos == 11); break;
                }

                if (isKick) {
                    note = _technoBassNote;
                    octave = 0;
                    _gatePercent = 80;
                } else {
                    bool isHat = false;
                    switch (_technoHatPattern) {
                    case 0: isHat = (beatPos == 2); break;
                    case 1: isHat = (beatPos == 1) || (beatPos == 3); break;
                    case 2: isHat = true; break;
                    case 3: isHat = (beatPos != 0) && (_rng.next() % 3 != 0); break;
                    }
                    if (isHat) {
                        note = 7 + (_extraRng.next() % 3);
                        octave = 1;
                        _gatePercent = 40;
                    } else {
                        shouldGate = false;
                        note = 0;
                        octave = 0;
                    }
                }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                } else {
                    _slide = 0;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 9: // FUNK - Syncopated grooves
            {
                shouldGate = true;
                _gatePercent = 75;
                int glide = _tuesdayTrack.glide();

                int pos = effectiveStep % 16;

                static const uint16_t funkPatterns[8] = {
                    0b1010010010100100,
                    0b1001001010010010,
                    0b1010100100101001,
                    0b1001010010100101,
                    0b1010010100100101,
                    0b1001001001010010,
                    0b1010100101001010,
                    0b1001010100100101,
                };

                bool isNote = (funkPatterns[_funkPattern] >> (15 - pos)) & 1;

                if (isNote) {
                    int noteChoice = _rng.next() % 8;
                    switch (_funkSyncopation) {
                    case 0: note = noteChoice % 5; break;
                    case 1: note = (noteChoice % 3) * 2; break;
                    case 2: note = noteChoice; break;
                    case 3: note = (pos % 4 == 0) ? 0 : (noteChoice % 5) + 2; break;
                    }
                    octave = (pos % 8 == 0) ? 0 : (_rng.nextBinary() ? 1 : 0);

                    if (_extraRng.nextRange(256) < _funkGhostProb && pos % 4 != 0) {
                        _gatePercent = 35;
                    }
                } else {
                    shouldGate = false;
                    note = 0;
                    octave = 0;
                }

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                } else {
                    _slide = 0;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 10: // DRONE - Sustained textures
            {
                shouldGate = true;
                int glide = _tuesdayTrack.glide();

                int interval = 0;
                switch (_droneInterval) {
                case 0: interval = 0; break;
                case 1: interval = 7; break;
                case 2: interval = 12; break;
                case 3: interval = 19; break;
                }

                // Guard against division by zero
                int droneRate = (_droneSpeed > 0) ? (4 * _droneSpeed) : 4;
                if ((effectiveStep % droneRate) == 0) {
                    if (_rng.next() % 4 == 0) {
                        interval += (_rng.nextBinary() ? 2 : -2);
                    }
                }

                note = (_droneBaseNote + interval) % 12;
                octave = (_droneBaseNote + interval) / 12;
                _gatePercent = 400;

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = 3;
                } else {
                    _slide = 0;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 11: // PHASE - Minimalist phasing
            {
                shouldGate = true;
                _gatePercent = 75;
                int glide = _tuesdayTrack.glide();

                _phaseAccum += _phaseSpeed;
                // Guard against division by zero
                int phaseLen = (_phaseLength > 0) ? _phaseLength : 4;
                int patternPos = (effectiveStep + (_phaseAccum >> 28)) % phaseLen;
                note = _phasePattern[patternPos];
                octave = 0;

                _rng.next();  // Consume for determinism

                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                } else {
                    _slide = 0;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 12: // RAGA - Indian classical melodies
            {
                shouldGate = true;
                _gatePercent = 75;
                int glide = _tuesdayTrack.glide();

                int movement = _rng.next() % 8;
                switch (movement) {
                case 0:
                case 1:
                case 2:
                    if (_ragaDirection == 0) {
                        _ragaPosition = (_ragaPosition + 1) % 7;
                        if (_ragaPosition == 6) _ragaDirection = 1;
                    } else {
                        _ragaPosition = (_ragaPosition + 6) % 7;
                        if (_ragaPosition == 0) _ragaDirection = 0;
                    }
                    break;
                case 3:
                case 4:
                    if (_ragaDirection == 0) {
                        _ragaPosition = (_ragaPosition + 2) % 7;
                    } else {
                        _ragaPosition = (_ragaPosition + 5) % 7;
                    }
                    break;
                case 5:
                    break;
                case 6:
                    _ragaPosition = (_rng.nextBinary()) ? 0 : 4;
                    break;
                case 7:
                    _ragaDirection = 1 - _ragaDirection;
                    break;
                }

                note = _ragaScale[_ragaPosition];
                octave = (_ragaPosition > 4) ? 1 : 0;

                int ornamentChance = _extraRng.next() % 8;
                if (ornamentChance < _ragaOrnament && glide > 0) {
                    _slide = 2;
                } else if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                } else {
                    _slide = 0;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 13: // AMBIENT - Slow evolving pads (infinite loop)
            {
                shouldGate = true;
                // --- Handle ongoing events first ---
                if (_ambient_event_type == 1) { // Single Note Event
                    note = (_ambient_root_note + 12) % 12; // Octave up
                    octave = 1;
                    _gatePercent = 150;
                    _ambient_event_type = 0; // Event is one step long
                } else if (_ambient_event_type == 2) { // Arpeggio Event
                    note = _ambient_drone_notes[_ambient_event_step];
                    octave = 0;
                    _gatePercent = 50;
                    _ambient_event_step++;
                    if (_ambient_event_step >= 3) {
                        _ambient_event_type = 0; // End of event
                    }
                } else {
                    // --- Default Drone Behavior ---
                    note = _ambient_drone_notes[ (effectiveStep / 4) % 3 ]; // Slowly cycle through drone notes
                    octave = 0;
                    _gatePercent = 1600; // Hold gate for 16 steps
                    _ambient_event_timer--;
                }

                // --- Check if it's time for a new event ---
                if (_ambient_event_timer <= 0) {
                    _ambient_event_type = 1 + (_extraRng.next() % 2); // 1 or 2
                    _ambient_event_step = 0;
                    // Reset timer based on Power
                    int power = _tuesdayTrack.power();
                    _ambient_event_timer = 16 + (power > 0 ? 256 - (power * 15) : 256);
                }
                
                _slide = 0;

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 15: // DRILL - UK Drill hi-hat rolls and bass slides (infinite loop)
            {
                int glide = _tuesdayTrack.glide();

                // Advance step in bar
                _drillStepInBar = (_drillStepInBar + 1) % 8;

                // Check hi-hat pattern
                bool hihatHit = (_drillHiHatPattern & (1 << _drillStepInBar)) != 0;

                if (hihatHit) {
                    // --- HI-HAT HIT ---
                    note = 7 + (_rng.next() % 5);
                    octave = 1;
                    _gatePercent = 25;
                    shouldGate = true;
                    _slide = 0;

                    // Check for Trill/Retrigger
                    int trillChanceAlgorithmic = 30; // 30% base chance for a roll on a hi-hat
                    int userTrillSetting = _tuesdayTrack.trill();
                    int finalTrillChance = (trillChanceAlgorithmic * userTrillSetting) / 100;

                    if (_uiRng.nextRange(100) < finalTrillChance) {
                        _retriggerArmed = true;
                        _retriggerCount = 3; // 4 notes total (1 initial + 3 retriggers)
                        _retriggerPeriod = CONFIG_SEQUENCE_PPQN / 4; // 16th notes
                        _retriggerLength = _retriggerPeriod / 2; // 50% gate
                        _isTrillNote = false; // Start on the base note

                        // Set the trill note (2 semitones up)
                        float baseVoltage = (note + (octave * 12)) / 12.f;
                        _trillCvTarget = baseVoltage + (2.f / 12.f);

                        // Make sure the first note of the retrigger is short
                        _gatePercent = (_retriggerLength * 100) / CONFIG_SEQUENCE_PPQN;
                    }

                } else {
                    // --- BASS NOTE ---
                    note = _drillLastNote;
                    octave = -1;
                    _gatePercent = 75;
                    shouldGate = true;

                    // Occasional bass note change
                    if (_rng.nextRange(8) < 2) {
                        _drillLastNote = _rng.next() % 5;
                    }

                    // Slide
                    if (_extraRng.nextRange(16) < 8) {
                        _slide = 2; // Hardcoded slide for drill feel
                        _drillSlideTarget = _rng.next() % 12;
                    } else if (glide > 0 && _rng.nextRange(100) < glide) {
                        _slide = (_rng.nextRange(3)) + 1; // Probabilistic slide
                    } else {
                        _slide = 0;
                    }
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 16: // MINIMAL - staccato bursts and silence (infinite loop)
            {
                int glide = _tuesdayTrack.glide();

                // Mode state machine
                if (_minimalMode == 0) {
                    // Silence mode
                    if (_minimalSilenceTimer > 0) {
                        _minimalSilenceTimer--;
                        _rng.next();
                        shouldGate = false;
                        _gatePercent = 0;
                        note = 0;
                        octave = 0;
                    } else {
                        // Switch to burst
                        _minimalMode = 1;
                        _minimalBurstTimer = _minimalBurstLength;
                        _minimalNoteIndex = 0;
                        note = _rng.next() % 12;
                        octave = 0;
                        _gatePercent = 25;
                        shouldGate = true;
                    }
                } else {
                    // Burst mode
                    if (_minimalBurstTimer > 0) {
                        _minimalBurstTimer--;
                        _minimalNoteIndex++;
                        int baseNote = _rng.next() % 12;

                        if (_extraRng.nextRange(256) < _minimalClickDensity) {
                            note = baseNote;
                            _gatePercent = 15;
                        } else {
                            note = baseNote;
                            _gatePercent = 25;
                        }
                        octave = 0;
                        shouldGate = true;
                    } else {
                        // Switch to silence
                        _minimalMode = 0;
                        _minimalSilenceTimer = _minimalSilenceLength;
                        shouldGate = false;
                        _gatePercent = 0;
                        note = 0;
                        octave = 0;
                    }
                }

                // Glide
                if (shouldGate && glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = (_rng.nextRange(3)) + 1;
                } else {
                    _slide = 0;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 14: // ACID - 303-style patterns (infinite loop)
            {
                int glide = _tuesdayTrack.glide();

                // Get note from sequence
                note = _acidSequence[_acidPosition];
                octave = 0;

                // Check for accent
                bool hasAccent = (_acidAccentPattern & (1 << _acidPosition)) != 0;
                _gatePercent = hasAccent ? 95 : 65;
                shouldGate = true;

                // Check for octave jump
                if (_acidOctaveMask & (1 << _acidPosition)) {
                    octave = 1;
                }

                // Slide based on flow
                if (glide > 0 && _rng.nextRange(100) < glide) {
                    _slide = 2;
                } else {
                    _slide = 0;
                }

                // Advance position
                _acidPosition = (_acidPosition + 1) % 8;
                _acidLastNote = note;
                _acidStepCount++;

                // Occasional pattern mutation
                if (_rng.nextRange(128) < 2) {
                    int mutatePos = _rng.next() % 8;
                    _acidSequence[mutatePos] = _rng.next() % 12;
                }

                _extraRng.next();
                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 17: // KRAFT - precise mechanical sequences (infinite loop)
            {
                int glide = _tuesdayTrack.glide();

                // Get note from sequence with transpose
                note = (_kraftSequence[_kraftPosition] + _kraftTranspose) % 12;
                octave = 0;

                // Check for ghost note
                bool isGhost = (_kraftGhostMask & (1 << _kraftPosition)) != 0;
                _gatePercent = isGhost ? 25 : 50;
                shouldGate = true;

                // Lock timer
                if (_kraftLockTimer > 0) {
                    _kraftLockTimer--;
                } else {
                    _kraftLockTimer = 16 + (_rng.next() % 16);
                    _kraftBaseNote = (_kraftBaseNote + _rng.nextRange(5)) % 12;
                    for (int i = 0; i < 8; i++) {
                        _kraftSequence[i] = (_kraftBaseNote + ((i % 2) ? 7 : 0)) % 12;
                    }
                }

                // Transpose
                if (_rng.nextRange(16) < 4) {
                    _kraftTranspose = _rng.next() % 12;
                    _kraftTranspCount++;
                }

                // Advance position
                _kraftPosition = (_kraftPosition + 1) % 8;

                // Glide (rare)
                if (glide > 0 && _rng.nextRange(100) < glide / 2) {
                    _slide = 1;
                } else {
                    _slide = 0;
                }

                _extraRng.next();
                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 18: // APHEX - complex polyrhythmic patterns (infinite loop)
            {
                shouldGate = true;
                // --- Main melody from Track 1 ---
                note = _aphex_track1_pattern[_aphex_pos1];
                octave = 0;
                _gatePercent = 75;
                _slide = 0;

                // --- Modification from Track 2 ---
                uint8_t modifier = _aphex_track2_pattern[_aphex_pos2];
                if (modifier == 1) { // Stutter
                    _gatePercent = 20;
                } else if (modifier == 2) { // Slide
                    _slide = 1;
                }

                // --- Override from Track 3 ---
                uint8_t bass_note = _aphex_track3_pattern[_aphex_pos3];
                if (bass_note > 0) {
                    note = bass_note;
                    octave = -1; // Deep bass
                    _gatePercent = 90;
                    _slide = 0;
                }

                // --- Advance all tracks ---
                _aphex_pos1 = (_aphex_pos1 + 1) % 4;
                _aphex_pos2 = (_aphex_pos2 + 1) % 3;
                _aphex_pos3 = (_aphex_pos3 + 1) % 5;

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        case 19: // AUTECHRE - constantly evolving abstract patterns (infinite loop)
            {
                shouldGate = true;
                // --- Always play the current state of the pattern ---
                note = _autechre_pattern[effectiveStep % 8];
                octave = 0;
                _gatePercent = 75;
                _slide = 0;

                // --- Countdown to the next transformation ---
                _autechre_rule_timer--;
                if (_autechre_rule_timer <= 0) {
                    uint8_t current_rule = _autechre_rule_sequence[_autechre_rule_index];
                    int intensity = _tuesdayTrack.power() / 2; // Power: 0-8

                    // --- Apply a deterministic transformation rule ---
                    switch (current_rule) {
                        case 0: // ROTATE
                            {
                                int8_t temp = _autechre_pattern[7];
                                for (int i = 7; i > 0; --i) _autechre_pattern[i] = _autechre_pattern[i-1];
                                _autechre_pattern[0] = temp;
                            }
                            break;
                        case 1: // REVERSE
                            for (int i = 0; i < 4; ++i) {
                                int8_t temp = _autechre_pattern[i];
                                _autechre_pattern[i] = _autechre_pattern[7-i];
                                _autechre_pattern[7-i] = temp;
                            }
                            break;
                        case 2: // INVERT around a pivot (e.g., note 6)
                            for (int i = 0; i < 8; ++i) {
                                int interval = _autechre_pattern[i] - 6;
                                _autechre_pattern[i] = (6 - interval + 12) % 12;
                            }
                            break;
                        case 3: // SWAP adjacent notes
                            for (int i = 0; i < 8; i += 2) {
                                int8_t temp = _autechre_pattern[i];
                                _autechre_pattern[i] = _autechre_pattern[i+1];
                                _autechre_pattern[i+1] = temp;
                            }
                            break;
                        case 4: // ADD intensity
                             for (int i = 0; i < 8; ++i) _autechre_pattern[i] = (_autechre_pattern[i] + intensity) % 12;
                            break;
                    }

                    // --- Reset for next rule ---
                    _autechre_rule_timer = 8 + (_tuesdayTrack.flow() * 4);
                    _autechre_rule_index = (_autechre_rule_index + 1) % 8;
                }

                noteVoltage = (note + (octave * 12)) / 12.0f;
            }
            break;

        default:
            shouldGate = false;
            noteVoltage = 0.f;
            break;
            }
        } // End else (infinite loop)

        // Apply global GateOffset override
        uint8_t globalGateOffset = _tuesdayTrack.gateOffset();
        if (globalGateOffset == 0) {
            // Mode 1: Force On-Beat
            _gateOffset = 0;
        } else if (globalGateOffset == 100) {
            // Mode 3: Pure Random Offset
            _gateOffset = _uiRng.nextRange(101); // 0-100
        } else {
            // Mode 2: Probabilistic blend of Algorithmic and Random
            // Note: _gateOffset already holds the algorithmic value here.
            // We just check for a random override.
            if (_uiRng.nextRange(100) < globalGateOffset) {
                _gateOffset = _uiRng.nextRange(101); // 0-100
            }
        }

        // Apply octave and transpose from sequence parameters
        int trackOctave = _tuesdayTrack.octave();
        int trackTranspose = _tuesdayTrack.transpose();

        // Get scale and root note (use track settings if not Default, otherwise project)
        int trackScaleIdx = _tuesdayTrack.scale();
        int trackRootNote = _tuesdayTrack.rootNote();

        const Scale &scale = (trackScaleIdx >= 0) ? Scale::get(trackScaleIdx) : _model.project().selectedScale();
        int rootNote = (trackRootNote >= 0) ? trackRootNote : _model.project().rootNote();

        // Apply scale quantization if useScale is enabled, track has specific scale, or project has non-chromatic scale
        if (_tuesdayTrack.useScale() || trackScaleIdx >= 0 || _model.project().scale() > 0) {
            // Treat note as scale degree, convert to voltage
            int scaleNote = note + octave * scale.notesPerOctave();
            // Add transpose (in semitones for chromatic, scale degrees otherwise)
            scaleNote += trackTranspose;
            noteVoltage = scale.noteToVolts(scaleNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
        } else {
            // Free (chromatic) mode - apply transpose directly
            noteVoltage = (note + trackTranspose + (octave * 12)) / 12.0f;
        }

        // Apply octave offset (1V per octave)
        noteVoltage += trackOctave;

        // Decrement cooldown
        if (_coolDown > 0) {
            _coolDown--;
            if (_coolDown > _coolDownMax) _coolDown = _coolDownMax;
        }

        // Apply gate using cooldown system
        // Note triggers only when cooldown has expired
        // Velocity (from algorithm) must beat current cooldown value
        bool gateTriggered = shouldGate && _coolDown == 0;
        if (gateTriggered) {
            // Instead of immediately firing the gate, calculate the offset delay
            uint32_t actualGateOffset = (divisor * _gateOffset) / 100;

            if (actualGateOffset > 0) {
                // Create a delayed gate activation
                _pendingGateOffsetTicks = static_cast<uint8_t>(std::min(actualGateOffset, static_cast<uint32_t>(255))); // Cap at 255
                _pendingGateActivation = true;

                // Don't set the gate yet - it will be set after the offset delay
                // But we still need to reset cooldown and other states
                _activity = true;
                _coolDown = _coolDownMax;
            } else {
                // No offset - set gate immediately as before
                _gateOutput = true;
                // Gate length: use algorithm-determined percentage
                _gateTicks = (divisor * _gatePercent) / 100;
                if (_gateTicks < 1) _gateTicks = 1;  // Minimum 1 tick
                _activity = true;
                // Reset cooldown after triggering
                _coolDown = _coolDownMax;
            }
        }

        // Process pending gate offset if any
        if (_pendingGateActivation && _pendingGateOffsetTicks > 0) {
            _pendingGateOffsetTicks--;
            if (_pendingGateOffsetTicks == 0) {
                // Time to fire the gate after offset delay
                _gateOutput = true;
                // Gate length: use algorithm-determined percentage
                _gateTicks = (divisor * _gatePercent) / 100;
                if (_gateTicks < 1) _gateTicks = 1;  // Minimum 1 tick
                _pendingGateActivation = false;
            }
        }

        // Apply CV with slide/portamento based on cvUpdateMode
        // Free mode: CV updates every step (continuous evolution)
        // Gated mode: CV only updates when gate fires (original Tuesday behavior)
        bool shouldUpdateCv = (_tuesdayTrack.cvUpdateMode() == TuesdayTrack::Free) || gateTriggered;

        if (shouldUpdateCv) {
            _cvTarget = noteVoltage;
            if (_slide > 0) {
                // Calculate slide time: slide * 12 ticks (scaled for our timing)
                int slideTicks = _slide * 12;
                _cvDelta = (_cvTarget - _cvCurrent) / slideTicks;
                _slideCountDown = slideTicks;
            } else {
                // Instant change
                _cvCurrent = _cvTarget;
                _cvOutput = _cvTarget;
                _slideCountDown = 0;
            }
            // Store this as last gated CV for maintaining output in Gated mode
            _lastGatedCv = noteVoltage;
        } else {
            // Gated mode and no gate - maintain last CV value
            // Ensure slide continues if in progress, otherwise keep static
            if (_slideCountDown == 0) {
                _cvOutput = _lastGatedCv;
            }
        }

        // Step advancement and loop handling now done at start of stepTrigger block
        // via tick-based calculation

        // Setup gate timing counters
        if (gateTriggered) {
            _pendingGateActivation = true;
            _pendingGateOffsetTicks = (CONFIG_SEQUENCE_PPQN * _gateOffset) / 100;

            // If a re-trigger was armed by the algorithm, use the short re-trigger
            // length for the gate. Otherwise, use the main gate length.
            if (_retriggerArmed) {
                _gateLengthTicks = _retriggerLength;
            } else {
                _gateLengthTicks = (CONFIG_SEQUENCE_PPQN * _gatePercent) / 100;
            }
        }

        return TickResult::CvUpdate | TickResult::GateUpdate;
    }

    return TickResult::NoUpdate;
}

void TuesdayTrackEngine::update(float dt) {
    // --- CV Sliding ---
    // This happens independently of gate logic
    if (_slideCountDown > 0) {
        _slideCountDown--;
        _cvCurrent += _cvDelta;
    } else {
        _cvCurrent = _cvTarget;
    }
    // Only update final CV output if not in a trill, which has its own CV logic
    if (!_retriggerArmed) {
        _cvOutput = _cvCurrent;
    }


    // --- Gate Timing State Machine ---

    // 1. Handle initial gate offset delay
    if (_pendingGateOffsetTicks > 0) {
        _pendingGateOffsetTicks--;
        return; // Still waiting for the first event to start
    }

    // 2. Activate the very first gate event if it was pending
    if (_pendingGateActivation) {
        _gateOutput = true;
        _pendingGateActivation = false;
        // _gateLengthTicks has already been set correctly in tick()
        // for either a normal note or the first note of a trill.

        // Set the initial CV for the first note
        _cvOutput = _cvTarget;
        return; // Gate is now on, wait for next update
    }

    // 3. Main state machine for gates that are already active or re-triggering
    if (_gateOutput) {
        // --- Gate is currently ON ---
        if (_gateLengthTicks > 0) {
            _gateLengthTicks--;
        } else {
            // ON-time has expired, so turn the gate OFF
            _gateOutput = false;

            // If we are in a re-trigger chain, set the timer for the silent OFF-time
            if (_retriggerArmed && _retriggerCount > 0) {
                _gateLengthTicks = _retriggerPeriod - _retriggerLength; // This now becomes the off-time
            }
        }
    } else {
        // --- Gate is currently OFF ---
        if (_retriggerArmed) {
            if (_retriggerCount > 0) {
                if (_gateLengthTicks > 0) {
                    _gateLengthTicks--;
                } else {
                    // OFF-time has expired, so fire the next re-triggered note
                    _retriggerCount--;
                    _gateOutput = true;
                    _gateLengthTicks = _retriggerLength; // Set ON-time for this new note

                    // Set the trill CV by alternating
                    _isTrillNote = !_isTrillNote;
                    if (_isTrillNote) {
                        _cvOutput = _trillCvTarget;
                    } else {
                        _cvOutput = _cvTarget;
                    }
                }
            } else {
                // Last re-trigger has finished, disarm the system for the next step
                _retriggerArmed = false;
            }
        }
    }

    _activity = _gateOutput || _retriggerArmed;
}
