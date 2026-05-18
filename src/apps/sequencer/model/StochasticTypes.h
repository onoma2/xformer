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

/**
 * RAM-packed pair of rhythm and melody source material.
 * This 8-byte record allows storing 17 patterns within the Performer track budget.
 */
struct StochasticSourceEvent {
    static constexpr uint32_t D0RateMask = 0xffu << 0;
    static constexpr uint32_t D0LengthMask = 0xffu << 8;
    static constexpr uint32_t D0DensityRankMask = 0xffu << 16;
    static constexpr uint32_t D0ChildCountMask = 0xffu << 24;

    static constexpr uint32_t D1RhythmMask = (1u << 13) - 1u;
    static constexpr uint32_t D1MelodyMask = ((1u << 13) - 1u) << 13;

    union {
        uint32_t raw;
        // Rhythm Domain (32 bits)
        BitField<uint32_t, 0, 8> rate;
        BitField<uint32_t, 8, 8> length;
        BitField<uint32_t, 16, 8> densityRank;
        BitField<uint32_t, 24, 8> childCount;
    } d0;
    union {
        uint32_t raw;
        // Rhythm Domain flags
        BitField<uint32_t, 0, 1> rest;
        BitField<uint32_t, 1, 1> legato;
        BitField<uint32_t, 2, 1> slide;
        BitField<uint32_t, 3, 1> accent;
        BitField<uint32_t, 4, 1> rhythmValid;
        BitField<uint32_t, 5, 8> burstRate;
        
        // Melody Domain (13 bits)
        BitField<uint32_t, 13, 7> degree;
        BitField<uint32_t, 20, 5> octave;
        BitField<uint32_t, 25, 1> melodyValid;

        // Reserved (6 bits)
    } d1;

    void clear() { d0.raw = 0; d1.raw = 0; }

    void setRate(int value) {
        d0.raw = (d0.raw & ~D0RateMask) | ((uint32_t(value) & 0xffu) << 0);
    }

    void setLength(int value) {
        d0.raw = (d0.raw & ~D0LengthMask) | ((uint32_t(value) & 0xffu) << 8);
    }

    void setDensityRank(int value) {
        d0.raw = (d0.raw & ~D0DensityRankMask) | ((uint32_t(value) & 0xffu) << 16);
    }

    void setChildCount(int value) {
        d0.raw = (d0.raw & ~D0ChildCountMask) | ((uint32_t(value) & 0xffu) << 24);
    }

    void setRest(bool value) {
        d1.raw = (d1.raw & ~(1u << 0)) | (uint32_t(value) << 0);
    }

    void setLegato(bool value) {
        d1.raw = (d1.raw & ~(1u << 1)) | (uint32_t(value) << 1);
    }

    void setSlide(bool value) {
        d1.raw = (d1.raw & ~(1u << 2)) | (uint32_t(value) << 2);
    }

    void setAccent(bool value) {
        d1.raw = (d1.raw & ~(1u << 3)) | (uint32_t(value) << 3);
    }

    void setRhythmValid(bool value) {
        d1.raw = (d1.raw & ~(1u << 4)) | (uint32_t(value) << 4);
    }

    void setBurstRate(int value) {
        d1.raw = (d1.raw & ~(0xffu << 5)) | ((uint32_t(value) & 0xffu) << 5);
    }

    void setDegree(int value) {
        d1.raw = (d1.raw & ~(0x7fu << 13)) | ((uint32_t(value) & 0x7fu) << 13);
    }

    void setOctave(int value) {
        d1.raw = (d1.raw & ~(0x1fu << 20)) | ((uint32_t(value) & 0x1fu) << 20);
    }

    void setMelodyValid(bool value) {
        d1.raw = (d1.raw & ~(1u << 25)) | (uint32_t(value) << 25);
    }

    void mergeRhythmFrom(const StochasticSourceEvent &event) {
        d0.raw = event.d0.raw;
        d1.raw = (d1.raw & ~D1RhythmMask) | (event.d1.raw & D1RhythmMask);
    }

    void mergeMelodyFrom(const StochasticSourceEvent &event) {
        d1.raw = (d1.raw & ~D1MelodyMask) | (event.d1.raw & D1MelodyMask);
    }
};

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
