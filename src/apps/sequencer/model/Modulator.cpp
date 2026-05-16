#include "Modulator.h"

void Modulator::editPhase(int value, bool shift) {
    int absValue = (value < 0) ? -value : value;
    int multiplier = shift ? 1 : 15;
    if (absValue >= 4) {
        multiplier *= 4;
    } else if (absValue >= 2) {
        multiplier *= 2;
    }
    setPhase(phase() - value * multiplier);
}

void Modulator::editRate(int value, bool shift) {
    if (isChaosShape(shape())) {
        // Chaos: rate is Hz * 10, stored in _rate
        int step = shift ? 1 : 5;  // 0.1 Hz or 0.5 Hz increment
        _rate = clamp(_rate + value * step, 1, 1000);  // 0.1 Hz to 100.0 Hz
        return;
    }
    // LFO: musical divisions
    static const int divisions[] = {
        6,    // 1/64
        8,    // 1/64T
        12,   // 1/32
        16,   // 1/32T
        18,   // 1/32.
        24,   // 1/16
        32,   // 1/16T
        36,   // 1/16.
        48,   // 1/8
        64,   // 1/8T
        72,   // 1/8.
        96,   // 1/4
        128,  // 1/4T
        144,  // 1/4.
        192,  // 1/2
        288,  // 1/2.
        384,  // 1 bar
        768,  // 2 bars
        1152, // 3 bars
        1536, // 4 bars
        2304, // 6 bars
        3072, // 8 bars
        4608, // 12 bars
        6144  // 16 bars
    };
    constexpr int numDivisions = sizeof(divisions) / sizeof(divisions[0]);

    int currentIndex = 0;
    for (int i = 0; i < numDivisions; ++i) {
        if (divisions[i] >= _rate) {
            currentIndex = i;
            break;
        }
    }

    int newIndex = clamp(currentIndex + value, 0, numDivisions - 1);
    _rate = divisions[newIndex];
}

void Modulator::printRate(StringBuilder &str) const {
    if (isChaosShape(shape())) {
        int hzTenths = rate();
        int whole = hzTenths / 10;
        int frac = hzTenths % 10;
        if (hzTenths < 10) {
            str("0.%dHz", frac);
        } else if (hzTenths < 1000) {
            str("%d.%dHz", whole, frac);
        } else {
            str("%dHz", whole);
        }
        return;
    }
    // LFO: musical divisions
    int r = rate();

    if (r == 8) str("1/64T");
    else if (r == 16) str("1/32T");
    else if (r == 18) str("1/32.");
    else if (r == 32) str("1/16T");
    else if (r == 36) str("1/16.");
    else if (r == 64) str("1/8T");
    else if (r == 72) str("1/8.");
    else if (r == 128) str("1/4T");
    else if (r == 144) str("1/4.");
    else if (r == 288) str("1/2.");
    else if (r >= 384) {
        int bars = (r + 192) / 384;
        str("%d bar%s", bars, bars > 1 ? "s" : "");
    } else if (r >= 96) {
        int quarters = (r + 48) / 96;
        str("%d/4", quarters);
    } else if (r >= 24) {
        int divisor = (384 + r / 2) / r;
        str("1/%d", divisor);
    } else {
        str("1/%d", 384 / r);
    }
}
