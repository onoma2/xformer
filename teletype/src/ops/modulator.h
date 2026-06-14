#ifndef _OPS_MODULATOR_H_
#define _OPS_MODULATOR_H_

#include "ops/op.h"

// Performer-only modulator control (MO.*). The op bodies are inert stubs: the
// native TT2 evaluator dispatches these through tt2NativeOpTable, not the C VM.
// These structs exist so the parser assigns the right name/arity.
extern const tele_op_t op_MO;
extern const tele_op_t op_MO_P;
extern const tele_op_t op_MO_SHAPE;
extern const tele_op_t op_MO_RATE;
extern const tele_op_t op_MO_DEPTH;
extern const tele_op_t op_MO_MODE;
extern const tele_op_t op_MO_OFF;
extern const tele_op_t op_MO_TRIG;
extern const tele_op_t op_MO_S;
extern const tele_op_t op_MO_R;
extern const tele_op_t op_MO_D;
extern const tele_op_t op_MO_M;
extern const tele_op_t op_MO_O;
extern const tele_op_t op_MO_T;

#endif
