#pragma once

#include <SDL2/SDL.h>
#include <vector>

// Audio engine for real-time sonification of algorithm outputs
class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    
    void init();
    void cleanup();
    
    // Process audio callback
    void process(float* stream, int len, int sampleRate);
    
    // Control parameters
    bool m_active;
    float m_volume;
    float m_modAmount;
    int m_sampleRate;
    
    // Get/set current algorithm output value (for modulation)
    float getCurrentOutput() const { return m_currentOutput; }
    void setCurrentOutput(float output) { m_currentOutput = output; }
    
private:
    SDL_AudioDeviceID m_audioDeviceId;
    
    // Audio generation state
    float m_phase;
    float m_currentOutput;
    float m_lastOutput;
    
    // Waveform generation
    float generateSine(float frequency, float amplitude);
    float generateAlgorithmicWave(float input);
};