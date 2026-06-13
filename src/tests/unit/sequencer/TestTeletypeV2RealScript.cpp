#include "UnitTest.h"

#include "engine/TT2ScriptLoader.h"
#include "engine/TT2Runner.h"

namespace {

struct Fixture {
    TeletypeProgram program;
    TT2Runtime runtime;
    TT2OutputState output;
    Fixture() {
        init(program);
        init(runtime);
        init(output);
    }
    TT2EvalResult run(int scriptIndex, const char *text) {
        int n = loadScriptText(program, scriptIndex, text);
        expectTrue(n >= 0, "script text loads");
        return runScript(program, runtime, output, uint8_t(scriptIndex));
    }
};

} // namespace

UNIT_TEST("TeletypeV2RealScript") {

CASE("cv_and_tr_from_real_text") {
    Fixture f;
    f.run(0, "CV 1 5000\nTR 1 1\n");
    expectEqual(int(f.output.cv[0].targetRaw), 5000, "CV 1 sets cv[0] raw");
    expectTrue((f.output.cvDirty & 1) != 0, "cv[0] dirty");
    expectTrue(f.output.tr[0].level != 0, "TR 1 sets tr[0] high");
}

CASE("variable_then_output") {
    Fixture f;
    f.run(0, "A 5\nCV 1 A\n");
    expectEqual(int(f.output.cv[0].targetRaw), 5, "CV 1 A writes A's value");
}

CASE("math_in_real_script") {
    Fixture f;
    f.run(0, "CV 1 ADD 100 23\n");
    expectEqual(int(f.output.cv[0].targetRaw), 123, "ADD 100 23 = 123");
}

CASE("multi_output_lines") {
    Fixture f;
    f.run(0, "CV 1 100\nCV 2 200\nTR 2 1\n");
    expectEqual(int(f.output.cv[0].targetRaw), 100, "cv[0] = 100");
    expectEqual(int(f.output.cv[1].targetRaw), 200, "cv[1] = 200");
    expectTrue(f.output.tr[1].level != 0, "tr[1] high");
}

CASE("blank_lines_skipped") {
    Fixture f;
    int n = loadScriptText(f.program, 0, "\nCV 1 7\n\n");
    expectEqual(n, 1, "one real command, blanks skipped");
    runScript(f.program, f.runtime, f.output, 0);
    expectEqual(int(f.output.cv[0].targetRaw), 7, "cv[0] = 7");
}

CASE("parse_error_reports") {
    Fixture f;
    int n = loadScriptText(f.program, 0, "CV 1 ZZZNOTANOP\n");
    expectEqual(n, -1, "bad token -> parse error");
}

} // UNIT_TEST("TeletypeV2RealScript")
