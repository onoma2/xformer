#include "stdbool.h"
#include "sysclk.h"

typedef struct {
    uint32_t last;
    uint32_t delta;
    bool tick;
} profile_t;

static inline void profile_update(profile_t *p) {
    uint32_t count = Get_system_register(AVR32_COUNT);

    if (count < p->last)
        p->delta = INT32_MAX - p->last + count;
    else
        p->delta = count - p->last;

    p->last = count;
    p->tick = !p->tick;
}

static inline uint32_t profile_delta(profile_t *p) {
    return p->delta;
}

static inline uint32_t profile_delta_us(profile_t *p) {
    return cpu_cy_2_us(p->delta, FCPU_HZ);
}

static inline bool profile_valid(profile_t *p) {
    return !p->tick;
}
