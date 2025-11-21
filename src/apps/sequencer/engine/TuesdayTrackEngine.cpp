#include "TuesdayTrackEngine.h"

#include "Engine.h"
#include "model/Scale.h"

void TuesdayTrackEngine::reset() {
    _stepIndex = 0;
    _gateLength = 0;
    _gateTicks = 0;
    _lastNote = 0.f;
    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;
    // Reseed RNG for deterministic loop playback
    _rng.seed(_rngSeed);
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
    int flow = _tuesdayTrack.flow();
    int ornament = _tuesdayTrack.ornament();
    int loopLength = _tuesdayTrack.actualLoopLength();
    int algorithm = _tuesdayTrack.algorithm();

    // Power = 0 means silent
    if (power == 0) {
        _gateOutput = false;
        _cvOutput = 0.f;
        _activity = false;
        return TickResult::NoUpdate;
    }

    // Calculate step timing with clock sync
    // Flow parameter adjusts speed:
    // - Flow 1-8: Slower (divisor 16-9, i.e., whole notes to dotted quarters)
    // - Flow 9-16: Faster (divisor 8-1, i.e., quarter notes to 32nd notes)
    int flowDivisor = 17 - flow; // flow 1->16, divisor 16->1
    if (flowDivisor < 1) flowDivisor = 1;
    uint32_t divisor = (CONFIG_PPQN / 4) * flowDivisor; // Base is 16th notes

    // Calculate reset divisor for clock sync (reset every N measures)
    // Use 4 measures as default reset period
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

        switch (algorithm) {
        case 0: // TEST - Simple ascending chromatic scale
        default:
            // Flow: Step speed (handled above)
            // Ornament: Octave offset (-2 to +2)
            {
                // Gate probability based on power (1-16 maps to 6.25% - 100%)
                int probability = (power * 100) / 16;
                shouldGate = (int(_rng.nextRange(100)) < probability);

                // CV: Chromatic scale based on step index
                // Each step = 1 semitone = 1/12 volt (1V/octave standard)
                int note = _stepIndex % 12; // One octave pattern

                // Ornament as octave offset (1-16 maps to -2 to +2 octaves)
                int octaveOffset = (ornament - 8) / 4;

                noteVoltage = (note + (octaveOffset * 12)) / 12.0f;
            }
            break;

        case 1: // TRITRANCE - Arpeggiated triads
            // Flow: Step speed (handled above)
            // Ornament: Chord type (1-5=minor, 6-11=major, 12-16=dominant7)
            {
                int probability = (power * 100) / 16;
                shouldGate = (int(_rng.nextRange(100)) < probability);

                // Select chord based on ornament
                int triadNotes[4];
                if (ornament <= 5) {
                    // Minor triad
                    triadNotes[0] = 0; triadNotes[1] = 3; triadNotes[2] = 7; triadNotes[3] = 0;
                } else if (ornament <= 11) {
                    // Major triad
                    triadNotes[0] = 0; triadNotes[1] = 4; triadNotes[2] = 7; triadNotes[3] = 0;
                } else {
                    // Dominant 7th
                    triadNotes[0] = 0; triadNotes[1] = 4; triadNotes[2] = 7; triadNotes[3] = 10;
                }

                int triadIndex = _stepIndex % (ornament > 11 ? 4 : 3);
                int octave = (_stepIndex / (ornament > 11 ? 4 : 3)) % 3;

                noteVoltage = (triadNotes[triadIndex] + (octave * 12)) / 12.0f;
            }
            break;

        case 2: // STOMPER - Kick drum pattern (4-on-floor)
            // Flow: Step speed (handled above)
            // Ornament: Pattern variation (1-8=basic, 9-16=syncopated)
            {
                bool strongBeat;
                if (ornament <= 8) {
                    // Basic 4-on-floor: 0, 4, 8, 12
                    strongBeat = (_stepIndex % 4 == 0);
                } else {
                    // Syncopated: 0, 3, 6, 10, 12
                    int pos = _stepIndex % 16;
                    strongBeat = (pos == 0 || pos == 3 || pos == 6 || pos == 10 || pos == 12);
                }

                int probability = strongBeat ? 100 : (power * 100) / 32;
                shouldGate = (int(_rng.nextRange(100)) < probability);

                // Low note for kick drum
                noteVoltage = strongBeat ? -1.0f : 0.f;
            }
            break;

        case 3: // MARKOV - Random walk melody
            // Flow: Step speed (handled above)
            // Ornament: Jump size (1-8=small ±1-2, 9-16=large ±3-5)
            {
                int probability = (power * 100) / 16;
                shouldGate = (int(_rng.nextRange(100)) < probability);

                // Random walk with ornament-controlled jump size
                int maxJump = (ornament <= 8) ? 2 : 5;
                int delta = _rng.nextRange(maxJump * 2 + 1) - maxJump;
                _lastNote = _lastNote + delta / 12.0f;

                // Clamp to reasonable range (-2V to +3V = 5 octaves)
                if (_lastNote < -2.0f) _lastNote = -2.0f;
                if (_lastNote > 3.0f) _lastNote = 3.0f;

                noteVoltage = _lastNote;
            }
            break;
        }

        // Apply gate
        if (shouldGate) {
            _gateOutput = true;
            // Gate length: 50% of step duration
            _gateTicks = divisor / 2;
            _activity = true;
        }

        // Apply CV
        _cvOutput = noteVoltage;

        // Advance step
        _stepIndex++;

        // Loop handling with RNG reseed for deterministic repeats
        if (loopLength > 0 && _stepIndex >= (uint32_t)loopLength) {
            _stepIndex = 0;
            _lastNote = 0.f;
            // Reseed RNG so loop repeats identically
            _rng.seed(_rngSeed);
        }

        return TickResult::CvUpdate | TickResult::GateUpdate;
    }

    return TickResult::NoUpdate;
}

void TuesdayTrackEngine::update(float dt) {
    // Nothing to update for now
    // Future: slide/glide processing
}
