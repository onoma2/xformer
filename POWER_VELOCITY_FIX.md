# Fix: Tuesday Power/Velocity/Cooldown Regression

## Problem

The Tuesday sequencer's power parameter and velocity-dependent cooldown system was broken during recent polyrhythm/trill refactoring (commits ea7af23 and 0353dde).

### How Power/Cooldown Works

The **power parameter** (0-16) controls note density through a cooldown mechanism:
- Power 0 → cooldown 17 (very sparse, gates every 17 steps)
- Power 8 → cooldown 9 (medium density, gates every 9 steps)
- Power 16 → cooldown 1 (dense, gates almost every step)

The **velocity** from algorithms (0-255) can override the cooldown:
- Velocity is divided by 16 to get a density value (0-15)
- If `velDensity >= coolDown`, the gate fires even if cooldown hasn't expired
- This allows algorithms to generate expressive, varying rhythmic patterns

### What Broke

After introducing the micro-gate system for polyrhythms/trills, two bugs were introduced:

**Bug 1: Per-Micro-Gate Cooldown Decrement**
```cpp
// WRONG: Decremented cooldown for EACH micro-gate in the loop
for (int i = 0; i < tupleN; i++) {
    // ... power check ...
    if (_coolDown > 0) _coolDown--;  // BUG: burns through cooldown in one step!
}
```

If `polyCount=3` and `coolDown=5`:
- Gate 0: checks cooldown=5, decrements to 4
- Gate 1: checks cooldown=4, decrements to 3
- Gate 2: checks cooldown=3, decrements to 2
- Result: Lost 3 cooldown ticks in a single step instead of 1

**Bug 2: Cooldown Never Decremented for Micro-Gates**
```cpp
// Cooldown decrement was guarded by !_retriggerArmed
if (!_retriggerArmed) {
    if (_coolDown > 0) _coolDown--;
}
```

But `_retriggerArmed = true` was set when using micro-gates, so the cooldown never decremented at all when polyrhythms/trills were active!

## Solution

### Changes to `/Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/engine/TuesdayTrackEngine.cpp`

**1. Decrement cooldown ONCE per step, before any gate decisions** (line 1319-1323):
```cpp
// Decrement cooldown once per step (before any gate decisions)
if (_coolDown > 0) {
    _coolDown--;
    if (_coolDown > _coolDownMax) _coolDown = _coolDownMax;
}
```

**2. Apply step-level power check to ALL micro-gates** (lines 1344-1361):
```cpp
// STEP-LEVEL POWER CHECK (applies to ALL micro-gates in this step)
// Cooldown decrements once per STEP, not once per micro-gate
bool stepAllowed = false;

if (result.velocity == 0 || power == 0) {
    stepAllowed = false;
} else if (result.accent) {
    stepAllowed = true;
} else if (_coolDown == 0) {
    stepAllowed = true;
    _coolDown = _coolDownMax;
} else {
    int velDensity = result.velocity / 16;
    if (velDensity >= _coolDown) {
        stepAllowed = true;
        _coolDown = _coolDownMax;
    }
}
```

**3. Schedule micro-gates only if step is allowed** (lines 1364-1375):
```cpp
// Schedule micro-gates if step is allowed
if (stepAllowed) {
    for (int i = 0; i < tupleN; i++) {
        uint32_t offset = (i * spacing);
        int noteWithOffset = result.note + result.noteOffsets[i];
        float volts = scaleToVolts(noteWithOffset, result.octave);

        _microGateQueue.push({ baseTick + offset, true, volts });
        _microGateQueue.push({ baseTick + offset + gateLen, false, volts });
    }
}
```

**4. Remove duplicate cooldown decrement logic** (lines 1403-1405):
```cpp
// C. Density (Power Gating)
// Power/cooldown already calculated and decremented above
// Cooldown now decrements once per step regardless of micro-gate vs normal gate path
```

## Testing

Created comprehensive test suite at `/Users/foronoma/Work/Code/Eurorack/performer-phazer/src/tests/unit/sequencer/TestTuesdayPowerVelocity.cpp`:

- `power_to_cooldown_calculation`: Verifies power→cooldown mapping
- `velocity_overcomes_cooldown`: Tests velocity override mechanism
- `accent_always_fires`: Confirms accents bypass cooldown
- `cooldown_decrements_each_step`: Validates cooldown decrement pattern
- `algorithms_should_generate_varying_velocity`: Documents algorithm velocity ranges
- `power_zero_blocks_all_gates`: Tests power=0 edge case
- `expected_gate_firing_pattern`: Simulates 40-step sequence with power=8

All tests pass ✓

## Verification

Ran existing test suites to verify no regressions:
- `TestTuesdayMinimal`: ✓ Passed (13/13 cases)
- `TestTuesdayPolyrhythm`: ✓ Passed (5/5 cases)
- `TestTuesdayPowerVelocity`: ✓ Passed (7/7 cases)

## Impact

This fix restores the original Tuesday behavior where:
1. Power parameter controls note density via cooldown (17-power)
2. Algorithm velocity values can override cooldown for expressive patterns
3. Cooldown decrements once per step (not per micro-gate)
4. Accents always fire regardless of cooldown
5. Power=0 blocks all non-accent gates

The fix maintains full compatibility with the micro-gate system for polyrhythms and trills while restoring proper power/velocity interaction for all algorithms.

## Files Modified

- `/Users/foronoma/Work/Code/Eurorack/performer-phazer/src/apps/sequencer/engine/TuesdayTrackEngine.cpp` (lines 1319-1405)
- `/Users/foronoma/Work/Code/Eurorack/performer-phazer/src/tests/unit/sequencer/TestTuesdayPowerVelocity.cpp` (new file)
- `/Users/foronoma/Work/Code/Eurorack/performer-phazer/src/tests/unit/sequencer/CMakeLists.txt` (added test registration)
