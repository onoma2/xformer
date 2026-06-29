#include "UnitTest.h"

#include "apps/sequencer/model/Project.h"
#include "apps/sequencer/model/FileDefs.h"
#include "core/io/VersionedSerializedWriter.h"
#include "core/io/VersionedSerializedReader.h"
#include "model/ProjectVersion.h"

#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>

// Drops a real .pro at this host path each run, so it can be inspected / copied
// onto an SD card. Mirrors FileManager::writeProject / readProject byte-for-byte
// (FileHeader magic -> versioned stream -> FnvHash) so it tests the actual magic
// + strict-version logic, not a reimplementation.
static const char *kPath = "/tmp/xformer-test.pro";

static void writeProjectFile(const Project &project, const char *path,
                             uint32_t magic, uint32_t version) {
    std::ofstream ofs(path, std::ios::binary);
    FileHeader header(FileType::Project, 0, project.name());
    header.magic = magic;   // forgeable for the negative cases
    ofs.write(reinterpret_cast<const char *>(&header), sizeof(header));
    VersionedSerializedWriter writer(
        [&ofs](const void *data, size_t sz) { ofs.write(reinterpret_cast<const char *>(data), sz); },
        version);
    project.write(writer);
    writer.writeHash();
}

// Returns the same accept/reject decision FileManager::readProject makes.
static bool readProjectFile(Project &project, const char *path) {
    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    FileHeader header;
    ifs.read(reinterpret_cast<char *>(&header), sizeof(header));
    if (!header.valid() || header.type != FileType::Project) return false;
    VersionedSerializedReader reader(
        [&ifs](void *data, size_t sz) { ifs.read(reinterpret_cast<char *>(data), sz); },
        ProjectVersion::Latest);
    if (reader.dataVersion() != ProjectVersion::Latest) return false;
    return project.read(reader);
}

UNIT_TEST("ProjectFileRoundTrip") {

CASE("write_drops_pro_and_round_trips") {
    Project project;
    project.setName("TESTPRO");
    project.setTempo(57.5f);
    writeProjectFile(project, kPath, FileHeader::Magic, ProjectVersion::Latest);

    Project loaded;
    expectTrue(readProjectFile(loaded, kPath));
    expectTrue(std::strcmp(loaded.name(), "TESTPRO") == 0);
    expectTrue(loaded.tempo() > 57.f && loaded.tempo() < 58.f);
}

CASE("strict_rejects_pre_version36") {
    Project project;
    // Valid magic, but an older data version (35) -> strict guard must reject.
    writeProjectFile(project, "/tmp/xformer-old.pro", FileHeader::Magic, ProjectVersion::Version35);
    Project loaded;
    expectFalse(readProjectFile(loaded, "/tmp/xformer-old.pro"));
}

CASE("rejects_bad_magic") {
    Project project;
    // Current version, but no/foreign magic -> header guard must reject.
    writeProjectFile(project, "/tmp/xformer-nomagic.pro", 0x00000000u, ProjectVersion::Latest);
    Project loaded;
    expectFalse(readProjectFile(loaded, "/tmp/xformer-nomagic.pro"));
}

} // UNIT_TEST("ProjectFileRoundTrip")
