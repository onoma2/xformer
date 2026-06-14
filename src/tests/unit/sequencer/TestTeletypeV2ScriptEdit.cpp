#include "UnitTest.h"

#include "engine/TT2ScriptLoader.h"   // setScriptCommand / insertScriptCommand / deleteScriptCommand
#include "engine/TT2Printer.h"
#include "model/TeletypeProgram.h"

#include <cstring>
#include <cstdio>

namespace {

// Read line back to text via the native printer.
bool lineText(const TeletypeProgram &p, int s, int line, char *out, size_t cap) {
    return tt2PrintCommand(p.scripts[s].commands[line], out, cap);
}

} // namespace

UNIT_TEST("TeletypeV2ScriptEdit") {

CASE("setScriptCommand stores, grows length, round-trips via printer") {
    TeletypeProgram p = {};
    expectTrue(setScriptCommand(p, 0, 0, "CV 1 5"), "set line 0");
    expectEqual(int(p.scripts[0].length), 1, "length grew to 1");
    char buf[TT2_PRINT_LINE_MAX];
    lineText(p, 0, 0, buf, sizeof buf);
    expectTrue(std::strcmp(buf, "CV 1 5") == 0, "round-trips");

    expectTrue(setScriptCommand(p, 0, 2, "ADD 1 2"), "set line 2 (grows past gap)");
    expectEqual(int(p.scripts[0].length), 3, "length grew to 3");
    lineText(p, 0, 2, buf, sizeof buf);
    expectTrue(std::strcmp(buf, "ADD 1 2") == 0, "line 2 round-trips");
}

CASE("blank text stores a zero-length (blank) line") {
    TeletypeProgram p = {};
    setScriptCommand(p, 0, 0, "CV 1 5");
    expectTrue(setScriptCommand(p, 0, 0, "   "), "blank overwrite");
    expectEqual(int(p.scripts[0].commands[0].length), 0, "command is blank");
}

CASE("invalid text returns false, leaves the line unchanged") {
    TeletypeProgram p = {};
    setScriptCommand(p, 0, 0, "CV 1 5");
    expectFalse(setScriptCommand(p, 0, 0, "CV 1 ###bogus"), "parse error -> false");
    char buf[TT2_PRINT_LINE_MAX];
    lineText(p, 0, 0, buf, sizeof buf);
    expectTrue(std::strcmp(buf, "CV 1 5") == 0, "line unchanged");
}

CASE("insert shifts down and bumps length") {
    TeletypeProgram p = {};
    setScriptCommand(p, 0, 0, "A 1");
    setScriptCommand(p, 0, 1, "B 2");
    expectTrue(insertScriptCommand(p, 0, 0, "C 3"), "insert at 0");
    expectEqual(int(p.scripts[0].length), 3, "length 3");
    char buf[TT2_PRINT_LINE_MAX];
    lineText(p, 0, 0, buf, sizeof buf); expectTrue(std::strcmp(buf, "C 3") == 0, "C at 0");
    lineText(p, 0, 1, buf, sizeof buf); expectTrue(std::strcmp(buf, "A 1") == 0, "A shifted to 1");
    lineText(p, 0, 2, buf, sizeof buf); expectTrue(std::strcmp(buf, "B 2") == 0, "B shifted to 2");
}

CASE("delete shifts up and drops length") {
    TeletypeProgram p = {};
    setScriptCommand(p, 0, 0, "A 1");
    setScriptCommand(p, 0, 1, "B 2");
    setScriptCommand(p, 0, 2, "C 3");
    expectTrue(deleteScriptCommand(p, 0, 1), "delete line 1");
    expectEqual(int(p.scripts[0].length), 2, "length 2");
    char buf[TT2_PRINT_LINE_MAX];
    lineText(p, 0, 0, buf, sizeof buf); expectTrue(std::strcmp(buf, "A 1") == 0, "A stays");
    lineText(p, 0, 1, buf, sizeof buf); expectTrue(std::strcmp(buf, "C 3") == 0, "C shifted up");
}

CASE("insert at the command cap is a safe no-op overflow (drops last)") {
    TeletypeProgram p = {};
    for (int i = 0; i < TT2_COMMANDS_PER_SCRIPT; ++i) {
        char t[8]; std::snprintf(t, sizeof t, "A %d", i);
        setScriptCommand(p, 0, i, t);
    }
    expectEqual(int(p.scripts[0].length), TT2_COMMANDS_PER_SCRIPT, "full");
    expectTrue(insertScriptCommand(p, 0, 0, "Z 9"), "insert into full script");
    expectEqual(int(p.scripts[0].length), TT2_COMMANDS_PER_SCRIPT, "length capped, no overflow");
    char buf[TT2_PRINT_LINE_MAX];
    lineText(p, 0, 0, buf, sizeof buf); expectTrue(std::strcmp(buf, "Z 9") == 0, "new at 0");
}

CASE("out-of-range slot/line rejected") {
    TeletypeProgram p = {};
    expectFalse(setScriptCommand(p, -1, 0, "A 1"), "bad script");
    expectFalse(setScriptCommand(p, 0, TT2_COMMANDS_PER_SCRIPT, "A 1"), "line past cap");
    expectFalse(deleteScriptCommand(p, 0, 0), "delete from empty");
}

}
