#include "UnitTest.h"

#include "engine/TT2Evaluator.h"

namespace {

// -- fake op table for skeleton testing ----------------------------------

enum class FakeOp : int16_t {
    Add = 0,
    Push42 = 1,
    VarSet = 2,
    VarGet = 3,
};

void fakeAdd(TT2Runtime &, TT2OutputState &, int16_t *stack,
             uint8_t &stackSize, bool, TT2EvalError &error) {
    if (stackSize < 2) {
        error = TT2EvalError::StackUnderflow;
        return;
    }
    int16_t b = stack[--stackSize];
    int16_t a = stack[--stackSize];
    stack[stackSize++] = a + b;
}

void fakePush42(TT2Runtime &, TT2OutputState &, int16_t *stack,
                uint8_t &stackSize, bool, TT2EvalError &) {
    stack[stackSize++] = 42;
}

void fakeVarSet(TT2Runtime &runtime, TT2OutputState &, int16_t *stack,
                uint8_t &stackSize, bool isSet, TT2EvalError &error) {
    if (!isSet || stackSize < 1) {
        error = TT2EvalError::StackUnderflow;
        return;
    }
    runtime.variables.a = stack[--stackSize];
}

void fakeVarGet(TT2Runtime &runtime, TT2OutputState &, int16_t *stack,
                uint8_t &stackSize, bool, TT2EvalError &) {
    stack[stackSize++] = runtime.variables.a;
}

const TT2OpFunc fakeOpTable[] = {
    fakeAdd,
    fakePush42,
    fakeVarSet,
    fakeVarGet,
};

constexpr size_t fakeOpCount = sizeof(fakeOpTable) / sizeof(fakeOpTable[0]);

// -- helpers -------------------------------------------------------------

TT2Command makeNumberCommand(int16_t value, uint8_t tag = uint8_t(NUMBER)) {
    TT2Command cmd = {};
    cmd.length = 1;
    cmd.tag[0] = tag;
    cmd.value[0] = value;
    return cmd;
}

TT2Command makeOpCommand(int16_t opValue) {
    TT2Command cmd = {};
    cmd.length = 1;
    cmd.tag[0] = uint8_t(OP);
    cmd.value[0] = opValue;
    return cmd;
}

TT2Command makeTwoNumberOp(int16_t a, int16_t b, int16_t opValue) {
    TT2Command cmd = {};
    cmd.length = 3;
    cmd.tag[0] = uint8_t(OP);
    cmd.value[0] = opValue;
    cmd.tag[1] = uint8_t(NUMBER);
    cmd.value[1] = a;
    cmd.tag[2] = uint8_t(NUMBER);
    cmd.value[2] = b;
    return cmd;
}

// Wrapper that calls evaluateSegment with a fake op table.
// Used for skeleton tests that do not touch output.
TT2EvalResult evaluateWithFakeOps(const TT2Command &cmd, TT2Runtime &runtime,
                                  const TT2OpFunc *table, size_t count) {
    TT2OutputState dummy = {};
    init(dummy);
    return evaluateSegment(cmd, 0, cmd.length, runtime, dummy, table, count);
}

} // namespace

UNIT_TEST("TeletypeV2Evaluator") {

    CASE("number_push_decimal") {
        TT2Runtime runtime = {};
        auto cmd = makeNumberCommand(1);
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::None), "no error");
        expectEqual(int(result.stackSize), 1, "stack size");
        expectEqual(result.value, int16_t(1), "pushed value");
    }

    CASE("number_push_hex") {
        TT2Runtime runtime = {};
        auto cmd = makeNumberCommand(255, uint8_t(XNUMBER));
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::None), "no error");
        expectEqual(result.value, int16_t(255), "XFF = 255");
    }

    CASE("number_push_binary") {
        TT2Runtime runtime = {};
        auto cmd = makeNumberCommand(10, uint8_t(BNUMBER));
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::None), "no error");
        expectEqual(result.value, int16_t(10), "B1010 = 10");
    }

    CASE("number_push_reversed_binary") {
        TT2Runtime runtime = {};
        auto cmd = makeNumberCommand(3, uint8_t(RNUMBER));
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::None), "no error");
        expectEqual(result.value, int16_t(3), "R1100 = 3");
    }

    CASE("fake_add_right_to_left_order") {
        TT2Runtime runtime = {};
        auto cmd = makeTwoNumberOp(5, 3, int16_t(FakeOp::Add));
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::None), "no error");
        expectEqual(int(result.stackSize), 1, "stack size");
        expectEqual(result.value, int16_t(8), "5 + 3 = 8");
    }

    CASE("fake_push_constant_op") {
        TT2Runtime runtime = {};
        auto cmd = makeOpCommand(int16_t(FakeOp::Push42));
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::None), "no error");
        expectEqual(int(result.stackSize), 1, "stack size");
        expectEqual(result.value, int16_t(42), "push 42");
    }

    CASE("fake_var_set_and_get") {
        TT2Runtime runtime = {};
        init(runtime);

        // Set: FAKE_VAR_SET 99
        {
            TT2Command setCmd = {};
            setCmd.length = 2;
            setCmd.tag[0] = uint8_t(OP);
            setCmd.value[0] = int16_t(FakeOp::VarSet);
            setCmd.tag[1] = uint8_t(NUMBER);
            setCmd.value[1] = 99;
            auto r = evaluateWithFakeOps(setCmd, runtime, fakeOpTable, fakeOpCount);
            expectEqual(int(r.error), int(TT2EvalError::None), "set no error");
            expectEqual(int(r.stackSize), 0, "set consumes value");
        }

        // Get: FAKE_VAR_GET
        {
            auto getCmd = makeOpCommand(int16_t(FakeOp::VarGet));
            auto r = evaluateWithFakeOps(getCmd, runtime, fakeOpTable, fakeOpCount);
            expectEqual(int(r.error), int(TT2EvalError::None), "get no error");
            expectEqual(int(r.stackSize), 1, "get pushes value");
            expectEqual(r.value, int16_t(99), "read back 99");
        }
    }

    CASE("stack_underflow") {
        TT2Runtime runtime = {};
        auto cmd = makeOpCommand(int16_t(FakeOp::Add));
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::StackUnderflow),
                    "stack underflow detected");
    }

    CASE("unknown_op_rejected") {
        TT2Runtime runtime = {};
        auto cmd = makeOpCommand(999);
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::UnknownOp),
                    "unknown op rejected");
    }

    CASE("empty_command") {
        TT2Runtime runtime = {};
        TT2Command cmd = {};
        cmd.length = 0;
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::None), "empty ok");
        expectEqual(int(result.stackSize), 0, "empty stack");
    }

    CASE("multiple_pushes_leave_stack") {
        TT2Runtime runtime = {};
        TT2Command cmd = {};
        cmd.length = 2;
        cmd.tag[0] = uint8_t(OP);
        cmd.value[0] = int16_t(FakeOp::Push42);
        cmd.tag[1] = uint8_t(OP);
        cmd.value[1] = int16_t(FakeOp::Push42);
        auto result = evaluateWithFakeOps(cmd, runtime, fakeOpTable, fakeOpCount);
        expectEqual(int(result.error), int(TT2EvalError::None), "no error");
        expectEqual(int(result.stackSize), 2, "two values");
        expectEqual(result.value, int16_t(42), "top is 42");
    }
}
