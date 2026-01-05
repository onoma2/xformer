#include "TeletypeBridge.h"

#include "TeletypeTrackEngine.h"

#include "core/Debug.h"
#include "core/math/Math.h"
#include "os/os.h"

#include <array>
#include <cmath>
#include <cstring>

namespace {
TeletypeTrackEngine *g_activeEngine = nullptr;
std::array<int16_t, 16> g_dashboardValues{};
bool g_hasDelays = false;
bool g_hasStack = false;

float busRawToVolts(int16_t value) {
    int16_t clamped = clamp<int16_t>(value, 0, 16383);
    float norm = clamped / 16383.f;
    return norm * 10.f - 5.f;
}

int16_t busVoltsToRaw(float volts) {
    float clamped = clamp(volts, -5.f, 5.f);
    float norm = (clamped + 5.f) / 10.f;
    int32_t raw = static_cast<int32_t>(std::lround(norm * 16383.f));
    return static_cast<int16_t>(clamp<int32_t>(raw, 0, 16383));
}
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

bool TeletypeBridge::hasDelays() {
    return g_hasDelays;
}

bool TeletypeBridge::hasStack() {
    return g_hasStack;
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

void tele_metro_all_set(int16_t m) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setMetroAllPeriod(m);
    }
}

void tele_metro_all_act(int16_t state) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setMetroAllActive(state > 0);
    }
}

void tele_metro_all_reset(void) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->resetMetroAll();
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

bool tele_tr_pulse_allow(uint8_t i) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        return engine->allowPulse(i);
    }
    return false;
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

void tele_tr_div(uint8_t i, int16_t div) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setTrDiv(i, div);
    }
}

void tele_tr_width(uint8_t i, int16_t pct) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setTrWidth(i, pct);
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

void tele_env_target(uint8_t i, int16_t value) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setEnvTarget(i, value);
    }
}

void tele_env_attack(uint8_t i, int16_t ms) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setEnvAttack(i, ms);
    }
}

void tele_env_decay(uint8_t i, int16_t ms) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setEnvDecay(i, ms);
    }
}

void tele_env_trigger(uint8_t i) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->triggerEnv(i);
    }
}

void tele_env_offset(uint8_t i, int16_t value) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setEnvOffset(i, value);
    }
}

void tele_env_loop(uint8_t i, int16_t count) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setEnvLoop(i, count);
    }
}

void tele_env_eor(uint8_t i, int16_t tr) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setEnvEor(i, tr);
    }
}

void tele_env_eoc(uint8_t i, int16_t tr) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setEnvEoc(i, tr);
    }
}

uint16_t tele_bus_cv_get(uint8_t i) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        return busVoltsToRaw(engine->busCv(i));
    }
    return 0;
}

void tele_bus_cv_set(uint8_t i, int16_t v) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setBusCv(i, busRawToVolts(v));
    }
}

int16_t tele_wbpm_get(void) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        return static_cast<int16_t>(std::lround(engine->tempo()));
    }
    return 0;
}

void tele_wbpm_set(int16_t bpm) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        int32_t clamped = clamp<int32_t>(bpm, 1, 1000);
        engine->setTempo(static_cast<float>(clamped));
    }
}

int16_t tele_bar(uint8_t bars) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        float fraction = engine->measureFractionBars(bars);
        return static_cast<int16_t>(fraction * 16383.0f);
    }
    return 0;
}

int16_t tele_wpat(uint8_t trackIndex) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        return static_cast<int16_t>(engine->trackPattern(trackIndex));
    }
    return 0;
}

void tele_wpat_set(uint8_t trackIndex, uint8_t patternIndex) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setTrackPattern(trackIndex, patternIndex);
    }
}

int16_t tele_wr(void) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        // Return 1 when transport is running (flip to invert behavior).
        return engine->isTransportRunning() ? 1 : 0;
    }
    return 0;
}

void tele_wr_act(int16_t state) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->setTransportRunning(state);
    }
}

int16_t tele_rt(uint8_t routeIndex) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        float normalized = engine->routingSource(routeIndex);
        int32_t raw = static_cast<int32_t>(std::lround(normalized * 16383.f));
        return static_cast<int16_t>(clamp<int32_t>(raw, 0, 16383));
    }
    return 0;
}

bool tele_timebase_is_clock(void) {
    if (auto *engine = TeletypeBridge::activeEngine()) {
        return engine->timeBase() == TeletypeTrack::TimeBase::Clock;
    }
    return false;
}

void tele_clock_mode_notice(void) {
    static uint32_t lastNoticeMs = 0;
    uint32_t nowMs = TeletypeBridge::ticksMs();
    if (nowMs - lastNoticeMs < 1000) {
        return;
    }
    lastNoticeMs = nowMs;
    if (auto *engine = TeletypeBridge::activeEngine()) {
        engine->showMessage("Clock Mode");
    }
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
    g_hasDelays = has_delays;
}

void tele_has_stack(bool has_stack) {
    g_hasStack = has_stack;
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
