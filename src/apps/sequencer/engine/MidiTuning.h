#pragma once
#include <algorithm>
#include <cmath>

// cv in 1V/oct (note 60 == 0V). Returns nearest MIDI note + signed 14-bit pitch-bend
// (−8192..+8191) carrying the fractional offset, scaled by bendRange semitones.
inline void cvToNoteBend(float cv, int bendRange, int &note, int &signedBend) {
    if (bendRange < 1) bendRange = 1;                      // divisor guard
    float notePos = 60.f + cv * 12.f;
    int nearest = std::max(0, std::min(127, int(std::lround(notePos))));
    float bendSemis = notePos - nearest;
    int b = int(std::lround(bendSemis / bendRange * 8192.f));
    signedBend = std::max(-8192, std::min(8191, b));
    note = nearest;
}
