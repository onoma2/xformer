#pragma once

#include <cstdint>

// Transpose a raw pattern value by `interval` equal-temperament semitones,
// quantizing to the ET note grid. Verbatim port of the teletype pattern_mode
// transpose_n_value over tt2EtTable. Used by the TT2 pattern editor.
int16_t tt2TransposeSemitones(int16_t value, int interval);
