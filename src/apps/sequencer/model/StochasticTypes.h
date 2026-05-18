#pragma once

#include "Bitfield.h"

#include <cstdint>

enum class StochasticMode : uint8_t { Dice, Realtime, Last };

enum class StochasticBurstPitch : uint8_t { Parent, Generate, Last };

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

struct StochasticParentEvent {
    // 64 bits (8 bytes)
    union {
        uint32_t raw;
        BitField<uint32_t, 0, 7> degree;      // 0..127
        BitField<uint32_t, 7, 4> octave;      // -4..3 (3 bits + sign bit)
        BitField<uint32_t, 11, 8> rate;       // encoded duration
        BitField<uint32_t, 19, 8> length;     // normalized length
        BitField<uint32_t, 27, 5> unused0;
    } d0;
    union {
        uint32_t raw;
        BitField<uint32_t, 0, 8> childFirst;  // index into children array
        BitField<uint32_t, 8, 8> childCount;
        BitField<uint32_t, 16, 8> densityRank;
        BitField<uint32_t, 24, 1> rest;
        BitField<uint32_t, 25, 1> legato;
        BitField<uint32_t, 26, 1> slide;
        BitField<uint32_t, 27, 1> accent;
        BitField<uint32_t, 28, 1> valid;
    } d1;

    void clear() { d0.raw = 0; d1.raw = 0; }
};


