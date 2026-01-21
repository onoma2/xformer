#ifndef _SCANNER_H_
#define _SCANNER_H_

#include <stdlib.h>

#include "teletype.h"

tele_error_t scanner(const char *cmd, tele_command_t *out,
                char error_msg[TELE_ERROR_MSG_LENGTH]);

#endif
