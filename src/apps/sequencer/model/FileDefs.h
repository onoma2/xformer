#pragma once

#include <algorithm>

#include <cstring>
#include <cstdint>

enum class FileType : uint8_t {
    Project     = 0,
    UserScale   = 1,
    TeletypeV2Program = 2,
    TeletypeV2Script = 3,
    Settings    = 255
};

struct FileHeader {
    static constexpr size_t NameLength = 8;
    // 'XMFR' — data-file signature. Written on every file, validated on load so
    // foreign or pre-0.8 files (which lack it) are rejected.
    static constexpr uint32_t Magic = 0x584D4652;

    uint32_t magic;
    FileType type;
    uint8_t version;
    char name[NameLength];

    FileHeader() = default;

    FileHeader(FileType type, uint8_t version, const char *name) {
        this->magic = Magic;
        this->type = type;
        this->version = version;
        size_t len = strlen(name);
        std::memset(this->name, 0, sizeof(this->name));
        std::memcpy(this->name, name, std::min(sizeof(this->name), len));
    }

    bool valid() const { return magic == Magic; }

    void readName(char *name, size_t len) {
        std::memcpy(name, this->name, std::min(sizeof(this->name), len));
        name[std::min(sizeof(this->name), len - 1)] = '\0';
    }

} __attribute__((packed));
