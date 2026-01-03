#include "UnitTest.h"

#include "core/io/VersionedSerializedReader.h"
#include "core/io/VersionedSerializedWriter.h"
#include "tests/unit/core/io/MemoryReaderWriter.h"

#include "model/TeletypeTrack.h"
#include "model/ProjectVersion.h"

#include <array>

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
        MemoryReader memoryReader(buffer.data(), written);
        VersionedSerializedReader reader([&memoryReader](void *data, size_t len) {
            memoryReader.read(data, len);
        }, ProjectVersion::Latest);

        TeletypeTrack restored;
        restored.read(reader);

        const tele_command_t *restoredCmd = ss_get_script_command(&restored.state(), 0, 0);
        expectTrue(restoredCmd && restoredCmd->length > 0, "restored line 0 present");
        char lineBuffer[96] = {};
        print_command(restoredCmd, lineBuffer);
        expectEqual(std::strcmp(lineBuffer, "CV 1 N 60"), 0, "line 0 text");

        restoredCmd = ss_get_script_command(&restored.state(), 0, 1);
        expectTrue(restoredCmd && restoredCmd->length > 0, "restored line 1 present");
        std::memset(lineBuffer, 0, sizeof(lineBuffer));
        print_command(restoredCmd, lineBuffer);
        expectEqual(std::strcmp(lineBuffer, "TR.PULSE 1"), 0, "line 1 text");
    }
}
