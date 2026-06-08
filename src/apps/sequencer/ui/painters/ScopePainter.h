#pragma once

#include "core/gfx/Canvas.h"

#include <cstdint>

// Shared rolling-oscilloscope trace, used by the Monitor scope and the Modulator page.
// Samples are a circular int8 buffer where +/-127 maps to +/-halfHeight around centerY
// (clamped). Consecutive samples are joined by vertical segments; the oldest sample sits
// at writeIndex (left edge), newest just behind it. The caller sets the trace color.
namespace ScopePainter {

    inline void drawTrace(Canvas &canvas, int x0, int centerY, int halfHeight,
                          const int8_t *buf, int count, int writeIndex) {
        if (count <= 1 || halfHeight <= 0) {
            return;
        }
        int lo = centerY - halfHeight;
        int hi = centerY + halfHeight;
        auto yAt = [&](int i) {
            int s = buf[(writeIndex + i) % count];
            int y = centerY - (s * halfHeight) / 127;
            return y < lo ? lo : (y > hi ? hi : y);
        };
        int last = yAt(0);
        for (int x = 1; x < count; ++x) {
            int y = yAt(x);
            int a = last < y ? last : y;
            int b = last < y ? y : last;
            canvas.vline(x0 + x, a, b - a + 1);
            last = y;
        }
    }

} // namespace ScopePainter
