#include "TT2Evaluator.h"
#include "TT2Runner.h"
#include "TT2Host.h"
#include "TeletypeNativeOps.h"

#include "Platform.h"   // CCMRAM_BSS

#include "TT2Helpers.h"  // native euclidean/tresillo/drum/velocity/chaos + scale tables

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

template<typename Cfg>
static void opA(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opB(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opX(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opI(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

// Generate a plain get/set handler over a single runtime variable.
#define TT2_SIMPLE_VAR_OP(fn, member)                                          \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *, \
                   int16_t *stack, uint8_t &stackSize, bool isSetPosition,     \
                   TT2EvalError &error) {                                       \
        if (isSetPosition && stackSize >= 1) {                                 \
            int16_t val = 0;                                                   \
            if (!popStack(stack, stackSize, val, error)) return;              \
            runtime.variables.member = val;                                    \
        } else {                                                               \
            pushStack(stack, stackSize, runtime.variables.member, error);      \
        }                                                                      \
    }

TT2_SIMPLE_VAR_OP(opC, c)
TT2_SIMPLE_VAR_OP(opD, d)
TT2_SIMPLE_VAR_OP(opY, y)
TT2_SIMPLE_VAR_OP(opZ, z)
TT2_SIMPLE_VAR_OP(opT, t)
TT2_SIMPLE_VAR_OP(opOInc, o_inc)
TT2_SIMPLE_VAR_OP(opOMin, o_min)
TT2_SIMPLE_VAR_OP(opOMax, o_max)
TT2_SIMPLE_VAR_OP(opOWrap, o_wrap)
TT2_SIMPLE_VAR_OP(opDrunkMin, drunk_min)
TT2_SIMPLE_VAR_OP(opDrunkMax, drunk_max)
TT2_SIMPLE_VAR_OP(opDrunkWrap, drunk_wrap)
TT2_SIMPLE_VAR_OP(opRMin, r_min)
TT2_SIMPLE_VAR_OP(opRMax, r_max)

// J / K — per-script variables indexed by the active script number.
template<typename Cfg>
static void opJ(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    uint8_t sn = tt2ActiveScriptNumber(runtime);
    if (sn >= Cfg::ScriptCount) sn = 0;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.j[sn] = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.j[sn], error);
    }
}

template<typename Cfg>
static void opK(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    uint8_t sn = tt2ActiveScriptNumber(runtime);
    if (sn >= Cfg::ScriptCount) sn = 0;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.k[sn] = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.k[sn], error);
    }
}

// Restrict value into [min,max]: wrap (clamp to the far bound) or clamp.
static int16_t tt2Normalise(int16_t mn, int16_t mx, int16_t wrap, int16_t v) {
    if (v >= mn && v <= mx) return v;
    if (wrap) return v < mn ? mx : mn;
    return v < mn ? mn : mx;
}

// O — read the current bounded value, then advance by O.INC (wrapped).
template<typename Cfg>
static void opO(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        v.o = val;
        return;
    }
    int16_t cur = tt2Normalise(v.o_min, v.o_max, v.o_wrap, v.o);
    pushStack(stack, stackSize, cur, error);
    v.o = tt2Normalise(v.o_min, v.o_max, v.o_wrap, int16_t(cur + v.o_inc));
}

// DRUNK — read the bounded value, then take a random ±1 step (wrapped).
template<typename Cfg>
static void opDrunk(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        v.drunk = val;
        return;
    }
    int16_t cur = tt2Normalise(v.drunk_min, v.drunk_max, v.drunk_wrap, v.drunk);
    pushStack(stack, stackSize, cur, error);
    int16_t step = int16_t(tt2RngRange(runtime.rng, TT2RngSlot::Drunk, 3)) - 1;  // -1/0/+1
    v.drunk = tt2Normalise(v.drunk_min, v.drunk_max, v.drunk_wrap, int16_t(cur + step));
}

// FLIP — read the current toggle, then flip it.
template<typename Cfg>
static void opFlip(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        v.flip = (val != 0) ? 1 : 0;
        return;
    }
    int16_t f = v.flip;
    pushStack(stack, stackSize, f, error);
    v.flip = (f == 0) ? 1 : 0;
}

// TIME — elapsed ms since reset (when active), masked to 15 bits.
template<typename Cfg>
static void opTime(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        v.time = v.time_act ? int64_t(runtime.clockMs) - val : val;
        return;
    }
    int64_t delta = v.time_act ? int64_t(runtime.clockMs) - v.time : v.time;
    pushStack(stack, stackSize, int16_t(delta & 0x7fff), error);
}

template<typename Cfg>
static void opTimeAct(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                      int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSetPosition && stackSize >= 1) {
        int16_t act = 0;
        if (!popStack(stack, stackSize, act, error)) return;
        if ((act && v.time_act) || (!act && !v.time_act)) return;
        v.time_act = act ? 1 : 0;
        v.time = int64_t(runtime.clockMs) - v.time;
        return;
    }
    pushStack(stack, stackSize, int16_t(v.time_act ? 1 : 0), error);
}

// LAST n — ms since script n last ran (n==0 -> init script), masked to 15 bits.
template<typename Cfg>
static void opLast(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    int sn = a - 1;
    if (sn < -1 || sn >= Cfg::ScriptCount) { pushStack(stack, stackSize, 0, error); return; }
    if (sn == -1) {
        if (Cfg::InitScript < 0) { pushStack(stack, stackSize, 0, error); return; }  // no init script
        sn = Cfg::InitScript;
    }
    uint32_t delta = (runtime.clockMs - runtime.scriptLastMs[sn]) & 0x7fff;
    pushStack(stack, stackSize, int16_t(delta), error);
}

// Pick a uniform value in [min,max] (inclusive), bounds either order.
template<typename Cfg>
static int16_t tt2PushRandom(TT2RuntimeT<Cfg> &runtime, int16_t a, int16_t b) {
    int16_t mn = a < b ? a : b;
    int16_t mx = a < b ? b : a;
    if (mn == mx) return mn;
    uint32_t range = uint32_t(mx - mn) + 1;
    return int16_t(int32_t(tt2RngRange(runtime.rng, TT2RngSlot::Rand, range)) + mn);
}

// RAND a — uniform in [0,a] (or [a,0] for negative a); a==32767 -> full range.
template<typename Cfg>
static void opRand(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (a < 0) {
        pushStack(stack, stackSize, int16_t(-int32_t(tt2RngRange(runtime.rng, TT2RngSlot::Rand, uint32_t(1 - a)))), error);
    } else if (a == 32767) {
        pushStack(stack, stackSize, int16_t(tt2RngNext(runtime.rng, TT2RngSlot::Rand)), error);
    } else {
        pushStack(stack, stackSize, int16_t(tt2RngRange(runtime.rng, TT2RngSlot::Rand, uint32_t(a) + 1)), error);
    }
}

// RRAND a b — uniform in [min(a,b), max(a,b)].
template<typename Cfg>
static void opRrand(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    pushStack(stack, stackSize, tt2PushRandom(runtime, a, b), error);
}

// R — uniform in [R.MIN, R.MAX]; a set form pins both bounds to one value.
template<typename Cfg>
static void opR(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.r_min = val;
        runtime.variables.r_max = val;
        return;
    }
    pushStack(stack, stackSize, tt2PushRandom(runtime, runtime.variables.r_min, runtime.variables.r_max), error);
}

// TOSS — random 0 or 1.
template<typename Cfg>
static void opToss(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, int16_t(tt2RngNext(runtime.rng, TT2RngSlot::Toss) & 1), error);
}

// ---------------------------------------------------------------------------
// Math ops
// ---------------------------------------------------------------------------

template<typename Cfg>
static void opAdd(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opSub(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first - second), error);
}

template<typename Cfg>
static void opMul(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    int32_t r = int32_t(first) * int32_t(second);
    if (r > 32767) r = 32767;
    if (r < -32768) r = -32768;
    pushStack(stack, stackSize, int16_t(r), error);
}

template<typename Cfg>
static void opDiv(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(second != 0 ? first / second : 0), error);
}

template<typename Cfg>
static void opMod(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(second != 0 ? first % second : 0), error);
}

template<typename Cfg>
static void opMin(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(second > first ? first : second), error);
}

template<typename Cfg>
static void opMax(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first > second ? first : second), error);
}

template<typename Cfg>
static void opEq(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first == second), error);
}

template<typename Cfg>
static void opNe(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first != second), error);
}

template<typename Cfg>
static void opLt(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first < second), error);
}

template<typename Cfg>
static void opGt(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first > second), error);
}

template<typename Cfg>
static void opLte(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first <= second), error);
}

template<typename Cfg>
static void opGte(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t first = 0, second = 0;
    if (!popBinary(stack, stackSize, first, second, error)) return;
    pushStack(stack, stackSize, int16_t(first >= second), error);
}

template<typename Cfg>
static void opAbs(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, int16_t(a < 0 ? -a : a), error);
}

template<typename Cfg>
static void opSgn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

// Verbatim port of teletype pattern_mode transpose_n_value: quantize value to
// the ET grid, shift by `interval` semitones (quantize-to-lower-note on
// off-grid up-transpose), return that note's raw value.
int16_t tt2TransposeSemitones(int16_t value, int interval) {
    const int last = 127;
    if (interval > last) interval = last;
    else if (interval < -last) interval = -last;
    if (value > tt2EtTable[last]) {
        int idx = last;
        if (interval < 0) idx++;
        return tt2EtTable[(idx + interval) % (last + 1)];
    }
    int16_t newVal = 0;
    for (int i = 0; i <= last; i++) {
        if (tt2EtTable[i] >= value) {
            int j = i + interval;
            if (tt2EtTable[i] > value && interval > 0) j--;
            if (j > last) j = j % last + 1;
            else if (j < 0) j = j + last + 1;
            newVal = tt2EtTable[j];
            break;
        }
    }
    return newVal;
}

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

template<typename Cfg>
static void opN(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, noteNumberToVolts(a), error);
}

template<typename Cfg>
static void opV(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (a > 10) a = 10; else if (a < -10) a = -10;
    if (a < 0) pushStack(stack, stackSize, int16_t(-tt2VTable[-a]), error);
    else pushStack(stack, stackSize, tt2VTable[a], error);
}

// table_vv[100]: hundredths of a volt -> raw fraction. Verbatim teletype/src/table.c.
static const int16_t tt2VvTable[100] = {
    0,    16,   33,   49,   66,   82,   98,   115,  131,  147,  164,  180,
    197,  213,  229,  246,  262,  279,  295,  311,  328,  344,  360,  377,
    393,  410,  426,  442,  459,  475,  492,  508,  524,  541,  557,  573,
    590,  606,  623,  639,  655,  672,  688,  705,  721,  737,  754,  770,
    786,  803,  819,  836,  852,  868,  885,  901,  918,  934,  950,  967,
    983,  999,  1016, 1032, 1049, 1065, 1081, 1098, 1114, 1130, 1147, 1163,
    1180, 1196, 1212, 1229, 1245, 1262, 1278, 1294, 1311, 1327, 1343, 1360,
    1376, 1393, 1409, 1425, 1442, 1458, 1475, 1491, 1507, 1524, 1540, 1556,
    1573, 1589, 1606, 1622
};

// table_hzv[76]: note (shifted +36) -> Hz value. Verbatim teletype/src/table.c.
static const int16_t tt2HzvTable[76] = {
    205,  217,  230,   244,   258,   273,   290,   307,   325,   344,  365,
    387,  410,  434,   460,   487,   516,   547,   579,   614,   650,  689,
    730,  773,  819,   868,   920,   974,   1032,  1094,  1159,  1227, 1300,
    1378, 1460, 1546,  1638,  1736,  1839,  1948,  2064,  2187,  2317, 2455,
    2601, 2755, 2919,  3093,  3277,  3472,  3678,  3897,  4129,  4374, 4634,
    4910, 5202, 5511,  5839,  6186,  6554,  6943,  7356,  7794,  8257, 8748,
    9268, 9819, 10403, 11022, 11677, 12372, 13107, 13887, 14712, 15587
};

// Linear rescale of i from [a,b] to [x,y] with rounding (upstream op_SCALE).
static int16_t tt2ScaleVal(int32_t i, int32_t a, int32_t b, int32_t x, int32_t y) {
    if ((b - a) == 0) return 0;
    int32_t result = (i - a) * (y - x) * 2 / (b - a);
    result = result / 2 + (result & 1);
    return int16_t(result + x);
}

// Binary search the ET table for the nearest note to a raw volts value.
static int16_t tt2VoltsToNote(int16_t vIn) {
    int16_t target = (vIn < 0) ? -vIn : vIn;
    const int len = 128;
    if (target <= tt2EtTable[0]) return 0;
    if (target >= tt2EtTable[len - 1]) return int16_t(len - 1);
    int i = 0, j = len, mid = 0;
    while (i < j) {
        mid = (i + j) / 2;
        if (tt2EtTable[mid] == target) return int16_t(vIn < 0 ? -mid : mid);
        if (target < tt2EtTable[mid]) {
            if (mid > 0 && target > tt2EtTable[mid - 1]) {
                if ((target - tt2EtTable[mid - 1]) >= (tt2EtTable[mid] - target))
                    return int16_t(vIn < 0 ? -mid : mid);
                return int16_t(vIn < 0 ? -(mid - 1) : mid - 1);
            }
            j = mid;
        } else {
            if (mid < len - 1 && target < tt2EtTable[mid + 1]) {
                if ((target - tt2EtTable[mid]) >= (tt2EtTable[mid + 1] - target))
                    return int16_t(vIn < 0 ? -(mid + 1) : mid + 1);
                return int16_t(vIn < 0 ? -mid : mid);
            }
            i = mid + 1;
        }
    }
    return int16_t(vIn < 0 ? -mid : mid);
}

// SCALE a b x y i — rescale i from [a,b] to [x,y].
template<typename Cfg>
static void opScale(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, x = 0, y = 0, i = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, x, error)) return;
    if (!popStack(stack, stackSize, y, error)) return;
    if (!popStack(stack, stackSize, i, error)) return;
    pushStack(stack, stackSize, tt2ScaleVal(i, a, b, x, y), error);
}

// SCALE0 b y i — rescale i from [0,b] to [0,y].
template<typename Cfg>
static void opScale0(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t b = 0, y = 0, i = 0;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, y, error)) return;
    if (!popStack(stack, stackSize, i, error)) return;
    pushStack(stack, stackSize, tt2ScaleVal(i, 0, b, 0, y), error);
}

// QT a b — round b to the nearest multiple of a (ties round up).
template<typename Cfg>
static void opQt(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    if (a == 0) { pushStack(stack, stackSize, 0, error); return; }
    int c = b / a;
    int d = c * a;
    int e = (c + 1) * a;
    int dd = b - d; if (dd < 0) dd = -dd;
    int ee = b - e; if (ee < 0) ee = -ee;
    pushStack(stack, stackSize, int16_t(dd < ee ? d : e), error);
}

// VN v — nearest ET note number for a raw volts value.
template<typename Cfg>
static void opVn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0;
    if (!popStack(stack, stackSize, v, error)) return;
    pushStack(stack, stackSize, tt2VoltsToNote(v), error);
}

// VV x — hundredths of a volt to raw (0..1000 -> 0..10V).
template<typename Cfg>
static void opVv(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    int16_t neg = 1;
    if (a < 0) { neg = -1; a = -a; }
    if (a > 1000) a = 1000;
    pushStack(stack, stackSize, int16_t(neg * (tt2VTable[a / 100] + tt2VvTable[a % 100])), error);
}

// HZ v — raw volts to a Hz-domain value (interpolated over the note tables).
template<typename Cfg>
static void opHz(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t vIn = 0;
    if (!popStack(stack, stackSize, vIn, error)) return;
    int16_t note = 0, hz = 0;
    float interpolate = 0.f;
    int16_t deltaTotal, delta;
    const int sizeHzv = 76;
    if (vIn < 0) {
        vIn = -vIn;
        for (int16_t i = 127; i >= 0; i--) {
            if (vIn <= tt2EtTable[i]) note = i;
        }
        deltaTotal = tt2EtTable[note] - tt2EtTable[note - 1];
        delta = tt2EtTable[note] - vIn;
        interpolate = deltaTotal ? delta / float(deltaTotal) : 0.f;
        note = -note;
    } else {
        for (int16_t i = 0; i <= 127; i++) {
            if (vIn >= tt2EtTable[i]) note = i;
        }
        if (note < 128) {
            deltaTotal = tt2EtTable[note + 1] - tt2EtTable[note];
            delta = vIn - tt2EtTable[note];
            interpolate = deltaTotal ? delta / float(deltaTotal) : 0.f;
        }
    }
    note = note + 36;
    if (note < 0) hz = tt2HzvTable[0];
    else if (note >= sizeHzv) hz = tt2HzvTable[sizeHzv - 1];
    else if (note < sizeHzv - 1)
        hz = int16_t(tt2HzvTable[note] + (tt2HzvTable[note + 1] - tt2HzvTable[note]) * interpolate);
    else hz = tt2HzvTable[note];
    pushStack(stack, stackSize, hz, error);
}

// table_exp[256]: exponential curve. Verbatim teletype/src/table.c.
static const int16_t tt2ExpTable[256] = {
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,2,2,2,3,3,4,4,
    5,5,6,6,7,8,9,10,11,12,13,14,16,17,19,20,22,24,26,28,30,32,
    35,38,40,43,46,49,53,56,60,64,68,72,77,82,86,92,97,103,108,114,121,127,
    134,141,149,156,164,172,181,190,199,209,219,229,239,250,262,273,285,298,311,324,338,352,
    366,381,397,413,429,446,464,482,500,519,538,559,579,600,622,644,667,691,715,740,765,791,
    818,845,873,902,931,961,992,1024,1056,1090,1123,1158,1194,1230,1267,1305,1344,1383,1424,1465,1508,1551,
    1595,1640,1686,1733,1781,1830,1880,1931,1983,2036,2090,2146,2202,2259,2318,2377,2438,2500,2563,2627,2693,2760,
    2827,2897,2967,3039,3112,3186,3262,3339,3417,3497,3578,3660,3744,3829,3916,4005,4094,4185,4278,4373,4468,4566,
    4665,4765,4868,4971,5077,5184,5293,5403,5516,5630,5745,5863,5982,6104,6227,6351,6478,6607,6737,6870,7004,7140,
    7279,7419,7561,7706,7852,8000,8151,8304,8459,8616,8775,8936,9100,9266,9434,9604,9777,9952,10129,10309,10491,10675,
    10862,11051,11243,11437,11634,11833,12035,12240,12447,12656,12869,13083,13301,13521,13744,13970,14199,14430,14664,14901,15141,15384,
    15629,15878,16129
};

// EXP x — exponential curve lookup (x>>6 indexes the table; signed-mirrored).
template<typename Cfg>
static void opExp(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (a > 16383) a = 16383; else if (a < -16383) a = -16383;
    a = int16_t(a >> 6);
    if (a < 0) pushStack(stack, stackSize, int16_t(-tt2ExpTable[-a]), error);
    else pushStack(stack, stackSize, tt2ExpTable[a], error);
}

// JI n d — just-intonation interval (prime-factor n/d into cents-ish value).
// Verbatim upstream algorithm.
template<typename Cfg>
static void opJi(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    static const uint8_t prime[6] = { 2, 3, 5, 7, 11, 13 };
    static const int16_t jiConst[6] = { 6554, 10388, 15218, 18399, 22673, 24253 };
    int16_t n = 0, d = 0;
    if (!popStack(stack, stackSize, n, error)) return;   // leftmost
    if (!popStack(stack, stackSize, d, error)) return;   // rightmost
    n = int16_t(n < 0 ? -n : n);
    d = int16_t(d < 0 ? -d : d);
    if (n == 0 || d == 0) { pushStack(stack, stackSize, 0, error); return; }
    int32_t result = 0;
    for (uint8_t p = 0; p <= 6; ++p) {
        if (n == 1) break;
        if (p == 6) { pushStack(stack, stackSize, 0, error); return; }
        int32_t q = n / prime[p];
        while (n == q * prime[p]) { result += jiConst[p]; n = int16_t(q); q = n / prime[p]; }
    }
    for (uint8_t p = 0; p <= 6; ++p) {
        if (d == 1) break;
        if (p == 6) { pushStack(stack, stackSize, 0, error); return; }
        int32_t q = d / prime[p];
        while (d == q * prime[p]) { result -= jiConst[p]; d = int16_t(q); q = d / prime[p]; }
    }
    result = (result + 2) >> 2;
    pushStack(stack, stackSize, int16_t(result), error);
}

// ---------------------------------------------------------------------------
// Logic / range ops (boolean results are 1/0)
// ---------------------------------------------------------------------------

template<typename Cfg>
static void opAnd(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    pushStack(stack, stackSize, int16_t(a && b), error);
}

template<typename Cfg>
static void opOr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    pushStack(stack, stackSize, int16_t(a || b), error);
}

template<typename Cfg>
static void opAnd3(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    pushStack(stack, stackSize, int16_t(a && b && c), error);
}

template<typename Cfg>
static void opOr3(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    pushStack(stack, stackSize, int16_t(a || b || c), error);
}

template<typename Cfg>
static void opAnd4(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0, d = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    if (!popStack(stack, stackSize, d, error)) return;
    pushStack(stack, stackSize, int16_t(a && b && c && d), error);
}

template<typename Cfg>
static void opOr4(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0, d = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    if (!popStack(stack, stackSize, d, error)) return;
    pushStack(stack, stackSize, int16_t(a || b || c || d), error);
}

template<typename Cfg>
static void opNz(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, int16_t(a != 0), error);
}

template<typename Cfg>
static void opEz(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, int16_t(a == 0), error);
}

// LIM value lo hi -> clamp value to [lo, hi]
template<typename Cfg>
static void opLim(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t i = 0, a = 0, b = 0;
    if (!popStack(stack, stackSize, i, error)) return;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    pushStack(stack, stackSize, int16_t(i < a ? a : (i > b ? b : i)), error);
}

// WRAP value lo hi -> wrap value into the [lo, hi] cycle
template<typename Cfg>
static void opWrap(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opAvg(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opInr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t lo = 0, x = 0, hi = 0;
    if (!popRange(stack, stackSize, lo, x, hi, error)) return;
    pushStack(stack, stackSize, int16_t((lo < x) && (x < hi)), error);
}

template<typename Cfg>
static void opOutr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t lo = 0, x = 0, hi = 0;
    if (!popRange(stack, stackSize, lo, x, hi, error)) return;
    pushStack(stack, stackSize, int16_t((lo > x) || (x > hi)), error);
}

template<typename Cfg>
static void opInri(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t lo = 0, x = 0, hi = 0;
    if (!popRange(stack, stackSize, lo, x, hi, error)) return;
    pushStack(stack, stackSize, int16_t((lo <= x) && (x <= hi)), error);
}

template<typename Cfg>
static void opOutri(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opCv(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                 const TeletypeProgramT<Cfg> *, int16_t *stack,
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
        int16_t target = normaliseCvValue(int16_t(val + runtime.variables.cv_off[idx]));
        tt2SeedCvSlew(output.cv[idx], target, runtime.variables.cv_slew[idx]);
        output.cvDirty |= uint8_t(1 << idx);
    } else {
        pushStack(stack, stackSize, runtime.variables.cv[idx], error);
    }
}

template<typename Cfg>
static void opTr(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                 const TeletypeProgramT<Cfg> *, int16_t *stack,
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
        output.tr[idx].pulseRemainingMs = 0;  // a direct set cancels any pulse
        output.trDirty |= uint8_t(1 << idx);
    } else {
        pushStack(stack, stackSize, runtime.variables.tr[idx], error);
    }
}

// CV.SLEW n [ms] — per-channel slew time in ms (min 1, upstream).
template<typename Cfg>
static void opCvSlew(TT2RuntimeT<Cfg> &runtime, TT2OutputState &,
                     const TeletypeProgramT<Cfg> *, int16_t *stack,
                     uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_CV_COUNT)) return;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        if (val < 1) val = 1;
        runtime.variables.cv_slew[idx] = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.cv_slew[idx], error);
    }
}

// CV.OFF n [v] — per-channel output offset. A set re-seeds the live ramp so
// the new offset takes effect immediately (upstream re-calls tele_cv).
template<typename Cfg>
static void opCvOff(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                    const TeletypeProgramT<Cfg> *, int16_t *stack,
                    uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_CV_COUNT)) return;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.cv_off[idx] = val;
        int16_t target = normaliseCvValue(int16_t(runtime.variables.cv[idx] + val));
        tt2SeedCvSlew(output.cv[idx], target, runtime.variables.cv_slew[idx]);
        output.cvDirty |= uint8_t(1 << idx);
    } else {
        pushStack(stack, stackSize, runtime.variables.cv_off[idx], error);
    }
}

// TR.POL n [p] — per-channel rest polarity (0/1).
template<typename Cfg>
static void opTrPol(TT2RuntimeT<Cfg> &runtime, TT2OutputState &,
                    const TeletypeProgramT<Cfg> *, int16_t *stack,
                    uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_TR_COUNT)) return;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        runtime.variables.tr_pol[idx] = (val > 0) ? 1 : 0;
    } else {
        pushStack(stack, stackSize, runtime.variables.tr_pol[idx], error);
    }
}

// TR.TIME n [ms] — per-channel pulse length in ms (>= 0).
template<typename Cfg>
static void opTrTime(TT2RuntimeT<Cfg> &runtime, TT2OutputState &,
                     const TeletypeProgramT<Cfg> *, int16_t *stack,
                     uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_TR_COUNT)) return;
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        if (val < 0) val = 0;
        runtime.variables.tr_time[idx] = val;
    } else {
        pushStack(stack, stackSize, runtime.variables.tr_time[idx], error);
    }
}

// TR.PULSE / TR.P n — fire a one-shot pulse: set the active level (tr_pol),
// arm the timer at tr_time. Action op (no value pushed).
template<typename Cfg>
static void opTrPulse(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                      const TeletypeProgramT<Cfg> *, int16_t *stack,
                      uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_TR_COUNT)) return;
    auto &v = runtime.variables;
    // Divisor: fire only every Nth call (TR.D).
    int div = v.tr_div[idx]; if (div < 1) div = 1;
    if (div > 1) {
        uint8_t c = uint8_t(v.tr_div_count[idx] + 1);
        if (c < div) { v.tr_div_count[idx] = c; return; }
        v.tr_div_count[idx] = 0;
    }
    // Width: if TR.W>0, pulse length = (metro period * div) * width% ; else tr_time.
    int timeMs = v.tr_time[idx];
    if (v.tr_width[idx] > 0) {
        int interval = int(v.m) * div;
        timeMs = interval * v.tr_width[idx] / 100;
        if (timeMs < 1) timeMs = 1;
        if (timeMs > 32767) timeMs = 32767;
    }
    if (timeMs <= 0) return;
    uint8_t pol = v.tr_pol[idx] ? 1 : 0;
    v.tr[idx] = pol;
    tt2ArmTrPulse(output.tr[idx], pol, int16_t(timeMs));
    output.trDirty |= uint8_t(1 << idx);
}

// TR.W n pct — pulse width as % of the metro interval (0 = use TR.TIME).
template<typename Cfg>
static void opTrW(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_TR_COUNT)) return;
    if (isSet && stackSize >= 1) {
        int16_t pct = 0; if (!popStack(stack, stackSize, pct, error)) return;
        if (pct < 0) pct = 0;
        runtime.variables.tr_width[idx] = pct;
    } else {
        pushStack(stack, stackSize, runtime.variables.tr_width[idx], error);
    }
}

// TR.D n div — pulse divisor (fire every Nth TR.P).
template<typename Cfg>
static void opTrD(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_TR_COUNT)) return;
    if (isSet && stackSize >= 1) {
        int16_t d = 0; if (!popStack(stack, stackSize, d, error)) return;
        if (d < 1) d = 1;
        runtime.variables.tr_div[idx] = d;
        runtime.variables.tr_div_count[idx] = 0;
    } else {
        pushStack(stack, stackSize, runtime.variables.tr_div[idx], error);
    }
}

// M.RESET / M.RESET.A — request the engine zero the metro phase.
template<typename Cfg>
static void opMReset(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *, uint8_t &, bool, TT2EvalError &) {
    runtime.metroResetReq = 1;
}

// SYNC count — set every-counters so the next EVERY boundary is `count` away.
template<typename Cfg>
static void opSync(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t count = 0;
    if (!popStack(stack, stackSize, count, error)) return;
    for (int s = 0; s < Cfg::ScriptCount; ++s) {
        for (int l = 0; l < TT2_COMMANDS_PER_SCRIPT; ++l) {
            TT2EveryCount &e = runtime.every.every[s][l];
            if (e.mod > 0) { e.count = int16_t(count % e.mod); if (e.count < 0) e.count += e.mod; }
        }
    }
}

// TR.TOG n — flip the current level. Action op (no value pushed).
template<typename Cfg>
static void opTrTog(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                    const TeletypeProgramT<Cfg> *, int16_t *stack,
                    uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_TR_COUNT)) return;
    uint8_t level = runtime.variables.tr[idx] ? 0 : 1;
    runtime.variables.tr[idx] = level;
    output.tr[idx].level = level;
    output.tr[idx].pulseRemainingMs = 0;
    output.trDirty |= uint8_t(1 << idx);
}

// ---------------------------------------------------------------------------
// Turtle (@ ops) — fixed-point walker over pattern memory. Verbatim port of
// teletype/src/turtle.c: x selects the pattern (0-3), y the index (0-63);
// Q9 fixed-point positions. (Grid-independent: pattern memory is the field.)
// ---------------------------------------------------------------------------

static constexpr int TT2Q_BITS = 9;
static constexpr int32_t TT2Q_1 = 1 << TT2Q_BITS;
static constexpr int32_t TT2Q_05 = 1 << (TT2Q_BITS - 1);
static inline int32_t tt2ToQ(int32_t x) { return x << TT2Q_BITS; }
static inline int32_t tt2ToI(int32_t x) { return (x >> TT2Q_BITS) & 0xFFFF; }

struct TT2QFence { int32_t x1, y1, x2, y2; };

static TT2QFence tt2NormalizeFence(const TT2TurtleFence &in, TT2TurtleMode mode) {
    TT2QFence o;
    if (mode == TT2TurtleMode::Wrap) {
        o.x1 = tt2ToQ(in.x1); o.x2 = tt2ToQ(in.x2 + 1);
        o.y1 = tt2ToQ(in.y1); o.y2 = tt2ToQ(in.y2 + 1);
    } else {
        o.x1 = tt2ToQ(in.x1) + (TT2Q_1 >> 1); o.x2 = tt2ToQ(in.x2 + 1) - (TT2Q_1 >> 1);
        o.y1 = tt2ToQ(in.y1) + (TT2Q_1 >> 1); o.y2 = tt2ToQ(in.y2 + 1) - (TT2Q_1 >> 1);
    }
    return o;
}

static void tt2TurtleResolve(const TT2TurtlePosition &src, TT2TurtlePosition &dst) {
    dst.x = tt2ToI(src.x);
    dst.y = tt2ToI(src.y);
}

static void tt2TurtleCheckStep(TT2Turtle &t) {
    TT2TurtlePosition here; tt2TurtleResolve(t.position, here);
    if (here.x != t.last.x || here.y != t.last.y) { t.last = here; t.stepped = 1; }
}

static void tt2TurtleSetHeading(TT2Turtle &t, int16_t h) {
    while (h < 0) h += 360;
    t.heading = uint16_t(h % 360);
}

static void tt2TurtleNormalizePos(TT2Turtle &t, TT2TurtleMode mode) {
    TT2QFence f = tt2NormalizeFence(t.fence, mode);
    int32_t fxl = f.x2 - f.x1, fyl = f.y2 - f.y1;
    int32_t &x = t.position.x, &y = t.position.y;
    if (mode == TT2TurtleMode::Wrap) {
        if (fxl > TT2Q_1 && x < f.x1) x = f.x2 + ((x - f.x1) % fxl);
        else if (fxl > TT2Q_1 && x > f.x2) x = f.x1 + ((x - f.x1) % fxl);
        if (fyl > TT2Q_1 && y < f.y1) y = f.y2 + ((y - f.y1) % fyl);
        else if (fyl > TT2Q_1 && y > f.y2) y = f.y1 + ((y - f.y1) % fyl);
    } else if (mode == TT2TurtleMode::Bounce) {
        TT2TurtlePosition last, here; tt2TurtleResolve(t.position, last);
        while (x > f.x2 || x < f.x1) {
            if (x > f.x2) { if (t.stepping) tt2TurtleSetHeading(t, int16_t(360 - t.heading)); x = f.x2 - (x - f.x2); }
            else if (x < f.x1) { if (t.stepping) tt2TurtleSetHeading(t, int16_t(360 - t.heading)); x = f.x1 + (f.x1 - x); }
            tt2TurtleResolve(t.position, here); if (here.x == last.x) break; last = here;
        }
        while (y > f.y2 || y < f.y1) {
            if (y >= f.y2) { if (t.stepping) tt2TurtleSetHeading(t, int16_t(180 - t.heading)); y = f.y2 - (y - f.y2); }
            else if (y < f.y1) { if (t.stepping) tt2TurtleSetHeading(t, int16_t(180 - t.heading)); y = f.y1 + (f.y1 - y); }
            tt2TurtleResolve(t.position, here); if (here.y == last.y) break; last = here;
        }
        if (x == f.x2) x -= 1;
        if (y == f.y2) y -= 1;
    }
    int32_t maxx = f.x2 - 1, maxy = f.y2 - 1;
    x = x < f.x1 ? f.x1 : (x > maxx ? maxx : x);
    y = y < f.y1 ? f.y1 : (y > maxy ? maxy : y);
    tt2TurtleCheckStep(t);
}

static uint8_t tt2TurtleGetX(TT2Turtle &t) { TT2TurtlePosition p; tt2TurtleResolve(t.position, p); return uint8_t(p.x); }
static uint8_t tt2TurtleGetY(TT2Turtle &t) { TT2TurtlePosition p; tt2TurtleResolve(t.position, p); return uint8_t(p.y); }

static void tt2TurtleSetX(TT2Turtle &t, int16_t x) { t.position.x = tt2ToQ(x) + TT2Q_05; tt2TurtleNormalizePos(t, TT2TurtleMode::Bump); }
static void tt2TurtleSetY(TT2Turtle &t, int16_t y) { t.position.y = tt2ToQ(y) + TT2Q_05; tt2TurtleNormalizePos(t, TT2TurtleMode::Bump); }

static void tt2TurtleMove(TT2Turtle &t, int16_t x, int16_t y) {
    t.position.y += tt2ToQ(y);
    t.position.x += tt2ToQ(x);
    tt2TurtleNormalizePos(t, t.mode);
}

// Third-order sine approximation (Q12 out, 2^15 units/circle). Verbatim upstream.
static inline int32_t tt2Sin(int32_t x) {
    static const int qN = 13, qP = 15, qR = 11, qS = 17;
    x = x << (30 - qN);
    if ((x ^ (x << 1)) < 0) x = (1 << 31) - x;
    x = x >> (30 - qN);
    return x * ((3 << qP) - (x * x >> qR)) >> qS;
}

static void tt2TurtleStep(TT2Turtle &t) {
    int32_t dx = 0, dy = 0;
    int32_t h1 = t.heading, h2 = t.heading;
    h1 = ((h1 % 360) << 15) / 360;
    h2 = (((h2 + 360 - 90) % 360) << 15) / 360;
    int32_t dxQ12 = (t.speed * tt2Sin(h1)) / 100;
    int32_t dyQ12 = (t.speed * tt2Sin(h2)) / 100;
    dx = dxQ12 < 0 ? (((dxQ12 >> (11 - TT2Q_BITS)) - 1) >> 1) : (((dxQ12 >> (11 - TT2Q_BITS)) + 1) >> 1);
    dy = dyQ12 < 0 ? (((dyQ12 >> (11 - TT2Q_BITS)) - 1) >> 1) : (((dyQ12 >> (11 - TT2Q_BITS)) + 1) >> 1);
    t.position.x += dx;
    t.position.y += dy;
    t.stepping = 1;
    tt2TurtleNormalizePos(t, t.mode);
    t.stepping = 0;
}

static void tt2TurtleCorrectFence(TT2Turtle &t) {
    auto clampU8 = [](int v, int lo, int hi) { return uint8_t(v < lo ? lo : (v > hi ? hi : v)); };
    t.fence.x1 = clampU8(t.fence.x1, 0, 3);
    t.fence.x2 = clampU8(t.fence.x2, 0, 3);
    t.fence.y1 = clampU8(t.fence.y1, 0, 63);
    t.fence.y2 = clampU8(t.fence.y2, 0, 63);
    if (t.fence.x1 > t.fence.x2) { uint8_t s = t.fence.x2; t.fence.x2 = t.fence.x1; t.fence.x1 = s; }
    if (t.fence.y1 > t.fence.y2) { uint8_t s = t.fence.y2; t.fence.y2 = t.fence.y1; t.fence.y1 = s; }
    tt2TurtleNormalizePos(t, TT2TurtleMode::Bump);
}

static void tt2TurtleSetFence(TT2Turtle &t, int16_t x1, int16_t y1, int16_t x2, int16_t y2) {
    auto clampU8 = [](int v, int lo, int hi) { return uint8_t(v < lo ? lo : (v > hi ? hi : v)); };
    t.fence.x1 = clampU8(x1, 0, 3);
    t.fence.x2 = clampU8(x2, 0, 3);
    t.fence.y1 = clampU8(y1, 0, 63);
    t.fence.y2 = clampU8(y2, 0, 63);
    tt2TurtleCorrectFence(t);
}

static void tt2TurtleSetMode(TT2Turtle &t, TT2TurtleMode m) { t.mode = m; tt2TurtleNormalizePos(t, m); }

template<typename Cfg>
static int16_t tt2TurtleGetVal(const TeletypeProgramT<Cfg> *program, TT2Turtle &t) {
    if (!program) return 0;
    TT2TurtlePosition p; tt2TurtleResolve(t.position, p);
    if (p.x > 3 || p.x < 0 || p.y > 63 || p.y < 0) return 0;
    return program->patterns[p.x].val[p.y];
}

template<typename Cfg>
static void tt2TurtleSetVal(const TeletypeProgramT<Cfg> *program, TT2Turtle &t, int16_t v) {
    if (!program) return;
    TT2TurtlePosition p; tt2TurtleResolve(t.position, p);
    if (p.x > 3 || p.x < 0 || p.y > 63 || p.y < 0) return;
    const_cast<TeletypeProgramT<Cfg> *>(program)->patterns[p.x].val[p.y] = v;
}

// --- @ op handlers ---

template<typename Cfg>
static void opTurtle(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t v = 0;
        if (!popStack(stack, stackSize, v, error)) return;
        tt2TurtleSetVal(program, runtime.turtle, v);
    } else {
        pushStack(stack, stackSize, tt2TurtleGetVal(program, runtime.turtle), error);
    }
}

template<typename Cfg>
static void opTurtleX(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                      int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        tt2TurtleSetX(runtime.turtle, v);
    } else pushStack(stack, stackSize, tt2TurtleGetX(runtime.turtle), error);
}

template<typename Cfg>
static void opTurtleY(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                      int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        tt2TurtleSetY(runtime.turtle, v);
    } else pushStack(stack, stackSize, tt2TurtleGetY(runtime.turtle), error);
}

template<typename Cfg>
static void opTurtleMove(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                         int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;  // tokens: native pops left->right; upstream x=last, y=first
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    tt2TurtleMove(runtime.turtle, b, a);
}

template<typename Cfg>
static void opTurtleF(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                      int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0, c = 0, d = 0;  // x1=d,y1=c,x2=b,y2=a (reversed pop order)
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, c, error)) return;
    if (!popStack(stack, stackSize, d, error)) return;
    tt2TurtleSetFence(runtime.turtle, d, c, b, a);
}

#define TT2_TURTLE_FENCE_OP(fn, member)                                        \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *, \
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) { \
        if (isSet && stackSize >= 1) {                                         \
            int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return; \
            runtime.turtle.fence.member = uint8_t(v > 0 ? v : 0);             \
            tt2TurtleCorrectFence(runtime.turtle);                            \
        } else pushStack(stack, stackSize, runtime.turtle.fence.member, error); \
    }
TT2_TURTLE_FENCE_OP(opTurtleFx1, x1)
TT2_TURTLE_FENCE_OP(opTurtleFy1, y1)
TT2_TURTLE_FENCE_OP(opTurtleFx2, x2)
TT2_TURTLE_FENCE_OP(opTurtleFy2, y2)

template<typename Cfg>
static void opTurtleSpeed(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                          int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        runtime.turtle.speed = v;
    } else pushStack(stack, stackSize, runtime.turtle.speed, error);
}

template<typename Cfg>
static void opTurtleDir(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                        int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        tt2TurtleSetHeading(runtime.turtle, v);
    } else pushStack(stack, stackSize, int16_t(runtime.turtle.heading), error);
}

template<typename Cfg>
static void opTurtleStep(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                         int16_t *, uint8_t &, bool, TT2EvalError &) {
    tt2TurtleStep(runtime.turtle);
}

#define TT2_TURTLE_MODE_OP(fn, MODE)                                           \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *, \
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) { \
        if (isSet && stackSize >= 1) {                                         \
            int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return; \
            if (v) tt2TurtleSetMode(runtime.turtle, TT2TurtleMode::MODE);     \
        } else pushStack(stack, stackSize, int16_t(runtime.turtle.mode == TT2TurtleMode::MODE ? 1 : 0), error); \
    }
TT2_TURTLE_MODE_OP(opTurtleBump, Bump)
TT2_TURTLE_MODE_OP(opTurtleWrap, Wrap)
TT2_TURTLE_MODE_OP(opTurtleBounce, Bounce)

template<typename Cfg>
static void opTurtleScript(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                           int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t sn = 0; if (!popStack(stack, stackSize, sn, error)) return;
        sn -= 1;
        runtime.turtle.scriptNumber = (sn < 0 || sn >= Cfg::ScriptCount) ? uint8_t(Cfg::ScriptCount) : uint8_t(sn);
        runtime.turtle.stepped = 0;
    } else {
        uint8_t s = runtime.turtle.scriptNumber;
        pushStack(stack, stackSize, int16_t(s >= Cfg::ScriptCount ? 0 : s + 1), error);
    }
}

template<typename Cfg>
static void opTurtleShow(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                         int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        runtime.turtle.shown = (v != 0) ? 1 : 0;
    } else pushStack(stack, stackSize, int16_t(runtime.turtle.shown ? 1 : 0), error);
}

// ---------------------------------------------------------------------------
// Bitwise / shift ops
// ---------------------------------------------------------------------------

template<typename Cfg>
static void opBitOr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    pushStack(stack, stackSize, int16_t(a | b), error);
}

template<typename Cfg>
static void opBitAnd(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    pushStack(stack, stackSize, int16_t(a & b), error);
}

template<typename Cfg>
static void opBitXor(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0, b = 0;
    if (!popBinary(stack, stackSize, a, b, error)) return;
    pushStack(stack, stackSize, int16_t(a ^ b), error);
}

template<typename Cfg>
static void opBitNot(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, int16_t(~a), error);
}

// BSET/BGET/BCLR/BTOG value = leftmost arg, bit index = rightmost (BSET x i).
template<typename Cfg>
static void opBset(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0, b = 0;
    if (!popBinary(stack, stackSize, v, b, error)) return;
    pushStack(stack, stackSize, int16_t(v | (1 << b)), error);
}

template<typename Cfg>
static void opBget(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0, b = 0;
    if (!popBinary(stack, stackSize, v, b, error)) return;
    pushStack(stack, stackSize, int16_t((v >> b) & 1), error);
}

template<typename Cfg>
static void opBclr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0, b = 0;
    if (!popBinary(stack, stackSize, v, b, error)) return;
    pushStack(stack, stackSize, int16_t(v & ~(1 << b)), error);
}

template<typename Cfg>
static void opBtog(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0, b = 0;
    if (!popBinary(stack, stackSize, v, b, error)) return;
    int16_t r = ((v >> b) & 1) ? int16_t(v & ~(1 << b)) : int16_t(v | (1 << b));
    pushStack(stack, stackSize, r, error);
}

static int16_t tt2BitReverse(int16_t v, int bits) {
    int16_t reversed = 0;
    for (int i = 0; i < bits; ++i) {
        if (v & (1 << i)) reversed |= int16_t(1 << ((bits - 1) - i));
    }
    return reversed;
}

template<typename Cfg>
static void opBrev(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    pushStack(stack, stackSize, tt2BitReverse(a, 16), error);
}

// RSH/LSH value = leftmost arg, shift = rightmost (RSH x n); negative flips.
template<typename Cfg>
static void opRsh(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t x = 0, n = 0;
    if (!popBinary(stack, stackSize, x, n, error)) return;
    pushStack(stack, stackSize, int16_t(n > 0 ? (x >> n) : (x << -n)), error);
}

template<typename Cfg>
static void opLsh(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t x = 0, n = 0;
    if (!popBinary(stack, stackSize, x, n, error)) return;
    pushStack(stack, stackSize, int16_t(n > 0 ? (x << n) : (x >> -n)), error);
}

static uint16_t tt2Rrot(uint16_t x, uint8_t n) { return uint16_t((x >> n) | (x << (16 - n))); }
static uint16_t tt2Lrot(uint16_t x, uint8_t n) { return uint16_t((x << n) | (x >> (16 - n))); }

template<typename Cfg>
static void opRrot(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t x = 0, n = 0;
    if (!popBinary(stack, stackSize, x, n, error)) return;
    n %= 16;
    uint16_t u = uint16_t(x);
    u = n > 0 ? tt2Rrot(u, uint8_t(n)) : tt2Lrot(u, uint8_t(-n));
    pushStack(stack, stackSize, int16_t(u), error);
}

template<typename Cfg>
static void opLrot(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t x = 0, n = 0;
    if (!popBinary(stack, stackSize, x, n, error)) return;
    n %= 16;
    uint16_t u = uint16_t(x);
    u = n > 0 ? tt2Lrot(u, uint8_t(n)) : tt2Rrot(u, uint8_t(-n));
    pushStack(stack, stackSize, int16_t(u), error);
}

// ---------------------------------------------------------------------------
// Scale-bitmask ops (N.S/N.C/N.CS/N.B/N.BX, QT.S/QT.CS/QT.B/QT.BX).
// Helpers reimplemented from upstream maths.c using tt2EtTable (= table_n) +
// the linked const scale tables.
// ---------------------------------------------------------------------------

static int16_t tt2EtAt(int idx) {  // table_n lookup, clamped to the 128-entry ET table
    if (idx < 0) idx = 0; else if (idx > 127) idx = 127;
    return tt2EtTable[idx];
}

static int16_t tt2ScaleNsToBitmask(int16_t s) {
    s %= 9; if (s < 0) s += 9;
    int16_t bits = 0;
    for (int i = 0; i < 7; ++i) bits |= int16_t(1 << table_n_s[s][i]);
    return bits;
}

static int16_t tt2ChordNsToBitmask(int16_t s, int16_t degree, int16_t voices) {
    s %= 9; if (s < 0) s += 9;
    degree %= 7; if (degree < 0) degree += 7;
    voices = tt2Normalise(1, 7, 0, voices);
    int16_t bits = 0;
    for (int i = 1; i <= voices; ++i) { bits |= int16_t(1 << table_n_s[s][degree]); degree = (degree + 2) % 7; }
    return bits;
}

static int16_t tt2DegreeInBitmaskScale(int16_t scaleBits, int16_t transpose, int16_t degree) {
    int note = 0;
    if (degree > 0) {
        for (int i = 0; i < 128; ++i) { if ((scaleBits >> (i % 12)) & 1) { degree--; if (!degree) break; } note++; }
    } else {
        degree--;
        for (int i = 0; i < 128; ++i) { if ((scaleBits >> (11 - (i % 12))) & 1) { degree++; if (!degree) break; } note--; }
        note--;
    }
    note += transpose;
    return note > 0 ? tt2EtAt(note) : int16_t(-tt2EtAt(-note));
}

static int16_t tt2QuantizeToBitmaskScale(int16_t scaleBits, int16_t transpose, int16_t vIn) {
    if (scaleBits == 0) return vIn;
    vIn = tt2Normalise(int16_t(-tt2EtTable[127]), tt2EtTable[127], 0, vIn);
    int16_t signOffset = (vIn < 0) ? 18022 : 0;
    vIn = int16_t(vIn + signOffset);
    int16_t oct = int16_t(vIn / tt2EtTable[12]);
    if (vIn <= 18021 && vIn >= 18018) oct = 10;
    int16_t semis = int16_t(vIn % tt2EtTable[12]);
    transpose = int16_t(transpose % tt2EtTable[12]);
    int16_t distNearest = INT16_MAX, noteNearest = INT16_MAX;
    for (int16_t i = 0; i < 12; ++i) {
        if (scaleBits & (1 << i)) {
            for (int16_t j = -2; j <= 2; ++j) {
                int16_t tryNote = int16_t(tt2EtTable[i] + transpose + (j * tt2EtTable[12]));
                int16_t tryDist = int16_t(tryNote - semis); if (tryDist < 0) tryDist = int16_t(-tryDist);
                if (tryDist < distNearest) { distNearest = tryDist; noteNearest = tryNote; }
            }
        }
    }
    return int16_t((noteNearest + tt2EtAt(oct * 12)) - signOffset);
}

// N.S root scale degree
template<typename Cfg>
static void opNS(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t root = 0, scale = 0, degree = 0;
    if (!popStack(stack, stackSize, root, error)) return;
    if (!popStack(stack, stackSize, scale, error)) return;
    if (!popStack(stack, stackSize, degree, error)) return;
    scale %= 9; if (scale < 0) scale += 9;
    degree = int16_t((degree - 1) % 7); if (degree < 0) degree += 7;
    int16_t transpose = table_n_s[scale][degree];
    if (root < 0) { if (root < -127) root = -127; pushStack(stack, stackSize, int16_t(-tt2EtAt(-root + transpose)), error); }
    else { if (root > 127) root = 127; pushStack(stack, stackSize, tt2EtAt(root + transpose), error); }
}

// N.C root chord component
template<typename Cfg>
static void opNC(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t root = 0, chord = 0, comp = 0;
    if (!popStack(stack, stackSize, root, error)) return;
    if (!popStack(stack, stackSize, chord, error)) return;
    if (!popStack(stack, stackSize, comp, error)) return;
    chord %= 13; if (chord < 0) chord += 13;
    comp %= 4; if (comp < 0) comp += 4;
    int16_t transpose = table_n_c[chord][comp];
    if (root < 0) { if (root < -127) root = -127; pushStack(stack, stackSize, int16_t(-tt2EtAt(-root + transpose)), error); }
    else { if (root > 127) root = 127; pushStack(stack, stackSize, tt2EtAt(root + transpose), error); }
}

// N.CS root scale scl_deg ch_deg
template<typename Cfg>
static void opNCS(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t root = 0, scale = 0, sclDeg = 0, chDeg = 0;
    if (!popStack(stack, stackSize, root, error)) return;
    if (!popStack(stack, stackSize, scale, error)) return;
    if (!popStack(stack, stackSize, sclDeg, error)) return;
    if (!popStack(stack, stackSize, chDeg, error)) return;
    scale %= 9; if (scale < 0) scale += 9;
    sclDeg = int16_t((sclDeg - 1) % 7); if (sclDeg < 0) sclDeg += 7;
    int16_t sclTrans = table_n_s[scale][sclDeg];
    chDeg %= 4; if (chDeg < 0) chDeg += 4;
    int16_t chTrans = table_n_c[table_n_cs[scale][sclDeg]][chDeg];
    if (root < 0) { if (root < -127) root = -127; pushStack(stack, stackSize, int16_t(-tt2EtAt(-root + sclTrans + chTrans)), error); }
    else { if (root > 127) root = 127; pushStack(stack, stackSize, tt2EtAt(root + sclTrans + chTrans), error); }
}

static int16_t tt2NbScaleBitsFromArg(int16_t scaleBits) {
    if (scaleBits < 1) {
        if (scaleBits > -20) return tt2BitReverse(int16_t(table_n_b[-scaleBits]), 12);
        return tt2BitReverse(int16_t(table_n_b[0]), 12);
    }
    return int16_t(scaleBits & 0xFFF);
}

// N.B degree (get) / N.B scale_bits root (set, slot 0)
template<typename Cfg>
static void opNB(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 2) {
        int16_t scaleBits = 0, root = 0;
        if (!popStack(stack, stackSize, scaleBits, error)) return;
        if (!popStack(stack, stackSize, root, error)) return;
        v.n_scale_root[0] = root;
        v.n_scale_bits[0] = tt2NbScaleBitsFromArg(scaleBits);
    } else {
        int16_t degree = 0;
        if (!popStack(stack, stackSize, degree, error)) return;
        pushStack(stack, stackSize, tt2DegreeInBitmaskScale(v.n_scale_bits[0], v.n_scale_root[0], degree), error);
    }
}

// N.BX scale_nb degree (get) / N.BX scale_nb scale_bits root (set)
template<typename Cfg>
static void opNBX(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    if (isSet && stackSize >= 3) {
        int16_t nb = 0, scaleBits = 0, root = 0;
        if (!popStack(stack, stackSize, nb, error)) return;
        if (!popStack(stack, stackSize, scaleBits, error)) return;
        if (!popStack(stack, stackSize, root, error)) return;
        nb %= 8; if (nb < 0) nb = 0; if (nb > TT2_NB_SCALES - 1) nb = TT2_NB_SCALES - 1;
        v.n_scale_root[nb] = root;
        v.n_scale_bits[nb] = tt2NbScaleBitsFromArg(scaleBits);
    } else {
        int16_t nb = 0, degree = 0;
        if (!popStack(stack, stackSize, nb, error)) return;
        if (!popStack(stack, stackSize, degree, error)) return;
        if (nb < 0) nb = 0; if (nb > TT2_NB_SCALES - 1) nb = TT2_NB_SCALES - 1;
        pushStack(stack, stackSize, tt2DegreeInBitmaskScale(v.n_scale_bits[nb], v.n_scale_root[nb], degree), error);
    }
}

// QT.S v_in transpose scale
template<typename Cfg>
static void opQtS(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t vIn = 0, transpose = 0, scale = 0;
    if (!popStack(stack, stackSize, vIn, error)) return;
    if (!popStack(stack, stackSize, transpose, error)) return;
    if (!popStack(stack, stackSize, scale, error)) return;
    scale %= 9; if (scale < 0) scale += 9;
    pushStack(stack, stackSize, tt2QuantizeToBitmaskScale(tt2ScaleNsToBitmask(scale), transpose, vIn), error);
}

// QT.CS v_in transpose scale degree voices
template<typename Cfg>
static void opQtCS(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t vIn = 0, transpose = 0, scale = 0, degree = 0, voices = 0;
    if (!popStack(stack, stackSize, vIn, error)) return;
    if (!popStack(stack, stackSize, transpose, error)) return;
    if (!popStack(stack, stackSize, scale, error)) return;
    if (!popStack(stack, stackSize, degree, error)) return;
    if (!popStack(stack, stackSize, voices, error)) return;
    scale %= 9; if (scale < 0) scale += 9;
    degree -= 1;
    voices = tt2Normalise(1, 7, 0, voices);
    pushStack(stack, stackSize, tt2QuantizeToBitmaskScale(tt2ChordNsToBitmask(scale, degree, voices), transpose, vIn), error);
}

// QT.B v_in (uses scale slot 0)
template<typename Cfg>
static void opQtB(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t vIn = 0;
    if (!popStack(stack, stackSize, vIn, error)) return;
    int16_t mask = runtime.variables.n_scale_bits[0];
    int16_t transpose = noteNumberToVolts(runtime.variables.n_scale_root[0]);
    pushStack(stack, stackSize, tt2QuantizeToBitmaskScale(mask, transpose, vIn), error);
}

// QT.BX v_in scale_nb
template<typename Cfg>
static void opQtBX(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t vIn = 0, nb = 0;
    if (!popStack(stack, stackSize, vIn, error)) return;
    if (!popStack(stack, stackSize, nb, error)) return;
    if (nb < 0) nb = 0; if (nb > TT2_NB_SCALES - 1) nb = TT2_NB_SCALES - 1;
    int16_t mask = runtime.variables.n_scale_bits[nb];
    int16_t transpose = noteNumberToVolts(runtime.variables.n_scale_root[nb]);
    pushStack(stack, stackSize, tt2QuantizeToBitmaskScale(mask, transpose, vIn), error);
}

// ---------------------------------------------------------------------------
// Euclid / drum / chaos / seeds (reuse linked pure C helpers)
// ---------------------------------------------------------------------------

// ER fill len step — Euclidean rhythm bit. (native pops leftmost first)
template<typename Cfg>
static void opEr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t fill = 0, len = 0, step = 0;
    if (!popStack(stack, stackSize, fill, error)) return;
    if (!popStack(stack, stackSize, len, error)) return;
    if (!popStack(stack, stackSize, step, error)) return;
    pushStack(stack, stackSize, int16_t(euclidean(fill, len, step)), error);
}

// NR prime mask factor step — numeric-rhythm bit (table_nr + factor/mask twiddle).
template<typename Cfg>
static void opNr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t prime = 0, mask = 0, factor = 0, step = 0;
    if (!popStack(stack, stackSize, prime, error)) return;
    if (!popStack(stack, stackSize, mask, error)) return;
    if (!popStack(stack, stackSize, factor, error)) return;
    if (!popStack(stack, stackSize, step, error)) return;
    prime %= 32; if (prime < 0) prime += 32;
    mask %= 4; if (mask < 0) mask += 4;
    factor %= 17; if (factor < 0) factor += 17;
    step %= 16; if (step < 0) step += 16;
    uint16_t rhythm = table_nr[prime];
    if (mask == 1) rhythm &= 0x0F0F;
    else if (mask == 2) rhythm &= 0xF003;
    else if (mask == 3) rhythm &= 0x1F0;
    uint32_t modified = uint32_t(rhythm) * uint32_t(factor);
    uint16_t fin = uint16_t((modified & 0xFFFF) | (modified >> 16));
    pushStack(stack, stackSize, int16_t((fin >> (15 - step)) & 1), error);
}

// DR.T bank p1 p2 len step — tresillo bit.
template<typename Cfg>
static void opDrT(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t bank = 0, p1 = 0, p2 = 0, len = 0, step = 0;
    if (!popStack(stack, stackSize, bank, error)) return;
    if (!popStack(stack, stackSize, p1, error)) return;
    if (!popStack(stack, stackSize, p2, error)) return;
    if (!popStack(stack, stackSize, len, error)) return;
    if (!popStack(stack, stackSize, step, error)) return;
    pushStack(stack, stackSize, int16_t(tresillo(bank, p1, p2, len, step)), error);
}

// DR.P bank pattern step — drum-bank bit.
template<typename Cfg>
static void opDrP(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t bank = 0, pattern = 0, step = 0;
    if (!popStack(stack, stackSize, bank, error)) return;
    if (!popStack(stack, stackSize, pattern, error)) return;
    if (!popStack(stack, stackSize, step, error)) return;
    pushStack(stack, stackSize, int16_t(drum(bank, pattern, step)), error);
}

// DR.V pattern step — drum velocity.
template<typename Cfg>
static void opDrV(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pattern = 0, step = 0;
    if (!popStack(stack, stackSize, pattern, error)) return;
    if (!popStack(stack, stackSize, step, error)) return;
    pushStack(stack, stackSize, int16_t(velocity(pattern, step)), error);
}

// CHAOS / CHAOS.R / CHAOS.ALG — process-global chaos generator (upstream).
template<typename Cfg>
static void opChaos(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) { int16_t v=0; if(!popStack(stack,stackSize,v,error))return; chaos_set_val(v); }
    else pushStack(stack, stackSize, chaos_get_val(), error);
}
template<typename Cfg>
static void opChaosR(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) { int16_t v=0; if(!popStack(stack,stackSize,v,error))return; chaos_set_r(v); }
    else pushStack(stack, stackSize, chaos_get_r(), error);
}
template<typename Cfg>
static void opChaosAlg(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                       int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) { int16_t v=0; if(!popStack(stack,stackSize,v,error))return; chaos_set_alg(v); }
    else pushStack(stack, stackSize, chaos_get_alg(), error);
}

// SEED — global reseed of all rng slots; *.SEED/*.SD reseed one slot.
template<typename Cfg>
static void opSeed(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        runtime.variables.seed = v;
        for (int i = 0; i < TT2_RNG_COUNT; ++i) runtime.rng.state[i] = uint32_t(uint16_t(v));
    } else {
        pushStack(stack, stackSize, runtime.variables.seed, error);
    }
}

#define TT2_SEED_SLOT_OP(fn, SLOT)                                             \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *, \
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) { \
        uint32_t &s = runtime.rng.state[uint8_t(TT2RngSlot::SLOT)];            \
        if (isSet && stackSize >= 1) { int16_t v=0; if(!popStack(stack,stackSize,v,error))return; s = uint32_t(uint16_t(v)); } \
        else pushStack(stack, stackSize, int16_t(s & 0x7fff), error);          \
    }
TT2_SEED_SLOT_OP(opRandSeed, Rand)
TT2_SEED_SLOT_OP(opTossSeed, Toss)
TT2_SEED_SLOT_OP(opProbSeed, Prob)
TT2_SEED_SLOT_OP(opDrunkSeed, Drunk)
TT2_SEED_SLOT_OP(opPSeed, Pattern)

// ---------------------------------------------------------------------------
// INIT family — reset native state to defaults.
// ---------------------------------------------------------------------------

template<typename Cfg>
static void tt2ResetCvCh(TT2RuntimeT<Cfg> &r, TT2OutputState &o, int i) {
    r.variables.cv[i] = 0; r.variables.cv_off[i] = 0; r.variables.cv_slew[i] = 1;
    tt2SeedCvSlew(o.cv[i], 0, 0);
}
template<typename Cfg>
static void tt2ResetTrCh(TT2RuntimeT<Cfg> &r, TT2OutputState &o, int i) {
    r.variables.tr[i] = 0; r.variables.tr_pol[i] = 1; r.variables.tr_time[i] = 100;
    o.tr[i].level = 0; o.tr[i].restLevel = 0; o.tr[i].pulseRemainingMs = 0;
}
template<typename Cfg>
static void tt2InitPatternN(const TeletypeProgramT<Cfg> *program, int pn) {
    if (!program || pn < 0 || pn >= Cfg::PatternCount) return;
    TT2PatternT<Cfg> *p = &const_cast<TeletypeProgramT<Cfg> *>(program)->patterns[pn];
    p->idx = 0; p->len = 0; p->wrap = 1; p->start = 0; p->end = Cfg::PatternLength - 1;
    for (int i = 0; i < Cfg::PatternLength; ++i) p->val[i] = 0;
}

template<typename Cfg>
static void opInitCv(TT2RuntimeT<Cfg> &r, TT2OutputState &o, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
    int i = v - 1; if (i >= 0 && i < TT2_OUTPUT_CV_COUNT) tt2ResetCvCh(r, o, i);
}
template<typename Cfg>
static void opInitCvAll(TT2RuntimeT<Cfg> &r, TT2OutputState &o, const TeletypeProgramT<Cfg> *,
                        int16_t *, uint8_t &, bool, TT2EvalError &) {
    for (int i = 0; i < TT2_OUTPUT_CV_COUNT; ++i) tt2ResetCvCh(r, o, i);
}
template<typename Cfg>
static void opInitTr(TT2RuntimeT<Cfg> &r, TT2OutputState &o, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
    int i = v - 1; if (i >= 0 && i < TT2_OUTPUT_TR_COUNT) tt2ResetTrCh(r, o, i);
}
template<typename Cfg>
static void opInitTrAll(TT2RuntimeT<Cfg> &r, TT2OutputState &o, const TeletypeProgramT<Cfg> *,
                        int16_t *, uint8_t &, bool, TT2EvalError &) {
    for (int i = 0; i < TT2_OUTPUT_TR_COUNT; ++i) tt2ResetTrCh(r, o, i);
}
template<typename Cfg>
static void opInitP(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
    tt2InitPatternN(program, v);  // 0-based, upstream
}
template<typename Cfg>
static void opInitPAll(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                       int16_t *, uint8_t &, bool, TT2EvalError &) {
    for (int i = 0; i < Cfg::PatternCount; ++i) tt2InitPatternN(program, i);
}
template<typename Cfg>
static void tt2ClearScriptN(const TeletypeProgramT<Cfg> *program, int idx) {
    if (!program || idx < 0 || idx >= Cfg::ScriptCount) return;
    TT2Script &s = const_cast<TeletypeProgramT<Cfg> *>(program)->scripts[idx];
    s.length = 0;
    for (int c = 0; c < TT2_COMMANDS_PER_SCRIPT; ++c) s.commands[c].length = 0;
}
template<typename Cfg>
static void opInitScript(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                         int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
    tt2ClearScriptN(program, v - 1);
}
template<typename Cfg>
static void opInitScriptAll(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                            int16_t *, uint8_t &, bool, TT2EvalError &) {
    for (int i = 0; i < Cfg::ScriptCount; ++i) tt2ClearScriptN(program, i);
}
template<typename Cfg>
static void opInitData(TT2RuntimeT<Cfg> &r, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                       int16_t *, uint8_t &, bool, TT2EvalError &) {
    tt2InitVariables(r.variables);
}
template<typename Cfg>
static void opInitTime(TT2RuntimeT<Cfg> &r, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                       int16_t *, uint8_t &, bool, TT2EvalError &) {
    tt2DelayClear(r);
    r.variables.time = int64_t(r.clockMs);  // TIME reads 0 right after
    for (int i = 0; i < Cfg::ScriptCount; ++i) r.scriptLastMs[i] = r.clockMs;  // LAST -> 0
}
template<typename Cfg>
static void opInit(TT2RuntimeT<Cfg> &r, TT2OutputState &o, const TeletypeProgramT<Cfg> *program,
                   int16_t *, uint8_t &, bool, TT2EvalError &) {
    init(r);
    init(o);
    for (int i = 0; i < Cfg::PatternCount; ++i) tt2InitPatternN(program, i);
}

// ---------------------------------------------------------------------------
// Transport / cross-track / bus ops (W*, RT, BUS) — via the active TT2Host.
// Null host (editor / tests) -> read 0, write no-op.
// ---------------------------------------------------------------------------

// BPM a — ms-per-step for a given BPM (pure formula, upstream op_BPM).
template<typename Cfg>
static void opBpm(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0; if (!popStack(stack, stackSize, a, error)) return;
    if (a < 2) a = 2; if (a > 1000) a = 1000;
    uint32_t ret = ((uint32_t(1u << 31) / ((uint32_t(a) << 20) / 60)) * 1000) >> 10;
    ret = ret / 2 + (ret & 1);
    pushStack(stack, stackSize, int16_t(ret), error);
}

template<typename Cfg>
static void opWbpm(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostTempo() : 0, error);
}
template<typename Cfg>
static void opWbpmS(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
    if (TT2Host *h = tt2ActiveHost()) h->hostSetTempo(v);
}
template<typename Cfg>
static void opWms(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t mult = 0; if (!popStack(stack, stackSize, mult, error)) return;
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostWms(uint8_t(mult)) : 0, error);
}
template<typename Cfg>
static void opWtu(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t div = 0, mult = 0;
    if (!popStack(stack, stackSize, div, error)) return;
    if (!popStack(stack, stackSize, mult, error)) return;
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostWtu(uint8_t(div), uint8_t(mult)) : 0, error);
}
template<typename Cfg>
static void opBar(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t bars = 0; if (!popStack(stack, stackSize, bars, error)) return;
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostBarFraction(uint8_t(bars < 1 ? 1 : bars)) : 0, error);
}
template<typename Cfg>
static void opWp(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t = 0; if (!popStack(stack, stackSize, t, error)) return;
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostTrackPattern(uint8_t(t - 1)) : 0, error);
}
template<typename Cfg>
static void opWpSet(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t = 0, p = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    if (!popStack(stack, stackSize, p, error)) return;
    if (TT2Host *h = tt2ActiveHost()) h->hostSetTrackPattern(uint8_t(t - 1), uint8_t(p));
}
template<typename Cfg>
static void opWr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostTransportRunning() : 0, error);
}
template<typename Cfg>
static void opWrAct(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t s = 0; if (!popStack(stack, stackSize, s, error)) return;
        if (TT2Host *h = tt2ActiveHost()) h->hostSetTransportRunning(s);
    } else {
        TT2Host *h = tt2ActiveHost();
        pushStack(stack, stackSize, h ? h->hostTransportRunning() : 0, error);
    }
}
template<typename Cfg>
static void opWng(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t t = 0, s = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    if (!popStack(stack, stackSize, s, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostNoteGateSet(uint8_t(t - 1), uint8_t(s), v);
    } else {
        pushStack(stack, stackSize, h ? h->hostNoteGateGet(uint8_t(t - 1), uint8_t(s)) : 0, error);
    }
}
template<typename Cfg>
static void opWpn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t t = 0, b = 0, i = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    if (!popStack(stack, stackSize, i, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostSetTrackPatternVal(uint8_t(t - 1), b, i, v);
    } else {
        pushStack(stack, stackSize, h ? h->hostTrackPatternVal(uint8_t(t - 1), b, i) : 0, error);
    }
}
template<typename Cfg>
static void opWs(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t = 0, n = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    if (!popStack(stack, stackSize, n, error)) return;
    if (TT2Host *h = tt2ActiveHost()) error = h->hostTriggerTrackScript(uint8_t(t - 1), n);
}
template<typename Cfg>
static void opWnn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t t = 0, s = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    if (!popStack(stack, stackSize, s, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostNoteNoteSet(uint8_t(t - 1), uint8_t(s), v);
    } else {
        pushStack(stack, stackSize, h ? h->hostNoteNoteGet(uint8_t(t - 1), uint8_t(s)) : 0, error);
    }
}
template<typename Cfg>
static void opWngH(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t = 0; if (!popStack(stack, stackSize, t, error)) return;
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostNoteGateHere(uint8_t(t - 1)) : 0, error);
}
template<typename Cfg>
static void opWnnH(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t = 0; if (!popStack(stack, stackSize, t, error)) return;
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostNoteNoteHere(uint8_t(t - 1)) : 0, error);
}
template<typename Cfg>
static void opRt(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t i = 0; if (!popStack(stack, stackSize, i, error)) return;
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostRoutingSource(uint8_t(i - 1)) : 0, error);
}
template<typename Cfg>
static void opBus(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, 4)) return;  // 1-based -> 0-based, 4 lanes
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostSetBusCv(uint8_t(idx), normaliseCvValue(v));
    } else {
        pushStack(stack, stackSize, h ? h->hostBusCv(uint8_t(idx)) : 0, error);
    }
}

template<typename Cfg>
static void opTv(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t i = 0;
    if (!popStack(stack, stackSize, i, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSetPosition && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostTvSet(uint8_t(i), v);
    } else {
        pushStack(stack, stackSize, h ? h->hostTvGet(uint8_t(i)) : 0, error);
    }
}

// ---------------------------------------------------------------------------
// Geode ops (G.*) — drive the live GeodeConfig/GeodeEngine via the active host.
// Run = M2 output (G.RUN), voices land in M3-M8. G.O/G.BAR/G.R have no live
// equivalent and stay unregistered (UnsupportedOp): run via MO 2, bars from the
// transport, voice CV routing via the modulator routing matrix.
// ---------------------------------------------------------------------------

#define TT2_GEODE_GLOBAL_OP(fn, getter, setter)                                \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,    \
                   int16_t *stack, uint8_t &stackSize, bool isSet,             \
                   TT2EvalError &error) {                                      \
        TT2Host *h = tt2ActiveHost();                                          \
        if (isSet && stackSize >= 1) {                                        \
            int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return; \
            if (h) h->hostGeodeConfig().setter(v);                            \
        } else {                                                              \
            pushStack(stack, stackSize,                                       \
                      h ? int16_t(h->hostGeodeConfig().getter()) : 0, error); \
        }                                                                     \
    }
TT2_GEODE_GLOBAL_OP(opGTime, time, setTime)
TT2_GEODE_GLOBAL_OP(opGTone, intone, setIntone)
TT2_GEODE_GLOBAL_OP(opGRamp, ramp, setRamp)
TT2_GEODE_GLOBAL_OP(opGCurv, curve, setCurve)
TT2_GEODE_GLOBAL_OP(opGMode, mode, setMode)

template<typename Cfg>
static void opGRun(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostGeodeSetRun(v);
    } else {
        pushStack(stack, stackSize, h ? h->hostGeodeRun() : 0, error);
    }
}

template<typename Cfg>
static void opGVal(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostGeodeMix() : 0, error);
}

// G.TUNE v num den (v=0 -> all voices). pops leftmost-first = doc order.
template<typename Cfg>
static void opGTune(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0, num = 0, den = 0;
    if (!popStack(stack, stackSize, v, error)) return;
    if (!popStack(stack, stackSize, num, error)) return;
    if (!popStack(stack, stackSize, den, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (!h) return;
    GeodeConfig &g = h->hostGeodeConfig();
    if (v == 0) {
        for (int i = 0; i < GeodeConfig::VoiceCount; ++i) g.setTune(i, num, den);
    } else if (v >= 1 && v <= GeodeConfig::VoiceCount) {
        g.setTune(v - 1, num, den);
    }
}

// G.V v divs reps (v=0 -> all voices). Host mirrors the live gate-fire sequence.
template<typename Cfg>
static void opGV(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t v = 0, divs = 0, reps = 0;
    if (!popStack(stack, stackSize, v, error)) return;
    if (!popStack(stack, stackSize, divs, error)) return;
    if (!popStack(stack, stackSize, reps, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (!h) return;
    if (v == 0) {
        h->hostGeodeTriggerAll(divs, reps);
    } else if (v >= 1 && v <= GeodeConfig::VoiceCount) {
        h->hostGeodeTriggerVoice(uint8_t(v - 1), divs, reps);
    }
}

// G.S t i n -> set TIME, TONE(intone), RUN in one line (no trigger).
template<typename Cfg>
static void opGS(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t = 0, i = 0, n = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    if (!popStack(stack, stackSize, i, error)) return;
    if (!popStack(stack, stackSize, n, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (!h) return;
    h->hostGeodeConfig().setTime(t);
    h->hostGeodeConfig().setIntone(i);
    h->hostGeodeSetRun(n);
}

// ---------------------------------------------------------------------------
// Modulator ops (MO.*) — drive the global modulator slots via the param
// dictionary on the active host. Slot index 1-based (1..8); bare MO reads the
// slot's current output. MO.P addresses any field; named verbs are fixed-addr
// sugar. gateSource (MO.P addr 14) takes a raw Routing::Source ordinal — an
// advanced, unvalidated write (no UI curation/self-filter).
// ---------------------------------------------------------------------------

template<typename Cfg>
static void opMo(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return;
    TT2Host *h = tt2ActiveHost();
    pushStack(stack, stackSize, h ? h->hostModulatorOutput(uint8_t(idx)) : 0, error);
}

template<typename Cfg>
static void opMoP(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return;
    int16_t addr = 0;
    if (!popStack(stack, stackSize, addr, error)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) h->hostModulator(uint8_t(idx)).paramSet(addr, v);
    } else {
        pushStack(stack, stackSize,
                  h ? int16_t(h->hostModulator(uint8_t(idx)).paramGet(addr)) : 0, error);
    }
}

#define TT2_MO_FIELD_OP(fn, paramName)                                         \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,    \
                   int16_t *stack, uint8_t &stackSize, bool isSet,             \
                   TT2EvalError &error) {                                      \
        int16_t idx = 0;                                                       \
        if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return; \
        const int addr = int(Modulator::Param::paramName);                     \
        TT2Host *h = tt2ActiveHost();                                          \
        if (isSet && stackSize >= 1) {                                        \
            int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return; \
            if (h) h->hostModulator(uint8_t(idx)).paramSet(addr, v);          \
        } else {                                                              \
            pushStack(stack, stackSize,                                       \
                      h ? int16_t(h->hostModulator(uint8_t(idx)).paramGet(addr)) : 0, error); \
        }                                                                     \
    }
TT2_MO_FIELD_OP(opMoShape, Shape)
TT2_MO_FIELD_OP(opMoRate, Rate)
TT2_MO_FIELD_OP(opMoDepth, Depth)
TT2_MO_FIELD_OP(opMoMode, Mode)
TT2_MO_FIELD_OP(opMoOff, Offset)

template<typename Cfg>
static void opMoTrig(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return;
    TT2Host *h = tt2ActiveHost();
    if (h) h->hostModulatorTrigger(uint8_t(idx));
}

// ---------------------------------------------------------------------------
// Envelope ops (E.*) — ADSR-shape-locked sugar over the same modulator slots
// as MO.*. The op family selects the shape: every write claims slot n as a
// triggered ADSR (Shape::ADSR + Mode::Trig), then sets one field. Reads do not
// mutate. Index 1-based (1..8). LFO.* deferred (rate representation undecided).
// ---------------------------------------------------------------------------

static void tt2ForceEnvelope(Modulator &m) {
    m.setShape(Modulator::Shape::ADSR);
    m.setMode(Modulator::Mode::Trig);
}

#define TT2_E_FIELD_OP(fn, getter, setter)                                     \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,    \
                   int16_t *stack, uint8_t &stackSize, bool isSet,             \
                   TT2EvalError &error) {                                      \
        int16_t idx = 0;                                                       \
        if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return; \
        TT2Host *h = tt2ActiveHost();                                          \
        if (isSet && stackSize >= 1) {                                        \
            int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return; \
            if (h) { Modulator &m = h->hostModulator(uint8_t(idx)); tt2ForceEnvelope(m); m.setter(v); } \
        } else {                                                              \
            pushStack(stack, stackSize,                                       \
                      h ? int16_t(h->hostModulator(uint8_t(idx)).getter()) : 0, error); \
        }                                                                     \
    }
TT2_E_FIELD_OP(opEnvA, attack, setAttack)
TT2_E_FIELD_OP(opEnvD, decay, setDecay)
TT2_E_FIELD_OP(opEnvO, offset, setOffset)

// Bare E: set claims the slot + sets the envelope peak (Amplitude); read pushes
// the slot's live output (mirrors bare MO), not the Amplitude field.
template<typename Cfg>
static void opEnv(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) { Modulator &m = h->hostModulator(uint8_t(idx)); tt2ForceEnvelope(m); m.setAmplitude(v); }
    } else {
        pushStack(stack, stackSize, h ? h->hostModulatorOutput(uint8_t(idx)) : 0, error);
    }
}

template<typename Cfg>
static void opEnvT(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return;
    TT2Host *h = tt2ActiveHost();
    if (h) { tt2ForceEnvelope(h->hostModulator(uint8_t(idx))); h->hostModulatorTrigger(uint8_t(idx)); }
}

// ---------------------------------------------------------------------------
// LFO ops (LFO.*) — Sine-shape-locked, free-running aliases over the modulator
// slots. Every write forces Shape::Sine + Mode::Run. LFO.R sets a free rate in
// centi-Hz; LFO.C a clocked rate from a note divisor (rate = PPQN*2/d). LFO.A/O
// set depth/offset. Index 1-based. LFO.W/F/S deferred.
// ---------------------------------------------------------------------------

static void tt2ForceLfo(Modulator &m) {
    m.setShape(Modulator::Shape::Sine);
    m.setMode(Modulator::Mode::Run);
}

#define TT2_LFO_FIELD_OP(fn, getter, setter)                                   \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,    \
                   int16_t *stack, uint8_t &stackSize, bool isSet,             \
                   TT2EvalError &error) {                                      \
        int16_t idx = 0;                                                       \
        if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return; \
        TT2Host *h = tt2ActiveHost();                                          \
        if (isSet && stackSize >= 1) {                                        \
            int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return; \
            if (h) { Modulator &m = h->hostModulator(uint8_t(idx)); tt2ForceLfo(m); m.setter(v); } \
        } else {                                                              \
            pushStack(stack, stackSize,                                       \
                      h ? int16_t(h->hostModulator(uint8_t(idx)).getter()) : 0, error); \
        }                                                                     \
    }
TT2_LFO_FIELD_OP(opLfoA, depth, setDepth)
TT2_LFO_FIELD_OP(opLfoO, offset, setOffset)

// LFO.R n x — free rate in centi-Hz (Free domain). Set the domain before the
// rate so setRate() clamps into the Free range (1..1600).
template<typename Cfg>
static void opLfoR(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        if (h) {
            Modulator &m = h->hostModulator(uint8_t(idx));
            tt2ForceLfo(m);
            m.setRateDomain(Modulator::RateDomain::Free);
            m.setRate(v);
        }
    } else {
        pushStack(stack, stackSize, h ? int16_t(h->hostModulator(uint8_t(idx)).rate()) : 0, error);
    }
}

// LFO.C n d — clocked rate from a note divisor (Tempo domain). rate = PPQN*2/d;
// the setter clamps into the Tempo range (6..6144). d floored to 1.
template<typename Cfg>
static void opLfoC(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, CONFIG_MODULATOR_COUNT)) return;
    TT2Host *h = tt2ActiveHost();
    if (isSet && stackSize >= 1) {
        int16_t d = 0; if (!popStack(stack, stackSize, d, error)) return;
        if (h) {
            Modulator &m = h->hostModulator(uint8_t(idx));
            tt2ForceLfo(m);
            m.setRateDomain(Modulator::RateDomain::Tempo);
            int div = d < 1 ? 1 : int(d);
            m.setRate((CONFIG_PPQN * 2) / div);
        }
    } else {
        pushStack(stack, stackSize, h ? int16_t(h->hostModulator(uint8_t(idx)).rate()) : 0, error);
    }
}

// PRINT / PRT — 16-slot dashboard value store in the runtime (ephemeral, like
// the TT1-track g_dashboardValues). PRINT n x writes slot n (1-based), PRINT n
// reads it. Pure runtime state — no host. PRT is an alias.
template<typename Cfg>
static void opPrint(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_PRINT_SLOT_COUNT)) return;
    if (isSet && stackSize >= 1) {
        int16_t v = 0; if (!popStack(stack, stackSize, v, error)) return;
        runtime.variables.dashboard[idx] = v;
    } else {
        pushStack(stack, stackSize, runtime.variables.dashboard[idx], error);
    }
}

// ---------------------------------------------------------------------------
// MIDI query ops (MI.*) — read the runtime MIDI buffer. Indexed ops read the
// active frame's I loop var (1-based), matching upstream teletype/src/ops/midi.c.
// ---------------------------------------------------------------------------

template<typename Cfg>
static int16_t tt2MiIndex(TT2RuntimeT<Cfg> &runtime) { return tt2ActiveFrame(runtime).i; }

#define TT2_MI_LAST_OP(fn, expr)                                               \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *, \
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) { \
        const TT2Midi &m = runtime.midi; (void)m;                              \
        pushStack(stack, stackSize, int16_t(expr), error);                     \
    }
TT2_MI_LAST_OP(opMiLe,   m.last_event_type)
TT2_MI_LAST_OP(opMiLn,   m.last_note)
TT2_MI_LAST_OP(opMiLo,   m.last_note)
TT2_MI_LAST_OP(opMiLnv,  tt2EtAt(m.last_note))
TT2_MI_LAST_OP(opMiLv,   m.last_velocity)
TT2_MI_LAST_OP(opMiLvv,  m.last_velocity * 129)
TT2_MI_LAST_OP(opMiLc,   m.last_controller)
TT2_MI_LAST_OP(opMiLcc,  m.last_cc)
TT2_MI_LAST_OP(opMiLccv, m.last_cc * 129)
TT2_MI_LAST_OP(opMiLch,  m.last_channel + 1)
TT2_MI_LAST_OP(opMiNl,   m.on_count)
TT2_MI_LAST_OP(opMiOl,   m.off_count)
TT2_MI_LAST_OP(opMiCl,   m.cc_count)

// Indexed (read I); 0 when I is out of [1, count].
#define TT2_MI_IDX_OP(fn, count, expr)                                         \
    template<typename Cfg> static void fn(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *, \
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) { \
        const TT2Midi &m = runtime.midi; (void)m;                              \
        int16_t i = tt2MiIndex(runtime);                                       \
        int16_t r = (i < 1 || i > m.count) ? 0 : int16_t(expr);                \
        pushStack(stack, stackSize, r, error);                                 \
    }
TT2_MI_IDX_OP(opMiN,   on_count, m.note_on[i - 1])
TT2_MI_IDX_OP(opMiNv,  on_count, tt2EtAt(m.note_on[i - 1]))
TT2_MI_IDX_OP(opMiV,   on_count, m.note_vel[i - 1])
TT2_MI_IDX_OP(opMiVv,  on_count, m.note_vel[i - 1] * 129)
TT2_MI_IDX_OP(opMiNch, on_count, m.on_channel[i - 1] + 1)
TT2_MI_IDX_OP(opMiO,   off_count, m.note_off[i - 1])
TT2_MI_IDX_OP(opMiOch, off_count, m.off_channel[i - 1] + 1)
TT2_MI_IDX_OP(opMiC,   cc_count, m.cn[i - 1])
TT2_MI_IDX_OP(opMiCc,  cc_count, m.cc[i - 1])
TT2_MI_IDX_OP(opMiCcv, cc_count, m.cc[i - 1] * 129)
TT2_MI_IDX_OP(opMiCch, cc_count, m.cc_channel[i - 1] + 1)

// MI.$ event [script] — get/set the per-event fire-script binding.
template<typename Cfg>
static void opMiDollar(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                       int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    TT2Midi &m = runtime.midi;
    if (isSet && stackSize >= 2) {
        int16_t event = 0, script = 0;
        if (!popStack(stack, stackSize, event, error)) return;
        if (!popStack(stack, stackSize, script, error)) return;
        script -= 1;
        if (script < 0 || script >= Cfg::ScriptCount) script = -1;   // was: script > Cfg::InitScript
        switch (event) {
            case 0: m.on_script = int8_t(script); m.off_script = int8_t(script); m.cc_script = int8_t(script);
                    m.on_count = m.off_count = m.cc_count = 0; break;
            case 1: m.on_script = int8_t(script); m.on_count = 0; break;
            case 2: m.off_script = int8_t(script); m.off_count = 0; break;
            case 3: m.cc_script = int8_t(script); m.cc_count = 0; break;
            default: break;
        }
    } else {
        int16_t event = 0;
        if (!popStack(stack, stackSize, event, error)) return;
        int16_t script = -1;
        switch (event) {
            case 0: script = m.on_script;
                    if (m.on_script != m.off_script || m.on_script != m.cc_script) script = -1;
                    break;
            case 1: script = m.on_script; break;
            case 2: script = m.off_script; break;
            case 3: script = m.cc_script; break;
            default: break;
        }
        pushStack(stack, stackSize, int16_t(script == -1 ? -1 : script + 1), error);
    }
}

// MI.CLKD — MIDI clock divisor (1..24); MI.CLKR — reset (no-op, TT2 has no MIDI clock counter yet).
template<typename Cfg>
static void opMiClkd(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t d = 0; if (!popStack(stack, stackSize, d, error)) return;
        if (d < 1 || d > 24) return;
        runtime.midi.clock_div = uint8_t(d);
    } else {
        pushStack(stack, stackSize, int16_t(runtime.midi.clock_div), error);
    }
}
template<typename Cfg>
static void opMiClkr(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                     int16_t *, uint8_t &, bool, TT2EvalError &) { /* no-op */ }

// ---------------------------------------------------------------------------
// Misc engine ops (TIF, M!, CV.GET/CV.SET, M.A/M.ACT.A, SCRIPT.POL)
// ---------------------------------------------------------------------------

// TIF cond a b — ternary select (pure).
template<typename Cfg>
static void opTif(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t cond = 0, a = 0, b = 0;
    if (!popStack(stack, stackSize, cond, error)) return;
    if (!popStack(stack, stackSize, a, error)) return;
    if (!popStack(stack, stackSize, b, error)) return;
    pushStack(stack, stackSize, cond ? a : b, error);
}

// M! — read the metro period.
template<typename Cfg>
static void opMBang(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, runtime.variables.m, error);
}

// CV.GET n — read the channel's current value.
template<typename Cfg>
static void opCvGet(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_CV_COUNT)) return;
    pushStack(stack, stackSize, runtime.variables.cv[idx], error);
}

// CV.SET n v — set CV directly with NO slew (snap), offset applied.
template<typename Cfg>
static void opCvSet(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, TT2_OUTPUT_CV_COUNT)) return;
    int16_t val = 0;
    if (!popStack(stack, stackSize, val, error)) return;
    val = normaliseCvValue(val);
    runtime.variables.cv[idx] = val;
    int16_t target = normaliseCvValue(int16_t(val + runtime.variables.cv_off[idx]));
    tt2SeedCvSlew(output.cv[idx], target, 0);  // slew 0 -> snap
    output.cvDirty |= uint8_t(1 << idx);
}

// M.A m — set metro period (single-track "all" == this track).
template<typename Cfg>
static void opMA(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t m = 0;
    if (!popStack(stack, stackSize, m, error)) return;
    if (m < 2) m = 2;
    runtime.variables.m = m;
}

// M.ACT.A state — set metro active (single-track "all").
template<typename Cfg>
static void opMActA(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t s = 0;
    if (!popStack(stack, stackSize, s, error)) return;
    runtime.variables.m_act = (s > 0) ? 1 : 0;
}

// SCRIPT.POL / $.POL — per-trigger-input script polarity (a==0 sets all).
template<typename Cfg>
static void opScriptPol(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                        int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 2) {
        int16_t a = 0, pol = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        if (!popStack(stack, stackSize, pol, error)) return;
        if (pol < 0 || pol > 3) return;
        if (a == 0) { for (int i = 0; i < Cfg::TriggerInputCount; ++i) runtime.variables.script_pol[i] = uint8_t(pol); }
        else { int s = a - 1; if (s >= 0 && s < Cfg::TriggerInputCount) runtime.variables.script_pol[s] = uint8_t(pol); }
    } else {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        int s = a - 1;
        pushStack(stack, stackSize, (s >= 0 && s < Cfg::TriggerInputCount) ? int16_t(runtime.variables.script_pol[s]) : 0, error);
    }
}

// ---------------------------------------------------------------------------
// Engine input ops
// ---------------------------------------------------------------------------

// Map a sampled raw input (0..16383) into the configured [outMin, outMax] range.
static int16_t scaleInput(int16_t raw, int16_t outMin, int16_t outMax) {
    if (raw < 0) raw = 0;
    if (raw > 16383) raw = 16383;
    return int16_t(outMin + int32_t(raw) * (outMax - outMin) / 16383);
}

// IN / PARAM — scaled read of the sampled CV input.
template<typename Cfg>
static void opIn(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, scaleInput(runtime.variables.in,
              runtime.variables.in_min, runtime.variables.in_max), error);
}

template<typename Cfg>
static void opParam(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, scaleInput(runtime.variables.param,
              runtime.variables.param_min, runtime.variables.param_max), error);
}

// IN.SCALE / PARAM.SCALE min max — set the output range (leftmost arg = min).
template<typename Cfg>
static void opInScale(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                      int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t mn = 0, mx = 0;
    if (!popStack(stack, stackSize, mn, error)) return;
    if (!popStack(stack, stackSize, mx, error)) return;
    runtime.variables.in_min = mn;
    runtime.variables.in_max = mx;
}

template<typename Cfg>
static void opParamScale(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                         int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t mn = 0, mx = 0;
    if (!popStack(stack, stackSize, mn, error)) return;
    if (!popStack(stack, stackSize, mx, error)) return;
    runtime.variables.param_min = mn;
    runtime.variables.param_max = mx;
}

// STATE n — live level of trigger input n (latched by the engine). Out of
// range pushes 0 (upstream semantics, no error).
template<typename Cfg>
static void opState(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t a = 0;
    if (!popStack(stack, stackSize, a, error)) return;
    int idx = a - 1;
    int16_t level = (idx >= 0 && idx < Cfg::TriggerInputCount)
                        ? int16_t(runtime.inputLevel[idx]) : 0;
    pushStack(stack, stackSize, level, error);
}

// MUTE n [v] — get/set the mute bit for trigger input n.
template<typename Cfg>
static void opMute(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popOutputIndex(stack, stackSize, idx, error, Cfg::TriggerInputCount)) return;
    uint8_t bit = uint8_t(1 << idx);
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        if (val > 0) runtime.variables.mutes |= bit;
        else runtime.variables.mutes &= uint8_t(~bit);
    } else {
        pushStack(stack, stackSize, int16_t((runtime.variables.mutes >> idx) & 1), error);
    }
}

// ---------------------------------------------------------------------------
// Metro ops
// ---------------------------------------------------------------------------

template<typename Cfg>
static void opM(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                int16_t *stack,
                uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        if (val < 2) val = 2;
        runtime.variables.m = val;
        runtime.variables.metroSyncDen = 0;  // plain M -> free ms (exit clock sync)
    } else {
        pushStack(stack, stackSize, runtime.variables.m, error);
    }
}

// M.C n d — clock-synced metro: period = n/d of a whole note, derived from live
// BPM in tt2AdvanceMetro. Set-only (consumes 2); plain M / M! revert to free ms.
template<typename Cfg>
static void opMC(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t n = 0, d = 0;
    if (!popStack(stack, stackSize, n, error)) return;
    if (!popStack(stack, stackSize, d, error)) return;
    if (n < 1) n = 1;
    if (d < 1) d = 1;
    runtime.variables.metroSyncNum = n;
    runtime.variables.metroSyncDen = d;
}

template<typename Cfg>
static void opMAct(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opScript(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                     const TeletypeProgramT<Cfg> *program, int16_t *stack,
                     uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    if (isSetPosition && stackSize >= 1) {
        int16_t oneBased = 0;
        if (!popStack(stack, stackSize, oneBased, error)) return;
        int16_t zeroBased = oneBased - 1;
        if (zeroBased < 0 || zeroBased >= Cfg::ScriptCount) {
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

// Function-call ops — $F/$F1/$F2 call a whole script, $L/$S a single line,
// passing params (read via I1/I2) and returning FR. Native pop order is
// left-to-right, so token roles mirror upstream's reversed cs_pop order.

template<typename Cfg>
static void opF(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                const TeletypeProgramT<Cfg> *program, int16_t *stack,
                uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t script = 0;
    if (!popStack(stack, stackSize, script, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunction(*program, runtime, output, uint8_t(script - 1), 0, 0), error);
}

template<typename Cfg>
static void opF1(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                 const TeletypeProgramT<Cfg> *program, int16_t *stack,
                 uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t param1 = 0, script = 0;
    if (!popStack(stack, stackSize, param1, error)) return;
    if (!popStack(stack, stackSize, script, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunction(*program, runtime, output, uint8_t(script - 1), param1, 0), error);
}

template<typename Cfg>
static void opF2(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                 const TeletypeProgramT<Cfg> *program, int16_t *stack,
                 uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t param2 = 0, param1 = 0, script = 0;
    if (!popStack(stack, stackSize, param2, error)) return;
    if (!popStack(stack, stackSize, param1, error)) return;
    if (!popStack(stack, stackSize, script, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunction(*program, runtime, output, uint8_t(script - 1), param1, param2), error);
}

template<typename Cfg>
static void opL(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                const TeletypeProgramT<Cfg> *program, int16_t *stack,
                uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t line = 0, script = 0;
    if (!popStack(stack, stackSize, line, error)) return;
    if (!popStack(stack, stackSize, script, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunctionLine(*program, runtime, output, uint8_t(script - 1), uint8_t(line - 1), 0, 0), error);
}

template<typename Cfg>
static void opL1(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                 const TeletypeProgramT<Cfg> *program, int16_t *stack,
                 uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t param1 = 0, line = 0, script = 0;
    if (!popStack(stack, stackSize, param1, error)) return;
    if (!popStack(stack, stackSize, line, error)) return;
    if (!popStack(stack, stackSize, script, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunctionLine(*program, runtime, output, uint8_t(script - 1), uint8_t(line - 1), param1, 0), error);
}

template<typename Cfg>
static void opL2(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                 const TeletypeProgramT<Cfg> *program, int16_t *stack,
                 uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t param2 = 0, param1 = 0, line = 0, script = 0;
    if (!popStack(stack, stackSize, param2, error)) return;
    if (!popStack(stack, stackSize, param1, error)) return;
    if (!popStack(stack, stackSize, line, error)) return;
    if (!popStack(stack, stackSize, script, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunctionLine(*program, runtime, output, uint8_t(script - 1), uint8_t(line - 1), param1, param2), error);
}

template<typename Cfg>
static void opS(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                const TeletypeProgramT<Cfg> *program, int16_t *stack,
                uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t line = 0;
    if (!popStack(stack, stackSize, line, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunctionLine(*program, runtime, output, tt2ActiveScriptNumber(runtime), uint8_t(line - 1), 0, 0), error);
}

template<typename Cfg>
static void opS1(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                 const TeletypeProgramT<Cfg> *program, int16_t *stack,
                 uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t param1 = 0, line = 0;
    if (!popStack(stack, stackSize, param1, error)) return;
    if (!popStack(stack, stackSize, line, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunctionLine(*program, runtime, output, tt2ActiveScriptNumber(runtime), uint8_t(line - 1), param1, 0), error);
}

template<typename Cfg>
static void opS2(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                 const TeletypeProgramT<Cfg> *program, int16_t *stack,
                 uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t param2 = 0, param1 = 0, line = 0;
    if (!popStack(stack, stackSize, param2, error)) return;
    if (!popStack(stack, stackSize, param1, error)) return;
    if (!popStack(stack, stackSize, line, error)) return;
    if (!program) { error = TT2EvalError::NoTrack; return; }
    pushStack(stack, stackSize, tt2RunFunctionLine(*program, runtime, output, tt2ActiveScriptNumber(runtime), uint8_t(line - 1), param1, param2), error);
}

// I1 / I2 — read the active function call's input params.
template<typename Cfg>
static void opI1(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, tt2ActiveFrame(runtime).fparam1, error);
}

template<typename Cfg>
static void opI2(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, tt2ActiveFrame(runtime).fparam2, error);
}

// FR — get/set the active function's return value.
template<typename Cfg>
static void opFr(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool isSetPosition, TT2EvalError &error) {
    TT2ExecFrame &frame = tt2ActiveFrame(runtime);
    if (isSetPosition && stackSize >= 1) {
        int16_t val = 0;
        if (!popStack(stack, stackSize, val, error)) return;
        frame.fresult = val;
        frame.fresult_set = 1;
    } else {
        pushStack(stack, stackSize, frame.fresult, error);
    }
}

// DEL.CLR — clear the delay queue (faithful subset: TT2 has no TR pulse
// timers / persistent op stack to also flush).
template<typename Cfg>
static void opDelClr(TT2RuntimeT<Cfg> &runtime, TT2OutputState &,
                     const TeletypeProgramT<Cfg> *, int16_t *,
                     uint8_t &, bool, TT2EvalError &) {
    tt2DelayClear(runtime);
}

// BREAK / BRK — flag the active exec frame so the enclosing L loop stops.
template<typename Cfg>
static void opBreak(TT2RuntimeT<Cfg> &runtime, TT2OutputState &,
                    const TeletypeProgramT<Cfg> *, int16_t *,
                    uint8_t &, bool, TT2EvalError &) {
    tt2ActiveBreaking(runtime) = 1;
}

// KILL — clear the stack, disable the metro, and clear the delay queue
// (upstream op_KILL_get; TT2 has no TR pulse timers to also flush).
template<typename Cfg>
static void opKill(TT2RuntimeT<Cfg> &runtime, TT2OutputState &,
                   const TeletypeProgramT<Cfg> *, int16_t *,
                   uint8_t &, bool, TT2EvalError &) {
    runtime.stack.top = 0;
    runtime.variables.m_act = 0;
    tt2DelayClear(runtime);
}

// ---------------------------------------------------------------------------
// Queue ops — TT2_Q_LENGTH ring; get vs set by leftmost position + arg count.
// Q.RND (rand source) and Q.2P / Q.P2 (pattern store) deferred to later batches.
// ---------------------------------------------------------------------------

template<typename Cfg>
static void opQ(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQN(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQAvg(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQClr(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t first = 0;
    bool doSet = isSet && stackSize >= 1;
    if (doSet && !popStack(stack, stackSize, first, error)) return;
    v.q_n = 1;
    for (int i = 0; i < TT2_Q_LENGTH; ++i) v.q[i] = 0;
    if (doSet) v.q[0] = first;
}

template<typename Cfg>
static void opQGrw(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQSum(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    auto &v = runtime.variables;
    int16_t sum = 0;
    for (int i = 0; i < v.q_n; ++i) sum += v.q[i];
    pushStack(stack, stackSize, sum, error);
}

template<typename Cfg>
static void opQMin(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQMax(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQSrt(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQRev(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *, uint8_t &, bool, TT2EvalError &) {
    auto &v = runtime.variables;
    for (int i = 0; i < v.q_n / 2; ++i) {
        int16_t tmp = v.q[i];
        v.q[i] = v.q[v.q_n - 1 - i];
        v.q[v.q_n - 1 - i] = tmp;
    }
}

template<typename Cfg>
static void opQSh(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQAdd(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg> static int16_t normalisePn(int16_t pn);  // defined with the pattern ops below

// Q.2P [i] — copy the queue (0..q_n-1) into the current pattern (p_n) or pattern i.
template<typename Cfg>
static void opQ2P(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    if (!program) { error = TT2EvalError::NoTrack; return; }
    auto &v = runtime.variables;
    int16_t pn = v.p_n;
    if (stackSize >= 1) { if (!popStack(stack, stackSize, pn, error)) return; }
    TT2PatternT<Cfg> &pat = const_cast<TeletypeProgramT<Cfg> *>(program)->patterns[normalisePn<Cfg>(pn)];
    int n = v.q_n; if (n < 0) n = 0; if (n > Cfg::PatternLength) n = Cfg::PatternLength;
    for (int i = 0; i < n; ++i) pat.val[i] = v.q[i];
    pat.len = uint16_t(n);
}

// Q.P2 [i] — copy the whole current pattern (or pattern i) into the queue; queue
// length becomes the pattern length (Q.P2 copies the entire pattern, not up to len).
template<typename Cfg>
static void opQP2(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    if (!program) { error = TT2EvalError::NoTrack; return; }
    auto &v = runtime.variables;
    int16_t pn = v.p_n;
    if (stackSize >= 1) { if (!popStack(stack, stackSize, pn, error)) return; }
    const TT2PatternT<Cfg> &pat = program->patterns[normalisePn<Cfg>(pn)];
    for (int i = 0; i < Cfg::PatternLength && i < TT2_Q_LENGTH; ++i) v.q[i] = pat.val[i];
    int16_t len = int16_t(pat.len);
    if (len < 1) len = 1; else if (len > TT2_Q_LENGTH) len = TT2_Q_LENGTH;
    v.q_n = len;
}

// Q.RND [x] — get a random element from the queue; if x>0, set all active
// elements to one random value.
template<typename Cfg>
static void opQRnd(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    auto &v = runtime.variables;
    int n = v.q_n < 1 ? 1 : (v.q_n > TT2_Q_LENGTH ? TT2_Q_LENGTH : v.q_n);
    if (isSet && stackSize >= 1) {
        int16_t x = 0; if (!popStack(stack, stackSize, x, error)) return;
        if (x > 0) {
            int16_t r = int16_t(tt2RngNext(runtime.rng, TT2RngSlot::Rand));
            for (int i = 0; i < n; ++i) v.q[i] = r;
        }
    } else {
        uint32_t idx = tt2RngRange(runtime.rng, TT2RngSlot::Rand, uint32_t(n));
        pushStack(stack, stackSize, v.q[idx], error);
    }
}

template<typename Cfg>
static void opQSub(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQMul(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQDiv(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQMod(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void opQI(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
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

template<typename Cfg>
static void runStoredCommand(const TT2RuntimeCommand &body, TT2RuntimeT<Cfg> &runtime,
                             TT2OutputState &output, const TeletypeProgramT<Cfg> *program) {
    TT2Command cmd;
    cmd.length = body.length;
    for (int k = 0; k < TT2_COMMAND_MAX_LENGTH; ++k) {
        cmd.tag[k] = body.tag[k];
        cmd.value[k] = body.value[k];
    }
    evaluateCommand(cmd, runtime, output, program);
}

// S.ALL — run every stacked command (most-recent first), then clear.
template<typename Cfg>
static void opSAll(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                   const TeletypeProgramT<Cfg> *program, int16_t *, uint8_t &, bool,
                   TT2EvalError &) {
    uint8_t n = runtime.stack.top;
    for (uint8_t i = 0; i < n; ++i) {
        runStoredCommand(runtime.stack.commands[n - 1 - i], runtime, output, program);
    }
    runtime.stack.top = 0;
}

// S.POP — run the most-recently pushed command, leaving the rest.
template<typename Cfg>
static void opSPop(TT2RuntimeT<Cfg> &runtime, TT2OutputState &output,
                   const TeletypeProgramT<Cfg> *program, int16_t *, uint8_t &, bool,
                   TT2EvalError &) {
    if (runtime.stack.top > 0) {
        runtime.stack.top--;
        runStoredCommand(runtime.stack.commands[runtime.stack.top], runtime, output, program);
    }
}

template<typename Cfg>
static void opSClr(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                   int16_t *, uint8_t &, bool, TT2EvalError &) {
    runtime.stack.top = 0;
}

template<typename Cfg>
static void opSL(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                 int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, int16_t(runtime.stack.top), error);
}

// ---------------------------------------------------------------------------
// Pattern ops — P.* (working pattern p_n) and PN.* (explicit pattern arg) over
// the persisted TeletypeProgram.patterns[4] store. Semantics match patterns.c.
// ---------------------------------------------------------------------------

template<typename Cfg> static int16_t normalisePn(int16_t pn) {
    if (pn < 0) return 0;
    if (pn >= Cfg::PatternCount) return Cfg::PatternCount - 1;
    return pn;
}

// Patterns are mutable scene state persisted in TeletypeProgram; the evaluator
// reaches the program through a const pointer (scripts don't change mid-run),
// so pattern-write ops cast it away. The TT2Track::_program object is non-const.
template<typename Cfg>
static TT2PatternT<Cfg> *mutablePattern(const TeletypeProgramT<Cfg> *program, int16_t pn) {
    if (!program) return nullptr;
    return &const_cast<TeletypeProgramT<Cfg> *>(program)->patterns[normalisePn<Cfg>(pn)];
}

// Negative idx counts from the back (upstream normalise_idx); clamp to range.
template<typename Cfg> static int16_t normaliseIdx(const TT2PatternT<Cfg> &p, int16_t idx) {
    int16_t len = int16_t(p.len);
    if (idx < 0) {
        if (idx < -len) idx = 0;
        else idx = len + idx;
    }
    if (idx >= Cfg::PatternLength) idx = Cfg::PatternLength - 1;
    if (idx < 0) idx = 0;
    return idx;
}

// P.N — working pattern selector.
template<typename Cfg>
static void opPatternN(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *,
                       int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        runtime.variables.p_n = normalisePn<Cfg>(a);
    } else {
        pushStack(stack, stackSize, runtime.variables.p_n, error);
    }
}

// --- value workers (shared by P.* and PN.*) -------------------------------

template<typename Cfg>
static int16_t patternGetVal(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t idx) {
    const TT2PatternT<Cfg> &p = program->patterns[normalisePn<Cfg>(pn)];
    return p.val[normaliseIdx(p, idx)];
}

template<typename Cfg>
static void patternSetVal(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t idx,
                          int16_t val) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (p) p->val[normaliseIdx(*p, idx)] = val;
}

template<typename Cfg>
static void patternSetLen(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t l) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return;
    if (l < 0) l = 0; else if (l > Cfg::PatternLength) l = Cfg::PatternLength;
    p->len = uint16_t(l);
}

template<typename Cfg>
static void patternSetIdx(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t i) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return;
    i = normaliseIdx(*p, i);
    int16_t len = int16_t(p->len);
    if (i < 0 || len == 0) p->idx = 0;
    else if (i >= len) p->idx = int16_t(len - 1);
    else p->idx = i;
}

// P / PN — indexed value access. P: idx [val] (working pattern). PN: pn idx [val].
template<typename Cfg>
static void opP(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
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

template<typename Cfg>
static void opPN(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
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

// Rescale every value in pattern pn's [start,end] window from [in_min,in_max]
// to [out_min,out_max] (upstream p_pat_scale; uses the shared SCALE math).
template<typename Cfg>
static void patScale(const TeletypeProgramT<Cfg> *program, int16_t pn,
                     int16_t inMin, int16_t inMax, int16_t outMin, int16_t outMax) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    int start = p->start, end = p->end;
    if (start < 0) start = 0;
    if (end > Cfg::PatternLength - 1) end = Cfg::PatternLength - 1;
    for (int idx = start; idx <= end; ++idx) {
        p->val[idx] = tt2ScaleVal(p->val[idx], inMin, inMax, outMin, outMax);
    }
}

// P.SCALE in_min in_max out_min out_max — rescale the working pattern.
template<typename Cfg>
static void opPScale(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t inMin = 0, inMax = 0, outMin = 0, outMax = 0;
    if (!popStack(stack, stackSize, inMin, error)) return;
    if (!popStack(stack, stackSize, inMax, error)) return;
    if (!popStack(stack, stackSize, outMin, error)) return;
    if (!popStack(stack, stackSize, outMax, error)) return;
    patScale(program, runtime.variables.p_n, inMin, inMax, outMin, outMax);
}

// PN.SCALE pn in_min in_max out_min out_max — rescale an explicit pattern.
template<typename Cfg>
static void opPNScale(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                      int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0, inMin = 0, inMax = 0, outMin = 0, outMax = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (!popStack(stack, stackSize, inMin, error)) return;
    if (!popStack(stack, stackSize, inMax, error)) return;
    if (!popStack(stack, stackSize, outMin, error)) return;
    if (!popStack(stack, stackSize, outMax, error)) return;
    patScale(program, pn, inMin, inMax, outMin, outMax);
}

// P.L / PN.L — length.
template<typename Cfg>
static void opPL(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                 int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t l = 0;
        if (!popStack(stack, stackSize, l, error)) return;
        patternSetLen(program, runtime.variables.p_n, l);
    } else {
        pushStack(stack, stackSize, int16_t(program->patterns[normalisePn<Cfg>(runtime.variables.p_n)].len), error);
    }
}

template<typename Cfg>
static void opPNL(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t l = 0;
        if (!popStack(stack, stackSize, l, error)) return;
        patternSetLen(program, pn, l);
    } else {
        pushStack(stack, stackSize, int16_t(program->patterns[normalisePn<Cfg>(pn)].len), error);
    }
}

// P.I / PN.I — working index.
template<typename Cfg>
static void opPI(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                 int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t i = 0;
        if (!popStack(stack, stackSize, i, error)) return;
        patternSetIdx(program, runtime.variables.p_n, i);
    } else {
        pushStack(stack, stackSize, program->patterns[normalisePn<Cfg>(runtime.variables.p_n)].idx, error);
    }
}

template<typename Cfg>
static void opPNI(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                  int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t i = 0;
        if (!popStack(stack, stackSize, i, error)) return;
        patternSetIdx(program, pn, i);
    } else {
        pushStack(stack, stackSize, program->patterns[normalisePn<Cfg>(pn)].idx, error);
    }
}

// P.HERE / PN.HERE — value at the working index (no advance).
template<typename Cfg>
static void opPHere(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = normalisePn<Cfg>(runtime.variables.p_n);
    const TT2PatternT<Cfg> &p = program->patterns[pn];
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2PatternT<Cfg> *m = mutablePattern(program, pn);
        if (m) m->val[m->idx] = a;
    } else {
        pushStack(stack, stackSize, p.val[p.idx], error);
    }
}

template<typename Cfg>
static void opPNHere(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    pn = normalisePn<Cfg>(pn);
    const TT2PatternT<Cfg> &p = program->patterns[pn];
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2PatternT<Cfg> *m = mutablePattern(program, pn);
        if (m) m->val[m->idx] = a;
    } else {
        pushStack(stack, stackSize, p.val[p.idx], error);
    }
}

// --- bounds (WRAP / START / END) and navigation (NEXT / PREV) --------------

// Advance the working index obeying START/END/WRAP/L (upstream p_next_inc_i).
template<typename Cfg> static void patternNextInc(TT2PatternT<Cfg> &p) {
    int16_t len = int16_t(p.len), start = p.start, end = p.end, idx = p.idx;
    if (idx == int16_t(len - 1) || idx == end) {
        if (p.wrap) idx = start;
    } else {
        idx++;
    }
    if (idx > len || idx < 0 || idx >= Cfg::PatternLength) idx = 0;
    p.idx = idx;
}

// Step the working index back obeying START/END/WRAP/L (upstream p_prev_dec_i).
template<typename Cfg> static void patternPrevDec(TT2PatternT<Cfg> &p) {
    int16_t len = int16_t(p.len), start = p.start, end = p.end, idx = p.idx;
    if (idx == 0 || idx == start) {
        if (p.wrap) idx = (end < len) ? end : int16_t(len - 1);
    } else {
        idx--;
    }
    p.idx = idx;
}

template<typename Cfg>
static void opPWrap(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2PatternT<Cfg> *p = mutablePattern(program, runtime.variables.p_n);
        if (p) p->wrap = (a >= 1) ? 1 : 0;
    } else {
        pushStack(stack, stackSize, int16_t(program->patterns[normalisePn<Cfg>(runtime.variables.p_n)].wrap), error);
    }
}

template<typename Cfg>
static void opPNWrap(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2PatternT<Cfg> *p = mutablePattern(program, pn);
        if (p) p->wrap = (a >= 1) ? 1 : 0;
    } else {
        pushStack(stack, stackSize, int16_t(program->patterns[normalisePn<Cfg>(pn)].wrap), error);
    }
}

template<typename Cfg>
static void opPStart(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2PatternT<Cfg> *p = mutablePattern(program, runtime.variables.p_n);
        if (p) p->start = normaliseIdx(*p, a);
    } else {
        pushStack(stack, stackSize, program->patterns[normalisePn<Cfg>(runtime.variables.p_n)].start, error);
    }
}

template<typename Cfg>
static void opPNStart(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                      int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2PatternT<Cfg> *p = mutablePattern(program, pn);
        if (p) p->start = normaliseIdx(*p, a);
    } else {
        pushStack(stack, stackSize, program->patterns[normalisePn<Cfg>(pn)].start, error);
    }
}

template<typename Cfg>
static void opPEnd(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2PatternT<Cfg> *p = mutablePattern(program, runtime.variables.p_n);
        if (p) p->end = normaliseIdx(*p, a);
    } else {
        pushStack(stack, stackSize, program->patterns[normalisePn<Cfg>(runtime.variables.p_n)].end, error);
    }
}

template<typename Cfg>
static void opPNEnd(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        TT2PatternT<Cfg> *p = mutablePattern(program, pn);
        if (p) p->end = normaliseIdx(*p, a);
    } else {
        pushStack(stack, stackSize, program->patterns[normalisePn<Cfg>(pn)].end, error);
    }
}

template<typename Cfg>
static void opPNext(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    TT2PatternT<Cfg> *p = mutablePattern(program, runtime.variables.p_n);
    if (!p) return;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        patternNextInc(*p);
        p->val[p->idx] = a;
    } else {
        patternNextInc(*p);
        pushStack(stack, stackSize, p->val[p->idx], error);
    }
}

template<typename Cfg>
static void opPNNext(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        patternNextInc(*p);
        p->val[p->idx] = a;
    } else {
        patternNextInc(*p);
        pushStack(stack, stackSize, p->val[p->idx], error);
    }
}

template<typename Cfg>
static void opPPrev(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    TT2PatternT<Cfg> *p = mutablePattern(program, runtime.variables.p_n);
    if (!p) return;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        patternPrevDec(*p);
        p->val[p->idx] = a;
    } else {
        patternPrevDec(*p);
        pushStack(stack, stackSize, p->val[p->idx], error);
    }
}

template<typename Cfg>
static void opPNPrev(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return;
    if (isSet && stackSize >= 1) {
        int16_t a = 0;
        if (!popStack(stack, stackSize, a, error)) return;
        patternPrevDec(*p);
        p->val[p->idx] = a;
    } else {
        patternPrevDec(*p);
        pushStack(stack, stackSize, p->val[p->idx], error);
    }
}

// --- insert / remove / push / pop (get-only edits) -------------------------

template<typename Cfg>
static void patternIns(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t idx, int16_t val) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return;
    idx = normaliseIdx(*p, idx);
    int16_t len = int16_t(p->len);
    if (len >= idx) {
        int16_t hi = len < Cfg::PatternLength ? len : int16_t(Cfg::PatternLength - 1);
        for (int16_t i = hi; i > idx; --i) p->val[i] = p->val[i - 1];
        if (len < Cfg::PatternLength - 1) p->len = uint16_t(len + 1);
    }
    p->val[idx] = val;
}

template<typename Cfg>
static int16_t patternRm(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t idx) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return 0;
    int16_t len = int16_t(p->len);
    if (len <= 0) return 0;
    idx = normaliseIdx(*p, idx);
    int16_t ret = p->val[idx];
    if (idx < len) {
        for (int16_t i = idx; i < len && i + 1 < Cfg::PatternLength; ++i)
            p->val[i] = p->val[i + 1];
        p->len = uint16_t(len - 1);
    }
    return ret;
}

template<typename Cfg>
static void patternPush(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t val) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return;
    int16_t len = int16_t(p->len);
    if (len < Cfg::PatternLength) { p->val[len] = val; p->len = uint16_t(len + 1); }
}

template<typename Cfg>
static int16_t patternPop(const TeletypeProgramT<Cfg> *program, int16_t pn) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return 0;
    int16_t len = int16_t(p->len);
    if (len > 0) { p->len = uint16_t(len - 1); return p->val[len - 1]; }
    return 0;
}

template<typename Cfg>
static void opPIns(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0, val = 0;
    if (!popStack(stack, stackSize, idx, error)) return;
    if (!popStack(stack, stackSize, val, error)) return;
    patternIns(program, runtime.variables.p_n, idx, val);
}

template<typename Cfg>
static void opPNIns(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0, idx = 0, val = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (!popStack(stack, stackSize, idx, error)) return;
    if (!popStack(stack, stackSize, val, error)) return;
    patternIns(program, pn, idx, val);
}

template<typename Cfg>
static void opPRm(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                  int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t idx = 0;
    if (!popStack(stack, stackSize, idx, error)) return;
    pushStack(stack, stackSize, patternRm(program, runtime.variables.p_n, idx), error);
}

template<typename Cfg>
static void opPNRm(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0, idx = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (!popStack(stack, stackSize, idx, error)) return;
    pushStack(stack, stackSize, patternRm(program, pn, idx), error);
}

template<typename Cfg>
static void opPPush(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t val = 0;
    if (!popStack(stack, stackSize, val, error)) return;
    patternPush(program, runtime.variables.p_n, val);
}

template<typename Cfg>
static void opPNPush(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0, val = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (!popStack(stack, stackSize, val, error)) return;
    patternPush(program, pn, val);
}

template<typename Cfg>
static void opPPop(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, patternPop(program, runtime.variables.p_n), error);
}

template<typename Cfg>
static void opPNPop(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    pushStack(stack, stackSize, patternPop(program, pn), error);
}

// --- reorder (REV / ROT / CYC / SHUF) over the [start, end] window ---------

template<typename Cfg> static void patternReverse(TT2PatternT<Cfg> &p, int16_t start, int16_t end) {
    if (end < start) return;
    int16_t mid = int16_t((end - start) / 2);
    for (int16_t i = 0; i <= mid; ++i) {
        int16_t t = p.val[end - i];
        p.val[end - i] = p.val[start + i];
        p.val[start + i] = t;
    }
}

template<typename Cfg> static void patternRotate(TT2PatternT<Cfg> &p, int16_t shift) {
    int16_t start = p.start, end = p.end;
    if (end < start) return;
    int16_t len = int16_t(end - start + 1);
    if (shift < 0) {
        shift = int16_t((-shift) % len);
        if (!shift) return;
        patternReverse(p, start, int16_t(start + shift - 1));
        patternReverse(p, int16_t(start + shift), end);
        patternReverse(p, start, end);
    } else {
        shift = int16_t(shift % len);
        if (!shift) return;
        patternReverse(p, int16_t(end - shift + 1), end);
        patternReverse(p, start, int16_t(end - shift));
        patternReverse(p, start, end);
    }
}

template<typename Cfg> static void patternShuffle(TT2PatternT<Cfg> &p, TT2Rng &rng) {
    int16_t start = p.start, end = p.end;
    if (end < start) return;
    for (int16_t i = end; i > start; --i) {
        int16_t draw = int16_t(tt2RngRange(rng, TT2RngSlot::Pattern, uint32_t(i - start + 1)) + start);
        int16_t t = p.val[draw];
        p.val[draw] = p.val[i];
        p.val[i] = t;
    }
}

template<typename Cfg>
static void opPRev(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *, uint8_t &, bool, TT2EvalError &) {
    TT2PatternT<Cfg> *p = mutablePattern(program, runtime.variables.p_n);
    if (p) patternReverse(*p, p->start, p->end);
}

template<typename Cfg>
static void opPNRev(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (p) patternReverse(*p, p->start, p->end);
}

template<typename Cfg>
static void opPRot(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t shift = 0;
    if (!popStack(stack, stackSize, shift, error)) return;
    TT2PatternT<Cfg> *p = mutablePattern(program, runtime.variables.p_n);
    if (p) patternRotate(*p, shift);
}

template<typename Cfg>
static void opPNRot(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0, shift = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (!popStack(stack, stackSize, shift, error)) return;
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (p) patternRotate(*p, shift);
}

template<typename Cfg>
static void opPShuf(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *, uint8_t &, bool, TT2EvalError &) {
    TT2PatternT<Cfg> *p = mutablePattern(program, runtime.variables.p_n);
    if (p) patternShuffle(*p, runtime.rng);
}

template<typename Cfg>
static void opPNShuf(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (p) patternShuffle(*p, runtime.rng);
}

// --- per-index arithmetic with optional wrap (P.+ / P.+W / P.- / P.-W) -----

static int16_t patternWrapVal(int16_t value, int16_t a, int16_t b) {
    int16_t i = value, c;
    if (a < b) { c = int16_t(b - a + 1); while (i >= b) i -= c; while (i < a) i += c; }
    else { c = int16_t(a - b + 1); while (i >= a) i -= c; while (i < b) i += c; }
    return i;
}

template<typename Cfg>
static void patternArithAt(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t idx,
                           int16_t delta, bool sub, bool doWrap, int16_t lo, int16_t hi) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return;
    idx = normaliseIdx(*p, idx);
    int16_t v = int16_t(sub ? p->val[idx] - delta : p->val[idx] + delta);
    if (doWrap) v = patternWrapVal(v, lo, hi);
    p->val[idx] = v;
}

// Pop the trailing (idx, delta[, lo, hi]) args after the optional leading pn.
template<typename Cfg>
static void patternArithDispatch(const TeletypeProgramT<Cfg> *program, int16_t pn, bool sub,
                                 bool doWrap, int16_t *stack, uint8_t &stackSize,
                                 TT2EvalError &error) {
    int16_t idx = 0, delta = 0, lo = 0, hi = 0;
    if (!popStack(stack, stackSize, idx, error)) return;
    if (!popStack(stack, stackSize, delta, error)) return;
    if (doWrap) {
        if (!popStack(stack, stackSize, lo, error)) return;
        if (!popStack(stack, stackSize, hi, error)) return;
    }
    patternArithAt(program, pn, idx, delta, sub, doWrap, lo, hi);
}

template<typename Cfg>
static void opPAdd(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    patternArithDispatch(program, runtime.variables.p_n, false, false, stack, stackSize, error);
}
template<typename Cfg>
static void opPAddW(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    patternArithDispatch(program, runtime.variables.p_n, false, true, stack, stackSize, error);
}
template<typename Cfg>
static void opPSub(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    patternArithDispatch(program, runtime.variables.p_n, true, false, stack, stackSize, error);
}
template<typename Cfg>
static void opPSubW(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    patternArithDispatch(program, runtime.variables.p_n, true, true, stack, stackSize, error);
}

template<typename Cfg>
static void opPNAdd(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    patternArithDispatch(program, pn, false, false, stack, stackSize, error);
}
template<typename Cfg>
static void opPNAddW(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    patternArithDispatch(program, pn, false, true, stack, stackSize, error);
}
template<typename Cfg>
static void opPNSub(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    patternArithDispatch(program, pn, true, false, stack, stackSize, error);
}
template<typename Cfg>
static void opPNSubW(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                     int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    patternArithDispatch(program, pn, true, true, stack, stackSize, error);
}

// --- whole-window arithmetic (PA/PS/PM/PD/PMOD) and queries ----------------

static int16_t clampI32(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return int16_t(v);
}

// kind: '+' '-' '*' '/' '%' applied to every element in [start,end].
template<typename Cfg>
static void patternPatArith(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t arg, char kind) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p) return;
    int16_t s = p->start, e = p->end;
    if (e < s) return;
    if ((kind == '/' || kind == '%') && arg == 0) return;
    for (int16_t i = s; i <= e; ++i) {
        if (kind == '%') { p->val[i] = int16_t(p->val[i] % arg); continue; }
        int32_t v = p->val[i];
        if (kind == '+') v += arg;
        else if (kind == '-') v -= arg;
        else if (kind == '*') v *= int32_t(arg);
        else v /= arg;
        p->val[i] = clampI32(v);
    }
}

template<typename Cfg> static int16_t patternMinIdx(const TT2PatternT<Cfg> &p) {
    int16_t pos = p.start, val = p.val[p.start];
    for (int16_t i = int16_t(p.start + 1); i <= p.end; ++i)
        if (p.val[i] < val) { val = p.val[i]; pos = i; }
    return pos;
}
template<typename Cfg> static int16_t patternMaxIdx(const TT2PatternT<Cfg> &p) {
    int16_t pos = p.start, val = p.val[p.start];
    for (int16_t i = int16_t(p.start + 1); i <= p.end; ++i)
        if (p.val[i] > val) { val = p.val[i]; pos = i; }
    return pos;
}
template<typename Cfg> static int16_t patternMinVal(const TT2PatternT<Cfg> &p) {
    if (p.end < p.start) return 0;
    int16_t v = p.val[p.start];
    for (int16_t i = int16_t(p.start + 1); i <= p.end; ++i) if (p.val[i] < v) v = p.val[i];
    return v;
}
template<typename Cfg> static int16_t patternMaxVal(const TT2PatternT<Cfg> &p) {
    if (p.end < p.start) return 0;
    int16_t v = p.val[p.start];
    for (int16_t i = int16_t(p.start + 1); i <= p.end; ++i) if (p.val[i] > v) v = p.val[i];
    return v;
}
template<typename Cfg> static int16_t patternSum(const TT2PatternT<Cfg> &p) {
    if (p.end < p.start) return 0;
    int32_t s = 0;
    for (int16_t i = p.start; i <= p.end; ++i) s += p.val[i];
    return clampI32(s);
}
template<typename Cfg> static int16_t patternAvg(const TT2PatternT<Cfg> &p) {
    if (p.end < p.start) return 0;
    int32_t s = 0, c = int32_t(p.end - p.start + 1);
    for (int16_t i = p.start; i <= p.end; ++i) s += p.val[i];
    return clampI32(s / c);
}
template<typename Cfg> static int16_t patternFind(const TT2PatternT<Cfg> &p, int16_t t) {
    if (p.end < p.start) return -1;
    for (int16_t i = p.start; i <= p.end; ++i) if (p.val[i] == t) return i;
    return -1;
}

#define TT2_P_ARITH_OP(NAME, KIND) \
    template<typename Cfg> static void opP##NAME(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program, \
                          int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) { \
        int16_t a = 0; if (!popStack(stack, stackSize, a, error)) return; \
        patternPatArith(program, runtime.variables.p_n, a, KIND); } \
    template<typename Cfg> static void opPN##NAME(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program, \
                           int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) { \
        int16_t pn = 0, a = 0; \
        if (!popStack(stack, stackSize, pn, error)) return; \
        if (!popStack(stack, stackSize, a, error)) return; \
        patternPatArith(program, pn, a, KIND); }

TT2_P_ARITH_OP(PA, '+')
TT2_P_ARITH_OP(PS, '-')
TT2_P_ARITH_OP(PM, '*')
TT2_P_ARITH_OP(PD, '/')
TT2_P_ARITH_OP(PMOD, '%')
#undef TT2_P_ARITH_OP

#define TT2_P_QUERY_OP(NAME, FN) \
    template<typename Cfg> static void opP##NAME(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program, \
                          int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) { \
        pushStack(stack, stackSize, FN(program->patterns[normalisePn<Cfg>(runtime.variables.p_n)]), error); } \
    template<typename Cfg> static void opPN##NAME(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program, \
                           int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) { \
        int16_t pn = 0; if (!popStack(stack, stackSize, pn, error)) return; \
        pushStack(stack, stackSize, FN(program->patterns[normalisePn<Cfg>(pn)]), error); }

TT2_P_QUERY_OP(Min, patternMinIdx)
TT2_P_QUERY_OP(Max, patternMaxIdx)
TT2_P_QUERY_OP(Minv, patternMinVal)
TT2_P_QUERY_OP(Maxv, patternMaxVal)
TT2_P_QUERY_OP(Sum, patternSum)
TT2_P_QUERY_OP(Avg, patternAvg)
#undef TT2_P_QUERY_OP

// FND takes a target arg.
template<typename Cfg>
static void opPFnd(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t t = 0;
    if (!popStack(stack, stackSize, t, error)) return;
    pushStack(stack, stackSize, patternFind(program->patterns[normalisePn<Cfg>(runtime.variables.p_n)], t), error);
}
template<typename Cfg>
static void opPNFnd(TT2RuntimeT<Cfg> &, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0, t = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    if (!popStack(stack, stackSize, t, error)) return;
    pushStack(stack, stackSize, patternFind(program->patterns[normalisePn<Cfg>(pn)], t), error);
}

// --- random (P.RND / RND.P) using the TT2 Pattern rng slot -----------------

template<typename Cfg> static int16_t patternRndVal(const TT2PatternT<Cfg> &p, TT2Rng &rng) {
    if (p.end < p.start) return 0;
    int16_t k = int16_t(tt2RngRange(rng, TT2RngSlot::Pattern, uint32_t(p.end - p.start + 1)) + p.start);
    return p.val[k];
}

template<typename Cfg>
static void patternRndFill(const TeletypeProgramT<Cfg> *program, int16_t pn, int16_t mn,
                           int16_t mx, TT2Rng &rng) {
    TT2PatternT<Cfg> *p = mutablePattern(program, pn);
    if (!p || p->end < p->start) return;
    if (mn > mx) { int16_t t = mn; mn = mx; mx = t; }
    uint32_t range = uint32_t(mx - mn + 1);
    for (int16_t i = p->start; i <= p->end; ++i)
        p->val[i] = int16_t(tt2RngRange(rng, TT2RngSlot::Pattern, range) + mn);
}

template<typename Cfg>
static void opPRnd(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    pushStack(stack, stackSize, patternRndVal(program->patterns[normalisePn<Cfg>(runtime.variables.p_n)], runtime.rng), error);
}

template<typename Cfg>
static void opPNRnd(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    pushStack(stack, stackSize, patternRndVal(program->patterns[normalisePn<Cfg>(pn)], runtime.rng), error);
}

// RND.P [min max] — fill the window with randoms; min/max optional (default 0..16383).
template<typename Cfg>
static void opRndP(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                   int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t mn = 0, mx = 16383;
    if (stackSize >= 2) {
        if (!popStack(stack, stackSize, mn, error)) return;
        if (!popStack(stack, stackSize, mx, error)) return;
    }
    patternRndFill(program, runtime.variables.p_n, mn, mx, runtime.rng);
}

template<typename Cfg>
static void opRndPN(TT2RuntimeT<Cfg> &runtime, TT2OutputState &, const TeletypeProgramT<Cfg> *program,
                    int16_t *stack, uint8_t &stackSize, bool, TT2EvalError &error) {
    int16_t pn = 0;
    if (!popStack(stack, stackSize, pn, error)) return;
    int16_t mn = 0, mx = 16383;
    if (stackSize >= 2) {
        if (!popStack(stack, stackSize, mn, error)) return;
        if (!popStack(stack, stackSize, mx, error)) return;
    }
    patternRndFill(program, pn, mn, mx, runtime.rng);
}

// ---------------------------------------------------------------------------
// Native op table
// ---------------------------------------------------------------------------

namespace {
    template<typename Cfg> struct OpTableBuilderT {
        TT2OpFuncT<Cfg> table[E_OP__LENGTH];
        OpTableBuilderT() {
            for (int i = 0; i < E_OP__LENGTH; ++i) {
                table[i] = nullptr;
            }
            table[E_OP_A]        = opA<Cfg>;
            table[E_OP_B]        = opB<Cfg>;
            table[E_OP_X]        = opX<Cfg>;
            table[E_OP_I]        = opI<Cfg>;
            table[E_OP_ADD]      = opAdd<Cfg>;
            table[E_OP_SYM_PLUS] = opAdd<Cfg>;
            table[E_OP_SUB]      = opSub<Cfg>;
            table[E_OP_SYM_DASH] = opSub<Cfg>;
            table[E_OP_MUL]      = opMul<Cfg>;
            table[E_OP_SYM_STAR] = opMul<Cfg>;
            table[E_OP_DIV]                = opDiv<Cfg>;
            table[E_OP_SYM_FORWARD_SLASH]  = opDiv<Cfg>;
            table[E_OP_MOD]                = opMod<Cfg>;
            table[E_OP_SYM_PERCENTAGE]     = opMod<Cfg>;
            table[E_OP_MIN]      = opMin<Cfg>;
            table[E_OP_MAX]      = opMax<Cfg>;
            table[E_OP_EQ]                   = opEq<Cfg>;
            table[E_OP_SYM_EQUAL_x2]         = opEq<Cfg>;
            table[E_OP_NE]                   = opNe<Cfg>;
            table[E_OP_SYM_EXCLAMATION_EQUAL] = opNe<Cfg>;
            table[E_OP_LT]                   = opLt<Cfg>;
            table[E_OP_SYM_LEFT_ANGLED]      = opLt<Cfg>;
            table[E_OP_GT]                   = opGt<Cfg>;
            table[E_OP_SYM_RIGHT_ANGLED]     = opGt<Cfg>;
            table[E_OP_LTE]                      = opLte<Cfg>;
            table[E_OP_SYM_LEFT_ANGLED_EQUAL]    = opLte<Cfg>;
            table[E_OP_GTE]                      = opGte<Cfg>;
            table[E_OP_SYM_RIGHT_ANGLED_EQUAL]   = opGte<Cfg>;
            table[E_OP_ABS]      = opAbs<Cfg>;
            table[E_OP_SGN]      = opSgn<Cfg>;
            table[E_OP_N]        = opN<Cfg>;
            table[E_OP_V]        = opV<Cfg>;
            table[E_OP_SCALE]    = opScale<Cfg>;
            table[E_OP_SCL]      = opScale<Cfg>;
            table[E_OP_SCALE0]   = opScale0<Cfg>;
            table[E_OP_SCL0]     = opScale0<Cfg>;
            table[E_OP_QT]       = opQt<Cfg>;
            table[E_OP_VN]       = opVn<Cfg>;
            table[E_OP_VV]       = opVv<Cfg>;
            table[E_OP_HZ]       = opHz<Cfg>;
            table[E_OP_P_SCALE]  = opPScale<Cfg>;
            table[E_OP_PN_SCALE] = opPNScale<Cfg>;
            table[E_OP_N_S]      = opNS<Cfg>;
            table[E_OP_N_C]      = opNC<Cfg>;
            table[E_OP_N_CS]     = opNCS<Cfg>;
            table[E_OP_N_B]      = opNB<Cfg>;
            table[E_OP_N_BX]     = opNBX<Cfg>;
            table[E_OP_QT_S]     = opQtS<Cfg>;
            table[E_OP_QT_CS]    = opQtCS<Cfg>;
            table[E_OP_QT_B]     = opQtB<Cfg>;
            table[E_OP_QT_BX]    = opQtBX<Cfg>;
            table[E_OP_BIT_OR]   = opBitOr<Cfg>;
            table[E_OP_BIT_AND]  = opBitAnd<Cfg>;
            table[E_OP_BIT_XOR]  = opBitXor<Cfg>;
            table[E_OP_BIT_NOT]  = opBitNot<Cfg>;
            table[E_OP_XOR]      = opNe<Cfg>;
            table[E_OP_BSET]     = opBset<Cfg>;
            table[E_OP_BGET]     = opBget<Cfg>;
            table[E_OP_BCLR]     = opBclr<Cfg>;
            table[E_OP_BTOG]     = opBtog<Cfg>;
            table[E_OP_BREV]     = opBrev<Cfg>;
            table[E_OP_RSH]      = opRsh<Cfg>;
            table[E_OP_LSH]      = opLsh<Cfg>;
            table[E_OP_SYM_RIGHT_ANGLED_x2] = opRsh<Cfg>;  // >> alias of RSH
            table[E_OP_SYM_LEFT_ANGLED_x2]  = opLsh<Cfg>;  // << alias of LSH
            table[E_OP_RROT]     = opRrot<Cfg>;
            table[E_OP_LROT]     = opLrot<Cfg>;
            table[E_OP_TURTLE]       = opTurtle<Cfg>;
            table[E_OP_TURTLE_X]     = opTurtleX<Cfg>;
            table[E_OP_TURTLE_Y]     = opTurtleY<Cfg>;
            table[E_OP_TURTLE_MOVE]  = opTurtleMove<Cfg>;
            table[E_OP_TURTLE_F]     = opTurtleF<Cfg>;
            table[E_OP_TURTLE_FX1]   = opTurtleFx1<Cfg>;
            table[E_OP_TURTLE_FY1]   = opTurtleFy1<Cfg>;
            table[E_OP_TURTLE_FX2]   = opTurtleFx2<Cfg>;
            table[E_OP_TURTLE_FY2]   = opTurtleFy2<Cfg>;
            table[E_OP_TURTLE_SPEED] = opTurtleSpeed<Cfg>;
            table[E_OP_TURTLE_DIR]   = opTurtleDir<Cfg>;
            table[E_OP_TURTLE_STEP]  = opTurtleStep<Cfg>;
            table[E_OP_TURTLE_BUMP]  = opTurtleBump<Cfg>;
            table[E_OP_TURTLE_WRAP]  = opTurtleWrap<Cfg>;
            table[E_OP_TURTLE_BOUNCE] = opTurtleBounce<Cfg>;
            table[E_OP_TURTLE_SCRIPT] = opTurtleScript<Cfg>;
            table[E_OP_TURTLE_SHOW]  = opTurtleShow<Cfg>;
            table[E_OP_AND]                = opAnd<Cfg>;
            table[E_OP_SYM_AMPERSAND_x2]   = opAnd<Cfg>;
            table[E_OP_OR]                 = opOr<Cfg>;
            table[E_OP_SYM_PIPE_x2]        = opOr<Cfg>;
            table[E_OP_AND3]               = opAnd3<Cfg>;
            table[E_OP_SYM_AMPERSAND_x3]   = opAnd3<Cfg>;
            table[E_OP_OR3]                = opOr3<Cfg>;
            table[E_OP_SYM_PIPE_x3]        = opOr3<Cfg>;
            table[E_OP_AND4]               = opAnd4<Cfg>;
            table[E_OP_SYM_AMPERSAND_x4]   = opAnd4<Cfg>;
            table[E_OP_OR4]                = opOr4<Cfg>;
            table[E_OP_SYM_PIPE_x4]        = opOr4<Cfg>;
            table[E_OP_NZ]       = opNz<Cfg>;
            table[E_OP_EZ]       = opEz<Cfg>;
            table[E_OP_LIM]      = opLim<Cfg>;
            table[E_OP_WRAP]     = opWrap<Cfg>;
            table[E_OP_WRP]      = opWrap<Cfg>;
            table[E_OP_AVG]      = opAvg<Cfg>;
            table[E_OP_INR]      = opInr<Cfg>;
            table[E_OP_OUTR]     = opOutr<Cfg>;
            table[E_OP_INRI]     = opInri<Cfg>;
            table[E_OP_OUTRI]    = opOutri<Cfg>;
            table[E_OP_Q]        = opQ<Cfg>;
            table[E_OP_Q_N]      = opQN<Cfg>;
            table[E_OP_Q_AVG]    = opQAvg<Cfg>;
            table[E_OP_Q_CLR]    = opQClr<Cfg>;
            table[E_OP_Q_GRW]    = opQGrw<Cfg>;
            table[E_OP_Q_SUM]    = opQSum<Cfg>;
            table[E_OP_Q_MIN]    = opQMin<Cfg>;
            table[E_OP_Q_MAX]    = opQMax<Cfg>;
            table[E_OP_Q_SRT]    = opQSrt<Cfg>;
            table[E_OP_Q_REV]    = opQRev<Cfg>;
            table[E_OP_Q_SH]     = opQSh<Cfg>;
            table[E_OP_Q_ADD]    = opQAdd<Cfg>;
            table[E_OP_Q_SUB]    = opQSub<Cfg>;
            table[E_OP_Q_2P]     = opQ2P<Cfg>;
            table[E_OP_Q_P2]     = opQP2<Cfg>;
            table[E_OP_Q_RND]    = opQRnd<Cfg>;
            table[E_OP_Q_MUL]    = opQMul<Cfg>;
            table[E_OP_Q_DIV]    = opQDiv<Cfg>;
            table[E_OP_Q_MOD]    = opQMod<Cfg>;
            table[E_OP_Q_I]      = opQI<Cfg>;
            table[E_OP_S_ALL]    = opSAll<Cfg>;
            table[E_OP_S_POP]    = opSPop<Cfg>;
            table[E_OP_S_CLR]    = opSClr<Cfg>;
            table[E_OP_S_L]      = opSL<Cfg>;
            table[E_OP_P_N]      = opPatternN<Cfg>;
            table[E_OP_P]        = opP<Cfg>;
            table[E_OP_PN]       = opPN<Cfg>;
            table[E_OP_P_L]      = opPL<Cfg>;
            table[E_OP_PN_L]     = opPNL<Cfg>;
            table[E_OP_P_I]      = opPI<Cfg>;
            table[E_OP_PN_I]     = opPNI<Cfg>;
            table[E_OP_P_HERE]   = opPHere<Cfg>;
            table[E_OP_PN_HERE]  = opPNHere<Cfg>;
            table[E_OP_P_WRAP]   = opPWrap<Cfg>;
            table[E_OP_PN_WRAP]  = opPNWrap<Cfg>;
            table[E_OP_P_START]  = opPStart<Cfg>;
            table[E_OP_PN_START] = opPNStart<Cfg>;
            table[E_OP_P_END]    = opPEnd<Cfg>;
            table[E_OP_PN_END]   = opPNEnd<Cfg>;
            table[E_OP_P_NEXT]   = opPNext<Cfg>;
            table[E_OP_PN_NEXT]  = opPNNext<Cfg>;
            table[E_OP_P_PREV]   = opPPrev<Cfg>;
            table[E_OP_PN_PREV]  = opPNPrev<Cfg>;
            table[E_OP_P_INS]    = opPIns<Cfg>;
            table[E_OP_PN_INS]   = opPNIns<Cfg>;
            table[E_OP_P_RM]     = opPRm<Cfg>;
            table[E_OP_PN_RM]    = opPNRm<Cfg>;
            table[E_OP_P_PUSH]   = opPPush<Cfg>;
            table[E_OP_PN_PUSH]  = opPNPush<Cfg>;
            table[E_OP_P_POP]    = opPPop<Cfg>;
            table[E_OP_PN_POP]   = opPNPop<Cfg>;
            table[E_OP_P_REV]    = opPRev<Cfg>;
            table[E_OP_PN_REV]   = opPNRev<Cfg>;
            table[E_OP_P_ROT]    = opPRot<Cfg>;
            table[E_OP_PN_ROT]   = opPNRot<Cfg>;
            table[E_OP_P_SHUF]   = opPShuf<Cfg>;
            table[E_OP_PN_SHUF]  = opPNShuf<Cfg>;
            table[E_OP_P_ADD]    = opPAdd<Cfg>;
            table[E_OP_PN_ADD]   = opPNAdd<Cfg>;
            table[E_OP_P_ADDW]   = opPAddW<Cfg>;
            table[E_OP_PN_ADDW]  = opPNAddW<Cfg>;
            table[E_OP_P_SUB]    = opPSub<Cfg>;
            table[E_OP_PN_SUB]   = opPNSub<Cfg>;
            table[E_OP_P_SUBW]   = opPSubW<Cfg>;
            table[E_OP_PN_SUBW]  = opPNSubW<Cfg>;
            table[E_OP_P_PA]     = opPPA<Cfg>;
            table[E_OP_PN_PA]    = opPNPA<Cfg>;
            table[E_OP_P_PS]     = opPPS<Cfg>;
            table[E_OP_PN_PS]    = opPNPS<Cfg>;
            table[E_OP_P_PM]     = opPPM<Cfg>;
            table[E_OP_PN_PM]    = opPNPM<Cfg>;
            table[E_OP_P_PD]     = opPPD<Cfg>;
            table[E_OP_PN_PD]    = opPNPD<Cfg>;
            table[E_OP_P_PMOD]   = opPPMOD<Cfg>;
            table[E_OP_PN_PMOD]  = opPNPMOD<Cfg>;
            table[E_OP_P_MIN]    = opPMin<Cfg>;
            table[E_OP_PN_MIN]   = opPNMin<Cfg>;
            table[E_OP_P_MAX]    = opPMax<Cfg>;
            table[E_OP_PN_MAX]   = opPNMax<Cfg>;
            table[E_OP_P_MINV]   = opPMinv<Cfg>;
            table[E_OP_PN_MINV]  = opPNMinv<Cfg>;
            table[E_OP_P_MAXV]   = opPMaxv<Cfg>;
            table[E_OP_PN_MAXV]  = opPNMaxv<Cfg>;
            table[E_OP_P_SUM]    = opPSum<Cfg>;
            table[E_OP_PN_SUM]   = opPNSum<Cfg>;
            table[E_OP_P_AVG]    = opPAvg<Cfg>;
            table[E_OP_PN_AVG]   = opPNAvg<Cfg>;
            table[E_OP_P_FND]    = opPFnd<Cfg>;
            table[E_OP_PN_FND]   = opPNFnd<Cfg>;
            table[E_OP_P_RND]    = opPRnd<Cfg>;
            table[E_OP_PN_RND]   = opPNRnd<Cfg>;
            table[E_OP_RND_P]    = opRndP<Cfg>;
            table[E_OP_RND_PN]   = opRndPN<Cfg>;
            table[E_OP_CV]       = opCv<Cfg>;
            table[E_OP_CV_SLEW]  = opCvSlew<Cfg>;
            table[E_OP_CV_OFF]   = opCvOff<Cfg>;
            table[E_OP_TR]       = opTr<Cfg>;
            table[E_OP_TR_POL]   = opTrPol<Cfg>;
            table[E_OP_TR_TIME]  = opTrTime<Cfg>;
            table[E_OP_TR_PULSE] = opTrPulse<Cfg>;
            table[E_OP_TR_P]     = opTrPulse<Cfg>;
            table[E_OP_TR_TOG]   = opTrTog<Cfg>;
            table[E_OP_C]        = opC<Cfg>;
            table[E_OP_D]        = opD<Cfg>;
            table[E_OP_Y]        = opY<Cfg>;
            table[E_OP_Z]        = opZ<Cfg>;
            table[E_OP_T]        = opT<Cfg>;
            table[E_OP_J]        = opJ<Cfg>;
            table[E_OP_K]        = opK<Cfg>;
            table[E_OP_O]        = opO<Cfg>;
            table[E_OP_O_INC]    = opOInc<Cfg>;
            table[E_OP_O_MIN]    = opOMin<Cfg>;
            table[E_OP_O_MAX]    = opOMax<Cfg>;
            table[E_OP_O_WRAP]   = opOWrap<Cfg>;
            table[E_OP_DRUNK]    = opDrunk<Cfg>;
            table[E_OP_DRUNK_MIN]  = opDrunkMin<Cfg>;
            table[E_OP_DRUNK_MAX]  = opDrunkMax<Cfg>;
            table[E_OP_DRUNK_WRAP] = opDrunkWrap<Cfg>;
            table[E_OP_FLIP]     = opFlip<Cfg>;
            table[E_OP_TIME]     = opTime<Cfg>;
            table[E_OP_TIME_ACT] = opTimeAct<Cfg>;
            table[E_OP_LAST]     = opLast<Cfg>;
            table[E_OP_RAND]     = opRand<Cfg>;
            table[E_OP_RRAND]    = opRrand<Cfg>;
            table[E_OP_RND]      = opRand<Cfg>;   // alias of RAND
            table[E_OP_RRND]     = opRrand<Cfg>;  // alias of RRAND
            table[E_OP_EXP]      = opExp<Cfg>;
            table[E_OP_JI]       = opJi<Cfg>;
            table[E_OP_SYM_RIGHT_ANGLED_LEFT_ANGLED]        = opInr<Cfg>;    // ><
            table[E_OP_SYM_LEFT_ANGLED_RIGHT_ANGLED]        = opOutr<Cfg>;   // <>
            table[E_OP_SYM_RIGHT_ANGLED_EQUAL_LEFT_ANGLED]  = opInri<Cfg>;   // >=<
            table[E_OP_SYM_LEFT_ANGLED_EQUAL_RIGHT_ANGLED]  = opOutri<Cfg>;  // <=>
            table[E_OP_SYM_EXCLAMATION]                     = opEz<Cfg>;     // !
            table[E_OP_SYM_LEFT_ANGLED_x3]                  = opLrot<Cfg>;   // <<<
            table[E_OP_SYM_RIGHT_ANGLED_x3]                 = opRrot<Cfg>;   // >>>
            table[E_OP_SYM_AMPERSAND_x3]                    = opAnd3<Cfg>;   // &&&
            table[E_OP_SYM_PIPE_x3]                         = opOr3<Cfg>;    // |||
            table[E_OP_SYM_AMPERSAND_x4]                    = opAnd4<Cfg>;   // &&&&
            table[E_OP_SYM_PIPE_x4]                         = opOr4<Cfg>;    // ||||
            table[E_OP_ER]        = opEr<Cfg>;
            table[E_OP_NR]        = opNr<Cfg>;
            table[E_OP_DR_T]      = opDrT<Cfg>;
            table[E_OP_DR_P]      = opDrP<Cfg>;
            table[E_OP_DR_V]      = opDrV<Cfg>;
            table[E_OP_CHAOS]     = opChaos<Cfg>;
            table[E_OP_CHAOS_R]   = opChaosR<Cfg>;
            table[E_OP_CHAOS_ALG] = opChaosAlg<Cfg>;
            table[E_OP_SEED]        = opSeed<Cfg>;
            table[E_OP_RAND_SEED]   = opRandSeed<Cfg>;
            table[E_OP_SYM_RAND_SD] = opRandSeed<Cfg>;
            table[E_OP_SYM_R_SD]    = opRandSeed<Cfg>;
            table[E_OP_TOSS_SEED]   = opTossSeed<Cfg>;
            table[E_OP_SYM_TOSS_SD] = opTossSeed<Cfg>;
            table[E_OP_PROB_SEED]   = opProbSeed<Cfg>;
            table[E_OP_SYM_PROB_SD] = opProbSeed<Cfg>;
            table[E_OP_DRUNK_SEED]  = opDrunkSeed<Cfg>;
            table[E_OP_SYM_DRUNK_SD] = opDrunkSeed<Cfg>;
            table[E_OP_P_SEED]      = opPSeed<Cfg>;
            table[E_OP_SYM_P_SD]    = opPSeed<Cfg>;
            table[E_OP_INIT]            = opInit<Cfg>;
            table[E_OP_INIT_CV]         = opInitCv<Cfg>;
            table[E_OP_INIT_CV_ALL]     = opInitCvAll<Cfg>;
            table[E_OP_INIT_TR]         = opInitTr<Cfg>;
            table[E_OP_INIT_TR_ALL]     = opInitTrAll<Cfg>;
            table[E_OP_INIT_P]          = opInitP<Cfg>;
            table[E_OP_INIT_P_ALL]      = opInitPAll<Cfg>;
            table[E_OP_INIT_SCRIPT]     = opInitScript<Cfg>;
            table[E_OP_INIT_SCRIPT_ALL] = opInitScriptAll<Cfg>;
            table[E_OP_INIT_DATA]       = opInitData<Cfg>;
            table[E_OP_INIT_TIME]       = opInitTime<Cfg>;
            table[E_OP_TIF]                = opTif<Cfg>;
            table[E_OP_M_SYM_EXCLAMATION]  = opMBang<Cfg>;
            table[E_OP_CV_GET]             = opCvGet<Cfg>;
            table[E_OP_CV_SET]             = opCvSet<Cfg>;
            table[E_OP_M_A]                = opMA<Cfg>;
            table[E_OP_M_ACT_A]            = opMActA<Cfg>;
            table[E_OP_SCRIPT_POL]         = opScriptPol<Cfg>;
            table[E_OP_SYM_DOLLAR_POL]     = opScriptPol<Cfg>;
            table[E_OP_TR_W]               = opTrW<Cfg>;
            table[E_OP_TR_D]               = opTrD<Cfg>;
            table[E_OP_M_RESET]            = opMReset<Cfg>;
            table[E_OP_M_RESET_A]          = opMReset<Cfg>;
            table[E_OP_SYNC]               = opSync<Cfg>;
            table[E_OP_BPM]                = opBpm<Cfg>;
            table[E_OP_WBPM]               = opWbpm<Cfg>;
            table[E_OP_WBPM_S]             = opWbpmS<Cfg>;
            table[E_OP_WMS]                = opWms<Cfg>;
            table[E_OP_WTU]                = opWtu<Cfg>;
            table[E_OP_BAR]                = opBar<Cfg>;
            table[E_OP_WP]                 = opWp<Cfg>;
            table[E_OP_WP_SET]             = opWpSet<Cfg>;
            table[E_OP_WR]                 = opWr<Cfg>;
            table[E_OP_WR_ACT]             = opWrAct<Cfg>;
            table[E_OP_WNG]                = opWng<Cfg>;
            table[E_OP_WPN]                = opWpn<Cfg>;
            table[E_OP_WS]                 = opWs<Cfg>;
            table[E_OP_WNN]                = opWnn<Cfg>;
            table[E_OP_WNG_H]              = opWngH<Cfg>;
            table[E_OP_WNN_H]              = opWnnH<Cfg>;
            table[E_OP_RT]                 = opRt<Cfg>;
            table[E_OP_BUS]                = opBus<Cfg>;
            table[E_OP_TV]                 = opTv<Cfg>;
            // Geode (G.*) — canonical + short aliases share one handler.
            table[E_OP_G_TIME] = opGTime<Cfg>;  table[E_OP_G_T]  = opGTime<Cfg>;
            table[E_OP_G_TONE] = opGTone<Cfg>;  table[E_OP_G_I]  = opGTone<Cfg>;
            table[E_OP_G_RAMP] = opGRamp<Cfg>;  table[E_OP_G_RA] = opGRamp<Cfg>;
            table[E_OP_G_CURV] = opGCurv<Cfg>;  table[E_OP_G_C]  = opGCurv<Cfg>;
            table[E_OP_G_MODE] = opGMode<Cfg>;  table[E_OP_G_M]  = opGMode<Cfg>;
            table[E_OP_G_RUN]  = opGRun<Cfg>;   table[E_OP_G_N]  = opGRun<Cfg>;
            table[E_OP_G_VAL]  = opGVal<Cfg>;   table[E_OP_G_L]  = opGVal<Cfg>;
            table[E_OP_G_TUNE] = opGTune<Cfg>;
            table[E_OP_G_V]    = opGV<Cfg>;
            table[E_OP_G_S]    = opGS<Cfg>;
            // Modulator (MO.*) — canonical + short aliases share one handler.
            table[E_OP_MO]       = opMo<Cfg>;
            table[E_OP_MO_P]     = opMoP<Cfg>;
            table[E_OP_MO_SHAPE] = opMoShape<Cfg>;  table[E_OP_MO_S] = opMoShape<Cfg>;
            table[E_OP_MO_RATE]  = opMoRate<Cfg>;   table[E_OP_MO_R] = opMoRate<Cfg>;
            table[E_OP_MO_DEPTH] = opMoDepth<Cfg>;  table[E_OP_MO_D] = opMoDepth<Cfg>;
            table[E_OP_MO_MODE]  = opMoMode<Cfg>;   table[E_OP_MO_M] = opMoMode<Cfg>;
            table[E_OP_MO_OFF]   = opMoOff<Cfg>;    table[E_OP_MO_O] = opMoOff<Cfg>;
            table[E_OP_MO_TRIG]  = opMoTrig<Cfg>;   table[E_OP_MO_T] = opMoTrig<Cfg>;
            // E.* envelope aliases (ADSR-locked over the modulator slots).
            table[E_OP_E]   = opEnv<Cfg>;
            table[E_OP_E_A] = opEnvA<Cfg>;
            table[E_OP_E_D] = opEnvD<Cfg>;
            table[E_OP_E_O] = opEnvO<Cfg>;
            table[E_OP_E_T] = opEnvT<Cfg>;
            // LFO.* aliases (Sine-locked over the modulator slots). W/F/S deferred.
            table[E_OP_LFO_R] = opLfoR<Cfg>;
            table[E_OP_LFO_C] = opLfoC<Cfg>;
            table[E_OP_LFO_A] = opLfoA<Cfg>;
            table[E_OP_LFO_O] = opLfoO<Cfg>;
            // PRINT/PRT dashboard value store (runtime).
            table[E_OP_PRINT] = opPrint<Cfg>;
            table[E_OP_PRT]   = opPrint<Cfg>;
            // E_OP_G_O / E_OP_G_BAR / E_OP_G_B / E_OP_G_R left nullptr ->
            // UnsupportedOp (no live GeodeConfig field; see Geode ops comment).
            table[E_OP_MI_SYM_DOLLAR]      = opMiDollar<Cfg>;
            table[E_OP_MI_LE]   = opMiLe<Cfg>;
            table[E_OP_MI_LN]   = opMiLn<Cfg>;
            table[E_OP_MI_LNV]  = opMiLnv<Cfg>;
            table[E_OP_MI_LV]   = opMiLv<Cfg>;
            table[E_OP_MI_LVV]  = opMiLvv<Cfg>;
            table[E_OP_MI_LO]   = opMiLo<Cfg>;
            table[E_OP_MI_LC]   = opMiLc<Cfg>;
            table[E_OP_MI_LCC]  = opMiLcc<Cfg>;
            table[E_OP_MI_LCCV] = opMiLccv<Cfg>;
            table[E_OP_MI_LCH]  = opMiLch<Cfg>;
            table[E_OP_MI_NL]   = opMiNl<Cfg>;
            table[E_OP_MI_N]    = opMiN<Cfg>;
            table[E_OP_MI_NV]   = opMiNv<Cfg>;
            table[E_OP_MI_V]    = opMiV<Cfg>;
            table[E_OP_MI_VV]   = opMiVv<Cfg>;
            table[E_OP_MI_NCH]  = opMiNch<Cfg>;
            table[E_OP_MI_OL]   = opMiOl<Cfg>;
            table[E_OP_MI_O]    = opMiO<Cfg>;
            table[E_OP_MI_OCH]  = opMiOch<Cfg>;
            table[E_OP_MI_CL]   = opMiCl<Cfg>;
            table[E_OP_MI_C]    = opMiC<Cfg>;
            table[E_OP_MI_CC]   = opMiCc<Cfg>;
            table[E_OP_MI_CCV]  = opMiCcv<Cfg>;
            table[E_OP_MI_CCH]  = opMiCch<Cfg>;
            table[E_OP_MI_CLKD] = opMiClkd<Cfg>;
            table[E_OP_MI_CLKR] = opMiClkr<Cfg>;
            table[E_OP_R]        = opR<Cfg>;
            table[E_OP_R_MIN]    = opRMin<Cfg>;
            table[E_OP_R_MAX]    = opRMax<Cfg>;
            table[E_OP_TOSS]     = opToss<Cfg>;
            table[E_OP_IN]          = opIn<Cfg>;
            table[E_OP_IN_SCALE]    = opInScale<Cfg>;
            table[E_OP_PARAM]       = opParam<Cfg>;
            table[E_OP_PRM]         = opParam<Cfg>;  // PRM alias of PARAM
            table[E_OP_PARAM_SCALE] = opParamScale<Cfg>;
            table[E_OP_STATE]       = opState<Cfg>;
            table[E_OP_MUTE]        = opMute<Cfg>;
            table[E_OP_M]        = opM<Cfg>;
            table[E_OP_M_C]      = opMC<Cfg>;
            table[E_OP_M_ACT]    = opMAct<Cfg>;
            table[E_OP_SCRIPT]   = opScript<Cfg>;
            table[E_OP_SYM_DOLLAR]    = opScript<Cfg>;
            table[E_OP_SYM_DOLLAR_F]  = opF<Cfg>;
            table[E_OP_SYM_DOLLAR_F1] = opF1<Cfg>;
            table[E_OP_SYM_DOLLAR_F2] = opF2<Cfg>;
            table[E_OP_SYM_DOLLAR_L]  = opL<Cfg>;
            table[E_OP_SYM_DOLLAR_L1] = opL1<Cfg>;
            table[E_OP_SYM_DOLLAR_L2] = opL2<Cfg>;
            table[E_OP_SYM_DOLLAR_S]  = opS<Cfg>;
            table[E_OP_SYM_DOLLAR_S1] = opS1<Cfg>;
            table[E_OP_SYM_DOLLAR_S2] = opS2<Cfg>;
            table[E_OP_I1]       = opI1<Cfg>;
            table[E_OP_I2]       = opI2<Cfg>;
            table[E_OP_FR]       = opFr<Cfg>;
            table[E_OP_DEL_CLR]  = opDelClr<Cfg>;
            table[E_OP_BREAK]    = opBreak<Cfg>;
            table[E_OP_BRK]      = opBreak<Cfg>;
            table[E_OP_KILL]     = opKill<Cfg>;
        }
    };
    CCMRAM_BSS OpTableBuilderT<TT2ConfigFull> opTableBuilderFull;  // CPU-only dispatch table -> CCMRAM (not DMA)
    CCMRAM_BSS OpTableBuilderT<TT2ConfigMini> opTableBuilderMini;  // CPU-only dispatch table -> CCMRAM (not DMA)
}

const TT2OpFunc *tt2NativeOpTable = opTableBuilderFull.table;
const size_t tt2NativeOpCount = E_OP__LENGTH;

template<> const TT2OpFuncT<TT2ConfigFull> *tt2OpTable<TT2ConfigFull>() { return opTableBuilderFull.table; }

const TT2OpFuncT<TT2ConfigMini> *tt2NativeOpTableMini = opTableBuilderMini.table;

template<> const TT2OpFuncT<TT2ConfigMini> *tt2OpTable<TT2ConfigMini>() { return tt2NativeOpTableMini; }
