#ifndef _TELETYPE_H_
#define _TELETYPE_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "command.h"
#include "state.h"
#include "tt_parser.h"

// #define TELETYPE_PROFILE // un-comment this line to enable profiling

typedef struct {
    bool has_value;
    int16_t value;
} process_result_t;


tele_error_t validate(const tele_command_t *c,
                 char error_msg[TELE_ERROR_MSG_LENGTH]);
process_result_t run_script(scene_state_t *ss, size_t script_no);
process_result_t run_script_with_exec_state(scene_state_t *ss, exec_state_t *es,
                                            size_t script_no);
process_result_t run_line_with_exec_state(scene_state_t *ss, exec_state_t *es,
                                          size_t script_no, uint8_t line_no);
process_result_t run_fscript_with_exec_state(scene_state_t *ss,
                                             exec_state_t *es,
                                             size_t script_no);
process_result_t run_fline_with_exec_state(scene_state_t *ss, exec_state_t *es,
                                           size_t script_no, uint8_t line_no);
process_result_t process_command(scene_state_t *ss, exec_state_t *es,
                                 const tele_command_t *cmd);

void tele_tick(scene_state_t *ss, uint8_t);

void clear_delays(scene_state_t *ss);

void tele_tr_pulse_end(scene_state_t *ss, uint8_t i);

#endif
