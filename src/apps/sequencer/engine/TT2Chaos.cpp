// Verbatim port of chaos.c (+ chaos.h struct/enum), VM-free.
#include "TT2Helpers.h"

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

static int16_t cellular_get_val(void);
static int16_t logistic_get_val(void);
static int16_t cubic_get_val(void);
static int16_t henon_get_val(void);
static void chaos_scale_values(chaos_state_t*);

// constants defining I/O ranges
static const int16_t chaos_value_min = -10000;
static const int16_t chaos_value_max = 10000;
static const int16_t chaos_param_min = 0;
static const int16_t chaos_param_max = 10000;
// fixed beta for henon map
static const float chaos_henon_b = 0.3;
// cellular automata parameters (1-d, binary)
static const int chaos_cell_count = 8;
static const int chaos_cell_max = 0xff;

// Upstream ran chaos_init() at boot (module/main.c) to scale fx/fr from the
// integer defaults before any script; without it a first CHAOS read would
// return zero-state instead of the scaled logistic default. Bake that scaling
// into the static init so the native port matches. (Positional init — C++17
// has no C99 designated initializers: ix, fx, ir, fr, fx0, fx1, alg.)
static chaos_state_t makeDefaultChaosState() {
    chaos_state_t s = { 5000, 0.f, 5000, 0.f, 0.f, 0.f, CHAOS_ALGO_LOGISTIC };
    chaos_scale_values(&s);
    return s;
}
static chaos_state_t chaos_state = makeDefaultChaosState();

void chaos_init() {
    chaos_scale_values(&chaos_state);
}

// scale integer state and param values to float,
// as appropriate for current algorithm
static void chaos_scale_values(chaos_state_t* state) {
    switch (state->alg) {
        case CHAOS_ALGO_HENON:
            // for henon, x in [-1.5, 1.5], r in [1, 1.4]
            state->fx = state->ix / (float)chaos_value_max * 1.5;
            state->fr = 1.f + state->ir / (float)chaos_param_max * 0.4;
            if (state->fr < 1.f) { state->fr = 1.f; }
            if (state->fr > 1.4) { state->fr = 1.4f; }
            break;
        case CHAOS_ALGO_CELLULAR:
            // 1d binary CA takes binary state and rule
            if (state->ix > chaos_cell_max) { state->ix = chaos_cell_max; }
            if (state->ix < 0) { state->ix = 0; }
            // rule is 8 bits
            if (state->ir > 0xff) { state->ir = 0xff; }
            if (state->ir < 0) { state->ir = 0; }
            break;
        case CHAOS_ALGO_CUBIC:
        case CHAOS_ALGO_LOGISTIC:  // fall through
        default:
            // for cubic / logistic, x in [-1, 1] and r in [3.2, 4)
            state->fx = state->ix / (float)chaos_value_max;
            state->fr = state->ir / (float)chaos_param_max * 0.9999 + 3.0;
            break;
    }
}

void chaos_set_val(int16_t val) {
    chaos_state.ix = val;
    chaos_scale_values(&chaos_state);
}

static int16_t logistic_get_val() {
    if (chaos_state.fx < 0.f) { chaos_state.fx = 0.f; }
    chaos_state.fx = chaos_state.fx * chaos_state.fr * (1.f - chaos_state.fx);
    chaos_state.ix = chaos_state.fx * (float)chaos_value_max;
    return chaos_state.ix;
}

static int16_t cubic_get_val() {
    float x3 = chaos_state.fx * chaos_state.fx * chaos_state.fx;
    chaos_state.fx =
        chaos_state.fr * x3 + chaos_state.fx * (1.f - chaos_state.fr);
    chaos_state.ix = chaos_state.fx * (float)chaos_value_max;
    return chaos_state.ix;
}

static int16_t henon_get_val() {
    float x0_2 = chaos_state.fx0 * chaos_state.fx0;
    float x = 1.f - (x0_2 * chaos_state.fr) + (chaos_henon_b * chaos_state.fx1);
    // reflect bounds to avoid blowup
    while (x < -1.5) { x = -1.5 - x; }
    while (x > 1.5) { x = 1.5 - x; }
    chaos_state.fx1 = chaos_state.fx0;
    chaos_state.fx0 = chaos_state.fx;
    chaos_state.fx = x;
    chaos_state.ix = x / 1.5 * (float)chaos_value_max;
    return chaos_state.ix;
}

static int16_t cellular_get_val() {
    uint8_t x = (uint8_t)chaos_state.ix;
    uint8_t y = 0;
    uint8_t code = 0;
    for (int i = 0; i < chaos_cell_count; ++i) {
        // 3-bit code representing cell and neighbor state
        code = 0;
        // LSb in neighbor code = right-side neighor, wrapping
        if (i == 0) {
            if (x & 0x80) { code |= 0b001; }
        }
        else {
            if (x & (1 << (i - 1))) { code |= 0b001; }
        }
        // MSb in neighbor code = left-side neighbor, wrapping
        if (i == chaos_cell_count - 1) {
            if (x & 1) { code |= 0b100; }
        }
        else {
            if (x & (1 << (i + 1))) { code |= 0b100; }
        }
        // middle bit = old value of this cell
        if (x & (1 << i)) { code |= 0b010; }
        // lookup the bit in the rule specified by this code;
        // this is the new bit value
        if (chaos_state.ir & (1 << code)) { y |= (1 << i); }
    }
    chaos_state.ix = y;
    return chaos_state.ix;
}


int16_t chaos_get_val() {
    switch (chaos_state.alg) {
        case CHAOS_ALGO_LOGISTIC: return logistic_get_val();
        case CHAOS_ALGO_CUBIC: return cubic_get_val();
        case CHAOS_ALGO_HENON: return henon_get_val();
        case CHAOS_ALGO_CELLULAR: return cellular_get_val();
        default: return 0;
    }
}

void chaos_set_r(int16_t r) {
    chaos_state.ir = r;
    chaos_scale_values(&chaos_state);
}

int16_t chaos_get_r() {
    return chaos_state.ir;
}

void chaos_set_alg(int16_t a) {
    if (a < 0) { a = 0; }
    if (a >= CHAOS_ALGO_COUNT) { a = CHAOS_ALGO_COUNT - 1; }
    chaos_state.alg = (chaos_algo_t)a;
    chaos_scale_values(&chaos_state);
}

int16_t chaos_get_alg() {
    return chaos_state.alg;
}
