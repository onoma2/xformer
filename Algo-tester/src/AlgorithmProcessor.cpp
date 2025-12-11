#include "AlgorithmProcessor.h"
#include "dj_fft.h"
#include <algorithm>
#include <random>

// Random number generator for algorithm simulation
static std::random_device rd;
static std::mt19937 gen(rd());
static std::uniform_real_distribution<float> real_dist(0.0f, 1.0f);
static std::uniform_int_distribution<int> int_dist(0, 100);

AlgorithmProcessor::AlgorithmProcessor(int sequenceLength) : m_sequenceLength(sequenceLength) {
}

std::string AlgorithmProcessor::getAlgorithmName(AlgorithmType type) {
    switch (type) {
        case AlgorithmType::Test: return "TEST";
        case AlgorithmType::Tritrance: return "TRITRANCE";
        case AlgorithmType::Stomper: return "STOMPER";
        case AlgorithmType::Markov: return "MARKOV";
        case AlgorithmType::Chiparp: return "CHIPARP";
        case AlgorithmType::Goaacid: return "GOACID";
        case AlgorithmType::Snh: return "SNH";
        case AlgorithmType::Wobble: return "WOBBLE";
        case AlgorithmType::Techno: return "TECHNO";
        case AlgorithmType::Funk: return "FUNK";
        case AlgorithmType::Drone: return "DRONE";
        case AlgorithmType::Phase: return "PHASE";
        case AlgorithmType::Raga: return "RAGA";
        case AlgorithmType::Ambient: return "AMBIENT";
        case AlgorithmType::Acid: return "ACID";
        case AlgorithmType::Drill: return "DRILL";
        case AlgorithmType::Minimal: return "MINIMAL";
        case AlgorithmType::Kraft: return "KRAFT";
        case AlgorithmType::Aphex: return "APHEX";
        case AlgorithmType::Autechre: return "AUTECHRE";
        case AlgorithmType::Stepwave: return "STEPWAVE";
        case AlgorithmType::Custom: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

std::string AlgorithmProcessor::getAlgorithmDescription(AlgorithmType type) {
    switch (type) {
        case AlgorithmType::Test: return "Test pattern algorithm with two modes: OCTSWEEPS and SCALEWALKER";
        case AlgorithmType::Tritrance: return "German minimal style arpeggios based on a 3-phase cycling pattern";
        case AlgorithmType::Stomper: return "Acid bass patterns with slides and state machine transitions";
        case AlgorithmType::Markov: return "Markov chain melody generation using an 8x8x2 transition matrix";
        case AlgorithmType::Chiparp: return "Chiptune arpeggio patterns with chord progressions";
        case AlgorithmType::Goaacid: return "Goa/psytrance acid patterns with systematic transposition";
        case AlgorithmType::Snh: return "Sample & Hold random walk algorithm";
        case AlgorithmType::Wobble: return "Dual-phase LFO bass with alternating patterns";
        case AlgorithmType::Techno: return "Four-on-floor club patterns with kick and hi-hat patterns";
        case AlgorithmType::Funk: return "Syncopated funk grooves with ghost notes";
        case AlgorithmType::Drone: return "Sustained drone textures with slow movement";
        case AlgorithmType::Phase: return "Minimalist phasing patterns with gradual shifts";
        case AlgorithmType::Raga: return "Indian classical melody patterns with traditional scales";
        case AlgorithmType::Ambient: return "Harmonic drone & event scheduler";
        case AlgorithmType::Acid: return "303-style patterns with slides";
        case AlgorithmType::Drill: return "UK Drill hi-hat rolls and bass slides";
        case AlgorithmType::Minimal: return "Staccato bursts and silence";
        case AlgorithmType::Kraft: return "Precise mechanical sequences";
        case AlgorithmType::Aphex: return "Polyrhythmic Event Sequencer";
        case AlgorithmType::Autechre: return "Algorithmic Transformation Engine";
        case AlgorithmType::Stepwave: return "Scale stepping with chromatic trill";
        case AlgorithmType::Custom: return "User-defined custom algorithm";
        default: return "Unknown algorithm";
    }
}

AlgorithmProcessor::SignalData AlgorithmProcessor::process(const AlgorithmParameters& params) {
    SignalData result;
    
    // Initialize all sequences to the correct size
    result.noteSequence.resize(m_sequenceLength, 0.5f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.5f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);
    
    // Generate algorithm-specific data based on type
    switch (params.type) {
        case AlgorithmType::Test: return generateTestAlgorithm(params);
        case AlgorithmType::Tritrance: return generateTritranceAlgorithm(params);
        case AlgorithmType::Stomper: return generateStomperAlgorithm(params);
        case AlgorithmType::Markov: return generateMarkovAlgorithm(params);
        case AlgorithmType::Chiparp: return generateChiparpAlgorithm(params);
        case AlgorithmType::Goaacid: return generateGoaacidAlgorithm(params);
        case AlgorithmType::Snh: return generateSnhAlgorithm(params);
        case AlgorithmType::Wobble: return generateWobbleAlgorithm(params);
        case AlgorithmType::Techno: return generateTechnoAlgorithm(params);
        case AlgorithmType::Funk: return generateFunkAlgorithm(params);
        case AlgorithmType::Drone: return generateDroneAlgorithm(params);
        case AlgorithmType::Phase: return generatePhaseAlgorithm(params);
        case AlgorithmType::Raga: return generateRagaAlgorithm(params);
        case AlgorithmType::Ambient: return generateAmbientAlgorithm(params);
        case AlgorithmType::Acid: return generateAcidAlgorithm(params);
        case AlgorithmType::Drill: return generateDrillAlgorithm(params);
        case AlgorithmType::Minimal: return generateMinimalAlgorithm(params);
        case AlgorithmType::Kraft: return generateKraftAlgorithm(params);
        case AlgorithmType::Aphex: return generateAphexAlgorithm(params);
        case AlgorithmType::Autechre: return generateAutechreAlgorithm(params);
        case AlgorithmType::Stepwave: return generateStepwaveAlgorithm(params);
        case AlgorithmType::Custom: return generateCustomAlgorithm(params);
        default: return generateTestAlgorithm(params);
    }
    
    // Calculate spectrum for visualization
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);
    
    return result;
}

// Helper functions
float AlgorithmProcessor::randomFloat(float min, float max) {
    return min + (max - min) * real_dist(gen);
}

int AlgorithmProcessor::randomInt(int min, int max) {
    return std::uniform_int_distribution<int>(min, max)(gen);
}

void AlgorithmProcessor::computeSpectrumPair(const std::vector<float>& signal, std::vector<float>& spectrum, std::vector<float>& oversampled_spectrum) {
    int size = signal.size();
    
    // Compute spectrum for normal signal
    dj::fft_arg<float> fft_data(size);
    for(int i = 0; i < size; ++i) {
        fft_data[i] = {signal[i], 0.0f};
    }
    auto fft_result = dj::fft1d(fft_data, dj::fft_dir::DIR_FWD);

    spectrum.resize(size / 2);
    for(int i = 0; i < size / 2; ++i) {
        float mag = std::abs(fft_result[i]);
        spectrum[i] = 20.0f * log10(mag + 1e-6);
    }
    
    // Compute oversampled spectrum
    int oversample_size = size * 2;
    std::vector<float> oversample_signal(oversample_size);
    for(int i = 0; i < oversample_size; ++i) {
        int src_idx = (i < size) ? i : size - 1;
        oversample_signal[i] = signal[src_idx];
    }
    
    dj::fft_arg<float> oversampled_fft_data(oversample_size);
    for(int i = 0; i < oversample_size; ++i) {
        oversampled_fft_data[i] = {oversample_signal[i], 0.0f};
    }
    auto oversampled_fft_result = dj::fft1d(oversampled_fft_data, dj::fft_dir::DIR_FWD);

    oversampled_spectrum.resize(oversample_size / 2);
    for(int i = 0; i < oversample_size / 2; ++i) {
        float mag = std::abs(oversampled_fft_result[i]);
        oversampled_spectrum[i] = 20.0f * log10(mag + 1e-6);
    }
}

// Core algorithm implementations
AlgorithmProcessor::SignalData AlgorithmProcessor::generateTestAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);
    
    int testMode = (params.flow - 1) >> 3;  // 0 or 1 (1-8 = mode 0, 9-16 = mode 1)
    
    if (testMode == 0) { // OCTSWEEPS
        for (int i = 0; i < m_sequenceLength; i++) {
            int octave = (i % 5);
            result.noteSequence[i] = ((octave * 12) / 127.0f);  // Convert to 0-1 normalized
            result.gateSequence[i] = 0.75f;  // 75% gate length
            result.gateOffsetSequence[i] = (i % 4) * 0.1f;  // Timing variations
        }
    } else { // SCALEWALKER
        for (int i = 0; i < m_sequenceLength; i++) {
            int note = (i % 12);
            result.noteSequence[i] = (note / 11.0f);  // Convert to 0-1 normalized (0-11 range)
            result.gateSequence[i] = 0.75f;  // 75% gate length
        }
    }
    
    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);
    
    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateTritranceAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);
    
    // Simulate the 3-phase cycling pattern
    for (int i = 0; i < m_sequenceLength; i++) {
        int phase = (i + (params.flow - 1)) % 3;  // Use flow to offset phase
        float gateLength = 0.75f;
        
        switch (phase) {
            case 0: // Phase 0 (step + b2) % 3 == 0: Plays note b3+4 in octave 0
                result.noteSequence[i] = ((4 + (params.ornament - 1)) % 12) / 11.0f;
                result.gateOffsetSequence[i] = 0.10f + randomFloat(0.0f, 0.15f);  // Early timing
                break;
            case 1: // Phase 1 (step + b2) % 3 == 1: Plays note b3+4 in octave 1
                result.noteSequence[i] = ((4 + (params.ornament - 1) + 12) % 36) / 35.0f;
                result.gateOffsetSequence[i] = 0.45f + randomFloat(0.0f, 0.10f);  // On-time or slightly late
                break;
            case 2: // Phase 2 (step + b2) % 3 == 2: Plays note b1 in octave 2
                result.noteSequence[i] = ((params.flow - 1) % 12 + 24) / 35.0f;  // Higher octave
                result.gateOffsetSequence[i] = 0.60f + randomFloat(0.0f, 0.20f);  // Swing delay
                break;
        }
        
        // Gate length: 50% to 400% of step duration
        int gateLengthChoice = randomInt(0, 99);
        if (gateLengthChoice < 40) {
            gateLength = 0.5f + randomInt(0, 3) * 0.12f;  // 50%, 62%, 74%, 86%
        } else if (gateLengthChoice < 70) {
            gateLength = 1.0f + randomInt(0, 3) * 0.25f; // 100%, 125%, 150%, 175%
        } else {
            gateLength = 2.0f + randomInt(0, 8) * 0.25f; // 200% to 400%
        }
        
        result.gateSequence[i] = gateLength;
        
        // Slide based on glide parameter
        if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = randomInt(1, 3) * 0.25f;  // 1-3 amount
        }
    }
    
    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);
    
    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateStomperAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);
    
    // Simulate the 14-state pattern machine
    int mode = ((params.flow - 1) % 7) * 2;  // Initial pattern mode
    int lowNote = (params.ornament - 1) % 3;
    int highNote0 = randomInt(0, 6);
    int highNote1 = randomInt(0, 4);
    
    for (int i = 0; i < m_sequenceLength; i++) {
        // Determine mode based on position for this step
        int currentMode = mode;
        if (i % 16 < 14) {
            currentMode = (mode + (i % 16)) % 15;
        } else {
            currentMode = 14;
        }
        
        float note = lowNote / 11.0f;
        int octave = 0;
        
        switch (currentMode) {
            case 0: case 1: case 2: case 3:
                octave = 0;
                note = lowNote / 11.0f;
                result.gateOffsetSequence[i] = 0.05f;   // Early timing
                break;
            case 4: case 5: case 6: case 7:
                octave = 0;
                note = lowNote / 11.0f;
                result.gateOffsetSequence[i] = 0.25f;  // Mid-range timing
                break;
            case 8: case 9: case 10: case 11:
                octave = 1;
                note = (highNote0 % 7) / 6.0f;
                result.gateOffsetSequence[i] = 0.75f;  // Later timing
                break;
            case 12: case 13:
                octave = 0;
                note = lowNote / 11.0f;
                result.gateOffsetSequence[i] = 0.15f;  // Moderate timing
                break;
            case 14:
            default:
                octave = 1;
                note = (highNote1 % 5) / 4.0f;
                result.gateOffsetSequence[i] = 0.0f;   // On-beat timing
                break;
        }
        
        result.noteSequence[i] = (note + octave) / 2.0f;  // Adjust for octave
        result.gateSequence[i] = 0.75f;
        
        // Slide based on glide parameter
        if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = randomInt(1, 3) * 0.25f;
        }
        
        // Occasionally have longer gate during countdown
        if (i % 16 < 5) {  // Simulate countdown periods
            result.gateSequence[i] = (5 - (i % 16)) * 0.25f;
        }
    }
    
    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);
    
    return result;
}

// Implementing more algorithms to complete the set...
AlgorithmProcessor::SignalData AlgorithmProcessor::generateMarkovAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);
    
    // Initialize history values based on seeds
    int history1 = (params.flow - 1) & 0x7;
    int history3 = (params.flow - 1) & 0x7;
    
    // Create a simplified transition matrix
    std::vector<std::vector<std::vector<int>>> matrix(8, std::vector<std::vector<int>>(8, std::vector<int>(2)));
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            matrix[i][j][0] = randomInt(0, 7);
            matrix[i][j][1] = randomInt(0, 7);
        }
    }
    
    for (int i = 0; i < m_sequenceLength; i++) {
        int idx = randomInt(0, 1) ? 1 : 0;
        int note = matrix[history1][history3][idx];
        result.noteSequence[i] = note / 7.0f;
        
        // Update history
        history1 = history3;
        history3 = note;
        
        // Determine octave
        result.noteSequence[i] = (result.noteSequence[i] + (randomInt(0, 1) ? 1 : 0)) / 2.0f;
        
        // Gate length: 50% to 400% of step duration
        int gateLengthChoice = randomInt(0, 99);
        if (gateLengthChoice < 40) {
            result.gateSequence[i] = 0.5f + randomInt(0, 3) * 0.12f;
        } else if (gateLengthChoice < 70) {
            result.gateSequence[i] = 1.0f + randomInt(0, 3) * 0.25f;
        } else {
            result.gateSequence[i] = 2.0f + randomInt(0, 8) * 0.25f;
        }
        
        // Slide based on glide parameter
        if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = randomInt(1, 3) * 0.25f;
        }
        
        // Markov: Use transition probabilities to determine timing variations
        int transition_delta = abs(note - history3);
        int history_factor = (history1 + history3) % 11;  // Keep in 0-10 range
        result.gateOffsetSequence[i] = (transition_delta * 0.1f) + (history_factor * 0.02f);
        if (result.gateOffsetSequence[i] > 1.0f) result.gateOffsetSequence[i] = 1.0f;
    }
    
    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);
    
    return result;
}

// Additional algorithm implementations would follow the same pattern...
// For brevity, I'll implement a few more essential ones
AlgorithmProcessor::SignalData AlgorithmProcessor::generateChiparpAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.5f);
    result.velocitySequence.resize(m_sequenceLength, 0.5f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);
    
    int base = (params.flow - 1) % 3;
    bool dir = (params.ornament - 1) % 2 == 0 ? false : true;  // Ascending or descending
    
    for (int i = 0; i < m_sequenceLength; i++) {
        int chordpos = i % 4;
        int pos = chordpos;
        if (dir) {
            pos = 3 - chordpos;
        }
        
        // Create 4-step chord pattern (0, 2, 4, 6) + base offset
        if (randomInt(0, 255) >= 0xd0) {  // ~19% chance of note off
            result.noteSequence[i] = 0.0f;
            result.gateSequence[i] = 0.0f; // Note off
        } else {
            int note = (pos * 2) + base;
            result.noteSequence[i] = note / 11.0f;  // 0-8 range converted to 0-1
            result.gateSequence[i] = 0.5f + 0.25f * (randomInt(0, 2)); // 50%, 75%, or 100%
        }
        
        // Chiparp: Apply chiptune-style timing variations
        if (pos == 0) {
            result.gateOffsetSequence[i] = 0.0f;    // Root note on-beat
        } else if (pos == 1) {
            result.gateOffsetSequence[i] = 0.1f;   // Second note slightly delayed
        } else if (pos == 2) {
            result.gateOffsetSequence[i] = 0.3f;   // Third note with more delay
        } else {  // pos == 3
            result.gateOffsetSequence[i] = 0.5f;   // Fourth note with even more delay for arpeggio feel
        }
        
        // Slide based on glide parameter
        if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = randomInt(1, 3) * 0.25f;
        }
    }
    
    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);
    
    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateDroneAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);
    
    int baseNote = (params.flow - 1) % 12;
    int interval = (params.ornament - 1) % 4;  // 0=unison, 1=5th, 2=octave, 3=5th+octave
    
    // Convert interval to semitone offset
    int semitoneOffset = 0;
    switch (interval) {
        case 0: semitoneOffset = 0; break;    // Unison
        case 1: semitoneOffset = 7; break;    // Perfect 5th
        case 2: semitoneOffset = 12; break;   // Octave
        case 3: semitoneOffset = 19; break;   // 5th + octave
    }
    
    int droneNote = (baseNote + semitoneOffset) % 12;
    int droneOctave = (baseNote + semitoneOffset) / 12;
    
    // Slow change rate based on power parameter
    int droneRate = (params.power > 0) ? (4 * params.power) : 4;
    
    for (int i = 0; i < m_sequenceLength; i++) {
        // Slow movement based on droneRate
        if ((i % droneRate) == 0) {
            // Occasional variation
            if (randomInt(0, 3) == 0) {
                semitoneOffset += (randomInt(0, 1) ? 2 : -2);
            }
        }
        
        droneNote = (baseNote + semitoneOffset) % 12;
        droneOctave = (baseNote + semitoneOffset) / 12;
        
        result.noteSequence[i] = (droneNote / 11.0f);  // Normalize to 0-1
        result.gateSequence[i] = 1.0f;  // Very long sustain (normalized to 1.0)
        
        // Long slide for drones if glide is active
        if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = 0.75f;  // Long slide for drones
        }
        
        // Drone: Apply sustained timing variations matching the slow drone nature
        int timingFactor = (i % droneRate);
        if (timingFactor < droneRate / 2) {
            result.gateOffsetSequence[i] = 0.0f;      // Stable timing during first half of drone change
        } else {
            result.gateOffsetSequence[i] = 0.15f;     // Slight delay during second half of drone change
        }
    }
    
    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);
    
    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateCustomAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);
    
    // Use custom parameters to generate user-defined algorithm
    float param1 = params.customParam1;
    float param2 = params.customParam2;
    float param3 = params.customParam3;
    float param4 = params.customParam4;
    
    for (int i = 0; i < m_sequenceLength; i++) {
        // Example of a custom algorithm that combines parameters
        float wave = sinf(i * param1 * 2.0f * M_PI / m_sequenceLength);
        float noise = param2 * (randomFloat(0.0f, 1.0f) - 0.5f);
        float step = fmod(i * param3, 1.0f);
        
        // Combine parameters to create note sequence
        result.noteSequence[i] = 0.5f + 0.5f * wave + noise * 0.1f + step * 0.2f;
        result.noteSequence[i] = std::max(0.0f, std::min(1.0f, result.noteSequence[i])); // Clamp to 0-1
        
        // Gate sequence based on param4
        result.gateSequence[i] = (sin(i * param4 * 2.0f * M_PI / m_sequenceLength) + 1.0f) / 2.0f;
        
        // Use custom param1 for slide probability
        if (randomFloat(0.0f, 1.0f) < param1 * 0.1f) {
            result.slideSequence[i] = 0.5f;
        }
        
        // Timing offset based on param2
        result.gateOffsetSequence[i] = param2 * 0.5f;
    }
    
    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);
    
    return result;
}

// The other algorithm implementations needed for linker
AlgorithmProcessor::SignalData AlgorithmProcessor::generateSnhAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate Sample & Hold random walk algorithm
    int seed = params.flow * 37 + params.ornament * 101;
    std::srand(seed);

    float currentValue = 0.5f;  // Start in middle
    for (int i = 0; i < m_sequenceLength; i++) {
        // Random walk
        float change = (static_cast<float>(std::rand()) / RAND_MAX - 0.5f) * 0.2f;
        currentValue = std::max(0.0f, std::min(1.0f, currentValue + change));

        result.noteSequence[i] = currentValue;
        result.gateSequence[i] = 0.75f;  // 75% gate length
        result.gateOffsetSequence[i] = (static_cast<float>(std::rand()) / RAND_MAX) * 0.1f;  // Random timing offset
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateWobbleAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate dual-phase LFO bass
    for (int i = 0; i < m_sequenceLength; i++) {
        // Two different phase oscillators
        float phase1 = fmod((i * 0.5f), 1.0f);
        float phase2 = fmod((i * 0.7f), 1.0f);

        // Use alternating phases
        float note;
        if (i % 4 < 2) {  // High phase
            note = phase1;
            if (i > 0 && (i-1) % 4 >= 2) {  // Just transitioned from low to high
                if (randomInt(0, 100) < params.glide) {
                    result.slideSequence[i] = 0.5f;  // Slide from low to high
                }
            }
        } else {  // Low phase
            note = phase2 * 0.5f;  // Lower values
            if (i > 0 && (i-1) % 4 < 2) {  // Just transitioned from high to low
                if (randomInt(0, 100) < params.glide) {
                    result.slideSequence[i] = 0.5f;  // Slide from high to low
                }
            }
        }

        result.noteSequence[i] = note;
        result.gateSequence[i] = 0.75f;

        // Wobble: Apply timing variations based on dual phase system
        uint32_t phaseSum = (static_cast<uint32_t>(phase1 * 1000) + static_cast<uint32_t>(phase2 * 1000)) % 100;
        result.gateOffsetSequence[i] = phaseSum / 100.0f;
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateTechnoAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate four-on-floor club patterns
    int kickPattern = params.flow % 4;
    int hatPattern = params.ornament % 4;
    int bassNote = params.flow % 5;  // Bass note 0-4

    for (int i = 0; i < m_sequenceLength; i++) {
        int beatPos = i % 4;  // Position within beat
        int barPos = i % 16;  // Position within bar

        // Four-on-floor kick pattern variations
        bool isKick = false;
        switch (kickPattern) {
        case 0: isKick = (beatPos == 0); break;  // Basic 4/4
        case 1: isKick = (beatPos == 0) || (barPos == 14); break;  // With pickup
        case 2: isKick = (beatPos == 0) || (barPos == 6); break;  // With offbeat
        case 3: isKick = (beatPos == 0) || (barPos == 3) || (barPos == 11); break;  // Syncopated
        }

        if (isKick) {
            result.noteSequence[i] = bassNote / 4.0f;  // Normalize bass note
            result.gateSequence[i] = 0.80f;  // 80% for kicks
            result.gateOffsetSequence[i] = 0.0f;  // Kick on-beat

            // Check for trill on kicks
            if (randomInt(0, 100) < 25) {  // 25% chance
                result.isTrillSequence[i] = 1.0f;
            }
        } else {
            // Hi-hat patterns on off-beats
            bool isHat = false;
            switch (hatPattern) {
            case 0: isHat = (beatPos == 2); break;  // Off-beat hats
            case 1: isHat = (beatPos == 1) || (beatPos == 3); break;  // 8th notes
            case 2: isHat = true; break;  // 16th notes
            case 3: isHat = (beatPos != 0) && (randomInt(0, 2) != 0); break;  // Random 16ths
            }
            if (isHat) {
                result.noteSequence[i] = (7 + randomInt(0, 2)) / 11.0f;  // Higher notes for hats
                result.gateSequence[i] = 0.40f;  // 40% for hats
                result.gateOffsetSequence[i] = 0.15f + (beatPos * 0.05f);  // Hi-hats with rhythmic timing
            } else {
                result.noteSequence[i] = 0.0f;
                result.gateSequence[i] = 0.0f;  // No gate
            }
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateFunkAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate syncopated funk grooves with ghost notes
    int funkPattern = params.flow % 8;
    int syncopation = params.ornament % 4;

    // Define 8 different funk patterns
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

    for (int i = 0; i < m_sequenceLength; i++) {
        int pos = i % 16;
        bool isNote = (funkPatterns[funkPattern] >> (15 - pos)) & 1;

        if (isNote) {
            // Note selection based on position
            int noteChoice = randomInt(0, 7);
            switch (syncopation) {
            case 0:
                result.noteSequence[i] = (noteChoice % 5) / 4.0f;
                break;  // Pentatonic-ish
            case 1:
                result.noteSequence[i] = ((noteChoice % 3) * 2) / 6.0f;
                break;  // Root/3rd/5th
            case 2:
                result.noteSequence[i] = noteChoice / 7.0f;
                break;  // Full range
            case 3:
                result.noteSequence[i] = (pos % 4 == 0) ? 0.0f : ((noteChoice % 5) + 2) / 11.0f;
                break;  // Root on beat
            }

            // Funk: Generate syncopated timing offsets to complement the rhythm
            if (pos % 4 == 0) {
                // On beats (1, 2, 3, 4) - often on-time
                result.gateOffsetSequence[i] = 0.40f + (randomFloat(0.0f, 0.2f));  // 40-60% timing
            } else if (pos % 2 == 0) {
                // On upbeats (e.g., "&" of 2, 4) - maybe slightly early or late
                result.gateOffsetSequence[i] = 0.20f + (randomFloat(0.0f, 0.2f));  // 20-40% timing
            } else {
                // On off-beats (e.g., "&" of 1, 3, or 16th notes) - syncopated feel
                result.gateOffsetSequence[i] = 0.50f + (randomFloat(0.0f, 0.3f));  // 50-80% timing
            }

            // Ghost notes (quieter)
            if (randomInt(0, 255) < 100 && pos % 4 != 0) {  // ~39% chance, not on beats
                result.gateSequence[i] = 0.35f;  // Ghost note

                // Check for trill on ghost notes
                if (randomInt(0, 100) < 15) {  // 15% chance
                    result.isTrillSequence[i] = 1.0f;
                }
            } else {
                result.gateSequence[i] = 0.75f;  // Regular gate
            }
        } else {
            result.noteSequence[i] = 0.0f;
            result.gateSequence[i] = 0.0f;
            result.gateOffsetSequence[i] = 0.0f;  // No gate, no offset
        }

        // Slide based on glide parameter
        if (result.gateSequence[i] > 0.0f && randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = (randomInt(0, 2) + 1) * 0.25f;
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateGoaacidAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate Goa/psytrance acid patterns with systematic transposition
    for (int i = 0; i < m_sequenceLength; i++) {
        // Generate notes from a lookup table of 8 values: {0, -12, 1, 3, 7, 12, 13} with modifications based on accent
        int lookup[8] = {0, -12, 1, 3, 7, 12, 13, 0};  // 0 at end to fill
        int index = randomInt(0, 6);  // Use 0-6 index
        int note = lookup[index];

        // Apply systematic transposition: +3 semitones if goaB1 is true during first 8 steps of 16-step cycle, -5 semitones if goaB2 is true during first 8 steps
        bool goaB1 = randomInt(0, 1) == 1;  // Simulate
        bool goaB2 = randomInt(0, 1) == 1;  // Simulate

        int stepInCycle = i % 16;
        if (stepInCycle < 8 && goaB1) {
            note += 3;
        }
        if (stepInCycle < 8 && goaB2) {
            note -= 5;
        }

        // Add a default +24 semitones (2 octaves) to all notes
        note += 24;

        // Convert to normalized note value (0.0 to 1.0)
        result.noteSequence[i] = (std::max(0, note)) / 127.0f;  // Cap at 127
        result.gateSequence[i] = 0.75f;  // Fixed 75%

        // Goaacid: Apply Goa/psytrance-style timing variations
        // Create rhythmic shifts that complement the acid patterns
        int stepInBar = i % 16;
        if (stepInBar == 0 || stepInBar == 8) {
            result.gateOffsetSequence[i] = 0.0f;    // On strong beats (1st and 3rd beat), on-time
        } else if (stepInBar % 4 == 0) {
            result.gateOffsetSequence[i] = 0.10f;   // On weak beats (2nd and 4th beat), slight delay
        } else {
            result.gateOffsetSequence[i] = 0.25f + (stepInBar % 5) * 0.02f;  // Off-beat patterns with varying delays
        }

        // Slide based on glide parameter
        if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = (randomInt(0, 2) + 1) * 0.25f;
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateAcidAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Generate 8-step acid sequence
    std::vector<int> acidSequence(8);
    for (int i = 0; i < 8; i++) {
        acidSequence[i] = randomInt(0, 11);  // 0-11 range for notes
    }

    for (int i = 0; i < m_sequenceLength; i++) {
        int position = i % 8;
        result.noteSequence[i] = acidSequence[position] / 11.0f;  // Normalize to 0-1

        // Check for accent
        bool hasAccent = (randomInt(0, 1) == 1);  // 50% chance
        result.gateSequence[i] = hasAccent ? 0.95f : 0.65f;  // Punchy 303 gates

        // Slide based on flow (in original, but using params.glide here)
        if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = 0.5f;  // 303-style slide
        }

        // Acid: Apply acid-style timing variations that complement the 303 patterns
        if (hasAccent) {
            result.gateOffsetSequence[i] = 0.10f;  // Accented notes slightly delayed

            // Check for Trill on accented notes
            if (randomInt(0, 100) < 20) {  // 20% chance
                result.isTrillSequence[i] = 1.0f;
            }
        } else if (position % 2 == 0) {
            result.gateOffsetSequence[i] = 0.0f;   // Even steps on-beat
        } else {
            result.gateOffsetSequence[i] = 0.20f;  // Odd steps with slight delay
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateDrillAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate UK Drill hi-hat rolls and bass slides
    uint16_t hiHatPattern = 0b10101010;  // Basic pattern
    int slideTarget = randomInt(0, 11);
    bool tripletMode = (params.ornament > 8);  // High ornament = triplets
    int lastNote = randomInt(0, 4);  // Low bass notes

    for (int i = 0; i < m_sequenceLength; i++) {
        int stepInBar = i % 8;  // Using 8 step bar like original

        // Check hi-hat pattern
        bool hihatHit = (hiHatPattern & (1 << stepInBar)) != 0;

        if (hihatHit) {
            // HI-HAT HIT
            result.noteSequence[i] = (7 + randomInt(0, 4)) / 11.0f;  // Higher notes
            result.gateSequence[i] = 0.25f;  // Short gate
            result.slideSequence[i] = 0.0f;

            // Check for Trill
            if (randomInt(0, 100) < 30) {  // 30% base chance
                result.isTrillSequence[i] = 1.0f;
                result.gateSequence[i] = 0.10f;  // Shorter gate for trill
            }
        } else {
            // BASS NOTE
            result.noteSequence[i] = lastNote / 4.0f;  // Normalize to 0-1

            if (randomInt(0, 7) < 2) {  // 2 in 8 chance
                lastNote = randomInt(0, 4);
            }

            result.gateSequence[i] = 0.75f;  // Standard gate

            if (randomInt(0, 15) < 8) {  // 50% chance
                result.slideSequence[i] = 0.5f;  // Slide to target
                slideTarget = randomInt(0, 11);
            } else if (randomInt(0, 99) < params.glide) {
                result.slideSequence[i] = (randomInt(0, 2) + 1) * 0.25f;
            } else {
                result.slideSequence[i] = 0.0f;
            }
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateMinimalAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate staccato bursts and silence
    int burstLength = 2 + (params.flow % 7);  // 2-8 steps
    int silenceLength = 4 + (params.flow % 13);  // 4-16 steps
    int clickDensity = params.ornament * 16;  // 0-255 scale

    int burstTimer = 0;
    int silenceTimer = silenceLength;  // Start in silence
    int noteIndex = 0;
    int mode = 0;  // 0=silence, 1=burst

    for (int i = 0; i < m_sequenceLength; i++) {
        if (mode == 0) {  // Silence mode
            if (silenceTimer > 0) {
                silenceTimer--;
                result.noteSequence[i] = 0.0f;
                result.gateSequence[i] = 0.0f;
            } else {
                // Switch to burst mode
                mode = 1;
                burstTimer = burstLength;
                noteIndex = 0;

                // Generate first note of burst
                int baseNote = randomInt(0, 11);

                // Glitch repeats based on ornament
                if (randomInt(0, 255) < clickDensity) {
                    // Glitch - repeat previous note or create click
                    result.noteSequence[i] = baseNote / 11.0f;
                    result.gateSequence[i] = 0.15f;  // Very short click
                } else {
                    result.noteSequence[i] = baseNote / 11.0f;
                    result.gateSequence[i] = 0.25f;  // Normal staccato
                }
            }
        } else {  // Burst mode
            if (burstTimer > 0) {
                burstTimer--;
                noteIndex++;

                // Generate note based on pattern
                int baseNote = randomInt(0, 11);

                // Glitch repeats based on ornament
                if (randomInt(0, 255) < clickDensity) {
                    // Glitch - repeat previous note or create click
                    result.noteSequence[i] = baseNote / 11.0f;
                    result.gateSequence[i] = 0.15f;  // Very short click
                } else {
                    result.noteSequence[i] = baseNote / 11.0f;
                    result.gateSequence[i] = 0.25f;  // Normal staccato
                }
            } else {
                // Switch to silence mode
                mode = 0;
                silenceTimer = silenceLength;
                result.noteSequence[i] = 0.0f;
                result.gateSequence[i] = 0.0f;
            }
        }

        // Glide check
        if (result.gateSequence[i] > 0.0f && randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = (randomInt(0, 2) + 1) * 0.25f;
        }

        // MINIMAL: Apply timing variations that complement the burst/silence pattern
        if (mode == 0) {
            result.gateOffsetSequence[i] = 0.0f;   // Silence mode - no gate offset
        } else {
            // Burst mode - vary timing based on burst progression
            result.gateOffsetSequence[i] = (burstTimer % 4) * 0.1f;  // Cycle through timing variations during burst
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateKraftAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate precise mechanical sequences
    int baseNote = params.flow % 12;
    std::vector<int> sequence(8);
    for (int i = 0; i < 8; i++) {
        // Kraftwerk patterns often alternate between 2-3 notes
        sequence[i] = (baseNote + ((i % 2) ? 7 : 0)) % 12;  // Root and 5th
    }

    int position = 0;
    int lockTimer = 16 + (params.flow % 16);  // Lock for 16-31 steps
    int transpose = 0;
    int ghostMask = randomInt(0, 255) & 0x55;  // Every other step ghost pattern

    for (int i = 0; i < m_sequenceLength; i++) {
        // Get note from sequence with transpose
        int note = (sequence[position] + transpose) % 12;
        result.noteSequence[i] = note / 11.0f;  // Normalize

        // Check for ghost note
        bool isGhost = ((ghostMask & (1 << position)) != 0);
        result.gateSequence[i] = isGhost ? 0.25f : 0.50f;  // Precise, mechanical gates

        // Lock timer controls pattern stability
        if (lockTimer > 0) {
            lockTimer--;
        } else {
            // Pattern evolution when lock expires
            lockTimer = 16 + (randomInt(0, 15));
            baseNote = (baseNote + randomInt(0, 4)) % 12;
            // Regenerate pattern
            for (int j = 0; j < 8; j++) {
                sequence[j] = (baseNote + ((j % 2) ? 7 : 0)) % 12;
            }
        }

        // Transpose based on flow occasionally
        if (randomInt(0, 15) < 4) {  // ~25% chance
            transpose = randomInt(0, 11);
        }

        // Advance position
        position = (position + 1) % 8;

        // Glide check (rare for mechanical feel)
        if (randomInt(0, 99) < params.glide / 2) {
            result.slideSequence[i] = 0.25f;  // Short slide
        }

        // KRAFT: Apply precise mechanical timing variations
        // Complement the precise, algorithmic nature of the sequence
        if (isGhost) {
            result.gateOffsetSequence[i] = 0.20f + (position * 0.05f);  // Ghost notes with specific timing
        } else {
            result.gateOffsetSequence[i] = (position % 3) * 0.10f;  // Pattern-based timing variations
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateAphexAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate Polyrhythmic Event Sequencer
    std::vector<int> track1_pattern(4);
    std::vector<int> track2_pattern(3);
    std::vector<int> track3_pattern(5);

    // Seed patterns based on params.flow
    int seed = params.flow;
    for (int i = 0; i < 4; i++) {
        track1_pattern[i] = (seed * (i + 1)) % 12;
        seed = (seed * 17 + 13) % 100;  // Simple PRNG
    }
    for (int i = 0; i < 3; i++) {
        track2_pattern[i] = (seed % 3);  // 0, 1, or 2
        seed = (seed * 17 + 13) % 100;
    }
    for (int i = 0; i < 5; i++) {
        track3_pattern[i] = (seed % 8 == 0) ? (seed % 5) : 0;  // Sparse bass notes
        seed = (seed * 17 + 13) % 100;
    }

    int pos1 = 0, pos2 = 0, pos3 = 0;

    for (int i = 0; i < m_sequenceLength; i++) {
        // MAIN MELODY from Track 1
        int note = track1_pattern[pos1];
        int octave = 0;
        float gatePct = 0.75f;
        float slideAmt = 0.0f;

        // MODIFICATION from Track 2
        int modifier = track2_pattern[pos2];
        if (modifier == 1) { // Stutter
            gatePct = 0.20f;
        } else if (modifier == 2) { // Slide
            slideAmt = 0.25f;
        }

        // OVERRIDE from Track 3
        int bass_note = track3_pattern[pos3];
        if (bass_note > 0) {
            note = bass_note;
            octave = -1; // Deep bass
            gatePct = 0.90f;
            slideAmt = 0.0f;
        }

        result.noteSequence[i] = (note + octave * 12) / 23.0f;  // Normalize with octave
        result.gateSequence[i] = gatePct;
        result.slideSequence[i] = slideAmt;

        // ADVANCE all tracks
        pos1 = (pos1 + 1) % 4;
        pos2 = (pos2 + 1) % 3;
        pos3 = (pos3 + 1) % 5;

        // APHEX: Apply polyrhythmic timing variations
        // Use multi-track positioning to create complex timing variations
        int polyRhythmFactor = (pos1 + pos2 + pos3) % 12;
        result.gateOffsetSequence[i] = polyRhythmFactor * 0.08f;  // Scale to 0-96% range
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateAutechreAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate Algorithmic Transformation Engine
    std::vector<int8_t> pattern(8);
    // Initialize with octave-jumping pattern: 5 roots, 2 at +2oct, 1 at +3oct
    pattern[0] = 0;   // root
    pattern[1] = 0;   // root
    pattern[2] = 24;  // +2 octaves
    pattern[3] = 0;   // root
    pattern[4] = 0;   // root
    pattern[5] = 24;  // +2 octaves
    pattern[6] = 0;   // root
    pattern[7] = 36;  // +3 octaves

    // Rule timer based on flow
    int ruleTimer = 8 + (params.flow * 4);

    // Create a sequence of rules based on params.ornament
    std::vector<int> ruleSequence(8);
    int seed = params.ornament;
    for (int i = 0; i < 8; i++) {
        ruleSequence[i] = seed % 5;  // 5 different rules
        seed = (seed * 17 + 13) % 100;
    }
    int ruleIndex = 0;

    for (int i = 0; i < m_sequenceLength; i++) {
        // Always play the current state of the pattern
        int8_t patternVal = pattern[i % 8];
        int note = patternVal % 12;
        int octave = patternVal / 12;

        result.noteSequence[i] = (note + octave * 12) / 47.0f;  // Normalize with octave
        result.gateSequence[i] = 0.75f;  // Standard gate

        // COUNTDOWN to the next transformation
        ruleTimer--;
        if (ruleTimer <= 0) {
            int current_rule = ruleSequence[ruleIndex];
            int intensity = params.power / 2; // Power: 0-8

            // Apply a deterministic transformation rule
            switch (current_rule) {
                case 0: // ROTATE
                    {
                        int8_t temp = pattern[7];
                        for (int j = 7; j > 0; j--) pattern[j] = pattern[j-1];
                        pattern[0] = temp;
                    }
                    break;
                case 1: // REVERSE
                    for (int j = 0; j < 4; j++) {
                        int8_t temp = pattern[j];
                        pattern[j] = pattern[7-j];
                        pattern[7-j] = temp;
                    }
                    break;
                case 2: // INVERT around a pivot - preserve octave
                    for (int j = 0; j < 8; j++) {
                        int oct = pattern[j] / 12;
                        int note = pattern[j] % 12;
                        int interval = note - 6;
                        note = (6 - interval + 12) % 12;
                        pattern[j] = note + (oct * 12);
                    }
                    break;
                case 3: // SWAP adjacent notes
                    for (int j = 0; j < 8; j += 2) {
                        int8_t temp = pattern[j];
                        pattern[j] = pattern[j+1];
                        pattern[j+1] = temp;
                    }
                    break;
                case 4: // ADD intensity - preserve octave
                    for (int j = 0; j < 8; j++) {
                        int oct = pattern[j] / 12;
                        int note = (pattern[j] % 12 + intensity) % 12;
                        pattern[j] = note + (oct * 12);
                    }
                    break;
            }

            // Reset for next rule
            ruleTimer = 8 + (params.flow * 4);
            ruleIndex = (ruleIndex + 1) % 8;
        }

        // AUTECHRE: Apply algorithmic timing variations that complement the pattern transformations
        // Use the current rule index and timer to create evolving timing variations
        result.gateOffsetSequence[i] = ((ruleIndex * 10) + (ruleTimer % 7)) / 100.0f;
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateStepwaveAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate Scale stepping with chromatic trill
    // Flow controls scale step direction with octave jumps
    // 0-7: step down, 8: stationary, 9-16: step up

    int scaleStepDir = 0;
    if (params.flow <= 7) {
        scaleStepDir = -1;  // Step down
    } else if (params.flow >= 9) {
        scaleStepDir = 1;   // Step up
    }
    // params.flow == 8 means stationary (scaleStepDir = 0)

    // Determine step size from ornament: 0-5=2 steps, 6-10=2-3 mixed, 11-16=3 steps
    int stepSize;
    if (params.ornament <= 5) {
        stepSize = 2;
    } else if (params.ornament >= 11) {
        stepSize = 3;
    } else {
        stepSize = 2 + (randomInt(0, 1));  // 2 or 3
    }

    int currentNote = 0;  // Start at 0

    for (int i = 0; i < m_sequenceLength; i++) {
        // Calculate note as scale degree offset from previous
        currentNote = (currentNote + scaleStepDir * stepSize) % 7;  // Keep within single octave range for scale
        if (currentNote < 0) currentNote += 7;

        // Octave jumps: probabilistically jump up 2 octaves
        // Higher flow = more upward movement = more high octave jumps
        int octaveJumpChance = 20 + (params.flow * 3);  // 20-68%
        int octave = 0;
        if (randomInt(0, 99) < octaveJumpChance) {
            octave = 2;
        } else {
            octave = 0;
        }

        result.noteSequence[i] = (currentNote + octave * 12) / 19.0f;  // Normalize with octave
        result.gateSequence[i] = 0.85f;  // Slightly longer gates for the stepping effect

        // Check for trill (base probability 50%)
        if (randomInt(0, 100) < 50) {  // 50% base chance
            result.isTrillSequence[i] = 1.0f;

            // Determine if this is stepped or slide (glide parameter)
            if (randomInt(0, 99) < params.glide) {
                result.slideSequence[i] = 0.5f;  // Long slide for smooth glissando
            }
        }

        // Gate offset: create rhythmic interest
        result.gateOffsetSequence[i] = (i % 4) * 0.15f;  // 0, 0.15, 0.30, 0.45
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateRagaAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate Indian classical melody patterns with traditional scales
    int scaleType = params.flow % 4;
    std::vector<int> scale(7);

    // Define Indian pentatonic-ish scales (sa re ga ma pa dha ni)
    switch (scaleType) {
    case 0: // Bhairav-like (morning raga)
        scale[0] = 0; scale[1] = 1; scale[2] = 4; scale[3] = 5; scale[4] = 7; scale[5] = 8; scale[6] = 11;
        break;
    case 1: // Yaman-like (evening raga)
        scale[0] = 0; scale[1] = 2; scale[2] = 4; scale[3] = 6; scale[4] = 7; scale[5] = 9; scale[6] = 11;
        break;
    case 2: // Todi-like
        scale[0] = 0; scale[1] = 1; scale[2] = 3; scale[3] = 6; scale[4] = 7; scale[5] = 8; scale[6] = 11;
        break;
    default: // Kafi-like (Dorian)
        scale[0] = 0; scale[1] = 2; scale[2] = 3; scale[3] = 5; scale[4] = 7; scale[5] = 9; scale[6] = 10;
        break;
    }

    int direction = 0;  // 0=ascending, 1=descending
    int position = 0;
    int ornament = params.ornament % 3;  // Ornament type

    for (int i = 0; i < m_sequenceLength; i++) {
        // Move through scale in characteristic ways
        int movement = randomInt(0, 7);
        switch (movement) {
        case 0: case 1: case 2:
            // Continue in current direction
            if (direction == 0) {
                position = (position + 1) % 7;
                if (position == 6) direction = 1;  // Change to descending at top
            } else {
                position = (position + 6) % 7;  // -1 mod 7
                if (position == 0) direction = 0;  // Change to ascending at bottom
            }
            break;
        case 3: case 4:
            // Skip a note
            if (direction == 0) {
                position = (position + 2) % 7;
            } else {
                position = (position + 5) % 7;  // -2 mod 7
            }
            break;
        case 5:
            // Repeat
            break;
        case 6:
            // Jump to root or 5th
            position = (randomInt(0, 1) == 0) ? 0 : 4;  // Root or Pa
            break;
        case 7:
            // Change direction
            direction = 1 - direction;
            break;
        }

        result.noteSequence[i] = scale[position] / 11.0f;  // Normalize
        result.gateSequence[i] = 0.75f;  // Standard gate

        // Ornaments (gamaka-like slides)
        int ornamentChance = randomInt(0, 7);
        if (ornamentChance < ornament && params.glide > 0) {
            result.slideSequence[i] = 0.5f;  // Characteristic slides
        } else if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = (randomInt(0, 2) + 1) * 0.25f;
        }

        // Check for Trill on ascending movements
        if (direction == 0) {  // Ascending
            if (randomInt(0, 100) < 25) {  // 25% chance
                result.isTrillSequence[i] = 1.0f;
            }
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generatePhaseAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate minimalist phasing patterns with gradual shifts
    uint32_t accum = 0;
    uint32_t speed = 0x1000000 + (params.ornament * 0x100000);  // Slow phase drift based on ornament
    int length = 3 + (params.flow % 6);  // Pattern length 3-8
    std::vector<int> pattern(8);
    for (int i = 0; i < 8; i++) {
        pattern[i] = randomInt(0, 7);  // Simple melodic cell
    }

    for (int i = 0; i < m_sequenceLength; i++) {
        accum += speed;

        // Get pattern position with phase offset
        int phaseLen = (length > 0) ? length : 4;
        int patternPos = (i + (accum >> 28)) % phaseLen;
        if (patternPos < 8) {
            result.noteSequence[i] = pattern[patternPos] / 7.0f;  // Normalize
        } else {
            result.noteSequence[i] = randomFloat(0.0f, 1.0f);  // Fallback
        }
        result.gateSequence[i] = 0.75f;  // Standard gate

        // PHASE: Generate timing variations based on phase accumulator
        // Use fractional part of accumulator to create evolving rhythmic patterns
        uint32_t fractionalPhase = accum & 0x0FFFFFFF;  // Get fractional part
        result.gateOffsetSequence[i] = (fractionalPhase >> 24) / 255.0f;  // Extract upper bits as timing offset

        // Consume RNG for determinism like original
        randomInt(0, 100);

        // Slide based on params.glide
        if (randomInt(0, 99) < params.glide) {
            result.slideSequence[i] = (randomInt(0, 2) + 1) * 0.25f;
        }
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}

AlgorithmProcessor::SignalData AlgorithmProcessor::generateAmbientAlgorithm(const AlgorithmParameters& params) {
    SignalData result;
    result.noteSequence.resize(m_sequenceLength, 0.0f);
    result.gateSequence.resize(m_sequenceLength, 0.0f);
    result.velocitySequence.resize(m_sequenceLength, 0.0f);
    result.slideSequence.resize(m_sequenceLength, 0.0f);
    result.accentSequence.resize(m_sequenceLength, 0.0f);
    result.probabilitySequence.resize(m_sequenceLength, 0.0f);
    result.gateOffsetSequence.resize(m_sequenceLength, 0.0f);
    result.isTrillSequence.resize(m_sequenceLength, 0.0f);

    // Simulate harmonic drone & event scheduler
    int root_note = (params.flow - 1) % 12;
    std::vector<int> drone_notes = {root_note, (root_note + 7) % 12, (root_note + 16) % 12}; // Root, 5th, 9th

    int event_timer = 16 + (params.ornament * 4);  // Event timer based on ornament
    int event_type = 0;  // 0=no event, 1=single note, 2=arp
    int event_step = 0;

    for (int i = 0; i < m_sequenceLength; i++) {
        int note, octave;
        float gatePct = 0.75f;

        // Handle ongoing events first
        if (event_type == 1) { // Single Note Event
            note = (root_note + 12) % 12; // Octave up
            octave = 1;
            gatePct = 1.5f;  // Long gate
            event_type = 0; // Event is one step long
        } else if (event_type == 2) { // Arpeggio Event
            note = drone_notes[event_step];
            octave = 0;
            gatePct = 0.5f;
            event_step++;
            if (event_step >= 3) {
                event_type = 0; // End of event
            }
        } else {
            // Default Drone Behavior
            note = drone_notes[(i / 4) % 3]; // Slowly cycle through drone notes
            octave = 0;
            gatePct = 2.55f; // Hold gate for long time (very long, will be clamped)
            event_timer--;
        }

        // Check if it's time for a new event
        if (event_timer <= 0) {
            event_type = 1 + (randomInt(0, 1)); // 1 or 2
            event_step = 0;
            // Reset timer based on Power parameter
            int power = params.power;
            event_timer = 16 + (power > 0 ? 256 / power : 256);

            // AMBIENT: Generate random timing variations for organic feel
            result.gateOffsetSequence[i] = randomFloat(0.0f, 0.5f);  // Random timing (0-50%) for organic feel
        }

        result.noteSequence[i] = (note + octave * 12) / 23.0f;  // Normalize with octave
        result.gateSequence[i] = std::min(gatePct, 1.0f);  // Clamp long gates to 1.0
    }

    // Calculate spectrum
    computeSpectrumPair(result.noteSequence, result.spectrum, result.spectrum_oversampled);

    return result;
}