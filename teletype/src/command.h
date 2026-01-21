#ifndef _COMMAND_H_
#define _COMMAND_H_

#include <stdbool.h>
#include <stdint.h>

#define COMMAND_MAX_LENGTH 16

typedef enum {
    NUMBER,
    XNUMBER,
    BNUMBER,
    RNUMBER,
    OP,
    MOD,
    PRE_SEP,
    SUB_SEP
} tele_word_t;

typedef struct {
    tele_word_t tag;
    int16_t value;
} tele_data_t;

typedef struct {
    uint8_t length;
    int8_t separator;
    // Store tags separately to keep tele_command_t compact in RAM.
    uint8_t tag[COMMAND_MAX_LENGTH];
    int16_t value[COMMAND_MAX_LENGTH];
    bool comment;
} tele_command_t;

static inline tele_word_t cmd_tag(const tele_command_t *cmd, uint8_t idx) {
    return (tele_word_t)cmd->tag[idx];
}

static inline int16_t cmd_value(const tele_command_t *cmd, uint8_t idx) {
    return cmd->value[idx];
}

static inline void cmd_set(tele_command_t *cmd, uint8_t idx, tele_word_t tag,
                           int16_t value) {
    cmd->tag[idx] = (uint8_t)tag;
    cmd->value[idx] = value;
}

void copy_command(tele_command_t *dst, const tele_command_t *src);
void copy_post_command(tele_command_t *dst, const tele_command_t *src);
void print_command(const tele_command_t *c, char *out);

#endif
