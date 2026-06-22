#ifndef _TT_PARSER_H_
#define _TT_PARSER_H_

#include <stdbool.h>
#include <stdint.h>

#include "command.h"

// Parser-only API: the Ragel tokenizer plus its error vocabulary, with no
// dependency on the teletype VM/state/op-table headers. This is the whole
// surface TT2 keeps from the vendored C library.

#define TELE_ERROR_MSG_LENGTH 16

typedef enum {
    E_OK,
    E_PARSE,
    E_LENGTH,
    E_NEED_PARAMS,
    E_EXTRA_PARAMS,
    E_NO_MOD_HERE,
    E_MANY_PRE_SEP,
    E_NEED_PRE_SEP,
    E_PLACE_PRE_SEP,
    E_NO_SUB_SEP_IN_PRE,
    E_NOT_LEFT,
    E_NEED_SPACE_PRE_SEP,
    E_NEED_SPACE_SUB_SEP
} tele_error_t;

tele_error_t parse(const char *cmd, tele_command_t *out,
                   char error_msg[TELE_ERROR_MSG_LENGTH]);
const char *tele_error(tele_error_t e);
int16_t rev_bitstring_to_int(const char *token);

#endif
