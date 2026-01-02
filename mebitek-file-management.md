# File Management in mebitek-performer vs original-performer

This document provides a detailed comparison of the file management systems between the mebitek-performer and original-performer firmware implementations, highlighting key differences and code examples.

## Overview

Both firmware implementations use a similar architecture for file management but differ significantly in feature scope and supported file types. The mebitek-performer is an enhanced fork that adds comprehensive sequence management capabilities.

## File System Architecture

### Common Components

Both implementations share the same underlying file system architecture:

```cpp
// Core file system includes
#include "core/fs/FileSystem.h"
#include "core/fs/FileWriter.h"
#include "core/fs/FileReader.h"
```

### File Header Structure

Both implementations use the same basic file header structure:

```cpp
struct FileHeader {
    static constexpr size_t NameLength = 8;

    FileType type;
    uint8_t version;
    char name[NameLength];

    FileHeader() = default;

    FileHeader(FileType type, uint8_t version, const char *name) {
        this->type = type;
        this->version = version;
        size_t len = strlen(name);
        std::memset(this->name, 0, sizeof(this->name));
        std::memcpy(this->name, name, std::min(sizeof(this->name), len));
    }

    void readName(char *name, size_t len) {
        std::memcpy(name, this->name, std::min(sizeof(this->name), len));
        name[std::min(sizeof(this->name), len - 1)] = '\0';
    }

} __attribute__((packed));
```

## File Type Support

### original-performer File Types

```cpp
enum class FileType : uint8_t {
    Project     = 0,
    UserScale   = 1,
    Settings    = 255
};
```

### mebitek-performer File Types

```cpp
enum class FileType : uint8_t {
    Project     = 0,
    UserScale   = 1,
    NoteSequence= 2,
    CurveSequence=3,
    LogicSequence=4,
    ArpSequence=5,
    Settings    = 255
};
```

**Key Difference**: mebitek-performer supports 4 additional sequence types that enable comprehensive pattern management.

## Directory Structure

### original-performer

```
SD Card/
├── PROJECTS/
│   ├── 001.PRO
│   ├── 002.PRO
│   └── ...
├── SCALES/
│   ├── 001.SCA
│   ├── 002.SCA
│   └── ...
└── LAST.DAT
```

### mebitek-performer

```
SD Card/
├── PROJECTS/
│   ├── 001.PRO
│   ├── 002.PRO
│   └── ...
├── SCALES/
│   ├── 001.SCA
│   ├── 002.SCA
│   └── ...
├── SEQS/
│   ├── 001.NSQ  (Note Sequence)
│   ├── 002.CSQ  (Curve Sequence)
│   ├── 003.LSQ  (Logic Sequence)
│   ├── 004.ASQ  (Arp Sequence)
│   └── ...
└── LAST.DAT
```

**Key Difference**: mebitek-performer has a dedicated SEQS directory for different sequence types.

## FileManager Implementation

### File Type Information Mapping

#### original-performer
```cpp
FileTypeInfo fileTypeInfos[] = {
    { "PROJECTS", "PRO" },
    { "SCALES", "SCA" },
};
```

#### mebitek-performer
```cpp
FileTypeInfo fileTypeInfos[] = {
    { "PROJECTS", "PRO" },
    { "SCALES", "SCA" },
    {"SEQS", "NSQ"},
    {"SEQS", "CSQ"},
    {"SEQS", "LSQ"},
    {"SEQS", "ASQ"}
};
```

## Serialization Methods

Both implementations use the same serialization pattern:

```cpp
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
```

## Key Differences Summary

| Feature | original-performer | mebitek-performer |
|---------|-------------------|-------------------|
| Project Support | ✅ | ✅ |
| User Scale Support | ✅ | ✅ |
| Note Sequence Support | ❌ | ✅ |
| Curve Sequence Support | ❌ | ✅ |
| Logic Sequence Support | ❌ | ✅ |
| Arp Sequence Support | ❌ | ✅ |
| Sequence Library | ❌ | ✅ |
| Dedicated SEQS Directory | ❌ | ✅ |
| File Type Variants | 2 | 6 |

## UI Integration

### Sequence Management in mebitek-performer

The mebitek-performer includes dedicated UI pages for sequence management:

```cpp
// Example from CurveSequencePage.h
void loadSequence();
void saveSequence();
void saveAsSequence();
void saveSequenceToSlot(int slot);
void loadSequenceFromSlot(int slot);
```

### Project Management (Both)

Both implementations have similar project management:

```cpp
// Common in both
void loadProject();
void saveProject();
void saveAsProject();
void saveProjectToSlot(int slot);
void loadProjectFromSlot(int slot);
```

## File Path Generation

### Slot Path Function

#### original-performer
```cpp
static void slotPath(StringBuilder &str, FileType type, int slot) {
    const auto &info = fileTypeInfos[int(type)];
    str("%s/%03d.%s", info.dir, slot + 1, info.ext);
}
```

#### mebitek-performer
```cpp
static void slotPath(StringBuilder &str, FileType type, int slot) {
    const auto &info = fileTypeInfos[int(type)];
    str("%s/%03d.%s", info.dir, slot + 1, info.ext);
}
```

The function is identical, but mebitek-performer handles more file types.

## Asynchronous File Operations

Both implementations use the same task-based system for file operations:

```cpp
typedef std::function<fs::Error(void)> TaskExecuteCallback;
typedef std::function<void(fs::Error)> TaskResultCallback;

static void task(TaskExecuteCallback executeCallback, TaskResultCallback resultCallback);
static void processTask();
```

## Volume Management

Both implementations handle volume mounting and availability checks:

```cpp
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
```

## Conclusion

The mebitek-performer firmware extends the original-performer with comprehensive sequence management capabilities, adding support for multiple sequence types and a more sophisticated file organization system. The core file management architecture remains the same, but mebitek-performer adds significant functionality for managing individual sequence patterns, making it more suitable for complex musical arrangements and pattern-based sequencing workflows.