#ifndef CHAOS_H
#define CHAOS_H
#include <stdint.h>

typedef enum {
    CHAOS_ALGO_LOGISTIC,  // logistic map
    CHAOS_ALGO_CUBIC,     // cubic map
    CHAOS_ALGO_HENON,     // henon map
    CHAOS_ALGO_CELLULAR,  // 1-d binary cellular automaton
    CHAOS_ALGO_COUNT      // unused, don't remve
} chaos_algo_t;

// keep value and parameter in both integer and float formats
// this way, can switch algos on the fly and re-initialize (dunno if this is
// possible anyway)
typedef struct {
    int16_t ix;        // state value in integer format
    float fx;          // normalized floating point state value (as needed)
    int16_t ir;        // parameter value in integer format
    float fr;          // floating-point parm value (as needed)
    float fx0;         // state history (as needed)
    float fx1;         // state history (as needed)
    chaos_algo_t alg;  // current algorithm
} chaos_state_t;

void chaos_init(void);
void chaos_set_val(int16_t);
int16_t chaos_get_val(void);
void chaos_set_r(int16_t);
int16_t chaos_get_r(void);
void chaos_set_alg(int16_t);
int16_t chaos_get_alg(void);

#endif
