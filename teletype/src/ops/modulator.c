#include "ops/modulator.h"

#include "helpers.h"
#include "teletype.h"

// Inert C-VM stubs. Real behavior lives in the native TT2 op handlers
// (TeletypeNativeOps.cpp), which dispatch MO.* through tt2NativeOpTable. These
// ops are never evaluated by the legacy scene_state_t VM.
static void op_MO_noop_get(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                           exec_state_t *NOTUSED(es), command_state_t *NOTUSED(cs)) {}
static void op_MO_noop_set(const void *NOTUSED(data), scene_state_t *NOTUSED(ss),
                           exec_state_t *NOTUSED(es), command_state_t *NOTUSED(cs)) {}

// Canonical ops. arity = GET form (slot, or slot+addr for MO.P); set adds a value.
const tele_op_t op_MO       = MAKE_GET_OP(MO, op_MO_noop_get, 1, true);
const tele_op_t op_MO_P     = MAKE_GET_SET_OP(MO.P, op_MO_noop_get, op_MO_noop_set, 2, true);
const tele_op_t op_MO_SHAPE = MAKE_GET_SET_OP(MO.SHAPE, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_RATE  = MAKE_GET_SET_OP(MO.RATE, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_DEPTH = MAKE_GET_SET_OP(MO.DEPTH, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_MODE  = MAKE_GET_SET_OP(MO.MODE, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_OFF   = MAKE_GET_SET_OP(MO.OFF, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_TRIG  = MAKE_GET_OP(MO.TRIG, op_MO_noop_get, 1, false);

// Short aliases (same name/arity; native table points both indices at one handler).
const tele_op_t op_MO_S = MAKE_ALIAS_OP(MO.S, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_R = MAKE_ALIAS_OP(MO.R, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_D = MAKE_ALIAS_OP(MO.D, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_M = MAKE_ALIAS_OP(MO.M, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_O = MAKE_ALIAS_OP(MO.O, op_MO_noop_get, op_MO_noop_set, 1, true);
const tele_op_t op_MO_T = MAKE_ALIAS_OP(MO.T, op_MO_noop_get, NULL, 1, false);

// LFO.C — clocked-rate modulator alias (native-only, like MO.*); real behavior
// is in TeletypeNativeOps.cpp. arity 2 (slot + divisor), mirrors LFO.R.
const tele_op_t op_LFO_C = MAKE_GET_OP(LFO.C, op_MO_noop_get, 2, false);
