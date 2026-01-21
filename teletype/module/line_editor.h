#ifndef _LINE_EDITOR_H_
#define _LINE_EDITOR_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "region.h"
#include "teletype.h"

#define LINE_EDITOR_SIZE 32  // 31 characters + null termination

typedef struct {
    char buffer[LINE_EDITOR_SIZE];
    size_t cursor;
    size_t length;
} line_editor_t;

void line_editor_set(line_editor_t *le, const char value[LINE_EDITOR_SIZE]);
void line_editor_set_command(line_editor_t *le, const tele_command_t *command);
char *line_editor_get(line_editor_t *le);
bool line_editor_process_keys(line_editor_t *le, uint8_t key, uint8_t mod_key,
                              bool is_key_held);
void line_editor_draw(line_editor_t *le, char prefix, region *reg);
#endif
