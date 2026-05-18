#pragma once

#include "Bitfield.h"

#include <cstdint>

enum class StochasticModeInternal : uint8_t { Dice, Realtime, Last };

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
