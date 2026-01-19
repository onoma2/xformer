#include "FileManager.h"
#include "ProjectVersion.h"
#include "TeletypeTrack.h"

#include "core/utils/StringBuilder.h"
#include "core/fs/FileSystem.h"
#include "core/fs/FileWriter.h"
#include "core/fs/FileReader.h"

#include "os/os.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>

extern "C" {
#include "teletype.h"
}

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
    { "TELS", "TXT" },
    { "TELT", "TXT" },
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

static bool parseTriggerInputSource(const char *text, TeletypeTrack::TriggerInputSource &out) {
    if (!text) {
        return false;
    }
    for (int i = 0; i < int(TeletypeTrack::TriggerInputSource::Last); ++i) {
        auto value = TeletypeTrack::TriggerInputSource(i);
        const char *name = TeletypeTrack::triggerInputSourceName(value);
        if (name && std::strcmp(text, name) == 0) {
            out = value;
            return true;
        }
    }
    return false;
}

static bool parseCvInputSource(const char *text, TeletypeTrack::CvInputSource &out) {
    if (!text) {
        return false;
    }
    for (int i = 0; i < int(TeletypeTrack::CvInputSource::Last); ++i) {
        auto value = TeletypeTrack::CvInputSource(i);
        const char *name = TeletypeTrack::cvInputSourceName(value);
        if (name && std::strcmp(text, name) == 0) {
            out = value;
            return true;
        }
    }
    return false;
}

static bool parseTriggerOutputDest(const char *text, TeletypeTrack::TriggerOutputDest &out) {
    if (!text) {
        return false;
    }
    for (int i = 0; i < int(TeletypeTrack::TriggerOutputDest::Last); ++i) {
        auto value = TeletypeTrack::TriggerOutputDest(i);
        const char *name = TeletypeTrack::triggerOutputDestName(value);
        if (name && std::strcmp(text, name) == 0) {
            out = value;
            return true;
        }
    }
    return false;
}

static bool parseCvOutputDest(const char *text, TeletypeTrack::CvOutputDest &out) {
    if (!text) {
        return false;
    }
    for (int i = 0; i < int(TeletypeTrack::CvOutputDest::Last); ++i) {
        auto value = TeletypeTrack::CvOutputDest(i);
        const char *name = TeletypeTrack::cvOutputDestName(value);
        if (name && std::strcmp(text, name) == 0) {
            out = value;
            return true;
        }
    }
    return false;
}

static bool parseVoltageRange(const char *text, Types::VoltageRange &out) {
    if (!text) {
        return false;
    }
    for (int i = 0; i < int(Types::VoltageRange::Last); ++i) {
        auto value = Types::VoltageRange(i);
        const char *name = Types::voltageRangeName(value);
        if (name && std::strcmp(text, name) == 0) {
            out = value;
            return true;
        }
    }
    return false;
}

static bool parseMidiPort(const char *text, Types::MidiPort &out) {
    if (!text) {
        return false;
    }
    for (int i = 0; i < int(Types::MidiPort::Last); ++i) {
        auto value = Types::MidiPort(i);
        const char *name = Types::midiPortName(value);
        if (name && std::strcmp(text, name) == 0) {
            out = value;
            return true;
        }
    }
    return false;
}

static bool parseTimeBase(const char *text, TeletypeTrack::TimeBase &out) {
    if (!text) {
        return false;
    }
    if (std::strcmp(text, "MS") == 0) {
        out = TeletypeTrack::TimeBase::Ms;
        return true;
    }
    if (std::strcmp(text, "Clock") == 0) {
        out = TeletypeTrack::TimeBase::Clock;
        return true;
    }
    return false;
}

static bool parseQuantizeScale(const char *text, int8_t &out) {
    if (!text) {
        return false;
    }
    if (std::strcmp(text, "Off") == 0) {
        out = TeletypeTrack::QuantizeOff;
        return true;
    }
    if (std::strcmp(text, "Default") == 0) {
        out = TeletypeTrack::QuantizeDefault;
        return true;
    }
    for (int i = 0; i < Scale::Count; ++i) {
        if (std::strcmp(text, Scale::name(i)) == 0) {
            out = static_cast<int8_t>(i);
            return true;
        }
    }
    return false;
}

static bool parseRootNote(const char *text, int8_t &out) {
    if (!text) {
        return false;
    }
    if (std::strcmp(text, "Default") == 0) {
        out = -1;
        return true;
    }
    static const char *names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    for (int i = 0; i < 12; ++i) {
        if (std::strcmp(text, names[i]) == 0) {
            out = static_cast<int8_t>(i);
            return true;
        }
    }
    int value = 0;
    if (parseInt(text, value)) {
        out = clamp<int8_t>(value, -1, 11);
        return true;
    }
    return false;
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

fs::Error FileManager::writeTeletypeScript(const TeletypeTrack &track, int scriptIndex, int slot) {
    return writeFile(FileType::TeletypeScript, slot, [&] (const char *path) {
        return writeTeletypeScript(track, scriptIndex, path);
    });
}

fs::Error FileManager::readTeletypeScript(TeletypeTrack &track, int scriptIndex, int slot) {
    return readFile(FileType::TeletypeScript, slot, [&] (const char *path) {
        return readTeletypeScript(track, scriptIndex, path);
    });
}

fs::Error FileManager::writeTeletypeTrack(const TeletypeTrack &track, const char *name, int slot) {
    return writeFile(FileType::TeletypeTrack, slot, [&] (const char *path) {
        return writeTeletypeTrack(track, name, path);
    });
}

fs::Error FileManager::readTeletypeTrack(TeletypeTrack &track, int slot) {
    return readFile(FileType::TeletypeTrack, slot, [&] (const char *path) {
        return readTeletypeTrack(track, path);
    });
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

    VersionedSerializedReader reader(
        [&fileReader] (void *data, size_t len) { fileReader.read(data, len); },
        ProjectVersion::Latest
    );

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

fs::Error FileManager::writeTeletypeScript(const TeletypeTrack &track, int scriptIndex, const char *path) {
    if (scriptIndex < 0 || scriptIndex >= TeletypeTrack::EditableScriptCount) {
        return fs::INVALID_PARAMETER;
    }

    fs::FileWriter fileWriter(path);
    if (fileWriter.error() != fs::OK) {
        return fileWriter.error();
    }

    char lineBuffer[256] = {};
    scene_state_t &state = const_cast<scene_state_t &>(track.state());
    for (int line = 0; line < TeletypeTrack::ScriptLineCount; ++line) {
        lineBuffer[0] = '\0';
        const tele_command_t *cmd = ss_get_script_command(&state, scriptIndex, line);
        if (cmd && cmd->length > 0) {
            print_command(cmd, lineBuffer);
        }
        fileWriter.write(lineBuffer, strlen(lineBuffer));
        fileWriter.write("\n", 1);
    }

    return fileWriter.finish();
}

fs::Error FileManager::readTeletypeScript(TeletypeTrack &track, int scriptIndex, const char *path) {
    if (scriptIndex < 0 || scriptIndex >= TeletypeTrack::EditableScriptCount) {
        return fs::INVALID_PARAMETER;
    }

    fs::FileReader fileReader(path);
    if (fileReader.error() != fs::OK) {
        return fileReader.error();
    }

    bool success = true;
    char lineBuffer[256] = {};
    scene_state_t &state = track.state();
    ss_clear_script(&state, scriptIndex);
    for (int line = 0; line < TeletypeTrack::ScriptLineCount; ++line) {
        if (!readLine(fileReader, lineBuffer, sizeof(lineBuffer))) {
            break;
        }
        if (lineBuffer[0] == '\0') {
            continue;
        }
        tele_command_t cmd = {};
        char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
        tele_error_t error = parse(lineBuffer, &cmd, errorMsg);
        if (error != E_OK) {
            success = false;
            continue;
        }
        error = validate(&cmd, errorMsg);
        if (error != E_OK) {
            success = false;
            continue;
        }
        ss_overwrite_script_command(&state, scriptIndex, line, &cmd);
    }

    auto error = fileReader.finish();
    if (error == fs::END_OF_FILE) {
        error = fs::OK;
    }
    if (error == fs::OK && !success) {
        error = fs::INVALID_CHECKSUM;
    }

    return error;
}

static void writeLine(fs::FileWriter &writer, const char *line) {
    if (!line) {
        return;
    }
    writer.write(line, std::strlen(line));
    writer.write("\n", 1);
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

template<size_t N>
static void writeLine(fs::FileWriter &writer, const FixedStringBuilder<N> &line) {
    writeLine(writer, static_cast<const char *>(line));
}

static void writeScriptSection(fs::FileWriter &writer, const char *header,
                               const std::array<tele_command_t, TeletypeTrack::ScriptLineCount> &lines,
                               uint8_t length) {
    writeLine(writer, header);
    char buffer[256] = {};
    const uint8_t count = clamp<uint8_t>(length, 0, TeletypeTrack::ScriptLineCount);
    for (uint8_t i = 0; i < count; ++i) {
        buffer[0] = '\0';
        if (lines[i].length > 0) {
            print_command(&lines[i], buffer);
        }
        writeLine(writer, buffer);
    }
}

static void writeScriptSectionRaw(fs::FileWriter &writer, const char *header,
                                  const tele_command_t *lines, uint8_t length) {
    writeLine(writer, header);
    char buffer[256] = {};
    const uint8_t count = clamp<uint8_t>(length, 0, TeletypeTrack::ScriptLineCount);
    for (uint8_t i = 0; i < count; ++i) {
        buffer[0] = '\0';
        if (lines[i].length > 0) {
            print_command(&lines[i], buffer);
        }
        writeLine(writer, buffer);
    }
}

static const char *quantizeScaleName(int8_t scale) {
    if (scale == TeletypeTrack::QuantizeOff) {
        return "Off";
    }
    if (scale == TeletypeTrack::QuantizeDefault) {
        return "Default";
    }
    if (scale >= 0 && scale < Scale::Count) {
        return Scale::name(scale);
    }
    return "Off";
}

static const char *rootNoteName(int8_t note, FixedStringBuilder<8> &buffer) {
    if (note < 0) {
        return "Default";
    }
    buffer.reset();
    Types::printNote(buffer, note);
    return buffer;
}

namespace {
    // Shared Teletype slot buffers to avoid large stack usage in file task.
    TeletypeTrack::PatternSlot ttSlot1;
    TeletypeTrack::PatternSlot ttSlot2;
    char ttLineBuffer[256];
}

static void writeSlotIo(fs::FileWriter &writer, int slotIndex, const TeletypeTrack::PatternSlot &slot) {
    writeLine(writer, FixedStringBuilder<8>("SLOT %d", slotIndex + 1));

    for (int i = 0; i < TeletypeTrack::TriggerInputCount; ++i) {
        const char *name = TeletypeTrack::triggerInputSourceName(slot.triggerInputSource[i]);
        writeLine(writer, FixedStringBuilder<32>("TI-TR%d %s", i + 1, name ? name : "None"));
    }
    writeLine(writer, FixedStringBuilder<32>("TI-IN %s", TeletypeTrack::cvInputSourceName(slot.cvInSource)));
    writeLine(writer, FixedStringBuilder<32>("TI-PARAM %s", TeletypeTrack::cvInputSourceName(slot.cvParamSource)));
    writeLine(writer, FixedStringBuilder<32>("TI-X %s", TeletypeTrack::cvInputSourceName(slot.cvXSource)));
    writeLine(writer, FixedStringBuilder<32>("TI-Y %s", TeletypeTrack::cvInputSourceName(slot.cvYSource)));
    writeLine(writer, FixedStringBuilder<32>("TI-Z %s", TeletypeTrack::cvInputSourceName(slot.cvZSource)));
    writeLine(writer, FixedStringBuilder<32>("TI-T %s", TeletypeTrack::cvInputSourceName(slot.cvTSource)));

    for (int i = 0; i < TeletypeTrack::TriggerOutputCount; ++i) {
        const char *name = TeletypeTrack::triggerOutputDestName(slot.triggerOutputDest[i]);
        writeLine(writer, FixedStringBuilder<32>("TO-TR%d %s", i + 1, name ? name : "Gate Out 1"));
    }
    for (int i = 0; i < TeletypeTrack::CvOutputCount; ++i) {
        const char *name = TeletypeTrack::cvOutputDestName(slot.cvOutputDest[i]);
        writeLine(writer, FixedStringBuilder<32>("TO-CV%d %s", i + 1, name ? name : "CV Out 1"));
    }

    for (int i = 0; i < TeletypeTrack::CvOutputCount; ++i) {
        const char *range = Types::voltageRangeName(slot.cvOutputRange[i]);
        writeLine(writer, FixedStringBuilder<32>("CV%d RNG %s", i + 1, range ? range : "5V Bipolar"));
        writeLine(writer, FixedStringBuilder<32>("CV%d OFF %d", i + 1, slot.cvOutputOffset[i]));
        writeLine(writer, FixedStringBuilder<32>("CV%d Q %s", i + 1, quantizeScaleName(slot.cvOutputQuantizeScale[i])));
        FixedStringBuilder<8> noteStr;
        writeLine(writer, FixedStringBuilder<32>("CV%d ROOT %s", i + 1, rootNoteName(slot.cvOutputRootNote[i], noteStr)));
    }

    FixedStringBuilder<8> midiPort;
    midiPort.reset();
    midiPort(Types::midiPortName(slot.midiSource.port()));
    writeLine(writer, FixedStringBuilder<32>("MIDI PORT %s", midiPort));

    FixedStringBuilder<8> midiChan;
    midiChan.reset();
    Types::printMidiChannel(midiChan, slot.midiSource.channel());
    writeLine(writer, FixedStringBuilder<32>("MIDI CH %s", midiChan));

    writeLine(writer, FixedStringBuilder<16>("BOOT %d", slot.bootScriptIndex + 1));
    writeLine(writer, FixedStringBuilder<16>("TIMEBASE %s", TeletypeTrack::timeBaseName(slot.timeBase)));
    writeLine(writer, FixedStringBuilder<16>("CLK.DIV %d", slot.clockDivisor));
    writeLine(writer, FixedStringBuilder<16>("CLK.MULT %d", slot.clockMultiplier));
    writeLine(writer, FixedStringBuilder<16>("RESET.METRO %d", slot.resetMetroOnLoad ? 1 : 0));
}

static void writePatterns(fs::FileWriter &writer, int slotIndex, const TeletypeTrack::PatternSlot &slot) {
    writeLine(writer, FixedStringBuilder<8>("SLOT %d", slotIndex + 1));
    for (int p = 0; p < PATTERN_COUNT; ++p) {
        const auto &pat = slot.patterns[p];
        writeLine(writer, FixedStringBuilder<24>("P%d LEN %d", p + 1, pat.len));
        writeLine(writer, FixedStringBuilder<24>("P%d WRAP %d", p + 1, pat.wrap));
        writeLine(writer, FixedStringBuilder<24>("P%d START %d", p + 1, pat.start));
        writeLine(writer, FixedStringBuilder<24>("P%d END %d", p + 1, pat.end));
        for (int chunk = 0; chunk < 4; ++chunk) {
            FixedStringBuilder<128> line("P%d VALS", p + 1);
            for (int i = 0; i < 16; ++i) {
                int idx = chunk * 16 + i;
                line(" %d", pat.val[idx]);
            }
            writeLine(writer, line);
        }
    }
}

fs::Error FileManager::writeTeletypeTrack(const TeletypeTrack &track, const char *name, const char *path) {
    fs::FileWriter fileWriter(path);
    if (fileWriter.error() != fs::OK) {
        return fileWriter.error();
    }

    const char *safeName = (name && name[0]) ? name : "TELETYPE";
    writeLine(fileWriter, FixedStringBuilder<64>("NAME %s", safeName));

    // Avoid large PatternSlot copies on the file task stack.
    ttSlot1 = track.patternSlotSnapshot(0);
    ttSlot2 = track.patternSlotSnapshot(1);

    writeLine(fileWriter, "#IO");
    writeSlotIo(fileWriter, 0, ttSlot1);
    writeSlotIo(fileWriter, 1, ttSlot2);

    writeScriptSection(fileWriter, "#S4P1", ttSlot1.slotScript, ttSlot1.slotScriptLength);
    writeScriptSection(fileWriter, "#M1", ttSlot1.metro, ttSlot1.metroLength);
    writeScriptSection(fileWriter, "#S4P2", ttSlot2.slotScript, ttSlot2.slotScriptLength);
    writeScriptSection(fileWriter, "#M2", ttSlot2.metro, ttSlot2.metroLength);
    const scene_state_t &state = track.state();
    writeScriptSectionRaw(fileWriter, "#S1", state.scripts[0].c, state.scripts[0].l);
    writeScriptSectionRaw(fileWriter, "#S2", state.scripts[1].c, state.scripts[1].l);
    writeScriptSectionRaw(fileWriter, "#S3", state.scripts[2].c, state.scripts[2].l);

    writeLine(fileWriter, "#PATS");
    writePatterns(fileWriter, 0, ttSlot1);
    writePatterns(fileWriter, 1, ttSlot2);

    return fileWriter.finish();
}

static void clearScriptBuffer(TeletypeTrack::PatternSlot &slot, bool metro) {
    if (metro) {
        slot.metroLength = 0;
        std::memset(slot.metro.data(), 0, sizeof(slot.metro));
    } else {
        slot.slotScriptLength = 0;
        std::memset(slot.slotScript.data(), 0, sizeof(slot.slotScript));
    }
}

fs::Error FileManager::readTeletypeTrack(TeletypeTrack &track, const char *path) {
    fs::FileReader fileReader(path);
    if (fileReader.error() != fs::OK) {
        return fileReader.error();
    }

    track.clear();
    scene_state_t &state = track.state();
    for (int script = 0; script < TeletypeTrack::EditableScriptCount; ++script) {
        ss_clear_script(&state, script);
    }
    // Avoid large PatternSlot copies on the file task stack.
    ttSlot1 = track.patternSlotSnapshot(0);
    ttSlot2 = track.patternSlotSnapshot(1);
    clearScriptBuffer(ttSlot1, false);
    clearScriptBuffer(ttSlot1, true);
    clearScriptBuffer(ttSlot2, false);
    clearScriptBuffer(ttSlot2, true);

    enum class Section {
        None,
        IO,
        Pats,
        ScriptSlotP1,
        ScriptSlotP2,
        ScriptM1,
        ScriptM2,
        ScriptS1,
        ScriptS2,
        ScriptS3,
    };

    Section section = Section::None;
    int currentSlot = 0;
    int currentPatternSlot = 0;
    int patternValueIndex[2][PATTERN_COUNT] = {};

    auto parseSlot = [&] () -> TeletypeTrack::PatternSlot & {
        return currentSlot == 0 ? ttSlot1 : ttSlot2;
    };

    ttLineBuffer[0] = '\0';
    while (readLine(fileReader, ttLineBuffer, sizeof(ttLineBuffer))) {
        trimRight(ttLineBuffer);
        const char *line = skipSpace(ttLineBuffer);
        if (!line || *line == '\0') {
            continue;
        }

        if (line[0] == '#') {
            if (std::strcmp(line, "#IO") == 0) {
                section = Section::IO;
            } else if (std::strcmp(line, "#PATS") == 0) {
                section = Section::Pats;
            } else if (std::strcmp(line, "#S4P1") == 0 || std::strcmp(line, "#S1P1") == 0) {
                section = Section::ScriptSlotP1;
            } else if (std::strcmp(line, "#S4P2") == 0 || std::strcmp(line, "#S1P2") == 0) {
                section = Section::ScriptSlotP2;
            } else if (std::strcmp(line, "#M1") == 0) {
                section = Section::ScriptM1;
            } else if (std::strcmp(line, "#M2") == 0) {
                section = Section::ScriptM2;
            } else if (std::strcmp(line, "#S1") == 0) {
                section = Section::ScriptS1;
            } else if (std::strcmp(line, "#S2") == 0) {
                section = Section::ScriptS2;
            } else if (std::strcmp(line, "#S3") == 0) {
                section = Section::ScriptS3;
            } else {
                section = Section::None;
            }
            continue;
        }

        if (std::strncmp(line, "NAME ", 5) == 0) {
            continue;
        }

        if (section == Section::IO || section == Section::Pats) {
            if (std::strncmp(line, "SLOT ", 5) == 0) {
                int slotIndex = 0;
                if (parseInt(line + 5, slotIndex)) {
                    currentSlot = clamp(slotIndex - 1, 0, TeletypeTrack::PatternSlotCount - 1);
                    if (section == Section::Pats) {
                        currentPatternSlot = currentSlot;
                    }
                }
                continue;
            }
        }

        if (section == Section::IO) {
            auto &slot = parseSlot();
            const char *value = nullptr;

            if (std::strncmp(line, "TI-TR", 5) == 0 && std::isdigit(line[5])) {
                int idx = line[5] - '1';
                value = skipSpace(line + 6);
                TeletypeTrack::TriggerInputSource source{};
                if (idx >= 0 && idx < TeletypeTrack::TriggerInputCount &&
                    parseTriggerInputSource(value, source)) {
                    slot.triggerInputSource[idx] = source;
                }
                continue;
            }
            if (std::strncmp(line, "TI-IN ", 6) == 0) {
                value = skipSpace(line + 6);
                TeletypeTrack::CvInputSource source{};
                if (parseCvInputSource(value, source)) {
                    slot.cvInSource = source;
                }
                continue;
            }
            if (std::strncmp(line, "TI-PARAM ", 9) == 0) {
                value = skipSpace(line + 9);
                TeletypeTrack::CvInputSource source{};
                if (parseCvInputSource(value, source)) {
                    slot.cvParamSource = source;
                }
                continue;
            }
            if (std::strncmp(line, "TI-X ", 5) == 0) {
                value = skipSpace(line + 5);
                TeletypeTrack::CvInputSource source{};
                if (parseCvInputSource(value, source)) {
                    slot.cvXSource = source;
                }
                continue;
            }
            if (std::strncmp(line, "TI-Y ", 5) == 0) {
                value = skipSpace(line + 5);
                TeletypeTrack::CvInputSource source{};
                if (parseCvInputSource(value, source)) {
                    slot.cvYSource = source;
                }
                continue;
            }
            if (std::strncmp(line, "TI-Z ", 5) == 0) {
                value = skipSpace(line + 5);
                TeletypeTrack::CvInputSource source{};
                if (parseCvInputSource(value, source)) {
                    slot.cvZSource = source;
                }
                continue;
            }
            if (std::strncmp(line, "TI-T ", 5) == 0) {
                value = skipSpace(line + 5);
                TeletypeTrack::CvInputSource source{};
                if (parseCvInputSource(value, source)) {
                    slot.cvTSource = source;
                }
                continue;
            }
            if (std::strncmp(line, "TO-TR", 5) == 0 && std::isdigit(line[5])) {
                int idx = line[5] - '1';
                value = skipSpace(line + 6);
                TeletypeTrack::TriggerOutputDest dest{};
                if (idx >= 0 && idx < TeletypeTrack::TriggerOutputCount &&
                    parseTriggerOutputDest(value, dest)) {
                    slot.triggerOutputDest[idx] = dest;
                }
                continue;
            }
            if (std::strncmp(line, "TO-CV", 5) == 0 && std::isdigit(line[5])) {
                int idx = line[5] - '1';
                value = skipSpace(line + 6);
                TeletypeTrack::CvOutputDest dest{};
                if (idx >= 0 && idx < TeletypeTrack::CvOutputCount &&
                    parseCvOutputDest(value, dest)) {
                    slot.cvOutputDest[idx] = dest;
                }
                continue;
            }

            if (std::strncmp(line, "CV", 2) == 0 && std::isdigit(line[2])) {
                int idx = line[2] - '1';
                if (idx >= 0 && idx < TeletypeTrack::CvOutputCount) {
                    if (std::strncmp(line + 3, " RNG ", 5) == 0) {
                        value = skipSpace(line + 8);
                        Types::VoltageRange range{};
                        if (parseVoltageRange(value, range)) {
                            slot.cvOutputRange[idx] = range;
                        }
                    } else if (std::strncmp(line + 3, " OFF ", 5) == 0) {
                        int off = 0;
                        if (parseInt(line + 8, off)) {
                            slot.cvOutputOffset[idx] = clamp<int16_t>(off, -500, 500);
                        }
                    } else if (std::strncmp(line + 3, " Q ", 3) == 0) {
                        value = skipSpace(line + 6);
                        int8_t q = slot.cvOutputQuantizeScale[idx];
                        if (parseQuantizeScale(value, q)) {
                            slot.cvOutputQuantizeScale[idx] = q;
                        }
                    } else if (std::strncmp(line + 3, " ROOT ", 6) == 0) {
                        value = skipSpace(line + 9);
                        int8_t note = slot.cvOutputRootNote[idx];
                        if (parseRootNote(value, note)) {
                            slot.cvOutputRootNote[idx] = note;
                        }
                    }
                }
                continue;
            }

            if (std::strncmp(line, "MIDI PORT ", 10) == 0) {
                value = skipSpace(line + 10);
                Types::MidiPort port{};
                if (parseMidiPort(value, port)) {
                    slot.midiSource.setPort(port);
                }
                continue;
            }
            if (std::strncmp(line, "MIDI CH ", 8) == 0) {
                value = skipSpace(line + 8);
                if (std::strcmp(value, "Omni") == 0) {
                    slot.midiSource.setChannel(-1);
                } else {
                    int ch = 0;
                    if (parseInt(value, ch)) {
                        slot.midiSource.setChannel(ch - 1);
                    }
                }
                continue;
            }
            if (std::strncmp(line, "BOOT ", 5) == 0) {
                int boot = 0;
                if (parseInt(line + 5, boot)) {
                    slot.bootScriptIndex = clamp<int8_t>(boot - 1, 0, TeletypeTrack::ScriptSlotCount - 1);
                }
                continue;
            }
            if (std::strncmp(line, "TIMEBASE ", 9) == 0) {
                value = skipSpace(line + 9);
                TeletypeTrack::TimeBase base{};
                if (parseTimeBase(value, base)) {
                    slot.timeBase = base;
                }
                continue;
            }
            if (std::strncmp(line, "CLK.DIV ", 8) == 0) {
                int div = 0;
                if (parseInt(line + 8, div)) {
                    slot.clockDivisor = ModelUtils::clampDivisor(div);
                }
                continue;
            }
            if (std::strncmp(line, "CLK.MULT ", 9) == 0) {
                int mult = 0;
                if (parseInt(line + 9, mult)) {
                    slot.clockMultiplier = clamp<int16_t>(mult, 50, 150);
                }
                continue;
            }
            if (std::strncmp(line, "RESET.METRO ", 12) == 0) {
                int value = 0;
                if (parseInt(line + 12, value)) {
                    slot.resetMetroOnLoad = value != 0;
                }
                continue;
            }
        }

        if (section == Section::ScriptSlotP1 || section == Section::ScriptSlotP2 ||
            section == Section::ScriptM1 || section == Section::ScriptM2) {
            TeletypeTrack::PatternSlot &slot =
                (section == Section::ScriptSlotP1 || section == Section::ScriptM1) ? ttSlot1 : ttSlot2;
            bool metro = (section == Section::ScriptM1 || section == Section::ScriptM2);
            auto &buffer = metro ? slot.metro : slot.slotScript;
            uint8_t &length = metro ? slot.metroLength : slot.slotScriptLength;
            if (length >= TeletypeTrack::ScriptLineCount) {
                continue;
            }
            tele_command_t cmd = {};
            char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
            if (parse(line, &cmd, errorMsg) != E_OK) {
                continue;
            }
            if (validate(&cmd, errorMsg) != E_OK) {
                continue;
            }
            buffer[length] = cmd;
            length++;
            continue;
        }

        if (section == Section::ScriptS1 || section == Section::ScriptS2 || section == Section::ScriptS3) {
            const int scriptIndex = (section == Section::ScriptS1) ? 0 : (section == Section::ScriptS2 ? 1 : 2);
            auto &script = state.scripts[scriptIndex];
            if (script.l >= TeletypeTrack::ScriptLineCount) {
                continue;
            }
            tele_command_t cmd = {};
            char errorMsg[TELE_ERROR_MSG_LENGTH] = {};
            if (parse(line, &cmd, errorMsg) != E_OK) {
                continue;
            }
            if (validate(&cmd, errorMsg) != E_OK) {
                continue;
            }
            script.c[script.l] = cmd;
            script.l++;
            continue;
        }

        if (section == Section::Pats) {
            if (line[0] == 'P' && std::isdigit(line[1])) {
                int patternIndex = line[1] - '1';
                if (patternIndex < 0 || patternIndex >= PATTERN_COUNT) {
                    continue;
                }
                auto &slot = (currentPatternSlot == 0) ? ttSlot1 : ttSlot2;
                auto &pat = slot.patterns[patternIndex];
                const char *rest = skipSpace(line + 2);
                if (std::strncmp(rest, "LEN ", 4) == 0) {
                    int value = 0;
                    if (parseInt(rest + 4, value)) {
                        pat.len = clamp<int16_t>(value, 0, 64);
                    }
                } else if (std::strncmp(rest, "WRAP ", 5) == 0) {
                    int value = 0;
                    if (parseInt(rest + 5, value)) {
                        pat.wrap = clamp<int16_t>(value, 0, 64);
                    }
                } else if (std::strncmp(rest, "START ", 6) == 0) {
                    int value = 0;
                    if (parseInt(rest + 6, value)) {
                        pat.start = clamp<int16_t>(value, 0, 63);
                    }
                } else if (std::strncmp(rest, "END ", 4) == 0) {
                    int value = 0;
                    if (parseInt(rest + 4, value)) {
                        pat.end = clamp<int16_t>(value, 0, 63);
                    }
                } else if (std::strncmp(rest, "VALS", 4) == 0) {
                    const char *values = skipSpace(rest + 4);
                    int &idx = patternValueIndex[currentPatternSlot][patternIndex];
                    while (values && *values && idx < 64) {
                        char *end = nullptr;
                        long val = std::strtol(values, &end, 10);
                        if (end == values) {
                            break;
                        }
                        pat.val[idx++] = clamp<int16_t>(val, -32768, 32767);
                        values = skipSpace(end);
                    }
                }
            }
        }
    }

    track.setPatternSlotForPattern(0, ttSlot1);
    track.setPatternSlotForPattern(1, ttSlot2);
    track.applyActivePatternSlot();

    auto error = fileReader.finish();
    if (error == fs::END_OF_FILE) {
        error = fs::OK;
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
        if (type == FileType::TeletypeScript) {
            FixedStringBuilder<9> name("TS%03d", slot + 1);
            std::strncpy(info.name, name, sizeof(info.name));
            info.name[sizeof(info.name) - 1] = '\0';
            info.used = true;
        } else if (type == FileType::TeletypeTrack) {
            FixedStringBuilder<9> name("TT%03d", slot + 1);
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
                std::strncpy(info.name, name, sizeof(info.name));
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
