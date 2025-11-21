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

    default:
        break;
    }
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

    // Calculate base cooldown from power
    // Power 1-16 maps to CoolDownMax 16-1 (inverted)
    int baseCooldown = 17 - power;

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

                // Random gate length - can be very long (25% to 400% = up to 4 beats)
                int gateLengthChoice = _rng.nextRange(100);
                if (gateLengthChoice < 40) {
                    _gatePercent = 25 + (_rng.nextRange(4) * 25);  // 25%, 50%, 75%, 100%
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

                // Random gate length - can be very long (25% to 400% = up to 4 beats)
                int gateLengthChoice = _rng.nextRange(100);
                if (gateLengthChoice < 40) {
                    _gatePercent = 25 + (_rng.nextRange(4) * 25);  // 25%, 50%, 75%, 100%
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

        default:
            shouldGate = false;
            noteVoltage = 0.f;
            break;
        }

        // Apply project scale if enabled
        if (_tuesdayTrack.useScale()) {
            const Scale &scale = _model.project().selectedScale();
            int rootNote = _model.project().rootNote();
            // Treat note as scale degree, convert to voltage
            int scaleNote = note + octave * scale.notesPerOctave();
            noteVoltage = scale.noteToVolts(scaleNote) + (scale.isChromatic() ? rootNote : 0) * (1.f / 12.f);
        }

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
