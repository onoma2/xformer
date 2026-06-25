#pragma once

// Loads multi-line TT2 source text into a TeletypeProgram via the Ragel
// frontend: each non-blank line is parse()'d -> lowerCommand()'d -> stored.
// Shared by tests now and the editor/loader later.

#include "model/TeletypeProgram.h"

extern "C" {
#include "tt_parser.h"
}

#include <cstdint>

// Returns the number of commands stored (>= 0), or -1 on a parse/lower error.
// Blank lines are skipped; stops at TT2_COMMANDS_PER_SCRIPT lines.
template<typename Cfg>
inline int loadScriptText(TeletypeProgramT<Cfg> &program, int scriptIndex, const char *text) {
    if (scriptIndex < 0 || scriptIndex >= Cfg::ScriptCount || !text) {
        return -1;
    }
    TT2Script &script = program.scripts[scriptIndex];
    script.length = 0;
    int line = 0;
    const char *p = text;
    char lineBuf[128];
    while (*p && line < TT2_COMMANDS_PER_SCRIPT) {
        int n = 0;
        while (*p && *p != '\n' && n < int(sizeof(lineBuf)) - 1) {
            lineBuf[n++] = *p++;
        }
        lineBuf[n] = '\0';
        if (*p == '\n') ++p;
        bool blank = true;
        for (int i = 0; i < n; ++i) {
            if (lineBuf[i] != ' ' && lineBuf[i] != '\t') { blank = false; break; }
        }
        if (blank) continue;
        tele_command_t cmd = {};
        char err[TELE_ERROR_MSG_LENGTH];
        if (parse(lineBuf, &cmd, err) != E_OK) {
            return -1;
        }
        if (!lowerCommand(cmd, script.commands[line])) {
            return -1;
        }
        ++line;
    }
    script.length = uint8_t(line);
    return line;
}

// --- Per-line edit primitives for the editor (parse -> lower -> store) ---------
// A blank/whitespace line lowers to a zero-length command (the runner skips it,
// the printer renders it empty). Parse failures leave the program unchanged.

namespace tt2detail {
inline bool isBlank(const char *text) {
    if (!text) return true;
    for (const char *q = text; *q; ++q) {
        if (*q != ' ' && *q != '\t') return false;
    }
    return true;
}
// Parse+lower into `dst`; blank text -> zero-length command. False on parse error.
inline bool parseLine(const char *text, TT2Command &dst) {
    dst = TT2Command{};
    if (tt2detail::isBlank(text)) return true;
    tele_command_t cmd = {};
    char err[TELE_ERROR_MSG_LENGTH];
    if (parse(text, &cmd, err) != E_OK) return false;
    return lowerCommand(cmd, dst);
}
} // namespace tt2detail

// Overwrite script `s` line `line` with `text`. Grows length to include `line`.
// Returns false (program unchanged) on bad indices or parse error.
template<typename Cfg>
inline bool setScriptCommand(TeletypeProgramT<Cfg> &program, int s, int line, const char *text) {
    if (s < 0 || s >= Cfg::ScriptCount) return false;
    if (line < 0 || line >= TT2_COMMANDS_PER_SCRIPT) return false;
    TT2Command tmp;
    if (!tt2detail::parseLine(text, tmp)) return false;
    TT2Script &script = program.scripts[s];
    script.commands[line] = tmp;
    if (line >= script.length) script.length = uint8_t(line + 1);
    return true;
}

// Insert `text` at `line`, shifting lines down; if the script is full the last
// line is dropped. `line` must be within [0, length]. False on error (unchanged).
template<typename Cfg>
inline bool insertScriptCommand(TeletypeProgramT<Cfg> &program, int s, int line, const char *text) {
    if (s < 0 || s >= Cfg::ScriptCount) return false;
    TT2Script &script = program.scripts[s];
    if (line < 0 || line > script.length || line >= TT2_COMMANDS_PER_SCRIPT) return false;
    TT2Command tmp;
    if (!tt2detail::parseLine(text, tmp)) return false;  // parse before mutating
    int top = (script.length < TT2_COMMANDS_PER_SCRIPT) ? script.length
                                                        : TT2_COMMANDS_PER_SCRIPT - 1;
    for (int i = top; i > line; --i) script.commands[i] = script.commands[i - 1];
    script.commands[line] = tmp;
    if (script.length < TT2_COMMANDS_PER_SCRIPT) script.length++;
    return true;
}

// Delete `line`, shifting lines up; drops length. False if line out of range.
template<typename Cfg>
inline bool deleteScriptCommand(TeletypeProgramT<Cfg> &program, int s, int line) {
    if (s < 0 || s >= Cfg::ScriptCount) return false;
    TT2Script &script = program.scripts[s];
    if (line < 0 || line >= script.length) return false;
    for (int i = line; i + 1 < script.length; ++i) script.commands[i] = script.commands[i + 1];
    script.length--;
    script.commands[script.length] = TT2Command{};
    return true;
}
