#include "ops/op.h"

#include <stddef.h>  // offsetof

#include "helpers.h"
#include "ops/controlflow.h"
#include "ops/delay.h"
#include "ops/hardware.h"
#include "ops/init.h"
#include "ops/maths.h"
#include "ops/metronome.h"
#include "ops/midi.h"
#include "ops/patterns.h"
#include "ops/queue.h"
#include "ops/seed.h"
#include "ops/stack.h"
#include "ops/turtle.h"
#include "ops/variables.h"
#include "teletype_io.h"

/////////////////////////////////////////////////////////////////
// OPS //////////////////////////////////////////////////////////

// If you edit this array, you need to run 'utils/op_enums.py' to update the
// values in 'op_enum.h' so that the order matches.
const tele_op_t *tele_ops[E_OP__LENGTH] = {
    // variables
    &op_A, &op_B, &op_C, &op_D, &op_DRUNK, &op_DRUNK_MAX, &op_DRUNK_MIN,
    &op_DRUNK_WRAP, &op_FLIP, &op_I, &op_O, &op_O_INC, &op_O_MAX, &op_O_MIN,
    &op_O_WRAP, &op_T, &op_TIME, &op_TIME_ACT, &op_LAST, &op_X, &op_Y, &op_Z,
    &op_J, &op_K,

    // init
    &op_INIT, &op_INIT_SCENE, &op_INIT_SCRIPT, &op_INIT_SCRIPT_ALL, &op_INIT_P,
    &op_INIT_P_ALL, &op_INIT_CV, &op_INIT_CV_ALL, &op_INIT_TR, &op_INIT_TR_ALL,
    &op_INIT_DATA, &op_INIT_TIME,

    // turtle
    &op_TURTLE, &op_TURTLE_X, &op_TURTLE_Y, &op_TURTLE_MOVE, &op_TURTLE_F,
    &op_TURTLE_FX1, &op_TURTLE_FY1, &op_TURTLE_FX2, &op_TURTLE_FY2,
    &op_TURTLE_SPEED, &op_TURTLE_DIR, &op_TURTLE_STEP, &op_TURTLE_BUMP,
    &op_TURTLE_WRAP, &op_TURTLE_BOUNCE, &op_TURTLE_SCRIPT, &op_TURTLE_SHOW,

    // metronome
    &op_M, &op_M_SYM_EXCLAMATION, &op_M_ACT, &op_M_RESET,

    // patterns
    &op_P_N, &op_P, &op_PN, &op_P_L, &op_PN_L, &op_P_WRAP, &op_PN_WRAP,
    &op_P_START, &op_PN_START, &op_P_END, &op_PN_END, &op_P_I, &op_PN_I,
    &op_P_HERE, &op_PN_HERE, &op_P_NEXT, &op_PN_NEXT, &op_P_PREV, &op_PN_PREV,
    &op_P_INS, &op_PN_INS, &op_P_RM, &op_PN_RM, &op_P_PUSH, &op_PN_PUSH,
    &op_P_POP, &op_PN_POP, &op_P_MIN, &op_PN_MIN, &op_P_MAX, &op_PN_MAX,
    &op_P_SHUF, &op_PN_SHUF, &op_P_REV, &op_PN_REV, &op_P_ROT, &op_PN_ROT,
    &op_P_RND, &op_PN_RND, &op_P_ADD, &op_PN_ADD, &op_P_SUB, &op_PN_SUB,
    &op_P_ADDW, &op_PN_ADDW, &op_P_SUBW, &op_PN_SUBW,

    // queue
    &op_Q, &op_Q_AVG, &op_Q_N, &op_Q_CLR, &op_Q_GRW, &op_Q_SUM, &op_Q_MIN,
    &op_Q_MAX, &op_Q_RND, &op_Q_SRT, &op_Q_REV, &op_Q_SH, &op_Q_ADD, &op_Q_SUB,
    &op_Q_MUL, &op_Q_DIV, &op_Q_MOD, &op_Q_I, &op_Q_2P, &op_Q_P2,

    // hardware
    &op_CV, &op_CV_OFF, &op_CV_SLEW, &op_IN, &op_IN_SCALE, &op_PARAM,
    &op_PARAM_SCALE, &op_IN_CAL_MIN, &op_IN_CAL_MAX, &op_IN_CAL_RESET,
    &op_PARAM_CAL_MIN, &op_PARAM_CAL_MAX, &op_PARAM_CAL_RESET, &op_PRM, &op_TR,
    &op_TR_POL, &op_TR_TIME, &op_TR_TOG, &op_TR_PULSE, &op_TR_P, &op_CV_SET,
    &op_MUTE, &op_STATE, &op_DEVICE_FLIP, &op_LIVE_OFF, &op_LIVE_O,
    &op_LIVE_DASH, &op_LIVE_D, &op_LIVE_GRID, &op_LIVE_G, &op_LIVE_VARS,
    &op_LIVE_V, &op_PRINT, &op_PRT, &op_CV_GET, &op_CV_CAL, &op_CV_CAL_RESET,

    // maths
    &op_ADD, &op_SUB, &op_MUL, &op_DIV, &op_MOD, &op_RAND, &op_RND, &op_RRAND,
    &op_RRND, &op_R, &op_R_MIN, &op_R_MAX, &op_TOSS, &op_MIN, &op_MAX, &op_LIM,
    &op_WRAP, &op_WRP, &op_QT, &op_QT_S, &op_QT_CS, &op_QT_B, &op_QT_BX,
    &op_AVG, &op_EQ, &op_NE, &op_LT, &op_GT, &op_LTE, &op_GTE, &op_INR,
    &op_OUTR, &op_INRI, &op_OUTRI, &op_NZ, &op_EZ, &op_RSH, &op_LSH, &op_LROT,
    &op_RROT, &op_EXP, &op_ABS, &op_SGN, &op_AND, &op_OR, &op_AND3, &op_OR3,
    &op_AND4, &op_OR4, &op_JI, &op_SCALE, &op_SCL, &op_SCALE0, &op_SCL0, &op_N,
    &op_VN, &op_HZ, &op_N_S, &op_N_C, &op_N_CS, &op_N_B, &op_N_BX, &op_V,
    &op_VV, &op_ER, &op_NR, &op_DR_T, &op_DR_P, &op_DR_V, &op_BPM, &op_BIT_OR,
    &op_BIT_AND, &op_BIT_NOT, &op_BIT_XOR, &op_BSET, &op_BGET, &op_BCLR,
    &op_BTOG, &op_BREV, &op_XOR, &op_CHAOS, &op_CHAOS_R, &op_CHAOS_ALG,
    &op_SYM_PLUS, &op_SYM_DASH, &op_SYM_STAR, &op_SYM_FORWARD_SLASH,
    &op_SYM_PERCENTAGE, &op_SYM_EQUAL_x2, &op_SYM_EXCLAMATION_EQUAL,
    &op_SYM_LEFT_ANGLED, &op_SYM_RIGHT_ANGLED, &op_SYM_LEFT_ANGLED_EQUAL,
    &op_SYM_RIGHT_ANGLED_EQUAL, &op_SYM_RIGHT_ANGLED_LEFT_ANGLED,
    &op_SYM_LEFT_ANGLED_RIGHT_ANGLED, &op_SYM_RIGHT_ANGLED_EQUAL_LEFT_ANGLED,
    &op_SYM_LEFT_ANGLED_EQUAL_RIGHT_ANGLED, &op_SYM_EXCLAMATION,
    &op_SYM_LEFT_ANGLED_x2, &op_SYM_RIGHT_ANGLED_x2, &op_SYM_LEFT_ANGLED_x3,
    &op_SYM_RIGHT_ANGLED_x3, &op_SYM_AMPERSAND_x2, &op_SYM_PIPE_x2,
    &op_SYM_AMPERSAND_x3, &op_SYM_PIPE_x3, &op_SYM_AMPERSAND_x4,
    &op_SYM_PIPE_x4, &op_TIF,

    // stack
    &op_S_ALL, &op_S_POP, &op_S_CLR, &op_S_L,

    // controlflow
    &op_SCRIPT, &op_SYM_DOLLAR, &op_SCRIPT_POL, &op_SYM_DOLLAR_POL, &op_KILL,
    &op_SCENE, &op_SCENE_G, &op_SCENE_P, &op_BREAK, &op_BRK, &op_SYNC,
    &op_SYM_DOLLAR_F, &op_SYM_DOLLAR_F1, &op_SYM_DOLLAR_F2, &op_SYM_DOLLAR_L,
    &op_SYM_DOLLAR_L1, &op_SYM_DOLLAR_L2, &op_SYM_DOLLAR_S, &op_SYM_DOLLAR_S1,
    &op_SYM_DOLLAR_S2, &op_I1, &op_I2, &op_FR,

    // delay
    &op_DEL_CLR,

    // seed
    &op_SEED, &op_RAND_SEED, &op_SYM_RAND_SD, &op_SYM_R_SD, &op_TOSS_SEED,
    &op_SYM_TOSS_SD, &op_PROB_SEED, &op_SYM_PROB_SD, &op_DRUNK_SEED,
    &op_SYM_DRUNK_SD, &op_P_SEED, &op_SYM_P_SD,

    &op_MI_SYM_DOLLAR, &op_MI_LN, &op_MI_LNV, &op_MI_LV, &op_MI_LVV, &op_MI_LO,
    &op_MI_LC, &op_MI_LCC, &op_MI_LCCV, &op_MI_NL, &op_MI_N, &op_MI_NV,
    &op_MI_V, &op_MI_VV, &op_MI_OL, &op_MI_O, &op_MI_CL, &op_MI_C, &op_MI_CC,
    &op_MI_CCV, &op_MI_LCH, &op_MI_NCH, &op_MI_OCH, &op_MI_CCH, &op_MI_LE,
    &op_MI_CLKD, &op_MI_CLKR
};

/////////////////////////////////////////////////////////////////
// MODS /////////////////////////////////////////////////////////

const tele_mod_t *tele_mods[E_MOD__LENGTH] = {
    // controlflow
    &mod_IF, &mod_ELIF, &mod_ELSE, &mod_L, &mod_W, &mod_EVERY, &mod_EV,
    &mod_SKIP, &mod_OTHER, &mod_PROB,

    // delay
    &mod_DEL, &mod_DEL_X, &mod_DEL_R, &mod_DEL_G, &mod_DEL_B,

    // pattern
    &mod_P_MAP, &mod_PN_MAP,

    // stack
    &mod_S,
};

/////////////////////////////////////////////////////////////////
// HELPERS //////////////////////////////////////////////////////

void op_peek_i16(const void *data, scene_state_t *ss, exec_state_t *NOTUSED(es),
                 command_state_t *cs) {
    char *base = (char *)ss;
    size_t offset = (size_t)data;
    int16_t *ptr = (int16_t *)(base + offset);
    cs_push(cs, *ptr);
}

void op_poke_i16(const void *data, scene_state_t *ss, exec_state_t *NOTUSED(es),
                 command_state_t *cs) {
    char *base = (char *)ss;
    size_t offset = (size_t)data;
    int16_t *ptr = (int16_t *)(base + offset);
    *ptr = cs_pop(cs);
    tele_vars_updated();
}

void op_simple_i2c(const void *data, scene_state_t *NOTUSED(ss),
                   exec_state_t *NOTUSED(es), command_state_t *cs) {
    int16_t message = (intptr_t)data;
    int16_t value = cs_pop(cs);

    uint8_t address = message & 0xF0;
    uint8_t message_type = message & 0xFF;

    uint8_t buffer[3] = { message_type, value >> 8, value & 0xFF };

    tele_ii_tx(address, buffer, 3);
}
