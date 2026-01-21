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
static void op_M_A_get(const void *data, scene_state_t *ss, exec_state_t *es,
                       command_state_t *cs);
static void op_M_ACT_A_get(const void *data, scene_state_t *ss,
                           exec_state_t *es, command_state_t *cs);
static void op_M_RESET_get(const void *data, scene_state_t *ss,
                           exec_state_t *es, command_state_t *cs);
static void op_M_RESET_A_get(const void *data, scene_state_t *ss,
                             exec_state_t *es, command_state_t *cs);

const tele_op_t op_M = MAKE_GET_SET_OP(M, op_M_get, op_M_set, 0, true);

static void op_M_get(const void *NOTUSED(data), scene_state_t *ss,
                     exec_state_t *NOTUSED(es), command_state_t *cs) {
    cs_push(cs, ss->variables.m);
}

static void op_M_set(const void *NOTUSED(data), scene_state_t *ss,
                     exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t m = cs_pop(cs);
    if (tele_timebase_is_clock()) {
        tele_clock_mode_notice();
        return;
    } else {
        if (m < 2) m = 2;
    }
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
    if (tele_timebase_is_clock()) {
        tele_clock_mode_notice();
        return;
    }
    if (m < 1) m = 1;
    ss->variables.m = m;
    tele_metro_updated();
}

const tele_op_t op_M_ACT =
    MAKE_GET_SET_OP(M.ACT, op_M_ACT_get, op_M_ACT_set, 0, true);

const tele_op_t op_M_A = MAKE_GET_OP(M.A, op_M_A_get, 1, false);
const tele_op_t op_M_ACT_A = MAKE_GET_OP(M.ACT.A, op_M_ACT_A_get, 1, false);


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

static void op_M_A_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                       exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t m = cs_pop(cs);
    if (tele_timebase_is_clock()) {
        tele_clock_mode_notice();
        return;
    }
    if (m < 2) m = 2;
    tele_metro_all_set(m);
}

static void op_M_ACT_A_get(const void *NOTUSED(data),
                           scene_state_t *NOTUSED(ss),
                           exec_state_t *NOTUSED(es),
                           command_state_t *cs) {
    int16_t state = cs_pop(cs);
    tele_metro_all_act(state > 0 ? 1 : 0);
}

const tele_op_t op_M_RESET = MAKE_GET_OP(M.RESET, op_M_RESET_get, 0, false);
const tele_op_t op_M_RESET_A = MAKE_GET_OP(M.RESET.A, op_M_RESET_A_get, 0, false);

static void op_M_RESET_get(const void *NOTUSED(data),
                           scene_state_t *NOTUSED(ss),
                           exec_state_t *NOTUSED(es),
                           command_state_t *NOTUSED(cs)) {
    tele_metro_reset();
}

static void op_M_RESET_A_get(const void *NOTUSED(data),
                             scene_state_t *NOTUSED(ss),
                             exec_state_t *NOTUSED(es),
                             command_state_t *NOTUSED(cs)) {
    tele_metro_all_reset();
}
