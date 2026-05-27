#include "TT2Evaluator.h"

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

static void opA(TT2Runtime &runtime, TT2OutputState &, int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.a = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.a, error);
    }
}

static void opB(TT2Runtime &runtime, TT2OutputState &, int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.b = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.b, error);
    }
}

static void opX(TT2Runtime &runtime, TT2OutputState &, int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.x = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.x, error);
    }
}

static void opI(TT2Runtime &runtime, TT2OutputState &, int16_t *stack,
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

static void opAdd(TT2Runtime &, TT2OutputState &, int16_t *stack,
                  uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t b = 0;
    int16_t a = 0;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, a + b, error);
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

static void opCv(TT2Runtime &runtime, TT2OutputState &output, int16_t *stack,
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

static void opTr(TT2Runtime &runtime, TT2OutputState &output, int16_t *stack,
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

static void opM(TT2Runtime &runtime, TT2OutputState &, int16_t *stack,
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

static void opMAct(TT2Runtime &runtime, TT2OutputState &, int16_t *stack,
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
            table[E_OP_CV]       = opCv;
            table[E_OP_TR]       = opTr;
            table[E_OP_M]        = opM;
            table[E_OP_M_ACT]    = opMAct;
        }
    };
    OpTableBuilder opTableBuilder;
}

const TT2OpFunc *tt2NativeOpTable = opTableBuilder.table;
const size_t tt2NativeOpCount = E_OP__LENGTH;
