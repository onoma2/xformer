#include "UnitTest.h"
#include "engine/Tt2OutputMix.h"

UNIT_TEST("Tt2OutputMix") {
    CASE("cv_sum_adds_active_tracks") {
        // per-track CV at jack index j; only "active" (TT2) tracks count
        float cv[8] = {1.0f, 2.0f, 0,0,0,0,0,0};
        bool  tt2[8] = {true, true, false,false,false,false,false,false};
        expectTrue(Tt2OutputMix::sumCv(cv, tt2, 8) == 3.0f, "1+2 from two TT2 tracks");
    }
    CASE("cv_sum_skips_non_tt2") {
        float cv[8] = {1.0f, 9.0f, 0,0,0,0,0,0};
        bool  tt2[8] = {true, false, false,false,false,false,false,false};
        expectTrue(Tt2OutputMix::sumCv(cv, tt2, 8) == 1.0f, "track1 (non-TT2) ignored");
    }
    CASE("gate_or_any_active_tt2_high") {
        bool g[8] = {false, true, false,false,false,false,false,false};
        bool tt2[8] = {true, true, false,false,false,false,false,false};
        expectTrue(Tt2OutputMix::anyGate(g, tt2, 8) == true, "a TT2 gate is high");
        bool tt2b[8] = {true, false, false,false,false,false,false,false};
        expectTrue(Tt2OutputMix::anyGate(g, tt2b, 8) == false, "the only high gate is non-TT2");
    }
}
