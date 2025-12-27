#pragma once

#include <cmath>
#include "core/math/Math.h"

class Latoocarfian {
public:
    Latoocarfian() {
        reset();
    }

    void reset() {
        _x = 0.5f;
        _y = 0.5f;
    }

    /**
     * Advances the chaotic map and returns a bipolar value (-1.0 to 1.0).
     * 
     * @param a, b, c, d - Chaotic coefficients. 
     *        Typical chaotic ranges are roughly 0.5 to 3.0.
     * @return A value clamped to -1..1 range.
     */
    float next(float a, float b, float c, float d) {
        float nextX = std::sin(b * _y) + c * std::sin(b * _x);
        float nextY = std::sin(a * _x) + d * std::sin(a * _y);
        
        _x = nextX;
        _y = nextY;
        
        // The output range of X is bounded by 1 + |c|.
        // We dynamically normalize so the chaos always fills the -1..1 range 
        // without hard clipping, regardless of parameter C.
        float maxVal = 1.0f + std::abs(c);
        return clamp(_x / maxVal, -1.0f, 1.0f);
    }

private:
    float _x;
    float _y;
};
