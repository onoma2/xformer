#include "UnitTest.h"

#include "apps/sequencer/model/Modulator.h"
#include "apps/sequencer/engine/ModulatorEngine.h"

#include "core/utils/StringBuilder.h"

#include <cmath>
#include <cstring>

// Rate-model rethink (modulator-enhancements slice 1): rate has an explicit
// Free(wall-Hz) / Tempo(clock-division) domain, applied to all shapes. Free reuses
// _rate as centi-Hz (1..1600 = 0.01..16Hz, default 5 = 0.05Hz); Tempo as ticks
// (6..6144). printRate/editRate/setRate are domain-keyed, not shape-keyed.

namespace {
using Domain = Modulator::RateDomain;
bool eq(const StringBuilder &b, const char *s) { return std::strcmp((const char *)b, s) == 0; }
}

UNIT_TEST("Modulator") {

CASE("clear() defaults to Free domain @ 0.05Hz") {
    Modulator m; m.clear();
    expectEqual(int(m.rateDomain()), int(Domain::Free), "Free domain by default");
    expectEqual(m.rate(), 5, "rate centi-Hz 5 = 0.05Hz");
}

CASE("Free rate clamps to centi-Hz 1..1600") {
    Modulator m; m.clear();
    m.setRate(0);    expectEqual(m.rate(), 1, "clamp lo to 1 (0.01Hz)");
    m.setRate(9999); expectEqual(m.rate(), 1600, "clamp hi to 1600 (16Hz)");
}

CASE("rateHz reads the centi-Hz value") {
    Modulator m; m.clear();
    m.setRate(105);
    expectTrue(std::fabs(m.rateHz() - 1.05f) < 1e-4f, "105 centi-Hz -> 1.05Hz");
}

CASE("Tempo rate clamps to ticks 6..6144") {
    Modulator m; m.clear();
    m.setRateDomain(Domain::Tempo);
    m.setRate(0);     expectEqual(m.rate(), 6, "clamp lo to 6 ticks");
    m.setRate(99999); expectEqual(m.rate(), 6144, "clamp hi to 6144 ticks");
}

CASE("printRate: Free shows Hz, Tempo shows clock division") {
    Modulator m; m.clear();
    FixedStringBuilder<16> a; m.setRate(5);    m.printRate(a); expectTrue(eq(a, "0.05Hz"), (const char *)a);
    FixedStringBuilder<16> b; m.setRate(105);  m.printRate(b); expectTrue(eq(b, "1.05Hz"), (const char *)b);
    FixedStringBuilder<16> c; m.setRate(1600); m.printRate(c); expectTrue(eq(c, "16.00Hz"), (const char *)c);
    m.setRateDomain(Domain::Tempo); m.setRate(96);
    FixedStringBuilder<16> d; m.printRate(d); expectTrue(eq(d, "1/4"), (const char *)d);
}

CASE("editRate: Free steps centi-Hz (coarse/fine)") {
    Modulator m; m.clear();
    m.setRate(100);
    m.editRate(1, false);  expectEqual(m.rate(), 105, "coarse +5 centi-Hz");
    m.editRate(-1, true);  expectEqual(m.rate(), 104, "fine -1 centi-Hz");
    m.setRate(1);
    m.editRate(-1, false); expectEqual(m.rate(), 1, "clamps at lo");
}

CASE("editRate: Tempo steps the division table") {
    Modulator m; m.clear();
    m.setRateDomain(Domain::Tempo);
    m.setRate(96);                 // 1/4
    m.editRate(-1, false);         // next slower division
    expectTrue(m.rate() > 96, "moved to a slower (larger-tick) division");
}

CASE("freePhaseIncrement carries the fractional remainder (no drift at slow rates)") {
    float rem = 0.f;
    uint32_t total = 0;
    for (int i = 0; i < 1000; ++i) {            // 1 s at a 1 kHz update rate
        total += ModulatorEngine::freePhaseIncrement(0.001f, 0.05f, rem);
    }
    // 0.05 Hz over 1 s = 0.05 cycle = 0.05 * 65536 = 3276.8 phase units
    expectTrue(total >= 3275 && total <= 3278, "~3277 units/s; remainder prevents truncation drift");
}

CASE("freePhaseIncrement: zero Hz does not advance") {
    float rem = 0.f;
    expectEqual(int(ModulatorEngine::freePhaseIncrement(0.001f, 0.f, rem)), 0, "0 Hz -> 0 step");
}

CASE("cycleRateDomain toggles and re-clamps into the new range") {
    Modulator m; m.clear();        // Free, rate 5
    m.cycleRateDomain();
    expectEqual(int(m.rateDomain()), int(Domain::Tempo), "-> Tempo");
    expectEqual(m.rate(), 6, "centi-Hz 5 re-clamped into ticks (>=6)");
    m.cycleRateDomain();
    expectEqual(int(m.rateDomain()), int(Domain::Free), "-> Free");
}

// --- gate source expansion ---

CASE("gateFromLevel: hysteresis around 0.5 (hi 0.55 / lo 0.45)") {
    using ME = ModulatorEngine;
    expectTrue(ME::gateFromLevel(0.60f, false), "rises above hi");
    expectFalse(ME::gateFromLevel(0.50f, false), "0.5 in band stays low from low");
    expectTrue(ME::gateFromLevel(0.50f, true), "0.5 in band stays high from high");
    expectFalse(ME::gateFromLevel(0.40f, true), "falls below lo");
    expectTrue(ME::gateFromLevel(1.0f, false), "boolean high (gate-out)");
    expectFalse(ME::gateFromLevel(0.0f, true), "boolean low (gate-out)");
}

CASE("gateSourceAllowed: curated set, self excluded") {
    using S = Routing::Source;
    expectTrue(Modulator::gateSourceAllowed(S::GateOut1, 0), "track gate allowed");
    expectTrue(Modulator::gateSourceAllowed(S::CvIn2, 0), "cv-in allowed");
    expectTrue(Modulator::gateSourceAllowed(S::BusCv1, 0), "bus allowed");
    expectTrue(Modulator::gateSourceAllowed(S::Mod4, 0), "other modulator allowed");
    expectFalse(Modulator::gateSourceAllowed(S::Mod1, 0), "self modulator (idx 0 = Mod1) excluded");
    expectFalse(Modulator::gateSourceAllowed(S::CvOut1, 0), "cv-out not in the set");
    expectFalse(Modulator::gateSourceAllowed(S::Midi, 0), "midi not in the set");
    expectFalse(Modulator::gateSourceAllowed(S::None, 0), "none not in the set");
}

CASE("cycleGateSource: steps the curated list and skips self") {
    using S = Routing::Source;
    expectEqual(int(Modulator::cycleGateSource(S::GateOut1, 1, 0)), int(S::GateOut2), "fwd within tracks");
    // self = Mod3 (idx 2): stepping onto Mod3 skips it
    expectEqual(int(Modulator::cycleGateSource(S::Mod2, 1, 2)), int(S::Mod4), "skips self Mod3");
    // backward from first allowed wraps to last allowed
    expectEqual(int(Modulator::cycleGateSource(S::GateOut1, -1, 0)), int(S::Mod8), "wrap back to last");
}

// --- JustF (ochd / Just Friends) rate-link mode ---

CASE("intoneRatio: index-proportional, M1 identity") {
    using ME = ModulatorEngine;
    expectTrue(std::fabs(ME::intoneRatio(0.f, 1) - 1.f) < 1e-5f, "M1 identity at unison");
    expectTrue(std::fabs(ME::intoneRatio(0.f, 8) - 1.f) < 1e-5f, "unison: all = 1");
    expectTrue(std::fabs(ME::intoneRatio(1.f, 2) - 2.f) < 1e-5f, "harmonic +1: M2 = 2x");
    expectTrue(std::fabs(ME::intoneRatio(1.f, 8) - 8.f) < 1e-5f, "harmonic +1: M8 = 8x");
    expectTrue(std::fabs(ME::intoneRatio(-1.f, 2) - 0.5f) < 1e-5f, "subharm -1: M2 = 1/2");
    expectTrue(std::fabs(ME::intoneRatio(-1.f, 8) - (1.f / 8.f)) < 1e-5f, "subharm -1: M8 = 1/8");
    expectTrue(std::fabs(ME::intoneRatio(1.f, 1) - 1.f) < 1e-5f, "M1 always identity");
}

CASE("justfMasterHz: B-clamp on the harmonic side only") {
    using ME = ModulatorEngine;
    expectTrue(std::fabs(ME::justfMasterHz(16.f, 0.f) - 16.f) < 1e-3f, "unison: no clamp");
    expectTrue(std::fabs(ME::justfMasterHz(16.f, 1.f) - 2.f) < 1e-3f, "+1: master capped 16/8 = 2");
    expectTrue(std::fabs(ME::justfMasterHz(16.f, -0.5f) - 16.f) < 1e-3f, "subharm side: no top clamp");
    expectTrue(ME::justfMasterHz(1.f, 1.f) <= 2.f + 1e-3f, "below the cap passes through");
}

CASE("justfEffectiveHz: fastest follower never exceeds 16Hz") {
    using ME = ModulatorEngine;
    expectTrue(ME::justfEffectiveHz(16.f, 1.f, 8) <= 16.f + 1e-3f, "M8 clamped to 16");
    expectTrue(std::fabs(ME::justfEffectiveHz(16.f, 0.f, 5) - 16.f) < 1e-3f, "unison: follower = master");
    expectTrue(ME::justfEffectiveHz(16.f, 1.f, 1) <= 2.f + 1e-3f, "M1 = clamped master at +1");
    expectTrue(ME::justfEffectiveHz(8.f, -1.f, 8) < 8.f, "M8 subharmonic slower than M1");
}

CASE("justfEffectiveHz: cap argument generalizes the ceiling (default = 16, rate unchanged)") {
    using ME = ModulatorEngine;
    // Default cap keeps the shipped rate behavior byte-identical.
    expectTrue(ME::justfEffectiveHz(16.f, 1.f, 8) <= 16.f + 1e-3f, "default cap still 16");
    // A higher cap lets the fastest follower climb past 16 to the new ceiling.
    expectTrue(std::fabs(ME::justfEffectiveHz(100.f, 1.f, 8, 80.f) - 80.f) < 1e-3f, "M8 clamped to cap 80");
    expectTrue(std::fabs(ME::justfMasterHz(100.f, 1.f, 80.f) - 10.f) < 1e-3f, "master = cap/maxRatio = 80/8");
}

CASE("justfEffectiveMs: envelope spread = ms face of the rate helper") {
    using ME = ModulatorEngine;
    expectEqual(ME::justfEffectiveMs(100, 1.f, 1), 100, "M1 identity: unscaled");
    expectEqual(ME::justfEffectiveMs(800, 1.f, 8), 100, "harmonic +1: M8 = 1/8 the time");
    expectEqual(ME::justfEffectiveMs(100, -1.f, 8), 800, "subharmonic -1: M8 = 8x the time");
    expectEqual(ME::justfEffectiveMs(50, 0.f, 5), 50, "unison: unchanged");
}

CASE("justfEffectiveMs: 12ms floor via cap, zero preserved, ratio held") {
    using ME = ModulatorEngine;
    expectEqual(ME::justfEffectiveMs(80, 1.f, 8), 12, "would be 10ms, floored to 12 via the cap");
    expectEqual(ME::justfEffectiveMs(0, 1.f, 8), 0, "zero stage stays instant, no floor");
    // Master-clamp lengthens M1's own stage so the fastest follower lands at the
    // 12ms floor with the 1:8 ratio intact (mirror of the rate-side B-clamp).
    int m1 = ME::justfEffectiveMs(80, 1.f, 1);
    int m8 = ME::justfEffectiveMs(80, 1.f, 8);
    expectEqual(m8, 12, "M8 clamped to 12ms");
    expectEqual(m1, 96, "M1 lengthened to 96ms to preserve the ratio");
    expectTrue(std::fabs(float(m1) / float(m8) - 8.f) < 0.1f, "ratio preserved at 8:1");
}

}
