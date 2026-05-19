#pragma once

#include "Bitfield.h"

#include <cstdint>

enum class StochasticModeInternal : uint8_t { Loop, Live, Last };

enum class StochasticSourceMode : uint8_t {
    Loop,
    Live,
    Last
};

enum class StochasticBurstPitch : uint8_t { Parent, Generate, Last };

enum class StochasticLevel : uint8_t {
    Core,     // Level 1
    Direct,   // Level 2
    Weights,  // Level 3
    Last
};

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
struct StochasticSourceEvent {
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

    int childCount() const { return (bytes[1] >> 2) & 0x07; }
    void setChildCount(int value) { bytes[1] = (bytes[1] & 0xE3) | ((value & 0x07) << 2); }

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

    bool accent() const { return (bytes[2] >> 7) & 0x01; }
    void setAccent(bool value) { bytes[2] = (bytes[2] & ~(1 << 7)) | ((value ? 1 : 0) << 7); }

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

    void mergeRhythmFrom(const StochasticSourceEvent &event) {
        bytes[0] = event.bytes[0];
        bytes[1] = event.bytes[1];
        bytes[2] = event.bytes[2];
        bytes[3] = event.bytes[3];
    }

    void mergeMelodyFrom(const StochasticSourceEvent &event) {
        bytes[4] = event.bytes[4];
        bytes[5] = event.bytes[5];
    }
};

static_assert(sizeof(StochasticSourceEvent) == 6, "Event must be exactly 6 bytes");

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
