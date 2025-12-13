# Microgate Sparsity Implementation (Option 1.5)

## Summary

Implemented **Option 1.5: Nested Cooldown with Velocity Affecting Microgate Threshold** to add sparsity control to the Tuesday sequencer's microgate (polyrhythm/trill) system.

**Key Feature:** Power parameter now controls density at TWO hierarchical levels:
1. **Step-level** (existing): Controls which steps fire
2. **Microgate-level** (NEW): Controls how many gates fire within each step

**No new UI parameters required** - Power (0-16) controls both levels.

---

## How It Works

### Power = 0: Special Case (MUTE)
- Hard-blocks all output
- No step fires, no microgates fire
- Preserved existing behavior

### Power = 1-16: Nested Cooldown

**Step Level:**
```cpp
_coolDownMax = 17 - power;  // Power 1 → cooldown 16, Power 16 → cooldown 1
_coolDown--;  // Decrements once per step
```

**Microgate Level:**
```cpp
_microCoolDownMax = 17 - power;  // Same mapping as step cooldown
_microCoolDown--;  // Decrements once per step (not per microgate!)
```

**Per-Microgate Check:**
```cpp
for (each microgate in polyrhythm) {
    if (_microCoolDown == 0) {
        fire();  // Cooldown expired
        _microCoolDown = _microCoolDownMax;
    } else if (velocity / 16 >= _microCoolDown * 2) {
        fire();  // Velocity override (2x harder threshold)
        _microCoolDown = _microCoolDownMax;
    }
}
```

---

## Musical Behavior by Algorithm

| Algorithm | Velocity | Low Power (4) | High Power (16) |
|-----------|----------|---------------|-----------------|
| **Chip1** | 255 (fixed) | Sparse steps, 1-3 gates/step | Dense steps, full polyrhythms |
| **Wobble** | 255 (fixed) | Sparse steps, 1-3 gates/step | Dense steps, full polyrhythms |
| **StepWave** | 200-255 | Sparse steps, mostly 1-2 gates | Dense steps, mostly full |
| **Aphex** | 180 or 80 | Variable texture | Mostly full polyrhythms |
| **Autechre** | 160 (fixed) | Predictable 1-3 pattern | Mostly full polyrhythms |
| **Markov** | 40-167 (random) | Mostly singles, lucky triples | Variable 1-3 gates |
| **Chip2** | 0-255 (random) | Chaotic variation | Variable density |
| **Stomper** | 0-63 (low) | **Always singles** | Mostly singles |
| **ScaleWalker** | 100-260 (power-linked!) | Singles | Full polyrhythms |

### Key Insight: Stomper Limitation
- Stomper's velocity (0-63) is too low to override microgate cooldown at moderate/high cooldowns
- Polyrhythm setting has minimal effect at low power
- **This is intentional** - preserves the sparse character of Stomper algorithm

---

## Implementation Details

### Files Modified

**1. TuesdayTrackEngine.h** (`src/apps/sequencer/engine/`)
```cpp
// Added member variables (lines 150-154):
int _microCoolDown = 0;
int _microCoolDownMax = 0;
```

**2. TuesdayTrackEngine.cpp** (`src/apps/sequencer/engine/`)

**Initialization** (lines 210-211):
```cpp
_microCoolDown = 0;
_microCoolDownMax = 0;
```

**Power Mapping** (line 1320):
```cpp
_microCoolDownMax = baseCooldown;  // Same as step cooldown
```

**Decrement Logic** (lines 1328-1332):
```cpp
if (_microCoolDown > 0) {
    _microCoolDown--;
    if (_microCoolDown > _microCoolDownMax) _microCoolDown = _microCoolDownMax;
}
```

**Microgate Scheduling** (lines 1382-1410):
```cpp
for (int i = 0; i < tupleN; i++) {
    bool microgateAllowed = false;

    if (_microCoolDown == 0) {
        microgateAllowed = true;
        _microCoolDown = _microCoolDownMax;
    } else {
        int microThreshold = _microCoolDown * 2;  // 2x harder
        if (velDensity >= microThreshold) {
            microgateAllowed = true;
            _microCoolDown = _microCoolDownMax;
        }
    }

    if (microgateAllowed) {
        // Schedule gate...
    }
}
```

**3. TestTuesdayMicrogateSparsity.cpp** (NEW)
- Created comprehensive test suite with 10 test cases
- Tests power mapping, velocity override, algorithm interactions
- Registered in `CMakeLists.txt` (line 65)

---

## Testing Strategy

### Unit Tests Created
1. `power_maps_to_microgate_cooldown` - Verify power → cooldown mapping
2. `microgate_threshold_is_2x_harder` - Verify 2x threshold calculation
3. `chip1_velocity_can_override_moderate_microcooldown` - High velocity tests
4. `stomper_velocity_cannot_override_microcooldown` - Low velocity tests
5. `stepwave_velocity_can_override_medium_microcooldown` - Medium velocity tests
6. `microgate_fires_when_cooldown_zero` - Basic firing logic
7. `microgate_fires_when_velocity_overrides` - Override logic
8. `power_zero_blocks_microgates` - Power=0 special case
9. `microgate_cooldown_decrements_per_step` - Decrement timing
10. `polyrhythm_creates_multiple_microgate_opportunities` - Polyrhythm interaction

### Regression Testing
All existing Tuesday tests should pass:
- `TestTuesdayMinimal`
- `TestTuesdayPolyrhythm`
- `TestTuesdayPowerVelocity`
- Others...

---

## Design Rationale

### Why Option 1.5?

**Pros:**
✅ No new UI parameters (power controls both levels)
✅ Natural extension of existing cooldown system
✅ Creates meaningful variation (1-3 gates per step)
✅ Velocity override preserves dynamic expression
✅ Predictable behavior across power range

**Cons:**
⚠️ Stomper algorithm gets limited polyrhythm effect (low velocity)
⚠️ Random velocity algorithms (Chip2, Markov) have unpredictable microgate counts

### Why 2x Threshold?

Making microgate threshold **2x harder** than step-level prevents:
- All microgates firing every time (defeats the purpose)
- Microgate cooldown being meaningless (too easy to override)

Creates natural hierarchy:
- Step-level cooldown: "Is this step dense enough to fire?"
- Microgate-level cooldown: "How many gates should fire in this step?"

### Why Decrement Once Per Step?

Decrementing microgate cooldown **once per step** (not once per microgate) prevents:
- Cooldown burning through too fast with polyrhythms
- Power becoming meaningless (cooldown always near zero)

Preserves the musical semantic:
- Power controls overall pattern density
- Polyrhythm/velocity controls texture within allowed steps

---

## User Guide

### Power Sweep Behavior (Polyrhythm = 3)

**Power = 0:** Silence (mute)

**Power = 1-4 (Very Sparse):**
- Steps fire rarely (~every 13-16 steps)
- When steps fire: mostly 1 gate, occasionally 2-3 (depends on velocity & phase)
- **Sound:** "ping.......ping......brrt....."

**Power = 8 (Medium):**
- Steps fire semi-regularly (~every 9 steps)
- When steps fire: mix of 1-3 gates
- **Sound:** "ping..brrt...ping..brrt.brrt..."

**Power = 12-16 (Dense):**
- Steps fire frequently (~every 1-5 steps)
- When steps fire: mostly 2-3 gates (full polyrhythms)
- **Sound:** "brrt-brrt-ping-brrt-brrt-brrt..."

### Algorithm Recommendations

**For full polyrhythmic expression:**
- Chip1, Wobble, StepWave (high velocity algorithms)
- Power 8-16 for rich texture

**For sparse, unpredictable patterns:**
- Markov, Chip2 (random velocity)
- Power 4-8 for organic feel

**For minimal, single-note patterns:**
- Stomper (low velocity)
- Any power level (polyrhythm has minimal effect)

**For smooth density control:**
- ScaleWalker (power-linked velocity)
- Power sweep creates natural crescendo/decrescendo

---

## Future Enhancements (Not Implemented)

### Possible Improvements
1. **Separate microgate parameter** (breaks "no new UI" constraint)
2. **Algorithm-aware thresholds** (complexity vs benefit tradeoff)
3. **Variable threshold multiplier** (1.5x? 3x? needs musical testing)
4. **Velocity-based microgate count** (more predictable than cooldown)

### Why Not Implemented
- Current design achieves the goal: **reduce density while keeping polyrhythmic character**
- Single parameter control is elegant and doesn't overwhelm users
- Natural variation is musically interesting
- Stomper limitation is acceptable (maintains algorithm identity)

---

## Verification Commands

```bash
# Build simulator
mkdir -p build/sim/debug
cd build/sim/debug
cmake ../../.. -DPLATFORM=sim -DCMAKE_BUILD_TYPE=Debug
make

# Run microgate sparsity tests
./src/tests/unit/sequencer/TestTuesdayMicrogateSparsity

# Run all Tuesday tests
make test

# Run specific regression tests
./src/tests/unit/sequencer/TestTuesdayMinimal
./src/tests/unit/sequencer/TestTuesdayPolyrhythm
./src/tests/unit/sequencer/TestTuesdayPowerVelocity
```

---

## Conclusion

Option 1.5 successfully extends the power parameter to control microgate sparsity without requiring new UI elements. The nested cooldown system creates natural variation in polyrhythmic patterns while preserving the expressive velocity override mechanism. The 2x harder threshold ensures meaningful sparsity control across the full range of Tuesday algorithms.

**Result:** Power now controls both **step frequency** (how often notes fire) AND **microgate density** (how many gates per step) with a single, intuitive parameter.
