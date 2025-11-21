#include "TuesdayTrackEngine.h"

#include "Engine.h"
#include "model/Scale.h"

// Initialize algorithm state based on Flow (seed1) and Ornament (seed2)
// This mirrors the original Tuesday Init functions
void TuesdayTrackEngine::initAlgorithm() {
    int flow = _tuesdayTrack.flow();
    int ornament = _tuesdayTrack.ornament();
    int algorithm = _tuesdayTrack.algorithm();

    _cachedFlow = flow;
    _cachedOrnament = ornament;

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

    default:
        break;
    }
}

void TuesdayTrackEngine::reset() {
    _stepIndex = 0;
    _gateLength = 0;
    _gateTicks = 0;
    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;

    // Re-initialize algorithm with current seeds
    initAlgorithm();
}

void TuesdayTrackEngine::restart() {
    reset();
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

    // Check if parameters changed - if so, reinitialize
    if (_cachedFlow != _tuesdayTrack.flow() || _cachedOrnament != _tuesdayTrack.ornament()) {
        initAlgorithm();
    }

    // Calculate step timing with clock sync
    // Use 16th notes as base timing
    uint32_t divisor = CONFIG_PPQN / 4;  // 48 ticks per 16th note

    // Calculate reset divisor for clock sync (reset every 4 measures)
    uint32_t resetDivisor = 4 * _engine.measureDivisor();
    uint32_t relativeTick = resetDivisor == 0 ? tick : tick % resetDivisor;

    // Reset on measure boundary
    if (relativeTick == 0) {
        reset();
    }

    // Check if we're at a step boundary
    bool stepTrigger = (relativeTick % divisor == 0);

    // Handle gate timing
    if (_gateTicks > 0) {
        _gateTicks--;
        if (_gateTicks == 0) {
            _gateOutput = false;
        }
    }

    if (stepTrigger) {
        // Generate output based on algorithm
        bool shouldGate = false;
        float noteVoltage = 0.f;
        int note = 0;
        int octave = 0;

        switch (algorithm) {
        case 0: // TEST - Test patterns
            // Flow: Mode (OCTSWEEPS or SCALEWALKER) + SweepSpeed
            // Ornament: Accent + Velocity
            {
                shouldGate = true;  // Always gate in test mode

                switch (_testMode) {
                case 0:  // OCTSWEEPS - sweep through octaves
                    octave = (_stepIndex % 5);  // 5 octaves
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

                // Tritrance pattern based on step position mod 3
                int phase = (_stepIndex + _triB2) % 3;
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

                if (_stomperCountDown > 0) {
                    // Rest/note-off period
                    shouldGate = false;
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

        default:
            shouldGate = false;
            noteVoltage = 0.f;
            break;
        }

        // Apply gate based on power (probability)
        if (shouldGate) {
            int probability = (power * 100) / 16;
            if ((int)_rng.nextRange(100) < probability) {
                _gateOutput = true;
                // Gate length: 50% of step duration
                _gateTicks = divisor / 2;
                _activity = true;
            }
        }

        // Apply CV
        _cvOutput = noteVoltage;

        // Advance step
        _stepIndex++;

        // Loop handling with algorithm reinitialization for deterministic repeats
        if (loopLength > 0 && _stepIndex >= (uint32_t)loopLength) {
            _stepIndex = 0;
            // Reinitialize algorithm to get same pattern
            initAlgorithm();
        }

        return TickResult::CvUpdate | TickResult::GateUpdate;
    }

    return TickResult::NoUpdate;
}

void TuesdayTrackEngine::update(float dt) {
    // Nothing to update for now
    // Future: slide/glide processing
}
