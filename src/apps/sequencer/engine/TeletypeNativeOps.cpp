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
// Pitch ops — N (semitone -> value) and V (volts -> value). Values are upstream
// Teletype verbatim (equal-temperament ET + 1V/oct table_v) so scripts are
// copy-paste compatible; output offset/scaling is applied downstream at the
// Performer track level, not here.
// ---------------------------------------------------------------------------

// ET[128]: semitone number -> raw value (1V/oct, 14-bit domain). Verbatim from
// teletype/libavr32/src/music.c. High notes exceed 16383 by design — the value
// is what the script sees; the CV write clamps.
static const int16_t tt2EtTable[128] = {
    0,     137,   273,   410,   546,   683,   819,   956,   1092,  1229,  1365,  1502,
    1638,  1775,  1911,  2048,  2185,  2321,  2458,  2594,  2731,  2867,  3004,  3140,
    3277,  3413,  3550,  3686,  3823,  3959,  4096,  4233,  4369,  4506,  4642,  4779,
    4915,  5052,  5188,  5325,  5461,  5598,  5734,  5871,  6007,  6144,  6281,  6417,
    6554,  6690,  6827,  6963,  7100,  7236,  7373,  7509,  7646,  7782,  7919,  8055,
    8192,  8329,  8465,  8602,  8738,  8875,  9011,  9148,  9284,  9421,  9557,  9694,
    9830,  9967,  10103, 10240, 10377, 10513, 10650, 10786, 10923, 11059, 11196, 11332,
    11469, 11605, 11742, 11878, 12015, 12151, 12288, 12425, 12561, 12698, 12834, 12971,
    13107, 13244, 13380, 13517, 13653, 13790, 13926, 14063, 14199, 14336, 14473, 14609,
    14746, 14882, 15019, 15155, 15292, 15428, 15565, 15701, 15838, 15974, 16111, 16247,
    16384, 16521, 16657, 16794, 16930, 17067, 17203, 17340,
};

// table_v[11]: integer volts 0..10 -> raw value. Verbatim from teletype/src/table.c.
static const int16_t tt2VTable[11] = {
    0, 1638, 3277, 4915, 6554, 8192, 9830, 11469, 13107, 14746, 16384,
};

static int16_t noteNumberToVolts(int16_t note) {
    if (note < 0) {
        if (note < -127) note = -127;
        return int16_t(-tt2EtTable[-note]);
    }
    if (note > 127) note = 127;
    return tt2EtTable[note];
}

static void opN(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, noteNumberToVolts(a), error);
}

static void opV(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (a > 10) a = 10; else if (a < -10) a = -10;
    if (a < 0) pushStack(stack, stackSize, int16_t(-tt2VTable[-a]), error);
    else pushStack(stack, stackSize, tt2VTable[a], error);
}

// ---------------------------------------------------------------------------
// Logic / range ops (boolean results are 1/0)
// ---------------------------------------------------------------------------

static void opAnd(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    pushStack(stack, stackSize, int16_t(a && b), error);
}

static void opOr(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    pushStack(stack, stackSize, int16_t(a || b), error);
}

static void opAnd3(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    pushStack(stack, stackSize, int16_t(a && b && c), error);
}

static void opOr3(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    pushStack(stack, stackSize, int16_t(a || b || c), error);
}

static void opAnd4(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0, d = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    if (!popStack(stack, stackSize, d, error)) return;
    pushStack(stack, stackSize, int16_t(a && b && c && d), error);
}

static void opOr4(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0, d = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    if (!popStack(stack, stackSize, d, error)) return;
    pushStack(stack, stackSize, int16_t(a || b || c || d), error);
}

static void opNz(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, int16_t(a != 0), error);
}

static void opEz(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, int16_t(a == 0), error);
}

// LIM value lo hi -> clamp value to [lo, hi]
static void opLim(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t i = 0, a = 0, b = 0;
    if (!popStack(stack, stackSize, i, error)) return;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    pushStack(stack, stackSize, int16_t(i < a ? a : (i > b ? b : i)), error);
}

// WRAP value lo hi -> wrap value into the [lo, hi] cycle
static void opWrap(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t i = 0, a = 0, b = 0, c = 0;
    if (!popStack(stack, stackSize, i, error)) return;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (a < b) {
        c = b - a + 1;
        while (i >= b) i -= c;
        while (i < a) i += c;
    } else {
        c = a - b + 1;
        while (i >= a) i -= c;
        while (i < b) i += c;
    }
    pushStack(stack, stackSize, i, error);
}

static void opAvg(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    int32_t ret = ((int32_t(a) * 2) + (int32_t(b) * 2)) / 2;
    if (ret % 2) ret += 1;
    pushStack(stack, stackSize, int16_t(ret / 2), error);
}

// INR/OUTR family: lo x hi (upstream arg order)
static bool popRange(int16_t *stack, uint8_t &stackSize,
                     int16_t &lo, int16_t &x, int16_t &hi, TT2EvalError &error) {
    if (!popStack(stack, stackSize, lo, error)) return false;
    if (!popStack(stack, stackSize, x, error)) return false;
    if (!popStack(stack, stackSize, hi, error)) return false;
    return true;
}

static void opInr(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t lo = 0, x = 0, hi = 0;
    if (!popRange(stack, stackSize, lo, x, hi, error)) return;
    pushStack(stack, stackSize, int16_t((lo < x) && (x < hi)), error);
}

static void opOutr(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t lo = 0, x = 0, hi = 0;
    if (!popRange(stack, stackSize, lo, x, hi, error)) return;
    pushStack(stack, stackSize, int16_t((lo > x) || (x > hi)), error);
}

static void opInri(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t lo = 0, x = 0, hi = 0;
    if (!popRange(stack, stackSize, lo, x, hi, error)) return;
    pushStack(stack, stackSize, int16_t((lo <= x) && (x <= hi)), error);
}

static void opOutri(TT2Runtime &, TT2OutputState &, const TeletypeProgram *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t lo = 0, x = 0, hi = 0;
    if (!popRange(stack, stackSize, lo, x, hi, error)) return;
    pushStack(stack, stackSize, int16_t((lo >= x) || (x >= hi)), error);
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
// Queue ops — TT2_Q_LENGTH ring; get vs set by leftmost position + arg count.
// Q.RND (rand source) and Q.2P / Q.P2 (pattern store) deferred to later batches.
// ---------------------------------------------------------------------------

static void opQ(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        for (int i = TT2_Q_LENGTH - 1; i > 0; --i) v.q[i] = v.q[i - 1];
        v.q[0] = val;
        if (v.q_grow && v.q_n < TT2_Q_LENGTH) v.q_n++;
    } else {
        pushStack(stack, stackSize, v.q[v.q_n - 1], error);
        if (v.q_grow && v.q_n > 1) v.q_n--;
    }
}

static void opQN(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        if (a < 1) a = 1; else if (a > TT2_Q_LENGTH) a = TT2_Q_LENGTH;
        v.q_n = a;
    } else {
        pushStack(stack, stackSize, v.q_n, error);
    }
}

static void opQAvg(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        for (int i = 0; i < TT2_Q_LENGTH; ++i) v.q[i] = a;
    } else if (v.q_n == 0) {
        pushStack(stack, stackSize, 0, error);
    } else {
        int32_t avg = 0;
        for (int i = 0; i < v.q_n; ++i) avg += v.q[i];
        avg = (avg * 2) / v.q_n;
        if (avg % 2) avg += 1;
        pushStack(stack, stackSize, int16_t(avg / 2), error);
    }
}

static void opQClr(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t first = 0;
    bool doSet = isSet && stackSize >= 1;
    if (doSet && !popStack(stack, stackSize, first, error)) return;
    v.q_n = 1;
    for (int i = 0; i < TT2_Q_LENGTH; ++i) v.q[i] = 0;
    if (doSet) v.q[0] = first;
}

static void opQGrw(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        v.q_grow = (a < 1) ? 0 : 1;
        if (!v.q_grow && v.q_n < 1) v.q_n = 1;
    } else {
        pushStack(stack, stackSize, v.q_grow, error);
    }
}

static void opQSum(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t sum = 0;
    for (int i = 0; i < v.q_n; ++i) sum += v.q[i];
    pushStack(stack, stackSize, sum, error);
}

static void opQMin(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 1) {
        int16_t mn = 0;
        if (!popStack(stack, stackSize, mn, error)) return;
        for (int i = 0; i < v.q_n; ++i) if (v.q[i] < mn) v.q[i] = mn;
    } else {
        int16_t mn = 32767;
        for (int i = 0; i < v.q_n; ++i) if (v.q[i] < mn) mn = v.q[i];
        pushStack(stack, stackSize, mn, error);
    }
}

static void opQMax(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 1) {
        int16_t mx = 0;
        if (!popStack(stack, stackSize, mx, error)) return;
        for (int i = 0; i < v.q_n; ++i) if (v.q[i] > mx) v.q[i] = mx;
    } else {
        int16_t mx = -32768;
        for (int i = 0; i < v.q_n; ++i) if (v.q[i] > mx) mx = v.q[i];
        pushStack(stack, stackSize, mx, error);
    }
}

// Selection sort over [lo, hi).
static void qSortRange(int16_t *q, int lo, int hi) {
    for (int i = lo; i < hi; ++i) {
        int16_t mn = 32767;
        int mnIdx = i;
        for (int j = i; j < hi; ++j) {
            if (q[j] < mn) { mn = q[j]; mnIdx = j; }
        }
        int16_t tmp = q[mnIdx]; q[mnIdx] = q[i]; q[i] = tmp;
    }
}

static void opQSrt(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 1) {
        int16_t bound = 0;
        if (!popStack(stack, stackSize, bound, error)) return;
        int lo, hi;
        if (bound > 0)      { lo = 0; hi = bound > v.q_n ? v.q_n : bound; }
        else if (bound < 0) { hi = v.q_n; lo = -bound > v.q_n ? v.q_n : -bound; }
        else                { lo = 0; hi = v.q_n; }
        qSortRange(v.q, lo, hi);
    } else {
        qSortRange(v.q, 0, v.q_n);
    }
}

static void opQRev(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *, uint8_t &, bool, TT2EvalError &) {
    auto &v = runtime.variables;
    for (int i = 0; i < v.q_n / 2; ++i) {
        int16_t tmp = v.q[i];
        v.q[i] = v.q[v.q_n - 1 - i];
        v.q[v.q_n - 1 - i] = tmp;
    }
}

static void opQSh(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 1) {
        int16_t n = 0;
        if (!popStack(stack, stackSize, n, error)) return;
        if (n > 0) n = n % v.q_n;
        else if (n < 0) n = v.q_n - (-n % v.q_n);
        if (!n) return;
        int16_t tmp[TT2_Q_LENGTH];
        for (int i = 0; i < v.q_n; ++i) tmp[i] = v.q[i];
        for (int i = 0; i < v.q_n; ++i) v.q[(i + n) % v.q_n] = tmp[i];
    } else {
        int16_t tmp = v.q[v.q_n - 1];
        for (int i = v.q_n - 1; i > 0; --i) v.q[i] = v.q[i - 1];
        v.q[0] = tmp;
    }
}

// Q.ADD/SUB/MUL/DIV/MOD: get (1 arg) applies to all; set (2 args: amount, index)
// applies to one slot. Upstream pop order is amount-first, index-second.
static int qClampIndex(int16_t i, int16_t q_n) {
    if (i < 0) return 0;
    if (i > q_n - 1) return q_n - 1;
    return i;
}

static void opQAdd(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t amt = 0;
    if (!popStack(stack, stackSize, amt, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t idx = 0;
        if (!popStack(stack, stackSize, idx, error)) return;
        idx = qClampIndex(idx, v.q_n);
        v.q[idx] = v.q[idx] + amt;
    } else {
        for (int i = 0; i < v.q_n; ++i) v.q[i] = v.q[i] + amt;
    }
}

static void opQSub(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t amt = 0;
    if (!popStack(stack, stackSize, amt, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t idx = 0;
        if (!popStack(stack, stackSize, idx, error)) return;
        idx = qClampIndex(idx, v.q_n);
        v.q[idx] = v.q[idx] - amt;
    } else {
        for (int i = 0; i < v.q_n; ++i) v.q[i] = v.q[i] - amt;
    }
}

static void opQMul(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t amt = 0;
    if (!popStack(stack, stackSize, amt, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t idx = 0;
        if (!popStack(stack, stackSize, idx, error)) return;
        idx = qClampIndex(idx, v.q_n);
        v.q[idx] = v.q[idx] * amt;
    } else {
        for (int i = 0; i < v.q_n; ++i) v.q[i] = v.q[i] * amt;
    }
}

static void opQDiv(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t amt = 0;
    if (!popStack(stack, stackSize, amt, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t idx = 0;
        if (!popStack(stack, stackSize, idx, error)) return;
        if (amt == 0) return;
        idx = qClampIndex(idx, v.q_n);
        v.q[idx] = v.q[idx] / amt;
    } else if (amt != 0) {
        for (int i = 0; i < v.q_n; ++i) v.q[i] = v.q[i] / amt;
    }
}

static void opQMod(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t amt = 0;
    if (!popStack(stack, stackSize, amt, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t idx = 0;
        if (!popStack(stack, stackSize, idx, error)) return;
        if (amt == 0) return;
        idx = qClampIndex(idx, v.q_n);
        v.q[idx] = v.q[idx] % amt;
    } else if (amt != 0) {
        for (int i = 0; i < v.q_n; ++i) v.q[i] = v.q[i] % amt;
    }
}

static void opQI(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t idx = 0;
    if (!popStack(stack, stackSize, idx, error)) return;
    if (idx < 0) idx = 0;
    if (idx > TT2_Q_LENGTH - 1) idx = TT2_Q_LENGTH - 1;
    if (isSet && stackSize >= 1) {
        int16_t value = 0;
        if (!popStack(stack, stackSize, value, error)) return;
        v.q[idx] = value;
    } else {
        pushStack(stack, stackSize, v.q[idx], error);
    }
}

// ---------------------------------------------------------------------------
// Command-stack ops — S.* run/clear/measure the deferred-command stack the S
// mod pushes onto. S.ALL/S.POP execute stored commands through the evaluator.
// ---------------------------------------------------------------------------

static void runStoredCommand(const TT2RuntimeCommand &body, TT2Runtime &runtime,
                             TT2OutputState &output, const TeletypeProgram *program) {
    TT2Command cmd;
    cmd.length = body.length;
    for (int k = 0; k < TT2_COMMAND_MAX_LENGTH; ++k) {
        cmd.tag[k] = body.tag[k];
        cmd.value[k] = body.value[k];
    }
    evaluateCommand(cmd, runtime, output, program);
}

// S.ALL — run every stacked command (most-recent first), then clear.
static void opSAll(TT2Runtime &runtime, TT2OutputState &output,
                   const TeletypeProgram *program, int16_t *, uint8_t &, bool,
                   TT2EvalError &) {
    uint8_t n = runtime.stack.top;
    for (uint8_t i = 0; i < n; ++i) {
        runStoredCommand(runtime.stack.commands[n - 1 - i], runtime, output, program);
    }
    runtime.stack.top = 0;
}

// S.POP — run the most-recently pushed command, leaving the rest.
static void opSPop(TT2Runtime &runtime, TT2OutputState &output,
                   const TeletypeProgram *program, int16_t *, uint8_t &, bool,
                   TT2EvalError &) {
    if (runtime.stack.top > 0) {
        runtime.stack.top--;
        runStoredCommand(runtime.stack.commands[runtime.stack.top], runtime, output, program);
    }
}

static void opSClr(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                   int16_t *, uint8_t &, bool, TT2EvalError &) {
    runtime.stack.top = 0;
}

static void opSL(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, int16_t(runtime.stack.top), error);
}

// ---------------------------------------------------------------------------
// Pattern ops — P.* (working pattern p_n) and PN.* (explicit pattern arg) over
// the persisted TeletypeProgram.patterns[4] store. Semantics match patterns.c.
// ---------------------------------------------------------------------------

static int16_t normalisePn(int16_t pn) {
    if (pn < 0) return 0;
    if (pn >= TT2_PATTERN_COUNT) return TT2_PATTERN_COUNT - 1;
    return pn;
}

// Patterns are mutable scene state persisted in TeletypeProgram; the evaluator
// reaches the program through a const pointer (scripts don't change mid-run),
// so pattern-write ops cast it away. The TT2Track::_program object is non-const.
static TT2Pattern *mutablePattern(const TeletypeProgram *program, int16_t pn) {
    if (!program) return nullptr;
    return &const_cast<TeletypeProgram *>(program)->patterns[normalisePn(pn)];
}

// Negative idx counts from the back (upstream normalise_idx); clamp to range.
static int16_t normaliseIdx(const TT2Pattern &p, int16_t idx) {
    int16_t len = int16_t(p.len);
    if (idx < 0) {
        if (idx < -len) idx = 0;
        else idx = len + idx;
    }
    if (idx >= TT2_PATTERN_LENGTH) idx = TT2_PATTERN_LENGTH - 1;
    if (idx < 0) idx = 0;
    return idx;
}

// P.N — working pattern selector.
static void opPatternN(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *,
                       int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        runtime.variables.p_n = normalisePn(a);
    } else {
        pushStack(stack, stackSize, runtime.variables.p_n, error);
    }
}

// --- value workers (shared by P.* and PN.*) -------------------------------

static int16_t patternGetVal(const TeletypeProgram *program, int16_t pn, int16_t idx) {
    const TT2Pattern &p = program->patterns[normalisePn(pn)];
    return p.val[normaliseIdx(p, idx)];
}

static void patternSetVal(const TeletypeProgram *program, int16_t pn, int16_t idx,
                          int16_t val) {
    TT2Pattern *p = mutablePattern(program, pn);
    if (p) p->val[normaliseIdx(*p, idx)] = val;
}

static void patternSetLen(const TeletypeProgram *program, int16_t pn, int16_t l) {
    TT2Pattern *p = mutablePattern(program, pn);
    if (!p) return;
    if (l < 0) l = 0; else if (l > TT2_PATTERN_LENGTH) l = TT2_PATTERN_LENGTH;
    p->len = uint16_t(l);
}

static void patternSetIdx(const TeletypeProgram *program, int16_t pn, int16_t i) {
    TT2Pattern *p = mutablePattern(program, pn);
    if (!p) return;
    i = normaliseIdx(*p, i);
    int16_t len = int16_t(p->len);
    if (i < 0 || len == 0) p->idx = 0;
    else if (i >= len) p->idx = int16_t(len - 1);
    else p->idx = i;
}

// P / PN — indexed value access. P: idx [val] (working pattern). PN: pn idx [val].
static void opP(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *program,
                int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popStack(stack, stackSize, idx, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        patternSetVal(program, runtime.variables.p_n, idx, val);
    } else {
        pushStack(stack, stackSize, patternGetVal(program, runtime.variables.p_n, idx), error);
    }
}

static void opPN(TT2Runtime &, TT2OutputState &, const TeletypeProgram *program,
                 int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0, idx = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (!popStack(stack, stackSize, idx, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        patternSetVal(program, pn, idx, val);
    } else {
        pushStack(stack, stackSize, patternGetVal(program, pn, idx), error);
    }
}

// P.L / PN.L — length.
static void opPL(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *program,
                 int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t l = 0;
        if (!popStack(stack, stackSize, l, error)) return;
        patternSetLen(program, runtime.variables.p_n, l);
    } else {
        pushStack(stack, stackSize, int16_t(program->patterns[normalisePn(runtime.variables.p_n)].len), error);
    }
}

static void opPNL(TT2Runtime &, TT2OutputState &, const TeletypeProgram *program,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t l = 0;
        if (!popStack(stack, stackSize, l, error)) return;
        patternSetLen(program, pn, l);
    } else {
        pushStack(stack, stackSize, int16_t(program->patterns[normalisePn(pn)].len), error);
    }
}

// P.I / PN.I — working index.
static void opPI(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *program,
                 int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t i = 0;
        if (!popStack(stack, stackSize, i, error)) return;
        patternSetIdx(program, runtime.variables.p_n, i);
    } else {
        pushStack(stack, stackSize, program->patterns[normalisePn(runtime.variables.p_n)].idx, error);
    }
}

static void opPNI(TT2Runtime &, TT2OutputState &, const TeletypeProgram *program,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t i = 0;
        if (!popStack(stack, stackSize, i, error)) return;
        patternSetIdx(program, pn, i);
    } else {
        pushStack(stack, stackSize, program->patterns[normalisePn(pn)].idx, error);
    }
}

// P.HERE / PN.HERE — value at the working index (no advance).
static void opPHere(TT2Runtime &runtime, TT2OutputState &, const TeletypeProgram *program,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = normalisePn(runtime.variables.p_n);
    const TT2Pattern &p = program->patterns[pn];
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2Pattern *m = mutablePattern(program, pn);
        if (m) m->val[m->idx] = a;
    } else {
        pushStack(stack, stackSize, p.val[p.idx], error);
    }
}

static void opPNHere(TT2Runtime &, TT2OutputState &, const TeletypeProgram *program,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    pn = normalisePn(pn);
    const TT2Pattern &p = program->patterns[pn];
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2Pattern *m = mutablePattern(program, pn);
        if (m) m->val[m->idx] = a;
    } else {
        pushStack(stack, stackSize, p.val[p.idx], error);
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
            table[E_OP_N]        = opN;
            table[E_OP_V]        = opV;
            table[E_OP_AND]                = opAnd;
            table[E_OP_SYM_AMPERSAND_x2]   = opAnd;
            table[E_OP_OR]                 = opOr;
            table[E_OP_SYM_PIPE_x2]        = opOr;
            table[E_OP_AND3]               = opAnd3;
            table[E_OP_SYM_AMPERSAND_x3]   = opAnd3;
            table[E_OP_OR3]                = opOr3;
            table[E_OP_SYM_PIPE_x3]        = opOr3;
            table[E_OP_AND4]               = opAnd4;
            table[E_OP_SYM_AMPERSAND_x4]   = opAnd4;
            table[E_OP_OR4]                = opOr4;
            table[E_OP_SYM_PIPE_x4]        = opOr4;
            table[E_OP_NZ]       = opNz;
            table[E_OP_EZ]       = opEz;
            table[E_OP_LIM]      = opLim;
            table[E_OP_WRAP]     = opWrap;
            table[E_OP_WRP]      = opWrap;
            table[E_OP_AVG]      = opAvg;
            table[E_OP_INR]      = opInr;
            table[E_OP_OUTR]     = opOutr;
            table[E_OP_INRI]     = opInri;
            table[E_OP_OUTRI]    = opOutri;
            table[E_OP_Q]        = opQ;
            table[E_OP_Q_N]      = opQN;
            table[E_OP_Q_AVG]    = opQAvg;
            table[E_OP_Q_CLR]    = opQClr;
            table[E_OP_Q_GRW]    = opQGrw;
            table[E_OP_Q_SUM]    = opQSum;
            table[E_OP_Q_MIN]    = opQMin;
            table[E_OP_Q_MAX]    = opQMax;
            table[E_OP_Q_SRT]    = opQSrt;
            table[E_OP_Q_REV]    = opQRev;
            table[E_OP_Q_SH]     = opQSh;
            table[E_OP_Q_ADD]    = opQAdd;
            table[E_OP_Q_SUB]    = opQSub;
            table[E_OP_Q_MUL]    = opQMul;
            table[E_OP_Q_DIV]    = opQDiv;
            table[E_OP_Q_MOD]    = opQMod;
            table[E_OP_Q_I]      = opQI;
            table[E_OP_S_ALL]    = opSAll;
            table[E_OP_S_POP]    = opSPop;
            table[E_OP_S_CLR]    = opSClr;
            table[E_OP_S_L]      = opSL;
            table[E_OP_P_N]      = opPatternN;
            table[E_OP_P]        = opP;
            table[E_OP_PN]       = opPN;
            table[E_OP_P_L]      = opPL;
            table[E_OP_PN_L]     = opPNL;
            table[E_OP_P_I]      = opPI;
            table[E_OP_PN_I]     = opPNI;
            table[E_OP_P_HERE]   = opPHere;
            table[E_OP_PN_HERE]  = opPNHere;
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
