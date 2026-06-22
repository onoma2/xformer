#pragma once

#include "core/utils/Random.h"

#include <cstdint>

// Music-Thing-style Turing machine: a circular shift register whose LSB flips
// with probability `p` (0-255) before each rotation. p=0 repeats a fixed loop,
// p=255 is random, in between the loop slowly mutates. Ported from o_c
// util_turing.h, native (takes a Random by reference instead of a global).
struct TuringRegister {
    static constexpr uint8_t kMinLength = 3;
    static constexpr uint8_t kMaxLength = 32;

    uint32_t reg = 0xffffffffu;
    uint8_t length = 16;

    void seed(uint32_t value) {
        reg = value ? value : 0xffffffffu;
    }

    void setLength(Random &rng, uint8_t len) {
        if (len < kMinLength) len = kMinLength;
        else if (len > kMaxLength) len = kMaxLength;
        if (len > length) {
            reg |= (uint32_t(rng.next() & 1u) << length);  // o_c grow guard: don't go all-zero
        }
        length = len;
    }

    // Clock once: maybe flip the LSB, circular-shift right with wrap into the top
    // bit, guard against all-zero. Returns the low `length` bits.
    uint32_t clock(Random &rng, uint8_t probability) {
        uint32_t sr = reg;
        if (probability >= 255 || rng.nextRange(255) < probability) {
            sr ^= 0x1u;
        }
        uint32_t msb = 0x1u << (length - 1);
        if (sr & 0x1u) {
            sr = (sr >> 1) | msb;
        } else {
            sr = (sr >> 1) & ~msb;
        }
        if (!sr) {
            sr = 0x1u << (length - 1);  // never settle on all-zero (a dead state)
        }
        reg = sr;
        uint32_t mask = (length >= 32) ? 0xffffffffu : ((1u << length) - 1u);
        return sr & mask;
    }
};
