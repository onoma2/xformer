#ifndef _LIVE_MODE_H_
#define _LIVE_MODE_H_

#include "grid.h"
#include "state.h"
#include "stdbool.h"
#include "stdint.h"

void set_slew_icon(bool display);
void set_metro_icon(bool display);
void init_live_mode(void);
void set_live_mode(void);
void set_grid_updated(void);
void set_vars_updated(void);
void set_dash_updated(void);
void set_mutes_updated(void);
void history_next(void);
void history_prev(void);
void execute_line(void);
void process_live_keys(uint8_t key, uint8_t mod_key, bool is_held_key,
                       bool is_release, scene_state_t *ss);
uint8_t screen_refresh_live(void);
uint8_t get_live_sub_mode(void);
extern uint8_t grid_page;
extern uint8_t grid_show_controls;

#endif
