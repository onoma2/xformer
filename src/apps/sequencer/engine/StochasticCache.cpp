#include "StochasticCache.h"
#include "StochasticTrackEngine.h"   // for getDurationFraction
#include "StochasticGenerator.h"     // for generateDegree (burst-child notes)
#include "KeyedRng.h"

#include "model/StochasticSequence.h"
#include "model/StochasticTrack.h"
#include "model/StochasticTypes.h"
#include "model/Scale.h"

#include "core/utils/Random.h"

#include <algorithm>

namespace stochastic_cache {

// Match StochasticGenerator.cpp burst LUTs. Kept in sync by hand for Patch A;
// shared header refactor is out of scope for this patch.
static const int kBurstSpacingLut[] = { 2, 3, 4, 5, 6 };
static const int kBurstSpacingLutSize = 5;
static constexpr uint32_t kMinBurstParentTicks = 96;
static constexpr uint32_t kMinChildGate = 6;

// Encode a duration LUT slot into the 6-bit cell field. Patch A uses the slot
// directly (0..7); higher bits unused. Future patches may pack gateLen as a
// fraction of relTick to a cell-edge encoding.
static uint8_t encodeGateLenAsDurSlot(uint8_t durationIndex) {
    return durationIndex & 0x3fu;
}

int regenerateCacheFromEvents(Cache &cache, const StochasticSequence &seq, uint32_t divisor, uint32_t seed,
                              const Scale *scale, const StochasticTrack *track, int rootNote) {
    (void)rootNote; // unused — generateDegree consumes scale + track only, root is applied at trigger time
    cache.count = 0;
    cache.cycleTicks = 0;
    for (int i = 0; i < kMaxEventSlots; ++i) cache.parentCacheIdx[i] = 0xff;

    const bool bakeChildNotes = (scale != nullptr) && (track != nullptr);
    const int activeNotes = bakeChildNotes ? std::max(1, scale->notesPerOctave()) : 1;

    const int first = std::max(0, std::min(int(seq.first()), int(seq.size()) - 1));
    const int last  = std::max(first, std::min(int(seq.last()),  int(seq.size()) - 1));
    if (last < first) return 0;

    uint32_t cycleTicks = 0;

    for (int i = first; i <= last; ++i) {
        const StochasticSourceEvent &ev = seq.events()[i];

        auto frac = StochasticTrackEngine::getDurationFraction(int(ev.durationIndex()));
        uint32_t parentTicks = (uint64_t(divisor) * frac.num) / frac.den;
        if (parentTicks == 0) parentTicks = 1;

        // Build parent cell. Parents have priority over burst children — the
        // mask filter looks up rank by parentCacheIdx[slot] and falls back to
        // legacy noisy ranks when a slot is unmapped, so silently dropping
        // late parents would mix rank semantics inside one pattern (Codex
        // adversarial review finding #2, 2026-05-22). Even under maximum-
        // burst-density worst cases (16 parents × 5 children = 80 cells), the
        // 64-cell cap is filled parents-first: bursts truncate, parents map.
        if (cache.count < kCellCap) {
            if (i < kMaxEventSlots) {
                cache.parentCacheIdx[i] = cache.count;
            }

            uint32_t relTick = cycleTicks > kMaxRelTick ? kMaxRelTick : cycleTicks;
            uint8_t gateLen = encodeGateLenAsDurSlot(ev.durationIndex());

            cache.cells[cache.count] = CachedCell::make(
                relTick,
                uint8_t(ev.degree()),
                int8_t(ev.octave()),
                gateLen,
                ev.slide(),
                ev.legato());

            cache.aux[cache.count] = CellAux::make(
                /*audible*/   !ev.rest(),
                /*burstChild*/ false,
                /*rank*/      0);

            ++cache.count;
        }

        // Burst children — mirror evaluateChildren's spacing math. Reserve
        // capacity for every remaining parent in the window before adding
        // children, so we never starve a later slot of its parent cell.
        int childCount = int(ev.childCount());
        if (parentTicks < kMinBurstParentTicks) childCount = 0;
        const int remainingParents = last - i;   // not counting current parent
        int childCapacity = kCellCap - int(cache.count) - remainingParents;
        if (childCapacity < 0) childCapacity = 0;
        if (childCount > 0 && childCapacity > 0) {
            int spacingSlot = int(ev.burstRate());
            if (spacingSlot < 0) spacingSlot = 0;
            if (spacingSlot >= kBurstSpacingLutSize) spacingSlot = kBurstSpacingLutSize - 1;
            int spacingDenom = kBurstSpacingLut[spacingSlot];
            float spacing = float(parentTicks) / float(spacingDenom);
            if (spacing < float(kMinChildGate + 1)) spacing = float(kMinChildGate + 1);

            for (int c = 0; c < childCount && c < StochasticTrackEngine::kMaxChildren; ++c) {
                if (childCapacity <= 0) break;
                if (cache.count >= kCellCap) break;
                uint32_t childOffset = uint32_t((c + 1) * spacing);
                uint32_t childGate = std::max(kMinChildGate, uint32_t(spacing * 0.5f));
                if (childOffset + childGate >= parentTicks) {
                    if (childOffset + kMinChildGate < parentTicks) {
                        childGate = parentTicks - childOffset - 1;
                        if (childGate < kMinChildGate) break;
                    } else {
                        break;
                    }
                }

                uint32_t childAbsTick = cycleTicks + childOffset;
                if (childAbsTick > kMaxRelTick) childAbsTick = kMaxRelTick;

                // Encode child gate as fraction of parent duration in 64ths
                // (6-bit slot, 0..63). Avoids the original 63-tick raw clamp
                // (Codex finding 3, 2026-05-22) which truncated long-parent
                // burst gates and skewed audible behavior vs the legacy
                // evaluateChildren path. At trigger time the engine decodes
                // back to ticks via the current parent's durationTicks:
                //   childGateTicks = (cell.gateLen() * parentTicks) / 64
                // Since childGate = parentTicks / (2 * spacingDenom), the
                // fraction is always 1/(2 * spacingDenom) — fits in 6 bits
                // for all spacingDenom ∈ {2..6}. Add 1 to round-half-up so
                // tiny child gates don't decode to zero.
                uint32_t childGateField32 = (childGate * 64u + parentTicks / 2u) / parentTicks;
                if (childGateField32 < 1u) childGateField32 = 1u;
                if (childGateField32 > kMaxGateLen) childGateField32 = kMaxGateLen;
                uint8_t childGateField = uint8_t(childGateField32);

                // Burst child melody fields baked at cache-build (Patch C-1b,
                // 2026-05-22). Parent mode: copy parent's degree+octave.
                // Generate mode: per-cell keyed RNG → generateDegree (scale-
                // aware), then split into degree-within-octave + octave.
                // Without scale/track (cache-build called from a test path),
                // children fall back to degree=0,octave=0.
                uint8_t childDegree = 0;
                uint8_t childOctave = 0;
                if (bakeChildNotes) {
                    if (seq.burstPitch() == StochasticBurstPitch::Generate) {
                        // Use the child's own cell index as the RNG key — child
                        // notes are independent per cell, reproducible from seed.
                        // Cell index for this child is `cache.count` (before push).
                        Random localRng(keyed_rng::cellSeed(seed, uint32_t(cache.count)));
                        int lastDegree = -1;
                        int absDeg = StochasticGenerator::generateDegree(seq, *track, *scale, lastDegree, localRng);
                        if (absDeg < 0) absDeg = 0;
                        childDegree = uint8_t(absDeg % activeNotes);
                        int oct = absDeg / activeNotes;
                        if (oct < 0) oct = 0;
                        if (oct > kMaxOctave) oct = kMaxOctave;
                        childOctave = uint8_t(oct);
                    } else {
                        // Parent mode: child plays parent's note. (Parent's
                        // octave was already clamped 0..31 by event encoding.)
                        childDegree = uint8_t(ev.degree());
                        childOctave = uint8_t(ev.octave());
                    }
                }

                cache.cells[cache.count] = CachedCell::make(
                    childAbsTick,
                    childDegree,
                    childOctave,
                    childGateField,
                    /*slide*/   false,
                    /*legato*/  false);

                cache.aux[cache.count] = CellAux::make(
                    /*audible*/    !ev.rest(),
                    /*burstChild*/ true,
                    /*rank*/       0);

                ++cache.count;
                --childCapacity;
            }
        }

        cycleTicks += parentTicks;
    }

    cache.cycleTicks = uint16_t(std::min(cycleTicks, uint32_t(UINT16_MAX)));

    recomputeCacheRanks(cache, seed, /*tilt*/ 0);
    return cache.count;
}

// Compute deterministic ranks. Parent cells get duration-aware tilt bias;
// burst-child cells inherit their parent's rank concept by being weighted
// the same as their parent's durationIndex (the gateLen field holds it).
void recomputeCacheRanks(Cache &cache, uint32_t seed, int tilt) {
    if (cache.count == 0) return;

    struct Weighted { uint8_t idx; int32_t weight; };
    Weighted weighted[kCellCap];

    const int tiltClamped = std::max(-100, std::min(100, tilt));

    for (uint8_t i = 0; i < cache.count; ++i) {
        const uint8_t durSlot = std::min(uint8_t(7), cache.cells[i].gateLen());
        // longShortAxis in fixed-point: (durSlot - 3.5) ∈ -3.5 .. +3.5
        // Multiply by 2 to get integer values -7..+7.
        const int32_t longShortFP14 = int32_t(durSlot) * 2 - 7;
        const int32_t biasTerm = int32_t(tiltClamped) * longShortFP14 * 100;

        // Hash salt: top byte of keyed seed × small constant. Always positive,
        // smaller magnitude than any non-zero tilt.
        const uint32_t hash = keyed_rng::cellSeed(seed, i);
        const int32_t saltTerm = int32_t(hash >> 24);   // 0..255

        weighted[i] = Weighted{ i, biasTerm + saltTerm };
    }

    std::sort(weighted, weighted + cache.count, [](const Weighted &a, const Weighted &b) {
        return a.weight < b.weight;
    });

    for (uint8_t k = 0; k < cache.count; ++k) {
        cache.aux[weighted[k].idx].rank = k;
    }
}

} // namespace stochastic_cache
