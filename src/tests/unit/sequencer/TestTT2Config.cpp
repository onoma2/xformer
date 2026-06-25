#include "UnitTest.h"

#include "model/TT2Config.h"
#include "model/TeletypeProgram.h"
#include "model/TeletypeRuntime.h"
#include "engine/TT2Evaluator.h"

#include <type_traits>

UNIT_TEST("TT2Config") {

CASE("TT2ConfigFull traits match today's values") {
    expectEqual(TT2ConfigFull::ScriptCount, 10, "ScriptCount");
    expectEqual(TT2ConfigFull::DelayDepth, 64, "DelayDepth");
    expectEqual(TT2ConfigFull::TriggerInputCount, 8, "TriggerInputCount");
    expectEqual(TT2ConfigFull::MetroScript, 8, "MetroScript");
    expectEqual(TT2ConfigFull::InitScript, 9, "InitScript");
    expectEqual(TT2ConfigFull::SceneCount, 1, "SceneCount");
    expectEqual(TT2ConfigFull::PatternCount, 4, "PatternCount");
    expectEqual(TT2ConfigFull::PatternLength, 64, "PatternLength");
}

CASE("TT2ConfigMini traits") {
    expectEqual(TT2ConfigMini::ScriptCount, 3, "ScriptCount");
    expectEqual(TT2ConfigMini::DelayDepth, 8, "DelayDepth");
    expectEqual(TT2ConfigMini::TriggerInputCount, 2, "TriggerInputCount");
    expectEqual(TT2ConfigMini::MetroScript, 2, "MetroScript");
    expectEqual(TT2ConfigMini::InitScript, -1, "InitScript");
    expectEqual(TT2ConfigMini::SceneCount, 4, "SceneCount");
    expectEqual(TT2ConfigMini::PatternCount, 4, "PatternCount");
    expectEqual(TT2ConfigMini::PatternLength, 64, "PatternLength");
}

CASE("TeletypeProgramT<Full> layout is unchanged") {
    static_assert(sizeof(TT2PatternT<TT2ConfigFull>) == 138, "");
    static_assert(sizeof(TeletypeProgramT<TT2ConfigFull>) == 3638, "");
    expect(std::is_trivially_copyable<TT2PatternT<TT2ConfigFull>>::value, "trivially copyable pattern");
    expect(std::is_trivially_copyable<TeletypeProgramT<TT2ConfigFull>>::value, "trivially copyable");
}

CASE("TT2RuntimeT<Full> layout is unchanged") {
    static_assert(sizeof(TT2RuntimeT<TT2ConfigFull>) == 5880, "");
    expect(std::is_trivially_copyable<TT2RuntimeT<TT2ConfigFull>>::value, "trivially copyable");
}

CASE("tt2OpTable<Full> resolves to the global native table") {
    expectEqual((const void *)tt2OpTable<TT2ConfigFull>(),
                (const void *)tt2NativeOpTable, "trait identity");
}

}
