#include "UnitTest.h"

#include "engine/TT2Printer.h"
#include "model/TeletypeProgram.h"

extern "C" {
#include "command.h"
#include "teletype.h"
#include "ops/op_enum.h"
}

#include <cstring>

namespace {

bool toCmd(const char *text, TT2Command &cmd) {
    tele_command_t src = {};
    char err[TELE_ERROR_MSG_LENGTH] = {};
    if (parse(text, &src, err) != E_OK) return false;
    return lowerCommand(src, cmd);
}

bool sameCmd(const TT2Command &a, const TT2Command &b) {
    if (a.length != b.length) return false;
    for (int i = 0; i < a.length; ++i) {
        if (a.tag[i] != b.tag[i] || a.value[i] != b.value[i]) return false;
    }
    return true;
}

// text -> cmd -> print -> cmd2 ; pass iff cmd == cmd2 (semantic idempotence)
bool roundtrip(const char *text) {
    TT2Command c1;
    if (!toCmd(text, c1)) return false;
    char buf[TT2_PRINT_LINE_MAX];
    if (!tt2PrintCommand(c1, buf, sizeof buf)) return false;
    TT2Command c2;
    if (!toCmd(buf, c2)) return false;
    return sameCmd(c1, c2);
}

} // namespace

UNIT_TEST("TT2Printer") {

CASE("per-tag formatting + spacing") {
    TT2Command c;
    char buf[TT2_PRINT_LINE_MAX];
    toCmd("MO.SHAPE 3 5", c); tt2PrintCommand(c, buf, sizeof buf);
    expectTrue(std::strcmp(buf, "MO.SHAPE 3 5") == 0, "op + numbers");
    toCmd("EQ X 1", c); tt2PrintCommand(c, buf, sizeof buf);
    expectTrue(std::strcmp(buf, "EQ X 1") == 0, "spacing between tokens");
}

CASE("number bit-pattern round-trips (uint16 pattern, signed storage)") {
    expectTrue(roundtrip("XFFFF"), "hex max");
    expectTrue(roundtrip("X0"), "hex zero");
    expectTrue(roundtrip("B1111111111111111"), "binary max");
    expectTrue(roundtrip("B0"), "binary zero");
    expectTrue(roundtrip("R1"), "reverse-binary");
    expectTrue(roundtrip("ADD -1 1"), "negative decimal");
    expectTrue(roundtrip("ADD -32768 1"), "int16 min");
}

CASE("idempotence corpus") {
    const char *corpus[] = {
        "ADD 1 2", "EQ X 1", "IF EQ X 1: TR.P 1", "MUL Y 3",
        "MO.SHAPE 3 5", "MO.P 3 2 100", "G.TIME 4000", "W.ACT",
        "XFF", "B101", "TR.P 1",
    };
    for (auto s : corpus) expectTrue(roundtrip(s), s);
}

CASE("multi-statement (sub-separator) round-trips") {
    expectTrue(roundtrip("A 1; B 2"), "two statements with ;");
}

CASE("alias canonicalization: WR.ACT prints canonical, re-parses to same enum") {
    TT2Command c;
    char buf[TT2_PRINT_LINE_MAX];
    expectTrue(toCmd("WR.ACT", c), "parse WR.ACT");
    expectTrue(tt2PrintCommand(c, buf, sizeof buf), "print");
    TT2Command c2;
    expectTrue(toCmd(buf, c2), "re-parse printed spelling");
    expectTrue(c.tag[0] == c2.tag[0] && c.value[0] == c2.value[0], "same op enum");
}

CASE("command truncation returns false, no overrun") {
    TT2Command c; toCmd("ADD 1 2", c);
    char small[4];
    std::memset(small, 0x7f, sizeof small);
    expectFalse(tt2PrintCommand(c, small, sizeof small), "too small -> false");
    expectTrue(std::strlen(small) < sizeof small, "NUL-terminated within bounds");
    char ok[TT2_PRINT_LINE_MAX];
    expectTrue(tt2PrintCommand(c, ok, sizeof ok), "ample buffer -> true");
}

CASE("script line model: length lines, newline-joined, index==line") {
    TT2Script s = {};
    toCmd("ADD 1 2", s.commands[0]);
    toCmd("MUL 3 4", s.commands[1]);
    s.length = 2;
    char buf[TT2_PRINT_SCRIPT_MAX];
    expectTrue(tt2PrintScript(s, buf, sizeof buf), "fits");
    expectTrue(std::strcmp(buf, "ADD 1 2\nMUL 3 4") == 0, "two lines joined by \\n");

    TT2Script empty = {}; empty.length = 0;
    expectTrue(tt2PrintScript(empty, buf, sizeof buf), "empty ok");
    expectTrue(buf[0] == '\0', "empty script -> empty output");
}

CASE("command length overflow is rejected (no OOB read)") {
    TT2Command c = {};
    toCmd("ADD 1 2", c);
    c.length = 99;   // malformed: past the fixed 16-slot arrays
    char buf[TT2_PRINT_LINE_MAX];
    expectFalse(tt2PrintCommand(c, buf, sizeof buf), "over-length command rejected");
    expectTrue(buf[0] == '\0', "output empty/terminated on reject");

    // and via tt2PrintScript, which calls tt2PrintCommand per line
    TT2Script s = {};
    s.commands[0] = c;
    s.length = 1;
    expectFalse(tt2PrintScript(s, buf, sizeof buf), "over-length line rejected in script");
}

CASE("script truncation stops at the last whole line") {
    TT2Script s = {};
    toCmd("ADD 1 2", s.commands[0]);
    toCmd("MUL 3 4", s.commands[1]);
    s.length = 2;
    char small[10];  // holds "ADD 1 2\0" but not the 2nd line
    expectFalse(tt2PrintScript(s, small, sizeof small), "truncates");
    expectTrue(std::strcmp(small, "ADD 1 2") == 0, "last whole line, no partial");
}

}
