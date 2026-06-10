#pragma once

#include "Config.h"
#include "Serialize.h"

#include "core/math/Math.h"

#include <array>
#include <cstdint>

// Persisted config for the modulator-driven Geode mode. The runtime GeodeParams
// in TeletypeTrackEngine is never serialized; this is the saved counterpart for
// the modulator front-end. Globals mirror GeodeParams' 0..16383 ranges so the
// handoff to GeodeEngine.update / setVoiceTune is identical. tune is provisioned
// but dormant in the first cut (stored + defaulted, not yet wired or editable).
class GeodeConfig {
public:
    static constexpr int VoiceCount = 6;   // M3..M8 (matches GeodeEngine::VoiceCount)

    GeodeConfig() { clear(); }

    // active: bank in Geode mode. Mutual exclusion with JustF is enforced at the toggle.
    bool active() const { return _active; }
    void setActive(bool active) { _active = active; }

    // globals: 0..16383 normalized (intone/curve bipolar, 8192 = centre)
    int time() const { return _time; }
    void setTime(int v) { _time = clamp(v, 0, 16383); }
    int intone() const { return _intone; }
    void setIntone(int v) { _intone = clamp(v, 0, 16383); }
    int ramp() const { return _ramp; }
    void setRamp(int v) { _ramp = clamp(v, 0, 16383); }
    int curve() const { return _curve; }
    void setCurve(int v) { _curve = clamp(v, 0, 16383); }
    int mode() const { return _mode; }
    void setMode(int v) { _mode = clamp(v, 0, 2); }

    // per-voice rhythm (voice 0..VoiceCount-1)
    int divs(int v) const { return _divs[v]; }
    void setDivs(int v, int divs) { _divs[v] = clamp(divs, 1, 64); }
    int repeats(int v) const { return _repeats[v]; }
    void setRepeats(int v, int r) { _repeats[v] = clamp(r, -1, 255); }

    // per-voice tune ratio (dormant first cut; default voice i = (i+1):1)
    int tuneNumerator(int v) const { return _tuneNum[v]; }
    int tuneDenominator(int v) const { return _tuneDen[v]; }
    void setTune(int v, int num, int den) { _tuneNum[v] = num; _tuneDen[v] = den; }

    void clear() {
        _active = false;
        _time = 8192;
        _intone = 8192;
        _ramp = 8192;
        _curve = 8192;
        _mode = 0;
        for (int i = 0; i < VoiceCount; ++i) {
            _divs[i] = 1;
            _repeats[i] = 0;
            _tuneNum[i] = i + 1;
            _tuneDen[i] = 1;
        }
    }

    void write(VersionedSerializedWriter &writer) const {
        writer.write(_active);
        writer.write(_time);
        writer.write(_intone);
        writer.write(_ramp);
        writer.write(_curve);
        writer.write(_mode);
        for (int i = 0; i < VoiceCount; ++i) writer.write(_divs[i]);
        for (int i = 0; i < VoiceCount; ++i) writer.write(_repeats[i]);
        for (int i = 0; i < VoiceCount; ++i) writer.write(_tuneNum[i]);
        for (int i = 0; i < VoiceCount; ++i) writer.write(_tuneDen[i]);
    }

    void read(VersionedSerializedReader &reader) {
        reader.read(_active);
        reader.read(_time);
        reader.read(_intone);
        reader.read(_ramp);
        reader.read(_curve);
        reader.read(_mode);
        for (int i = 0; i < VoiceCount; ++i) reader.read(_divs[i]);
        for (int i = 0; i < VoiceCount; ++i) reader.read(_repeats[i]);
        for (int i = 0; i < VoiceCount; ++i) reader.read(_tuneNum[i]);
        for (int i = 0; i < VoiceCount; ++i) reader.read(_tuneDen[i]);
    }

private:
    bool _active;
    int16_t _time;
    int16_t _intone;
    int16_t _ramp;
    int16_t _curve;
    uint8_t _mode;
    std::array<uint8_t, VoiceCount> _divs;
    std::array<int16_t, VoiceCount> _repeats;
    std::array<int16_t, VoiceCount> _tuneNum;
    std::array<int16_t, VoiceCount> _tuneDen;
};
