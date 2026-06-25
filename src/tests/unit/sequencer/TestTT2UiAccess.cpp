#include "UnitTest.h"

#include "engine/TT2UiAccess.h"
#include "apps/sequencer/model/Project.h"

// Track::setTrackMode is private (Project/ClipBoard friends only); flip the
// mode through Project, matching TestPhaseFluxSequenceSerialization.

UNIT_TEST("TT2UiAccess") {

CASE("count accessors report Full config dimensions") {
    Project project;
    project.setTrackMode(0, Track::TrackMode::TeletypeV2);
    Track &track = project.track(0);
    expectEqual(tt2ScriptCount(track), 10, "ScriptCount");
    expectEqual(tt2TriggerInputCount(track), 8, "TriggerInputCount");
}

CASE("tt2ScriptCommand returns the same pointer as direct access") {
    Project project;
    project.setTrackMode(0, Track::TrackMode::TeletypeV2);
    Track &track = project.track(0);
    for (int s = 0; s < tt2ScriptCount(track); ++s) {
        for (int l = 0; l < TT2_COMMANDS_PER_SCRIPT; ++l) {
            TT2Command *direct = &track.tt2Track().program().scripts[s].commands[l];
            expect(tt2ScriptCommand(track, s, l) == direct, "command pointer identity");
        }
    }
}

CASE("script length round-trips and agrees with direct access") {
    Project project;
    project.setTrackMode(0, Track::TrackMode::TeletypeV2);
    Track &track = project.track(0);
    tt2SetScriptLength(track, 2, 5);
    expectEqual(tt2ScriptLength(track, 2), 5, "set/get length");
    expectEqual(int(track.tt2Track().program().scripts[2].length), 5, "direct length");
    tt2SetScriptLength(track, 2, 0);
    expectEqual(tt2ScriptLength(track, 2), 0, "reset length");
}

CASE("IO-grid accessors round-trip") {
    Project project;
    project.setTrackMode(0, Track::TrackMode::TeletypeV2);
    Track &track = project.track(0);

    tt2SetCvOutputRange(track, 1, Types::VoltageRange::Unipolar5V);
    expect(tt2CvOutputRange(track, 1) == Types::VoltageRange::Unipolar5V, "cvOutputRange");
    expect(track.tt2Track().program().cvOutputRange[1] == Types::VoltageRange::Unipolar5V, "cvOutputRange direct");

    tt2SetCvOutputOffset(track, 3, -250);
    expectEqual(int(tt2CvOutputOffset(track, 3)), -250, "cvOutputOffset");

    tt2SetCvOutputQuantizeScale(track, 0, 4);
    expectEqual(int(tt2CvOutputQuantizeScale(track, 0)), 4, "cvOutputQuantizeScale");

    tt2SetCvOutputRootNote(track, 0, 7);
    expectEqual(int(tt2CvOutputRootNote(track, 0)), 7, "cvOutputRootNote");

    tt2SetTriggerSource(track, 0, TT2TriggerSource::GateOut5);
    expect(tt2TriggerSource(track, 0) == TT2TriggerSource::GateOut5, "triggerSource");

    tt2SetCvInputSource(track, 0, TT2CvInputSource::CvOut2);
    expect(tt2CvInputSource(track, 0) == TT2CvInputSource::CvOut2, "cvInputSource");
}

}
