#pragma once

// Loads multi-line TT2 source text into a TeletypeProgram via the Ragel
// frontend: each non-blank line is parse()'d -> lowerCommand()'d -> stored.
// Shared by tests now and the editor/loader later.

// Pre-include model/Types.h in C++ mode so that when teletype.h pulls in
// state.h -> types.h inside extern "C", the C++ templates are already
// processed and skipped by #pragma once (mirrors the TT2 test pattern).
#include "model/Types.h"
#include "model/TeletypeProgram.h"

extern "C" {
#include "command.h"
#include "teletype.h"
}

#include <cstdint>

// Returns the number of commands stored (>= 0), or -1 on a parse/lower error.
// Blank lines are skipped; stops at TT2_COMMANDS_PER_SCRIPT lines.
inline int loadScriptText(TeletypeProgram &program, int scriptIndex, const char *text) {
    if (scriptIndex < 0 || scriptIndex >= TT2_SCRIPT_COUNT || !text) {
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
