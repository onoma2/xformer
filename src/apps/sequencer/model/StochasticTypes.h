#pragma once

#include "Bitfield.h"

#include <cstdint>

enum class StochasticSourceMode : uint8_t {
    Loop,
    Live,
    Last
};

// Duration LUT shared by engine, cache, and UI. ticks = (divisor * num) / den.
// Labels at divisor = 1/16:
//   0: ×8     → 1/2     5: ×1     → 1/16 (= divisor)
//   1: ×4     → 1/4     6: ×2/3   → 1/16T
//   2: ×3     → 3/16    7: ×1/2   → 1/32
//   3: ×2     → 1/8
//   4: ×4/3   → 1/8T
struct StochasticDurationFraction { uint16_t num; uint16_t den; };
constexpr StochasticDurationFraction kStochasticDurationLut[] = {
    { 8, 1 }, { 4, 1 }, { 3, 1 }, { 2, 1 },
    { 4, 3 }, { 1, 1 }, { 2, 3 }, { 1, 2 },
};
constexpr int kStochasticDurationLutSize =
    int(sizeof(kStochasticDurationLut) / sizeof(kStochasticDurationLut[0]));

inline StochasticDurationFraction stochasticDurationFraction(int index) {
    if (index < 0) index = 0;
    if (index >= kStochasticDurationLutSize) index = kStochasticDurationLutSize - 1;
    return kStochasticDurationLut[index];
}

// Position-only pitch centrality. Higher = more tonal (root, fifth, thirds, etc).
// Triangular kernels around root/half/thirds/quarters; ceiling = root anchor weight.
// Used by the engine trigger-time melody-mask block (maskMelody/tiltMelody).
constexpr int kStochasticPitchCentralityMax = 30;
inline int stochasticPitchCentrality(int degInOct, int N) {
    if (N <= 1) return 0;
    int halfWidth = N / 6;
    if (halfWidth < 1) halfWidth = 1;
    auto kernel = [&](int target, int weight) -> int {
        int dist = degInOct - target;
        if (dist < 0) dist = -dist;
        int wrap = N - dist;
        if (wrap < dist) dist = wrap;
        int w = (halfWidth - dist) * weight / halfWidth;
        return w > 0 ? w : 0;
    };
    int b = 0;
    b += kernel(0,             kStochasticPitchCentralityMax);
    if (N >= 2) b += kernel(N / 2,        20);
    if (N >= 3) b += kernel(N / 3,        10);
    if (N >= 3) b += kernel((2 * N) / 3,  10);
    if (N >= 4) b += kernel(N / 4,         5);
    if (N >= 4) b += kernel((3 * N) / 4,   5);
    return b;
}

// Burst-cluster mode — two orthogonal axes packed into one enum:
//
//   Pitch axis:   Hold = cluster cells share anchor's pitch
//                 Roll = each cluster cell rolls own pitch
//
//   Timing axis:  Fit  = cluster packs into one prev_dur with BurstRate curve
//                 Over = uniform denom from BurstRate, can overflow prev_dur
//
// Values ordered so that bit 0 = Fit/Over, bit 1 = Hold/Roll. Helpers expose
// the two axes individually for downstream branches.
enum class StochasticBurstHold : uint8_t {
    HoldFit,
    HoldOver,
    RollFit,
    RollOver,
    Last
};

inline bool burstHoldIsRoll(StochasticBurstHold m) {
    return m == StochasticBurstHold::RollFit || m == StochasticBurstHold::RollOver;
}
inline bool burstHoldIsFit(StochasticBurstHold m) {
    return m == StochasticBurstHold::HoldFit || m == StochasticBurstHold::RollFit;
}

// MarblesMode
enum class MarblesMode : uint8_t {
    Off,
    On,
    Last
};

/**
 * RAM-packed pair of rhythm and melody source material.
 * This 6-byte record allows storing 17 patterns within the Performer track budget.
 */
struct StochasticStepContent {
    uint8_t bytes[6];

    void clear() {
        for (int i = 0; i < 6; ++i) bytes[i] = 0;
    }

    void clearRhythm() {
        bytes[0] = 0;
        bytes[1] = 0;
        bytes[2] = 0;
        bytes[3] = 0;
    }

    // --- Rhythm Domain (bytes 0..3) ---
    int durationIndex() const { return bytes[0] & 0x0F; }
    void setDurationIndex(int value) { bytes[0] = (bytes[0] & 0xF0) | (value & 0x0F); }

    int densityRank() const { return ((bytes[0] >> 4) & 0x0F) | ((bytes[1] & 0x03) << 4); }
    void setDensityRank(int value) {
        bytes[0] = (bytes[0] & 0x0F) | ((value & 0x0F) << 4);
        bytes[1] = (bytes[1] & 0xFC) | ((value >> 4) & 0x03);
    }

    // burstTails — number of cluster cells AFTER the anchor cell.
    // Storage: 3 bits (0..7). Value 0 = no cluster fires on this step.
    // Total audible cluster cells = anchor + burstTails (range 1..8).
    int burstTails() const { return (bytes[1] >> 2) & 0x07; }
    void setBurstTails(int value) { bytes[1] = (bytes[1] & 0xE3) | ((value & 0x07) << 2); }

    int burstRate() const { return ((bytes[1] >> 5) & 0x07) | ((bytes[2] & 0x0F) << 3); }
    void setBurstRate(int value) {
        bytes[1] = (bytes[1] & 0x1F) | ((value & 0x07) << 5);
        bytes[2] = (bytes[2] & 0xF0) | ((value >> 3) & 0x0F);
    }

    bool rest() const { return (bytes[2] >> 4) & 0x01; }
    void setRest(bool value) { bytes[2] = (bytes[2] & ~(1 << 4)) | ((value ? 1 : 0) << 4); }

    bool legato() const { return (bytes[2] >> 5) & 0x01; }
    void setLegato(bool value) { bytes[2] = (bytes[2] & ~(1 << 5)) | ((value ? 1 : 0) << 5); }

    bool slide() const { return (bytes[2] >> 6) & 0x01; }
    void setSlide(bool value) { bytes[2] = (bytes[2] & ~(1 << 6)) | ((value ? 1 : 0) << 6); }

    // bit 7 of byte 2 — reserved. Was the accent flag (Bernoulli set in
    // generateRhythmEvent vs accentProb). Removed 2026-05-22 because (a) the
    // accent semantic was never read by the audio path, and (b) accentProb's
    // storage was aliased with patienceMelody, so the same knob was driving
    // two independent engine behaviors. Free to repurpose in a later
    // event-struct extension.

    bool rhythmValid() const { return bytes[3] & 0x01; }
    void setRhythmValid(bool value) { bytes[3] = (bytes[3] & ~0x01) | (value ? 1 : 0); }

    // --- Melody Domain (bytes 4..5) ---
    int degree() const { return bytes[4] & 0x7F; }
    void setDegree(int value) { bytes[4] = (bytes[4] & 0x80) | (value & 0x7F); }

    int octave() const { return ((bytes[4] >> 7) & 0x01) | ((bytes[5] & 0x0F) << 1); }
    void setOctave(int value) {
        bytes[4] = (bytes[4] & 0x7F) | ((value & 0x01) << 7);
        bytes[5] = (bytes[5] & 0xF0) | ((value >> 1) & 0x0F);
    }

    bool melodyValid() const { return (bytes[5] >> 4) & 0x01; }
    void setMelodyValid(bool value) { bytes[5] = (bytes[5] & ~(1 << 4)) | ((value ? 1 : 0) << 4); }

    void mergeRhythmFrom(const StochasticStepContent &event) {
        bytes[0] = event.bytes[0];
        bytes[1] = event.bytes[1];
        bytes[2] = event.bytes[2];
        bytes[3] = event.bytes[3];
    }

    void mergeMelodyFrom(const StochasticStepContent &event) {
        bytes[4] = event.bytes[4];
        bytes[5] = event.bytes[5];
    }
};

static_assert(sizeof(StochasticStepContent) == 6, "Event must be exactly 6 bytes");

struct StochasticChildHit {
    // 32 bits (4 bytes)
    union {
        uint32_t raw;
        BitField<uint32_t, 0, 7> degree;    // 0..127
        BitField<uint32_t, 7, 4> octave;    // -8..7
        BitField<uint32_t, 11, 8> offset;   // normalized 0..255 inside parent
        BitField<uint32_t, 19, 8> length;   // normalized 0..255 inside parent
        BitField<uint32_t, 27, 1> gate;
        BitField<uint32_t, 28, 1> slide;
        BitField<uint32_t, 29, 1> accent;
    };

    void clear() { raw = 0; }
};
