# er-101 wallclock + per-track clocking notes

This file documents the wallclock implementation and how clock multipliers/divisors are handled in the er-101 codebase, with exact snippets for porting.

## Wallclock core (64-bit timebase + timers)

The wallclock is a 64-bit tick counter that combines a software-maintained base with the AVR32 COUNT register. The tick rate is `CPU_HZ`.

From `er-101/src/wallclock.h`:

```c
#define WALLCLOCK_TICKS_PER_SEC CPU_HZ
typedef uint64_t wallclock_t;

extern volatile wallclock_t wallclock_base;

__always_inline static wallclock_t wallclock(void)
{
  return wallclock_base + (wallclock_t)Get_system_register(AVR32_COUNT);
}
```

The storage for the base is defined in `er-101/src/wallclock.c`:

```c
volatile wallclock_t wallclock_base = 0;
```

The base is advanced in a core compare interrupt (every `COMPARE_PERIOD` cycles) in `er-101/src/interrupt_handlers.c`:

```c
#define COMPARE_PERIOD (10 * CPU_HZ)

__attribute__((__interrupt__)) static void compare_irq_handler(void)
{
  Set_system_register(AVR32_COMPARE, COMPARE_PERIOD);
  wallclock_base += COMPARE_PERIOD;
  // gpio_toggle_pin(PIN_COMMIT_LED);
}
```

Initialization of the compare interrupt and wallclock base is done in `configure_late_interrupts`:

```c
INTC_register_interrupt(&compare_irq_handler, AVR32_CORE_COMPARE_IRQ, AVR32_INTC_INT3);
Set_system_register(AVR32_COMPARE, COMPARE_PERIOD);
Set_system_register(AVR32_COUNT, 0);
wallclock_base = 0;
```

Timer helpers are thin wrappers around the wallclock, also in `er-101/src/wallclock.h`:

```c
typedef struct
{
  wallclock_t end_time;
  bool running;
} walltimer_t;

__always_inline static void walltimer_schedule(wallclock_t end_time, walltimer_t *timer)
{
  timer->end_time = end_time;
  timer->running = true;
}

__always_inline static bool walltimer_elapsed(walltimer_t *timer)
{
  if (timer->running && wallclock() > timer->end_time)
  {
    timer->running = false;
    return true;
  }

  return false;
}
```

## External clock capture and period estimation

The clock input ISR timestamps arrivals with `wallclock()` and filters period changes. This is how `seq.latched_period` and `seq.last_period` are tracked (from `er-101/src/interrupt_handlers.c`):

```c
clock_arrival_time = wallclock();

if (clock_arrival_time > seq.last_clock_timestamp)
{
  unsigned long this_period = clock_arrival_time - seq.last_clock_timestamp;
  if (this_period > seq.last_period / 2 && this_period < 2 * seq.last_period)
  {
    seq.latched_period = this_period;
  }
  else if (seq.latched_period == 0)
  {
    seq.latched_period = this_period;
  }
  seq.last_period = this_period;
}
else
{
  tilt_trigger();
}
seq.last_clock_timestamp = clock_arrival_time;

// queue this handler so that it doesn't fight with the extdac buffers
ui_q_push(on_clock_event);
```

The ISR queues `on_clock_event`, which forwards the timestamp to the sequencer (`er-101/src/event_handlers.c`):

```c
void on_clock_event()
{
  seq_on_clock_rising(clock_arrival_time, false, NULL);
}
```

Soft vs hard reset timing is also wallclock-based (reset input ISR in `er-101/src/interrupt_handlers.c`):

```c
if (wallclock() - clock_arrival_time < seq.soft_reset_interval)
{
  ui_q_push(ext_reset_after);
}
else
{
  ui_q_push(ext_reset_before);
}
```

The soft reset interval is configured in `sequencer_init` (`er-101/src/sequencer.c`):

```c
seq.soft_reset_interval = cpu_us_2_cy(500, CPU_HZ);
```

Clock-late detection uses the wallclock to compare against the last arrival time (from `er-101/src/event_handlers.c`):

```c
if (wallclock() - clock_arrival_time > 2 * seq.latched_period)
{
  segdisp_display[SEGDISP_TRACK] &= 0xFF00;
  seq.clock_is_late = true;
}
else
{
  seq.clock_is_late = false;
}
```

## Per-track clock multipliers (PPQN)

Each track has a per-track PPQN multiplier and per-track divide factor in `track_t` (from `er-101/src/sequencer.h`):

```c
uint8_t ppqn;
uint8_t divide;
```

The PPQN values are stepped using a jump table in `er-101/src/sequencer.c`:

```c
#define PPQN_JUMP_TABLE_SIZE 12
const uint8_t ppqn_jump_table[PPQN_JUMP_TABLE_SIZE] = {2, 3, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96};
```

Track defaults are initialized to 1:1 (no multiply, no divide) in `track_init`:

```c
track->ppqn = 1;
track->divide = 1;
```

Core PPQN multiply scheduling happens on each (real or synthetic) clock in `seq_on_clock_rising` (`er-101/src/sequencer.c`):

```c
if (p->pt->ppqn > 1)
{
  unsigned long synthetic_clock_period = PPQN_DIVIDE(seq.latched_period, p->pt->ppqn);
  seq.trigger_max_duration[i] = synthetic_clock_period - seq.trigger_unit;
  if (synthetic)
  {
    if (seq.ppqn_counters[i] < p->pt->ppqn)
    {
      walltimer_restart(synthetic_clock_period, &(seq.ppqn_timers[i]));
      seq.ppqn_counters[i]++;
    }
  }
  else
  {
    walltimer_schedule(now + synthetic_clock_period, &(seq.ppqn_timers[i]));
    seq.ppqn_counters[i] = 2;
  }
  extdac_set_smoother_timing(i, now, synthetic_clock_period);
}
else
{
  extdac_set_smoother_timing(i, now, seq.latched_period);
  seq.trigger_max_duration[i] = seq.latched_period - seq.trigger_unit;
}
```

Synthetic clocks are driven by per-track timers in `synthetic_clock_maintenance`:

```c
if (seq.players[0].pt->ppqn > 1 && walltimer_elapsed(&(seq.ppqn_timers[0])))
{
  include[0] = true;
  have_clock = true;
}
...
if (have_clock)
  seq_on_clock_rising(wallclock(), true, include);
```

The main loop calls `synthetic_clock_maintenance()` to generate those synthetic pulses (`er-101/src/main.c`):

```c
synthetic_clock_maintenance();
trigger_maintenance();
ui_timers_maintenance();
```

When PPQN changes, synthetic multiplication is canceled until the next real clock (`er-101/src/sequencer.c`):

```c
void seq_notify_ppqn_changed(position_t *p)
{
  // cancel clock multiplication until the next real clock pulse
  seq.ppqn_counters[p->pt->id] = 255;
  seq.ppqn_timers[p->pt->id].running = false;
  // reset to the beginning of the current step
  p->ticks = 0;
}
```

PPQN jump behavior is explicit:

```c
for (int i = 0; i < PPQN_JUMP_TABLE_SIZE; i++)
{
  if (ppqn_jump_table[i] > p->pt->ppqn)
  {
    p->pt->ppqn = ppqn_jump_table[i];
    return;
  }
}
// Did not find a larger ppqn so wrap back to 1.
p->pt->ppqn = 1;
```

## Per-track divisors (duration/gate scaling)

The per-track `divide` factor scales step duration and gate lengths throughout playback. In `seq_write_to_dac` (`er-101/src/sequencer.c`):

```c
int duration = p->ps->duration * p->pt->divide;

if (BIT_TEST(p->pt->options, OPTION_TRIGGER))
{
  // when trigger mode is enabled just assume that gate is half the step duration
  gate = duration >> 1;
}
else
{
  gate = GET_STEP_GATE(p->ps->gate) * p->pt->divide;
}
```

Gate logic applies `divide` again in `seq_update_gate_outputs`:

```c
int gate = GET_STEP_GATE(p->ps->gate);
int duration = p->ps->duration * p->pt->divide;
...
gate *= p->pt->divide;
```

The current step duration used for advancing also includes the per-track divide (`seq_on_clock_rising`):

```c
int16_t duration = (p->ps->duration + seq.current_tick_adjustment[i]) * p->pt->divide;
if (p->ticks >= duration)
{
  track_rewound[i] = seq_advance_position(p);
  p->ticks = 0;
  seq.current_tick_adjustment[i] = seq.next_tick_adjustment[i];
  seq.next_tick_adjustment[i] = 0;
}
```

## Reset handling tied to clocking

External reset behavior uses wallclock timing and explicitly resets PPQN counters/ticks. In `ext_reset_after` (`er-101/src/event_handlers.c`):

```c
for (int i = 0; i < NUM_TRACKS; i++)
{
  position_init(&(seq.players[i]), &(seq.data), &(seq.data.tracks[i]));
  seq.players[i].ticks = 0;
  seq.ppqn_counters[i] = 1;
}
...
seq.waiting_for_clock_after_reset = true;
// replay one clock pulse to account for clearing the last clock pulse
for (int i = 0; i < NUM_TRACKS; i++)
{
  seq.players[i].ticks = 1;
  seq.ppqn_counters[i] = 2;
}
```

If a clock never returns after a reset, wallclock timeouts shut gates down (`ui_timers_maintenance`):

```c
if (seq.waiting_for_clock_after_reset)
{
  if (wallclock() - clock_arrival_time > seq.last_period * 2)
  {
    // it appears the clock has stopped so lets shut down all high gates
    seq_hard_reset();
    seq_update_dac_outputs(NULL);
    seq.waiting_for_clock_after_reset = false;
  }
}
```

## Porting checklist (er-101 concepts -> your project)

Use this as a fill-in mapping to your target APIs. Keep semantics the same; only swap the underlying clock/timer primitives.

- Wallclock tick source -> `TODO: <your monotonic counter>` (must be monotonic, stable across interrupts)
- `wallclock_base` accumulation -> `TODO: <overflow/compare handler>` (adds fixed period to extend counter)
- `wallclock()` -> `TODO: <read monotonic 64-bit ticks>` (combine base + hardware counter)
- `walltimer_schedule/start/restart/elapsed` -> `TODO: <timer helpers>` (end_time + running flag semantics)
- Clock input ISR -> `TODO: <GPIO/edge interrupt>` (timestamp with wallclock and estimate period)
- Period filtering -> `TODO: <reject outliers>` (the 0.5x..2x window + latched_period fallback)
- Queueing clock events -> `TODO: <defer to audio/UI thread>` (equivalent to `ui_q_push(on_clock_event)`)
- `seq.latched_period` -> `TODO: <global clock period>` (used for synthetic and gate timing)
- Per-track PPQN -> `TODO: <per-track multiplier>` (ppqn jump table or direct value)
- `seq.ppqn_timers[]` + `seq.ppqn_counters[]` -> `TODO: <per-track synthetic timers>` (drive synthetic pulses)
- `synthetic_clock_maintenance()` -> `TODO: <periodic poll>` (run in main loop/tick thread)
- Track ticks -> `TODO: <per-track tick counter>` (increment on real or synthetic pulses)
- Step advance -> `TODO: <duration compare>` (uses `(duration + adjustment) * divide`)
- Gate + trigger -> `TODO: <gate scheduler>` (scale with divide; handle trigger mode and ratchet)
- Reset semantics -> `TODO: <external reset logic>` (soft vs hard reset based on wallclock delta)
- Clock late detection -> `TODO: <timeout guard>` (sets paused/late state if no clock in 2*period)

### API mapping template

Fill this in once you know your target project names:

- `wallclock()` -> `YOUR_CLOCK_NOW()`
- `walltimer_t` -> `YOUR_TIMER_TYPE`
- `walltimer_schedule()` -> `YOUR_TIMER_SET_ABS(end_time)`
- `walltimer_restart()` -> `YOUR_TIMER_ADD_REL(delta)`
- `walltimer_elapsed()` -> `YOUR_TIMER_EXPIRED()`
- `ui_q_push(on_clock_event)` -> `YOUR_DEFER(on_clock_event)`
- `gpio` clock IRQ -> `YOUR_GPIO_IRQ_HANDLER`

## Timing diagram (real clock, PPQN, divide)

Example: `latched_period = L`, `ppqn = 4` (4x multiplier), `divide = 2`, `step.duration = 1`.

```
Time:    0      L/4      L/2     3L/4      L     5L/4    3L/2    7L/4     2L
Real:   |R0---------------|R1---------------|R2---------------|R3---------> 
Synth:  |S0|-----|S1|-----|S2|-----|S3|-----|S4|-----|S5|-----|S6|-----|S7|
Ticks:   0   1     2   3     4   5     6   7     8   9    10  11    12  13

Advance rule per track:
  duration_ticks = (step.duration + current_tick_adjustment) * divide
  advance when ticks >= duration_ticks

With duration=1, divide=2 -> duration_ticks=2:
  step0: ticks 0..1  (advance at tick==2)
  step1: ticks 2..3  (advance at tick==4)
  step2: ticks 4..5  (advance at tick==6)
```

Notes:
- Tracks with `ppqn=1` only tick on real pulses (`R0`, `R1`, ...); no `S*` pulses are generated.
- `divide` scales both duration and gate calculations; gates are evaluated each tick.
- When `ppqn > 1`, synthetic pulses are scheduled from the most recent real pulse; changing PPQN cancels pending synthetic timers until the next real pulse.
