#include "UnitTest.h"

#include "engine/TT2ScriptLoader.h"
#include "engine/TT2Runner.h"
#include "engine/TT2MiniTrackEngine.h"
#include "model/TT2Config.h"

// Free-function level: a Mini boot script (script0) running `M <value>` sets the
// metro period. This is the payload the engine's boot-in-update mechanism carries
// (TT2MiniTrackEngine::update runs program.bootScriptIndex on first refresh).
UNIT_TEST("TT2MiniEngine") {

CASE("boot_script_sets_metro_period") {
    TeletypeProgramT<TT2ConfigMini> program;
    TT2RuntimeT<TT2ConfigMini> runtime;
    TT2OutputState output;
    init(program);
    init(runtime);
    init(output);

    expectTrue(loadScriptText(program, 0, "M 250\n") >= 0, "script0 loads");
    runScript(program, runtime, output, uint8_t(0));
    expectEqual(int(runtime.variables.m), 250, "boot script sets metro period");
}

CASE("tt2SceneIndex wraps modulo SceneCount") {
    expectEqual(tt2SceneIndex(0, 4), 0, "");
    expectEqual(tt2SceneIndex(5, 4), 1, "");
    expectEqual(tt2SceneIndex(7, 4), 3, "");
}

} // UNIT_TEST("TT2MiniEngine")
