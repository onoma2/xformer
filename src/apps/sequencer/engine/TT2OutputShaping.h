#pragma once

#include "model/Types.h"
#include "model/Scale.h"

// Per-CV-output shaping for TT2, mirroring the TT1 TeletypeTrackEngine transform:
// base ±5V volts -> range normalize/denormalize -> +offset -> clamp -> quantize.
// Pure (no engine/runtime) so it is unit-testable on its own.
namespace Tt2OutputShaping {

inline float clampVolts(float v) {
    return v < -5.f ? -5.f : (v > 5.f ? 5.f : v);
}

// volts: base value in ±5V. quantizeScale < 0 = off. rootNote < 0 = use projectRoot.
inline float shapeCv(float volts, Types::VoltageRange range, int offsetCenti,
                     int quantizeScale, int rootNote, int projectRoot) {
    const auto &base = Types::voltageRangeInfo(Types::VoltageRange::Bipolar5V);
    const auto &out = Types::voltageRangeInfo(range);
    volts = out.denormalize(base.normalize(volts));
    volts += offsetCenti * 0.01f;
    volts = clampVolts(volts);
    if (quantizeScale >= 0) {
        const Scale &scale = Scale::get(quantizeScale);
        int root = (rootNote < 0) ? projectRoot : rootNote;
        if (scale.isChromatic()) volts -= root * (1.f / 12.f);
        volts = scale.noteToVolts(scale.noteFromVolts(volts));
        if (scale.isChromatic()) volts += root * (1.f / 12.f);
        volts = clampVolts(volts);
    }
    return volts;
}

} // namespace Tt2OutputShaping
