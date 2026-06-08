#include "UnitTest.h"

#include "apps/sequencer/engine/GateRotation.h"

using Combine = RouteApply::Combine;

// Group gate rotation (spec 018). gateRotateAmount maps the source to an int over ±8
// (Modulate centered / Absolute sweep); rotatedGroupMember is legacy's (p - n) mod K.

UNIT_TEST("GateRotation") {

CASE("gateRotateAmount: Modulate is centered, full swing +-8") {
    expectEqual(gateRotationFromSource(0.5f, 100, Combine::Modulate), 0, "center = no rotation");
    expectEqual(gateRotationFromSource(1.0f, 100, Combine::Modulate), 8, "top = +8");
    expectEqual(gateRotationFromSource(0.0f, 100, Combine::Modulate), -8, "bottom = -8");
    expectEqual(gateRotationFromSource(1.0f, 50, Combine::Modulate), 4, "half depth = +4");
}

CASE("gateRotateAmount: Absolute sweeps 0..8 from the source") {
    expectEqual(gateRotationFromSource(0.0f, 100, Combine::Absolute), 0, "bottom = 0");
    expectEqual(gateRotationFromSource(0.5f, 100, Combine::Absolute), 4, "mid = +4");
    expectEqual(gateRotationFromSource(1.0f, 100, Combine::Absolute), 8, "top = +8");
}

CASE("firstMaskedSlot: lowest set track in the group mask") {
    expectEqual(firstMaskedSlot(0b00000001), 0, "track 1");
    expectEqual(firstMaskedSlot(0b00001100), 2, "tracks 3,4 -> first is 3 (slot 2)");
    expectEqual(firstMaskedSlot(0b10000000), 7, "track 8");
    expectEqual(firstMaskedSlot(0), 0, "empty mask -> 0");
}

CASE("rotatedGroupMember: identity, swap, wrap, negative-safe") {
    expectEqual(rotatedGroupMember(0, 0, 3), 0, "N=0 identity p0");
    expectEqual(rotatedGroupMember(2, 0, 3), 2, "N=0 identity p2");
    // 2-track group, N=1 -> swap
    expectEqual(rotatedGroupMember(0, 1, 2), 1, "jack0 sources member1");
    expectEqual(rotatedGroupMember(1, 1, 2), 0, "jack1 sources member0");
    // wrap: N == N + K
    expectEqual(rotatedGroupMember(0, 3, 3), rotatedGroupMember(0, 0, 3), "N=K equals N=0");
    // negative-safe (p - n) can go negative before wrap
    expectEqual(rotatedGroupMember(0, 2, 3), 1, "(0-2) mod 3 = 1");
}

} // UNIT_TEST
