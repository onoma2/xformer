#include "TuesdayTrackEngine.h"

#include "Engine.h"
#include "model/Scale.h"

#include <cmath>
#include <algorithm>

// Initialize algorithm state based on Flow (seed1) and Ornament (seed2)
//
// DUAL RNG SYSTEM (Tuesday Spec Law 2):
// - _rng (Main RNG, seeded from Flow): Controls FLOW decisions
//   * Timing, mode transitions, gesture state machines, big structural changes
//   * Example: When to mutate, which phrase to play, beat-locked decisions
//
// - _extraRng (Aux RNG, seeded from Ornament): Controls ORNAMENT details
//   * Pitch variations, velocity, articulation, micro-timing
//   * Example: Which note in a scale, accent probability, slide amount
//
// This separation prevents unwanted correlation (e.g., slides always sync with accents).
// Salt value (0x9e3779b9) ensures distinct sequences even when Flow == Ornament.
void TuesdayTrackEngine::initAlgorithm() {
    const auto &_tuesdayTrackRef = tuesdayTrack();
    const auto &sequence = _tuesdayTrackRef.sequence(pattern());
    int flow = sequence.flow();
    int ornament = sequence.ornament();
    int algorithm = sequence.algorithm();

    // Generate seeds
    // Salt the Extra seed to ensure it's distinct even if Flow == Ornament
    uint32_t flowSeed = (flow - 1) << 4;
    uint32_t ornamentSeed = (ornament - 1) << 4;
    
    _uiRng = Random(flow * 37 + ornament * 101);

    _cachedFlow = flow;
    _cachedOrnament = ornament;
    _cachedAlgorithm = algorithm;
    _cachedLoopLength = sequence.loopLength();

    switch (algorithm) {
    case 0: // TEST
        _testMode = (flow - 1) >> 3;
        _testSweepSpeed = ((flow - 1) & 0x3);
        _testAccent = (ornament - 1) >> 3;
        _testVelocity = ((ornament - 1) << 4);
        _testNote = 0;
        
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);
        break;

    case 1: // TRITRANCE
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _triB1 = (_rng.next() & 0x7);
        _triB2 = (_rng.next() & 0x7);

        _triB3 = (_extraRng.next() & 0x15);
        if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
        _triB3 -= 4;
        break;

    case 2: // STOMPER
        // FIX: Corrected RNG assignment to match Tuesday spec (Law 2)
        // Original C had R=seed2, Extra=seed1 (swapped), which violated the spec.
        // Correct: Flow → main RNG (gesture state machine), Ornament → extra RNG (velocity/timing)
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _stomperMode = (_extraRng.next() % 7) * 2;
        _stomperCountDown = 0;
        _stomperLowNote = _rng.next() % 3;
        _stomperLastNote = _stomperLowNote;
        _stomperLastOctave = 0;
        _stomperHighNote[0] = _rng.next() % 7;
        _stomperHighNote[1] = _rng.next() % 5;
        break;

    case 6: // MARKOV
        _rng = Random(flowSeed);
        // Init History
        _markovHistory1 = (_rng.next() & 0x7);
        _markovHistory3 = (_rng.next() & 0x7);
        // Init Matrix
        for (int i = 0; i < 8; i++) {
            for (int j = 0; j < 8; j++) {
                _markovMatrix[i][j][0] = (_rng.next() % 8);
                _markovMatrix[i][j][1] = (_rng.next() % 8);
            }
        }
        break;

    case 7: // CHIPARP 1
        _rng = Random(flowSeed);
        _chipChordSeed = _rng.next();
        _chipRng = Random(_chipChordSeed);
        _chipBase = _rng.next() % 3;
        _chipDir = (_rng.next() >> 7) % 2;
        break;

    case 8: // CHIPARP 2
        _rng = Random(flowSeed);
        _chipChordSeed = _rng.next();
        _chip2Rng = Random(_chipChordSeed);
        _chip2ChordScaler = (_rng.next() % 3) + 2;
        _chip2Offset = (_rng.next() % 5);
        _chip2Len = ((_rng.next() & 0x3) + 1) * 2;
        _chip2TimeMult = _rng.nextBinary() ? (_rng.nextBinary() ? 1 : 0) : 0;
        _chip2DeadTime = 0;
        _chip2Idx = 0;
        _chip2Dir = _rng.nextBinary() ? (_rng.nextBinary() ? 1 : 0) : 0;
        _chip2ChordLen = 3 + (flow >> 2); 
        break;

    case 9: // WOBBLE
        // FIX: Corrected RNG assignment to match Tuesday spec (Law 2)
        // Original C had R=seed2, Extra=seed1 (swapped), which violated the spec.
        // Correct: Flow → main RNG (phase selection), Ornament → extra RNG (velocity)
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        _wobblePhase = 0;
        // Speed based on pattern length (dynamically updated in generateStep)
        // Original: 0xFFFFFFFF / Length
        _wobblePhaseSpeed = 0x08000000; // Slow (will be overridden)
        _wobblePhase2 = 0;
        _wobbleLastWasHigh = 0;
        _wobblePhaseSpeed2 = 0x02000000; // Slower (will be overridden)
        break;

    case 10: // SCALEWALKER
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);
        // Walker position persists across loop boundaries and parameter changes
        // Only reset via reset() (algorithm change) or reseed() (manual Shift+F5)
        break;

    case 3: // APHEX (Mapped from 18)
    case 18:
        _rng = Random(flowSeed);
        for (int i = 0; i < 4; ++i) _aphex_track1_pattern[i] = _rng.next() % 12;
        for (int i = 0; i < 3; ++i) _aphex_track2_pattern[i] = _rng.next() % 3;
        for (int i = 0; i < 5; ++i) _aphex_track3_pattern[i] = (_rng.next() % 8 == 0) ? (_rng.next() % 5) : 0;

        _aphex_pos1 = (ornament * 1) % 4;
        _aphex_pos2 = (ornament * 2) % 3;
        _aphex_pos3 = (ornament * 3) % 5;
        break;

    case 4: // AUTECHRE (Mapped from 19)
    case 19:
        // Seed pattern with variation based on Flow
        _rng = Random(flowSeed);
        for (int i = 0; i < 8; ++i) {
            // 50% chance of root, 25% +1oct, 25% +2oct
            int r = _rng.next() % 4;
            if (r == 0) _autechre_pattern[i] = 12; // +1 oct
            else if (r == 1) _autechre_pattern[i] = 24; // +2 oct
            else _autechre_pattern[i] = 0; // root
            
            // Add some melodic movement
            if (_rng.nextBinary()) _autechre_pattern[i] += (_rng.next() % 5) * 2; // 0, 2, 4, 6, 8
        }
        _autechre_rule_timer = 8 + (flow * 4);

        _rng = Random(ornamentSeed);
        for (int i = 0; i < 8; ++i) _autechre_rule_sequence[i] = _rng.next() % 5;
        _autechre_rule_index = 0;
        break;

    case 5: // STEPWAVE (Mapped from 20)
    case 20:
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);

        if (ornament <= 5) {
            _stepwave_direction = -1;
        } else if (ornament >= 11) {
            _stepwave_direction = 1;
        } else {
            _stepwave_direction = 0;
        }

        _stepwave_step_count = 3 + (_rng.next() % 5);
        _stepwave_current_step = 0;
        _stepwave_chromatic_offset = 0;
        _stepwave_is_stepped = true;
        break;

    default:
        // Fallback to TEST
        _testMode = (flow - 1) >> 3;
        _testSweepSpeed = ((flow - 1) & 0x3);
        _testAccent = (ornament - 1) >> 3;
        _testVelocity = ((ornament - 1) << 4);
        _testNote = 0;
        _rng = Random(flowSeed);
        _extraRng = Random(ornamentSeed + 0x9e3779b9);
        break;
    }
}

void TuesdayTrackEngine::reset() {
    _cachedAlgorithm = -1;
    _cachedFlow = -1;
    _cachedOrnament = -1;
    _cachedLoopLength = -1;

    _stepIndex = 0;
    _displayStep = -1;
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

    _microGateQueue.clear();
    _tieActive = false;

    _retriggerArmed = false;
    _retriggerCount = 0;
    _retriggerPeriod = 0;
    _retriggerLength = 0;
    _retriggerTimer = 0;
    _isTrillNote = false;
    _trillCvTarget = 0.f;
    _ratchetInterval = 0;

    _activity = false;
    _gateOutput = false;
    _cvOutput = 0.f;
    _lastGatedCv = 0.f;

    initAlgorithm();
}

void TuesdayTrackEngine::restart() {
    reset();
}

void TuesdayTrackEngine::reseed() {
    // Reset step to beginning
    _stepIndex = 0;
    _coolDown = 0;

    // Advance the RNGs to get new seeds
    uint32_t newSeed1 = _rng.next();
    uint32_t newSeed2 = _extraRng.next();

    // Re-seed with these new values
    // This simulates "turning the knobs" to a new random position
    // We need to temporarily override the cached Flow/Ornament effectively
    // But initAlgorithm reads from sequence. 
    // Ideally, 'reseed' implies we want variation *within* the current parameters?
    // Or does it mean "Scramble"?
    // For now, we'll just re-init the RNGs directly to simulate a fresh start
    _rng = Random(newSeed1);
    _extraRng = Random(newSeed2);
    
    // We should probably re-run init logic that depends on RNG
    // But without changing the 'flow/ornament' variables which are read from sequence.
    // A full initAlgorithm call would reset seeds based on Sequence.
    // So we need to manually re-init state vars here if we want true random reseed.
    
    const auto &sequence = tuesdayTrack().sequence(pattern());
    int algorithm = sequence.algorithm();

    switch (algorithm) {
    case 1: // TRITRANCE
        _triB1 = (_rng.next() & 0x7);
        _triB2 = (_rng.next() & 0x7);
        _triB3 = (_extraRng.next() & 0x15);
        if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
        _triB3 -= 4;
        break;
    case 2: // STOMPER
        _stomperMode = (_extraRng.next() % 7) * 2;
        _stomperCountDown = 0;
        _stomperLowNote = _rng.next() % 3;
        _stomperHighNote[0] = _rng.next() % 7;
        _stomperHighNote[1] = _rng.next() % 5;
        break;
        // Add others if needed
    }
}

float TuesdayTrackEngine::scaleToVolts(int noteIndex, int octave) const {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    const auto &project = _model.project();

    // 1. Resolve which Scale to use
    int trackScaleIdx = sequence.scale();
    const Scale &scale = (trackScaleIdx < 0)
        ? project.selectedScale()   // -1 = use project scale
        : Scale::get(trackScaleIdx); // 0+ = use specific scale (0 = Semitones/chromatic)

    // 2. Resolve Root Note
    int trackRoot = sequence.rootNote();
    int rootNote = (trackRoot < 0) ? project.rootNote() : trackRoot;

    // 3. Quantization is always applied
    // Scale 0 (Semitones) naturally provides chromatic quantization (all 12 notes)
    bool quantize = true;

    float voltage = 0.f;

    if (quantize) {
        // SCALE MODE: noteIndex is Scale Degree
        int totalDegree = noteIndex + (octave * scale.notesPerOctave());
        
        // Apply Transpose (in Degrees)
        totalDegree += sequence.transpose();

        // Convert to Volts
        voltage = scale.noteToVolts(totalDegree);

        // Add Root Note offset
        voltage += (rootNote * (1.f / 12.f));
    } else {
        // CHROMATIC MODE: noteIndex is Semitone
        int totalSemitones = noteIndex + (octave * 12);
        
        // Apply Transpose (in Semitones)
        totalSemitones += sequence.transpose();

        // 1V/Oct
        voltage = totalSemitones * (1.f / 12.f);
    }

    // 4. Apply Track Octave Offset
    voltage += sequence.octave();

    return voltage;
}

TuesdayTrackEngine::TuesdayTickResult TuesdayTrackEngine::generateStep(uint32_t tick) {
    const auto &sequence = tuesdayTrack().sequence(pattern());
    int algorithm = sequence.algorithm();
    
    // --- CONTEXT CALCULATION (Porting Tick/Beat/Loop) ---

    // Compute divisor (ticks per step) - needed for beat grid calculations
    uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);

    // Beat detection (for algorithms that need to sync to beat grid)
    int stepsPerBeat = (divisor > 0) ? (CONFIG_PPQN / divisor) : 0;
    bool isBeatStart = (stepsPerBeat > 0) && ((_stepIndex % stepsPerBeat) == 0);

    // 1. Derive TPB (Ticks Per Beat) based on Quarter Note (192 PPQN)
    // This ensures "Beat" based algorithms lock to the musical grid.
    uint32_t stepTicks = divisor;
    int tpb = 1;
    if (stepTicks > 0) {
        // Round to nearest integer to handle slight timing drifts or swing if applied globally (unlikely here)
        tpb = std::max(1, (int)((192 + (stepTicks/2)) / stepTicks));
    }
    
    // 2. Derive Effective Loop Length
    // If infinite (0), assume 32 steps (2 bars of 16ths) for LFO scaling
    int loopLength = sequence.actualLoopLength();
    int effectiveLoopLength = (loopLength > 0) ? loopLength : 32;

    // Apply rotation offset (wire up existing UI parameter)
    int rotate = (loopLength > 0) ? sequence.rotate() : 0;
    int rotatedStep = (loopLength > 0) ? ((_stepIndex + rotate + loopLength) % loopLength) : _stepIndex;

    // Ornament parameter (used by multiple algorithms)
    int ornament = sequence.ornament();

    // Standard subdivision count (from ornament parameter)
    // Used by polyrhythmic algorithms for tuplet generation
    int subdivisions = 4;  // Default: straight 16ths
    if (ornament >= 5 && ornament <= 8) subdivisions = 3;      // Triplets (3:4)
    else if (ornament >= 9 && ornament <= 12) subdivisions = 5; // Quintuplets (5:4)
    else if (ornament >= 13) subdivisions = 7;                  // Septuplets (7:4)

    TuesdayTickResult result;
    result.velocity = 255; // Default high velocity

    // Map legacy/alternate algorithms to supported set
    int algo = algorithm;
    if (algorithm == 18) algo = 3; // APHEX
    if (algorithm == 19) algo = 4; // AUTECHRE
    if (algorithm == 20) algo = 5; // STEPWAVE
    
    switch (algo) {
    case 0: // TEST
        {
            result.gateRatio = 75;
            if (_rng.nextRange(100) < sequence.glide()) {
                result.slide = true;
            }

            switch (_testMode) {
            case 0: // OCTSWEEPS
                result.octave = (_stepIndex % 5);
                result.note = 0;
                break;
            case 1: // SCALEWALKER
            default:
                result.octave = 0;
                result.note = _testNote;
                _testNote = (_testNote + 1) % 12;
                break;
            }
            result.velocity = _testVelocity;
        }
        break;

    case 1: // TRITRANCE
        {
            // Gate Length Logic (Probabilistic)
            int gateLenRnd = _rng.nextRange(100);
            if (gateLenRnd < 40) result.gateRatio = 50 + (_rng.nextRange(4) * 12);
            else if (gateLenRnd < 70) result.gateRatio = 100 + (_rng.nextRange(4) * 25);
            else result.gateRatio = 200 + (_rng.nextRange(9) * 25);

            // Slide Logic
            if (_rng.nextRange(100) < sequence.glide()) {
                result.slide = true;
            }

            // Note: TriTrance uses modulo 3, which is its own rhythmic grid (Polyrhythm).
            // It does NOT use the Beat (TPB) concept, so we leave it as is.
            int phase = (_stepIndex + _triB2) % 3;
            switch (phase) {
            case 0:
                if (_extraRng.nextBinary() && _extraRng.nextBinary()) {
                    _triB3 = (_extraRng.next() & 0x15);
                    if (_triB3 >= 7) _triB3 -= 7; else _triB3 = 0;
                    _triB3 -= 4;
                }
                result.octave = 0;
                result.note = _triB3 + 4;
                // Phase 0: Tight/Human (0-10%)
                result.gateOffset = clamp(int(10 - _rng.nextRange(10)), 0, 100);
                break;
            case 1:
                result.octave = 1;
                result.note = _triB3 + 4;
                if (_rng.nextBinary()) _triB2 = (_rng.next() & 0x7);
                // Phase 1: Heavy Swing (25-35%)
                result.gateOffset = clamp(int(25 + _rng.nextRange(10)), 0, 100);
                break;
            case 2:
                result.octave = 2;
                result.note = _triB1;
                result.velocity = 255; // Accent phase
                result.accent = true;
                if (_rng.nextBinary()) _triB1 = ((_rng.next() >> 5) & 0x7);
                // Phase 2: Max Drag (40-50%)
                result.gateOffset = clamp(int(40 + _rng.nextRange(10)), 0, 100);
                break;
            }

            // Velocity variation (unless accented above)
            if (!result.accent) {
                result.velocity = (_rng.nextRange(256) / 2);
            }
        }
        break;

    case 2: // STOMPER
        {
            result.gateRatio = 75;
            int accentoffs = 0;
            uint8_t veloffset = 0;

            if (_stomperCountDown > 0) {
                // Rest period (Countdown)
                result.gateRatio = _stomperCountDown * 25;
                result.velocity = 0; // Silent
                _stomperCountDown--;
                result.note = _stomperLastNote;
                result.octave = _stomperLastOctave;
            } else {
                // Active period
                if (_stomperMode >= 14) {
                    _stomperMode = (_extraRng.next() % 7) * 2;
                }

                // FIX: Original C had PercChance(R, 100) = 100% probability (always true)
                // Randomize notes every tick for chaotic character (Pattern C: Range-Biased Random)
                _stomperLowNote = _rng.next() % 3;
                _stomperHighNote[0] = _rng.next() % 7;
                _stomperHighNote[1] = _rng.next() % 5;

                veloffset = 100; // Base velocity when active
                int maxticklen = 2;

                switch (_stomperMode) {
                case 10: // SLIDEDOWN1
                    result.octave = 1;
                    result.note = _stomperHighNote[_rng.next() % 2];
                    _stomperMode++;
                    break;
                case 11: // SLIDEDOWN2
                    result.octave = 0;
                    result.note = _stomperLowNote;
                    result.slide = true;
                    if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                    _stomperMode = 14;
                    break;
                case 12: // SLIDEUP1
                    result.octave = 0;
                    result.note = _stomperLowNote;
                    _stomperMode++;
                    break;
                case 13: // SLIDEUP2
                    result.octave = 1;
                    result.note = _stomperHighNote[_rng.next() % 2];
                    result.slide = true;
                    if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                    _stomperMode = 14;
                    break;
                case 4: case 5: // LOW
                    accentoffs = 100;
                    result.octave = 0;
                    result.note = _stomperLowNote;
                    if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                    _stomperMode = 14;
                    break;
                case 0: case 1: // PAUSE
                    result.octave = _stomperLastOctave;
                    result.note = _stomperLastNote;
                    veloffset = 0; // Quiet
                    if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                    _stomperMode = 14;
                    break;
                case 6: case 7: // HIGH
                case 8: case 9: // HILOW
                case 2: case 3: // LOWHI
                    // Simplified mapping for brevity, adhering to logic
                    if (_stomperMode == 6 || _stomperMode == 7 || _stomperMode == 9 || _stomperMode == 2) accentoffs = 100;

                    if (_stomperMode == 8 || _stomperMode == 3) {
                        result.octave = 1;
                        result.note = _stomperHighNote[_rng.next() % 2];
                        if (_stomperMode == 8) _stomperMode++; else if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                        if (_stomperMode == 3) _stomperMode = 14;
                    } else {
                        // NOTE: Original C bug faithfully preserved:
                        // STOMPER_HIGH cases (6,7) play low octave/low note instead of high octave/high note
                        // Original C line 101: case STOMPER_HIGH1: NOTE(0, PS->Stomper.LowNote);
                        // This gives STOMPER its characteristic "heavy" sound
                         result.octave = 0;
                         result.note = _stomperLowNote;
                         if (_extraRng.nextBinary()) _stomperCountDown = _extraRng.next() % maxticklen;
                         _stomperMode = 14;
                    }
                    break;
                }
                _stomperLastNote = result.note;
                _stomperLastOctave = result.octave;
                
                // Calculate velocity
                result.velocity = (_extraRng.nextRange(256) / 4) + veloffset;
                // Accent logic: PercChance(50 + accentoffs)
                if (_rng.nextRange(256) >= (50 + accentoffs)) {
                     result.accent = true;
                }
                
                result.chaos = 18; // Base chaos
            }
        }
        break;

    case 3: // APHEX
        {
            // Polyrhythmic Time Warping
            // Subdivisions control the time base for the Modifier Track (Track 2)
            uint32_t modifierStep = _stepIndex;
            if (subdivisions != 4) {
                modifierStep = (_stepIndex * subdivisions) / 4;
            }

            // Track 1: Main Melody (Straight)
            // Use the standard position counters but update them based on warped steps?
            // No, simpler: Use the calculated steps directly to look up pattern.
            // But Aphex uses stateful position counters (_aphex_pos1).
            // We must advance the state differently.
            // Let's apply the warping to the *increment* decision.
            
            // Actually, since we run live every step, we can't skip updates.
            // We must index into the pattern using the warped step.
            
            // Re-calculate positions based on warped indices
            // This makes the algo stateless (good for loop points!)
            // Use rotatedStep for pattern indexing to enable rotation parameter
            int pos1 = rotatedStep % 4; // Track 1 is always straight 4/4
            int pos2 = modifierStep % 3; // Track 2 is warped (intentionally uses unrotated time)
            int pos3 = rotatedStep % 5; // Track 3 is straight 5/4 (natural poly)

            result.note = _aphex_track1_pattern[pos1];
            result.octave = 0;
            result.gateRatio = 75;
            result.velocity = 180;

            uint8_t modifier = _aphex_track2_pattern[pos2];
            if (modifier == 1) {
                result.gateRatio = 25;
                result.velocity = 80; 
                result.chaos = 60; 
            } else if (modifier == 2) {
                result.gateRatio = 100;
                result.slide = true;
            }

            uint8_t bass = _aphex_track3_pattern[pos3];
            if (bass > 0) {
                result.note = bass;
                result.octave = -1;
                result.gateRatio = 90;
                result.velocity = 255;
                result.accent = true;
            }
            
            // Collision logic needs warped pos2
            int polyCollision = ((pos1 * 7) ^ (pos2 * 5) ^ (pos3 * 3)) % 12;
            if (polyCollision > 9) {
                result.chaos = 100;
                result.velocity = 255;
            }
            
            // Polyrhythm Offset
            int polyRhythmFactor = (pos1 + pos2 + pos3) % 12;
            result.gateOffset = clamp(polyRhythmFactor * 8, 0, 100);
        }
        break;
        
    case 4: // AUTECHRE
        {
            // Polyrhythmic Time Warping (Fast Tuplets)
            int flow = sequence.flow();

            // Signal polyrhythm on beat starts ONLY (don't mask intermediate steps)
            if (isBeatStart && subdivisions != 4) {
                // Vary chaos based on flow (probabilistic polyrhythm)
                // flow 0-16 → chaos 20-100 (low flow = sparse, high flow = dense)
                result.chaos = 20 + (flow * 5);
                result.polyCount = subdivisions;  // Pass tuplet count to FX layer
                result.isSpatial = true;          // Polyrhythm mode (spread in time)

                // Generate note offsets from pattern (micro-melody)
                // Use pattern values as intervals, cycling through pattern if needed
                for (int i = 0; i < subdivisions && i < 8; i++) {
                    int patternIdx = (rotatedStep + i) % 8;
                    int patternNote = _autechre_pattern[patternIdx] % 12;
                    // Convert to interval relative to base note (first pattern value)
                    int baseNote = _autechre_pattern[rotatedStep % 8] % 12;
                    result.noteOffsets[i] = patternNote - baseNote;
                }
            }

            // Algorithm logic (ALWAYS run, no masking)
            // Use rotatedStep for pattern indexing to enable rotation parameter
            int patternVal = _autechre_pattern[rotatedStep % 8];
            result.note = patternVal % 12;
            result.octave = patternVal / 12;

            // Random Jitter (0-10%)
            result.gateOffset = clamp(int(_rng.nextRange(11)), 0, 100);

            _autechre_rule_timer--;

            result.velocity = 160;  // NO MASKING
            result.gateRatio = 75;
            // ... (Transformation Logic) ...
            if (_autechre_rule_timer <= 0) {
                uint8_t rule = _autechre_rule_sequence[_autechre_rule_index];
                int intensity = sequence.power() / 2;
                if (result.velocity > 0) {
                    result.velocity = 255;
                    result.accent = true;
                }
                if (rule == 0) { int8_t t = _autechre_pattern[7]; for(int i=7; i>0; --i) _autechre_pattern[i]=_autechre_pattern[i-1]; _autechre_pattern[0]=t; }
                else if (rule == 1) { for(int i=0;i<4;++i) std::swap(_autechre_pattern[i], _autechre_pattern[7-i]); }
                else if (rule == 2) { for(int i=0;i<8;++i) { int o=_autechre_pattern[i]/12; int n=_autechre_pattern[i]%12; n=(6-(n-6)+12)%12; _autechre_pattern[i]=n+o*12; } }
                else if (rule == 3) { for(int i=0;i<8;i+=2) std::swap(_autechre_pattern[i], _autechre_pattern[i+1]); }
                else if (rule == 4) { for(int i=0;i<8;++i) { int o=_autechre_pattern[i]/12; int n=(_autechre_pattern[i]%12+intensity)%12; _autechre_pattern[i]=n+o*12; } }

                // FIX: Use cached flow to prevent timer drift if user tweaks knob mid-pattern
                _autechre_rule_timer = 8 + (_cachedFlow * 4);
                _autechre_rule_index = (_autechre_rule_index + 1) % 8;
            }
        }
        break;

    case 5: // STEPWAVE
        {
             int flow = sequence.flow();

             // Signal micro-sequencing on beat starts (TRILL mode - rapid chromatic stepping)
             if (isBeatStart && subdivisions != 4) {
                 // Vary chaos based on flow (probabilistic micro-sequencing)
                 result.chaos = 20 + (flow * 5);
                 result.polyCount = subdivisions;
                 result.isSpatial = false;  // TRILL mode (rapid succession, not spread)

                 // Update direction based on flow
                 if (flow <= 7) _stepwave_direction = -1;      // Descending
                 else if (flow >= 9) _stepwave_direction = 1;  // Ascending
                 else _stepwave_direction = 0;                 // Random/static

                 _stepwave_step_count = subdivisions;

                 // INTENTIONAL CHROMATIC BYPASS (Spec Law 3 deviation):
                 // Trill mode requires chromatic semitone steps for the sliding effect.
                 // These noteOffsets are added directly to voltage (line 1023), bypassing scale quantization.
                 // This is a documented design choice for STEPWAVE's unique chromatic trill character.
                 for (int i = 0; i < subdivisions && i < 8; i++) {
                     result.noteOffsets[i] = i * _stepwave_direction;  // Chromatic intervals (semitones)
                 }
             }

             // Base note generation (scale-based walking)
             int dir;
             if (flow <= 7) dir = -1;
             else if (flow >= 9) dir = 1;
             else dir = 1;

             int stepSize = 2 + (_stepIndex % 2);

             result.note = (_stepIndex * dir * stepSize) % 7;
             if (result.note < 0) result.note += 7;

             // Always generate velocity (NO MASKING)
             result.velocity = 200;
             if (_rng.nextRange(100) < (20 + flow * 3)) {
                 result.octave = 2;
                 result.velocity = 255;
                 result.accent = true;
             } else {
                 result.octave = 0;
             }

             if (_rng.nextRange(100) < sequence.glide()) {
                 result.slide = true;
                 result.gateRatio = 100;
             } else {
                 result.gateRatio = 75;
             }
             
             // Micro Jitter (0-6%)
             result.gateOffset = clamp(int(_rng.nextRange(7)), 0, 100);
        }
        break;

    case 6: // MARKOV
        {
            result.gateRatio = 75;
            int idx = _rng.nextBinary() ? 1 : 0;
            int newNote = _markovMatrix[_markovHistory1][_markovHistory3][idx];
            
            _markovHistory1 = _markovHistory3;
            _markovHistory3 = newNote;
            
            result.note = newNote; // 0-7 scale degrees
            result.octave = _rng.nextBinary();
            
            // Random Slide/Length
            if (_rng.nextRange(100) < 50) result.gateRatio = 100 + (_rng.nextRange(4) * 25);
            else result.gateRatio = 75;
            
            if (_rng.nextBinary() && _rng.nextBinary()) result.slide = true;

            // 100% accent probability per original Tuesday
            result.accent = true;

            result.velocity = (_rng.nextRange(256) / 2) + 40;
            
            // Markov Timing (Push/Pull)
            if (_rng.nextBinary()) {
                result.gateOffset = clamp(int(10 - _rng.nextRange(10)), 0, 100);
            } else {
                result.gateOffset = clamp(int(15 + _rng.nextRange(10)), 0, 100);
            }
        }
        break;

    case 7: // CHIPARP 1
        {
            result.gateRatio = 75;
            // Use TPB for beat-sync reset (replaces hardcoded % 4)
            int chordpos = _stepIndex % tpb;
            
            if (chordpos == 0) {
                result.accent = true;
                _chipRng = Random(_chipChordSeed);
                if (_rng.nextRange(256) >= 0x20) _chipBase = _rng.next() % 3;
                if (_rng.nextRange(256) >= 0xF0) _chipDir = (_rng.next() >> 7) % 2;
            }
            
            if (_chipDir == 1) chordpos = tpb - chordpos - 1;
            
            if (_chipRng.nextRange(256) >= 0x20) result.accent = true;
            
            if (_chipRng.nextRange(256) >= 0x80) {
                result.slide = true;
            }
            
            if (_chipRng.nextRange(256) >= 0xD0) {
                result.velocity = 0; // Note Off
            } else {
                result.note = (chordpos * 2) + _chipBase;
                result.octave = 0;
                int rnd = (_chipRng.next() >> 7) % 3;
                result.gateRatio = (4 + 6 * rnd) * (100/6); // approx mapping
            }

            // FIX: Add extra velocity on first step of PATTERN (not global step)
            // Original used GENERIC.b2 to track pattern position
            int extraVel = (rotatedStep == 0) ? 127 : 0;
            result.velocity = (_rng.nextRange(256) / 2) + extraVel; 
            
            // Pattern Offset
            result.gateOffset = clamp(int(_stepIndex % 7), 0, 100);
        }
        break;

    case 8: // CHIPARP 2
        {
            result.gateRatio = 75;
            result.octave = 0;
            
            if (_chip2DeadTime > 0) {
                _chip2DeadTime--;
                result.velocity = 0; // Note Off
            } else {
                int deadTimeAdd = 0;
                if (_chip2Idx == _chip2ChordLen) {
                    _chip2Idx = 0;
                    _chip2Len--;
                    result.accent = true;
                    if (_rng.nextRange(256) >= 200) deadTimeAdd = 1 + (_rng.next() % 3);
                    
                    if (_chip2Len == 0) {
                        _chipChordSeed = _rng.next();
                        _chip2ChordScaler = (_rng.next() % 3) + 2;
                        _chip2Offset = (_rng.next() % 5);
                        _chip2Len = ((_rng.next() & 0x3) + 1) * 2;
                        if (_rng.nextBinary()) {
                            _chip2Dir = _rng.nextBinary();
                        }
                    }
                    _chip2Rng = Random(_chipChordSeed);
                }
                
                int scaleidx = (_chip2Idx % _chip2ChordLen);
                if (_chip2Dir) scaleidx = _chip2ChordLen - scaleidx - 1;
                
                result.note = (scaleidx * _chip2ChordScaler) + _chip2Offset;
                _chip2Idx++;
                
                _chip2DeadTime = _chip2TimeMult;
                if (_chip2DeadTime > 0) {
                    result.gateRatio = (1 + _chip2TimeMult) * 100; 
                }
                _chip2DeadTime += deadTimeAdd;
            }
            
            result.velocity = _chip2Rng.nextRange(256);
        }
        break;

    case 9: // WOBBLE
        {
            // Update PhaseSpeed based on Effective Loop Length
            // Original: 0xffffffff / Length
            // We update it dynamically here to allow breathing with Loop Length changes
            _wobblePhaseSpeed = 0xFFFFFFFF / std::max(1, effectiveLoopLength);
            _wobblePhaseSpeed2 = 0xCFFFFFFF / std::max(1, effectiveLoopLength / 4);

            _wobblePhase += _wobblePhaseSpeed;
            _wobblePhase2 += _wobblePhaseSpeed2;

            // PercChance(R, seed2). seed2=ornament.
            // Map ornament 0-16 to 0-255 threshold
            if (_rng.nextRange(256) >= (sequence.ornament() * 16)) {
                // Use Phase 2
                // FIX: Wrap 5-bit phase (0-31) to scale degrees and overflow to octaves
                int rawPhase = (_wobblePhase2 >> 27) & 0x1F;
                result.note = rawPhase % 7;  // Wrap to scale degrees 0-6
                result.octave = 1 + (rawPhase / 7);  // Base octave + overflow

                if (_wobbleLastWasHigh == 0) {
                    if (_rng.nextRange(256) >= 200) result.slide = true;
                }
                _wobbleLastWasHigh = 1;
            } else {
                // Use Phase 1
                // FIX: Wrap 5-bit phase (0-31) to scale degrees and overflow to octaves
                int rawPhase = (_wobblePhase >> 27) & 0x1F;
                result.note = rawPhase % 7;  // Wrap to scale degrees 0-6
                result.octave = (rawPhase / 7);  // Overflow octaves

                if (_wobbleLastWasHigh == 1) {
                    if (_rng.nextRange(256) >= 200) result.slide = true;
                }
                _wobbleLastWasHigh = 0;
            }

            result.velocity = (_extraRng.nextRange(256) / 4);
            if (_rng.nextRange(256) >= 50) result.accent = true;
            
            // Wobble Phase Offset (0-30%)
            result.gateOffset = clamp(int(((_wobblePhase + _wobblePhase2) >> 28) % 30), 0, 100);
        }
        break;

    case 10: // SCALEWALKER - Rhythmic subdivisions with scale walking
        {
            int flow = sequence.flow();

            // 1. Direction
            int direction = (flow <= 7) ? -1 : ((flow == 8) ? 0 : 1);

            // 2. Base note is current walker position
            result.note = _scalewalker_pos;
            result.octave = 0;
            result.velocity = 100 + (sequence.power() * 10);  // 100-260 range
            result.gateRatio = 75;

            // 3. Trigger micro-sequencing on beat starts (isBeatStart calculated at top of generateStep)
            if (isBeatStart && subdivisions != 4) {
                result.chaos = 100;  // Always fire
                result.polyCount = subdivisions;
                result.isSpatial = true;  // Spread across beat

                // Generate sequential scale degree offsets
                for (int i = 0; i < subdivisions && i < 8; i++) {
                    result.noteOffsets[i] = i * direction;
                }
            }
            else if (subdivisions != 4) {
                // Polyrhythm mode on non-beat steps: mute to prevent gate leakage
                result.velocity = 0;
            }

            // Advance walker every step (not just on beat starts)
            _scalewalker_pos = (_scalewalker_pos + direction + 7) % 7;
        }
        break;
    }

    return result;
}

TrackEngine::TickResult TuesdayTrackEngine::tick(uint32_t tick) {
    if (mute()) { _gateOutput=false; _cvOutput=0.f; _activity=false; return TickResult::NoUpdate; }
    
    const auto &sequence = tuesdayTrack().sequence(pattern());
    // int power = sequence.power(); 
    // Power check moved inside stepTrigger to allow for Skew modulation

    // Parameter Change Check
    bool paramsChanged = (_cachedAlgorithm != sequence.algorithm() ||
                         _cachedFlow != sequence.flow() ||
                         _cachedOrnament != sequence.ornament() ||
                         _cachedLoopLength != sequence.loopLength());
    if (paramsChanged) {
        initAlgorithm();
    }

    // Time Calculation
    uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);

    // Apply Start Delay (Time Shift)
    uint32_t startTicks = sequence.start() * divisor;
    if (tick < startTicks) {
        _gateOutput = false;
        _activity = false;
        return TickResult::NoUpdate;
    }
    tick -= startTicks;

    int loopLength = sequence.actualLoopLength();
    uint32_t resetDivisor = (loopLength > 0) ? (loopLength * divisor) : 0;
    
    uint32_t relativeTick = (resetDivisor == 0) ? tick : tick % resetDivisor;
    
    // Finite Loop Reset
    if (resetDivisor > 0 && relativeTick == 0) {
        initAlgorithm(); // Reset RNGs to loop start state
        _stepIndex = 0;
    }

    bool stepTrigger = (relativeTick % divisor == 0);

    // --- MICRO-GATE QUEUE PROCESSING ---

    // Process scheduled gates from micro-queue
    while (!_microGateQueue.empty() && tick >= _microGateQueue.front().tick) {
        auto event = _microGateQueue.front();
        _microGateQueue.pop();

        _gateOutput = event.gate;
        _activity = event.gate;

        // Update CV from micro-gate queue (always apply when gate fires)
        // Micro-gates store their target CV and should always use it
        if (event.gate) {
            _cvOutput = event.cvTarget;
        }
    }

    // --- ENGINE UPDATE LOGIC ---
    
    // Handle existing gate timer
    if (_gateTicks > 0) {
        _gateTicks--;
        if (_gateTicks == 0) {
             // Gate OFF
             _gateOutput = false;
             // If in retrigger cycle, this is the gap between ratchets
             // Logic handled in update()
        }
    }
    
    // Handle CV Slide
    if (_slideCountDown > 0) {
        _cvCurrent += _cvDelta;
        _slideCountDown--;
        if (_slideCountDown == 0) _cvCurrent = _cvTarget;
        _cvOutput = _cvCurrent;
    }

    if (stepTrigger) {
        // Advance Step
        uint32_t calculatedStep = relativeTick / divisor;
        _stepIndex = calculatedStep;
        _displayStep = _stepIndex;

        // 1. GENERATE (The Brain)
        TuesdayTickResult result = generateStep(tick);
        
        // Apply Algorithm's Timing Offset with User Scaler
        // Scaler Model: 
        // - User Knob 0%   -> Force 0 (Strict/Quantized)
        // - User Knob 50%  -> 1x (Original Algo Timing)
        // - User Knob 100% -> 2x (Exaggerated Swing)
        int userScaler = sequence.gateOffset();
        _gateOffset = clamp((int)((result.gateOffset * userScaler * 2) / 100), 0, 100);
        
        // 2. FX LAYER (Post-Processing)
        
        // A. Polyrhythm / Trill Detection
        _retriggerArmed = false;

        if (result.polyCount > 0 && result.chaos > 50) {
            // MICRO-SEQUENCING MODE: Distribute N gates with independent pitches
            int tupleN = result.polyCount;

            // Determine timing distribution (spatial = spread, temporal = rapid)
            uint32_t windowTicks = result.isSpatial ? (4 * divisor) : divisor;
            uint32_t spacing = windowTicks / tupleN;  // Even distribution

            // Gate length: Use algorithm's gateRatio
            uint32_t baseGateLen = (spacing * result.gateRatio) / 100;
            uint32_t userGate = sequence.gateLength();
            uint32_t gateLen = (baseGateLen * userGate) / 50;
            if (gateLen < 1) gateLen = 1;

            // Gate offset (applies to FIRST gate only)
            uint32_t gateOffsetTicks = (divisor * _gateOffset) / 100;

            // Schedule N gate ON/OFF pairs with independent pitches
            // Apply gate offset to the base tick, then space gates evenly from there
            uint32_t baseTick = tick + gateOffsetTicks;

            for (int i = 0; i < tupleN; i++) {
                uint32_t offset = (i * spacing);

                // Calculate voltage for THIS gate using note offset
                float volts;
                if (result.isSpatial) {
                    // POLYRHYTHM: Scale-degree based (quantized to scale)
                    // noteOffsets are intervals in scale degrees
                    int noteWithOffset = result.note + result.noteOffsets[i];
                    volts = scaleToVolts(noteWithOffset, result.octave);
                } else {
                    // TRILL: Chromatic (bypass quantization)
                    // noteOffsets are semitone intervals, add directly to voltage
                    float baseVolts = scaleToVolts(result.note, result.octave);
                    volts = baseVolts + (result.noteOffsets[i] / 12.f);
                }

                _microGateQueue.push({ baseTick + offset, true, volts });
                _microGateQueue.push({ baseTick + offset + gateLen, false, volts });
            }

            _retriggerArmed = true;  // Skip normal gate logic
        }
        else if (result.chaos > 0) {
            // TRILL MODE: Original probabilistic ratchet (unchanged)
            int chance = (result.chaos * sequence.trill()) / 100;
            if (_uiRng.nextRange(100) < chance) {
                _retriggerArmed = true;
                _retriggerCount = 2;
                _retriggerPeriod = divisor / 3;
                _retriggerLength = _retriggerPeriod / 2;
                _ratchetInterval = 0;

                // StepWave special trill (climbing intervals)
                if (sequence.algorithm() == 20) {
                    _ratchetInterval = _stepwave_direction;
                    _retriggerCount = _stepwave_step_count - 1;
                    _retriggerPeriod = divisor / _stepwave_step_count;
                    _retriggerLength = _retriggerPeriod / 2;
                }
            }
        }

        // B. Glide
        _slide = 0;
        if (result.slide && sequence.glide() > 0) {
             _slide = 1 + (sequence.glide() / 30); // Map 0-100 to 1-4 range approx
        }

        // C. Density (Power Gating)
        int power = sequence.power();
        int loopLength = sequence.actualLoopLength();
        int skew = sequence.skew();

        if (loopLength > 0 && skew != 0) {
             // Skew Logic: -8 to +8 over the loop
             int currentPos = _stepIndex % loopLength;
             // Linear interpolation: -skew at start, +skew at end
             // Formula: offset = -skew + (2 * skew * pos) / (len - 1)
             int offset = -skew + ((2 * skew * currentPos) / std::max(1, loopLength - 1));
             power = std::max(0, std::min(16, power + offset));
        }

        // Calculate Base Cooldown from Power (Linear 1-16)
        // High Power (16) -> Cooldown 1 (Every step allowed)
        // Low Power (1) -> Cooldown 16 (Sparse)
        int baseCooldown = 17 - power;
        if (baseCooldown < 1) baseCooldown = 1;
        _coolDownMax = baseCooldown;
        
        // Decrement cooldown state
        if (_coolDown > 0) {
            _coolDown--;
            if (_coolDown > _coolDownMax) _coolDown = _coolDownMax;
        }
        
        // Gate Decision:
        // 1. Must satisfy Cooldown (Density)
        // 2. Velocity helps overcome Cooldown (Original Tuesday Logic)
        // Original: if (vel >= cooldown)
        // We map velocity 0-255 to roughly 0-16 range to compare with cooldown?
        // Or just use raw velocity if cooldown was velocity-based?
        // In C++ port, cooldown is a counter.
        // Let's implement the "Beat the Clock" logic:
        // If cooldown is high, we need high velocity to trigger.
        // If we trigger, we reset cooldown.
        
        bool densityGate = false;
        bool accentActive = false;  // Track accent for gate length extension

        // 0. Explicit Mute from Algorithm (e.g. Polyrhythm Masking) OR Power 0
        if (result.velocity == 0 || power == 0) {
            densityGate = false;
        }
        // 1. ACCENT: Always fire + extend gate length (mapped from original GATE_ACCENT output)
        else if (result.accent) {
            densityGate = true;
            accentActive = true;  // Will extend gate length later
        }
        // 2. Timer Expired (Power)
        else if (_coolDown == 0) {
            densityGate = true;
        }
        // 3. High Velocity Override
        else {
            int velDensity = result.velocity / 16;
            if (velDensity >= _coolDown) {
                densityGate = true;
            }
        }
        
        if (densityGate) {
             // Reset pressure
             _coolDown = _coolDownMax;

             // D. Gate Length (Scaler)
             uint32_t gateOffsetTicks = (divisor * _gateOffset) / 100;
             uint32_t baseLen = (divisor * result.gateRatio) / 100;
             uint32_t userGate = sequence.gateLength();
             uint32_t gateLengthTicks = (baseLen * userGate) / 50;
             if (gateLengthTicks < 1) gateLengthTicks = 1;

             // Accent: Extend gate length by 50% (per original Tuesday's GATE_ACCENT mapping)
             if (accentActive) {
                 gateLengthTicks = (gateLengthTicks * 3) / 2;
             }

             // Calculate CV voltage
             float volts = scaleToVolts(result.note, result.octave);

             // Schedule gates in micro-queue (not immediate)
             // Skip if retrigger/polyrhythm will handle it
             if (!_retriggerArmed) {
                 _microGateQueue.push({ tick + gateOffsetTicks, true, volts });
                 _microGateQueue.push({ tick + gateOffsetTicks + gateLengthTicks, false, volts });
                 _activity = true;
             }

             // Handle CV Slide
             if (_slide > 0) {
                 _cvTarget = volts;
                 int slideTicks = _slide * 12;
                 _cvDelta = (_cvTarget - _cvCurrent) / slideTicks;
                 _slideCountDown = slideTicks;
             } else {
                 _cvCurrent = volts;
                 _cvOutput = volts;
                 _slideCountDown = 0;
             }

             // Handle Trill Start CV
             if (_retriggerArmed) {
                 _gateTicks = _retriggerLength;
                 _trillCvTarget = volts;
             }

        } else {
             // Ghost note / Rest
             // Update CV anyway if Free mode?
             // BUT: Don't update on explicitly muted steps (velocity=0 from polyrhythm suppression)
             if (sequence.cvUpdateMode() == TuesdaySequence::Free && result.velocity > 0) {
                  float volts = scaleToVolts(result.note, result.octave);
                  _cvCurrent = volts;
                  _cvOutput = volts;
             }
        }
        
        return TickResult::CvUpdate | TickResult::GateUpdate;
    }

    return TickResult::NoUpdate;
}

void TuesdayTrackEngine::update(float dt) {
    // CV Slide Update
    if (_slideCountDown > 0) {
        _slideCountDown--;
        _cvCurrent += _cvDelta;
        if (!_retriggerArmed) _cvOutput = _cvCurrent;
    }

    // Retrigger Logic
    if (_retriggerArmed) {
        if (_gateOutput) {
            // Waiting for ON phase to end
            if (_gateTicks > 0) _gateTicks--;
            else {
                _gateOutput = false; // Gap
                _gateTicks = _retriggerPeriod - _retriggerLength; // Gap length
            }
        } else {
            // Waiting for OFF phase to end (Gap)
            if (_gateTicks > 0) _gateTicks--;
            else {
                if (_retriggerCount > 0) {
                    _retriggerCount--;
                    _gateOutput = true;
                    _gateTicks = _retriggerLength;
                    
                    // Apply Trill Interval
                    if (_ratchetInterval != 0) {
                        // Arp/Step mode
                         // We need to know current note to offset it.
                         // This is tricky without persistent note state.
                         // Simplification: just add voltage
                         _cvOutput += (_ratchetInterval / 12.f);
                    } else {
                        // Standard Trill: Toggle
                        // Need to know interval (e.g. +2 semitones)
                        // For now, just keep same voltage (Ratchet) or simple offset
                        // Original code added +2/12v.
                         _cvOutput += (2.f / 12.f); 
                    }
                } else {
                    _retriggerArmed = false;
                }
            }
        }
    }
    
    _activity = _gateOutput || _retriggerArmed;
}