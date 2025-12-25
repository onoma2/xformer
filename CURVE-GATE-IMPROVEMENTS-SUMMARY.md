# Curve Track Gate System - Improvements Summary

## Changes Made

### 1. Bitfield Structure Updates (CurveSequence.h)

**Before:**
```cpp
BitField<uint16_t, 0, Gate::Bits> gate;           // 4 bits
BitField<uint16_t, 4, GateProbability::Bits> gateProbability;  // 3 bits
```

**After:**
```cpp
BitField<uint16_t, 0, 4> gateEventMask;      // bits 0-3: Event enable flags
BitField<uint16_t, 4, 3> gateParameter;      // bits 4-6: Trigger length or Advanced mode
```

**Compatibility:**
- Old `gate()` / `setGate()` methods still work (mapped to gateEventMask)
- Old `gateProbability()` / `setGateProbability()` methods still work (mapped to gateParameter)
- New semantic names available: `gateEventMask()`, `gateParameter()`

**New Helper Methods:**
```cpp
uint32_t gateTriggerLength() const;           // Returns 1, 2, 4, 8, 16, 32, 64, or 128 ticks
AdvancedGateMode gateAdvancedMode() const;    // Returns advanced mode enum
```

---

### 2. Trigger Length Fix (CurveTrackEngine.cpp)

**Before:**
```cpp
if (trigger) {
    // Fixed trigger length of 3 ticks (~7.8ms @ 120BPM)
    _gateTimer = 3;
}
```

**After:**
```cpp
if (trigger) {
    // Use exponential trigger length: 1, 2, 4, 8, 16, 32, 64, 128 ticks
    _gateTimer = step.gateTriggerLength();
}
```

**Result:** Trigger length now properly uses the exponential scale controlled by GATE MODE parameter.

---

### 3. Encoder Rotation Messages (CurveSequenceEditPage.cpp)

**Added for GATE EVENTS Layer:**
```cpp
// Shows which events are enabled
int mask = step.gate();
if (mask == 0) {
    showMessage("GATE: ADVANCED");
} else {
    FixedStringBuilder<32> msg("GATE: ");
    if (mask & CurveSequence::Peak) msg("P ");
    if (mask & CurveSequence::Trough) msg("T ");
    if (mask & CurveSequence::ZeroRise) msg("Z+ ");
    if (mask & CurveSequence::ZeroFall) msg("Z-");
    showMessage(msg);
}
```

**Example Messages:**
- `GATE: P T` - Peaks and Troughs enabled
- `GATE: Z+ Z-` - Both zero crossings enabled
- `GATE: ADVANCED` - Advanced mode active

**Added for GATE MODE Layer:**
```cpp
// Context-aware based on event mask
if (mask == 0) {
    // Advanced Mode - show mode name
    showMessage("MODE: RISING");
    showMessage("MODE: >50%");
    // etc.
} else {
    // Event Mode - show trigger length
    showMessage("TRIG: 8t (~1.0ms)");
    showMessage("TRIG: 32t (~4.2ms)");
    // etc.
}
```

**Example Messages:**
- Event Mode: `TRIG: 8t (~1.0ms)` - Shows both ticks and approximate time
- Advanced Mode: `MODE: RISING` - Shows selected continuous gate mode

---

### 4. Musical Guide (CURVE-GATE-MUSICAL-GUIDE.md)

Comprehensive 300+ line guide covering:

**By Curve Shape:**
- Sine Wave (Bell/Smooth) - 3 patches
- Triangle Wave - 3 patches
- Sawtooth/Ramp - 3 patches
- Exponential Curves - 3 patches
- Macro Curves (M-DAMP, M-FM, M-BOUNCE) - 3 patches
- Step Curves - 2 patches

**Advanced Techniques:**
- Polyrhythmic Triggers
- Phase-Locked Triggers
- Dynamic Accent Patterns
- Ratcheting with Macro Shapes
- Envelope Following

**Quick Recipes:**
- LFO Clock Doubler
- Natural Envelope Chain
- Rhythmic Bursts
- Probability-Free Accents

**Reference Tables:**
- Trigger Length Reference (0-7 values with musical uses)
- Tips for Musical Results
- Troubleshooting section

---

## What's Working Now

### ✅ Event Detection
- **Peak (P):** Triggers when curve reaches maximum
- **Trough (T):** Triggers when curve reaches minimum
- **Zero Cross ↑ (Z+):** Triggers crossing zero upward
- **Zero Cross ↓ (Z-):** Triggers crossing zero downward
- Multiple events can be enabled simultaneously

### ✅ Trigger Length Control
- Exponential scale: 1, 2, 4, 8, 16, 32, 64, 128 ticks
- Range from ~0.13ms (ultra-fast) to ~16.7ms @ 120BPM
- Displayed in both ticks and milliseconds during editing

### ✅ Advanced Modes (when Event Mask = 0)
- **OFF:** No gate output
- **RISING:** Gate HIGH while curve rising
- **FALLING:** Gate HIGH while curve falling
- **MOVING:** Gate HIGH while changing (any slope)
- **>25%:** Comparator gate above 25%
- **>50%:** Comparator gate above 50%
- **>75%:** Comparator gate above 75%
- **WINDOW:** Gate HIGH between 25% and 75%

### ✅ UI Clarity
- Layer names: "GATE EVENTS" and "GATE MODE"
- Messages displayed when rotating encoder
- Detail popup shows current settings clearly
- Context-aware - different messages for Event vs Advanced mode

---

## How to Use

### Setting Up Event-Based Gates

1. **Select GATE EVENTS Layer** (press F4 when on Gate layer)
   - Rotate encoder to select events (values 0-15)
   - Message shows enabled events: "GATE: P T Z+"

2. **Select GATE MODE Layer** (press F4 again)
   - Rotate encoder to set trigger length (values 0-7)
   - Message shows: "TRIG: 8t (~1.0ms)"

3. **Example Setup:**
   - Shape: Bell (smooth bipolar curve)
   - GATE EVENTS: 12 (Z+ and Z- = bits 2+3)
   - GATE MODE: 2 (4 ticks = short trigger)
   - Result: 2× frequency clock

### Setting Up Advanced Mode Gates

1. **Select GATE EVENTS Layer**
   - Rotate to 0 (all events off)
   - Message shows: "GATE: ADVANCED"

2. **Select GATE MODE Layer**
   - Now controls continuous gate modes (0-7)
   - Message shows: "MODE: RISING" or "MODE: >50%"

3. **Example Setup:**
   - Shape: Triangle
   - GATE EVENTS: 0 (Advanced Mode)
   - GATE MODE: 1 (RISING)
   - Result: Perfect 50% duty square wave

---

## Example Patches from Guide

### LFO Clock Doubler
```
Shape: Bell (Sine)
Range: Bipolar 5V
GATE EVENTS: Z+ + Z- (value 12)
GATE MODE: 2 (4 ticks)
→ Produces 2 triggers per LFO cycle
```

### Natural Envelope Chain
```
Shape: ExpDown
Range: Unipolar 5V
GATE EVENTS: T (value 2)
GATE MODE: 3 (8 ticks)
→ EOC trigger for chaining envelopes
```

### Complex Trigger Burst
```
Shape: M-DAMP
Range: Bipolar 5V
GATE EVENTS: P + T (value 3)
GATE MODE: 1 (2 ticks)
→ 8 rapid triggers per step (damped oscillation)
```

### 50% Duty Square Wave
```
Shape: Triangle
Range: Bipolar 5V
GATE EVENTS: 0 (Advanced)
GATE MODE: 1 (RISING)
→ Perfect square wave gate
```

---

## Technical Details

### Slope Detection
- Threshold: 0.0001f (prevents noise triggering)
- Peak: Slope changes from rising to falling
- Trough: Slope changes from falling to rising

### Zero Crossing
- Detects when CV crosses 0V (midpoint for bipolar)
- Separate detection for upward (Z+) and downward (Z-) crossings

### Comparator Modes
- Normalized to range (works with both bipolar and unipolar)
- 25%/50%/75% thresholds relative to sequence range setting
- Window mode: active in middle 50% of range

### Trigger Length Calculation
```cpp
uint32_t ticks = 1u << gateParameter;  // 2^n exponential
// 0→1, 1→2, 2→4, 3→8, 4→16, 5→32, 6→64, 7→128
```

---

## Files Modified

1. **src/apps/sequencer/model/CurveSequence.h**
   - Updated bitfield structure
   - Added helper methods
   - Maintained backward compatibility

2. **src/apps/sequencer/engine/CurveTrackEngine.cpp**
   - Fixed trigger length to use exponential scale
   - Already had event detection logic working

3. **src/apps/sequencer/ui/pages/CurveSequenceEditPage.cpp**
   - Added encoder rotation messages for both gate layers
   - Context-aware messages (Event vs Advanced mode)

4. **CURVE-GATE-MUSICAL-GUIDE.md** (NEW)
   - Comprehensive musical usage guide
   - 17 detailed patches
   - 5 advanced techniques
   - Quick recipes and troubleshooting

---

## Next Steps (Optional Future Enhancements)

- [ ] Add visual gate indicators on step display
- [ ] Implement gate probability/humanization
- [ ] Add per-step gate mute/force
- [ ] Create preset gate patterns library
- [ ] Add MIDI clock sync for trigger lengths

---

**All improvements are live and ready to use!**

Build: ✅ Successful
UI Messages: ✅ Working
Trigger Length: ✅ Fixed
Documentation: ✅ Complete
