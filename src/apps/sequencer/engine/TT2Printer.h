#pragma once

#include "model/TeletypeProgram.h"   // TT2Command, TT2Script, TT2_COMMANDS_PER_SCRIPT

#include <cstddef>

// Native TT2 ops->text printer (no tele_ops[]/print_command). Reconstructs source
// text from lowered TT2Command tokens; the reverse of the kept Ragel parse path.
// Semantic (command/script-level) round-trip, not byte-exact: comments are not
// stored and whitespace is normalized.

// Worst case: 16 tokens (TT2_COMMAND_MAX_LENGTH), widest token a 16-bit binary
// literal ("B" + 16 = 17 chars) + 15 separating spaces ~= 287. Round up.
static constexpr size_t TT2_PRINT_LINE_MAX = 320;
static constexpr size_t TT2_PRINT_SCRIPT_MAX =
    TT2_COMMANDS_PER_SCRIPT * TT2_PRINT_LINE_MAX + TT2_COMMANDS_PER_SCRIPT;

// Reconstruct one command into out[0..cap). Always NUL-terminates. Returns false
// (stopping, NUL-terminated within bounds) if the result would exceed cap.
bool tt2PrintCommand(const TT2Command &cmd, char *out, size_t cap);

// Print script commands 0..length-1, '\n'-separated (line index == command index).
// Returns false on overflow, stopping at the last whole line that fit (no partial
// line). Empty script (length 0) -> empty output, returns true.
bool tt2PrintScript(const TT2Script &script, char *out, size_t cap);
