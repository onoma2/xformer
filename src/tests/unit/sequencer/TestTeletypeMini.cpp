#include "UnitTest.h"
#include "model/TT2Config.h"
#include "model/TeletypeProgram.h"
#include "model/TeletypeRuntime.h"
#include "engine/TT2Evaluator.h"

UNIT_TEST("TeletypeMini") {
CASE("Mini struct sizes fit budget") {
    expect(sizeof(TeletypeProgramT<TT2ConfigMini>) < 1700, "program");
    expect(sizeof(TT2RuntimeT<TT2ConfigMini>) < 2600, "runtime");
}
CASE("Mini op table covers every op Full covers") {
    auto *full = tt2OpTable<TT2ConfigFull>();
    auto *mini = tt2OpTable<TT2ConfigMini>();
    for (int i = 0; i < E_OP__LENGTH; ++i)
        if (full[i]) expect(mini[i] != nullptr, "coverage");
}
}
