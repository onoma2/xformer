#include "UnitTest.h"

#include "engine/TT2TrackEngine.h"

#include "model/Types.h"

extern "C" {
#include "command.h"
#include "tt_parser.h"
}

UNIT_TEST("TeletypeV2TriggerInput") {

    CASE("rising_edge_fires_once") {
        bool prev[TT2_TRIGGER_INPUT_COUNT] = {};
        bool high0[4] = { true, false, false, false };
        expectEqual(int(tt2RisingEdges(high0, prev, 4)), 0x1, "low->high fires bit 0");
        bool stillHigh[4] = { true, false, false, false };
        expectEqual(int(tt2RisingEdges(stillHigh, prev, 4)), 0x0, "held high does not refire");
        bool low[4] = { false, false, false, false };
        expectEqual(int(tt2RisingEdges(low, prev, 4)), 0x0, "high->low does not fire");
        bool reRise[4] = { true, false, false, false };
        expectEqual(int(tt2RisingEdges(reRise, prev, 4)), 0x1, "re-rise fires again");
    }

    CASE("inputs_are_independent") {
        bool prev[TT2_TRIGGER_INPUT_COUNT] = {};
        bool now[4] = { false, true, false, true };  // inputs 1 and 3 rise
        expectEqual(int(tt2RisingEdges(now, prev, 4)), 0xA, "bits 1 and 3 fire");
        expectTrue(prev[1] && prev[3] && !prev[0] && !prev[2], "prev tracks each input");
    }

    CASE("trigger_source_defaults") {
        TeletypeProgram p;
        init(p);
        expectEqual(int(p.triggerSource[0]), int(TT2TriggerSource::CvIn1), "TI0 -> CvIn1");
        expectEqual(int(p.triggerSource[1]), int(TT2TriggerSource::CvIn2), "TI1 -> CvIn2");
        expectEqual(int(p.triggerSource[2]), int(TT2TriggerSource::CvIn3), "TI2 -> CvIn3");
        expectEqual(int(p.triggerSource[3]), int(TT2TriggerSource::CvIn4), "TI3 -> CvIn4");
    }

    CASE("eight_inputs_fire_scripts_0_to_7") {
        // Original-teletype parity: 8 trigger inputs -> scripts 0..7. The engine
        // loop fires triggerScript(i) for each set bit i, so an 8-wide rising
        // edge mask covers all numbered scripts.
        expectEqual(TT2_TRIGGER_INPUT_COUNT, 8, "8 trigger inputs");
        bool prev[TT2_TRIGGER_INPUT_COUNT] = {};
        bool now[8] = { true, true, true, true, true, true, true, true };
        expectEqual(int(tt2RisingEdges(now, prev, 8)), 0xFF, "all 8 rise -> bits 0..7");
        bool held[8] = { true, true, true, true, true, true, true, true };
        expectEqual(int(tt2RisingEdges(held, prev, 8)), 0x00, "held high does not refire");
    }
}
