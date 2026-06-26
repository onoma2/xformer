#include "model/Project.h"
#include "model/ProjectVersion.h"

#include <cstdio>
#include <fstream>

static const char *kPath = "project.pro";
static const char *kBak = "project.pro.bak";

static bool loadProject(Project &project, const char *filename) {
    std::ifstream ifs(filename, std::ios::binary);
    if (!ifs.good()) {
        return false;
    }
    FileHeader header;
    ifs.read(reinterpret_cast<char *>(&header), sizeof(header));
    VersionedSerializedReader reader(
        [&ifs] (void *data, size_t len) { ifs.read(reinterpret_cast<char *>(data), len); },
        ProjectVersion::Latest
    );
    return project.read(reader);
}

static bool saveProject(const Project &project, const char *filename) {
    std::ofstream ofs(filename, std::ios::binary);
    if (!ofs.good()) {
        return false;
    }
    FileHeader header(FileType::Project, 0, project.name());
    ofs.write(reinterpret_cast<const char *>(&header), sizeof(header));
    VersionedSerializedWriter writer(
        [&ofs] (const void *data, size_t len) { ofs.write(reinterpret_cast<const char *>(data), len); },
        ProjectVersion::Latest
    );
    project.write(writer);
    return ofs.good();
}

static void printLayout(const Project &project, const char *title) {
    printf("%s\n", title);
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        const char *name = Track::trackModeName(project.track(i).trackMode());
        printf("  track %d: %s\n", i + 1, name ? name : "?");
    }
}

int main() {
    // 1. Back up the bytes.
    {
        std::ifstream src(kPath, std::ios::binary);
        if (!src.good()) {
            printf("ERROR: cannot open %s\n", kPath);
            return 1;
        }
        std::ofstream dst(kBak, std::ios::binary);
        dst << src.rdbuf();
    }
    printf("backed up %s -> %s\n\n", kPath, kBak);

    // 2. Load.
    Project project;
    if (!loadProject(project, kPath)) {
        printf("ERROR: failed to load %s\n", kPath);
        return 1;
    }

    // 3. Before layout.
    printLayout(project, "BEFORE:");
    printf("\n");

    // 4. Silence teletype tracks by clearing their scripts (init -> no-op).
    int ttCleared = 0;
    for (int i = 0; i < CONFIG_TRACK_COUNT; ++i) {
        auto mode = project.track(i).trackMode();
        if (mode == Track::TrackMode::TeletypeV2) {
            project.track(i).tt2Track().clear();
            ++ttCleared;
        } else if (mode == Track::TrackMode::TeletypeMini) {
            project.track(i).tt2MiniTrack().clear();
            ++ttCleared;
        }
    }
    printf("teletype tracks silenced (script-clear): %d\n", ttCleared);

    // 5. Parents: tracks 6 and 7 (index 5, 6) -> Note with an audible pattern.
    for (int idx : {5, 6}) {
        if (project.track(idx).trackMode() != Track::TrackMode::Note) {
            project.setTrackMode(idx, Track::TrackMode::Note);
        }
        auto &seq = project.track(idx).noteTrack().sequence(0);
        for (int s = 0; s < CONFIG_STEP_COUNT; ++s) {
            seq.step(s).setGate(true);
            seq.step(s).setNote(s % 12);
        }
    }
    printf("parents (tracks 6 & 7) set to Note with 16-step ascending gates\n");

    // 6. Track 8 (index 7) -> Fractal, sources 5/6, armed capture.
    project.setTrackMode(7, Track::TrackMode::Fractal);
    auto &fractal = project.track(7).fractalTrack();
    fractal.setSourceA(5);
    fractal.setSourceB(6);
    fractal.setGateLogic(FractalTrack::GateLogic::A);
    for (auto &seq : fractal.sequences()) {
        seq.setRecordTrigger(1);
        seq.setRecordMode(0);
        seq.setRecordSkip(0);
    }
    printf("track 8 -> Fractal, sourceA=5 sourceB=6, capture armed on all patterns\n\n");

    // 7. Save.
    if (!saveProject(project, kPath)) {
        printf("ERROR: failed to save %s\n", kPath);
        return 1;
    }
    printf("saved %s\n\n", kPath);

    // 8. Verify by reloading.
    Project verify;
    if (!loadProject(verify, kPath)) {
        printf("ERROR: failed to reload %s for verify\n", kPath);
        return 1;
    }
    printLayout(verify, "AFTER (reloaded):");
    auto vmode = verify.track(7).trackMode();
    printf("\nverify: track 8 mode = %s\n", Track::trackModeName(vmode));
    if (vmode == Track::TrackMode::Fractal) {
        const auto &vf = verify.track(7).fractalTrack();
        printf("verify: sourceA=%d sourceB=%d recordTrigger(seq0)=%d\n",
            vf.sourceA(), vf.sourceB(), vf.sequence(0).recordTrigger());
    }
    return 0;
}
