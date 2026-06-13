#include "TT2Evaluator.h"
#include "TT2Runner.h"

// ---------------------------------------------------------------------------
// Helper: pop a 1-based output index and convert to 0-based.
// Returns true on success, sets error on failure.
// ---------------------------------------------------------------------------
static bool popOutputIndex(int16_t *stack, uint8_t &stackSize,
                           int16_t &outIndex, TT2EvalError &error,
                           int maxCount) {
    int16_t oneBased = 0;
    if (!popStack(stack, stackSize, oneBased, error)) {
        return false;
    }
    int16_t zeroBased = oneBased - 1;
    if (zeroBased < 0 || zeroBased >= maxCount) {
        error = TT2EvalError::OutOfRange;
        return false;
    }
    outIndex = zeroBased;
    return true;
}

// ---------------------------------------------------------------------------
// Variable ops
// ---------------------------------------------------------------------------

static void opA(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.a = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.a, error);
    }
}

static void opB(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.b = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.b, error);
    }
}

static void opX(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.x = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.x, error);
    }
}

static void opI(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        tt2ActiveI(runtime) = val;
    } else {
        pushStack(stack, stackSize, tt2ActiveI(runtime), error);
    }
}

// ---------------------------------------------------------------------------
// Math ops
// ---------------------------------------------------------------------------

static void opAdd(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack,
                  uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t b = 0;
    int16_t a = 0;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, a + b, error);
}

// Binary ops pop the leftmost arg first (matches opCv index-first order).
static bool popBinary(int16_t *stack, uint8_t &stackSize,
                      int16_t &first, int16_t &second, TT2EvalError &error) {
    if (!popStack(stack, stackSize, first, error)) return false;
    if (!popStack(stack, stackSize, second, error)) return false;
    return true;
}

static void opSub(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first - second), error);
}

static void opMul(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    int32_t r = int32_t(first) * int32_t(second);
    if (r > 32767) r = 32767;
    if (r < -32768) r = -32768;
    pushStack(stack, stackSize, int16_t(r), error);
}

static void opDiv(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(second != 0 ? first / second : 0), error);
}

static void opMod(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(second != 0 ? first % second : 0), error);
}

static void opMin(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(second > first ? first : second), error);
}

static void opMax(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first > second ? first : second), error);
}

static void opEq(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first == second), error);
}

static void opNe(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first != second), error);
}

static void opLt(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first < second), error);
}

static void opGt(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first > second), error);
}

static void opLte(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first <= second), error);
}

static void opGte(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first >= second), error);
}

static void opAbs(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, int16_t(a < 0 ? -a : a), error);
}

static void opSgn(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, int16_t(a > 0 ? 1 : (a < 0 ? -1 : 0)), error);
}

// ---------------------------------------------------------------------------
// Output ops
// ---------------------------------------------------------------------------

static int16_t normaliseCvValue(int16_t value) {
    // Match current Teletype: clamp to [0, 16383], no wrap.
    if (value < 0) return 0;
    if (value > 16383) return 16383;
    return value;
}

static void opCv(TT2Runtime &runtime, TT2OutputState &output,
                 const TeletypeProgram *, int16_t *stack,
                 uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_CV_COUNT)) {
        return;
    }
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        val = normaliseCvValue(val);
        runtime.variables.cv[idx] = val;
        output.cv[idx].targetRaw = val;
        output.cvDirty |= uint8_t(1 << idx);
    } else {
        pushStack(stack, stackSize, runtime.variables.cv[idx], error);
    }
}

static void opTr(TT2Runtime &runtime, TT2OutputState &output,
                 const TeletypeProgram *, int16_t *stack,
                 uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_TR_COUNT)) {
        return;
    }
    if (isSetPosition && stackSize >= 1) {
        int16_t level = 0;
        if (!popStack(stack, stackSize, level, error)) return;
        uint8_t boolLevel = (level != 0) ? 1 : 0;
        runtime.variables.tr[idx] = boolLevel;
        output.tr[idx].level = boolLevel;
        output.trDirty |= uint8_t(1 << idx);
    } else {
        pushStack(stack, stackSize, runtime.variables.tr[idx], error);
    }
}

// ---------------------------------------------------------------------------
// Metro ops
// ---------------------------------------------------------------------------

static void opM(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        if (val < 2) val = 2;
        runtime.variables.m = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.m, error);
    }
}

static void opMAct(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack,
                   uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.m_act = (val != 0) ? 1 : 0;
    } else {
        pushStack(stack, stackSize, runtime.variables.m_act, error);
    }
}

// ---------------------------------------------------------------------------
// Script op — nested script call
// ---------------------------------------------------------------------------

static void opScript(TT2Runtime &runtime, TT2OutputState &output,
                     const TeletypeProgram *program, int16_t *stack,
                     uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t oneBased = 0;
        if (!popStack(stack, stackSize, oneBased, error)) return;
        int16_t zeroBased = oneBased - 1;
        if (zeroBased < 0 || zeroBased >= TT2_SCRIPT_COUNT) {
            error = TT2EvalError::OutOfRange;
            return;
        }
        if (program == nullptr) {
            error = TT2EvalError::NoTrack;
            return;
        }
        TT2EvalResult result = runScript(*program, runtime, output,
                                           static_cast<uint8_t>(zeroBased));
        if (result.error != TT2EvalError::None) {
            error = result.error;
        }
    } else {
        pushStack(stack, stackSize,
                  runtime.exec.depth > 0
                      ? runtime.exec.frames[runtime.exec.depth - 1].script_number + 1
                      : 0,
                  error);
    }
}

// DEL.CLR — clear the delay queue (faithful subset: TT2 has no TR pulse
// timers / persistent op stack to also flush).
static void opDelClr(TT2Runtime &runtime, TT2OutputState &,
                     const TeletypeProgram *, int16_t *,
                     uint8_t &, bool, TT2EvalError &) {
    tt2DelayClear(runtime);
}

// BREAK / BRK — flag the active exec frame so the enclosing L loop stops.
static void opBreak(TT2Runtime &runtime, TT2OutputState &,
                    const TeletypeProgram *, int16_t *,
                    uint8_t &, bool, TT2EvalError &) {
    tt2ActiveBreaking(runtime) = 1;
}

// KILL — clear the stack, disable the metro, and clear the delay queue
// (upstream op_KILL_get; TT2 has no TR pulse timers to also flush).
static void opKill(TT2Runtime &runtime, TT2OutputState &,
                   const TeletypeProgram *, int16_t *,
                   uint8_t &, bool, TT2EvalError &) {
    runtime.stack.top = 0;
    runtime.variables.m_act = 0;
    tt2DelayClear(runtime);
}

// ---------------------------------------------------------------------------
// Native op table
// ---------------------------------------------------------------------------

namespace {
    struct OpTableBuilder {
        TT2OpFunc table[E_OP__LENGTH];
        OpTableBuilder() {
            for (int i = 0; i < E_OP__LENGTH; ++i) {
                table[i] = nullptr;
            }
            table[E_OP_A]        = opA;
            table[E_OP_B]        = opB;
            table[E_OP_X]        = opX;
            table[E_OP_I]        = opI;
            table[E_OP_ADD]      = opAdd;
            table[E_OP_SYM_PLUS] = opAdd;
            table[E_OP_SUB]      = opSub;
            table[E_OP_SYM_DASH] = opSub;
            table[E_OP_MUL]      = opMul;
            table[E_OP_SYM_STAR] = opMul;
            table[E_OP_DIV]                = opDiv;
            table[E_OP_SYM_FORWARD_SLASH]  = opDiv;
            table[E_OP_MOD]                = opMod;
            table[E_OP_SYM_PERCENTAGE]     = opMod;
            table[E_OP_MIN]      = opMin;
            table[E_OP_MAX]      = opMax;
            table[E_OP_EQ]                   = opEq;
            table[E_OP_SYM_EQUAL_x2]         = opEq;
            table[E_OP_NE]                   = opNe;
            table[E_OP_SYM_EXCLAMATION_EQUAL] = opNe;
            table[E_OP_LT]                   = opLt;
            table[E_OP_SYM_LEFT_ANGLED]      = opLt;
            table[E_OP_GT]                   = opGt;
            table[E_OP_SYM_RIGHT_ANGLED]     = opGt;
            table[E_OP_LTE]                      = opLte;
            table[E_OP_SYM_LEFT_ANGLED_EQUAL]    = opLte;
            table[E_OP_GTE]                      = opGte;
            table[E_OP_SYM_RIGHT_ANGLED_EQUAL]   = opGte;
            table[E_OP_ABS]      = opAbs;
            table[E_OP_SGN]      = opSgn;
            table[E_OP_CV]       = opCv;
            table[E_OP_TR]       = opTr;
            table[E_OP_M]        = opM;
            table[E_OP_M_ACT]    = opMAct;
            table[E_OP_SCRIPT]   = opScript;
            table[E_OP_DEL_CLR]  = opDelClr;
            table[E_OP_BREAK]    = opBreak;
            table[E_OP_BRK]      = opBreak;
            table[E_OP_KILL]     = opKill;
        }
    };
    OpTableBuilder opTableBuilder;
}

const TT2OpFunc *tt2NativeOpTable = opTableBuilder.table;
const size_t tt2NativeOpCount = E_OP__LENGTH;
