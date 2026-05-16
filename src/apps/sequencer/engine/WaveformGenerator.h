#pragma once

#include "model/Modulator.h"
#include <cstdint>

/**
 * WaveformGenerator - Shared waveform generation for ModulatorEngine and UI preview.
 *
 * Generates consistent waveform sample values so engine output matches visual
 * preview. All shapes output in range -127 to +127.
 */
class WaveformGenerator {
public:
    static int generate(Modulator::Shape shape, uint16_t phase) {
        switch (shape) {
        case Modulator::Shape::Sine:     return generateSine(phase);
        case Modulator::Shape::Triangle:  return generateTriangle(phase);
        case Modulator::Shape::SawUp:     return generateSawUp(phase);
        case Modulator::Shape::SawDown:   return generateSawDown(phase);
        case Modulator::Shape::Square:    return generateSquare(phase);
        case Modulator::Shape::Random:    return 0; // handled in ModulatorEngine with state
        default:                          return 0;
        }
    }

private:
    static int generateSine(uint16_t phase) {
        int32_t x = static_cast<int32_t>(phase) - 32768;
        int32_t abs_x = (x < 0) ? -x : x;
        int32_t result = (4 * x * (32768 - abs_x)) / 32768;
        return -(result * 127 / 32768);
    }

    static int generateTriangle(uint16_t phase) {
        if (phase < 32768) {
            return -127 + ((phase * 254) / 32768);
        }
        return 127 - (((phase - 32768) * 254) / 32768);
    }

    static int generateSawUp(uint16_t phase) {
        return (static_cast<int>(phase) * 254 / 65536) - 127;
    }

    static int generateSawDown(uint16_t phase) {
        return 127 - (static_cast<int>(phase) * 254 / 65536);
    }

    static int generateSquare(uint16_t phase) {
        return (phase < 32768) ? 127 : -127;
    }
};
