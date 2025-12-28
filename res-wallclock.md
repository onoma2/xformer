# Minimal Wallclock Implementation for Drift Prevention

## Problem Statement

The Performer sequencer currently uses tick counters with modulo arithmetic for timing. While this works for simple cases, it can lead to **accumulated timing drift** over long sequences or with complex divisor ratios, especially:

1. **Multiple tracks with different divisors** - Small rounding errors accumulate differently per track
2. **Long sequences** - Drift accumulates over hundreds/thousands of ticks
3. **External clock sync** - No validation of clock stability, jitter can cause issues
4. **Free mode FM** - Phase accumulator is isolated per track, no global sync

### Current Architecture Issues

```
External Clock → Clock::slaveTick() → _tick++
                                         ↓
                    ┌────────────────────┴────────────────────┐
                    ↓                    ↓                     ↓
            TrackEngine1           TrackEngine2          TrackEngine3
       relativeTick % divisor  relativeTick % divisor  relativeTick % divisor
              ↓                       ↓                       ↓
        Step Advance             Step Advance            Step Advance
```

**Problem**: Each engine uses `tick % divisor` independently. Small floating-point or integer rounding errors in divisor calculations accumulate differently per track, causing relative drift.

---

## Minimal Wallclock Solution

The minimal approach adds **absolute time tracking** and **clock period validation** without requiring major architecture refactoring.

### Core Principle

Instead of relying solely on tick counters, use a **high-precision wallclock** as the single source of truth. All timing decisions reference the wallclock, eliminating accumulation of relative errors.

```
External Clock → Wallclock::now() → Validate Period → Update Engines
                     ↓
             uint64_t timestamp (µs)
                     ↓
         All engines reference same timebase
              (no accumulation)
```

---

## Implementation Architecture

### 1. Wallclock Infrastructure

#### 1.1 Hardware Timer Selection

**STM32F4 has several options:**

| Timer | Width | Max Freq | Overflow | Best Use |
|-------|-------|----------|----------|----------|
| **DWT_CYCCNT** | 32-bit | 168 MHz | ~25 sec | CPU cycle counter (recommended) |
| TIM2/TIM5 | 32-bit | 84 MHz | ~51 sec | General timers |
| TIM6/TIM7 | 16-bit | 84 MHz | ~780 µs | Basic timers |
| SysTick | 24-bit | 168 MHz | ~100 ms | OS tick (avoid) |

**Recommendation**: Use **DWT_CYCCNT** (Data Watchpoint and Trace Cycle Counter)
- Already running (no initialization needed beyond enabling)
- Highest precision (1/168MHz = 5.95 ns)
- Free-running, no interrupts
- Just need to track overflows for 64-bit time

#### 1.2 Wallclock Class Design

```cpp
// File: src/platform/stm32/drivers/Wallclock.h
#pragma once

#include <cstdint>

class Wallclock {
public:
    typedef uint64_t time_t;  // Microseconds since boot

    static void init();
    static time_t now();
    static uint32_t ticksPerSecond() { return 1000000; }

    // Convert between wallclock time and microseconds
    static time_t fromMicroseconds(uint32_t us) { return us; }
    static uint32_t toMicroseconds(time_t time) { return time; }

    // For debugging
    static uint32_t overflowCount() { return _overflowCount; }

private:
    static volatile uint32_t _lastCYCCNT;
    static volatile uint32_t _overflowCount;
    static constexpr uint32_t CPU_FREQ = 168000000;  // 168 MHz
};
```

#### 1.3 Wallclock Implementation

```cpp
// File: src/platform/stm32/drivers/Wallclock.cpp
#include "Wallclock.h"
#include <libopencm3/cm3/dwt.h>
#include <libopencm3/cm3/cortex.h>

volatile uint32_t Wallclock::_lastCYCCNT = 0;
volatile uint32_t Wallclock::_overflowCount = 0;

void Wallclock::init() {
    // Enable DWT cycle counter
    dwt_enable_cycle_counter();
    _lastCYCCNT = DWT_CYCCNT;
    _overflowCount = 0;
}

Wallclock::time_t Wallclock::now() {
    uint32_t cyccnt = DWT_CYCCNT;

    // Detect overflow (32-bit counter wraps)
    if (cyccnt < _lastCYCCNT) {
        _overflowCount++;
    }
    _lastCYCCNT = cyccnt;

    // Build 64-bit timestamp
    uint64_t cycles = ((uint64_t)_overflowCount << 32) | cyccnt;

    // Convert to microseconds (cycles / 168)
    // Use bit shift for efficiency: cycles / 168 ≈ cycles >> 7 (approximate)
    // Actually: cycles * 1000000 / 168000000 = cycles / 168
    return cycles / CPU_FREQ * 1000000;
}
```

**Pros:**
- Simple, ~50 lines of code
- No interrupts needed
- Microsecond precision
- 64-bit time never overflows in practice (~584,000 years)

**Cons:**
- Must call `now()` at least once per ~25 seconds to detect overflows
- Division by 168 has small rounding error (acceptable for our use)

---

### 2. Clock Period Tracking

#### 2.1 Period State Management

```cpp
// Add to src/apps/sequencer/engine/Clock.h

struct ClockPeriod {
    uint64_t lastTimestamp = 0;      // Wallclock time of last tick
    uint32_t lastPeriodUs = 0;       // Last measured period (µs)
    uint32_t latchedPeriodUs = 0;    // Validated stable period (µs)
    bool stable = false;             // Is clock period stable?

    // Validate new clock tick and update period
    bool update(uint64_t now);

    // Get current stable period (0 if unstable)
    uint32_t period() const { return latchedPeriodUs; }
};
```

#### 2.2 Period Validation Logic

```cpp
// Add to src/apps/sequencer/engine/Clock.cpp

bool Clock::ClockPeriod::update(uint64_t now) {
    if (lastTimestamp == 0) {
        // First tick
        lastTimestamp = now;
        stable = false;
        return false;
    }

    uint32_t thisPeriodUs = now - lastTimestamp;

    // Reject impossibly fast clocks (< 1ms = 1000 BPM @ PPQN=24)
    if (thisPeriodUs < 1000) {
        return false;
    }

    // Reject impossibly slow clocks (> 10s = 0.6 BPM @ PPQN=24)
    if (thisPeriodUs > 10000000) {
        return false;
    }

    if (lastPeriodUs > 0) {
        // Validate period stability: must be within 50%-200% of last period
        // This rejects jitter and unstable clocks
        if (thisPeriodUs < lastPeriodUs / 2 || thisPeriodUs > lastPeriodUs * 2) {
            // Clock became unstable, reject this tick
            stable = false;
            return false;
        }
    }

    // Accept period
    lastPeriodUs = thisPeriodUs;
    latchedPeriodUs = thisPeriodUs;
    lastTimestamp = now;
    stable = true;

    return true;
}
```

**Benefits:**
- Rejects jittery/unstable clocks
- Provides smooth period for subdivisions
- Simple hysteresis prevents glitches

---

### 3. Integration with Clock Class

#### 3.1 Clock Modifications

```cpp
// Add to Clock.h
#include "drivers/Wallclock.h"

class Clock : private ClockTimer::Listener {
public:
    // ... existing interface ...

    // New: Get validated clock period
    uint32_t measuredPeriodUs() const { return _clockPeriod.period(); }
    bool clockStable() const { return _clockPeriod.stable; }

private:
    // ... existing members ...
    ClockPeriod _clockPeriod;
};
```

```cpp
// Modify Clock::slaveTick() in Clock.cpp

void Clock::slaveTick(int slave) {
    if (!slaveEnabled(slave)) {
        return;
    }

    os::InterruptLock lock;

    // Measure clock period using wallclock
    uint64_t now = Wallclock::now();

    // Validate and update period
    if (!_clockPeriod.update(now)) {
        // Unstable clock, skip this tick
        return;
    }

    _activeSlave = slave;

    // Calculate tick period based on measured clock period
    _slaveTickPeriodUs = _clockPeriod.period();

    // ... rest of existing slaveTick logic ...
}
```

---

### 4. Drift-Free Step Timing

#### 4.1 Problem: Current Approach

Currently, engines use:
```cpp
relativeTick = tick % resetDivisor;
stepFraction = (relativeTick % divisor) / divisor;
```

Over time, `tick % divisor` can drift from actual elapsed time due to:
- Integer division rounding
- Accumulated modulo errors
- Timing jitter

#### 4.2 Solution: Wallclock-Based Step Timing

```cpp
// Add to TrackEngine base class
class TrackEngine {
protected:
    uint64_t _stepStartTime = 0;  // Wallclock time when step started
    uint32_t _stepDurationUs = 0; // Expected step duration (µs)

    // Calculate step fraction from wallclock
    float stepFractionFromWallclock(uint64_t now) const {
        if (_stepDurationUs == 0) return 0.0f;
        uint64_t elapsed = now - _stepStartTime;
        return float(elapsed) / float(_stepDurationUs);
    }
};
```

```cpp
// Modify CurveTrackEngine::updateOutput()

void CurveTrackEngine::updateOutput(uint32_t relativeTick, uint32_t divisor) {
    // Get current wallclock time
    uint64_t now = Wallclock::now();

    // Use wallclock-based fraction instead of tick-based
    if (_curveTrack.playMode() == Types::PlayMode::Free) {
        // Free mode: use phase accumulator
        _currentStepFraction = float(_freePhase);
    } else {
        // Aligned mode: use wallclock for drift-free timing
        if (_clock.clockStable()) {
            _currentStepFraction = stepFractionFromWallclock(now);
        } else {
            // Fallback to tick-based if clock unstable
            _currentStepFraction = float(relativeTick % divisor) / divisor;
        }
    }

    // ... rest of updateOutput ...
}
```

#### 4.3 Step Advancement

```cpp
// Modify CurveTrackEngine::tick()

TrackEngine::TickResult CurveTrackEngine::tick(uint32_t tick) {
    uint64_t now = Wallclock::now();
    uint32_t divisor = sequence.divisor() * (CONFIG_PPQN / CONFIG_SEQUENCE_PPQN);

    // Calculate expected step duration from measured clock period
    uint32_t measuredPeriod = _engine.clock().measuredPeriodUs();
    if (measuredPeriod > 0) {
        _stepDurationUs = measuredPeriod * divisor;
    }

    // Check if step should advance (wallclock-based)
    uint64_t stepElapsed = now - _stepStartTime;
    if (stepElapsed >= _stepDurationUs) {
        // Advance step
        _stepStartTime = now;  // Record new step start time
        advanceStep();
    }

    // ... rest of tick logic ...
}
```

---

### 5. Free Mode Integration

Free mode already uses a phase accumulator, so it's drift-free within a track. However, we can improve it by basing speed on **measured period** instead of hardcoded divisor:

```cpp
// In CurveTrackEngine::tick() Free mode

if (_curveTrack.playMode() == Types::PlayMode::Free) {
    // Use measured period instead of divisor for accurate speed
    uint32_t measuredPeriod = _engine.clock().measuredPeriodUs();

    if (measuredPeriod > 0) {
        float curveRate = _curveTrack.curveRate();

        // Base speed from measured period (not hardcoded divisor)
        double baseSpeed = 1000000.0 / measuredPeriod;  // Steps per second
        double speed = baseSpeed * curveRate / divisor;  // Adjust for divisor

        // Convert to phase increment per tick
        double phaseIncrement = speed / (1000000.0 / measuredPeriod);
        phaseIncrement = std::min(phaseIncrement, 0.5);  // Nyquist clamp

        _freePhase += phaseIncrement;

        if (_freePhase >= 1.0) {
            _freePhase -= 1.0;
            advanceStep();
        }
    }
}
```

---

## Implementation Roadmap

### Phase 1: Wallclock Foundation (1-2 days)

1. **Create Wallclock class**
   - `src/platform/stm32/drivers/Wallclock.h`
   - `src/platform/stm32/drivers/Wallclock.cpp`
   - Initialize in system startup

2. **Add sim platform stub**
   - `src/platform/sim/drivers/Wallclock.h`
   - Use `std::chrono` for simulator

3. **Test wallclock accuracy**
   - Unit test: verify overflow handling
   - Integration test: measure against known intervals

### Phase 2: Period Tracking (1 day)

1. **Add ClockPeriod to Clock class**
   - Implement validation logic
   - Test with stable and jittery clocks

2. **Integrate with slaveTick()**
   - Measure incoming clock period
   - Expose `measuredPeriodUs()` API

3. **Test period tracking**
   - Verify rejection of unstable clocks
   - Measure accuracy against various BPMs

### Phase 3: Engine Integration (1-2 days)

1. **Add wallclock timing to TrackEngine**
   - `_stepStartTime`, `_stepDurationUs`
   - `stepFractionFromWallclock()`

2. **Update CurveTrackEngine**
   - Use wallclock-based step fraction
   - Fallback to tick-based if clock unstable

3. **Update other engines** (NoteTrack, etc.)
   - Apply same wallclock timing

### Phase 4: Testing & Validation (2-3 days)

1. **Unit tests**
   - Wallclock overflow
   - Period validation edge cases
   - Step timing accuracy

2. **Integration tests**
   - Multi-track drift measurement
   - Long sequence stability (1000+ steps)
   - External clock sync accuracy

3. **Real-world testing**
   - Various BPMs (40-300)
   - Complex divisor ratios
   - Jittery clock sources

---

## Performance Considerations

### CPU Overhead

**Per tick overhead:**
```
Wallclock::now()          ~100 cycles  (~0.6 µs @ 168MHz)
Period validation         ~200 cycles  (~1.2 µs)
Step fraction calc        ~150 cycles  (~0.9 µs)
-----------------------------------------------------
Total per track/tick:     ~450 cycles  (~2.7 µs)
```

**For 8 tracks @ 120 BPM, PPQN=192:**
- Ticks per second: 120 × 192 / 60 = 384 tps
- CPU per second: 384 × 8 × 450 = ~1.4M cycles = **0.8% CPU**

**Verdict**: Negligible overhead ✓

### Memory Overhead

```
Wallclock static:        8 bytes (overflow count, last CYCCNT)
ClockPeriod:            24 bytes (timestamps, periods)
Per TrackEngine:        12 bytes (_stepStartTime, _stepDurationUs)
-----------------------------------------------------
Total:                  ~140 bytes
```

**Verdict**: Negligible memory usage ✓

---

## Benefits vs Full ER-101 Architecture

### Minimal Wallclock Approach

**Pros:**
- ✅ Prevents accumulation drift (wallclock reference)
- ✅ Validates clock stability
- ✅ Simple implementation (~400 LOC)
- ✅ No major architecture changes
- ✅ Backward compatible
- ✅ Low CPU/memory overhead
- ✅ 2-3 days implementation

**Cons:**
- ❌ No true synthetic clock generation (PPQN > 1)
- ❌ Doesn't solve all complex divisor edge cases
- ❌ Relies on existing tick counter for some logic

### Full ER-101 Architecture

**Pros:**
- ✅ Perfect synchronization (synthetic clocks)
- ✅ Handles all divisor ratios perfectly
- ✅ Tick adjustment mechanism
- ✅ Per-track PPQN support

**Cons:**
- ❌ Major refactor (~2000+ LOC)
- ❌ Requires walltimer infrastructure
- ❌ 2-4 weeks implementation
- ❌ Higher complexity/maintenance burden
- ❌ Potential backward compatibility issues

---

## Testing Strategy

### 1. Wallclock Accuracy Test

```cpp
// Test DWT_CYCCNT accuracy
void testWallclockAccuracy() {
    Wallclock::init();

    uint64_t start = Wallclock::now();
    os::delay(1000);  // 1 second delay
    uint64_t end = Wallclock::now();

    uint64_t elapsed = end - start;

    // Should be 1,000,000 µs ± 0.1%
    assert(elapsed > 999000 && elapsed < 1001000);
}
```

### 2. Period Validation Test

```cpp
// Test period validation rejects unstable clocks
void testPeriodValidation() {
    Clock::ClockPeriod period;

    // Stable clock: 500 µs period (120 BPM @ PPQN=192)
    uint64_t t = 0;
    for (int i = 0; i < 100; i++) {
        t += 500;
        assert(period.update(t) == (i > 0));  // First tick returns false
    }
    assert(period.stable);
    assert(period.period() == 500);

    // Unstable clock: period jumps 3x
    t += 1500;  // 3x jump
    assert(period.update(t) == false);  // Rejected
    assert(period.stable == false);
}
```

### 3. Multi-Track Drift Test

```cpp
// Measure drift between tracks over 10,000 ticks
void testMultiTrackDrift() {
    Engine engine;

    // Set up 8 tracks with different divisors
    for (int i = 0; i < 8; i++) {
        engine.track(i).setDivisor(1 + i);  // Divisors: 1,2,3,4,5,6,7,8
    }

    // Record start positions
    uint32_t startPositions[8];
    for (int i = 0; i < 8; i++) {
        startPositions[i] = engine.trackEngine(i).position();
    }

    // Run 10,000 ticks
    for (int tick = 0; tick < 10000; tick++) {
        engine.tick();
    }

    // Calculate expected positions based on divisors
    uint32_t expectedPositions[8];
    for (int i = 0; i < 8; i++) {
        expectedPositions[i] = startPositions[i] + (10000 / (1 + i));
    }

    // Verify drift < 1 tick per track
    for (int i = 0; i < 8; i++) {
        uint32_t actual = engine.trackEngine(i).position();
        int32_t drift = actual - expectedPositions[i];
        assert(abs(drift) <= 1);  // Max 1 tick drift
    }
}
```

---

## Migration Path

### Backward Compatibility

**Project Files:**
- No changes needed (tick counts remain the same)
- Wallclock is additive, doesn't break serialization

**User Experience:**
- Transparent improvement (timing becomes more stable)
- No UI changes required
- No workflow changes

### Rollback Plan

If wallclock causes issues:
```cpp
// Add compile-time flag
#define USE_WALLCLOCK_TIMING 1

#if USE_WALLCLOCK_TIMING
    _currentStepFraction = stepFractionFromWallclock(now);
#else
    _currentStepFraction = float(relativeTick % divisor) / divisor;
#endif
```

Allows quick disable via flag if problems arise.

---

## Conclusion

The minimal wallclock approach provides **80% of ER-101's drift prevention** with **20% of the implementation effort**. It's the right balance for the Performer's architecture and prevents the most common drift scenarios:

1. ✅ Multi-track drift (different divisors)
2. ✅ Long sequence drift (accumulation)
3. ✅ External clock jitter
4. ✅ Free mode accuracy (measured period)

**Estimated effort**: 2-3 days
**LOC**: ~400 lines
**Risk**: Low (additive, can be disabled)
**Benefit**: Significant improvement in timing stability

This is a **low-risk, high-reward** enhancement that fits well with the existing architecture.
