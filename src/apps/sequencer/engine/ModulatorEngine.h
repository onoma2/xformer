#pragma once

#include "Config.h"
#include "model/Modulator.h"
#include "WaveformGenerator.h"
#include "core/utils/Random.h"
#include "core/math/Math.h"

#include <cstdint>

class ModulatorEngine {
public:
    enum class ADSRState : uint8_t {
        Idle,
        Attack,
        Decay,
        Sustain,
        Release
    };

    ModulatorEngine() = default;

    void reset() {
        for (int i = 0; i < CONFIG_MODULATOR_COUNT; ++i) {
            _phaseAccumulator[i] = 0;
            _lastRandomValue[i] = 0;
            _currentValue[i] = 0;
            _lastGate[i] = false;
            _targetValue[i] = 0;
            _lastPhase[i] = 0;
            _rng[i] = Random(12345 + i * 6789);
            _adsrState[i] = ADSRState::Idle;
            _adsrLevel[i] = 0;
            _adsrTimer[i] = 0;
        }
    }

    void tick(uint32_t tick, const Modulator &modulator, int index, bool gate) {
        if (index < 0 || index >= CONFIG_MODULATOR_COUNT) {
            return;
        }

        // Handle Random shape in Triggered mode
        if (modulator.shape() == Modulator::Shape::Random &&
            modulator.randomMode() == Modulator::RandomMode::Triggered) {
            // Detect rising edge
            if (gate && !_lastGate[index]) {
                _targetValue[index] = _rng[index].nextRange(255) - 127;
            }
            _lastGate[index] = gate;

            // Smooth interpolation
            int current = _lastRandomValue[index];
            int target = _targetValue[index];
            int smoothMs = modulator.smooth();
            int slewRate = calculateSlewRate(smoothMs, modulator.rate());

            int diff = target - current;
            current += diff / slewRate;
            _lastRandomValue[index] = current;

            int value = current;
            value = (value * modulator.depth()) / 127;
            value += modulator.offset();
            _currentValue[index] = clamp(value + 64, 0, 127);
            return;
        }

        // Handle ADSR shape
        if (modulator.shape() == Modulator::Shape::ADSR) {
            bool gateRising = gate && !_lastGate[index];
            bool gateFalling = !gate && _lastGate[index];
            _lastGate[index] = gate;

            if (gateRising) {
                _adsrState[index] = ADSRState::Attack;
                _adsrTimer[index] = 0;
            } else if (gateFalling && _adsrState[index] != ADSRState::Release) {
                _adsrState[index] = ADSRState::Release;
                _adsrTimer[index] = 0;
            }

            int level = 0;
            switch (_adsrState[index]) {
            case ADSRState::Idle:
                level = 0;
                break;
            case ADSRState::Attack: {
                int attackMs = modulator.attack();
                if (attackMs == 0) {
                    level = 127;
                    _adsrState[index] = ADSRState::Decay;
                    _adsrTimer[index] = 0;
                } else {
                    int attackTicks = (attackMs * CONFIG_PPQN) / 2500;
                    _adsrTimer[index]++;
                    level = clamp(static_cast<int>((127 * _adsrTimer[index]) / attackTicks), 0, 127);
                    if (level >= 127) {
                        _adsrState[index] = ADSRState::Decay;
                        _adsrTimer[index] = 0;
                    }
                }
                break;
            }
            case ADSRState::Decay: {
                int decayMs = modulator.decay();
                int sustainLevel = modulator.sustain();
                if (decayMs == 0 || sustainLevel >= 127) {
                    level = sustainLevel;
                    _adsrState[index] = ADSRState::Sustain;
                } else {
                    int decayTicks = (decayMs * CONFIG_PPQN) / 2500;
                    _adsrTimer[index]++;
                    int decayAmount = 127 - sustainLevel;
                    level = 127 - clamp(static_cast<int>((decayAmount * _adsrTimer[index]) / decayTicks), 0, decayAmount);
                    if (level <= sustainLevel) {
                        level = sustainLevel;
                        _adsrState[index] = ADSRState::Sustain;
                    }
                }
                break;
            }
            case ADSRState::Sustain:
                level = modulator.sustain();
                break;
            case ADSRState::Release: {
                int releaseMs = modulator.release();
                int startLevel = _adsrLevel[index];
                if (releaseMs == 0) {
                    level = 0;
                    _adsrState[index] = ADSRState::Idle;
                } else {
                    int releaseTicks = (releaseMs * CONFIG_PPQN) / 2500;
                    _adsrTimer[index]++;
                    level = startLevel - clamp(static_cast<int>((startLevel * _adsrTimer[index]) / releaseTicks), 0, startLevel);
                    if (level <= 0) {
                        level = 0;
                        _adsrState[index] = ADSRState::Idle;
                    }
                }
                break;
            }
            }

            _adsrLevel[index] = level;
            int value = (level * modulator.amplitude()) / 127;
            _currentValue[index] = clamp(value, 0, 127);
            return;
        }

        // Clocked Random mode
        if (modulator.shape() == Modulator::Shape::Random) {
            // Rate is in PPQN=96 ticks in reference; convert to our PPQN=192
            uint32_t rate = modulator.rate() * 2;
            uint32_t phaseIncrement = 65536 / rate;
            _phaseAccumulator[index] += phaseIncrement;

            uint32_t phase = _phaseAccumulator[index] + (static_cast<uint32_t>(modulator.phase()) * 65536 / 360);

            // New random target each cycle (phase wraps)
            if ((phase & 0xFFFF) < _lastPhase[index]) {
                _targetValue[index] = _rng[index].nextRange(255) - 127;
            }
            _lastPhase[index] = phase & 0xFFFF;

            int current = _lastRandomValue[index];
            int target = _targetValue[index];
            int smoothMs = modulator.smooth();
            int slewRate = calculateSlewRate(smoothMs, modulator.rate());

            int diff = target - current;
            current += diff / slewRate;
            _lastRandomValue[index] = current;

            int value = current;
            value = (value * modulator.depth()) / 127;
            value += modulator.offset();
            _currentValue[index] = clamp(value + 64, 0, 127);
            return;
        }

        // Standard LFO modes (Sine, Triangle, Saw, Square)
        // Rate is in PPQN=96 ticks in reference; convert to our PPQN=192
        uint32_t rate = modulator.rate() * 2;

        // Sync/Retrigger/Hold modes
        if (modulator.mode() == Modulator::Mode::Sync) {
            // Hard sync: reset phase on gate rising edge
            if (gate && !_lastGate[index]) {
                _phaseAccumulator[index] = 0;
            }
            _lastGate[index] = gate;
        } else if (modulator.mode() == Modulator::Mode::Hold) {
            // Hold: reset phase on gate rising edge, only advance while gate is high
            if (gate && !_lastGate[index]) {
                _phaseAccumulator[index] = 0;
            }
            _lastGate[index] = gate;
        } else if (modulator.mode() == Modulator::Mode::Retrigger) {
            // Soft retrigger: restart from phase offset on gate rising edge
            if (gate && !_lastGate[index]) {
                _phaseAccumulator[index] = static_cast<uint32_t>(modulator.phase()) * 65536 / 360;
            }
            _lastGate[index] = gate;
        }

        // Increment phase
        uint32_t phaseIncrement = 65536 / rate;

        if (modulator.mode() == Modulator::Mode::Hold) {
            // Hold: only advance phase while gate is high
            if (gate) {
                _phaseAccumulator[index] += phaseIncrement;
            }
        } else {
            // Free/Sync/Retrigger: always advance
            _phaseAccumulator[index] += phaseIncrement;
        }

        uint32_t phase = _phaseAccumulator[index] + (static_cast<uint32_t>(modulator.phase()) * 65536 / 360);

        // Generate waveform value (-127 to +127)
        int value = WaveformGenerator::generate(modulator.shape(), phase & 0xFFFF);

        // Apply depth and offset
        value = (value * modulator.depth()) / 127;
        value += modulator.offset();

        // Store as 0-127 for output
        _currentValue[index] = clamp(value + 64, 0, 127);
    }

    int currentValue(int index) const {
        if (index < 0 || index >= CONFIG_MODULATOR_COUNT) {
            return 0;
        }
        return _currentValue[index];
    }

    uint16_t currentPhase(int index) const {
        if (index < 0 || index >= CONFIG_MODULATOR_COUNT) {
            return 0;
        }
        return _phaseAccumulator[index] & 0xFFFF;
    }

    ADSRState adsrState(int index) const {
        if (index < 0 || index >= CONFIG_MODULATOR_COUNT) {
            return ADSRState::Idle;
        }
        return _adsrState[index];
    }

    uint32_t adsrTimer(int index) const {
        if (index < 0 || index >= CONFIG_MODULATOR_COUNT) {
            return 0;
        }
        return _adsrTimer[index];
    }

private:
    int calculateSlewRate(int smoothMs, uint32_t rate) {
        if (smoothMs == 0) {
            return 1;
        }
        int ticksForSmooth = (smoothMs * CONFIG_PPQN) / 2500;
        return (ticksForSmooth > 0) ? ticksForSmooth : 1;
    }

    uint32_t _phaseAccumulator[CONFIG_MODULATOR_COUNT] = {};
    int _currentValue[CONFIG_MODULATOR_COUNT] = {};
    int _lastRandomValue[CONFIG_MODULATOR_COUNT] = {};
    uint16_t _lastPhase[CONFIG_MODULATOR_COUNT] = {};
    bool _lastGate[CONFIG_MODULATOR_COUNT] = {};
    int _targetValue[CONFIG_MODULATOR_COUNT] = {};
    Random _rng[CONFIG_MODULATOR_COUNT];

    // ADSR envelope state
    ADSRState _adsrState[CONFIG_MODULATOR_COUNT] = {};
    int _adsrLevel[CONFIG_MODULATOR_COUNT] = {};
    uint32_t _adsrTimer[CONFIG_MODULATOR_COUNT] = {};
};
