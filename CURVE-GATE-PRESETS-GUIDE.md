# Curve Track Gate Presets - Quick Reference

## How to Access

**Press: Page + Step 7** while on Curve Sequence Edit page

This opens the Gate Presets menu with 8 musically useful configurations.

---

## The 8 Presets

### 1. CLK x2 (Clock Doubler)
**Best for:** Bipolar sine/triangle LFOs
```
Events: ZERO UP + ZERO DOWN
Trigger Length: 4 ticks (~0.5ms)
```
**Result:** Produces 2 triggers per curve cycle - one on upward zero crossing, one on downward

**Musical Use:**
- Double the clock rate from an LFO
- Create polyrhythms (LFO at half tempo → triggers at actual tempo)
- Sync two sequencers at different rates

**Works Best With:**
- Bell (Sine) shapes
- Triangle shapes
- Any bipolar curve that crosses zero twice

---

### 2. EOC/EOR (End of Cycle/End of Rise)
**Best for:** Any curved shape with peaks and troughs
```
Events: PEAK + TROUGH
Trigger Length: 8 ticks (~1ms)
```
**Result:** Trigger at the top (peak) and bottom (trough) of the curve

**Musical Use:**
- Classic modular envelope triggers (EOC = end of cycle)
- Accent patterns on peaks
- Bounce/pluck effects on troughs
- Chaining envelopes

**Works Best With:**
- Bell shapes (smooth peaks/troughs)
- M-DAMP (multiple peaks = burst of triggers)
- Exponential curves (clean EOC trigger)

---

### 3. RISING (Rising Slope Gate)
**Best for:** Creating rhythmic gates from ramps
```
Events: ADVANCED MODE
Mode: RISING SLOPE
```
**Result:** Gate is HIGH while curve is rising, LOW while falling/flat

**Musical Use:**
- 50% duty cycle square wave from triangle
- VCA envelope follower
- Dynamic filter opening
- First-half-only trigger window

**Works Best With:**
- Triangle (perfect 50/50 split)
- RampUp (gate matches ramp)
- Any ascending shape

---

### 4. FALLING (Falling Slope Gate)
**Best for:** Decay envelopes and downward ramps
```
Events: ADVANCED MODE
Mode: FALLING SLOPE
```
**Result:** Gate is HIGH while curve is falling, LOW while rising/flat

**Musical Use:**
- Decay-phase gate
- Second-half-only trigger window
- Inverted envelope behavior
- Downward movement detection

**Works Best With:**
- RampDown shapes
- ExpDown (exponential decay)
- Any descending shape

---

### 5. PEAKS (Peaks Only)
**Best for:** Accent triggers at curve maxima
```
Events: PEAK
Trigger Length: 8 ticks (~1ms)
```
**Result:** Single trigger when curve reaches its peak (maximum value)

**Musical Use:**
- Half-speed clock (triangle = trigger every other cycle)
- Accent on loudest part of modulation
- End-of-rise detection
- Melodic note triggers at phrase peaks

**Works Best With:**
- Bell shapes (single clear peak)
- M-DAMP (4 peaks = 4 triggers)
- Triangle (peak at midpoint)

---

### 6. TROUGHS (Troughs Only)
**Best for:** End-of-cycle triggers
```
Events: TROUGH
Trigger Length: 8 ticks (~1ms)
```
**Result:** Single trigger when curve reaches its trough (minimum value)

**Musical Use:**
- Classic EOC (end of cycle) trigger
- Envelope chaining
- Start-of-next-cycle detection
- Bass drum on downbeat

**Works Best With:**
- ExpDown (clean EOC)
- RampUp (trough at start)
- Any shape returning to minimum

---

### 7. WINDOW (Window Detector)
**Best for:** Rhythmic gating at curve peaks
```
Events: ADVANCED MODE
Mode: WINDOW 25-75%
```
**Result:** Gate is HIGH only when curve is in middle 50% of its range

**Musical Use:**
- Rhythmic gate patterns from LFOs
- VCA control (only loud during peaks)
- Filter opening only at maxima
- Center-focused modulation

**Works Best With:**
- Bell shapes (gate during peak)
- Triangle (gate during middle portions)
- Any shape with clear peaks

---

### 8. CLEAR (Clear Gates)
**Best for:** Removing all gate configurations
```
Events: 0 (none)
Parameter: 0 (off)
```
**Result:** All gate outputs disabled

**Musical Use:**
- Quick reset of gate settings
- A/B testing (clear → apply new preset)
- CV-only tracks (no gates needed)

---

## How to Use

### Method 1: Apply to Selected Steps
1. **Select steps** (hold steps 1-4 for example)
2. **Press Page + Step 7**
3. **Choose preset** from menu
4. **Result:** Preset applied only to selected steps

**Example:**
```
Steps 1-4: Select and apply "CLK x2"
Steps 5-8: Select and apply "PEAKS"
→ Creates two different rhythmic patterns
```

### Method 2: Apply to Entire Sequence
1. **Don't select any steps**
2. **Press Page + Step 7**
3. **Choose preset** from menu
4. **Result:** Preset applied to all steps in loop range (First Step → Last Step)

**Example:**
```
First Step: 1
Last Step: 16
No selection → Page + Step 7 → "EOC/EOR"
→ All 16 steps get Peak + Trough triggers
```

---

## Musical Workflows

### Workflow 1: Polyrhythmic Triggers
```
Step 1: Bell shape
        → Apply "CLK x2" preset
        → Triggers at 2× rate

Step 5: Bell shape
        → Apply "PEAKS" preset
        → Triggers at 1× rate

Result: 2:1 polyrhythm
```

### Workflow 2: Envelope Chain
```
Step 1: ExpDown shape
        → Apply "TROUGHS" preset
        → EOC trigger at end of decay

Patch: Gate Out → next envelope trigger
Result: Auto-chaining envelopes
```

### Workflow 3: Dynamic Accent Pattern
```
Steps 1-16: Varying MIN/MAX values
            → Apply "WINDOW" preset
            → Only loud steps produce gates

Result: Natural accents without probability
```

### Workflow 4: Rhythmic Gate Patterns
```
Steps 1-4: Triangle shape
           → Apply "RISING" preset
           → 50% duty gate

Steps 5-8: Triangle shape
           → Apply "FALLING" preset
           → Inverted 50% duty gate

Result: Phase-offset rhythmic pattern
```

---

## Preset Combinations

### Combination 1: Dual Trigger Streams
```
Track 1: Bell + "CLK x2"     → Zero crossings
Track 2: Bell + "EOC/EOR"    → Peaks/Troughs

Result: 2 different trigger patterns from same curve
```

### Combination 2: Ratcheting with M-DAMP
```
Step 1: M-DAMP shape
        → Apply "EOC/EOR" preset

Result: 8 rapid triggers per step (damped oscillation)
        → Natural ratchet/roll effect
```

### Combination 3: Progressive Complexity
```
Steps 1-4:   "PEAKS" only        → Simple quarter notes
Steps 5-8:   "EOC/EOR"           → Double triggers
Steps 9-12:  "CLK x2"            → Even faster
Steps 13-16: "CLEAR"             → Silence (breakdown)

Result: Evolving rhythmic complexity
```

---

## Quick Tips

1. **Start with CLK x2 or EOC/EOR** - Most universally useful
2. **Use CLEAR first** if changing presets - Clean slate
3. **Select steps before applying** - Precise control over which steps get preset
4. **Combine with Shape Variation** - Add organic variation to rigid presets
5. **Use WINDOW with varying amplitudes** - Natural accent without probability
6. **RISING + FALLING on alternate steps** - Phase-locked gates
7. **Test with single Bell step first** - Understand behavior before multi-step
8. **Remember trigger length** - CLK x2 uses short triggers, EOC/EOR uses medium

---

## Preset Reference Table

| Preset | Events | Parameter | Tick Length | Best Shape | Use Case |
|--------|--------|-----------|-------------|------------|----------|
| **CLK x2** | Zero Up + Down | 2 (4 ticks) | ~0.5ms | Bipolar LFO | Clock doubling |
| **EOC/EOR** | Peak + Trough | 3 (8 ticks) | ~1.0ms | Bell, M-DAMP | Envelope triggers |
| **RISING** | Advanced | Rising Slope | Continuous | Triangle, Ramp | Rhythmic gates |
| **FALLING** | Advanced | Falling Slope | Continuous | Ramp Down, Exp | Decay gates |
| **PEAKS** | Peak | 3 (8 ticks) | ~1.0ms | Bell, Triangle | Accents |
| **TROUGHS** | Trough | 3 (8 ticks) | ~1.0ms | Exp Down, Ramp | EOC triggers |
| **WINDOW** | Advanced | Window 25-75% | Continuous | Bell, Triangle | Rhythmic VCA |
| **CLEAR** | 0 | 0 | - | Any | Reset gates |

---

## Troubleshooting

**Q: Preset applied but no triggers?**
- Check curve is actually moving (MIN ≠ MAX)
- For Zero crossings, ensure Bipolar range
- For Peaks/Troughs, ensure curve has peaks/troughs
- Verify not muted

**Q: Too many triggers?**
- Use PEAKS or TROUGHS instead of EOC/EOR
- Use WINDOW instead of CLK x2
- Apply to fewer steps

**Q: Preset applied to wrong steps?**
- Selection persists! Clear selection (press all steps again)
- Check First Step / Last Step range

**Q: Want different trigger length?**
- Apply preset first
- Then manually adjust GATE MODE layer for specific steps
- Presets are starting points, customize as needed!

---

**Access: Page + Step 7 on Curve Sequence Edit page**
