# ER-101 Duration and Gate Computation from Global Clock and Track Divisor/Multiplier

## Overview

This document explains how the ER-101 computes duration and gate signals from the global clock and track-specific divisor/multiplier settings. The system uses a sophisticated timing mechanism that allows each track to operate independently while maintaining synchronization with the master clock.

## Core Timing Concepts

### Global Clock System

The ER-101 uses a centralized wallclock system that serves as the single timing reference:

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

### Clock Period Detection

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

## Track-Specific Timing Parameters

Each track has independent timing settings that affect duration and gate computation:

```c
// From sequencer.h
typedef struct
{
  // ... other fields ...
  uint8_t ppqn;      // Pulses Per Quarter Note (multiplier)
  uint8_t divide;    // Division factor
  // ... other fields ...
} track_t;
```

## Duration Computation

### Basic Duration Formula

Duration is computed by multiplying the step's duration value by the track's divide factor:

```c
// From sequencer.c in seq_write_to_dac function
int duration = p->ps->duration * p->pt->divide;
```

### Step Advancement Logic

The system advances to the next step when the current tick count reaches the calculated duration:

```c
// From sequencer.c in seq_on_clock_rising function
int16_t duration = (p->ps->duration + seq.current_tick_adjustment[i]) * p->pt->divide;
if (p->ticks >= duration)
{
  track_rewound[i] = seq_advance_position(p);
  p->ticks = 0;
  seq.current_tick_adjustment[i] = seq.next_tick_adjustment[i];
  seq.next_tick_adjustment[i] = 0;
}
```

### PPQN (Pulses Per Quarter Note) Effect

When PPQN > 1, the system generates synthetic clock pulses at a faster rate:

```c
// From sequencer.c in seq_on_clock_rising function
if (p->pt->ppqn > 1)
{
  unsigned long synthetic_clock_period = PPQN_DIVIDE(seq.latched_period, p->pt->ppqn);
  seq.trigger_max_duration[i] = synthetic_clock_period - seq.trigger_unit;
  // ... synthetic clock handling ...
}
else
{
  extdac_set_smoother_timing(i, now, seq.latched_period);
  seq.trigger_max_duration[i] = seq.latched_period - seq.trigger_unit;
}
```

## Gate Computation

### Gate Length Calculation

Gate length is computed differently depending on whether the track is in trigger mode:

```c
// From sequencer.c in seq_write_to_dac function
int gate;
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

### Gate Output Control

The gate output is managed in the `seq_update_gate_outputs` function:

```c
// From sequencer.c
void seq_update_gate_outputs(wallclock_t now, bool *include)
{
  for (int i = 0; i < NUM_TRACKS; i++)
  {
    if (include != NULL && !include[i])
      continue;
    position_t *p = &(seq.players[i]);

    int gate = GET_STEP_GATE(p->ps->gate);
    int duration = p->ps->duration * p->pt->divide;

    if (BIT_TEST(p->pt->options, OPTION_TRIGGER))
    {
      if (!seq.pause && p->s != INVALID_STEP && duration > 0 && gate > 0)
      {
        if (GET_STEP_RATCHET(p->ps->gate))
        {
          gate *= p->pt->divide;
          if (gate > duration)
            gate = duration;
          if (p->ticks % gate == 0)
          {
            walltimer_schedule(now + min(seq.trigger_max_duration[i], 5 * seq.trigger_unit), &(seq.trigger_timers[i]));
            gpio_set_pin_high(seq.gate_output_pins[i]);
          }
        }
        else
        {
          if (p->ticks == 0)
          {
            walltimer_schedule(now + min(seq.trigger_max_duration[i], gate * seq.trigger_unit), &(seq.trigger_timers[i]));
            gpio_set_pin_high(seq.gate_output_pins[i]);
          }
        }
      }
    }
    else
    {
      if (!seq.pause && p->s != INVALID_STEP)
      {
        gate *= p->pt->divide;
        if (GET_STEP_RATCHET(p->ps->gate))
        {
          if (gate > duration)
            gate = duration;
          if (p->ticks >= duration - (duration % gate))
          {
            gpio_set_pin_low(seq.gate_output_pins[i]);
          }
          else
          {
            if (p->ticks % (2 * gate) < gate)
            {
              gpio_set_pin_high(seq.gate_output_pins[i]);
            }
            else
            {
              gpio_set_pin_low(seq.gate_output_pins[i]);
            }
          }
        }
        else
        {
          if (gate == duration && p->ticks + 1 == gate)
          {
            uint32_t period;
            if (p->pt->ppqn > 1)
            {
              period = PPQN_DIVIDE(seq.latched_period, p->pt->ppqn);
            }
            else
            {
              period = seq.latched_period;
            }
            walltimer_schedule(now + (period >> 1), &(seq.trigger_timers[i]));
            gpio_set_pin_high(seq.gate_output_pins[i]);
          }
          else if (p->ticks < gate)
          {
            gpio_set_pin_high(seq.gate_output_pins[i]);
          }
          else
          {
            gpio_set_pin_low(seq.gate_output_pins[i]);
          }
        }
      }
    }
  }
}
```

## Key Timing Relationships

### Track Divisor Effect

The `divide` parameter affects both duration and gate calculations:
- `effective_duration = step_duration × track_divide_factor`
- `effective_gate_length = step_gate × track_divide_factor`

### PPQN Multiplier Effect

The `ppqn` (Pulses Per Quarter Note) parameter creates a multiplier effect:
- Higher PPQN values create faster internal clock rates
- `synthetic_clock_period = global_clock_period / ppqn`
- This allows for subdivision of the main clock

### Ratchet Mode

When ratchet mode is enabled (`GET_STEP_RATCHET(p->ps->gate)`), the gate output toggles multiple times within the duration:

```c
if (GET_STEP_RATCHET(p->ps->gate))
{
  if (gate > duration)
    gate = duration;
  if (p->ticks >= duration - (duration % gate))
  {
    gpio_set_pin_low(seq.gate_output_pins[i]);
  }
  else
  {
    if (p->ticks % (2 * gate) < gate)
    {
      gpio_set_pin_high(seq.gate_output_pins[i]);
    }
    else
    {
      gpio_set_pin_low(seq.gate_output_pins[i]);
    }
  }
}
```

## Trigger vs. Gate Modes

### Trigger Mode

When `OPTION_TRIGGER` is set:
- Gate is typically half the duration
- Creates short trigger pulses rather than sustained gates
- Used for drum sounds or short events

### Gate Mode

When in normal gate mode:
- Gate length is determined by the step's gate value multiplied by track divisor
- Creates sustained gate signals for note-on/note-off scenarios
- Gate remains high for the specified duration

## Maximum Duration Constraints

The system implements maximum duration limits to prevent overlapping triggers:

```c
// From sequencer.c
seq.trigger_max_duration[i] = synthetic_clock_period - seq.trigger_unit;
```

This ensures that consecutive triggers don't overlap and maintains timing integrity.

## Summary

The ER-101's duration and gate computation system provides flexible timing control through:

1. **Global Clock Reference**: Single timing source for all tracks
2. **Track-Specific Divisors**: Independent timing adjustments per track
3. **PPQN Multiplication**: Subdivision capabilities for complex rhythms
4. **Ratchet Support**: Multiple gates within a single step
5. **Trigger/Gate Modes**: Different output behaviors for different musical contexts
6. **Synchronization**: All tracks remain locked to the master clock

This architecture allows for complex polyrhythmic patterns while maintaining precise timing relationships between tracks.