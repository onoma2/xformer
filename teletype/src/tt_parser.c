#include "tt_parser.h"

#include <string.h>

#include "scanner.h"

// The Ragel scanner is the entire front end; parse() is a thin wrapper.
tele_error_t parse(const char *cmd, tele_command_t *out,
                   char error_msg[TELE_ERROR_MSG_LENGTH]) {
    return scanner(cmd, out, error_msg);
}

const char *tele_error(tele_error_t e) {
    const char *error_string[] = { "OK",
                                   "UNKNOWN WORD",
                                   "COMMAND TOO LONG",
                                   "NOT ENOUGH PARAMS",
                                   "TOO MANY PARAMS",
                                   "MOD NOT ALLOWED HERE",
                                   "EXTRA PRE SEPARATOR",
                                   "NEED PRE SEPARATOR",
                                   "BAD PRE SEPARATOR",
                                   "NO SUB SEP IN PRE",
                                   "MOVE LEFT",
                                   "NEED SPACE AFTER :",
                                   "NEED SPACE AFTER ;" };

    return error_string[e];
}

int16_t rev_bitstring_to_int(const char *token) {
    int8_t length = strlen(token);
    int16_t value = 0;
    for (int8_t i = 0; i < length; i++) {
        if (token[i] == '1') { value += 1 << i; }
    }
    return value;
}
