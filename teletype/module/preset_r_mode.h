#ifndef _PRESET_R_MODE_H_
#define _PRESET_R_MODE_H_

#include <stdbool.h>
#include <stdint.h>

uint8_t get_preset(void);
void set_preset_r_mode(uint8_t preset);
void process_preset_r_preset(uint8_t preset);
void preset_line_down(void);
void preset_line_up(void);
void process_preset_r_load(void);
void process_preset_r_keys(uint8_t key, uint8_t mod_key, bool is_held_key);
uint8_t screen_refresh_preset_r(void);

#endif
