#include "FileManager.h"
#include "ProjectVersion.h"
#include "TT2Track.h"

#include "engine/TT2SceneSerializer.h"

#include "core/utils/StringBuilder.h"
#include "core/fs/FileSystem.h"
#include "core/fs/FileWriter.h"
#include "core/fs/FileReader.h"

#include "os/os.h"

#include "Platform.h"   // CCMRAM_BSS

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <new>


uint32_t FileManager::_volumeState = 0;
uint32_t FileManager::_nextVolumeStateCheckTicks = 0;

std::array<FileManager::CachedSlotInfo, 4> FileManager::_cachedSlotInfos;
uint32_t FileManager::_cachedSlotInfoTicket = 0;

FileManager::TaskExecuteCallback FileManager::_taskExecuteCallback;
FileManager::TaskResultCallback FileManager::_taskResultCallback;
volatile uint32_t FileManager::_taskPending;

struct FileTypeInfo {
    const char *dir;
    const char *ext;
};

FileTypeInfo fileTypeInfos[] = {
    { "PROJECTS", "PRO" },
    { "SCALES", "SCA" },
    { "TT2", "TXT" },
    { "TT2SC", "TXT" },
};

static void slotPath(StringBuilder &str, FileType type, int slot) {
    const auto &info = fileTypeInfos[int(type)];
    str("%s/%03d.%s", info.dir, slot + 1, info.ext);
}

static bool readLine(fs::FileReader &reader, char *buffer, size_t max) {
    size_t pos = 0;
    bool got = false;
    char c = '\0';
    while (true) {
        fs::Error err = reader.read(&c, 1);
        if (err == fs::END_OF_FILE) {
            break;
        }
        if (err != fs::OK) {
            return false;
        }
        got = true;
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            break;
        }
        if (pos + 1 < max) {
            buffer[pos++] = c;
        }
    }
    buffer[pos] = '\0';

    if (pos + 1 >= max) {
        while (true) {
            fs::Error err = reader.read(&c, 1);
            if (err == fs::END_OF_FILE || err != fs::OK) {
                break;
            }
            if (c == '\n') {
                break;
            }
        }
    }

    return got || pos > 0;
}

static const char *skipSpace(const char *text) {
    while (text && *text && std::isspace(static_cast<unsigned char>(*text))) {
        ++text;
    }
    return text;
}

static void trimRight(char *text) {
    if (!text) {
        return;
    }
    size_t len = std::strlen(text);
    while (len > 0 && std::isspace(static_cast<unsigned char>(text[len - 1]))) {
        text[len - 1] = '\0';
        --len;
    }
}

static bool readFirstLine(const char *path, char *line, size_t max) {
    if (max == 0) {
        return false;
    }
    fs::File file(path, fs::File::Read);
    if (file.error() != fs::OK) {
        line[0] = '\0';
        return false;
    }
    // Use a static SRAM buffer to keep SD DMA happy (UI stack is in CCM).
    static uint8_t sramBuffer[128];
    size_t lenRead = 0;
    if (file.read(sramBuffer, sizeof(sramBuffer), &lenRead) != fs::OK || lenRead == 0) {
        line[0] = '\0';
        return false;
    }
    size_t i = 0;
    while (i + 1 < max && i < lenRead) {
        char c = static_cast<char>(sramBuffer[i]);
        if (c == '\n' || c == '\r') {
            break;
        }
        line[i] = c;
        ++i;
    }
    line[i] = '\0';
    return true;
}

static bool parseInt(const char *text, int &out) {
    if (!text) {
        return false;
    }
    char *end = nullptr;
    long value = std::strtol(text, &end, 10);
    if (end == text) {
        return false;
    }
    out = static_cast<int>(value);
    return true;
}

void FileManager::init() {
    _volumeState = 0;
    _nextVolumeStateCheckTicks = 0;
    _taskExecuteCallback = nullptr;
    _taskResultCallback = nullptr;
    _taskPending = 0;
}

bool FileManager::volumeAvailable() {
    return _volumeState & Available;
}

bool FileManager::volumeMounted() {
    return _volumeState & Mounted;
}

fs::Error FileManager::format() {
    invalidateAllSlots();
    return fs::volume().format();
}

fs::Error FileManager::writeProject(Project &project, int slot) {
    return writeFile(FileType::Project, slot, [&] (const char *path) {
        auto result = writeProject(project, path);
        if (result == fs::OK) {
            project.setSlot(slot);
            writeLastProject(slot);
        }
        return result;
    });
}

fs::Error FileManager::readProject(Project &project, int slot) {
    return readFile(FileType::Project, slot, [&] (const char *path) {
        auto result = readProject(project, path);
        if (result == fs::OK) {
            project.setSlot(slot);
            writeLastProject(slot);
        }
        return result;
    });
}

fs::Error FileManager::readLastProject(Project &project) {
    int slot;

    auto result = readLastProject(slot);

    if (result == fs::OK && slot >= 0) {
        result = readProject(project, slot);
        project.setAutoLoaded(true);
    }

    return result;
}

fs::Error FileManager::writeUserScale(const UserScale &userScale, int slot) {
    return writeFile(FileType::UserScale, slot, [&] (const char *path) {
        return writeUserScale(userScale, path);
    });
}

fs::Error FileManager::readUserScale(UserScale &userScale, int slot) {
    return readFile(FileType::UserScale, slot, [&] (const char *path) {
        return readUserScale(userScale, path);
    });
}

namespace {
// Single staging program for atomic TT2 scene load — CPU-only parse target in
// CCMRAM (not a DMA target), swapped into the live track only on a clean parse.
CCMRAM_BSS TeletypeProgram gTeletypeLoadScratch;

void tt2FileWrite(void *ctx, const char *data, size_t len) {
    static_cast<fs::FileWriter *>(ctx)->write(data, len);
}
int tt2FileRead(void *ctx) {
    char ch = '\0';
    return static_cast<fs::FileReader *>(ctx)->read(&ch, 1) == fs::OK
        ? int(static_cast<unsigned char>(ch)) : -1;
}
}

fs::Error FileManager::writeTt2Program(const TT2Track &track, const char *name, int slot) {
    return writeFile(FileType::TeletypeV2Program, slot, [&] (const char *path) {
        return writeTt2Program(track, name, path);
    });
}

fs::Error FileManager::readTt2Program(TT2Track &track, int slot) {
    return readFile(FileType::TeletypeV2Program, slot, [&] (const char *path) {
        return readTt2Program(track, path);
    });
}

fs::Error FileManager::writeTt2Program(const TT2Track &track, const char *name, const char *path) {
    fs::FileWriter fileWriter(path);
    if (fileWriter.error() != fs::OK) {
        return fileWriter.error();
    }
    // Leading NAME line for the slot browser; the deserializer ignores any
    // leading non-#-section lines.
    const char *safeName = (name && name[0]) ? name : "TT2";
    FixedStringBuilder<64> nameLine("NAME %s\n", safeName);
    fileWriter.write(static_cast<const char *>(nameLine), std::strlen(nameLine));
    tt2SerializeScene(track.program(), tt2FileWrite, &fileWriter);
    return fileWriter.finish();
}

fs::Error FileManager::readTt2Program(TT2Track &track, const char *path) {
    fs::FileReader fileReader(path);
    if (fileReader.error() != fs::OK) {
        return fileReader.error();
    }
    TeletypeProgram &staging = gTeletypeLoadScratch;
    bool ok = tt2DeserializeScene(staging, tt2FileRead, &fileReader);
    fs::Error error = fileReader.finish();
    // The streaming read intentionally drains to EOF; clean EOF is success, not
    // a failure. Preserve any real I/O error (and parse failure) instead.
    if (error == fs::END_OF_FILE) {
        error = fs::OK;
    }
    if (error == fs::OK && !ok) {
        error = fs::INVALID_DATA;
    }
    if (error == fs::OK) {
        track.program() = staging;  // atomic swap on clean parse
    }
    return error;
}

fs::Error FileManager::writeTt2Script(const TT2Track &track, int scriptIndex, const char *name, int slot) {
    return writeFile(FileType::TeletypeV2Script, slot, [&] (const char *path) {
        return writeTt2Script(track, scriptIndex, name, path);
    });
}

fs::Error FileManager::readTt2Script(TT2Track &track, int scriptIndex, int slot) {
    return readFile(FileType::TeletypeV2Script, slot, [&] (const char *path) {
        return readTt2Script(track, scriptIndex, path);
    });
}

fs::Error FileManager::writeTt2Script(const TT2Track &track, int scriptIndex, const char *name, const char *path) {
    fs::FileWriter fileWriter(path);
    if (fileWriter.error() != fs::OK) {
        return fileWriter.error();
    }
    const char *safeName = (name && name[0]) ? name : "SCRIPT";
    FixedStringBuilder<64> nameLine("NAME %s\n", safeName);
    fileWriter.write(static_cast<const char *>(nameLine), std::strlen(nameLine));
    tt2SerializeScript(track.program(), scriptIndex, tt2FileWrite, &fileWriter);
    return fileWriter.finish();
}

fs::Error FileManager::readTt2Script(TT2Track &track, int scriptIndex, const char *path) {
    fs::FileReader fileReader(path);
    if (fileReader.error() != fs::OK) {
        return fileReader.error();
    }
    TeletypeProgram &staging = gTeletypeLoadScratch;
    bool ok = tt2DeserializeScript(staging, scriptIndex, tt2FileRead, &fileReader);
    fs::Error error = fileReader.finish();
    if (error == fs::END_OF_FILE) {
        error = fs::OK;
    }
    if (error == fs::OK && !ok) {
        error = fs::INVALID_DATA;
    }
    if (error == fs::OK) {
        track.program().scripts[scriptIndex] = staging.scripts[scriptIndex];
    }
    return error;
}

fs::Error FileManager::writeProject(const Project &project, const char *path) {
    fs::FileWriter fileWriter(path);
    if (fileWriter.error() != fs::OK) {
        return fileWriter.error();
    }

    FileHeader header(FileType::Project, 0, project.name());
    fileWriter.write(&header, sizeof(header));

    VersionedSerializedWriter writer(
        [&fileWriter] (const void *data, size_t len) { fileWriter.write(data, len); },
        ProjectVersion::Latest
    );

    project.write(writer);

    return fileWriter.finish();
}

fs::Error FileManager::readProject(Project &project, const char *path) {
    fs::FileReader fileReader(path);
    if (fileReader.error() != fs::OK) {
        return fileReader.error();
    }

    FileHeader header;
    fileReader.read(&header, sizeof(header));
    // Reject foreign / pre-0.8 files (no magic signature, or wrong file type).
    if (!header.valid() || header.type != FileType::Project) {
        return fs::INVALID_DATA;
    }

    VersionedSerializedReader reader(
        [&fileReader] (void *data, size_t len) { fileReader.read(data, len); },
        ProjectVersion::Latest
    );
    // Strict: only the current project version loads — no migration of older data.
    if (reader.dataVersion() != ProjectVersion::Latest) {
        return fs::INVALID_DATA;
    }

    bool success = project.read(reader);

    auto error = fileReader.finish();
    if (error == fs::OK && !success) {
        error = fs::INVALID_CHECKSUM;
    }

    return error;
}

fs::Error FileManager::writeUserScale(const UserScale &userScale, const char *path) {
    fs::FileWriter fileWriter(path);
    if (fileWriter.error() != fs::OK) {
        return fileWriter.error();
    }

    FileHeader header(FileType::UserScale, 0, userScale.name());
    fileWriter.write(&header, sizeof(header));

    VersionedSerializedWriter writer(
        [&fileWriter] (const void *data, size_t len) { fileWriter.write(data, len); },
        ProjectVersion::Latest
    );

    userScale.write(writer);

    return fileWriter.finish();
}

fs::Error FileManager::readUserScale(UserScale &userScale, const char *path) {
    fs::FileReader fileReader(path);
    if (fileReader.error() != fs::OK) {
        return fileReader.error();
    }

    FileHeader header;
    fileReader.read(&header, sizeof(header));

    VersionedSerializedReader reader(
        [&fileReader] (void *data, size_t len) { fileReader.read(data, len); },
        ProjectVersion::Latest
    );

    bool success = userScale.read(reader);

    auto error = fileReader.finish();
    if (error == fs::OK && !success) {
        error = fs::INVALID_CHECKSUM;
    }

    return error;
}

fs::Error FileManager::writeSettings(const Settings &settings, const char *path) {
    fs::FileWriter fileWriter(path);
    if (fileWriter.error() != fs::OK) {
        return fileWriter.error();
    }

    FileHeader header(FileType::Settings, 0, "SETTINGS");
    fileWriter.write(&header, sizeof(header));

    VersionedSerializedWriter writer(
        [&fileWriter] (const void *data, size_t len) { fileWriter.write(data, len); },
        Settings::Version
    );

    settings.write(writer);

    return fileWriter.finish();
}

fs::Error FileManager::readSettings(Settings &settings, const char *path) {
    fs::FileReader fileReader(path);
    if (fileReader.error() != fs::OK) {
        return fileReader.error();
    }

    FileHeader header;
    fileReader.read(&header, sizeof(header));

    VersionedSerializedReader reader(
        [&fileReader] (void *data, size_t len) { fileReader.read(data, len); },
        Settings::Version
    );

    bool success = settings.read(reader);

    auto error = fileReader.finish();
    if (error == fs::OK && !success) {
        error = fs::INVALID_CHECKSUM;
    }

    return error;
}

void FileManager::slotInfo(FileType type, int slot, SlotInfo &info) {
    if (cachedSlot(type, slot, info)) {
        return;
    }

    info.used = false;

    FixedStringBuilder<32> path;
    slotPath(path, type, slot);

    if (fs::exists(path)) {
        if (type == FileType::TeletypeV2Program) {
            FixedStringBuilder<9> fallback("T2%03d", slot + 1);
            info.name[0] = '\0';

            char line[64] = {};
            if (readFirstLine(path, line, sizeof(line))) {
                trimRight(line);
                const char *text = skipSpace(line);
                if (std::strncmp(text, "NAME ", 5) == 0) {
                    text = skipSpace(text + 5);
                    if (*text) {
                        std::strncpy(info.name, text, sizeof(info.name) - 1);
                        info.name[sizeof(info.name) - 1] = '\0';
                    }
                }
            }

            if (info.name[0] == '\0') {
                std::strncpy(info.name, fallback, sizeof(info.name));
            }
            info.name[sizeof(info.name) - 1] = '\0';
            info.used = true;
        } else {
            fs::File file(path, fs::File::Read);
            FileHeader header;
            size_t lenRead;
            if (file.read(&header, sizeof(header), &lenRead) == fs::OK && lenRead == sizeof(header)) {
                header.readName(info.name, sizeof(info.name));
                info.used = true;
            }
        }
    }

    cacheSlot(type, slot, info);
}

bool FileManager::slotUsed(FileType type, int slot) {
    SlotInfo info;
    slotInfo(type, slot, info);
    return info.used;
}

void FileManager::task(TaskExecuteCallback executeCallback, TaskResultCallback resultCallback) {
    _taskExecuteCallback = executeCallback;
    _taskResultCallback = resultCallback;
    _taskPending = 1;
}

void FileManager::processTask() {
    // check volume availability & mount
    uint32_t ticks = os::ticks();
    if (ticks >= _nextVolumeStateCheckTicks) {
        _nextVolumeStateCheckTicks = ticks + os::time::ms(1000);

        uint32_t newVolumeState = fs::volume().available() ? Available : 0;
        if (newVolumeState & Available) {
            if (!(_volumeState & Mounted)) {
                newVolumeState |= (fs::volume().mount() == fs::OK) ? Mounted : 0;
            } else {
                newVolumeState |= Mounted;
            }
        } else {
            invalidateAllSlots();
        }

        _volumeState = newVolumeState;
    }

    if (_taskPending) {
        fs::Error result = _taskExecuteCallback();
        _taskPending = 0;
        _taskResultCallback(result);
    }
}


fs::Error FileManager::writeFile(FileType type, int slot, std::function<fs::Error(const char *)> write) {
    const auto &info = fileTypeInfos[int(type)];
    if (!fs::exists(info.dir)) {
        fs::mkdir(info.dir);
    }

    FixedStringBuilder<32> path;
    slotPath(path, type, slot);

    auto result = write(path);
    if (result == fs::OK) {
        invalidateSlot(type, slot);
    }

    return result;
}

fs::Error FileManager::readFile(FileType type, int slot, std::function<fs::Error(const char *)> read) {
    const auto &info = fileTypeInfos[int(type)];
    if (!fs::exists(info.dir)) {
        fs::mkdir(info.dir);
    }

    FixedStringBuilder<32> path;
    slotPath(path, type, slot);

    auto result = read(path);

    return result;
}

fs::Error FileManager::writeLastProject(int slot) {
    fs::FileWriter fileWriter("LAST.DAT");
    if (fileWriter.error() != fs::OK) {
        return fileWriter.error();
    }

    fileWriter.write(&slot, sizeof(slot));

    return fileWriter.finish();
}

fs::Error FileManager::readLastProject(int &slot) {
    fs::FileReader fileReader("LAST.DAT");
    if (fileReader.error() != fs::OK) {
        return fileReader.error();
    }

    fileReader.read(&slot, sizeof(slot));

    return fileReader.finish();
}

bool FileManager::cachedSlot(FileType type, int slot, SlotInfo &info) {
    for (auto &cachedSlotInfo : _cachedSlotInfos) {
        if (cachedSlotInfo.ticket != 0 && cachedSlotInfo.type == type && cachedSlotInfo.slot == slot) {
            info = cachedSlotInfo.info;
            cachedSlotInfo.ticket = nextCachedSlotTicket();
            return true;
        }
    }
    return false;
}

void FileManager::cacheSlot(FileType type, int slot, const SlotInfo &info) {
    auto cachedSlotInfo = std::min_element(_cachedSlotInfos.begin(), _cachedSlotInfos.end());
    cachedSlotInfo->type = type;
    cachedSlotInfo->slot = slot;
    cachedSlotInfo->info = info;
    cachedSlotInfo->ticket = nextCachedSlotTicket();
}

void FileManager::invalidateSlot(FileType type, int slot) {
    for (auto &cachedSlotInfo : _cachedSlotInfos) {
        if (cachedSlotInfo.ticket != 0 && cachedSlotInfo.type == type && cachedSlotInfo.slot == slot) {
            cachedSlotInfo.ticket = 0;
        }
    }
}

void FileManager::invalidateAllSlots() {
    for (auto &cachedSlotInfo : _cachedSlotInfos) {
        cachedSlotInfo.ticket = 0;
    }
}

uint32_t FileManager::nextCachedSlotTicket() {
    _cachedSlotInfoTicket = std::max(uint32_t(1), _cachedSlotInfoTicket + 1);
    return _cachedSlotInfoTicket;
}
