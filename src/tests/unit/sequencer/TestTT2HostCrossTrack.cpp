#include "UnitTest.h"

#include "engine/TT2HostCrossTrack.h"
#include "model/Model.h"

UNIT_TEST("TT2HostCrossTrack") {

CASE("mini target: write+read active-scene cell") {
    Model model;
    auto &project = model.project();
    project.setTrackMode(1, Track::TrackMode::TeletypeMini);
    int16_t *cell = tt2CrossPatternCell(model, 1, 0, 5);
    expectTrue(cell != nullptr, "mini cell resolves");
    *cell = 42;
    expectEqual(*tt2CrossPatternCell(model, 1, 0, 5), int16_t(42), "read back");
    expectEqual(project.track(1).tt2MiniTrack().program(0).patterns[0].val[5],
                int16_t(42), "landed in scene 0 pattern 0 idx 5");
}

CASE("negative idx counts back from p.len (NOT clamp-to-0)") {
    Model model;
    auto &project = model.project();
    project.setTrackMode(0, Track::TrackMode::TeletypeV2);
    auto &prog = project.track(0).tt2Track().program();
    prog.patterns[0].len = 16;   // init() defaults len to 0; negative idx only counts back over a non-empty pattern
    int len = prog.patterns[0].len;
    int16_t *cell = tt2CrossPatternCell(model, 0, 0, -1);
    expectTrue(cell != nullptr, "v2 cell resolves");
    expectEqual(cell, &prog.patterns[0].val[len - 1], "idx -1 == len-1");
}

CASE("bank clamps like normalisePn; non-tt2 track → nullptr") {
    Model model;
    auto &project = model.project();
    project.setTrackMode(0, Track::TrackMode::TeletypeV2);
    auto &prog = project.track(0).tt2Track().program();
    expectEqual(tt2CrossPatternCell(model, 0, 99, 0),
                &prog.patterns[TT2ConfigFull::PatternCount - 1].val[0], "bank over → last");
    expectEqual(tt2CrossPatternCell(model, 0, -3, 0),
                &prog.patterns[0].val[0], "bank under → 0");
    project.setTrackMode(2, Track::TrackMode::Note);
    expectTrue(tt2CrossPatternCell(model, 2, 0, 0) == nullptr, "Note track → nullptr");
    expectTrue(tt2CrossPatternCell(model, 99, 0, 0) == nullptr, "OOB track → nullptr");
}

}
