# Performer Geode Implementation Plan

## Overview

Implementation of Just Friends "Geode" engine within Performer's TeletypeTrackEngine. Provides 6 virtual voices generating polyrhythmic envelopes, mixed to a single CV output, controllable via Teletype G.* operations.

**Key Characteristics:**
- Control-rate only (1kHz update)
- Phase-locked to Performer's transport (measure-based timing)
- Event-based voice triggering (not continuous)
- Single mixed output (-5V to +5V full bipolar, with G.O offset like E.O)
- Optional per-voice outputs for advanced patching

---

## Architecture

### Three-Layer Design

```
┌─────────────────────────────────────────────────────────┐
│  Layer 1: Teletype Ops (G.*)                           │
│  Location: teletype/src/ops/hardware.c                 │
│  Purpose: Parse commands, validate, call bridge        │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│  Layer 2: TeletypeBridge                               │
│  Location: src/apps/sequencer/engine/TeletypeBridge.cpp│
│  Purpose: C/C++ boundary, route to active engine       │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│  Layer 3: TeletypeTrackEngine + GeodeEngine            │
│  Location: src/apps/sequencer/engine/                  │
│  Purpose: Parameter storage, DSP orchestration         │
└────────────────┬────────────────────────────────────────┘
                 │
┌────────────────▼────────────────────────────────────────┐
│  Layer 4: GeodeEngine (Pure DSP)                       │
│  Location: src/apps/sequencer/engine/GeodeEngine.h/cpp │
│  Purpose: Voice state, physics, envelope generation    │
└─────────────────────────────────────────────────────────┘
```

### Data Flow

```
User Script       → Teletype Op      → Bridge           → TrackEngine
G.TIME 12000        op_G_TIME_get()    tele_g_time()     setGeodeTime()
                                                          ↓
                                                     _geodeParams.time = 12000

Update Cycle (1kHz):
TeletypeTrackEngine::update(dt)
  ↓
  Convert params: 0-16383 → normalized floats
  ↓
  GeodeEngine::update(dt, measureFraction, normalized_params)
    ↓
    Update voice phases (measure-locked)
    Detect phase wraps → trigger envelopes
    Apply physics (calculate velocity)
    Generate AR envelopes (INTONE-scaled)
    Mix voices → single output
  ↓
  Output: TeletypeTrackEngine::cvOutput() → -5V to +5V
```

---

## Teletype Operations (G.*)

All ops use **0-16383 unipolar encoding** at user level.
Bipolar parameters use **8192 as center/noon**.

### Global Performance Parameters (6 ops - bidirectional)

```c
G.TIME [x]   // Base envelope time (0-16383, unipolar)
             // 0 = fastest, 16383 = slowest
             // Maps to: 0.0-1.0 internally

G.TONE [x]   // INTONE spread (0-16383, BIPOLAR)
             // 0 = undertones (voices slower), 8192 = noon (equal), 16383 = overtones (voices faster)
             // Maps to: -1.0 to +1.0 internally

G.RAMP [x]   // Attack/Decay balance (0-16383, unipolar)
             // 0 = instant attack/long decay (sawtooth down)
             // 8192 = equal attack/decay (triangle)
             // 16383 = long attack/instant decay (sawtooth up)
             // Maps to: 0.0-1.0 internally

G.CURV [x]   // Envelope curve shape (0-16383, BIPOLAR)
             // 0 = rectangular (square), 8192 = linear, 16383 = sinusoidal (smooth)
             // Maps to: -1.0 to +1.0 internally

G.RUN [x]    // Physics parameter (0-16383, unipolar)
             // Mode 0: Accent cycle rate (0=all accents, high=every Nth)
             // Mode 1: Decay/bounce rate
             // Mode 2: Amplitude LFO rate
             // Maps to: 0.0-1.0 internally

G.MODE [x]   // Physics mode (0-2, discrete)
             // 0 = Transient (rhythmic accents)
             // 1 = Sustain (decay/gravity)
             // 2 = Cycle (amplitude LFO)

G.O [x]      // Output offset (0-16383, unipolar)
             // Sets the "silence" voltage level (like E.O for envelopes)
             // 0 = silence at -5V (default)
             // 8192 = silence at 0V
             // 16383 = silence at +5V
             // Output interpolates between offset and +5V based on envelope level
```

**Usage:**
```c
// Setter
G.TIME 12000        // Set time to 12000

// Getter
X G.TIME            // Read current time into X
```

### Voice Configuration (1 op - trigger + configure)

```c
G.V i divs repeats   // Trigger voice i with divs & repeats
                     // i: 1-6 (voice index, 0 = all voices)
                     // divs: 1-64 (divisions per measure)
                     // repeats: -1 to 255 (-1 = infinite)
```

**Behavior:**
- Sets voice `i` configuration (divs, repeats)
- **Starts/restarts** the voice immediately
- Voice generates envelopes until repeats exhausted (unless -1)

**Examples:**
```c
G.V 1 4 8      // Voice 1: quarter notes, 8 repeats, then stop
G.V 2 7 -1     // Voice 2: 7-div polyrhythm, infinite
G.V 0 1 16     // ALL voices: whole notes, 16 repeats
```

### Output (2 ops - getters only)

```c
G.VAL           // Get mixed output (0-16383)
                // All 6 voices mixed using analog max algorithm
                // Returns: raw value interpolated between G.O offset and 16383
                // When silent (all voices at 0): returns G.O value
                // When max (any voice at 1.0): returns 16383
                // Maps to voltage via rawToVolts(): 0→-5V, 8192→0V, 16383→+5V

G.R x y         // Route Geode output to CV x (1-4)
                // x: CV output (1-4)
                // y: 0 = mix, 1-6 = voice index, <0 = none
```

**Usage:**
```c
// Route mixed output to CV 1
CV 1 G.VAL

// Route mix to CV 1, voice 3 to CV 2
G.R 1 0
G.R 2 3
```

### Complete Op Summary

| Op | Args | Type | Description |
|-----|------|------|-------------|
| `G.TIME` | `[x]` | Bidirectional | Base envelope time |
| `G.TONE` | `[x]` | Bidirectional | INTONE spread (bipolar) |
| `G.RAMP` | `[x]` | Bidirectional | Attack/decay balance |
| `G.CURV` | `[x]` | Bidirectional | Curve shape (bipolar) |
| `G.RUN` | `[x]` | Bidirectional | Physics parameter |
| `G.MODE` | `[x]` | Bidirectional | Physics mode (0/1/2) |
| `G.O` | `[x]` | Bidirectional | Output offset (silence point) |
| `G.V` | `i divs reps` | Setter | Trigger voice with config |
| `G.VAL` | - | Getter | Mixed output |
| `G.R` | `x y` | Setter | Route mix/voice to CV output |

**Total: 10 operations**

---

## Parameter Mapping Reference

### Unipolar Parameters (0-16383 → 0.0-1.0)

```cpp
float normalizeUnipolar(int16_t value) {
    return clamp(value, 0, 16383) / 16383.0f;
}

int16_t denormalizeUnipolar(float value) {
    return static_cast<int16_t>(clamp(value, 0.0f, 1.0f) * 16383.0f);
}
```

**Applied to:** TIME, RAMP, RUN

### Bipolar Parameters (0-16383 → -1.0 to +1.0)

```cpp
float normalizeBipolar(int16_t value) {
    float clamped = clamp(value, 0, 16383);
    return (clamped / 8192.0f) - 1.0f;
    // 0     → -1.0
    // 8192  →  0.0 (noon/center)
    // 16383 → +1.0
}

int16_t denormalizeBipolar(float value) {
    float clamped = clamp(value, -1.0f, 1.0f);
    return static_cast<int16_t>((clamped + 1.0f) * 8192.0f);
}
```

**Applied to:** TONE (INTONE), CURV (curve shape)

### Complete Mapping Table

| Parameter | Teletype Range | Center Value | Internal Range | Type |
|-----------|----------------|--------------|----------------|------|
| TIME | 0-16383 | N/A | 0.0-1.0 | Unipolar |
| TONE | 0-16383 | 8192 | -1.0 to +1.0 | Bipolar |
| RAMP | 0-16383 | 8192 | 0.0-1.0 | Unipolar |
| CURV | 0-16383 | 8192 | -1.0 to +1.0 | Bipolar |
| RUN | 0-16383 | N/A | 0.0-1.0 | Unipolar |
| MODE | 0-2 | N/A | 0-2 | Direct |
| OFFSET | 0-16383 | 0 | 0-16383 (raw) | Direct |
| DIVS | 1-64 | N/A | 1-64 | Direct |
| REPEATS | -1 to 255 | N/A | -1 to 255 | Direct |

---

## GeodeEngine Implementation

### Header (GeodeEngine.h)

```cpp
#pragma once

#include <cstdint>
#include <array>

class GeodeEngine {
public:
    static constexpr int VoiceCount = 6;

    GeodeEngine();

    // Voice triggering (called from G.V operation)
    void triggerVoice(int voiceIndex, int divs, int repeats);
    void triggerAllVoices(int divs, int repeats);

    // Main update (1kHz, called from TeletypeTrackEngine)
    void update(float dt, float measureFraction,
                float time, float intone, float ramp, float curve,
                float run, uint8_t mode);

    // Outputs (offset is raw 0-16383, determines silence voltage)
    float output(int16_t offsetRaw) const;
    int16_t outputRaw(int16_t offsetRaw) const;
    float voiceOutput(int index, int16_t offsetRaw) const;
    int16_t voiceOutputRaw(int index, int16_t offsetRaw) const;
    float computeMixLevel() const;

    // State query
    bool voiceActive(int index) const;

    void reset();

private:
    struct Voice {
        // Sequencer state
        float phase;              // 0.0-1.0 (measure fraction * divs)
        int divs;                 // 1-64 (rhythm divisor)
        int repeatsTotal;         // -1=infinite, 0-255
        int repeatsRemaining;     // Countdown
        int stepIndex;            // Current step in burst (for physics)
        bool active;              // Voice is running

        // Envelope state
        float level;              // Current output level (0.0-1.0)
        float targetLevel;        // From physics calculation
        float riseTime;           // Attack time (scaled by INTONE)
        float fallTime;           // Decay time (scaled by INTONE)
        float envelopePhase;      // 0.0-1.0 through AR cycle
        bool inAttack;            // true=attack, false=decay

        void reset();
    };

    std::array<Voice, VoiceCount> _voices;
    float _mixedOutput;
    float _prevMeasureFraction;  // Track for delta calculation

    // Helper functions
    bool updateVoicePhase(Voice &voice, float measureDelta, float measureFraction);
    void triggerVoiceEnvelope(Voice &voice, float velocity,
                               float time, float intone, int voiceIndex);
    float calculatePhysics(const Voice &voice, float run, uint8_t mode);
    void updateVoiceEnvelope(Voice &voice, float dt, float ramp, float curve);
    float computeMixedOutput() const;
    float getVoiceTimeScale(int voiceIndex, float intone) const;
    float applyCurveShape(float phase, float curve, bool isAttack, float ramp) const;
};
```

### Core Algorithms

#### 1. Phase Accumulation (Measure-Locked)

```cpp
bool GeodeEngine::updateVoicePhase(Voice &voice, float measureDelta,
                                   float measureFraction) {
    if (!voice.active) return false;

    // Increment phase by measure delta * divs
    float prevPhase = voice.phase;
    voice.phase += measureDelta * voice.divs;

    // Detect wrap (trigger event)
    if (voice.phase >= 1.0f) {
        voice.phase = std::fmod(voice.phase, 1.0f);

        // Check if repeats remaining
        if (voice.repeatsRemaining > 0) {
            voice.repeatsRemaining--;
            voice.stepIndex++;
            return true; // Trigger happened
        } else if (voice.repeatsRemaining < 0) {
            // Infinite repeats
            voice.stepIndex++;
            return true;
        } else {
            // Exhausted - stop voice
            voice.active = false;
            return false;
        }
    }

    return false;
}
```

#### 2. Physics Calculation (Velocity)

```cpp
float GeodeEngine::calculatePhysics(const Voice &voice, float run, uint8_t mode) {
    switch (mode) {
    case 0: // Transient (Rhythmic Accent)
        {
            int cycle = static_cast<int>(run * 7.0f) + 1; // 1-8
            bool accent = (voice.stepIndex % cycle) == 0;
            return accent ? 1.0f : 0.3f;
        }

    case 1: // Sustain (Decay/Gravity)
        {
            float damp = 0.05f + run * 0.20f;
            float velocity = std::pow(1.0f - damp, voice.stepIndex);
            return velocity;
        }

    case 2: // Cycle (Amplitude LFO)
        {
            float rate = 1.0f + run * 3.0f; // 1-4 cycles per burst
            float burstProgress = (voice.repeatsTotal > 0)
                ? static_cast<float>(voice.stepIndex) / voice.repeatsTotal
                : 0.0f;
            float lfoPhase = burstProgress * rate * 2.0f * M_PI;
            return 0.5f + 0.5f * std::sin(lfoPhase);
        }
    }

    return 1.0f;
}
```

#### 3. INTONE Scaling (JF Formula)

```cpp
float GeodeEngine::getVoiceTimeScale(int voiceIndex, float intone) const {
    // voiceIndex: 0-5 for voices 1-6
    // intone: -1.0 (undertones) to 0.0 (noon) to +1.0 (overtones)

    // JF formula: scale = 2^(intone * (voiceIndex - 3.5) / 5.0)
    float exponent = intone * ((voiceIndex + 1) - 3.5f) / 5.0f;
    return std::pow(2.0f, exponent);

    // Examples:
    // Voice 1 (index 0), intone=+1.0: 2^(-0.5) = 0.707x (slower)
    // Voice 6 (index 5), intone=+1.0: 2^(+0.5) = 1.414x (faster)
}
```

#### 4. Envelope Generation (AR with Curve Shaping)

```cpp
void GeodeEngine::updateVoiceEnvelope(Voice &voice, float dt,
                                       float ramp, float curve) {
    if (voice.level <= 0.0f && !voice.inAttack) {
        return; // Idle
    }

    float timeConstant = voice.inAttack ? voice.riseTime : voice.fallTime;
    if (timeConstant <= 0.0f) {
        voice.level = voice.inAttack ? voice.targetLevel : 0.0f;
        if (!voice.inAttack) {
            voice.level = 0.0f;
        }
        return;
    }

    // Advance envelope phase
    voice.envelopePhase += dt / timeConstant;

    if (voice.envelopePhase >= 1.0f) {
        if (voice.inAttack) {
            // Attack complete, switch to decay
            voice.inAttack = false;
            voice.envelopePhase = 0.0f;
            voice.level = voice.targetLevel;
        } else {
            // Decay complete
            voice.level = 0.0f;
            voice.envelopePhase = 0.0f;
        }
    } else {
        // Apply curve shaping
        float shapedPhase = applyCurveShape(voice.envelopePhase, curve,
                                            voice.inAttack, ramp);

        if (voice.inAttack) {
            voice.level = shapedPhase * voice.targetLevel;
        } else {
            voice.level = (1.0f - shapedPhase) * voice.targetLevel;
        }
    }
}
```

#### 5. Curve Shaping

```cpp
float GeodeEngine::applyCurveShape(float phase, float curve,
                                    bool isAttack, float ramp) const {
    // curve: -1.0 (rectangular) to 0.0 (linear) to +1.0 (sine)

    // Map to existing Curve library
    Curve::Type type;

    if (curve < -0.66f) {
        type = isAttack ? Curve::StepUp : Curve::StepDown;  // Rectangular
    } else if (curve < -0.33f) {
        type = isAttack ? Curve::LogUp : Curve::LogDown;
    } else if (curve < 0.33f) {
        type = isAttack ? Curve::RampUp : Curve::RampDown;  // Linear
    } else if (curve < 0.66f) {
        type = isAttack ? Curve::ExpUp : Curve::ExpDown;
    } else {
        type = Curve::Bell;  // Sinusoidal
    }

    return Curve::eval(type, phase);
}
```

#### 6. Voice Mixing (Analog Max with Offset)

```cpp
float GeodeEngine::computeMixLevel() const {
    // JF MIX algorithm: divide each voice by its index, take max
    float mix = 0.0f;

    for (int i = 0; i < VoiceCount; i++) {
        if (!_voices[i].active) continue;

        float scaled = _voices[i].level / (i + 1);  // Divide by 1-6
        mix = std::max(mix, scaled);                // Analog OR
    }

    return mix;  // 0.0-1.0
}

int16_t GeodeEngine::outputRaw(int16_t offsetRaw) const {
    float mix = computeMixLevel();

    // Interpolate between offset (silence) and 16383 (max)
    // Same pattern as E.* envelope: offset→target based on level
    int16_t targetRaw = 16383;
    int16_t result = offsetRaw + static_cast<int16_t>(mix * (targetRaw - offsetRaw));
    return clamp<int16_t>(result, 0, 16383);
}

// Called by TeletypeTrackEngine to get voltage output
float GeodeEngine::output(int16_t offsetRaw) const {
    int16_t raw = outputRaw(offsetRaw);
    // rawToVolts: 0→-5V, 8192→0V, 16383→+5V
    float norm = raw / 16383.f;
    return norm * 10.f - 5.f;
}
```

**Output Behavior:**
- `G.O 0` (default): silence = -5V, max = +5V (full bipolar swing)
- `G.O 8192`: silence = 0V, max = +5V (positive unipolar)
- `G.O 16383`: silence = +5V, max = +5V (no swing, always max)

---

## TeletypeTrackEngine Integration

### Parameter Storage

```cpp
// In TeletypeTrackEngine.h
class TeletypeTrackEngine : public TrackEngine {
private:
    struct GeodeParams {
        int16_t time;    // 0-16383
        int16_t intone;  // 0-16383 (BIPOLAR: 8192=noon)
        int16_t ramp;    // 0-16383
        int16_t curve;   // 0-16383 (BIPOLAR: 8192=noon)
        int16_t run;     // 0-16383
        int16_t offset;  // 0-16383 (output offset, like E.O)
        uint8_t mode;    // 0-2

        struct VoiceConfig {
            uint8_t divs;      // 1-64
            int16_t repeats;   // -1 to 255
        } voices[6];

        void clear() {
            time = 8192;
            intone = 8192;   // Noon
            ramp = 8192;     // Triangle
            curve = 8192;    // Linear (noon)
            run = 0;
            offset = 0;      // Silence at -5V (full bipolar)
            mode = 0;
            for (int i = 0; i < 6; i++) {
                voices[i].divs = 1;
                voices[i].repeats = 0;
            }
        }
    };

    GeodeParams _geodeParams;
    GeodeEngine _geodeEngine;
    bool _geodeActive;

    // Conversion helpers
    float normalizeUnipolar(int16_t value) const;
    float normalizeBipolar(int16_t value) const;
    int16_t denormalizeUnipolar(float value) const;
    int16_t denormalizeBipolar(float value) const;
};
```

### Setter/Getter Methods

```cpp
// Setters
void TeletypeTrackEngine::setGeodeTime(int16_t value) {
    _geodeParams.time = clamp(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeIntone(int16_t value) {
    _geodeParams.intone = clamp(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeRamp(int16_t value) {
    _geodeParams.ramp = clamp(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeCurve(int16_t value) {
    _geodeParams.curve = clamp(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeRun(int16_t value) {
    _geodeParams.run = clamp(value, 0, 16383);
}

void TeletypeTrackEngine::setGeodeMode(int16_t value) {
    _geodeParams.mode = clamp(value, 0, 2);
}

void TeletypeTrackEngine::setGeodeOffset(int16_t value) {
    _geodeParams.offset = clamp(value, 0, 16383);
}

void TeletypeTrackEngine::triggerGeodeVoice(uint8_t voiceIndex,
                                             int16_t divs, int16_t repeats) {
    if (voiceIndex == 0) {
        // All voices
        for (int i = 0; i < 6; i++) {
            _geodeParams.voices[i].divs = clamp(divs, 1, 64);
            _geodeParams.voices[i].repeats = clamp(repeats, -1, 255);
            _geodeEngine.triggerVoice(i, divs, repeats);
        }
    } else if (voiceIndex >= 1 && voiceIndex <= 6) {
        int i = voiceIndex - 1;
        _geodeParams.voices[i].divs = clamp(divs, 1, 64);
        _geodeParams.voices[i].repeats = clamp(repeats, -1, 255);
        _geodeEngine.triggerVoice(i, divs, repeats);
    }
    _geodeActive = true;
}

// Getters
int16_t TeletypeTrackEngine::getGeodeTime() const {
    return _geodeParams.time;
}

int16_t TeletypeTrackEngine::getGeodeIntone() const {
    return _geodeParams.intone;
}

int16_t TeletypeTrackEngine::getGeodeRamp() const {
    return _geodeParams.ramp;
}

int16_t TeletypeTrackEngine::getGeodeCurve() const {
    return _geodeParams.curve;
}

int16_t TeletypeTrackEngine::getGeodeRun() const {
    return _geodeParams.run;
}

int16_t TeletypeTrackEngine::getGeodeMode() const {
    return _geodeParams.mode;
}

int16_t TeletypeTrackEngine::getGeodeOffset() const {
    return _geodeParams.offset;
}

int16_t TeletypeTrackEngine::getGeodeVal() const {
    // Returns raw value (0-16383) with offset applied
    return _geodeEngine.outputRaw(_geodeParams.offset);
}

int16_t TeletypeTrackEngine::getGeodeVoice(uint8_t voiceIndex) const {
    if (voiceIndex < 1 || voiceIndex > 6) return _geodeParams.offset;
    return _geodeEngine.voiceOutputRaw(voiceIndex - 1, _geodeParams.offset);
}
```

### Update Integration

```cpp
void TeletypeTrackEngine::update(float dt) {
    // ... existing code ...

    if (_geodeActive) {
        // Convert parameters once per update
        float time = normalizeUnipolar(_geodeParams.time);
        float intone = normalizeBipolar(_geodeParams.intone);
        float ramp = normalizeUnipolar(_geodeParams.ramp);
        float curve = normalizeBipolar(_geodeParams.curve);
        float run = normalizeUnipolar(_geodeParams.run);

        // Update GeodeEngine
        _geodeEngine.update(dt, measureFraction(),
                           time, intone, ramp, curve, run,
                           _geodeParams.mode);
    }

    // ... rest of update ...
}
```

---

## TeletypeBridge Implementation

### C Interface Functions

```cpp
// In TeletypeBridge.cpp (extern "C" section)

// Global parameter setters
void tele_g_time(int16_t value) {
    auto *engine = g_activeEngine;
    if (!engine) return;
    engine->setGeodeTime(value);
}

void tele_g_intone(int16_t value) {
    auto *engine = g_activeEngine;
    if (!engine) return;
    engine->setGeodeIntone(value);
}

void tele_g_ramp(int16_t value) {
    auto *engine = g_activeEngine;
    if (!engine) return;
    engine->setGeodeRamp(value);
}

void tele_g_curve(int16_t value) {
    auto *engine = g_activeEngine;
    if (!engine) return;
    engine->setGeodeCurve(value);
}

void tele_g_run(int16_t value) {
    auto *engine = g_activeEngine;
    if (!engine) return;
    engine->setGeodeRun(value);
}

void tele_g_mode(int16_t value) {
    auto *engine = g_activeEngine;
    if (!engine) return;
    engine->setGeodeMode(value);
}

void tele_g_offset(int16_t value) {
    auto *engine = g_activeEngine;
    if (!engine) return;
    engine->setGeodeOffset(value);
}

// Voice trigger
void tele_g_vox(uint8_t voiceIndex, int16_t divs, int16_t repeats) {
    auto *engine = g_activeEngine;
    if (!engine) return;
    engine->triggerGeodeVoice(voiceIndex, divs, repeats);
}

// Global parameter getters
int16_t tele_g_get_time() {
    auto *engine = g_activeEngine;
    if (!engine) return 8192;
    return engine->getGeodeTime();
}

int16_t tele_g_get_intone() {
    auto *engine = g_activeEngine;
    if (!engine) return 8192;
    return engine->getGeodeIntone();
}

int16_t tele_g_get_ramp() {
    auto *engine = g_activeEngine;
    if (!engine) return 8192;
    return engine->getGeodeRamp();
}

int16_t tele_g_get_curve() {
    auto *engine = g_activeEngine;
    if (!engine) return 8192;
    return engine->getGeodeCurve();
}

int16_t tele_g_get_run() {
    auto *engine = g_activeEngine;
    if (!engine) return 0;
    return engine->getGeodeRun();
}

int16_t tele_g_get_mode() {
    auto *engine = g_activeEngine;
    if (!engine) return 0;
    return engine->getGeodeMode();
}

int16_t tele_g_get_offset() {
    auto *engine = g_activeEngine;
    if (!engine) return 0;
    return engine->getGeodeOffset();
}

// Output getters
int16_t tele_g_get_val() {
    auto *engine = g_activeEngine;
    if (!engine) return 8192;  // Center value
    return engine->getGeodeVal();
}

int16_t tele_g_get_voice(uint8_t voiceIndex) {
    auto *engine = g_activeEngine;
    if (!engine) return 8192;
    return engine->getGeodeVoice(voiceIndex);
}
```

### Header Declarations

```cpp
// In TeletypeBridge.h
extern "C" {
    // Global parameters (setters)
    void tele_g_time(int16_t value);
    void tele_g_intone(int16_t value);
    void tele_g_ramp(int16_t value);
    void tele_g_curve(int16_t value);
    void tele_g_run(int16_t value);
    void tele_g_mode(int16_t value);
    void tele_g_offset(int16_t value);

    // Voice trigger
    void tele_g_vox(uint8_t voiceIndex, int16_t divs, int16_t repeats);

    // Getters
    int16_t tele_g_get_time();
    int16_t tele_g_get_intone();
    int16_t tele_g_get_ramp();
    int16_t tele_g_get_curve();
    int16_t tele_g_get_run();
    int16_t tele_g_get_mode();
    int16_t tele_g_get_offset();
    int16_t tele_g_get_val();
    int16_t tele_g_get_voice(uint8_t voiceIndex);
}
```

---

## Teletype Ops Implementation

### In teletype/src/ops/hardware.h

```c
// Declarations
extern const tele_op_t op_G_TIME;
extern const tele_op_t op_G_TONE;
extern const tele_op_t op_G_RAMP;
extern const tele_op_t op_G_CURV;
extern const tele_op_t op_G_RUN;
extern const tele_op_t op_G_MODE;
extern const tele_op_t op_G_O;
extern const tele_op_t op_G_V;
extern const tele_op_t op_G_VAL;
extern const tele_op_t op_G_R;
```

### In teletype/src/ops/hardware.c

**Pattern Reference:** Uses same pattern as CV op (MAKE_GET_SET_OP with separate get/set functions).

```c
// Forward declarations - bidirectional ops need both get and set
static void op_G_TIME_get(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_TIME_set(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_TONE_get(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_TONE_set(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_RAMP_get(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_RAMP_set(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_CURV_get(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_CURV_set(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_RUN_get(const void *data, scene_state_t *ss, exec_state_t *es,
                         command_state_t *cs);
static void op_G_RUN_set(const void *data, scene_state_t *ss, exec_state_t *es,
                         command_state_t *cs);
static void op_G_MODE_get(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_MODE_set(const void *data, scene_state_t *ss, exec_state_t *es,
                          command_state_t *cs);
static void op_G_O_get(const void *data, scene_state_t *ss, exec_state_t *es,
                       command_state_t *cs);
static void op_G_O_set(const void *data, scene_state_t *ss, exec_state_t *es,
                       command_state_t *cs);
static void op_G_V_set(const void *data, scene_state_t *ss, exec_state_t *es,
                       command_state_t *cs);
static void op_G_VAL_get(const void *data, scene_state_t *ss, exec_state_t *es,
                         command_state_t *cs);
static void op_G_R_get(const void *data, scene_state_t *ss, exec_state_t *es,
                       command_state_t *cs);

// Op definitions - bidirectional ops use MAKE_GET_SET_OP
// Pattern: MAKE_GET_SET_OP(name, getter, setter, num_args, returns_value)
const tele_op_t op_G_TIME = MAKE_GET_SET_OP(G.TIME, op_G_TIME_get, op_G_TIME_set, 1, true);
const tele_op_t op_G_TONE = MAKE_GET_SET_OP(G.TONE, op_G_TONE_get, op_G_TONE_set, 1, true);
const tele_op_t op_G_RAMP = MAKE_GET_SET_OP(G.RAMP, op_G_RAMP_get, op_G_RAMP_set, 1, true);
const tele_op_t op_G_CURV = MAKE_GET_SET_OP(G.CURV, op_G_CURV_get, op_G_CURV_set, 1, true);
const tele_op_t op_G_RUN  = MAKE_GET_SET_OP(G.RUN, op_G_RUN_get, op_G_RUN_set, 1, true);
const tele_op_t op_G_MODE = MAKE_GET_SET_OP(G.MODE, op_G_MODE_get, op_G_MODE_set, 1, true);
const tele_op_t op_G_O    = MAKE_GET_SET_OP(G.O, op_G_O_get, op_G_O_set, 1, true);
const tele_op_t op_G_V    = MAKE_SET_OP(G.V, op_G_V_set, 3, false);
const tele_op_t op_G_VAL  = MAKE_GET_OP(G.VAL, op_G_VAL_get, 0, true);
const tele_op_t op_G_R    = MAKE_GET_OP(G.R, op_G_R_get, 2, false);

// ============================================================
// Bidirectional ops: G.TIME, G.TONE, G.RAMP, G.CURV, G.RUN, G.MODE, G.O
// Pattern: separate getter and setter functions
// ============================================================

// G.TIME - base envelope time
static void op_G_TIME_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, tele_g_get_time());
}

static void op_G_TIME_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t value = cs_pop(cs);
    tele_g_time(value);
}

// G.TONE - INTONE spread
static void op_G_TONE_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, tele_g_get_intone());
}

static void op_G_TONE_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t value = cs_pop(cs);
    tele_g_intone(value);
}

// G.RAMP - attack/decay balance
static void op_G_RAMP_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, tele_g_get_ramp());
}

static void op_G_RAMP_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t value = cs_pop(cs);
    tele_g_ramp(value);
}

// G.CURV - curve shape
static void op_G_CURV_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, tele_g_get_curve());
}

static void op_G_CURV_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t value = cs_pop(cs);
    tele_g_curve(value);
}

// G.RUN - physics parameter
static void op_G_RUN_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, tele_g_get_run());
}

static void op_G_RUN_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t value = cs_pop(cs);
    tele_g_run(value);
}

// G.MODE - physics mode
static void op_G_MODE_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, tele_g_get_mode());
}

static void op_G_MODE_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                          exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t value = cs_pop(cs);
    tele_g_mode(value);
}

// G.O - output offset (silence point)
static void op_G_O_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, tele_g_get_offset());
}

static void op_G_O_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t value = cs_pop(cs);
    tele_g_offset(value);
}

// ============================================================
// Setter-only op: G.V (voice trigger)
// ============================================================

static void op_G_V_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    // Teletype pushes args right-to-left, so popping gives left-to-right order
    // G.V voiceIndex divs repeats
    int16_t voiceIndex = cs_pop(cs);
    int16_t divs = cs_pop(cs);
    int16_t repeats = cs_pop(cs);

    tele_g_vox((uint8_t)voiceIndex, divs, repeats);
}

// ============================================================
// Getter-only ops: G.VAL
// ============================================================

// G.VAL - mixed output
static void op_G_VAL_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, tele_g_get_val());
}

// G.R - route mix/voice to CV output
static void op_G_R_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t voiceIndex = cs_pop(cs);
    cs_push(cs, tele_g_get_voice((uint8_t)voiceIndex));
}
```

### In teletype/src/ops/op.c

```c
// Register ops in the op table
&op_G_TIME,
&op_G_TONE,
&op_G_RAMP,
&op_G_CURV,
&op_G_RUN,
&op_G_MODE,
&op_G_O,
&op_G_V,
&op_G_VAL,
&op_G_R,
```

### In teletype/src/ops/op_enum.h

```c
// Add to enum
E_OP_G_TIME,
E_OP_G_TONE,
E_OP_G_RAMP,
E_OP_G_CURV,
E_OP_G_RUN,
E_OP_G_MODE,
E_OP_G_O,
E_OP_G_V,
E_OP_G_VAL,
E_OP_G_R,
```

### In teletype/src/match_token.rl

**Note:** Uses MATCH_OP() not META_OP() - matches existing codebase pattern.

```ragel
"G.TIME" => { MATCH_OP(E_OP_G_TIME); };
"G.TONE" => { MATCH_OP(E_OP_G_TONE); };
"G.RAMP" => { MATCH_OP(E_OP_G_RAMP); };
"G.CURV" => { MATCH_OP(E_OP_G_CURV); };
"G.RUN"  => { MATCH_OP(E_OP_G_RUN); };
"G.MODE" => { MATCH_OP(E_OP_G_MODE); };
"G.O"    => { MATCH_OP(E_OP_G_O); };
"G.V"    => { MATCH_OP(E_OP_G_V); };
"G.VAL"  => { MATCH_OP(E_OP_G_VAL); };
"G.R"    => { MATCH_OP(E_OP_G_R); };
"G.TUNE" => { MATCH_OP(E_OP_G_TUNE); };
```

---

## Implementation Phases

### Phase 1: Core GeodeEngine (Pure C++, No Dependencies) ✅

**Files:**
- `src/apps/sequencer/engine/GeodeEngine.h`
- `src/apps/sequencer/engine/GeodeEngine.cpp`

**Deliverables:**
- GeodeEngine class with voice state
- Phase accumulation logic
- Physics calculation (3 modes)
- Envelope generation (AR with INTONE scaling)
- Curve shaping integration
- Voice mixing (analog max)

**Test:**
- Unit test with hardcoded parameters
- Verify phase wrapping
- Verify physics calculations
- Verify INTONE scaling formula

**Acceptance:**
- `TestGeodeEngine.cpp` passes all cases
- No Performer dependencies (pure standalone)

### Phase 2: TeletypeTrackEngine Integration ✅

**Files:**
- `src/apps/sequencer/engine/TeletypeTrackEngine.h` (modifications)
- `src/apps/sequencer/engine/TeletypeTrackEngine.cpp` (modifications)

**Deliverables:**
- GeodeParams struct
- GeodeEngine instance in TeletypeTrackEngine
- Setter/getter methods
- Parameter conversion helpers
- update() integration
- Per-voice tune ratios stored in GeodeParams (G.TUNE support)

**Test:**
- Call setters directly (bypass Teletype)
- Verify parameter storage
- Verify conversion (0-16383 → normalized)
- Verify output in cvOutput()

**Acceptance:**
- Can trigger voices programmatically
- Mixed output appears in cvOutput()
- Parameters serialize/deserialize correctly

### Phase 3: TeletypeBridge Implementation ✅

**Files:**
- `src/apps/sequencer/engine/TeletypeBridge.h` (modifications)
- `src/apps/sequencer/engine/TeletypeBridge.cpp` (modifications)

**Deliverables:**
- C interface functions (extern "C")
- Bridge between Teletype and TeletypeTrackEngine
- All getter/setter functions
- G.TUNE bridge functions (set + get num/den)

**Test:**
- Call bridge functions from C test
- Verify parameter routing
- Verify active engine selection

**Acceptance:**
- Can call tele_g_time() and see parameter change
- Can call tele_g_get_val() and get output

### Phase 4: Teletype Ops ✅

**Files:**
- `teletype/src/ops/hardware.h` (modifications)
- `teletype/src/ops/hardware.c` (modifications)
- `teletype/src/ops/op.c` (modifications)
- `teletype/src/ops/op_enum.h` (modifications)
- `teletype/src/match_token.rl` (modifications)

**Deliverables:**
- All 11 G.* ops registered (adds G.TUNE)
- Bidirectional getter/setter pattern
- Token matching

**Test:**
- Simple Teletype script test
- Verify each op parses
- Verify parameter flow

**Acceptance:**
- Can execute: `G.TIME 12000`
- Can execute: `X G.TIME`
- Can execute: `G.V 1 4 8`
- Can execute: `CV 1 G.VAL`

### Phase 5: Testing & Refinement

**Deliverables:**
- Comprehensive unit tests
- Integration tests
- Performance profiling
- Documentation

**Test Cases:**
- Single voice quarter notes
- 6-voice polyrhythm
- Infinite repeats
- Mode switching
- INTONE sweep
- Curve shaping
- Parameter modulation

**Performance Targets:**
- update() execution < 100μs (at 1kHz = 10% CPU)
- No memory allocations in update()
- No floating point exceptions

**Acceptance:**
- All test cases pass
- Performance within budget
- No crashes/artifacts in simulator
- Works on hardware

---

## Test Strategy

### Unit Tests

**Location:** `src/tests/unit/sequencer/`

```cpp
// TestGeodeEngine.cpp
UNIT_TEST("GeodeEngine") {

CASE("voice_phase_accumulation") {
    GeodeEngine geo;
    geo.triggerVoice(0, 4, 8);  // Voice 1: quarter notes, 8 repeats

    // Simulate 1/4 measure
    geo.update(0.001f, 0.25f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0);

    expectTrue(geo.voiceActive(0), "Voice should be active");
}

CASE("intone_scaling") {
    GeodeEngine geo;

    // INTONE at noon (0.0) - all voices equal
    geo.update(0.001f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0);
    // Verify equal scaling

    // INTONE at +1.0 - voice 6 faster
    geo.update(0.001f, 0.0f, 0.5f, 1.0f, 0.5f, 0.0f, 0.0f, 0);
    // Verify voice 6 envelope faster than voice 1
}

CASE("physics_transient") {
    GeodeEngine geo;
    geo.triggerVoice(0, 1, 8);

    // Mode 0, RUN 0 - all accents
    geo.update(0.001f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0);
    // Verify all triggers have velocity 1.0
}

CASE("repeats_countdown") {
    GeodeEngine geo;
    geo.triggerVoice(0, 1, 2);  // 2 repeats

    // Trigger 1st
    geo.update(0.001f, 0.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0);
    expectTrue(geo.voiceActive(0), "Active after 1st");

    // Trigger 2nd
    geo.update(0.001f, 1.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0);
    expectTrue(geo.voiceActive(0), "Active after 2nd");

    // Trigger 3rd - should stop
    geo.update(0.001f, 2.0f, 0.5f, 0.0f, 0.5f, 0.0f, 0.0f, 0);
    expectFalse(geo.voiceActive(0), "Inactive after exhausted");
}

} // UNIT_TEST
```

### Integration Tests

**Python-based UI tests:**

```python
# src/apps/sequencer/tests/test_geode.py
def test_geode_basic():
    """Test basic Geode operation via Teletype"""
    # Initialize
    send_teletype("G.TIME 12000")
    send_teletype("G.TONE 8192")
    send_teletype("G.V 1 4 8")

    # Wait for output
    time.sleep(0.5)

    # Read output
    val = send_teletype("G.VAL")
    assert val != 8192  # Should have movement

def test_geode_polyrhythm():
    """Test 6-voice polyrhythm"""
    send_teletype("G.V 1 1 -1")   # Whole notes
    send_teletype("G.V 2 2 -1")   # Half notes
    send_teletype("G.V 3 3 -1")   # Triplets
    send_teletype("G.V 4 4 -1")   # Quarter notes
    send_teletype("G.V 5 5 -1")   # Quintuplets
    send_teletype("G.V 6 6 -1")   # Sextuplets

    # Sample over time
    values = []
    for i in range(100):
        values.append(send_teletype("G.VAL"))
        time.sleep(0.01)

    # Verify variation
    assert len(set(values)) > 10  # Should have many different values
```

### Performance Profiling

```cpp
// In Engine::update() profiler section
#ifdef CONFIG_ENABLE_PROFILER
if (_geodeActive) {
    uint32_t start = os::ticks();
    _geodeEngine.update(dt, measureFraction(), ...);
    uint32_t elapsed = os::ticks() - start;

    // Log if > 100μs
    if (elapsed > os::time::us(100)) {
        DBG("Geode update: %u us", elapsed / os::time::us(1));
    }
}
#endif
```

---

## File Checklist

### New Files
- [x] `src/apps/sequencer/engine/GeodeEngine.h`
- [x] `src/apps/sequencer/engine/GeodeEngine.cpp`
- [ ] `src/tests/unit/sequencer/TestGeodeEngine.cpp`
- [ ] `src/apps/sequencer/tests/test_geode.py` (Python integration tests)

### Modified Files
- [x] `src/apps/sequencer/engine/TeletypeTrackEngine.h`
- [x] `src/apps/sequencer/engine/TeletypeTrackEngine.cpp`
- [x] `src/apps/sequencer/engine/TeletypeBridge.h`
- [x] `src/apps/sequencer/engine/TeletypeBridge.cpp`
- [x] `teletype/src/ops/hardware.h`
- [x] `teletype/src/ops/hardware.c`
- [x] `teletype/src/ops/op.c`
- [x] `teletype/src/ops/op_enum.h`
- [x] `teletype/src/match_token.rl`
- [x] `teletype/src/teletype_io.h` (add tele_g_* extern declarations)
- [ ] `src/tests/unit/sequencer/CMakeLists.txt` (register TestGeodeEngine)

---

## Example Teletype Scripts

### Basic Initialization

```c
SCRIPT 1: INIT
  G.TIME 10000      // Medium-slow envelopes
  G.TONE 8192       // Noon (no intone)
  G.RAMP 8192       // Triangle (equal A/D)
  G.CURV 8192       // Linear
  G.RUN 0           // No physics
  G.MODE 0          // Transient
  G.O 8192          // Silence at 0V (positive unipolar output)
```

### Euclidean Polyrhythm

```c
SCRIPT 2: POLY
  G.V 1 3 -1        // Triplets infinite
  G.V 2 5 -1        // Quintuplets infinite
  G.V 3 7 -1        // Septuplets infinite
  G.V 4 11 -1       // 11-div infinite
  CV 1 G.VAL        // Route to CV 1
```

### Dynamic Modulation

```c
SCRIPT M:
  G.TONE RRAND 0 16383      // Random intone
  G.RUN RRAND 0 8192        // Random physics
  G.MODE RRAND 0 2          // Random mode
```

### Per-Voice Routing

```c
SCRIPT 3: ROUTE
  G.R 1 1           // Voice 1 to CV1
  G.R 2 3           // Voice 3 to CV2
  G.R 3 5           // Voice 5 to CV3
  CV 4 G.VAL        // Mix to CV4
```

### Decay Burst

```c
SCRIPT 4: BURST
  G.MODE 1          // Sustain (decay)
  G.RUN 6000        // Medium decay
  G.V 1 16 16       // 16th notes, 16 repeats
  // Decaying burst of 16 envelopes
```

---

## Memory Budget

### GeodeEngine State

```cpp
struct Voice {
    float phase;              // 4 bytes
    int divs;                 // 4 bytes
    int repeatsTotal;         // 4 bytes
    int repeatsRemaining;     // 4 bytes
    int stepIndex;            // 4 bytes
    bool active;              // 1 byte
    // (3 bytes padding)
    float level;              // 4 bytes
    float targetLevel;        // 4 bytes
    float riseTime;           // 4 bytes
    float fallTime;           // 4 bytes
    float envelopePhase;      // 4 bytes
    bool inAttack;            // 1 byte
    // (3 bytes padding)
};
// Total: 48 bytes per voice

6 voices × 48 bytes = 288 bytes
+ _mixedOutput (4 bytes)
= 292 bytes total
```

### Parameter Storage

```cpp
struct GeodeParams {
    int16_t time;             // 2 bytes
    int16_t intone;           // 2 bytes
    int16_t ramp;             // 2 bytes
    int16_t curve;            // 2 bytes
    int16_t run;              // 2 bytes
    int16_t offset;           // 2 bytes
    uint8_t mode;             // 1 byte
    // (1 byte padding)

    struct VoiceConfig {
        uint8_t divs;         // 1 byte
        int16_t repeats;      // 2 bytes
        // (1 byte padding)
    } voices[6];              // 4 × 6 = 24 bytes
};
// Total: 14 + 24 = 38 bytes
```

**Total Memory:**
- GeodeEngine: 292 bytes
- Parameters: 38 bytes
- **Grand Total: ~330 bytes**

**CPU Budget (estimated):**
- update() target: < 100μs @ 1kHz
- ~10% of 1ms update cycle
- Leaves 90% for other engines

---

## Serialization (Save/Load)

### TeletypeTrack Modifications

```cpp
// In TeletypeTrack::write()
void TeletypeTrack::write(VersionedSerializedWriter &writer) const {
    // ... existing code ...

    // Geode parameters (added at end for backwards compatibility)
    writer.write(_geodeParams.time);
    writer.write(_geodeParams.intone);
    writer.write(_geodeParams.ramp);
    writer.write(_geodeParams.curve);
    writer.write(_geodeParams.run);
    writer.write(_geodeParams.offset);
    writer.write(_geodeParams.mode);

    for (int i = 0; i < 6; i++) {
        writer.write(_geodeParams.voices[i].divs);
        writer.write(_geodeParams.voices[i].repeats);
    }
}

// In TeletypeTrack::read()
void TeletypeTrack::read(VersionedSerializedReader &reader) {
    // ... existing code ...

    // Check version for Geode support
    if (reader.dataVersion() >= YOUR_NEW_VERSION) {
        reader.read(_geodeParams.time);
        reader.read(_geodeParams.intone);
        reader.read(_geodeParams.ramp);
        reader.read(_geodeParams.curve);
        reader.read(_geodeParams.run);
        reader.read(_geodeParams.offset);
        reader.read(_geodeParams.mode);

        for (int i = 0; i < 6; i++) {
            reader.read(_geodeParams.voices[i].divs);
            reader.read(_geodeParams.voices[i].repeats);
        }
    } else {
        // Default values for old projects
        _geodeParams.clear();
    }
}
```

---

## Known Limitations

1. **Control-rate only** - 1kHz update, not audio-rate (by design)
2. **No JF.QT quantization** - Phase wraps are sample-accurate to measure, no additional quantization grid
3. **No JF.TICK** - Uses Performer's transport exclusively, no independent clock
4. **Single track** - Only one TeletypeTrackEngine can host Geode (not a limitation per se, but worth noting)
5. **No voice allocation** - G.V explicitly addresses voices (no JF.NOTE round-robin)

---

## Future Enhancements (Post-v1)

- [ ] Per-voice INTONE override (different spreads per voice)
- [ ] Velocity input modulation (external CV to physics calculation)
- [ ] Envelope shape per-voice (not just global CURV)
- [ ] Quantization grid (JF.QT equivalent)
- [ ] Manual trigger (independent of phase)
- [ ] Trigger output on voice events (gate out)
- [ ] UI page for visual parameter editing
- [ ] Save/recall presets (8 slots?)

---

## Success Criteria

### Minimal Viable Product (MVP)

- [ ] All 9 G.* ops implemented and working
- [ ] 6 voices generate measure-locked envelopes
- [ ] All 3 physics modes function correctly
- [ ] INTONE spreads voice speeds per JF formula
- [ ] Mixed output reaches -5V to +5V range
- [ ] Parameters save/load with projects
- [ ] No crashes in simulator or hardware
- [ ] Performance within budget (< 100μs update)

### Quality Bar

- [ ] Unit tests cover all core functions
- [ ] Integration tests verify Teletype scripts work
- [ ] Documentation complete (this file + inline comments)
- [ ] Code follows existing Performer style
- [ ] No compiler warnings
- [ ] No memory leaks (static allocation only)

---

## Questions / Decisions Log

1. **Q:** Should G.R (routing mix/voice to CV outputs) be in v1?
   **A:** YES - Simple to implement, valuable for advanced users

2. **Q:** RAMP center value - should 8192 be triangle or configurable?
   **A:** 8192 = triangle (matches JF noon position)

3. **Q:** Output scaling - 0-5V or -5V to +5V?
   **A:** Full bipolar -5V to +5V with G.O offset (same pattern as E.O for envelopes).
   Default G.O=0 means silence=-5V, max=+5V. User can set G.O=8192 for silence=0V.

4. **Q:** Curve library - reuse Performer's Curve or custom?
   **A:** Reuse Curve library with wrapper for -1 to +1 mapping

5. **Q:** Voice trigger - should it reset phase?
   **A:** YES - G.V restarts voice from phase 0 (synchronization)

6. **Q:** Transport stopped - what happens to Geode?
   **A:** TBD - Options: freeze (pause envelopes), continue free-running, or mute output

7. **Q:** Invalid voice index (G.V 7 4 8) - behavior?
   **A:** Ignore silently (same as out-of-range CV ops)

8. **Q:** Silent output voltage - 0V or -5V?
   **A:** 0V (center) - updated in mixing algorithm

---

## Open Implementation Questions (Resolved)

1. **Phase delta calculation:** ✅ Resolved - store `_prevMeasureFraction` and handle measure wrap correctly

2. **Curve.h dependency:** ✅ Resolved - Accept dependency (Curve.h is lightweight).

3. **CV output routing:** How exactly does `CV 1 G.VAL` work? G.VAL returns raw 0-16383, CV op converts via rawToVolts().

4. **Op bidirectional pattern:** ✅ Resolved - Use MAKE_GET_SET_OP with separate get/set functions (like CV op).

5. **Output voltage mapping:** ✅ Resolved - Full bipolar -5V to +5V with G.O offset (same pattern as E.O).

---

## Design Note: Shared Slope Helper

**Considered:** Factoring a shared `SlopeState`/`slopeStep()` helper reusable by Geode, E.*, and CurveTrack.

**Decision:** Keep Geode's slope implementation self-contained for v1.

**Rationale:**
- **E.*** uses slew-based CV targeting, not a dedicated slope generator. The "envelope" emerges from state machine toggling between target/offset values. Retrofitting would change its fundamental approach.
- **CurveTrack** is continuous tempo-synced playback (LFO-like), not event-triggered envelopes.
- **Geode** is the natural fit for classic triggered AR envelopes.

**What IS shared:**
- Shape evaluation via `Curve::eval()` - ensures consistent curve feel across Geode and CurveTrack.

**Future consideration:**
- If E.* ever gets refactored, consider adopting a shared `SlopeState` pattern.
- New envelope-like features should evaluate using this shared approach.

---

## References

- **Just Friends Manual:** `jf-tm.md`
- **Original Geode Spec:** `feat-geo.md`
- **Single Geode Spec:** `feat-singlegeo.md`
- **Existing LFO Implementation:** `src/apps/sequencer/engine/TeletypeTrackEngine.cpp` (lines 1211-1281)
- **Existing ENV Implementation:** Similar pattern in TeletypeTrackEngine
- **Curve Library:** `src/apps/sequencer/model/Curve.h/cpp`

---

*End of Implementation Plan*
