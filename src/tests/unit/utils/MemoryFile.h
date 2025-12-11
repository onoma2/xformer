#pragma once

#include "core/fs/File.h"

#include <vector>
#include <cstring>

namespace fs {

class MemoryFile {
public:
    MemoryFile(size_t capacity = 1024) {
        _data.reserve(capacity);
    }

    int read(void *data, int length) {
        if (_position + length > _data.size()) {
            length = _data.size() - _position;
        }
        std::memcpy(data, _data.data() + _position, length);
        _position += length;
        return length;
    }

    int write(const void *data, int length) {
        if (_position + length > _data.size()) {
            _data.resize(_position + length);
        }
        std::memcpy(_data.data() + _position, data, length);
        _position += length;
        return length;
    }

    void rewind() {
        _position = 0;
    }

private:
    std::vector<uint8_t> _data;
    size_t _position = 0;
};

} // namespace fs
