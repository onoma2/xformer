#include "UnitTest.h"

#include "engine/generators/TuringRegister.h"
#include "core/utils/Random.h"

#include <vector>

UNIT_TEST("TuringRegister") {

CASE("probability 0 yields a loop of period length") {
    TuringRegister tr; tr.length = 4; tr.seed(0x6);
    Random rng(1);
    std::vector<uint32_t> out;
    for (int i = 0; i < 12; ++i) out.push_back(tr.clock(rng, 0));
    for (int i = 0; i + 4 < int(out.size()); ++i) {
        expectEqual(int(out[i]), int(out[i + 4]), "period == length at p=0");
    }
}

CASE("probability 255 diverges from the frozen loop") {
    TuringRegister a; a.length = 8; a.seed(0x5A);
    TuringRegister b; b.length = 8; b.seed(0x5A);
    Random ra(7), rb(7);
    bool differs = false;
    for (int i = 0; i < 8; ++i) {
        uint32_t va = a.clock(ra, 0);     // frozen rotation
        uint32_t vb = b.clock(rb, 255);   // always flip
        if (va != vb) differs = true;
    }
    expectTrue(differs, "p=255 diverges from p=0");
}

CASE("same seed + same rng + same probability reproduces") {
    TuringRegister a; a.length = 12; a.seed(0xABCD);
    TuringRegister b; b.length = 12; b.seed(0xABCD);
    Random ra(99), rb(99);
    for (int i = 0; i < 32; ++i) {
        expectEqual(int(a.clock(ra, 128)), int(b.clock(rb, 128)), "deterministic given seed+rng");
    }
}

CASE("output masked to the low length bits") {
    TuringRegister tr; tr.length = 5; tr.seed(0xFFFFFFFF);
    Random rng(3);
    for (int i = 0; i < 64; ++i) {
        uint32_t v = tr.clock(rng, 200);
        expectTrue(v <= 0x1Fu, "value within low `length` bits");
    }
}

CASE("setLength clamps to [3,32]") {
    TuringRegister tr; tr.seed(0xF0);
    Random rng(2);
    tr.setLength(rng, 1);
    expectEqual(int(tr.length), 3, "clamp low to 3");
    tr.setLength(rng, 40);
    expectEqual(int(tr.length), 32, "clamp high to 32");
}

CASE("never sticks at all-zero") {
    TuringRegister tr; tr.length = 4; tr.seed(0x1);
    Random rng(5);
    for (int i = 0; i < 200; ++i) {
        tr.clock(rng, 200);
        expectTrue(tr.reg != 0u, "register never all-zero");
    }
}

}
