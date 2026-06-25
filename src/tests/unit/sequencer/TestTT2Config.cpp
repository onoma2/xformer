#include "UnitTest.h"

#include "model/TT2Config.h"

UNIT_TEST("TT2Config") {

CASE("TT2ConfigFull traits match today's values") {
    expectEqual(TT2ConfigFull::ScriptCount, 10, "ScriptCount");
    expectEqual(TT2ConfigFull::DelayDepth, 64, "DelayDepth");
    expectEqual(TT2ConfigFull::TriggerInputCount, 8, "TriggerInputCount");
    expectEqual(TT2ConfigFull::MetroScript, 8, "MetroScript");
    expectEqual(TT2ConfigFull::InitScript, 9, "InitScript");
    expectEqual(TT2ConfigFull::SceneCount, 1, "SceneCount");
}

}
