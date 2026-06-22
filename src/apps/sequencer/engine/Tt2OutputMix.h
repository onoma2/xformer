#pragma once

namespace Tt2OutputMix {

// cvAtJack[t] / gateAtJack[t] = track t's CV/gate output for this jack index;
// isTt2[t] = track t is in TeletypeV2 mode. Aggregate only the TT2 tracks.
inline float sumCv(const float *cvAtJack, const bool *isTt2, int nTracks) {
    float s = 0.f;
    for (int t = 0; t < nTracks; ++t) if (isTt2[t]) s += cvAtJack[t];
    return s;
}

inline bool anyGate(const bool *gateAtJack, const bool *isTt2, int nTracks) {
    for (int t = 0; t < nTracks; ++t) if (isTt2[t] && gateAtJack[t]) return true;
    return false;
}

} // namespace Tt2OutputMix
