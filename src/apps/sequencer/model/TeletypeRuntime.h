#pragma once

#include "TeletypeProgram.h"

#include <cassert>
#include <cstdint>
#include <cstring>

static constexpr int TT2_STACK_DEPTH      = 16;
static constexpr int TT2_DELAY_DEPTH      = 8;
static constexpr int TT2_RNG_COUNT        = 5;
static constexpr int TT2_VARIABLE_COUNT   = 20;
static constexpr int TT2_CV_COUNT         = 8;
static constexpr int TT2_TR_COUNT         = 8;
static constexpr int TT2_Q_LENGTH         = 64;
static constexpr int TT2_EXEC_DEPTH       = 8;
static constexpr int TT2_NB_SCALES        = 16;
static constexpr int TT2_TRIGGER_INPUTS   = 8;
static constexpr int TT2_PRINT_SLOT_COUNT = 16;  // PRINT/PRT dashboard slots

struct TT2RuntimeCommand {
    uint8_t length;
    uint8_t tag[TT2_COMMAND_MAX_LENGTH];
    int16_t value[TT2_COMMAND_MAX_LENGTH];
};

struct TT2Variables {
    int16_t a, x, b, y, c, z, d, t;
    int16_t j[TT2_SCRIPT_COUNT];
    int16_t k[TT2_SCRIPT_COUNT];
    int16_t cv[TT2_CV_COUNT];
    int16_t cv_off[TT2_CV_COUNT];
    int16_t cv_slew[TT2_CV_COUNT];
    int16_t dashboard[TT2_PRINT_SLOT_COUNT];  // PRINT/PRT value store (ephemeral)
    int16_t drunk;
    int16_t drunk_max;
    int16_t drunk_min;
    int16_t drunk_wrap;
    int16_t flip;
    int16_t in;
    int16_t m;
    uint8_t m_act;
    uint8_t mutes;
    int16_t o;
    int16_t o_inc;
    int16_t o_min;
    int16_t o_max;
    int16_t o_wrap;
    int16_t p_n;
    int16_t param;
    int16_t q[TT2_Q_LENGTH];
    int16_t q_n;
    int16_t q_grow;
    int16_t r_min;
    int16_t r_max;
    int16_t n_scale_bits[TT2_NB_SCALES];
    int16_t n_scale_root[TT2_NB_SCALES];
    uint8_t script_pol[TT2_TRIGGER_INPUTS];
    int64_t time;
    uint8_t time_act;
    int16_t tr[TT2_TR_COUNT];
    int16_t tr_pol[TT2_TR_COUNT];
    int16_t tr_time[TT2_TR_COUNT];
    int16_t tr_width[TT2_TR_COUNT];   // pulse width %, 0 = use tr_time
    int16_t tr_div[TT2_TR_COUNT];     // pulse divisor (fire every Nth)
    uint8_t tr_div_count[TT2_TR_COUNT];
    int16_t seed;
    int16_t in_min;
    int16_t in_max;
    int16_t param_min;
    int16_t param_max;
};

struct TT2Stack {
    TT2RuntimeCommand commands[TT2_STACK_DEPTH];
    uint8_t top;
};

struct TT2DelayEntry {
    TT2RuntimeCommand command;
    int16_t time;
    uint8_t originScript;
    int16_t originI;
    int16_t originFParam1;
    int16_t originFParam2;
};

struct TT2DelayQueue {
    TT2DelayEntry entries[TT2_DELAY_DEPTH];
    uint8_t count;
};

struct TT2EveryCount {
    int16_t count;
    int16_t mod;
    uint8_t skip;
};

struct TT2EveryState {
    TT2EveryCount every[TT2_SCRIPT_COUNT][TT2_COMMANDS_PER_SCRIPT];
};

struct TT2Metro {
    int16_t interval;
    uint32_t lastTick;
    uint8_t running;
};

enum class TT2RngSlot : uint8_t {
    Rand,
    Prob,
    Toss,
    Drunk,
    Pattern,
};

struct TT2Rng {
    uint32_t state[TT2_RNG_COUNT];
};

// Advance the LCG in the given slot and return the next 32-bit value.
// Each slot maintains independent state. State 0 is a valid seed.
inline uint32_t tt2RngNext(TT2Rng &rng, TT2RngSlot slot) {
    uint32_t &s = rng.state[static_cast<uint8_t>(slot)];
    s = s * 1664525u + 1013904223u;
    return s;
}

// Return a value in [0, range) using the slot's LCG.
// Guards range == 0.
inline uint32_t tt2RngRange(TT2Rng &rng, TT2RngSlot slot, uint32_t range) {
    if (range == 0) return 0;
    return tt2RngNext(rng, slot) % range;
}

enum class TT2TurtleMode : uint8_t {
    Wrap,
    Bump,
    Bounce,
};

struct TT2TurtlePosition {
    int32_t x;
    int32_t y;
};

struct TT2TurtleFence {
    uint8_t x1;
    uint8_t y1;
    uint8_t x2;
    uint8_t y2;
};

struct TT2Turtle {
    TT2TurtlePosition position;
    TT2TurtlePosition last;
    TT2TurtleFence fence;
    TT2TurtleMode mode;
    uint16_t heading;
    int16_t speed;
    uint8_t scriptNumber;
    uint8_t stepping;
    uint8_t stepped;
    uint8_t shown;
};

struct TT2ExecFrame {
    uint8_t if_else_condition;
    int16_t i;
    uint8_t while_continue;
    uint16_t while_depth;
    uint8_t breaking;
    uint8_t script_number;
    uint8_t line_number;
    uint8_t delayed;
    int16_t fparam1;
    int16_t fparam2;
    int16_t fresult;
    uint8_t fresult_set;
};

struct TT2ExecState {
    TT2ExecFrame frames[TT2_EXEC_DEPTH];
    uint8_t depth;
    uint8_t overflow;
};

static constexpr int TT2_MIDI_EVENTS = 10;

// Native MIDI event buffer (mirrors upstream scene_midi_t). Populated by
// TT2TrackEngine::processMidiMessage, read by the MI.* ops. Per-event fire-script
// selectors (on/off/cc, -1 = disabled) live here like upstream scene_midi.
struct TT2Midi {
    uint8_t last_event_type;  // 0 off, 1 on, 2 cc
    uint8_t last_channel;
    uint8_t last_note;
    uint8_t last_velocity;
    uint8_t last_controller;
    uint8_t last_cc;
    uint8_t on_count;
    uint8_t on_channel[TT2_MIDI_EVENTS];
    uint8_t note_on[TT2_MIDI_EVENTS];
    uint8_t note_vel[TT2_MIDI_EVENTS];
    uint8_t off_count;
    uint8_t note_off[TT2_MIDI_EVENTS];
    uint8_t off_channel[TT2_MIDI_EVENTS];
    uint8_t cc_count;
    uint8_t cc_channel[TT2_MIDI_EVENTS];
    uint8_t cn[TT2_MIDI_EVENTS];
    uint8_t cc[TT2_MIDI_EVENTS];
    uint8_t clock_div;
    int8_t on_script;
    int8_t off_script;
    int8_t cc_script;
};

// Clear event counts + last-event fields for a fresh message (upstream
// clearMidiMonitoring). Leaves fire-script selectors / clock_div intact.
inline void tt2MidiClear(TT2Midi &m) {
    m.last_event_type = 0;
    m.last_channel = m.last_note = m.last_velocity = m.last_controller = m.last_cc = 0;
    m.on_count = m.off_count = m.cc_count = 0;
}
inline void tt2MidiNoteOn(TT2Midi &m, uint8_t ch, uint8_t note, uint8_t vel) {
    if (m.on_count < TT2_MIDI_EVENTS) {
        m.note_on[m.on_count] = note; m.note_vel[m.on_count] = vel; m.on_channel[m.on_count] = ch;
        m.on_count++;
    }
    m.last_event_type = 1; m.last_channel = ch; m.last_note = note; m.last_velocity = vel;
}
inline void tt2MidiNoteOff(TT2Midi &m, uint8_t ch, uint8_t note) {
    if (m.off_count < TT2_MIDI_EVENTS) {
        m.note_off[m.off_count] = note; m.off_channel[m.off_count] = ch;
        m.off_count++;
    }
    m.last_event_type = 0; m.last_channel = ch; m.last_note = note; m.last_velocity = 0;
}
inline void tt2MidiCc(TT2Midi &m, uint8_t ch, uint8_t controller, uint8_t value) {
    if (m.cc_count < TT2_MIDI_EVENTS) {
        m.cn[m.cc_count] = controller; m.cc[m.cc_count] = value; m.cc_channel[m.cc_count] = ch;
        m.cc_count++;
    }
    m.last_event_type = 2; m.last_channel = ch; m.last_controller = controller; m.last_cc = value;
}

struct TT2Runtime {
    TT2Variables variables;
    TT2Stack stack;
    TT2DelayQueue delay;
    TT2EveryState every;
    TT2Metro metro;
    TT2Rng rng;
    TT2ExecState exec;
    TT2Turtle turtle;
    uint8_t inputLevel[TT2_TRIGGER_INPUT_COUNT];  // latched trigger levels for STATE
    uint32_t clockMs;                             // engine ms clock for TIME / LAST
    uint32_t scriptLastMs[TT2_SCRIPT_COUNT];      // clockMs at each script's last run
    uint8_t metroResetReq;                        // M.RESET -> engine zeroes the metro phase
    TT2Midi midi;                                 // MIDI event buffer for MI.* ops
};

// Variable defaults (shared by init() and the INIT.DATA op). Zeroes the
// variables block first, then applies the kept Teletype core defaults.
inline void tt2InitVariables(TT2Variables &v) {
    memset(&v, 0, sizeof(TT2Variables));
    v.a = 1;
    v.b = 2;
    v.c = 3;
    v.d = 4;
    for (int i = 0; i < TT2_CV_COUNT; ++i) {
        v.cv_slew[i] = 1;
    }
    v.drunk_min = 0;
    v.drunk_max = 255;
    v.m = 1000;
    v.m_act = 1;
    v.o_inc = 1;
    v.o_min = 0;
    v.o_max = 63;
    v.o_wrap = 1;
    v.q_n = 1;
    v.q_grow = 0;
    v.r_min = 0;
    v.r_max = 16383;
    for (int i = 0; i < TT2_TRIGGER_INPUTS; ++i) {
        v.script_pol[i] = 1;
    }
    v.time_act = 1;
    for (int i = 0; i < TT2_TR_COUNT; ++i) {
        v.tr_pol[i] = 1;
        v.tr_time[i] = 100;
        v.tr_width[i] = 0;
        v.tr_div[i] = 1;
        v.tr_div_count[i] = 0;
    }
    v.in_min = 0;
    v.in_max = 16383;
    v.param_min = 0;
    v.param_max = 16383;
    for (int i = 0; i < TT2_NB_SCALES; ++i) {
        v.n_scale_bits[i] = 0x0AB5;  // bit_reverse(0b101011010101, 12)
        v.n_scale_root[i] = 0;
    }
}

inline void init(TT2Runtime &r) {
    memset(&r, 0, sizeof(TT2Runtime));

    tt2InitVariables(r.variables);

    // MIDI fire-script selectors default to -1 (disabled), like upstream.
    r.midi.on_script = -1;
    r.midi.off_script = -1;
    r.midi.cc_script = -1;

    // RNG seeding: each slot gets a distinct non-zero starting state
    // so that different slots produce independent sequences.
    for (int i = 0; i < TT2_RNG_COUNT; ++i) {
        r.rng.state[i] = 0x12345678u + static_cast<uint32_t>(i) * 0x9e3779b9u;
    }

    // Turtle defaults matching upstream turtle_init().
    r.turtle.fence.x1 = 0;
    r.turtle.fence.y1 = 0;
    r.turtle.fence.x2 = 3;
    r.turtle.fence.y2 = 63;
    r.turtle.mode = TT2TurtleMode::Bump;
    r.turtle.heading = 180;
    r.turtle.speed = 100;
    r.turtle.scriptNumber = TT2_SCRIPT_COUNT;  // NO_SCRIPT equivalent
}

// sizeof guards are <= bounds verified on ARM STM32 release builds.
static_assert(sizeof(TT2Variables) <= 496, "TT2Variables size drift");
static_assert(sizeof(TT2RuntimeCommand) <= 52, "TT2RuntimeCommand size drift");
static_assert(sizeof(TT2Stack) <= 804, "TT2Stack size drift");
static_assert(sizeof(TT2DelayEntry) <= 62, "TT2DelayEntry size drift");
static_assert(sizeof(TT2DelayQueue) <= 484, "TT2DelayQueue size drift");
static_assert(sizeof(TT2EveryState) <= 372, "TT2EveryState size drift");
static_assert(sizeof(TT2Metro) <= 14, "TT2Metro size drift");
static_assert(sizeof(TT2Rng) <= 22, "TT2Rng size drift");
static_assert(sizeof(TT2ExecFrame) <= 22, "TT2ExecFrame size drift");
static_assert(sizeof(TT2ExecState) <= 164, "TT2ExecState size drift");
static_assert(sizeof(TT2Runtime) <= 5900, "TT2Runtime size drift");

// Active execution context accessors — resolve through the exec stack.
// depth must be > 0 (set by runScript or future nested execution).
// Callers must ensure depth > 0 before calling; accessing I without a
// frame is undefined behavior.
inline int16_t &tt2ActiveI(TT2Runtime &runtime) {
    assert(runtime.exec.depth > 0 && "tt2ActiveI called without exec frame");
    return runtime.exec.frames[runtime.exec.depth - 1].i;
}

inline uint8_t &tt2ActiveScriptNumber(TT2Runtime &runtime) {
    return runtime.exec.frames[runtime.exec.depth > 0
                                ? runtime.exec.depth - 1 : 0].script_number;
}

// Active execution frame — for function params (I1/I2) and result (FR).
inline TT2ExecFrame &tt2ActiveFrame(TT2Runtime &runtime) {
    return runtime.exec.frames[runtime.exec.depth > 0 ? runtime.exec.depth - 1 : 0];
}

inline uint8_t &tt2ActiveLineNumber(TT2Runtime &runtime) {
    return runtime.exec.frames[runtime.exec.depth > 0
                               ? runtime.exec.depth - 1 : 0].line_number;
}

// IF/ELIF/ELSE chain flag — lives on the exec frame (like upstream
// es_variables->if_else_condition) so it persists across script lines, not
// just across segments of one command. Only IF resets it.
inline uint8_t &tt2ActiveIfElse(TT2Runtime &runtime) {
    return runtime.exec.frames[runtime.exec.depth > 0
                               ? runtime.exec.depth - 1 : 0].if_else_condition;
}

// BREAK flag on the active exec frame — set by BREAK/BRK, checked + reset by
// the L loop (upstream es_variables->breaking).
inline uint8_t &tt2ActiveBreaking(TT2Runtime &runtime) {
    return runtime.exec.frames[runtime.exec.depth > 0
                               ? runtime.exec.depth - 1 : 0].breaking;
}

// Delay queue — faithful to upstream Teletype: parallel-slot model with a
// time==0 empty sentinel, delay clamped to >= 1 ms, caller context (script /
// I / fparams) snapshotted for restore at fire time. tt2DelayAdd returns
// false if the queue is full.
inline bool tt2DelayAdd(TT2Runtime &runtime, const TT2RuntimeCommand &command,
                        int16_t timeMs, uint8_t originScript, int16_t originI,
                        int16_t originFParam1, int16_t originFParam2) {
    if (timeMs < 1) timeMs = 1;
    for (int i = 0; i < TT2_DELAY_DEPTH; ++i) {
        TT2DelayEntry &e = runtime.delay.entries[i];
        if (e.time == 0) {
            e.command = command;
            e.time = timeMs;
            e.originScript = originScript;
            e.originI = originI;
            e.originFParam1 = originFParam1;
            e.originFParam2 = originFParam2;
            ++runtime.delay.count;
            return true;
        }
    }
    return false;
}

inline void tt2DelayClear(TT2Runtime &runtime) {
    for (int i = 0; i < TT2_DELAY_DEPTH; ++i) {
        runtime.delay.entries[i].time = 0;
    }
    runtime.delay.count = 0;
}

// EVERY / SKIP helpers — per-(script, line) counter state.
// Matches old Teletype semantics: tick does count++ then count %= mod.
// EVERY fires when count == 0 after modulo.
// SKIP fires when count != 0 after modulo.
inline void tt2EverySetSkip(TT2EveryCount &every, bool skip) {
    every.skip = skip ? 1 : 0;
}

inline void tt2EverySetMod(TT2EveryCount &every, int16_t mod) {
    if (mod < 0) mod = -mod;
    else if (mod == 0) mod = 1;
    every.mod = mod;
    every.count %= every.mod;
}

inline void tt2EveryTick(TT2EveryCount &every) {
    every.count++;
    every.count %= every.mod;
}

inline bool tt2EveryIsNow(const TT2EveryCount &every) {
    return every.count == 0 && every.skip == 0;
}

inline bool tt2SkipIsNow(const TT2EveryCount &every) {
    return every.count != 0 && every.skip != 0;
}
