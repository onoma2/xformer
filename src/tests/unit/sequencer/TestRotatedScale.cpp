#include "UnitTest.h"

#include "apps/sequencer/model/Scale.h"
#include "apps/sequencer/model/RotatedScale.h"

#include "core/utils/StringBuilder.h"

#include <cstring>

UNIT_TEST("RotatedScale") {

CASE("rotate 0 is identity vs base") {
    const Scale &major = Scale::get(1);
    RotatedScaleView v(major, 0);
    for (int n = 0; n < 7; ++n) expectEqual(v.noteToVolts(n), major.noteToVolts(n), "identity");
}
CASE("Major rotate 5 equals natural-minor interval set") {
    RotatedScaleView v(Scale::get(1), 5);                 // Aeolian
    const int expect[7] = {0,2,3,5,7,8,10};
    for (int n = 0; n < 7; ++n)
        expectEqual(int(v.noteToVolts(n) * 12.f + 0.5f), expect[n], "aeolian semitone");
}
CASE("noteFromVolts round-trips through rotate") {
    RotatedScaleView v(Scale::get(1), 2);                 // Dorian
    for (int n = 0; n < 7; ++n) expectEqual(v.noteFromVolts(v.noteToVolts(n)), n, "roundtrip");
}
CASE("voltage scale ignores rotate (no-op)") {
    VoltScale volt("V", 0.1f);                            // VoltScale ctor is public (used at Scale.cpp:38)
    expectTrue(!volt.supportsRotation(), "voltage scale not rotatable");
    RotatedScaleView v(volt, 3);
    for (int n = -3; n < 5; ++n) expectEqual(v.noteToVolts(n), volt.noteToVolts(n), "no-op");
}
CASE("rotate wraps modulo on smaller scale") {
    RotatedScaleView v(Scale::get(1), 9);                 // 9 mod 7 == 2
    RotatedScaleView d(Scale::get(1), 2);
    for (int n = 0; n < 7; ++n) expectEqual(v.noteToVolts(n), d.noteToVolts(n), "9 wraps to 2");
}
CASE("noteName names rebased tonic with octave wrap (Major+5 root C)") {
    RotatedScaleView v(Scale::get(1), 5);
    FixedStringBuilder<8> s0; v.noteName(s0, 0, 0, Scale::Long);  // degree 0
    expectEqual(strncmp((const char *)s0, "C", 1), 0, "degree 0 names C, not A");
    FixedStringBuilder<8> s7; v.noteName(s7, 7, 0, Scale::Long);  // degree 7 wraps an octave
    expectTrue(strncmp((const char *)s7, "C", 1) == 0 && strstr((const char *)s7, "+1") != nullptr,
               "degree 7 names C+1");
}

}
