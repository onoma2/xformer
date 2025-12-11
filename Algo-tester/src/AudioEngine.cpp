#include "AudioEngine.h"
#include <cmath>

AudioEngine::AudioEngine() :
    m_active(false),
    m_volume(0.1f),
    m_modAmount(0.0f),
    m_sampleRate(44100),
    m_audioDeviceId(0),
    m_phase(0.0f),
    m_currentOutput(0.0f),
    m_lastOutput(0.0f)
{
}

AudioEngine::~AudioEngine() {
    cleanup();
}

void AudioEngine::init() {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = m_sampleRate;
    want.format = AUDIO_F32;
    want.channels = 1;
    want.samples = 1024;
    want.callback = [](void* userdata, Uint8* stream, int len) {
        static_cast<AudioEngine*>(userdata)->process(reinterpret_cast<float*>(stream), len / sizeof(float), 44100);
    };
    want.userdata = this;

    m_audioDeviceId = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (m_audioDeviceId == 0) {
        // Failed to open audio - this is OK, we can continue without audio
    } else {
        SDL_PauseAudioDevice(m_audioDeviceId, 0); // Start playing
    }
}

void AudioEngine::cleanup() {
    if (m_audioDeviceId != 0) {
        SDL_CloseAudioDevice(m_audioDeviceId);
        m_audioDeviceId = 0;
    }
}

void AudioEngine::process(float* stream, int len, int sampleRate) {
    if (!m_active) {
        // Fill with silence if not active
        for (int i = 0; i < len; i++) {
            stream[i] = 0.0f;
        }
        return;
    }
    
    float frequency = 1.0f; // Base LFO frequency
    
    for (int i = 0; i < len; i++) {
        // Update phase based on frequency
        m_phase += frequency * 2.0f * M_PI / sampleRate;
        if (m_phase > 2.0f * M_PI) {
            m_phase -= 2.0f * M_PI;
        }
        
        // Generate audio based on algorithm output
        float output = generateAlgorithmicWave(m_currentOutput);
        
        // Apply modulation from algorithm
        output += m_modAmount * m_currentOutput;
        
        // Apply volume
        output *= m_volume;
        
        // Clamp to prevent clipping
        if (output > 1.0f) output = 1.0f;
        if (output < -1.0f) output = -1.0f;
        
        stream[i] = output;
        
        // Update last output for next iteration
        m_lastOutput = m_currentOutput;
    }
}

float AudioEngine::generateSine(float frequency, float amplitude) {
    return amplitude * sinf(m_phase * frequency);
}

float AudioEngine::generateAlgorithmicWave(float input) {
    // Convert algorithm input to audio waveform
    // This creates a simple mapping from algorithm output to audio
    float wave = sinf(m_phase + input * 2.0f * M_PI);
    
    // Add harmonics based on input
    wave += 0.3f * sinf(2.0f * m_phase + input * 4.0f * M_PI);
    wave += 0.1f * sinf(4.0f * m_phase + input * 8.0f * M_PI);
    
    // Normalize
    wave *= 0.33f;
    
    return wave;
}