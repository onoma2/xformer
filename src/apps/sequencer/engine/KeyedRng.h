#pragma once

#include "core/utils/Random.h"

#include <cstdint>

// Per-cell keyed RNG. Each cell of a generated sequence gets its own RNG state
// derived deterministically from (globalSeed, cellIdx) via a hash mixer.
//
// Foundational invariant for Phase 14B (seed + mutation log): with keyed RNG,
// cell N's output depends only on (seed, N, knobs). No stream-order coupling,
// no replay needed for REGEN_CELL.
//
// Hash: murmur3-style finalizer applied to (seed XOR'd with idx * golden ratio).
// 32-bit, branch-free, ~6 multiplies + shifts. Adequate for musical RNG seeding
// — not crypto. Same idea used in xxhash32 and Marbles.
namespace keyed_rng {

inline uint32_t mix32(uint32_t h) {
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

// Produce a deterministic 32-bit seed from a base seed and a cell index.
// Golden-ratio constant scatters adjacent indices so cells 0,1,2,… don't
// produce neighbouring RNG states.
inline uint32_t cellSeed(uint32_t baseSeed, uint32_t cellIdx) {
    return mix32(baseSeed ^ (cellIdx * 0x9e3779b9u));
}

// Construct a per-cell Random instance.
inline Random forCell(uint32_t baseSeed, uint32_t cellIdx) {
    return Random(cellSeed(baseSeed, cellIdx));
}

// REGEN_CELL keyed seed: mutates cell N's seed deterministically via a
// generation counter. mutationGen 0 = original seed. mutationGen N = Nth
// regeneration of that cell. Other cells (different idx) are unaffected.
inline uint32_t cellSeedAfterRegen(uint32_t baseSeed, uint32_t cellIdx, uint32_t mutationGen) {
    // Fold mutationGen into the high bits so it doesn't collide with neighbour
    // indices. cellIdx ∈ [0, 96) → low 8 bits; mutationGen takes the next 8.
    uint32_t combined = (uint32_t(cellIdx) & 0xffu) | ((mutationGen & 0xffu) << 8);
    return mix32(baseSeed ^ (combined * 0x9e3779b9u));
}

} // namespace keyed_rng
