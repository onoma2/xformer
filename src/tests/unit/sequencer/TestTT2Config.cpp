#include "UnitTest.h"

#include "model/TT2Config.h"
#include "model/TeletypeProgram.h"
#include "model/TeletypeRuntime.h"

#include <type_traits>

UNIT_TEST("TT2Config") {

CASE("TT2ConfigFull traits match today's values") {
    expectEqual(TT2ConfigFull::ScriptCount, 10, "ScriptCount");
    expectEqual(TT2ConfigFull::DelayDepth, 64, "DelayDepth");
    expectEqual(TT2ConfigFull::TriggerInputCount, 8, "TriggerInputCount");
    expectEqual(TT2ConfigFull::MetroScript, 8, "MetroScript");
    expectEqual(TT2ConfigFull::InitScript, 9, "InitScript");
    expectEqual(TT2ConfigFull::SceneCount, 1, "SceneCount");
}

CASE("TeletypeProgramT<Full> layout is unchanged") {
    static_assert(sizeof(TeletypeProgramT<TT2ConfigFull>) == 3638, "");
    expect(std::is_trivially_copyable<TeletypeProgramT<TT2ConfigFull>>::value, "trivially copyable");
}

CASE("TT2RuntimeT<Full> layout is unchanged") {
    static_assert(sizeof(TT2RuntimeT<TT2ConfigFull>) == 5880, "");
    expect(std::is_trivially_copyable<TT2RuntimeT<TT2ConfigFull>>::value, "trivially copyable");
}

}
