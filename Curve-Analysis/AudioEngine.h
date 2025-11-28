#pragma once
#include <cmath>
#include <vector>
#include <algorithm>
#include <mutex>

class AudioEngine {
public:
    AudioEngine() : m_phase(0.0f), m_frequency(220.0f), m_volume(0.0f), m_active(false), m_modAmount(0.0f), m_lfoPhase(0.0f), m_lfoFrequency(1.0f) {}

    void process(float* buffer, int samples, int sampleRate) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        float dt = 1.0f / sampleRate;
        
        for (int i = 0; i < samples; ++i) {
            if (!m_active) {
                buffer[i] = 0.0f;
                continue;
            }

            // Get current LFO value from buffer
            float lfoValue = 0.0f;
            if (!m_lfoBuffer.empty()) {
                // Interpolate from buffer based on LFO phase
                // LFO Phase 0..1 maps to Buffer Index 0..Size
                float indexFloat = m_lfoPhase * m_lfoBuffer.size();
                int idxA = static_cast<int>(indexFloat) % m_lfoBuffer.size();
                int idxB = (idxA + 1) % m_lfoBuffer.size();
                float frac = indexFloat - static_cast<int>(indexFloat);
                lfoValue = m_lfoBuffer[idxA] * (1.0f - frac) + m_lfoBuffer[idxB] * frac;
                
                // Advance LFO phase
                // Phase increment = LFO_Freq * dt
                m_lfoPhase += m_lfoFrequency * dt;
                if (m_lfoPhase >= 1.0f) m_lfoPhase -= 1.0f;
            }

            // Calculate modulated frequency
            // LFO Value is typically -5V to +5V.
            // Let's say 1V = 1 Octave (doubling frequency)
            // Freq = BaseFreq * 2^(LFO * ModAmount)
            // ModAmount scaling: 1.0 = 1V/Oct standard sensitivity
            float modOctaves = lfoValue * m_modAmount; 
            float currentFreq = m_frequency * powf(2.0f, modOctaves);
            
            // Safety clamp for frequency
            currentFreq = std::max(20.0f, std::min(20000.0f, currentFreq));

            // Advance Audio phase
            m_phase += currentFreq * dt;
            if (m_phase > 1.0f) m_phase -= 1.0f;

            // Generate Sine Wave
            float sample = sinf(m_phase * 2.0f * M_PI) * m_volume;

            // Hard Limiter (-1.0 to 1.0)
            sample = std::max(-1.0f, std::min(1.0f, sample));

            buffer[i] = sample;
        }
    }

    void setLfoBuffer(const std::vector<float>& buffer, float frequency) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lfoBuffer = buffer;
        m_lfoFrequency = frequency;
    }

    // Parameters
    float m_phase;
    float m_frequency;
    float m_volume;     // 0.0 - 1.0
    bool m_active;      // Toggle
    float m_modAmount;  // Modulation depth
    
    // LFO State
    std::vector<float> m_lfoBuffer;
    float m_lfoPhase;   // 0.0 to 1.0 tracking the LFO cycle
    float m_lfoFrequency; // Hz
    
    std::mutex m_mutex;
};
