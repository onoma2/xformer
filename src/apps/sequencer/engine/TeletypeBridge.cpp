// teletype_io.h host-hook stubs.
//
// The vendored teletype C library (op.c / teletype.c) references these hooks at
// link time. TT2 (the native reimplementation) dispatches through
// TeletypeNativeOps and never executes the vendored C VM, so none of these are
// invoked at runtime — they exist only to satisfy the linker. The legacy TT1
// bridge that used to route them to a live TeletypeTrackEngine was removed.

#include <cstdint>

extern "C" {
#include "teletype_io.h"

uint32_t tele_get_ticks(void) { return 0; }

void tele_metro_updated(void) {}
void tele_metro_all_set(int16_t) {}
void tele_metro_all_act(int16_t) {}
void tele_metro_all_reset(void) {}
void tele_metro_reset(void) {}

void tele_tr(uint8_t, int16_t) {}
void tele_tr_pulse(uint8_t, int16_t) {}
bool tele_tr_pulse_allow(uint8_t) { return true; }
void tele_tr_pulse_clear(uint8_t) {}
void tele_tr_pulse_time(uint8_t, int16_t) {}
void tele_tr_div(uint8_t, int16_t) {}
void tele_tr_width(uint8_t, int16_t) {}

void tele_cv(uint8_t, int16_t, uint8_t) {}
void tele_cv_slew(uint8_t, int16_t) {}
void tele_cv_interpolate(uint8_t, int16_t) {}
uint16_t tele_get_cv(uint8_t) { return 0; }
void tele_cv_cal(uint8_t, int32_t, int32_t) {}
void tele_cv_off(uint8_t, int16_t) {}

uint16_t tele_bus_cv_get(uint8_t) { return 0; }
void tele_bus_cv_set(uint8_t, int16_t) {}

int16_t tele_wbpm_get(void) { return 0; }
void tele_wbpm_set(int16_t) {}
int16_t tele_wms(uint8_t) { return 0; }
int16_t tele_wtu(uint8_t, uint8_t) { return 0; }
int16_t tele_bar(uint8_t) { return 0; }
int16_t tele_wpat(uint8_t) { return 0; }
void tele_wpat_set(uint8_t, uint8_t) {}
int16_t tele_wr(void) { return 0; }
void tele_wr_act(int16_t) {}
int16_t tele_wng(uint8_t, uint8_t) { return 0; }
void tele_wng_set(uint8_t, uint8_t, int16_t) {}
int16_t tele_wnn(uint8_t, uint8_t) { return 0; }
void tele_wnn_set(uint8_t, uint8_t, int16_t) {}
int16_t tele_wng_here(uint8_t) { return 0; }
int16_t tele_wnn_here(uint8_t) { return 0; }
int16_t tele_rt(uint8_t) { return 0; }

bool tele_timebase_is_clock(void) { return false; }
void tele_clock_mode_notice(void) {}

void tele_env_target(uint8_t, int16_t) {}
void tele_env_attack(uint8_t, int16_t) {}
void tele_env_decay(uint8_t, int16_t) {}
void tele_env_trigger(uint8_t) {}
void tele_env_offset(uint8_t, int16_t) {}
void tele_env_loop(uint8_t, int16_t) {}
void tele_env_eor(uint8_t, int16_t) {}
void tele_env_eoc(uint8_t, int16_t) {}

void tele_lfo_rate(uint8_t, int16_t) {}
void tele_lfo_wave(uint8_t, int16_t) {}
void tele_lfo_amp(uint8_t, int16_t) {}
void tele_lfo_fold(uint8_t, int16_t) {}
void tele_lfo_offset(uint8_t, int16_t) {}
void tele_lfo_start(uint8_t, int16_t) {}

void tele_g_time(int16_t) {}
void tele_g_intone(int16_t) {}
void tele_g_ramp(int16_t) {}
void tele_g_curve(int16_t) {}
void tele_g_run(int16_t) {}
void tele_g_mode(int16_t) {}
void tele_g_offset(int16_t) {}
void tele_g_bar(int16_t) {}
void tele_g_tune(uint8_t, int16_t, int16_t) {}
void tele_g_out(uint8_t, int16_t) {}
void tele_g_vox(uint8_t, int16_t, int16_t) {}
int16_t tele_g_get_time(void) { return 0; }
int16_t tele_g_get_intone(void) { return 0; }
int16_t tele_g_get_ramp(void) { return 0; }
int16_t tele_g_get_curve(void) { return 0; }
int16_t tele_g_get_run(void) { return 0; }
int16_t tele_g_get_mode(void) { return 0; }
int16_t tele_g_get_offset(void) { return 0; }
int16_t tele_g_get_bar(void) { return 0; }
int16_t tele_g_get_val(void) { return 0; }
int16_t tele_g_get_voice(uint8_t) { return 0; }
int16_t tele_g_get_tune_num(uint8_t) { return 0; }
int16_t tele_g_get_tune_den(uint8_t) { return 0; }

void tele_update_adc(uint8_t) {}
void tele_has_delays(bool) {}
void tele_has_stack(bool) {}
void tele_ii_tx(uint8_t, uint8_t *, uint8_t) {}
void tele_ii_rx(uint8_t, uint8_t *, uint8_t) {}
void tele_scene(uint8_t, uint8_t, uint8_t) {}
void tele_pattern_updated(void) {}
void tele_vars_updated(void) {}
void tele_kill(void) {}
void tele_mute(void) {}
bool tele_get_input_state(uint8_t) { return false; }

void grid_key_press(uint8_t, uint8_t, uint8_t) {}
void device_flip(void) {}
void set_live_submode(uint8_t) {}
void select_dash_screen(uint8_t) {}
void print_dashboard_value(uint8_t, int16_t) {}
int16_t get_dashboard_value(uint8_t) { return 0; }
void reset_midi_counter(void) {}

} // extern "C"
