#include "UnitTest.h"

#include "engine/TeletypeNativeOps.h"

// Verbatim port of teletype pattern_mode transpose_n_value over the ET table.
// tt2EtTable reference: note0=0, 11=1502, 12=1638, 13=1775, 19=2594, 24=3277.

UNIT_TEST("TT2Transpose") {

CASE("on-grid intervals") {
    expectEqual(int(tt2TransposeSemitones(1638, 1)), 1775, "note12 +1 -> note13");
    expectEqual(int(tt2TransposeSemitones(1638, 7)), 2594, "note12 +fifth -> note19");
    expectEqual(int(tt2TransposeSemitones(1638, 12)), 3277, "note12 +octave -> note24");
    expectEqual(int(tt2TransposeSemitones(1638, -1)), 1502, "note12 -1 -> note11");
}

CASE("note 0 octave up") {
    expectEqual(int(tt2TransposeSemitones(0, 12)), 1638, "note0 +octave -> note12");
}

CASE("off-grid up-transpose quantizes to lower note") {
    // 1640 sits just above note12 (1638); +1 lands on note13 (1775)
    expectEqual(int(tt2TransposeSemitones(1640, 1)), 1775, "off-grid +1 -> note13");
}

CASE("negative value quantizes to note 0") {
    expectEqual(int(tt2TransposeSemitones(-100, 1)), 0, "negative -> note0");
}

CASE("value above the top entry wraps") {
    expectEqual(int(tt2TransposeSemitones(17341, 1)), 0, "above top +1 wraps to note0");
}

}
