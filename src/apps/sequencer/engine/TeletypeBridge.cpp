#include "TeletypeBridge.h"

#include "TeletypeTrackEngine.h"

#include "core/Debug.h"
#include "os/os.h"

#include <array>
#include <cstring>

namespace {
TeletypeTrackEngine *g_activeEngine = nullptr;
std::array<int16_t, 16> g_dashboardValues{};
} // namespace

TeletypeBridge::ScopedEngine::ScopedEngine(TeletypeTrackEngine &engine) {
    _prev = g_activeEngine;
    g_activeEngine = &engine;
}

TeletypeBridge::ScopedEngine::~ScopedEngine() {
    g_activeEngine = _prev;
}

TeletypeTrackEngine *TeletypeBridge::activeEngine() {
    return g_activeEngine;
}

void TeletypeBridge::setActiveEngine(TeletypeTrackEngine *engine) {
    g_activeEngine = engine;
}

uint32_t TeletypeBridge::ticksMs() {
    return os::ticks() / os::time::ms(1);
}

extern "C" {

uint32_t tele_get_ticks(void) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        return engine->timeTicks();
    }
    return TeletypeBridge::ticksMs();
}

void tele_metro_updated(void) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->syncMetroFromState();
    }
}

void tele_metro_reset(void) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->resetMetroTimer();
    }
}

void tele_tr(uint8_t i, int16_t v) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->handleTr(i, v);
    }
}

void tele_tr_pulse(uint8_t i, int16_t time) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->beginPulse(i, time);
    }
}

void tele_tr_pulse_clear(uint8_t i) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->clearPulse(i);
    }
}

void tele_tr_pulse_time(uint8_t i, int16_t time) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setPulseTime(i, time);
    }
}

void tele_cv(uint8_t i, int16_t v, uint8_t s) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->handleCv(i, v, s != 0);
    }
}

void tele_cv_slew(uint8_t i, int16_t v) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setCvSlew(i, v);
    }
}

uint16_t tele_get_cv(uint8_t i) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        return engine->cvRaw(i);
    }
    return 0;
}

void tele_cv_cal(uint8_t n, int32_t b, int32_t m) {
    (void)n;
    (void)b;
    (void)m;
}

void tele_update_adc(uint8_t force) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->updateAdc(force != 0);
    }
}

void tele_has_delays(bool has_delays) {
    (void)has_delays;
}

void tele_has_stack(bool has_stack) {
    (void)has_stack;
}

void tele_cv_off(uint8_t i, int16_t v) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setCvOffset(i, v);
    }
}

void tele_ii_tx(uint8_t addr, uint8_t *data, uint8_t l) {
    (void)addr;
    (void)data;
    (void)l;
}

void tele_ii_rx(uint8_t addr, uint8_t *data, uint8_t l) {
    (void)addr;
    if (data && l) {
        std::memset(data, 0, l);
    }
}

void tele_scene(uint8_t i, uint8_t init_grid, uint8_t init_pattern) {
    (void)i;
    (void)init_grid;
    (void)init_pattern;
}

void tele_pattern_updated(void) {}

void tele_vars_updated(void) {}

void tele_kill(void) {}

void tele_mute(void) {}

bool tele_get_input_state(uint8_t i) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        return engine->inputState(i);
    }
    return false;
}

void tele_save_calibration(void) {}

void grid_key_press(uint8_t x, uint8_t y, uint8_t z) {
    (void)x;
    (void)y;
    (void)z;
}

void device_flip(void) {}

void set_live_submode(uint8_t submode) {
    (void)submode;
}

void select_dash_screen(uint8_t screen) {
    (void)screen;
}

void print_dashboard_value(uint8_t index, int16_t value) {
    if (index < g_dashboardValues.size()) {
        g_dashboardValues[index] = value;
    }
    DBG("TT PRINT %d=%d", index + 1, value);
}

int16_t get_dashboard_value(uint8_t index) {
    if (index < g_dashboardValues.size()) {
        return g_dashboardValues[index];
    }
    return 0;
}

void reset_midi_counter(void) {}

} // extern "C"
