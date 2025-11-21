#include "TuesdayTrackEngine.h"

#include "Engine.h"
#include "model/Scale.h"

#include <cmath>

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

    default:
        break;
    }
}

void TuesdayTrackEngine::reset() {
    _stepIndex = 0;
    _gateLength = 0;
    _gateTicks = 0;
    _coolDown = 0;
    _slide = 0;
    _cvTarget = 0.f;
    _cvCurrent = 0.f;
    _cvDelta = 0.f;
    _slideCountDown = 0;
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
                    break;
                case 1:  // SCALEWALKER
                default:
                    octave = 0;
                    note = _testNote;
                    _testNote = (_testNote + 1) % 12;
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

                int phase = (step + _triB2) % 3;
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
    bool paramsChanged = (_cachedAlgorithm != _tuesdayTrack.algorithm() ||
                         _cachedFlow != _tuesdayTrack.flow() ||
                         _cachedOrnament != _tuesdayTrack.ornament() ||
                         _cachedLoopLength != _tuesdayTrack.loopLength() ||
                         _cachedScan != _tuesdayTrack.scan());
    if (paramsChanged) {
        _cachedAlgorithm = _tuesdayTrack.algorithm();
        _cachedLoopLength = _tuesdayTrack.loopLength();
        _cachedScan = _tuesdayTrack.scan();
        initAlgorithm();
        _bufferValid = false;
    }

    // Generate buffer for finite loops if needed
    if (loopLength > 0 && !_bufferValid) {
        generateBuffer();
    }

    // Calculate base cooldown from power
    // Linear mapping: Power = number of notes in loop
    // Gap between notes = loopLength / power
    int effectiveLength = (loopLength > 0) ? loopLength : 16;  // Use 16 for infinite loops
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
    // For infinite loops (loopLength == 0), ignore reset measure to allow true infinite patterns
    int resetMeasure = _tuesdayTrack.resetMeasure();
    uint32_t resetDivisor = (resetMeasure > 0 && loopLength > 0) ? resetMeasure * _engine.measureDivisor() : 0;
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
                shouldGate = true;
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

        default:
            shouldGate = false;
            noteVoltage = 0.f;
            break;
            }
        } // End else (infinite loop)

        // Apply octave and transpose from sequence parameters
        int trackOctave = _tuesdayTrack.octave();
        int trackTranspose = _tuesdayTrack.transpose();

        // Get scale and root note (use track settings if not Default, otherwise project)
        int trackScaleIdx = _tuesdayTrack.scale();
        int trackRootNote = _tuesdayTrack.rootNote();

        const Scale &scale = (trackScaleIdx >= 0) ? Scale::get(trackScaleIdx) : _model.project().selectedScale();
        int rootNote = (trackRootNote >= 0) ? trackRootNote : _model.project().rootNote();

        // Apply scale quantization if useScale is enabled or track has specific scale
        if (_tuesdayTrack.useScale() || trackScaleIdx >= 0) {
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
            _gateOutput = true;
            // Gate length: use algorithm-determined percentage
            _gateTicks = (divisor * _gatePercent) / 100;
            if (_gateTicks < 1) _gateTicks = 1;  // Minimum 1 tick
            _activity = true;
            // Reset cooldown after triggering
            _coolDown = _coolDownMax;
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
