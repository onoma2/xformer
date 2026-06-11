#pragma once

#include "Config.h"
#include "model/Modulator.h"
#include "WaveformGenerator.h"
#include "generators/Lorenz.h"
#include "generators/Latoocarfian.h"
#include "core/utils/Random.h"
#include "core/math/Math.h"

#include <cstdint>
#include <cmath>

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

    // Free-domain phase step in 16-bit phase units for one engine update of `dt`
    // seconds at `rateHz`. Carries the fractional remainder so slow rates don't
    // drift (truncating dt*Hz*65536 each update would lose phase at low Hz).
    static uint32_t freePhaseIncrement(float dt, float rateHz, float &remainder) {
        float inc = dt * rateHz * 65536.f + remainder;
        uint32_t step = (inc > 0.f) ? uint32_t(inc) : 0u;
        remainder = inc - float(step);
        return step;
    }

    // --- Spring shape param maps (shared by engine + UI). Reuses attack=TENSION,
    // decay=RING, smooth=CLANG. See docs/spring-modulator-spec.md §4. ---
    static float springTensionHz(int attack) { return 0.3f * std::pow(40.f, attack / 2000.f); }
    static float springZeta(int decay)       { return 0.0015f * std::pow(800.f, 1.f - decay / 2000.f); }
    static float springClang(int smooth)     { return smooth / 5000.f; }

    // A source level in [0,1] (0.5 = threshold) gated with hysteresis. Boolean
    // sources (0/1) cross cleanly; continuous ones get a debounce band.
    static bool gateFromLevel(float value, bool prevGate) {
        return prevGate ? (value >= 0.45f) : (value >= 0.55f);
    }

    // --- JustF (Just Friends / ochd) rate link: M1 master + INTONE spread ---
    // index is 1-based; M1 (index 1) is the identity. INTONE -1..+1: noon = unison,
    // CW = harmonic (M_k = k x M1), CCW = subharmonic (M_k = M1 / k).
    static float intoneRatio(float intone, int index) {
        if (index <= 1) return 1.f;
        float n = float(index - 1);
        return intone >= 0.f ? (1.f + intone * n) : (1.f / (1.f - intone * n));
    }
    // B-clamp: cap the master so the fastest follower stays <= `cap` (rate ceiling
    // 16 Hz by default). Top clamp only on the harmonic side; subharmonic keeps range.
    static float justfMasterHz(float m1Hz, float intone, float cap = 16.f) {
        float maxN = float(CONFIG_MODULATOR_COUNT - 1);
        float maxRatio = 1.f + maxN * (intone > 0.f ? intone : 0.f);
        float master = cap / maxRatio;
        return m1Hz < master ? m1Hz : master;
    }
    static float justfEffectiveHz(float m1Hz, float intone, int index, float cap = 16.f) {
        float hz = justfMasterHz(m1Hz, intone, cap) * intoneRatio(intone, index);
        if (hz > cap) hz = cap;
        if (hz < 0.0001f) hz = 0.0001f;
        return hz;
    }
    // ms face of the same spread: an envelope duration is a rate (speed = 1000/ms).
    // Spreads via justfEffectiveHz with the 12 ms floor expressed as its cap; a 0 ms
    // stage (instant) is preserved unscaled. No spread logic of its own.
    static int justfEffectiveMs(int m1Ms, float intone, int index) {
        if (m1Ms <= 0) return 0;
        float hz = justfEffectiveHz(1000.f / float(m1Ms), intone, index, 1000.f / 12.f);
        return int(1000.f / hz + 0.5f);
    }

    // --- Output transform: configurable floor + invert ---
    // Unipolar (ADSR / Geode voice): env in [0,amplitude] (rest 0) lifts into [floor,127];
    // invert flips within [0,amplitude] so the envelope rests at the window top and ducks.
    static int unipolarOutput(int env, int amplitude, int floor, bool invert) {
        if (invert) env = amplitude - env;
        return clamp(floor + (env * (127 - floor)) / 127, 0, 127);
    }
    // Bipolar (LFO / Random / Chaos): offset already biases the value; invert flips around mid.
    static int bipolarOutput(int value, bool invert) {
        value = clamp(value, 0, 127);
        return invert ? (127 - value) : value;
    }

    bool justfActive() const { return _justfActive; }
    void setJustfActive(bool active) { _justfActive = active; }
    float intone() const { return _intone; }
    void setIntone(float v) { _intone = clamp(v, -1.f, 1.f); }

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
            _lorenz[i].reset();
            _latoocarfian[i].reset();
            _freeRemainder[i] = 0.f;
            _manualGateMs[i] = 0.f;
            for (int m = 0; m < 3; ++m) { _springX[i][m] = 0.f; _springV[i][m] = 0.f; }
        }
        _justfActive = false;
        _intone = 0.f;
    }

    // Panel audition: a one-shot gate pulse OR'd into a modulator's gate (F1 re-press).
    // ADSR/Random-Triggered/Chaos-Trig/Geode-voice respond; a Run-mode LFO ignores it.
    void triggerManual(int index) {
        if (index >= 0 && index < CONFIG_MODULATOR_COUNT) _manualGateMs[index] = 60.f;
    }
    bool manualGateHigh(int index) const {
        return index >= 0 && index < CONFIG_MODULATOR_COUNT && _manualGateMs[index] > 0.f;
    }
    void decayManualGate(int index, float dt) {
        if (index >= 0 && index < CONFIG_MODULATOR_COUNT && _manualGateMs[index] > 0.f) {
            _manualGateMs[index] -= dt * 1000.f;
        }
    }

    void tick(uint32_t tick, float dt, const Modulator &modulator, int index, bool gate,
              float rateHzOverride = -1.f, const Modulator *justfEnvBase = nullptr) {
        if (index < 0 || index >= CONFIG_MODULATOR_COUNT) {
            return;
        }
        // JustF (Free-domain) substitutes the derived rate for the modulator's own.
        const float effRateHz = (rateHzOverride >= 0.f) ? rateHzOverride : modulator.rateHz();

        // Random shape — driven by the universal Mode (uniform with Chaos):
        //   Run  = free-running clocked sample-and-hold (gate ignored)
        //   Trig = sample-and-hold on gate rising edge
        //   Gate = track-and-hold: resample on the clock while gate high, hold when low
        if (modulator.shape() == Modulator::Shape::Random) {
            const Modulator::Mode mode = modulator.mode();
            const bool gateRising = gate && !_lastGate[index];
            _lastGate[index] = gate;

            if (mode == Modulator::Mode::Trig) {
                if (gateRising) {
                    _targetValue[index] = _rng[index].nextRange(255) - 127;
                }
            } else if (mode == Modulator::Mode::Run || gate) {
                // Run advances always; Gate advances only while the gate is high.
                uint32_t phaseIncrement = (modulator.rateDomain() == Modulator::RateDomain::Free)
                    ? freePhaseIncrement(dt, effRateHz, _freeRemainder[index])
                    : 65536 / (modulator.rate() * 2);
                _phaseAccumulator[index] += phaseIncrement;
                uint32_t phase = _phaseAccumulator[index] + (static_cast<uint32_t>(modulator.phase()) * 65536 / 360);
                if ((phase & 0xFFFF) < _lastPhase[index]) {
                    _targetValue[index] = _rng[index].nextRange(255) - 127;
                }
                _lastPhase[index] = phase & 0xFFFF;
            }

            // Smooth interpolation toward the held target.
            int current = _lastRandomValue[index];
            int target = _targetValue[index];
            int slewRate = calculateSlewRate(modulator.smooth(), modulator.rate());
            current += (target - current) / slewRate;
            _lastRandomValue[index] = current;

            int value = (current * modulator.depth()) / 127 + modulator.offset();
            _currentValue[index] = bipolarOutput(value + 64, modulator.invert());
            return;
        }

        // Spring shape — mallet-struck 3-mode inharmonic resonator (semi-implicit
        // Euler at control rate). Universal Mode: Run = Free strike clock,
        // Trig = gate edge, Gate = hold-displaced then release. Deterministic
        // strike (no RNG) so cloned slots phase-lock. docs/spring-modulator-spec.md.
        if (modulator.shape() == Modulator::Shape::Spring) {
            const Modulator::Mode mode = modulator.mode();
            const bool gateRising = gate && !_lastGate[index];
            _lastGate[index] = gate;

            static const float R[3] = { 1.0f, 2.76f, 5.40f };
            const float PI2 = 6.2831853f;
            float w0 = PI2 * springTensionHz(modulator.attack());
            // §6.3 stability: keep the top partial below the Euler limit (w·dt < 1.5).
            const float wMaxStable = 1.5f / dt;
            if (w0 * R[2] > wMaxStable) w0 = wMaxStable / R[2];
            const float zeta = springZeta(modulator.decay());
            const float clang = springClang(modulator.smooth());
            const float strikeForce = 0.7f * (modulator.depth() / 127.f);   // §6.6 DEPTH = hit force
            const float a[3] = { 1.f, clang, 0.6f * clang };   // pickup amplitude weights
            float *X = _springX[index];
            float *V = _springV[index];

            // strike trigger
            bool doStrike = false;
            if (mode == Modulator::Mode::Trig) {
                doStrike = gateRising;
            } else if (mode == Modulator::Mode::Run) {
                // freePhaseIncrement works in 16-bit phase units (one cycle = 65536),
                // so detect the wrap on the low 16 bits, not the 32-bit overflow.
                uint32_t inc = (modulator.rateDomain() == Modulator::RateDomain::Free)
                    ? freePhaseIncrement(dt, effRateHz, _freeRemainder[index])
                    : 65536 / (modulator.rate() * 2);
                _phaseAccumulator[index] += inc;
                uint16_t now16 = uint16_t(_phaseAccumulator[index] & 0xFFFF);
                if (now16 < _lastPhase[index]) doStrike = true;          // cycle wrap = hit
                _lastPhase[index] = now16;
            }
            if (doStrike) {
                const float v0 = w0 * strikeForce;
                for (int i = 0; i < 3; ++i) V[i] += v0;
            }

            const bool held = (mode == Modulator::Mode::Gate && gate);
            if (held) {
                for (int i = 0; i < 3; ++i) { X[i] = (strikeForce / R[i]) * a[i]; V[i] = 0.f; }
            } else {
                for (int i = 0; i < 3; ++i) {
                    const float w = w0 * R[i];
                    const float k = w * w;
                    const float c = 2.f * zeta * w * (1.f + 0.8f * i);   // upper modes die faster
                    V[i] += (-k * X[i] - c * V[i]) * dt;
                    X[i] += V[i] * dt;
                    if (X[i] > 4.f) X[i] = 4.f; else if (X[i] < -4.f) X[i] = -4.f;
                    const float vMax = 4.f * w0;
                    if (V[i] > vMax) V[i] = vMax; else if (V[i] < -vMax) V[i] = -vMax;
                }
            }

            const float w0sq = w0 * w0 + 1e-9f;
            float s = 0.f;
            switch (modulator.springPickup()) {
            case 1: for (int i = 0; i < 3; ++i) s += a[i] * V[i]; s /= (w0 > 1e-6f ? w0 : 1e-6f); break;        // velocity
            case 2: for (int i = 0; i < 3; ++i) s += a[i]*a[i] * 0.5f*V[i]*V[i]/w0sq; s *= 4.f; break;          // kinetic
            case 3: for (int i = 0; i < 3; ++i) s += a[i]*a[i] * 0.5f*R[i]*R[i]*X[i]*X[i]; s *= 4.f; break;     // potential
            case 4: { float ke = 0.f, pe = 0.f;
                for (int i = 0; i < 3; ++i) { ke += a[i]*a[i]*0.5f*V[i]*V[i]/w0sq; pe += a[i]*a[i]*0.5f*R[i]*R[i]*X[i]*X[i]; }
                s = 4.f * (ke + pe); break; }                                                                  // total
            default: for (int i = 0; i < 3; ++i) s += a[i] * X[i]; break;                                       // position
            }

            int value = int(64.f + 63.f * std::tanh(s)) + modulator.offset();   // soft-sat, no hard clip
            if (value < 0) value = 0; else if (value > 127) value = 127;
            _currentValue[index] = bipolarOutput(value, modulator.invert());
            return;
        }

        // Handle ADSR shape
        if (modulator.shape() == Modulator::Shape::ADSR) {
            bool gateRising = gate && !_lastGate[index];
            bool gateFalling = !gate && _lastGate[index];
            _lastGate[index] = gate;

            // JustF: inherit M1's envelope (A/D/R + sustain level) spread by this
            // follower's index ratio; amplitude stays per-follower. justfEnvBase is M1
            // when JustF is active, null otherwise (then the follower's own params).
            int attackMs, decayMs, releaseMs, sustainLevel;
            if (justfEnvBase) {
                attackMs = justfEffectiveMs(justfEnvBase->attack(), _intone, index + 1);
                decayMs = justfEffectiveMs(justfEnvBase->decay(), _intone, index + 1);
                releaseMs = justfEffectiveMs(justfEnvBase->release(), _intone, index + 1);
                sustainLevel = justfEnvBase->sustain();
            } else {
                attackMs = modulator.attack();
                decayMs = modulator.decay();
                releaseMs = modulator.release();
                sustainLevel = modulator.sustain();
            }

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
                if (attackMs == 0) {
                    level = 127;
                    _adsrState[index] = ADSRState::Decay;
                    _adsrTimer[index] = 0;
                } else {
                    int attackTicks = (attackMs * CONFIG_PPQN) / 2500;
                    if (attackTicks < 1) attackTicks = 1;
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
                if (decayMs == 0 || sustainLevel >= 127) {
                    level = sustainLevel;
                    _adsrState[index] = ADSRState::Sustain;
                } else {
                    int decayTicks = (decayMs * CONFIG_PPQN) / 2500;
                    if (decayTicks < 1) decayTicks = 1;
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
                level = sustainLevel;
                break;
            case ADSRState::Release: {
                int startLevel = _adsrLevel[index];
                if (releaseMs == 0) {
                    level = 0;
                    _adsrState[index] = ADSRState::Idle;
                } else {
                    int releaseTicks = (releaseMs * CONFIG_PPQN) / 2500;
                    if (releaseTicks < 1) releaseTicks = 1;
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
            int env = (level * modulator.amplitude()) / 127;
            _currentValue[index] = unipolarOutput(env, modulator.amplitude(),
                                                  modulator.floorValue(), modulator.invert());
            return;
        }

        // Chaos shapes (Lorenz, Latoocarfian)
        if (modulator.shape() == Modulator::Shape::ChaosLorenz ||
            modulator.shape() == Modulator::Shape::ChaosLatoocarfian) {
            // Gate mode handling: Trig and Gate both re-seed the attractor on gate rising.
            if (modulator.mode() == Modulator::Mode::Trig ||
                modulator.mode() == Modulator::Mode::Gate) {
                if (gate && !_lastGate[index]) {
                    resetChaos(index, modulator.shape(), modulator.phase());
                }
                _lastGate[index] = gate;
            }

            // Phase advance this update: Free = real dt*Hz (drift-free), Tempo = PPQN division.
            uint32_t inc = (modulator.rateDomain() == Modulator::RateDomain::Free)
                ? freePhaseIncrement(dt, effRateHz, _freeRemainder[index])
                : 65536 / (modulator.rate() * 2);
            bool shouldAdvance = (modulator.mode() == Modulator::Mode::Run) ||
                                (modulator.mode() == Modulator::Mode::Trig) ||
                                (modulator.mode() == Modulator::Mode::Gate && gate);

            float rawValue = 0.f;
            if (shouldAdvance) {
                // Normalize P1/P2 to 0..1 (attack/decay are 0..2000)
                float p1n = modulator.attack() / 2000.f;
                float p2n = modulator.decay() / 2000.f;

                // Parabolic curve: t*(2-t) expands the sweet spot into the
                // upper-mid range while keeping low values stable.
                float p1curve = p1n * (2.0f - p1n);
                float p2curve = p2n * (2.0f - p2n);

                if (modulator.shape() == Modulator::Shape::ChaosLorenz) {
                    float rho  = 10.0f + p1curve * 40.0f;     // P1: 10..50
                    float beta = 0.5f + p2curve * 3.5f;        // P2: 0.5..4.0
                    // attractor-time = inc/65536 cycles (1.0 unit/cycle); substep keeps each
                    // Lorenz integration step small enough to stay stable at higher rates.
                    float adv = inc / 65536.f;
                    int sub = clamp(static_cast<int>(adv / 0.001f) + 1, 1, 64);
                    float st = adv / sub;
                    for (int i = 0; i < sub; ++i) {
                        rawValue = _lorenz[index].next(st, rho, beta);
                    }
                } else {
                    // Latoocarfian: phase-stepped, steps the map on phase wrap
                    _phaseAccumulator[index] += inc;
                    if ((_phaseAccumulator[index] & 0xFFFF) < _lastPhase[index]) {
                        float ab = 0.5f + p1curve * 2.5f;      // P1: 0.5..3.0
                        float cd = 0.5f + p2curve * 2.5f;      // P2: 0.5..3.0
                        rawValue = _latoocarfian[index].next(ab, ab, cd, cd);
                    }
                    _lastPhase[index] = _phaseAccumulator[index] & 0xFFFF;
                }
            }

            int target = static_cast<int>(rawValue * 127.f);  // -127..+127
            int smoothMs = modulator.smooth();
            int slewRate = calculateSlewRate(smoothMs, 0);
            int current = _lastRandomValue[index];
            int diff = target - current;
            current += diff / slewRate;
            _lastRandomValue[index] = current;
            int value = current;
            // Apply depth and offset
            value = (value * modulator.depth()) / 127;
            value += modulator.offset();
            _currentValue[index] = bipolarOutput(value + 64, modulator.invert());
            return;
        }

        // Standard LFO modes (Sine, Triangle, Saw, Square)

        // Gate modes: Trig restarts from the phase offset on gate rising (offset 0 = hard
        // sync); Gate restarts at 0 and only advances while gate high (windowed).
        if (modulator.mode() == Modulator::Mode::Trig) {
            if (gate && !_lastGate[index]) {
                _phaseAccumulator[index] = static_cast<uint32_t>(modulator.phase()) * 65536 / 360;
                _freeRemainder[index] = 0.f;
            }
            _lastGate[index] = gate;
        } else if (modulator.mode() == Modulator::Mode::Gate) {
            if (gate && !_lastGate[index]) {
                _phaseAccumulator[index] = 0;
                _freeRemainder[index] = 0.f;
            }
            _lastGate[index] = gate;
        }

        // Increment phase: Free advances by real dt*Hz (drift-free), Tempo by the PPQN division.
        uint32_t phaseIncrement = (modulator.rateDomain() == Modulator::RateDomain::Free)
            ? freePhaseIncrement(dt, effRateHz, _freeRemainder[index])
            : 65536 / (modulator.rate() * 2);

        if (modulator.mode() == Modulator::Mode::Gate) {
            // Gate: only advance phase while gate is high
            if (gate) {
                _phaseAccumulator[index] += phaseIncrement;
            }
        } else {
            // Run/Trig: always advance
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

    // Geode: write an externally-driven voice level (0..1) into this modulator's output,
    // through the same floor/invert transform a normal envelope uses (amplitude scales it).
    void setVoiceOutput(int index, float level01, const Modulator &modulator) {
        if (index < 0 || index >= CONFIG_MODULATOR_COUNT) {
            return;
        }
        int env = int(clamp(level01, 0.f, 1.f) * modulator.amplitude() + 0.5f);
        _currentValue[index] = unipolarOutput(env, modulator.amplitude(),
                                              modulator.floorValue(), modulator.invert());
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
    void resetChaos(int index, Modulator::Shape shape, uint16_t phase) {
        if (shape == Modulator::Shape::ChaosLorenz) {
            _lorenz[index].reset();
        } else {
            _latoocarfian[index].reset();
        }
        _lastRandomValue[index] = 0;
        _lastGate[index] = false;
    }

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
    float _freeRemainder[CONFIG_MODULATOR_COUNT] = {};   // Free-domain phase fractional carry
    float _manualGateMs[CONFIG_MODULATOR_COUNT] = {};    // panel audition one-shot pulse
    float _springX[CONFIG_MODULATOR_COUNT][3] = {};      // Spring shape — 3-mode position
    float _springV[CONFIG_MODULATOR_COUNT][3] = {};      // Spring shape — 3-mode velocity
    bool _justfActive = false;                           // JustF rate-link mode (runtime-only)
    float _intone = 0.f;                                 // JustF spread (-1..+1), runtime-only
    Random _rng[CONFIG_MODULATOR_COUNT];

    // ADSR envelope state
    ADSRState _adsrState[CONFIG_MODULATOR_COUNT] = {};
    int _adsrLevel[CONFIG_MODULATOR_COUNT] = {};
    uint32_t _adsrTimer[CONFIG_MODULATOR_COUNT] = {};

    // Chaos generators
    Lorenz _lorenz[CONFIG_MODULATOR_COUNT];
    Latoocarfian _latoocarfian[CONFIG_MODULATOR_COUNT];
};
