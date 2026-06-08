#pragma once

#include "model/RouteApply.h"

#include <cmath>
#include <cstdint>

// Group gate-output rotation (spec 018). The engine reads one (mask, amount) per frame and
// rotates the masked group's jacks together, instead of each jack reading its own track's
// value. Cycle math + ±8 range are legacy's; only the single-amount collapse is new.

// Source -> integer rotation over the legacy ±8 range (Modulate centered / Absolute sweep).
inline int gateRotationFromSource(float source, int depthPct, RouteApply::Combine combine) {
    return int(std::lround(RouteApply::delta(source, 1.f, combine, depthPct, 8.f)));
}

// Lowest set track in the group mask — the slot whose depth supplies the group amount.
// Returns 0 for an empty mask (inactive — handler only runs on a routed, non-zero mask).
inline int firstMaskedSlot(uint8_t mask) {
    for (int i = 0; i < 8; ++i) {
        if (mask & (1 << i)) return i;
    }
    return 0;
}

// Which group member feeds the jack at group-position p, rotated by n: legacy (p - n) mod K.
inline int rotatedGroupMember(int p, int n, int groupSize) {
    int m = (p - n) % groupSize;
    if (m < 0) m += groupSize;
    return m;
}
