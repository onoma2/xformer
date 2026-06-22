#include "UnitTest.h"

#include "engine/GeodeEngine.h"

// Proves the host's gate-fire sequence (TT2TrackEngine::hostGeodeTriggerVoice,
// mirroring Engine.cpp's Geode driver) fires a voice immediately — closing the
// coverage gap where the MO/G op tests only assert the op->host dispatch.

UNIT_TEST("GeodeEngineFire") {

CASE("full gate-fire sequence fires a voice immediately") {
    GeodeEngine ge; ge.reset();
    expectTrue(ge.voiceLevel(0) <= 0.0001f, "voice starts silent");

    // The four calls the real host makes, in order:
    ge.triggerVoice(0, 1, 0);              // set up the burst
    ge.setVoicePhase(0, 0.f);              // reset phase
    ge.markVoiceTriggered(0);              // suppress the wrap-retrigger
    ge.triggerImmediate(0, 0.5f, 0.f, 0.5f, 0);  // fire the first envelope now (mode 0)

    // One 20ms tick, measureFraction held at 0 (no phase wrap) — so any level rise
    // comes solely from the immediate trigger's armed envelope.
    ge.update(0.02f, 0.f, 0.5f, 0.f, 0.5f, 0.f, 0.5f, 0);
    expectTrue(ge.voiceLevel(0) > 0.f, "immediate trigger -> voice rising after one tick");
}

CASE("triggerVoice alone does NOT fire immediately") {
    GeodeEngine ge; ge.reset();
    ge.triggerVoice(0, 1, 0);              // burst armed, but no immediate envelope
    ge.update(0.02f, 0.f, 0.5f, 0.f, 0.5f, 0.f, 0.5f, 0);  // same tick, no wrap
    expectTrue(ge.voiceLevel(0) <= 0.0001f, "no fire without the immediate-trigger sequence");
}

}
