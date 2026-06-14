#include "UnitTest.h"

#include "engine/TT2OpNames.h"

extern "C" {
#include "ops/op_enum.h"
}

#include <cstring>

UNIT_TEST("TT2OpNames") {

CASE("every op enum has a name (full coverage / drift guard)") {
    for (int e = 0; e < E_OP__LENGTH; ++e) {
        expectTrue(tt2OpName(e) != nullptr, "op enum missing a name");
    }
}

CASE("every mod enum has a name") {
    for (int e = 0; e < E_MOD__LENGTH; ++e) {
        expectTrue(tt2ModName(e) != nullptr, "mod enum missing a name");
    }
}

CASE("spot-check single-spelling names") {
    expectTrue(std::strcmp(tt2OpName(E_OP_A), "A") == 0, "A");
    expectTrue(std::strcmp(tt2OpName(E_OP_MO_SHAPE), "MO.SHAPE") == 0, "MO.SHAPE");
    expectTrue(std::strcmp(tt2OpName(E_OP_G_TIME), "G.TIME") == 0, "G.TIME");
    expectTrue(std::strcmp(tt2ModName(E_MOD_IF), "IF") == 0, "IF");
}

CASE("alias dedup: W.ACT/WR.ACT collapse to one canonical spelling") {
    const char *n = tt2OpName(E_OP_WR_ACT);
    expectTrue(n != nullptr, "WR_ACT has a name");
    expectTrue(std::strcmp(n, "W.ACT") == 0 || std::strcmp(n, "WR.ACT") == 0,
               "one of the two spellings");
}

CASE("out-of-range index returns nullptr") {
    expectTrue(tt2OpName(E_OP__LENGTH) == nullptr, "op length sentinel -> null");
    expectTrue(tt2OpName(-1) == nullptr, "op negative -> null");
    expectTrue(tt2ModName(E_MOD__LENGTH) == nullptr, "mod length sentinel -> null");
    expectTrue(tt2ModName(-1) == nullptr, "mod negative -> null");
}

}
