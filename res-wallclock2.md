# Wallclock-Based Drift Elimination for Free + Aligned Tracks

## Problem
Free-mode tracks advance with per-track counters (e.g., `_stepTimer++`, `_freeRelativeTick++`, `_freePhase += speed`). Aligned tracks derive step position from the global transport tick (`tick % divisor`). Any rounding error, tick jitter, or local counter drift accumulates in free mode, so two tracks with identical length (one aligned, one free) slowly slip.

## Goals
- Keep free-mode tracks phase-locked to the transport clock without quantizing them to the aligned grid.
- Ensure a free and aligned track of equal length never drift over time.
- Minimize code changes and preserve existing sequencing semantics.

## Non-Goals
- Full scheduling refactor or new task threads.
- Changes to UI or project storage.
- New clock sources or transport states.

## Key Idea
Compute a continuous transport tick position from a monotonic wallclock and the current tick period, then drive free-mode phase from that shared timebase. Aligned tracks continue to use integer ticks. Free tracks use the same transport but with fractional phase.

This makes both play modes share the same wallclock-derived period while keeping free mode smooth and unquantized.

## Proposed Design

### 1) Add wallclock-based transport tick position
Expose a continuous tick position in `Clock` (or `Engine`) that combines:
- integer tick count (`_tick`), and
- fractional tick phase based on time since last tick.

Minimal approach (Clock-level):
- Store `_lastTickUs` whenever a tick is generated.
- Provide:
  - `double tickPosition()` = `_tick + (nowUs - _lastTickUs) / tickPeriodUs`
  - `double tickPeriodUs()` = period derived from BPM/PPQN in master, or `_slaveSubTickPeriodUs` in slave.

**Performance Consideration**: `tickPosition()` involves division on every call. On STM32, this could impact `Engine::update()` timing if called frequently across multiple track engines. Consider:
- Caching `tickPeriodUs()` when BPM is stable
- Pre-computing `1.0 / tickPeriodUs()` for multiplication instead of division
- Profiling the impact on Engine::update() execution time (check against task priorities)

**Precision Consideration**: Double-precision should be sufficient for sequencer timescales. At 192 PPQN, tick counts reach billions after ~24 hours of continuous operation. This is unlikely to cause precision issues but worth adding a long-duration test.

### 2) Use transport tick position in free-mode code paths
Free-mode should compute phase from `tickPosition()` rather than incrementing local counters.

Aligned mode remains unchanged and continues using integer ticks.

### 3) Keep phase stability across start/stop/reset
- On transport reset, track free-mode phase resets to 0 (current behavior).
- On continue, phase is continuous since `tickPosition()` is continuous.
- If needed, add a per-track `freeOffsetTicks` to allow intentional offsets without drift.

**Implementation Note**: When transport continues (not just ticks), `_lastTickUs` needs explicit initialization to avoid stale values on first `tickPosition()` call. Consider initializing on `start()` and `continue()`.

## Minimal Code Changes (Outline)

### A) Clock (shared timebase)
Files:
- `src/apps/sequencer/engine/Clock.h`
- `src/apps/sequencer/engine/Clock.cpp`

Add:
- `uint32_t _lastTickUs` (updated at each output tick)
- `double tickPeriodUs() const`
- `double tickPosition() const`

Implementation sketch:
```cpp
// Clock.h
uint32_t tickPeriodUs() const;
double tickPosition() const;

// Clock.cpp
uint32_t Clock::tickPeriodUs() const {
    // master: 60e6 / (bpm * ppqn)
    // slave: use _slaveSubTickPeriodUs when running
    // Consider: separate methods for master/slave or handle both cases?
}

double Clock::tickPosition() const {
    uint32_t now = HighResolutionTimer::us();
    uint32_t elapsed = now - _lastTickUs; // wrap-safe
    return double(_tick) + (double(elapsed) / tickPeriodUs());
}

// in outputTick or onClockTimerTick
_lastTickUs = HighResolutionTimer::us();

// in start() and continue()
_lastTickUs = HighResolutionTimer::us(); // Initialize to avoid stale values
```

**Master vs Slave**: The `tickPeriodUs()` calculation differs significantly between master and slave mode. Consider whether this should be two methods (e.g., `masterTickPeriodUs()` and `slaveTickPeriodUs()`) or handle both cases in a single method with clear branching.

### B) Free-mode updates per engine
Files:
- `src/apps/sequencer/engine/NoteTrackEngine.cpp`
- `src/apps/sequencer/engine/IndexedTrackEngine.cpp`
- `src/apps/sequencer/engine/CurveTrackEngine.cpp`
- `src/apps/sequencer/engine/DiscreteMapTrackEngine.cpp`
- `src/apps/sequencer/engine/TuesdayTrackEngine.cpp`

Replace local increment counters in free mode with `tickPosition()`-based math.

**Shared Helper Consideration**: Since the pattern is similar across engines, consider adding a shared helper method in `TrackEngine` base class to reduce code duplication. For example:
```cpp
// TrackEngine.h
protected:
    double getTransportTickPosition() const { return _engine.clock().tickPosition(); }
    int detectStepTransition(double tickPos, uint32_t divisor, int& lastStepIndex);
```

#### Example pattern (shared)
```cpp
// before
_freeRelativeTick++;
if (_freeRelativeTick >= divisor) { ... }

// after
double tickPos = _engine.clock().tickPosition();
double stepPos = tickPos / divisor;  // continuous
int stepIndex = int(floor(stepPos));
if (stepIndex != _lastFreeStepIndex) {
    _lastFreeStepIndex = stepIndex;
    // trigger/advance
}
```

#### IndexedTrackEngine free mode (duration-based)
Use a floating accumulator in tick units and advance by elapsed ticks:
```cpp
double tickPos = _engine.clock().tickPosition();
double deltaTicks = tickPos - _lastFreeTickPos;
_lastFreeTickPos = tickPos;

// Clamp deltaTicks to prevent multiple step boundaries in one update
// (e.g., after long UI operations or file I/O suspend)
const double MAX_DELTA_TICKS = _effectiveStepDuration * 2.0;
if (deltaTicks > MAX_DELTA_TICKS) {
    deltaTicks = MAX_DELTA_TICKS;
}

_freeStepTicks += deltaTicks;
if (_freeStepTicks >= _effectiveStepDuration) {
    advanceStep();
    _freeStepTicks -= _effectiveStepDuration;
}
```

**Delta Clamping**: Document the expected maximum `deltaTicks` value based on:
- Typical `Engine::update()` call frequency
- Worst-case suspend duration (file I/O operations)
- Use clamping to prevent multiple step boundaries in a single update

#### CurveTrackEngine free mode (phase-based)
Replace `_freePhase += speed` with wallclock-derived phase:
```cpp
// speed in steps per tick
double tickPos = _engine.clock().tickPosition();
double deltaTicks = tickPos - _lastFreeTickPos;
_lastFreeTickPos = tickPos;

_freePhase += deltaTicks * speed;
if (_freePhase >= 1.0) { advance; _freePhase -= 1.0; }
```

#### DiscreteMapTrackEngine free mode
Drive ramp phase from tickPosition:
```cpp
double tickPos = _engine.clock().tickPosition();
uint32_t period = scaledDivisorTicks();
_rampPhase = fmod(tickPos, period) / period;
```

#### TuesdayTrackEngine free mode
Replace `_stepIndex++` with a wallclock-based step index:
```cpp
double tickPos = _engine.clock().tickPosition();
int stepIndex = int(floor(tickPos / divisor));
if (stepIndex != _stepIndex) {
    _stepIndex = stepIndex;
    // trigger/advance
}
```

## Why This Eliminates Drift
- Both aligned and free tracks derive timing from the same transport clock.
- Free tracks use fractional ticks (continuous time), so rounding errors do not accumulate.
- Equal-length free and aligned tracks share the same divisor and therefore the same period.

## Edge Cases

### Tempo changes
Tick period changes should smoothly affect phase; `tickPosition()` handles it because it is derived from current period.

**Caveat**: During tempo ramps, `tickPeriodUs()` changes continuously. This may cause small phase jumps between updates (the phase advances by a different amount based on the new period). These jumps should be imperceptible at typical sequencer timescales but worth testing under:
- Rapid BPM changes (tap tempo, MIDI clock jitter)
- Smooth tempo ramps (if implemented)

If abrupt changes cause audible artifacts, consider smoothing or clamping deltas to a reasonable maximum.

### Slave jitter
Use filtered BPM or the derived `_slaveSubTickPeriodUs` for stability. External MIDI clock can be jittery; ensure the tick period calculation includes sufficient smoothing.

### Timer wrap
`uint32_t` microsecond timer wraps at ~71 minutes (2^32 / 1e6 / 60).

**Important**: This wrap limit should be more prominent in documentation and testing. The wrap-safe subtraction pattern (`now - last`) is correct and must be consistently applied in all `tickPosition()` calculations. Verify that:
- `HighResolutionTimer::us()` uses `uint32_t`
- All elapsed time calculations use unsigned arithmetic
- No code path assumes monotonically increasing values

Consider adding a unit test that simulates timer wrap conditions.

## Verification Plan

### Basic Drift Test
- Create two tracks with equal divisor/length: one Aligned, one Free.
- Measure phase difference after long runs (e.g. 10+ minutes); it should remain constant.
- Repeat under master BPM changes and external clock.

### Extended Duration Tests
- **24-hour run**: Verify no precision degradation with high tick counts (billions)
- **Wrap test**: Simulate or wait for 71-minute timer wrap; verify no discontinuities
- **Tempo sweep**: Test gradual BPM changes from minimum to maximum
- **Jitter test**: Inject simulated MIDI clock jitter in slave mode

### Performance Tests
- Profile `Engine::update()` execution time with/without wallclock calculations
- Verify timing stays within task priority constraints (see `Config.h`)
- Test on actual STM32 hardware (simulator timing differs significantly)

### Simulation Tests
Add a long-duration automated test:
```cpp
// TestWallclockDrift.cpp
CASE("free_aligned_equivalence") {
    // Run for simulated 1+ hour
    // Verify phase difference < epsilon
}
```

## Risk and Mitigations

### Risk: free-mode trigger logic expects integer ticks
**Mitigation**: use `floor(tickPosition)` when an integer tick is needed.

### Risk: multiple step boundaries in one update
**Mitigation**: if needed, iterate for large deltas or clamp deltaTicks to maximum safe value (e.g., 2x typical step duration).

### Risk: division performance impact on STM32
**Mitigation**:
- Cache `tickPeriodUs()` result when BPM is stable
- Pre-compute reciprocal for multiplication
- Profile early on hardware
- Consider update throttling if needed

### Risk: reset/continue behavior with stale `_lastTickUs`
**Mitigation**: Explicitly initialize `_lastTickUs` in `start()` and `continue()` methods.

## Implementation Phasing

To reduce risk, implement in stages:

1. **Phase 1**: Add `tickPosition()` infrastructure to Clock
   - Add `_lastTickUs`, `tickPeriodUs()`, `tickPosition()`
   - Update `_lastTickUs` only when a tick is emitted (in `outputTick()` or the exact branch that increments `_tick`)
   - In master mode, source `tickPeriodUs()` from `_timer.period()` to match the actual timer cadence
   - Guard against `tickPeriodUs() == 0` (e.g., before first slave tick); if zero, return `_tick` with no fractional component
   - Add unit tests for wrap-safety and precision
   - Profile performance impact

2. **Phase 2**: Convert one engine type (e.g., TuesdayTrackEngine)
   - Simplest free-mode logic
   - Prefer absolute position (step index from `floor(tickPos / divisor)`) where possible; if absolute positioning is used, delta clamping can be dropped
   - Verify no regressions in aligned mode
   - Test drift elimination

3. **Phase 3**: Convert remaining engines
   - Apply pattern to NoteTrackEngine, CurveTrackEngine, etc.
   - Each engine can be independently verified

4. **Phase 4**: Long-duration validation
   - Run extended tests (24hr, wrap)
   - Hardware validation on STM32
   - Performance tuning if needed

## Visual Reference

Consider adding a timing diagram showing:
```
Aligned Track:  |----tick----|----tick----|----tick----|
                step 0       step 1       step 2

Free Track:     |----tick----|----tick----|----tick----|
                step 0.0     step 0.5     step 1.0     step 1.5
                     ^fractional phase from tickPosition()
```

## Summary
Convert free-mode progression to a wallclock-derived transport phase shared with aligned tracks. Keep aligned logic unchanged. This removes drift while requiring only small, localized changes in `Clock` plus free-mode sections in track engines.

**Key Success Factors**:
- Profile early and often on STM32 hardware
- Test wrap-safety and long-duration stability
- Implement incrementally to isolate issues
- Document delta clamping and tempo change behavior
