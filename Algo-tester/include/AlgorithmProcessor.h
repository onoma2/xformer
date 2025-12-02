#pragma once

#include <vector>
#include <string>
#include <functional>
#include <cstdint>

// Algorithm types for the PEW|FORMER system (TuesdayTrack algorithms)
enum class AlgorithmType {
    Test = 0,      // Algorithm #0 - Test pattern
    Tritrance,     // Algorithm #1 - German minimal style arpeggios
    Stomper,       // Algorithm #2 - Acid bass patterns with slides
    Markov,        // Algorithm #3 - Markov chain melody generation
    Chiparp,       // Algorithm #4 - Chiptune arpeggio patterns
    Goaacid,       // Algorithm #5 - Goa/psytrance acid patterns
    Snh,           // Algorithm #6 - Sample & Hold random walk
    Wobble,        // Algorithm #7 - Dual-phase LFO bass
    Techno,        // Algorithm #8 - Four-on-floor club patterns
    Funk,          // Algorithm #9 - Syncopated funk grooves
    Drone,         // Algorithm #10 - Sustained drone textures
    Phase,         // Algorithm #11 - Minimalist phasing patterns
    Raga,          // Algorithm #12 - Indian classical melody patterns
    Ambient,       // Algorithm #13 - Harmonic drone & event scheduler
    Acid,          // Algorithm #14 - 303-style patterns with slides
    Drill,         // Algorithm #15 - UK Drill hi-hat rolls and bass slides
    Minimal,       // Algorithm #16 - Staccato bursts and silence
    Kraft,         // Algorithm #17 - Precise mechanical sequences
    Aphex,         // Algorithm #18 - Polyrhythmic Event Sequencer
    Autechre,      // Algorithm #19 - Algorithmic Transformation Engine
    Stepwave,      // Algorithm #20 - Scale stepping with chromatic trill
    Custom,        // For user-defined algorithms
    Last
};

// Algorithm parameters structure
struct AlgorithmParameters {
    AlgorithmType type = AlgorithmType::Test;
    int flow = 8;              // Parameter that affects algorithm flow (1-16)
    int ornament = 8;          // Parameter that adds variations (1-16)
    int power = 8;             // Controls playback speed (1-16, 0 = silent)
    int glide = 0;             // Probability of slides between notes (0-16)
    int trill = 0;             // Probability of trills (0-8)
    int baseNote = 60;         // Base MIDI note (0-127)
    int octaveRange = 2;       // Range of octaves to use
    
    // Pattern and sequence parameters
    int steps = 16;            // Number of steps in the pattern
    int loopLength = 0;        // 0 = infinite, >0 = finite loop length
    
    // For hardware constraints simulation
    float minNote = 0.0f;      // Minimum note value (0-1 normalized)
    float maxNote = 1.0f;      // Maximum note value (0-1 normalized)
    float minGate = 0.25f;     // Minimum gate length (percentage of step)
    float maxGate = 1.0f;      // Maximum gate length (percentage of step)
    
    // Custom algorithm parameters
    float customParam1 = 0.5f;
    float customParam2 = 0.5f;
    float customParam3 = 0.5f;
    float customParam4 = 0.5f;
};

// Algorithm processor class to generate and process algorithmic sequences
class AlgorithmProcessor {
public:
    struct SignalData {
        std::vector<float> noteSequence;      // Normalized note values (0.0 to 1.0)
        std::vector<float> gateSequence;      // Gate values (0.0 to 1.0)
        std::vector<float> velocitySequence;  // Velocity values (0.0 to 1.0)
        std::vector<float> slideSequence;     // Slide values (0.0 to 1.0)
        std::vector<float> accentSequence;    // Accent values (0.0 to 1.0)
        std::vector<float> probabilitySequence; // Probability values for each step (0.0 to 1.0)
        std::vector<float> spectrum;          // FFT spectrum of the note sequence
        std::vector<float> spectrum_oversampled; // Oversampled FFT for aliasing analysis
        std::vector<float> gateOffsetSequence; // Timing offset for each step (0.0 to 1.0)
        std::vector<float> isTrillSequence;   // Trill indicator for each step (0.0 to 1.0)
    };

    AlgorithmProcessor(int sequenceLength = 64);
    ~AlgorithmProcessor() = default;

    SignalData process(const AlgorithmParameters& params);
    
    // Get algorithm name by type
    static std::string getAlgorithmName(AlgorithmType type);
    
    // Get algorithm description by type
    static std::string getAlgorithmDescription(AlgorithmType type);

private:
    int m_sequenceLength;
    
    // Core algorithm functions
    SignalData generateTestAlgorithm(const AlgorithmParameters& params);
    SignalData generateTritranceAlgorithm(const AlgorithmParameters& params);
    SignalData generateStomperAlgorithm(const AlgorithmParameters& params);
    SignalData generateMarkovAlgorithm(const AlgorithmParameters& params);
    SignalData generateChiparpAlgorithm(const AlgorithmParameters& params);
    SignalData generateGoaacidAlgorithm(const AlgorithmParameters& params);
    SignalData generateSnhAlgorithm(const AlgorithmParameters& params);
    SignalData generateWobbleAlgorithm(const AlgorithmParameters& params);
    SignalData generateTechnoAlgorithm(const AlgorithmParameters& params);
    SignalData generateFunkAlgorithm(const AlgorithmParameters& params);
    SignalData generateDroneAlgorithm(const AlgorithmParameters& params);
    SignalData generatePhaseAlgorithm(const AlgorithmParameters& params);
    SignalData generateRagaAlgorithm(const AlgorithmParameters& params);
    SignalData generateAmbientAlgorithm(const AlgorithmParameters& params);
    SignalData generateAcidAlgorithm(const AlgorithmParameters& params);
    SignalData generateDrillAlgorithm(const AlgorithmParameters& params);
    SignalData generateMinimalAlgorithm(const AlgorithmParameters& params);
    SignalData generateKraftAlgorithm(const AlgorithmParameters& params);
    SignalData generateAphexAlgorithm(const AlgorithmParameters& params);
    SignalData generateAutechreAlgorithm(const AlgorithmParameters& params);
    SignalData generateStepwaveAlgorithm(const AlgorithmParameters& params);
    SignalData generateCustomAlgorithm(const AlgorithmParameters& params);
    
    // Helper functions
    std::vector<float> computeSpectrum(const std::vector<float>& signal, bool window = false);
    float randomFloat(float min = 0.0f, float max = 1.0f);
    int randomInt(int min, int max);
    std::vector<float> generateMarkovMatrix(int seed1, int seed2, int history1, int history3, int steps);
    
    // Helper for spectrum analysis
    void computeSpectrumPair(const std::vector<float>& signal, std::vector<float>& spectrum, std::vector<float>& oversampled_spectrum);
};