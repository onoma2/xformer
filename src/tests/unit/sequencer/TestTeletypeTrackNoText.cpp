#include "UnitTest.h"

#include "core/io/VersionedSerializedReader.h"
#include "core/io/VersionedSerializedWriter.h"
#include "tests/unit/core/io/MemoryReaderWriter.h"

#include "model/FileManager.h"
#include "model/Project.h"
#include "model/TeletypeTrack.h"
#include "model/ProjectVersion.h"

#include <array>
#include <cstdio>

#include "../../../platform/sim/sim/Simulator.h"

extern "C" {
#include "command.h"
#include "teletype.h"
}

namespace {
sim::Simulator &ensureSimulator() {
    static sim::Target target{
        []() {},
        []() {},
        []() {},
    };
    static sim::Simulator *simulator = new sim::Simulator(target);
    return *simulator;
}

bool parseLine(const char *text, tele_command_t &out) {
    char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
    tele_error_t error = parse(text, &out, errorMsg);
    if (error != E_OK) {
        return false;
    }
    error = validate(&out, errorMsg);
    return error == E_OK;
}

bool commandToText(scene_state_t &state, int script, int line, char *buffer, size_t size) {
    if (!buffer || size == 0) {
        return false;
    }
    buffer[0] = '\0';
    const tele_command_t *cmd = ss_get_script_command(&state, script, line);
    if (!cmd || cmd->length == 0) {
        return false;
    }
    print_command(cmd, buffer);
    return true;
}
} // namespace

UNIT_TEST("TeletypeTrackNoText") {
    CASE("roundtrip_scripts") {
        (void)ensureSimulator();

        TeletypeTrack track;

        tele_command_t cmd = {};
        expectTrue(parseLine("CV 1 N 60", cmd), "parse line 0");
        ss_overwrite_script_command(&track.state(), 0, 0, &cmd);

        expectTrue(parseLine("TR.PULSE 1", cmd), "parse line 1");
        ss_overwrite_script_command(&track.state(), 0, 1, &cmd);

        std::array<uint8_t, 65536> buffer{};
        MemoryWriter memoryWriter(buffer.data(), buffer.size());
        VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t len) {
            memoryWriter.write(data, len);
        }, ProjectVersion::Latest);

        track.write(writer);

        const size_t written = memoryWriter.bytesWritten();
        MemoryReader memoryReader(buffer.data(), written + 1);
        VersionedSerializedReader reader([&memoryReader](void *data, size_t len) {
            memoryReader.read(data, len);
        }, ProjectVersion::Latest);

        TeletypeTrack restored;
        restored.read(reader);

        char lineBuffer[96] = {};
        expectTrue(commandToText(restored.state(), 0, 0, lineBuffer, sizeof(lineBuffer)), "restored line 0 present");
        expectEqual(std::strcmp(lineBuffer, "CV 1 N 60"), 0, "line 0 text");

        std::memset(lineBuffer, 0, sizeof(lineBuffer));
        expectTrue(commandToText(restored.state(), 0, 1, lineBuffer, sizeof(lineBuffer)), "restored line 1 present");
        expectEqual(std::strcmp(lineBuffer, "TR.PULSE 1"), 0, "line 1 text");
    }

    CASE("project_roundtrip_teletype_binary") {
        (void)ensureSimulator();

        Project project;
        project.setTrackMode(0, Track::TrackMode::Teletype);
        auto &track = project.track(0).teletypeTrack();

        tele_command_t cmd = {};
        expectTrue(parseLine("CV 1 N 60", cmd), "parse line 0");
        ss_overwrite_script_command(&track.state(), 0, 0, &cmd);
        expectTrue(parseLine("TR.PULSE 1", cmd), "parse line 1");
        ss_overwrite_script_command(&track.state(), 0, 1, &cmd);

        std::array<uint8_t, 262144> buffer{};
        MemoryWriter memoryWriter(buffer.data(), buffer.size());
        VersionedSerializedWriter writer([&memoryWriter](const void *data, size_t len) {
            memoryWriter.write(data, len);
        }, ProjectVersion::Latest);
        project.write(writer);

        const size_t written = memoryWriter.bytesWritten();
        MemoryReader memoryReader(buffer.data(), written + 1);
        VersionedSerializedReader reader([&memoryReader](void *data, size_t len) {
            memoryReader.read(data, len);
        }, ProjectVersion::Latest);

        Project restoredProject;
        expectTrue(restoredProject.read(reader), "project read");
        expectEqual(int(restoredProject.track(0).trackMode()), int(Track::TrackMode::Teletype), "track mode teletype");

        auto &restored = restoredProject.track(0).teletypeTrack();
        char lineBuffer[96] = {};
        expectTrue(commandToText(restored.state(), 0, 0, lineBuffer, sizeof(lineBuffer)), "restored line 0 present");
        expectEqual(std::strcmp(lineBuffer, "CV 1 N 60"), 0, "project line 0 text");
        std::memset(lineBuffer, 0, sizeof(lineBuffer));
        expectTrue(commandToText(restored.state(), 0, 1, lineBuffer, sizeof(lineBuffer)), "restored line 1 present");
        expectEqual(std::strcmp(lineBuffer, "TR.PULSE 1"), 0, "project line 1 text");
    }

    CASE("file_script_roundtrip") {
        (void)ensureSimulator();

        if (!FileManager::volumeMounted()) {
            return;
        }

        TeletypeTrack track;
        tele_command_t cmd = {};
        expectTrue(parseLine("CV 1 N 60", cmd), "parse line 0");
        ss_overwrite_script_command(&track.state(), 0, 0, &cmd);
        expectTrue(parseLine("TR.PULSE 1", cmd), "parse line 1");
        ss_overwrite_script_command(&track.state(), 0, 1, &cmd);

        const char *path = "tt_script_test.txt";
        fs::Error writeResult = FileManager::writeTeletypeScript(track, 0, path);
        expectEqual(int(writeResult), int(fs::OK), "script write");

        TeletypeTrack restored;
        fs::Error readResult = FileManager::readTeletypeScript(restored, 0, path);
        expectEqual(int(readResult), int(fs::OK), "script read");

        char lineBuffer[96] = {};
        expectTrue(commandToText(restored.state(), 0, 0, lineBuffer, sizeof(lineBuffer)), "file line 0 present");
        expectEqual(std::strcmp(lineBuffer, "CV 1 N 60"), 0, "file line 0 text");
        std::memset(lineBuffer, 0, sizeof(lineBuffer));
        expectTrue(commandToText(restored.state(), 0, 1, lineBuffer, sizeof(lineBuffer)), "file line 1 present");
        expectEqual(std::strcmp(lineBuffer, "TR.PULSE 1"), 0, "file line 1 text");

        std::remove(path);
    }
}
