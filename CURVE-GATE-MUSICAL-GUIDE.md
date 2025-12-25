# Curve Track Gate Events - Musical Guide

This guide shows how to create musical results using different curve shapes combined with gate event parameters on the **PEW|FORMER** Curve Track.

## Understanding the Two Gate Layers

### Layer 1: GATE EVENTS (Event Mask)
Controls **which slope events trigger gates**. You can enable multiple events simultaneously:
- **P** (Peak) - End of Rise - trigger when curve reaches maximum
- **T** (Trough) - End of Cycle - trigger when curve reaches minimum
- **Z+** (Zero Cross ↑) - trigger when crossing zero upward
- **Z-** (Zero Cross ↓) - trigger when crossing zero downward
- **0000** (All off) - Enables Advanced Mode

### Layer 2: GATE MODE (Parameter)
**Context-aware** - behavior changes based on GATE EVENTS setting:

**When GATE EVENTS ≠ 0 (Event Mode):**
- Controls **trigger length** in exponential scale
- Values: 0→1 tick, 1→2, 2→4, 3→8, 4→16, 5→32, 6→64, 7→128 ticks
- Example: Value 3 = 8 ticks ≈ 1ms trigger

**When GATE EVENTS = 0 (Advanced Mode):**
- Selects continuous gate modes:
  - 0: OFF - no gate output
  - 1: RISING - gate HIGH while curve rising
  - 2: FALLING - gate HIGH while curve falling
  - 3: MOVING - gate HIGH while curve changing (rising OR falling)
  - 4: >25% - comparator gate when above 25%
  - 5: >50% - comparator gate when above 50%
  - 6: >75% - comparator gate when above 75%
  - 7: WINDOW - gate HIGH when between 25% and 75%

---

## Musical Patches by Curve Shape

### 1. SINE WAVE (Bell/Smooth)

**Patch: Dual Clock from LFO**
- Shape: Bell or Smooth curves
- Range: Bipolar 5V
- GATE EVENTS: Z+ + Z- (bits 2+3 = value 12)
- GATE MODE: 2 (4 ticks = short trigger)
- **Result:** 2× frequency clock - each LFO cycle produces 2 triggers
- **Use:** Clock divisions, polyrhythms, triggering envelopes at double rate

**Patch: Peak/Trough for Accent Pattern**
- Shape: Bell
- Range: Bipolar 5V
- GATE EVENTS: P + T (bits 0+1 = value 3)
- GATE MODE: 4 (16 ticks = medium trigger)
- **Result:** 2 triggers per cycle at peaks and troughs
- **Use:** Accent triggers for drums, modulation resets

**Patch: Window Detector for Rhythmic Gate**
- Shape: Bell
- Range: Bipolar 5V
- GATE EVENTS: 0 (Advanced Mode)
- GATE MODE: 7 (WINDOW)
- **Result:** Gate HIGH only in middle portion of sine wave
- **Use:** Rhythmic gate patterns, VCA control, filter modulation

---

### 2. TRIANGLE WAVE

**Patch: 50% Duty Cycle Clock**
- Shape: Triangle
- Range: Bipolar 5V
- GATE EVENTS: 0 (Advanced Mode)
- GATE MODE: 1 (RISING)
- **Result:** Perfect 50% duty square wave
- **Use:** Clean clock signal, rhythmic gate

**Patch: Alternating Triggers**
- Shape: Triangle
- Range: Bipolar 5V
- GATE EVENTS: P only (bit 0 = value 1)
- GATE MODE: 3 (8 ticks)
- **Result:** Single trigger at each peak
- **Use:** Half-speed clock, alternating pattern triggers

**Patch: Quadrature Outputs**
- Step 1: Triangle, GATE EVENTS = Z+ (bit 2 = value 4)
- Step 2: Triangle, GATE EVENTS = Z- (bit 3 = value 8)
- **Result:** Two phase-offset trigger streams
- **Use:** Polyrhythmic patterns, dual sequencer clocking

---

### 3. SAWTOOTH / RAMP (RampUp/RampDown)

**Patch: End-of-Cycle Trigger**
- Shape: RampUp
- Range: Bipolar 5V
- GATE EVENTS: T only (bit 1 = value 2)
- GATE MODE: 2 (4 ticks = short)
- **Result:** Single trigger at end of ramp (classic EOC)
- **Use:** Triggering next stage in envelope chains, sequencer advance

**Patch: Ramp-Up Gate**
- Shape: RampUp
- Range: Unipolar 5V
- GATE EVENTS: 0 (Advanced Mode)
- GATE MODE: 1 (RISING)
- **Result:** Gate HIGH during entire ramp-up
- **Use:** VCA envelope follower, increasing modulation depth

**Patch: Rising Edge Detector**
- Shape: RampUp
- Range: Bipolar 5V
- GATE EVENTS: Z+ (bit 2 = value 4)
- GATE MODE: 5 (32 ticks = longer trigger)
- **Result:** Trigger when ramp crosses zero (middle of ramp)
- **Use:** Mid-ramp events, polyrhythmic subdivisions

---

### 4. EXPONENTIAL CURVES (ExpUp/ExpDown)

**Patch: Natural Envelope with EOC**
- Shape: ExpDown
- Range: Unipolar 5V
- GATE EVENTS: T (bit 1 = value 2)
- GATE MODE: 3 (8 ticks)
- **Result:** Classic "ping" envelope with EOC trigger
- **Use:** Chaining envelopes, triggering next voice

**Patch: Attack Trigger**
- Shape: ExpUp
- Range: Unipolar 5V
- GATE EVENTS: P (bit 0 = value 1)
- GATE MODE: 2 (4 ticks = short)
- **Result:** Trigger at end of attack phase
- **Use:** Triggering decay phase, secondary envelope

**Patch: Threshold-Based Gate**
- Shape: ExpDown
- Range: Unipolar 5V
- GATE EVENTS: 0 (Advanced Mode)
- GATE MODE: 5 (>50%)
- **Result:** Gate HIGH while envelope is above 50%
- **Use:** Controlling filter or VCA during sustain

---

### 5. MACRO CURVES (M-DAMP, M-FM, M-BOUNCE)

**Patch: Complex Trigger Pattern from M-DAMP**
- Shape: M-DAMP (damped oscillation, ~4 cycles)
- Range: Bipolar 5V
- GATE EVENTS: P + T (bits 0+1 = value 3)
- GATE MODE: 1 (2 ticks = very short)
- **Result:** 8 triggers per step (4 peaks + 4 troughs)
- **Use:** Rapid trigger bursts, roll/flam effects, complex rhythms

**Patch: Frequency Sweep with Event Triggers**
- Shape: M-FM (frequency modulated triangle)
- Range: Bipolar 5V
- GATE EVENTS: Z+ + Z- (bits 2+3 = value 12)
- GATE MODE: 2 (4 ticks)
- **Result:** Accelerating trigger pattern as FM sweep progresses
- **Use:** Rhythmic acceleration effects, generative patterns

**Patch: Bounce Envelope Chain**
- Shape: M-BOUNCE
- Range: Unipolar 5V
- GATE EVENTS: P (bit 0 = value 1)
- GATE MODE: 4 (16 ticks)
- **Result:** Multiple triggers with decreasing amplitude
- **Use:** Bouncing ball effects, decay triggers

---

### 6. STEP CURVES (StepUp/StepDown)

**Patch: Square Wave Gate**
- Shape: StepUp
- Range: Bipolar 5V
- GATE EVENTS: 0 (Advanced Mode)
- GATE MODE: 6 (>75%)
- **Result:** Gate HIGH during top half of square
- **Use:** Clean square wave for clocking, S&H triggers

**Patch: Edge Triggers Only**
- Shape: StepUp
- Range: Bipolar 5V
- GATE EVENTS: Z+ + Z- (bits 2+3 = value 12)
- GATE MODE: 1 (2 ticks = very short)
- **Result:** Sharp triggers at both edges of square wave
- **Use:** Precise timing triggers, clock edges

---

## Advanced Patching Techniques

### Technique 1: Polyrhythmic Triggers
Use multiple steps with different shapes and gate settings:
- **Step 1:** Triangle, GATE EVENTS = P, GATE MODE = 3 → triggers every 2 cycles
- **Step 2:** Bell, GATE EVENTS = P+T, GATE MODE = 2 → triggers every cycle (2× rate)
- **Result:** 3:2 polyrhythm

### Technique 2: Phase-Locked Triggers
Use Global Phase offset with gate events:
- **Track 1:** Bell, GATE EVENTS = Z+, GATE MODE = 3
- **Track 2:** Same, but Global Phase = 0.25
- **Result:** Two trigger streams offset by 90°

### Technique 3: Dynamic Accent Patterns
Use comparator modes with varying curve amplitudes:
- Set MIN/MAX to create varying amplitudes across steps
- Use GATE EVENTS = 0, GATE MODE = 5 (>50%)
- **Result:** Only loudest steps trigger gates (natural accents)

### Technique 4: Ratcheting with Macro Shapes
Use M-DAMP for automatic ratchets:
- Shape: M-DAMP
- GATE EVENTS: P + T
- GATE MODE: 0 (1 tick = very fast)
- **Result:** Up to 8 rapid triggers per step (natural decay)

### Technique 5: Envelope Following
Use slope detection for dynamic response:
- GATE EVENTS: 0 (Advanced Mode)
- GATE MODE: 3 (MOVING)
- **Result:** Gate active whenever modulation is changing
- **Use:** VCA sidechain, dynamic filtering

---

## Trigger Length Reference

Trigger length is **exponential** for musical usefulness:

| Value | Ticks | ~Time @ 120BPM | Musical Use |
|-------|-------|----------------|-------------|
| 0 | 1 | 0.13ms | Ultra-short blip, sample triggers |
| 1 | 2 | 0.26ms | Short trigger, fast envelopes |
| 2 | 4 | 0.52ms | Standard trigger, snappy response |
| 3 | 8 | 1.0ms | Medium trigger, most versatile |
| 4 | 16 | 2.1ms | Long trigger, slower envelopes |
| 5 | 32 | 4.2ms | Very long, gate-like |
| 6 | 64 | 8.3ms | Extended gate |
| 7 | 128 | 16.7ms | Maximum length, rhythmic gates |

**Note:** Actual time depends on tempo. At 120 BPM: 1 tick ≈ 0.13ms

---

## Quick Recipes

### Recipe: LFO Clock Doubler
1. Select GATE EVENTS layer (F4)
2. Set to Z+ + Z- (rotate to 12)
3. Select GATE MODE layer (F4 again)
4. Set to 2 (4 tick trigger)
5. Use bipolar sine/triangle shape
6. **Result:** 2× frequency clock

### Recipe: Natural Envelope Chain
1. Use ExpDown shape
2. Set GATE EVENTS to T (rotate to 2)
3. Set GATE MODE to 3 (8 tick trigger)
4. Patch gate output to trigger next envelope
5. **Result:** Classic envelope chain

### Recipe: Rhythmic Bursts
1. Use M-DAMP shape
2. Set GATE EVENTS to P+T (rotate to 3)
3. Set GATE MODE to 1 (2 tick triggers)
4. **Result:** 8-trigger burst per step

### Recipe: Probability-Free Accents
1. Vary MIN/MAX across steps (some loud, some quiet)
2. Set GATE EVENTS to 0 (Advanced Mode)
3. Set GATE MODE to 5 (>50%)
4. **Result:** Only loud steps produce gates

---

## Tips for Musical Results

1. **Start Simple:** Begin with single event (P or T) before combining
2. **Match to Shape:** Use P/T for smooth curves, Z+/Z- for bipolar LFOs
3. **Trigger Length Matters:** Shorter (0-2) for percussive, longer (4-6) for melodic
4. **Combine with Probability:** Use Shape Probability to add variation
5. **Use Multiple Tracks:** Different gate patterns on different tracks = polyrhythms
6. **Experiment with Chaos:** Add chaos amount to introduce organic variation
7. **Watch the Range:** Bipolar curves work best with zero-crossing events
8. **Use Macro Shapes:** M-DAMP and M-BOUNCE create complex trigger patterns automatically

---

## Troubleshooting

**Problem: No triggers appearing**
- Check GATE EVENTS is not all zeros (unless using Advanced Mode)
- Verify curve is actually moving (check MIN/MAX values)
- For Z+/Z-, ensure using Bipolar range

**Problem: Too many triggers**
- Reduce number of enabled events in GATE EVENTS
- Use Advanced Mode with comparator instead of event detection
- Increase trigger spacing using longer divisor

**Problem: Triggers feel late/early**
- Adjust Global Phase to shift trigger timing
- Use different event (Peak vs Zero Cross) for earlier/later timing
- Consider slope threshold - very slow curves may not trigger

**Problem: Inconsistent triggering**
- Check slope threshold - very gentle curves may not detect peaks
- Increase curve depth (larger MIN/MAX difference)
- Use comparator mode (Advanced) instead of slope detection

---

**Enjoy exploring the intersection of curves and gates!**
