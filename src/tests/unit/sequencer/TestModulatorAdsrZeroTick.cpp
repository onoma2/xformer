#include "UnitTest.h"

#include "apps/sequencer/engine/ModulatorEngine.h"
#include "apps/sequencer/model/Modulator.h"

// Small nonzero attack/decay/release times round to zero envelope ticks
// (ms * CONFIG_PPQN / 2500 == 0 for ms < ~14 at PPQN 192). The `== 0` guards
// miss this. On ARM, divide-by-zero returns 0 rather than trapping, so each
// segment computes 0 progress and the envelope freezes mid-stage instead of
// completing. A sub-tick stage should complete in one tick. Fix clamps ticks
// to >= 1.

UNIT_TEST("ModulatorAdsrZeroTick") {

CASE("sub_tick_attack_completes") {
    Modulator m;
    m.setShape(Modulator::Shape::ADSR);
    m.setAmplitude(127);
    m.setAttack(1); // 1ms -> 0 ticks pre-fix; attack should still complete
    ModulatorEngine engine;
    engine.reset();
    engine.tick(0, 0.001f, m, 0, true); // gate rising -> Attack
    // pre-fix: level stuck at 0; fixed: attack completes to full
    expectEqual(engine.currentValue(0), 127, "sub-tick attack reaches full level");
}

CASE("sub_tick_decay_reaches_sustain") {
    Modulator m;
    m.setShape(Modulator::Shape::ADSR);
    m.setAmplitude(127);
    m.setAttack(0);
    m.setDecay(1); // 0 ticks pre-fix; decay should still reach sustain
    m.setSustain(0);
    ModulatorEngine engine;
    engine.reset();
    engine.tick(0, 0.001f, m, 0, true); // Attack(0) -> Decay
    engine.tick(1, 0.001f, m, 0, true); // Decay
    // pre-fix: stuck at 127; fixed: decays to sustain (0)
    expectEqual(engine.currentValue(0), 0, "sub-tick decay reaches sustain");
}

CASE("sub_tick_release_reaches_idle") {
    Modulator m;
    m.setShape(Modulator::Shape::ADSR);
    m.setAmplitude(127);
    m.setAttack(0);
    m.setDecay(0);
    m.setSustain(64);
    m.setRelease(1); // 0 ticks pre-fix; release should still reach idle
    ModulatorEngine engine;
    engine.reset();
    engine.tick(0, 0.001f, m, 0, true);  // Attack(0) -> Decay
    engine.tick(1, 0.001f, m, 0, true);  // Decay(0) -> Sustain
    engine.tick(2, 0.001f, m, 0, true);  // Sustain (level 64)
    engine.tick(3, 0.001f, m, 0, false); // gate falling -> Release
    // pre-fix: stuck at 64; fixed: releases to 0
    expectEqual(engine.currentValue(0), 0, "sub-tick release reaches idle");
}

}
