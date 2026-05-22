#include "StochasticCache.h"
#include "StochasticTrackEngine.h"   // for getDurationFraction
#include "KeyedRng.h"

#include "model/StochasticSequence.h"
#include "model/StochasticTypes.h"

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

int regenerateCacheFromEvents(Cache &cache, const StochasticSequence &seq, uint32_t divisor, uint32_t seed) {
    cache.count = 0;
    cache.cycleTicks = 0;

    const int first = std::max(0, std::min(int(seq.first()), int(seq.size()) - 1));
    const int last  = std::max(first, std::min(int(seq.last()),  int(seq.size()) - 1));
    if (last < first) return 0;

    uint32_t cycleTicks = 0;

    for (int i = first; i <= last; ++i) {
        const StochasticSourceEvent &ev = seq.events()[i];

        auto frac = StochasticTrackEngine::getDurationFraction(int(ev.durationIndex()));
        uint32_t parentTicks = (uint64_t(divisor) * frac.num) / frac.den;
        if (parentTicks == 0) parentTicks = 1;

        // Build parent cell.
        if (cache.count < kCellCap) {
            uint32_t relTick = cycleTicks > kMaxRelTick ? kMaxRelTick : cycleTicks;
            int cv = 0;  // Patch A: CV resolution deferred to engine wiring (Patch B).
            uint8_t gateLen = encodeGateLenAsDurSlot(ev.durationIndex());

            cache.cells[cache.count] = CachedCell::make(
                relTick,
                cv,
                gateLen,
                ev.slide(),
                ev.legato());

            cache.aux[cache.count] = CellAux::make(
                /*audible*/   !ev.rest(),
                /*burstChild*/ false,
                /*rank*/      0);

            ++cache.count;
        }

        // Burst children — mirror evaluateChildren's spacing math.
        int childCount = int(ev.childCount());
        if (parentTicks < kMinBurstParentTicks) childCount = 0;
        if (childCount > 0) {
            int spacingSlot = int(ev.burstRate());
            if (spacingSlot < 0) spacingSlot = 0;
            if (spacingSlot >= kBurstSpacingLutSize) spacingSlot = kBurstSpacingLutSize - 1;
            int spacingDenom = kBurstSpacingLut[spacingSlot];
            float spacing = float(parentTicks) / float(spacingDenom);
            if (spacing < float(kMinChildGate + 1)) spacing = float(kMinChildGate + 1);

            for (int c = 0; c < childCount && c < StochasticTrackEngine::kMaxChildren; ++c) {
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

                uint8_t childGateField = uint8_t(std::min(uint32_t(kMaxGateLen), childGate));

                cache.cells[cache.count] = CachedCell::make(
                    childAbsTick,
                    /*cv*/      0,    // Patch A: deferred.
                    childGateField,
                    /*slide*/   false,
                    /*legato*/  false);

                cache.aux[cache.count] = CellAux::make(
                    /*audible*/    !ev.rest(),
                    /*burstChild*/ true,
                    /*rank*/       0);

                ++cache.count;
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
