#ifndef _MATCH_TOKEN_H_
#define _MATCH_TOKEN_H_

#include <stdbool.h>
#include <stdlib.h>

#include "command.h"

bool match_token(const char *token, const size_t len, tele_data_t *out);

#endif
