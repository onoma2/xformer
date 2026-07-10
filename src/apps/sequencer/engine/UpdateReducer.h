#pragma once

#include "os/os.h"

#include <cstdint>

template<uint32_t Interval>
class UpdateReducer {
public:
    bool update() {
        return update(os::ticks());
    }

    bool update(uint32_t now) {
        if (int32_t(now - _lastUpdate) >= int32_t(Interval)) {
            _lastUpdate = now;
            return true;
        }
        return false;
    }

private:
    uint32_t _lastUpdate = 0;
};
