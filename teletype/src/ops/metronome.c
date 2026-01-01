#include "ops/metronome.h"

#include "helpers.h"
#include "teletype.h"
#include "teletype_io.h"

static void op_M_get(const void *data, scene_state_t *ss, exec_state_t *es,
                     command_state_t *cs);
static void op_M_set(const void *data, scene_state_t *ss, exec_state_t *es,
                     command_state_t *cs);
static void op_M_SYM_EXCLAMATION_get(const void *data, scene_state_t *ss,
                                     exec_state_t *es, command_state_t *cs);
static void op_M_SYM_EXCLAMATION_set(const void *data, scene_state_t *ss,
                                     exec_state_t *es, command_state_t *cs);
static void op_M_ACT_get(const void *data, scene_state_t *ss, exec_state_t *es,
                         command_state_t *cs);
static void op_M_ACT_set(const void *data, scene_state_t *ss, exec_state_t *es,
                         command_state_t *cs);
static void op_M_RESET_get(const void *data, scene_state_t *ss,
                           exec_state_t *es, command_state_t *cs);

const tele_op_t op_M = MAKE_GET_SET_OP(M, op_M_get, op_M_set, 0, true);

static void op_M_get(const void *NOTUSED(data), scene_state_t *ss,
                     exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, ss->variables.m);
}

static void op_M_set(const void *NOTUSED(data), scene_state_t *ss,
                     exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t m = cs_pop(cs);
    if (m < METRO_MIN_MS) m = METRO_MIN_MS;
    ss->variables.m = m;
    tele_metro_updated();
}

// clang-format off
// clang format has problems with the M!
const tele_op_t op_M_SYM_EXCLAMATION = MAKE_GET_SET_OP(
    M!, op_M_SYM_EXCLAMATION_get, op_M_SYM_EXCLAMATION_set, 0, true);
// clang-format on

static void op_M_SYM_EXCLAMATION_get(const void *NOTUSED(data),
                                     scene_state_t *ss,
                                     exec_state_t *NOTUSED(es),
                                     command_state_t *cs) {
    cs_push(cs, ss->variables.m);
}

static void op_M_SYM_EXCLAMATION_set(const void *NOTUSED(data),
                                     scene_state_t *ss,
                                     exec_state_t *NOTUSED(es),
                                     command_state_t *cs) {
    int16_t m = cs_pop(cs);
    if (m < METRO_MIN_UNSUPPORTED_MS) m = METRO_MIN_UNSUPPORTED_MS;
    ss->variables.m = m;
    tele_metro_updated();
}

const tele_op_t op_M_ACT =
    MAKE_GET_SET_OP(M.ACT, op_M_ACT_get, op_M_ACT_set, 0, true);


static void op_M_ACT_get(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, ss->variables.m_act);
}

static void op_M_ACT_set(const void *NOTUSED(data), scene_state_t *ss,
                         exec_state_t *NOTUSED(es), command_state_t *cs) {
    bool m_act = cs_pop(cs) > 0;
    ss->variables.m_act = m_act;
    tele_metro_updated();
}

const tele_op_t op_M_RESET = MAKE_GET_OP(M.RESET, op_M_RESET_get, 0, false);

static void op_M_RESET_get(const void *NOTUSED(data),
                           scene_state_t *NOTUSED(ss),
                           exec_state_t *NOTUSED(es),
                           command_state_t *NOTUSED(cs)) {
    tele_metro_reset();
}
