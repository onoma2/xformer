# Routing Signal Flow Manual: Per-Track Bias, Depth, and Shaper Setup

## 1. Introduction

The routing system in PEW|FORMER provides sophisticated per-track modulation capabilities using Bias, Depth, and Shaper parameters. Understanding the signal flow is crucial for creating dynamic and expressive patches.

**Key Concepts:**
- **Bias**: Offset applied to source signal (±100%)
- **Depth**: Modulation amount applied to target parameter (0-100%)
- **Shapers**: Non-linear transformations of source signals
- **Per-Track Control**: Each track can have unique modulation settings

## 2. Signal Flow Overview

```
Raw Source Value
      ↓
[Normalization] → Converts to 0-100% range
      ↓
[Bias + (Raw × Depth/100)] → Per-track adjustment
      ↓
[Shaper (if active)] → Non-linear transformation
      ↓
[Target Parameter] → Final modulated value
```

### 2.1 Normalization

All source values are normalized to a standard 0-100% range:
- CV Inputs: Voltage range (-5V to +5V, 0V to +5V, etc.) → 0-100%
- MIDI CC: 0-127 → 0-100%
- Internal sources: Specific ranges → 0-100%

This allows all source types to work with the same bias/depth percentage system.

### 2.2 Per-Track Parameters

Each track has independent bias, depth, and shaper settings:

**Bias (-100% to +100%)**:
- Adds offset to normalized source value
- Positive bias increases source signal
- Negative bias decreases source signal
- Example: Bias +50% adds 50% to all source values

**Depth (0% to 100%)**:
- Controls amount of source modulation applied
- 0% = no modulation (use target base value)
- 100% = full source range applied to target range
- Example: Depth 50% applies half of source variation

**Shapers**:
- Apply non-linear transformations to source
- Each shaper modifies how source affects target
- Can create complex modulation curves

## 3. Setting Up Routes

### 3.1 Creating a Basic Route

1. **Navigate to Routing Page**:
   - Press Shift + S2, then select "ROUTING"
   - Or access via main menu "ROUTING"

2. **Select Empty Route Slot**:
   - Choose unused route (indicated by "None" target)

3. **Configure Target**:
   - Use encoder to select target parameter
   - Examples: Slide Time, Transpose, Divisor, Chaos Amount, etc.
   - Press to confirm selection

4. **Configure Source**:
   - Set source (CV In 1-4, CV Out 1-8, MIDI)
   - Configure source-specific settings (CV range, MIDI CC#)

5. **Set Min/Max Range**:
   - Define parameter range for target
   - Use encoder to set minimum value
   - Use encoder to set maximum value

6. **Select Per-Track Targets**:
   - Use Track selection to choose which tracks are affected
   - Multiple tracks can share the same route with different bias/depth

### 3.3 BUS Targets (Global)

BUS targets (`BUS 1–4`) are global CV slots that can be routed to any target or read by Teletype.
Unlike per-track targets, BUS targets do not have a track mask; they are single global values.

**BUS shaping controls:**
- **Bias / Depth / Shaper** are still available for BUS targets.
- These use **track slot A (track index 0)** as the shared shaping slot.
- The shaper is applied before the BUS value is written.

**UI behavior:**
- When the target is a BUS, the route list shows three extra rows:
  - `Bias`
  - `Depth`
  - `Shaper`
- These rows control the BUS shaping slot directly.

### 3.2 Per-Track Parameter Assignment

Once route is created, per-track parameters are accessible:

**Via Track Page**:
- Navigate to affected track
- Access bias/depth via Shift + Track page
- Or use routing-specific parameters on track page

**Via Routing Page**:
- While editing route, select specific track
- Adjust bias/depth per track
- Shaper settings also per-track

## 4. Bias Application

### 4.1 Understanding Bias Impact

Bias shifts the entire source signal up or down:

**Example with CV Input**:
- Source: ±5V ramp (sine wave) = 0-100% normalized
- Base value: 50% (center of range)
- Bias +25%: Effective range becomes 25%-125% (wrapped to 0-100%)
- Result: Overall offset to all modulated values

**Mathematical Expression**:
```
Biased Value = clamp(Normalized Source + Bias, 0, 100)
```

### 4.2 Bias vs Depth Relationship

**Bias alone**:
- Shifts center point without changing modulation range
- Good for setting base parameter values

**Depth alone**:
- Controls amount of modulation applied
- 0% = no modulation, 100% = full range modulation

**Combined**:
- Bias shifts center, depth controls range
- Allows precise modulation mapping

## 5. Depth Control

### 5.1 Depth Scaling

Depth controls how much of the source signal variation is applied:

**Depth = 100%**:
- Full source range affects target
- Source 0% → Target Min
- Source 100% → Target Max

**Depth = 50%**:
- Half of source range affects target
- Source 50% (center) → Target center
- Source 0% → Target 25% of range
- Source 100% → Target 75% of range

**Mathematical Expression**:
```
Depth-Affected Value = Base + ((Biased Value - 50) * Depth / 100)
```

### 5.2 Practical Depth Examples

**Full Modulation (Depth 100%)**:
- Use when you want maximum parameter movement
- Good for dramatic effects
- Example: Full-range LFO controlling filter cutoff

**Subtle Modulation (Depth 10-30%)**:
- Use for gentle parameter variation
- Good for adding life to static sounds
- Example: Gentle pitch variation on bassline

## 6. Shaper Functions

### 6.1 Available Shapers

**None**:
- Direct linear mapping: Source = Target (with bias/depth applied)
- Standard behavior for most modulation needs

**Crease**:
- Creates "crease" or fold in the middle of the range
- Central values have less effect
- Outer values have more effect
- Good for emphasizing extremes

**Location**:
- Shifts where the modulation is most sensitive
- Can make lower or upper source values more impactful
- Useful for "threshold" effects

**Envelope**:
- Applies envelope-like transformation
- Creates attack and release characteristics
- Good for dynamic responses

**Triangle Fold**:
- Folds the range like a triangle wave
- Creates complex, folded modulation patterns
- Useful for creating harmonic relationships

**Frequency Follower**:
- Responds to rate of change in source
- Faster changes have more effect
- Good for dynamics based on input rate

**Activity**:
- Responds to activity and movement in source
- Quiet periods have different behavior than active periods
- Useful for adaptive modulation

**Progressive Divider**:
- Applies different divisions to the source
- Creates stepped, quantized modulation
- Good for rhythmic modulation patterns

**VcaNext (VC)**:
- Performs amplitude modulation using the **next route's** raw source
- Formula: `Output = Center + (Input - Center) * NextRouteSource`
- If next route source is 100%, input passes through
- If next route source is 0%, input is silenced (flattened to center)
- Allows one route to control the intensity of another (VCA behavior)

### 6.2 Shaper Signal Flow

After bias and depth are applied:

```
Biased + Depth-Affected Value
      ↓
[Shaper Applied] → Non-linear transformation
      ↓
[Target Parameter Range] → Final clamped value
```

## 7. Practical Setup Examples

### 7.1 Dynamic CV-to-Parameter Mapping

**Goal**: Map external LFO to track transpose with per-track control

1. **Create Route**:
   - Target: Transpose
   - Source: CV In 1
   - Min: -24 semitones, Max: +24 semitones

2. **Per-Track Setup**:
   - Track 1: Bias 0%, Depth 12% (subtle pitch modulation)
   - Track 2: Bias -5%, Depth 30% (moderate with offset)
   - Track 3: Bias +20%, Depth 50% (wide with positive offset)

3. **Result**:
   - Same LFO, different character per track
   - Track 1: Gentle pitch wobble
   - Track 2: Moderate pitch modulation with base offset
   - Track 3: Wide pitch variations with higher base pitch

### 7.2 Multi-Dimensional Modulation

**Goal**: Complex modulation combining multiple shapers

1. **Route 1**: Basic modulation
   - Target: Slide Time
   - Source: CV In 2
   - Min: 0%, Max: 100%
   - Shaper: None

2. **Per-Track with Shapers**:
   - Track 1: Bias 10%, Depth 40%, Shaper: Location (emphasizes lower values)
   - Track 2: Bias 0%, Depth 60%, Shaper: Crease (emphasizes extremes)
   - Track 3: Bias 0%, Depth 80%, Shaper: Triangle Fold (folded response)

### 7.3 MIDI CC to Multiple Parameters

**Goal**: Single MIDI CC controlling multiple parameters per track

1. **Route 1**: Master control
   - Target: Transpose
   - Source: MIDI CC 1
   - Min: -12, Max: +12

2. **Route 2**: Secondary parameter
   - Target: Slide Time
   - Source: MIDI CC 1
   - Min: 0%, Max: 80%

3. **Per-Track Setup**:
   - Track 1: Transpose (Bias 0%, Depth 30%), Slide (Bias 10%, Depth 25%)
   - Track 2: Transpose (Bias 0%, Depth 15%), Slide (Bias 5%, Depth 50%)
   - Track 3: Transpose (Bias 0%, Depth 50%), Slide (Bias 0%, Depth 15%)

### 7.4 VCA Modulation (Sidechaining)

**Goal**: Use an envelope to modulate the intensity of an LFO (VCA effect)

1. **Route 1**: The Signal (Carrier)
   - Target: Filter Cutoff (or any parameter)
   - Source: CV In 1 (Fast LFO)
   - Shaper: **VcaNext (VC)**

2. **Route 2**: The Control (Modulator)
   - Target: None (Dummy route just for source)
   - Source: CV In 2 (Slow Envelope)

3. **Result**:
   - The LFO from Route 1 is amplitude-modulated by the Envelope from Route 2.
   - When Envelope is high, LFO is full strength.
   - When Envelope is low, LFO is silenced.

## 8. Advanced Techniques

### 8.1 Dynamic Range Control

Use bias and depth to create dynamic range changes:

**Technique**:
- Lower depth for subtle modulation
- Increase depth via another route for "intensity" control
- Use shapers to create non-linear responses to dynamic changes

### 8.2 Cross-Modulation

Create routes that control other routing parameters:

**Example**:
- Route 1: Controls Track 1 Transpose
- Route 2: Controls Route 1 Depth
- Result: One source controls modulation amount of another

### 8.3 Threshold-Based Modulation

Use shapers to create threshold effects:

**Setup**:
- Shaper: Location with extreme settings
- Bias: Offset to desired threshold
- Depth: Amount of response above/below threshold

### 8.4 Per-Track Reset

Use the **Reset** target to force a track to restart:

**Behavior**:
- Target: `Reset`
- Source: Gate or Trigger (CV In, MIDI Note)
- **Rising Edge**: When the source signal crosses 50%, the targeted track(s) immediately reset.
- This overrides internal loop counters and positions.

### 8.5 Global Routing Changes

Recent updates to the routing system include:

**Enhanced Routing System**:
- Improved routing note handling for more accurate modulation
- Better integration between different track types and routing parameters
- Corrected routing source selection for various track types
- Enhanced LED feedback for routing status
- More responsive routing assignment

**VCA Next Shaper Improvements**:
- Enhanced VCA Next (VC) shaper functionality
- Better amplitude modulation between routes
- Improved carrier/modulator relationship handling
- More stable VCA behavior with dynamic sources

**Per-Track Parameter Improvements**:
- More reliable per-track bias and depth settings
- Better handling of track-specific shaper parameters
- Improved memory management for routing parameters

### 8.6 Special Routing Targets

#### 8.6.1 Clock Multiplier (ClockMult)

**Target**: `Clock Mult`
- **Type**: Sequence target (per-track)
- **Range**: 50% to 150% (normalized)
- **Function**: Controls the clock multiplier for sequences
- **Effect**: Adjusts how sequences respond to clock input
  - 100% = normal speed (1.0x)
  - Below 100% = slower playback
  - Above 100% = faster playback
- **Use Case**: Create rhythmic variations by modulating clock speed

#### 8.6.2 CV Output Rotation

**Target**: `CV Out Rot`
- **Type**: Track target (per-track)
- **Range**: -8 to +8
- **Function**: Rotates the CV output values for a track
- **Effect**: Shifts CV values by a specified number of steps
  - Positive values: Rotate CV values forward in sequence
  - Negative values: Rotate CV values backward in sequence
- **Use Case**: Create evolving CV patterns by rotating output values

**How Rotation Pools Work**
- Rotation only applies within the pool of outputs whose tracks are routed to `CV Out Rot`.
- The rotation value is an integer (rounded) and wraps within the pool size.
- CV and Gate rotations use separate pools.

**Source Range -> Rotation Value**
Routing maps the source (normalized 0..1) to the target range:
`r = round(min + src * (max - min))`

Examples assume the source range is set correctly in the route:

**Micro Rotation (neighbor swap)**
- **Min/Max**: `-1 .. +1`
- **Input range**: Unipolar 0..5V or Bipolar -5..+5V
- **Result**: r in {-1, 0, +1}
- **Use**: Gentle movement within the pool.

**Full Pool Rotation (all positions)**
- **Min/Max**: `0 .. (poolSize-1)`
- **Input range**: Unipolar 0..5V
- **Result**: r covers every pool position.
- **Use**: Sweep through every output in the pool.

**Wide Span (wrap-heavy)**
- **Min/Max**: `-8 .. +8`
- **Input range**: Bipolar -5..+5V
- **Result**: Large rotations that wrap in smaller pools.
- **Use**: Fast, chaotic reshuffling.

**Example Mapping (pool size 3)**
- r = 0: 1->1, 2->2, 3->3
- r = +1: 1<-2, 2<-3, 3<-1
- r = +2: 1<-3, 2<-1, 3<-2
- r = -1: 1<-3, 2<-1, 3<-2

#### 8.6.3 Gate Output Rotation

**Target**: `Gate Out Rot`
- **Type**: Track target (per-track)
- **Range**: -8 to +8
- **Function**: Rotates the gate output values for a track
- **Effect**: Shifts gate timing by a specified number of steps
  - Positive values: Rotate gate timing forward
  - Negative values: Rotate gate timing backward
- **Use Case**: Create rhythmic variations by rotating gate patterns
 
Gate rotation uses the same min/max mapping as CV rotation, but has its own pool.

#### 8.6.4 Track Run Control

**Target**: `Run`
- **Type**: Track target (per-track)
- **Range**: Binary (0 or 1)
- **Function**: Controls whether a track is running or stopped
- **Effect**:
  - 0 (off): Track stops advancing
  - 1 (on): Track continues advancing
- **Use Case**: Remotely start/stop tracks using CV or MIDI sources

#### 8.6.5 Track Reset Control

**Target**: `Reset`
- **Type**: Track target (per-track)
- **Range**: Binary (0 or 1)
- **Function**: Resets track position to beginning
- **Effect**:
  - Rising edge detection (triggers when signal crosses 50%)
  - Immediately resets the targeted track(s)
  - Overrides internal loop counters and positions
- **Use Case**: Synchronize tracks to a common start point using external triggers

## 9. Troubleshooting Common Issues

### 9.1 No Modulation Visible

**Check**:
- Is route active (target not "None")?
- Are tracks selected in route's track mask?
- Is depth > 0%?
- Is source actually changing?

### 9.2 Inverted Response

**Solution**:
- Swap Min/Max values in route settings
- Or apply -100% bias and adjust depth accordingly

### 9.3 Extreme or Unpredictable Values

**Check**:
- Is bias too high/low causing wraparound?
- Is depth too high creating large swings?
- Is target range appropriate for application?

### 9.4 Shaper Not Working

**Verify**:
- Shaper is correctly selected
- Source is actually changing (shapers affect dynamic signals)
- Per-track shaper settings are properly configured

### 9.5 Routing Issues

**Common Routing Problems**:
- Check that routing targets are properly assigned
- Verify that track selection in routes is correct
- Ensure source values are within expected ranges
- Confirm that bias/depth settings are appropriate for the target parameter

## 10. Performance Tips

### 10.1 CPU Considerations

- Shapers require additional CPU processing
- Complex shapers (Envelope, Frequency Follower) use more resources
- Many active routes can impact performance
- Monitor CPU usage with complex routing setups

### 10.2 Memory Usage

- Per-track bias/depth/shaper settings consume memory
- Each route uses memory for source tracking
- Memory impact is generally minimal for typical setups

### 10.3 Best Practices

- Start simple: Bias/Depth without shapers
- Add shapers gradually to understand their effects
- Use appropriate voltage ranges for sources
- Test routes in context of full patch
- Group related modulations logically

## 11. Creative Applications

### 11.1 Expressive Performance

- Map expression pedal to multiple parameters simultaneously
- Use different bias/depth per track for complex responses
- Apply shapers for non-linear expression curves

### 11.2 Algorithmic Composition

- Use slowly evolving LFOs with complex shapers
- Create interdependent modulations
- Use progressive dividers for rhythmic development

### 11.3 Sound Design

- Map complex CV to multiple synthesis parameters
- Create evolving timbres through intermodulation
- Use shapers to create non-linear response curves
