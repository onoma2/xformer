# Clock Management Comparison: Teletype vs. Performer

## Teletype Clock Implementation

Teletype treats time as a series of discrete events rather than a continuous musical timeline. Its "clock" is primarily a scheduler for running scripts and decrementing counters.

### 1. System Tick (Script Delays)
The core "heartbeat" of Teletype is a coarse system timer used for `WAIT` (delay) commands and general housekeeping.

*   **Source:** `teletype/module/main.c`
*   **Resolution:** 10ms (`RATE_CLOCK` = 10)
*   **Mechanism:**
    *   `clockTimer_callback` fires every 10ms.
    *   It posts a `kEventTimer` event.
    *   `handler_EventTimer` calls `tele_tick`.

```c
// teletype/module/main.c
#define RATE_CLOCK 10  // 10ms tick

void clockTimer_callback(void* o) {
    event_t e = { .type = kEventTimer, .data = 0 };
    event_post(&e);
}

void handler_EventTimer(int32_t data) {
    tele_tick(&scene_state, RATE_CLOCK);
    // ...
}
```

*   **Logic:** `tele_tick` in `teletype/src/teletype.c` iterates through active delays. If a delay counter reaches zero, it executes the delayed command using a temporary script context.

### 2. Metronome (`M`)
The Metronome is an independent timer that triggers the `METRO` script (default Script 9) at a user-defined interval.

*   **Source:** `teletype/module/main.c` & `teletype_io.h`
*   **Mechanism:**
    *   `tele_metro_updated` sets up a `softTimer_t` (`metroTimer`).
    *   The callback `metroTimer_callback` posts `kEventAppCustom`.
    *   `handler_AppCustom` runs `run_script(&scene_state, METRO_SCRIPT)`.

### 3. MIDI Clock
MIDI Clock is reactive. It counts incoming `0xF8` bytes and executes a script when a divisor is met.

*   **Source:** `teletype/module/main.c`
*   **Mechanism:**
    *   No internal "flywheel" or smoothing.
    *   `midi_clock_tick` increments a counter.
    *   If `counter >= clock_div`, it runs `scene_state.midi.clk_script`.

```c
// teletype/module/main.c
static void midi_clock_tick(void) {
    if (++midi_clock_counter >= scene_state.midi.clock_div) {
        midi_clock_counter = 0;
        if (scene_state.midi.clk_script >= 0 ...)
            run_script(&scene_state, scene_state.midi.clk_script);
    }
}
```

---

## Performer Clock Implementation

Performer implements a high-resolution **Sequencer Transport** designed for musical timing, groove, and continuous playback.

### 1. High-Resolution Core
*   **Source:** `src/apps/sequencer/engine/Clock.h`
*   **Resolution:** 192 PPQN (Parts Per Quarter Note), defined in `Config.h`.
*   **Hardware:** Backed by `ClockTimer` (STM32 hardware timer) for microsecond precision.

### 2. Transport Logic
The `Clock` class maintains the global state of the sequencer:
*   **Tick Counting:** Increments a `_tick` counter from 0 to `infinity`.
*   **State Machine:** Manages `Idle`, `MasterRunning`, `SlaveRunning`.
*   **Sub-Tick Timing:** Calculates precise intervals for pulse widths and swing.

### 3. Synchronization (PLL-like)
Unlike Teletype's reactive trigger, Performer uses a `MovingAverage` to filter incoming slave clock jitter.
*   **Flywheel:** It calculates a smooth `_slaveBpm` and continues to generate internal ticks between external clock pulses. This ensures the sequence keeps moving smoothly even if the external clock has jitter.

```cpp
// src/apps/sequencer/engine/Clock.h
class Clock : private ClockTimer::Listener {
    // ...
    float bpm() const { return _state == State::SlaveRunning ? _slaveBpm : _masterBpm; }
    // ...
    MovingAverage<float, 4> _slaveBpmAvg;
};
```

---

## Comparison Summary

| Feature | Teletype | Performer |
| :--- | :--- | :--- |
| **Paradigm** | **Event-Driven**: Time is just another trigger. | **Transport-Driven**: Continuous musical timeline. |
| **Resolution** | **Coarse**: 10ms system tick. | **Fine**: 192 PPQN (Microsecond precision). |
| **Wait/Delay** | `WAIT` command counts down 10ms ticks. | N/A (Sequencer steps are fixed grid). |
| **MIDI Clock** | **Reactive**: Counts pulses -> Runs Script. | **Flywheel**: Smoothed average BPM -> Drives Engine. |
| **Jitter** | Susceptible (scripts fire exactly on receipt). | Resilient (internal smoothing). |
| **Metronome** | Simple timer triggering a script. | Integrated into the Master Transport logic. |

### Implications for Porting
When integrating Teletype features into Performer (or vice versa):
*   **Teletype scripts in Performer** cannot rely on the 10ms "System Tick" for musical timing if they need to be tight. They should be driven by Performer's **Transport Ticks** (192 PPQN) to stay in sync with the music.
*   **`WAIT` commands** in a Performer environment might need to be redefined as "Wait X Steps" or "Wait X Ticks" rather than "Wait X * 10ms" to be musically useful.
