# Curve Studio Manual

## 1. Introduction

**Curve Studio** represents a major overhaul of the Curve Track engine, transforming it from a simple LFO sequencer into a complex, "wavetable-style" modulation source. This manual covers the new signal processing chain, generative tools, and advanced playback features.

## 2. Signal Processor

The signal path has been expanded to include Chaos, Wavefolding, and Filtering.

### 2.1 Signal Flow
`Step Shape` -> `Chaos` -> `Wavefolder` -> `Filter` -> `Crossfade` -> `Limiter` -> `Output`

### 2.2 Wavefolder
- **Access**: Press **F5** to cycle to **WAVEFOLDER**.
- **FOLD**: Controls the amount of wavefolding (sine-based folding). Range: 0.00-1.00.
- **GAIN**: Input gain before the folder. Range: 0.00-2.00 (0% to 200%).

### 2.3 DJ Filter
- **FILTER**: One-knob filter control.
  - **Center (0.00)**: Filter off.
  - **Negative**: Low Pass Filter (LPF).
  - **Positive**: High Pass Filter (HPF).

### 2.4 Crossfade (XFADE)
- **XFADE**: Blends between the dry signal and the processed (folded/filtered) signal.

## 3. Playback Modes & Signal Flow

The Curve Track engine supports two fundamentally different playback modes: **Aligned** (grid-locked) and **Free** (FM-capable).

### 3.1 Aligned Mode Signal Flow

Grid-locked timing synchronized to the sequencer clock. Steps advance at exact divisor boundaries.

```
┌─────────────────────────────────────────────────────────────────────┐
│                        ALIGNED MODE TICK                             │
└─────────────────────────────────────────────────────────────────────┘

  Sequencer Tick
       │
       ├──> divisor = sequence.divisor() × 4        (e.g., 2 × 4 = 8 ticks)
       ├──> resetDivisor = resetMeasure × measureDiv
       │
       ▼
  relativeTick = tick % resetDivisor
       │
       ├──> if (relativeTick == 0) → reset()
       │
       ▼
  if (relativeTick % divisor == 0)  ← GRID BOUNDARY
       │
       ├──> advanceAligned()  ← Advance to next step
       ├──> triggerStep()     ← Trigger gate events
       │
       ▼
  updateOutput(relativeTick, divisor)
       │
       ├──> stepFraction = (relativeTick % divisor) / divisor
       │                   └─> 0.0 to 1.0 within step
       │
       ├──> if (direction < 0) → invert fraction  ← Reverse playback
       │
       ▼
  Lookup step shape at fraction
       │
       └──> [Continue to Signal Processing Chain]
```

**Key Characteristics:**
- Steps advance **only** at `relativeTick % divisor == 0`
- Perfectly quantized to sequencer grid
- No speed modulation possible
- Step duration = `divisor` ticks (always)

---

### 3.2 Free Mode Signal Flow (FM-Capable)

Phase-accumulator based timing with smooth speed modulation via **Curve Rate**.

```
┌─────────────────────────────────────────────────────────────────────┐
│                         FREE MODE TICK                               │
└─────────────────────────────────────────────────────────────────────┘

  Sequencer Tick
       │
       ├──> divisor = sequence.divisor() × 4
       │
       ▼
  if (_sequenceState.step() < 0)  ← Initial trigger
       │
       ├──> advanceFree()   ← Start sequence immediately
       ├──> triggerStep()
       │
       ▼
  ┌─────────────────────────────────────────────┐
  │     PHASE ACCUMULATOR (FM Engine)           │
  └─────────────────────────────────────────────┘
       │
       ├──> curveRate = track.curveRate()      ← 0.0 to 4.0
       ├──> baseSpeed = 1.0 / divisor          ← Base step duration
       ├──> speed = baseSpeed × curveRate
       │             └─> Example: 1/8 × 2.0 = 0.25 phase/tick
       │
       ├──> QUALITY LIMIT: min 8 ticks/step   ← Anti-aliasing
       │     speed = min(speed, 1.0 / 8.0)
       │             └─> Guarantees clean waveform reproduction
       │
       ├──> speed = min(speed, 0.5)           ← Nyquist clamping
       │             └─> Prevents step skipping (max 0.5 phase/tick)
       │
       ▼
  _freePhase += speed                          ← Increment every tick
       │
       ├──> if (_freePhase >= 1.0)            ← Step boundary crossed
       │         │
       │         ├──> _freePhase -= 1.0       ← Wrap phase
       │         ├──> advanceFree()           ← Advance to next step
       │         └──> triggerStep()           ← Trigger gates
       │
       ▼
  updateOutput(relativeTick, divisor)
       │
       ├──> stepFraction = _freePhase         ← Use phase (0.0-1.0)
       │                   └─> Smooth, continuous
       │
       ├──> if (direction < 0) → invert fraction
       │
       ▼
  Lookup step shape at fraction
       │
       └──> [Continue to Signal Processing Chain]
```

**Key Characteristics:**
- Steps advance **when phase crosses 1.0** (not grid-locked)
- Speed modulation range: **0x to 4x** (via curveRate)
- **Quality limit:** minimum 8 ticks/step (prevents aliasing/undersampling)
- **Nyquist limit:** max 0.5 phase/tick (prevents step skipping)
- **1kHz interpolation:** smooth output between tick samples
- Smooth, glitch-free FM modulation

**Curve Rate Values:**
```
curveRate = 0.0   →  Freeze (no movement)
curveRate = 0.5   →  Half speed
curveRate = 1.0   →  Normal speed (default)
curveRate = 2.0   →  Double speed
curveRate = 4.0   →  Quadruple speed (Nyquist-limited)
```

**Step Duration Calculation:**
```
Ticks per step = divisor / curveRate (minimum 8 ticks enforced)

Examples with Quality Limit:
  divisor = 2:
    curveRate = 1.0  →  8 ticks/step   (quality limit reached)
    curveRate = 2.0  →  8 ticks/step   (clamped to 1.0x for quality)
    curveRate = 4.0  →  8 ticks/step   (clamped to 1.0x for quality)
    Effective max rate: 1.0x

  divisor = 8:
    curveRate = 1.0  →  8 ticks/step   (normal)
    curveRate = 2.0  →  8 ticks/step   (clamped to 1.0x for quality)
    curveRate = 4.0  →  8 ticks/step   (clamped to 1.0x for quality)
    Effective max rate: 1.0x

  divisor = 32:
    curveRate = 1.0  →  32 ticks/step  (normal)
    curveRate = 2.0  →  16 ticks/step  (double speed)
    curveRate = 4.0  →  8 ticks/step   (quadruple speed, quality limit)
    Effective max rate: 4.0x

  divisor = 64:
    curveRate = 1.0  →  64 ticks/step  (normal)
    curveRate = 2.0  →  32 ticks/step  (double speed)
    curveRate = 4.0  →  16 ticks/step  (quadruple speed)
    Effective max rate: 4.0x (very smooth)
```

---

### 3.3 Complete Signal Processing Chain

Both playback modes feed into the same signal processing chain:

```
┌─────────────────────────────────────────────────────────────────────┐
│                   SIGNAL PROCESSING CHAIN                            │
└─────────────────────────────────────────────────────────────────────┘

  Step Lookup (currentStep, stepFraction)
       │
       ├──> Evaluate step shape at fraction → value (0.0-1.0)
       │     └─> Uses Curve::function(shapeType)
       │
       ├──> Apply min/max scaling
       │     └─> scaled = min + value × (max - min)
       │
       ▼
  ┌──────────────────────────────────┐
  │  1. CHAOS MIX (Crossfade)        │
  └──────────────────────────────────┘
       │
       ├──> if (chaosAmount > 0)
       │     │
       │     ├──> Get chaos value (-1.0 to +1.0)
       │     │     └─> Latoocarfian or Lorenz
       │     │
       │     ├──> normalizedChaos = (chaos + 1.0) × 0.5  (0.0-1.0)
       │     │
       │     ├──> Apply range offset:
       │     │     └─> Below: -0.25  (wiggles around 0.25)
       │     │     └─> Mid:    0.0   (wiggles around 0.5)
       │     │     └─> Above: +0.25  (wiggles around 0.75)
       │     │
       │     └──> value = value × (1 - amt) + chaos × amt
       │
       ▼
  originalValue = denormalize(value)  ← Save for XFADE
       │
  ┌──────────────────────────────────┐
  │  2. WAVEFOLDER (Optional)        │
  └──────────────────────────────────┘
       │
       ├──> if (fold > 0)
       │     │
       │     ├──> gain = 1.0 + (wavefolderGain × 2.0)
       │     ├──> fold_exp = fold × fold
       │     └──> value = applyWavefolder(value, fold_exp, gain)
       │           └─> Sine-based folding
       │
       ▼
  voltage = denormalize(value)
       │
  ┌──────────────────────────────────┐
  │  3. DJ FILTER (Always Active)    │
  └──────────────────────────────────┘
       │
       ├──> filterControl = djFilter  (-1.0 to +1.0)
       │     │
       │     ├──> if (< 0): LPF mode
       │     └──> if (> 0): HPF mode
       │
       └──> voltage = applyDjFilter(voltage, lpfState, control)
       │
       ▼
  ┌──────────────────────────────────┐
  │  4. CROSSFADE (Dry/Wet Mix)      │
  └──────────────────────────────────┘
       │
       ├──> xFade = sequence.xFade()  (0.0 to 1.0)
       │     └─> Non-routable, UI-only control
       │
       └──> voltage = originalValue × (1 - xFade) + voltage × xFade
       │              └───── dry ─────┘           └──── wet ────┘
       │
       ▼
  ┌──────────────────────────────────┐
  │  5. LIMITER (Hard Clamp)         │
  └──────────────────────────────────┘
       │
       └──> cvOutputTarget = clamp(voltage, -5.0, +5.0)
       │
       └──> prevCvOutputTarget saved  ← For interpolation
       │
       └──> [END OF TICK @ 384 Hz]
       │
       ▼
  ┌─────────────────────────────────────────────────────────┐
  │  6. 1kHz INTERPOLATION (in update(), ~1000 Hz)          │
  └─────────────────────────────────────────────────────────┘
       │
       ├──> tickPhase += dt / tickDuration  ← Fraction through tick (0.0-1.0)
       │
       └──> interpolatedTarget = prevCvOutputTarget +
                                 (cvOutputTarget - prevCvOutputTarget) × tickPhase
       │              └─> Linear interpolation between tick samples
       │
       ▼
  ┌──────────────────────────────────┐
  │  7. SLIDE (Slew Limiter)         │
  └──────────────────────────────────┘
       │
       ├──> if (slideTime > 0)
       │     └──> cvOutput = applySlide(cvOutput, interpolatedTarget + offset, slideTime, dt)
       │
       ├──> else
       │     └──> cvOutput = interpolatedTarget + offset
       │
       ▼
  CV OUTPUT → DAC @ 1kHz
```

---

### 3.4 Gate Generation Logic

Gate outputs are generated dynamically based on curve events:

```
┌─────────────────────────────────────────────────────────────────────┐
│                       GATE GENERATION                                │
└─────────────────────────────────────────────────────────────────────┘

  cvOutputTarget (current voltage)
       │
       ├──> slope = current - prevCvOutput
       │
       ├──> isRising  = (slope > threshold)
       ├──> isFalling = (slope < -threshold)
       │
       ▼
  Step Gate Mask (0-255 bitmask)
       │
       ├──> Bit 0: Zero Cross Rising (ZC+)
       ├──> Bit 1: Zero Cross Falling (ZC-)
       ├──> Bit 2: Peak (local maximum)
       ├──> Bit 3: Trough (local minimum)
       ├──> Bit 4: Rising Edge
       ├──> Bit 5: Falling Edge
       ├──> Bit 6: Above 50%
       ├──> Bit 7: Below 50%
       │
       ▼
  Detect Events:
       │
       ├──> Zero Cross: (prev < 0) && (current >= 0)
       ├──> Peak:       wasRising && !isRising
       ├──> Trough:     wasFalling && !isFalling
       ├──> Rising:     isRising
       ├──> Falling:    isFalling
       ├──> >50%:       voltage > midpoint
       │
       ▼
  if (mask & event)
       │
       ├──> Trigger gate (duration based on probability)
       │
       └──> gateOutput = HIGH for N ticks
                         └─> N = f(gateProbability)
```

**Gate Probability Scale:**
```
gateProbability = -1  →  Always OFF
gateProbability =  0  →  12.5% chance (1 tick)
gateProbability =  3  →  50% chance (4 ticks)
gateProbability =  7  →  100% chance (8 ticks)
```

---

### 3.5 Playback Mode Comparison

| Aspect | Aligned Mode | Free Mode |
|--------|--------------|-----------|
| **Step Advance** | Grid-locked (divisor boundaries) | Phase-based (smooth FM) |
| **Timing** | Quantized | Continuous |
| **Speed Control** | Fixed (divisor only) | Variable (curveRate 0-4x) |
| **FM Modulation** | Not possible | Full support via routing |
| **Step Duration** | `divisor` ticks (constant) | `divisor / curveRate` ticks (min 8) |
| **Output Rate** | 1kHz (interpolated) | 1kHz (interpolated) |
| **Quality Limit** | N/A | Min 8 ticks/step (anti-aliasing) |
| **Use Case** | Rhythmic, quantized LFOs | Smooth audio-rate FM, drones |
| **Aliasing Risk** | None | Quality-limited + 1kHz interpolation |

### 3.6 Routing Curve Rate

The **Curve Rate** parameter can be modulated via CV routing for dynamic FM effects:

**Routing Range:** 0 to 400
- **0** → 0.0x (freeze)
- **100** → 1.0x (normal speed, default)
- **200** → 2.0x (double speed)
- **400** → 4.0x (quadruple speed)

**Effective Range (depends on divisor):**
```
divisor = 2:   Max effective = 1.0x  (quality-limited)
divisor = 8:   Max effective = 1.0x  (quality-limited)
divisor = 32:  Max effective = 4.0x
divisor = 64:  Max effective = 4.0x  (smoothest)
```

**Example Routing:**
- **CV1 → Curve Rate** with min=100, max=400
  - CV at 0V → 1.0x speed (normal)
  - CV at +5V → 4.0x speed (or quality limit)

**Quality Guarantees:**

The combination of quality limiting and 1kHz interpolation ensures clean output:

1. **Quality Limit (8 ticks/step minimum)**
   - Prevents severe undersampling at low divisor values
   - Ensures bell/sine curves render with at least 8 samples per cycle
   - Automatically caps speed based on divisor for clean waveform reproduction

2. **1kHz Interpolation**
   - Smooths tick-rate samples (384 Hz @ 120 BPM) to 1kHz output
   - Eliminates staircase artifacts
   - Works even without slide/slew enabled
   - Provides smooth FM modulation at all speeds

**Result:** You can safely use fast LFO rates (divisor=32, rate=4x) for audio-rate FM without aliasing or clicking artifacts.

**Curve Rate Routing Setup:**
- Target: Curve Rate (available in routing menu)
- Source: CV Input, MIDI CC, or other modulation source
- Min/Max: Set range for modulation depth (e.g., 100-200 for 1x to 2x speed)
- Track Selection: Apply to specific curve tracks only
- Bias/Depth: Use per-track bias/depth for individual control

### 3.7 Play Mode Configuration

The Curve track now supports different play modes that determine how the sequence advances:

**Free Mode**:
- Uses the phase accumulator system described above
- Allows for FM modulation via Curve Rate
- Provides continuous, non-grid-locked timing
- Best for audio-rate FM and smooth modulation

**Aligned Mode**:
- Grid-locked to sequencer clock
- Steps advance at divisor boundaries
- Quantized timing perfect for rhythmic LFOs
- No speed modulation possible (Curve Rate has no effect)

**Setting Play Mode**:
- Access via Track settings page (Shift+S3)
- Select "Play Mode" parameter
- Choose between "Free" or "Aligned"

## 4. Chaos Engine

A generative engine inserted *before* the wavefolder.

- **Access**: Press **F5** to cycle to **CHAOS**.
- **Controls**:
  - **AMT**: Modulation depth (0-100%).
  - **HZ**: Evolution speed (0.01Hz-50Hz, default ~0.7Hz).
  - **HEAT**: Chaos intensity parameter (0-100, default: 45).
  - **DAMP**: Chaos damping parameter (0-100, default: 62).

### 4.1 Chaos Range Offset
- **Function**: Shifts the centerpoint of chaos wiggling within the voltage range.
- **Access**: Hold **Shift + F1** to cycle through range modes.
- **Modes**:
  - **Mid**: Wiggles around centerpoint (0V for bipolar ranges, +2.5V for unipolar 5V).
  - **Below**: Wiggles around bottom quarter (e.g., -2.5V for bipolar 5V, peaks at 0V, bottoms at -5V).
  - **Above**: Wiggles around top quarter (e.g., +2.5V for bipolar 5V, dips to 0V, peaks at +5V).
- **Use Case**: Asymmetric chaos modulation - chaos affects only positive or negative swings when mixed with bipolar shapes.

### 4.2 Chaos Algorithms
- **Toggle**: Hold **Shift + F2** to switch between algorithms.
- **Latoocarfian**: Stepped/Sample&Hold character, chaotic jumps.
- **Lorenz**: Smooth/Attractor character, flowing modulation.

### 4.3 Chaos Engine Enhancements
Recent updates to the chaos engine include:

**Above/Below Engine Integration**:
- Enhanced chaos behavior with range control
- Improved voltage range handling for more musical results
- Better integration with curve shapes and wavefolding

**Crossfade Original Shape**:
- Option to blend between original shape and chaos-modified shape
- Access via XFADE parameter when in chaos mode
- Allows for subtle chaos application without complete signal transformation

## 5. Advanced Playback

### 5.1 Global Phase
- **Function**: Offsets the playback position of the sequence (0-100%).
- **Access**: Press **F5** to cycle to **PHASE**.
- **Use Case**: Phase-shifting LFOs, canon effects, fine-tuning timing.

### 5.2 True Reverse Playback
The engine now supports "True Reverse" playback for shapes.
- **Backward Mode**: Steps play in reverse order (4, 3, 2...), AND the internal shape plays backwards (100% -> 0%).
- **Pendulum/PingPong**: Shapes play forward on the "up" phase and backward on the "down" phase, creating perfectly smooth loops.

### 5.3 Min/Max Inversion
You can now create inverted signal windows by setting **Min > Max**.
- **Normal**: Min=0, Max=255 (Signal goes UP).
- **Inverted**: Min=255, Max=0 (Signal goes DOWN).
- **Step**: The step shape scales from "Start" (Min) to "End" (Max).

## 6. Curve Studio Workflow

Curve Studio introduces powerful context menus for populating sequences.

### 6.1 LFO Menu (Step 6)
Populate steps with single-cycle waveforms. Defaults to active loop range or selection.

- **Access**: Press **Page + Step 6** (Button 6).
- **Options**:
  - **TRI**: Triangle (1 cycle/step).
  - **SINE**: Sine / Bell (1 cycle/step).
  - **SAW**: Sawtooth / Ramp (1 cycle/step).
  - **SQUA**: Square / Pulse (1 cycle/step).
  - **MM-RND**: Randomize Min/Max values for each step (supports inversion).

### 6.2 Macro Shape Menu (Step 5)
"Draw" complex shapes that span multiple steps. Automatically handles selection logic (Single step -> to end of loop).

- **Access**: Press **Page + Step 5** (Button 5).
- **Options**:
  - **MM-INIT**: **Min/Max Reset**. Resets the range to default (Min=0, Max=255) without changing shapes.
  - **M-FM**: **Chirp / FM**. A triangle wave that accelerates in frequency over the range.
  - **M-DAMP**: **Damped Oscillation** (Decaying Sine, 4 cycles).
  - **M-BOUNCE**: **Bouncing Ball** physics (Decaying absolute sines).
  - **M-RSTR**: **Rasterize**. Stretches the shape of the *first step* across the entire range.

### 6.3 Transform Menu (Step 7)
Manipulate existing sequence data.

- **Access**: Press **Page + Step 7** (Button 7).
- **Options**:
  - **T-INV**: **Invert**. Swaps Min and Max values (Flips slope direction).
  - **T-REV**: **Reverse**. Reverses step order and internal shape direction.
  - **T-MIRR**: **Mirror**. Reflects voltages across the centerline.
  - **T-ALGN**: **Align**. Ensures continuity by setting `Min = previous Max`.
  - **T-WALK**: **Smooth Walk**. Generates a continuous random path ("Drunken Sine").

### 6.4 Gate Presets Menu (Step 15)
Quickly assign dynamic gate behaviors based on the curve slope or levels.

- **Access**: Press **Page + Step 15** (Button 15).
- **Options**:
  - **ZC+**: **Zero Cross**. Trigger at every zero crossing (rising + falling).
  - **EOC/EOR**: **Peaks/Troughs**. Trigger at every local maximum and minimum.
  - **RISING**: Gate HIGH while voltage is increasing.
  - **FALLING**: Gate HIGH while voltage is decreasing.
  - **>50%**: **Comparator**. Gate HIGH when voltage is in the top half of the range.

## 7. Shortcuts & Gestures

### 7.1 Double-Click Toggles
- **Note Track**: Double-click a step button to toggle the Gate (on any non-gate layer).
- **Curve Track**: Double-click a step button to toggle the **Peak+Trough** gate preset.

### 7.2 Shaper Gates
Shaper Gates provide advanced gate generation based on curve analysis:

**Available Shaper Gate Types:**
- **Zero Cross (ZC+)**: Trigger at every zero crossing (rising + falling)
- **End of Cycle/End of Rise (EOC/EOR)**: Trigger at every local maximum and minimum (peaks/troughs)
- **Rising**: Gate HIGH while voltage is increasing
- **Falling**: Gate HIGH while voltage is decreasing
- **>50%**: Comparator - gate HIGH when voltage is in the top half of the range

**Setting Shaper Gates:**
- Access via Gate Presets Menu (Page + Step 15)
- Or configure directly in step gate settings
- Combine with curve shape and Min/Max values for complex gate patterns

**Advanced Gate Modes:**
- The Gate Probability parameter in Curve tracks has been repurposed as an Advanced Gate Mode selector
- Range: 0 to 7 with different behaviors:
  - 0: OFF (no gate output)
  - 1: RISE (gate triggers on rising slope)
  - 2: FALL (gate triggers on falling slope)
  - 3: MOVE (gate triggers on any slope movement)
  - 4: > 25% (gate HIGH when voltage is above 25% of range)
  - 5: > 50% (gate HIGH when voltage is above 50% of range)
  - 6: > 75% (gate HIGH when voltage is above 75% of range)
  - 7: WIND (Window mode - gate HIGH when voltage is within a specific window)
- When combined with Shaper Gates, enables sophisticated gate generation based on curve events (zero crossings, peaks, slopes)

### 7.3 Multi-Step Gradient Editing
- **Action**: Select multiple steps, hold **Shift** and turn the Encoder on **MIN** or **MAX**.
- **Result**: Creates a linear ramp of values from the first selected step to the last.
