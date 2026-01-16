#ifndef _TELETYPE_IO_H_
#define _TELETYPE_IO_H_

#include <stdbool.h>
#include <stdint.h>

#define SUB_MODE_OFF 0
#define SUB_MODE_VARS 1
#define SUB_MODE_GRID 2
#define SUB_MODE_FULLGRID 3
#define SUB_MODE_DASH 4

// These functions are for interacting with the teletype hardware, each target
// must provide it's own implementation

// used for TIME and LAST
extern uint32_t tele_get_ticks(void);

// called when M or M.ACT are updated
extern void tele_metro_updated(void);
extern void tele_metro_all_set(int16_t m);
extern void tele_metro_all_act(int16_t state);
extern void tele_metro_all_reset(void);

// called by M.RESET
extern void tele_metro_reset(void);

extern void tele_tr(uint8_t i, int16_t v);
extern void tele_tr_pulse(uint8_t i, int16_t time);
extern bool tele_tr_pulse_allow(uint8_t i);
extern void tele_tr_pulse_clear(uint8_t i);
extern void tele_tr_pulse_time(uint8_t i, int16_t time);
extern void tele_tr_div(uint8_t i, int16_t div);
extern void tele_tr_width(uint8_t i, int16_t pct);
extern void tele_cv(uint8_t i, int16_t v, uint8_t s);
extern void tele_cv_slew(uint8_t i, int16_t v);
extern void tele_cv_interpolate(uint8_t i, int16_t enabled);
extern uint16_t tele_get_cv(uint8_t i);
extern void tele_cv_cal(uint8_t n, int32_t b, int32_t m);
extern uint16_t tele_bus_cv_get(uint8_t i);
extern void tele_bus_cv_set(uint8_t i, int16_t v);
extern int16_t tele_wbpm_get(void);
extern void tele_wbpm_set(int16_t bpm);
extern int16_t tele_wms(uint8_t mult);
extern int16_t tele_wtu(uint8_t div, uint8_t mult);
extern int16_t tele_bar(uint8_t bars);
extern int16_t tele_wpat(uint8_t trackIndex);
extern void tele_wpat_set(uint8_t trackIndex, uint8_t patternIndex);
extern int16_t tele_wr(void);
extern void tele_wr_act(int16_t state);
extern int16_t tele_wng(uint8_t trackIndex, uint8_t stepIndex);
extern void tele_wng_set(uint8_t trackIndex, uint8_t stepIndex, int16_t value);
extern int16_t tele_wnn(uint8_t trackIndex, uint8_t stepIndex);
extern void tele_wnn_set(uint8_t trackIndex, uint8_t stepIndex, int16_t value);
extern int16_t tele_wng_here(uint8_t trackIndex);
extern int16_t tele_wnn_here(uint8_t trackIndex);
extern int16_t tele_rt(uint8_t routeIndex);
extern bool tele_timebase_is_clock(void);
extern void tele_clock_mode_notice(void);

extern void tele_env_target(uint8_t i, int16_t value);
extern void tele_env_attack(uint8_t i, int16_t ms);
extern void tele_env_decay(uint8_t i, int16_t ms);
extern void tele_env_trigger(uint8_t i);
extern void tele_env_offset(uint8_t i, int16_t value);
extern void tele_env_loop(uint8_t i, int16_t count);
extern void tele_env_eor(uint8_t i, int16_t tr);
extern void tele_env_eoc(uint8_t i, int16_t tr);
extern void tele_lfo_rate(uint8_t i, int16_t ms);
extern void tele_lfo_wave(uint8_t i, int16_t value);
extern void tele_lfo_amp(uint8_t i, int16_t value);
extern void tele_lfo_fold(uint8_t i, int16_t value);
extern void tele_lfo_offset(uint8_t i, int16_t value);
extern void tele_lfo_start(uint8_t i, int16_t state);
extern void tele_g_time(int16_t value);
extern void tele_g_intone(int16_t value);
extern void tele_g_ramp(int16_t value);
extern void tele_g_curve(int16_t value);
extern void tele_g_run(int16_t value);
extern void tele_g_mode(int16_t value);
extern void tele_g_offset(int16_t value);
extern void tele_g_tune(uint8_t voiceIndex, int16_t numerator, int16_t denominator);
extern void tele_g_out(uint8_t cvIndex, int16_t voiceIndex);
extern void tele_g_vox(uint8_t voiceIndex, int16_t divs, int16_t repeats);
extern int16_t tele_g_get_time(void);
extern int16_t tele_g_get_intone(void);
extern int16_t tele_g_get_ramp(void);
extern int16_t tele_g_get_curve(void);
extern int16_t tele_g_get_run(void);
extern int16_t tele_g_get_mode(void);
extern int16_t tele_g_get_offset(void);
extern int16_t tele_g_get_val(void);
extern int16_t tele_g_get_voice(uint8_t voiceIndex);
extern int16_t tele_g_get_tune_num(uint8_t voiceIndex);
extern int16_t tele_g_get_tune_den(uint8_t voiceIndex);

extern void tele_update_adc(uint8_t force);

// inform target if there are delays
extern void tele_has_delays(bool has_delays);

// inform target if the stack has entries
extern void tele_has_stack(bool has_stack);

extern void tele_cv_off(uint8_t i, int16_t v);
extern void tele_ii_tx(uint8_t addr, uint8_t *data, uint8_t l);
extern void tele_ii_rx(uint8_t addr, uint8_t *data, uint8_t l);
extern void tele_scene(uint8_t i, uint8_t init_grid, uint8_t init_pattern);

// called when a pattern is updated
extern void tele_pattern_updated(void);

extern void tele_vars_updated(void);

extern void tele_kill(void);
extern void tele_mute(void);
extern bool tele_get_input_state(uint8_t);

void tele_save_calibration(void);

#ifdef TELETYPE_PROFILE
void tele_profile_script(size_t);
void tele_profile_delay(uint8_t);
#endif

// emulate grid key press
extern void grid_key_press(uint8_t x, uint8_t y, uint8_t z);

// manage device config
extern void device_flip(void);

// live screen / dashboard
extern void set_live_submode(uint8_t submode);
extern void select_dash_screen(uint8_t screen);
extern void print_dashboard_value(uint8_t index, int16_t value);
extern int16_t get_dashboard_value(uint8_t index);

extern void reset_midi_counter(void);

#endif
