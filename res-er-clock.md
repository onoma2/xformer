# ER-101 Clock Synchronization and Time Drift Prevention

## Overview

The ER-101 uses a sophisticated synchronization mechanism to ensure all tracks remain perfectly synchronized without time drift. This document explains how the system achieves this using code snippets from the actual implementation.

## Core Timing System

### Wallclock Implementation

The system uses a high-precision wallclock that serves as the single timing reference:

```c
// From wallclock.h
#define WALLCLOCK_TICKS_PER_SEC CPU_HZ
typedef uint64_t wallclock_t;

extern volatile wallclock_t wallclock_base;

__always_inline static wallclock_t wallclock(void)
{
  return wallclock_base + (wallclock_t)Get_system_register(AVR32_COUNT);
}
```

### Clock Period Tracking

When an external clock arrives, the system calculates and validates the period:

```c
// From interrupt_handlers.c
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
```

## Per-Track Timing Parameters

Each track has independent timing settings:

```c
// From sequencer.h
typedef struct
{
  // ... other fields ...
  uint8_t ppqn;      // Pulses Per Quarter Note
  uint8_t divide;    // Division factor
  // ... other fields ...
} track_t;
```

## Synthetic Clock Generation

For tracks with PPQN > 1, the system generates synthetic clock pulses:

```c
// From sequencer.c
void synthetic_clock_maintenance(void)
{
  bool include[4] = {false, false, false, false};
  bool have_clock = false;
  if (seq.players[0].pt->ppqn > 1 && walltimer_elapsed(&(seq.ppqn_timers[0])))
  {
    include[0] = true;
    have_clock = true;
  }
  // ... similar checks for other tracks ...
  if (have_clock)
    seq_on_clock_rising(wallclock(), true, include);
}
```

## Tick-Based Timing System

Each track maintains its own tick counter for precise timing:

```c
// From sequencer.c
void seq_on_clock_rising(wallclock_t now, bool synthetic, bool *include)
{
  // ... initialization code ...

  // advance the play position on each track
  for (int i = 0; i < NUM_TRACKS; i++)
  {
    if (synthetic && !include[i])
      continue;
    position_t *p = &(seq.players[i]);

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

    if (p->s == INVALID_STEP)
    {
      // try to reinitialize the play position
      position_init(p, p->data, p->pt);
      seq.current_tick_adjustment[i] = 0;
      seq.next_tick_adjustment[i] = 0;
      track_rewound[i] = true;
    }
    else
    {
      int16_t duration = (p->ps->duration + seq.current_tick_adjustment[i]) * p->pt->divide;
      if (p->ticks >= duration)
      {
        track_rewound[i] = seq_advance_position(p);
        p->ticks = 0;
        seq.current_tick_adjustment[i] = seq.next_tick_adjustment[i];
        seq.next_tick_adjustment[i] = 0;
      }
    }

    track_rewound[i] = track_rewound[i] || skip_unplayable_steps(p);
  }

  // Now set the outputs
  seq_update_dac_outputs(include);
  seq_update_gate_outputs(now, include);

  // increment the clock count for the appropriate players
  if (synthetic)
  {
    if (include[0])
    {
      seq.players[0].ticks++;
    }
    if (include[1])
    {
      seq.players[1].ticks++;
    }
    if (include[2])
    {
      seq.players[2].ticks++;
    }
    if (include[3])
    {
      seq.players[3].ticks++;
    }
  }
  else
  {
    seq.players[0].ticks++;
    seq.players[1].ticks++;
    seq.players[2].ticks++;
    seq.players[3].ticks++;
  }
}
```

## Adjustment Mechanism

The system includes tick adjustment to maintain synchronization:

```c
// From sequencer.c
if (p->ticks >= duration)
{
  track_rewound[i] = seq_advance_position(p);
  p->ticks = 0;
  seq.current_tick_adjustment[i] = seq.next_tick_adjustment[i];
  seq.next_tick_adjustment[i] = 0;
}
```

## PPQN Division Calculation

The system calculates precise timing for different PPQN values:

```c
// From sequencer.h
#define PPQN_DIVIDE(period, ppqn) (period / ppqn)
```

## Smooth Timing Updates

For smooth CV transitions, timing is synchronized with the clock:

```c
// From sequencer.c
if (p->pt->ppqn > 1)
{
  extdac_set_smoother_timing(i, now, synthetic_clock_period);
}
else
{
  extdac_set_smoother_timing(i, now, seq.latched_period);
}
```

## Clock Event Processing

All tracks are processed simultaneously on each clock pulse:

```c
// From interrupt_handlers.c
void on_clock_event()
{
  seq_on_clock_rising(clock_arrival_time, false, NULL);
}
```

## Reset Synchronization

When tracks are reset, they all return to their start positions simultaneously:

```c
// From sequencer.c
void seq_hard_reset()
{
  for (int i = 0; i < NUM_TRACKS; i++)
  {
    gpio_set_pin_low(seq.gate_output_pins[i]);
    position_init(&(seq.players[i]), &(seq.data), &(seq.data.tracks[i]));
    seq.current_tick_adjustment[i] = 0;
    seq.next_tick_adjustment[i] = 0;
  }
  // ... other reset code ...
}
```

## Summary

The ER-101's synchronization system ensures that regardless of different PPQN settings or timing divisions across tracks, all tracks remain perfectly synchronized to the master clock without drift. The key components are:

1. **Centralized wallclock** - Provides single timing reference
2. **Per-track timing parameters** - Allow independent PPQN and division settings
3. **Synthetic clock generation** - Creates subdivided clocks when needed
4. **Tick-based timing** - Precise step advancement
5. **Adjustment mechanism** - Compensates for timing changes
6. **Simultaneous processing** - All tracks updated together
7. **Reset synchronization** - All tracks reset together

This architecture prevents any relative drift between tracks while allowing independent timing subdivisions.