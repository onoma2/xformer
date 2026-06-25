#include "UnitTest.h"
#include "model/TT2MiniTrack.h"

UNIT_TEST("TT2MiniTrack") {
    CASE("fits the Track union budget") {
        static_assert(sizeof(TT2MiniTrack) <= 9520, "");
        expect(true, "");
    }
    CASE("program(scene) wraps modulo SceneCount") {
        TT2MiniTrack t;
        expect(&t.program(0) == &t.program(TT2ConfigMini::SceneCount), "wrap");
        expect(&t.program(1) != &t.program(0), "distinct scenes");
    }
}
