#include "TuesdayTrackEngine.h"

#include "Engine.h"
#include "model/Scale.h"

void TuesdayTrackEngine::reset() {
    _stepIndex = 0;
    _gateLength = 0;
    _gateTicks = 0;
    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;
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

    // Calculate step timing
    // Default divisor: 1 step per beat (quarter note)
    // Flow parameter adjusts speed: 1 = slowest (2 beats), 8 = normal, 16 = fastest (1/4 beat)
    int flowDivisor = 17 - flow; // flow 1->16, divisor 16->1
    if (flowDivisor < 1) flowDivisor = 1;
    uint32_t divisor = (CONFIG_PPQN / 4) * flowDivisor; // Base is 16th notes

    // Check if we're at a step boundary
    bool stepTrigger = (tick % divisor == 0);

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
        case 0: // TEST - Simple ascending scale
        default:
            {
                // Gate probability based on power (1-16 maps to 6.25% - 100%)
                int probability = (power * 100) / 16;
                shouldGate = (int(_rng.nextRange(100)) < probability);

                // CV: Chromatic scale based on step index
                // Each step = 1 semitone = 1/12 volt (1V/octave standard)
                int note = _stepIndex % 12; // One octave pattern

                // Add ornament as octave offset (1-16 maps to -2 to +2 octaves)
                int octaveOffset = (ornament - 8) / 4;

                noteVoltage = (note + (octaveOffset * 12)) / 12.0f;
            }
            break;

        case 1: // TRITRANCE - Arpeggiated triads
            {
                int probability = (power * 100) / 16;
                shouldGate = (int(_rng.nextRange(100)) < probability);

                // Cycle through triad notes: root, 3rd, 5th
                static const int triadNotes[] = {0, 4, 7}; // Major triad in semitones
                int triadIndex = _stepIndex % 3;
                int octave = (_stepIndex / 3) % 3;

                noteVoltage = (triadNotes[triadIndex] + (octave * 12)) / 12.0f;
            }
            break;

        case 2: // STOMPER - Kick drum pattern (low notes, strong beats)
            {
                // Strong beats: 0, 4, 8, 12 (4-on-floor)
                bool strongBeat = (_stepIndex % 4 == 0);
                int probability = strongBeat ? 100 : (power * 100) / 32;
                shouldGate = (int(_rng.nextRange(100)) < probability);

                // Low note for kick drum
                noteVoltage = strongBeat ? -1.0f : 0.f;
            }
            break;

        case 3: // MARKOV - Random walk
            {
                int probability = (power * 100) / 16;
                shouldGate = (int(_rng.nextRange(100)) < probability);

                // Random walk: current note +/- 1-3 semitones
                static float lastNote = 0.f;
                int delta = _rng.nextRange(7) - 3; // -3 to +3
                lastNote = lastNote + delta / 12.0f;
                // Clamp to reasonable range (-2V to +3V = 5 octaves)
                if (lastNote < -2.0f) lastNote = -2.0f;
                if (lastNote > 3.0f) lastNote = 3.0f;

                noteVoltage = lastNote;
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
        if (loopLength > 0 && _stepIndex >= (uint32_t)loopLength) {
            _stepIndex = 0;
        }

        return TickResult::CvUpdate | TickResult::GateUpdate;
    }

    return TickResult::NoUpdate;
}

void TuesdayTrackEngine::update(float dt) {
    // Nothing to update for now
    // Future: slide/glide processing
}
