#include "UnitTest.h"
#include "apps/sequencer/engine/MidiTuning.h"

static float semisToCv(float semisAbove60) { return semisAbove60 / 12.f; }

UNIT_TEST("MidiTuning") {
    CASE("on-grid note → bend 0") {
        int n, b; cvToNoteBend(semisToCv(0.f), 2, n, b);
        expectEqual(n, 60, "note"); expectEqual(b, 0, "bend");
    }
    CASE("scaling: +0.3 st → lround(0.3/range · 8192), nearest unchanged") {
        int n, b;
        cvToNoteBend(semisToCv(0.3f), 1, n, b); expectEqual(n, 60); expectEqual(b, 2458);
        cvToNoteBend(semisToCv(0.3f), 2, n, b); expectEqual(n, 60); expectEqual(b, 1229);
    }
    CASE("negative offset → negative bend") {
        int n, b; cvToNoteBend(semisToCv(-0.3f), 1, n, b); expectEqual(n, 60); expectEqual(b, -2458);
    }
    CASE("half-step rounds away from zero (lround)") {
        int n, b; cvToNoteBend(semisToCv(0.5f), 2, n, b);
        expectEqual(n, 61, "61 at .5"); expectEqual(b, -2048, "back down");
    }
    CASE("offset to nearest ≤ half-step → |bend| ≤ 4096 at range 1") {
        int n, b; cvToNoteBend(semisToCv(7.49f), 1, n, b);
        expectTrue(b >= -4096 && b <= 4096, "within half-step");
    }
}
