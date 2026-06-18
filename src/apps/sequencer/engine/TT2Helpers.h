#pragma once

#include <stdint.h>

// Native ports of the small VM-free helper functions TT2 dispatch uses,
// formerly the vendored teletype C: drum_helpers.c (tresillo/drum/velocity),
// chaos.c (chaos_*), and the libavr32 euclidean. Behaviour is a verbatim port
// of the C originals. (The scale-table subset of table.c moves native in U4,
// atomically with table.c's removal, to avoid a duplicate-symbol clash.)

int euclidean(int fill, int len, int step);
int tresillo(int bank, int pattern1, int pattern2, int len, int step);
int drum(int bank, int pattern, int step);
int velocity(int pattern, int step);

void chaos_set_val(int16_t);
int16_t chaos_get_val(void);
void chaos_set_r(int16_t);
int16_t chaos_get_r(void);
void chaos_set_alg(int16_t);
int16_t chaos_get_alg(void);
