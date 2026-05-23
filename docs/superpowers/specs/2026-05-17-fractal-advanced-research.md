# Fractal Track — Advanced Features Research

> Current status, 2026-05-22: post-MVP research only. The current Fractal contract is
> parent melodic material -> volatile engine trunk -> model-owned command/rule sequence
> -> output. Capture policy is a model rule, not a separate `_recordArmed` recorder phase.
> Fractal is not a high-fidelity CV recorder; ideas below that imply capturing every CV
> movement inside a section are out of scope unless explicitly promoted as a separate mode.
> The captured unit is a Fractal section, not a parent event. Parent sequencing state,
> mutes, routing causes, resetMeasure events, held notes, curves, and scripts are opaque.
> Treat older "record arm" language below as "capture rule / external write arm" unless a
> future implementation explicitly restores a manual arm flag.

Four research areas for post-MVP Fractal Track features. Each section covers what exists in the codebase, what can be reused, and what needs new code.

---

## 1. Recording Trigger Variants

### What Exists

The Performer has three recording trigger mechanisms, all operating on the global `_engine.recording()` bool:

| Trigger | Location | Mechanism |
|---------|----------|-----------|
| UI key combo | `TopPage.cpp:190` | `Page + Play` toggles `_engine.recording()` |
| Gate sync | `RoutingEngine.cpp:632` | `Routing::Target::Record` — gate high = record ON, low = OFF |
| Gate toggle | `RoutingEngine.cpp:637` | `Routing::Target::RecordToggle` — rising edge toggles |

The current Fractal contract defines model-owned capture rules, decoupled from global
recording. This is the right foundation.

### Proposed Trigger Variants for Fractal

All variants would be implemented as a `RecordTrigger` enum on `FractalTrack`:

| Mode | Trigger Source | Behavior |
|------|---------------|----------|
| **Manual** | UI button / list param | Classic arm/disarm toggle |
| **Gate** | Routed gate input (CV in) | Arm on gate high, disarm on gate low. Like `Routing::Target::Record` but Fractal-local. |
| **GateToggle** | Routed gate input | Arm on rising edge. Like `Routing::Target::RecordToggle` but Fractal-local. |
| **Fill** | Fill state from routing | Arm when fill is active, disarm when fill ends. Captures only during fill passages. |
| **LoopBoundary** | Loop wrap (loopLast → loopFirst) | Arm at next loop start, record exactly one loop pass, auto-disarm at loop end. Single-capture mode. |
| **SourceGate** | Source track A or B gate output | Arm when source gate goes high, disarm when it goes low. Records only the active notes from the source. |
| **AutoFirst** | Buffer empty detection | If buffer has no valid steps, auto-arm on transport start. Disarm after first full pass. "First-take automatic." |

### Implementation Path

The engine's `tick()` already knows when it crosses a loop boundary (step wraps from `loopLast` to `loopFirst`). Adding a trigger evaluation at that point is trivial:

```cpp
// In triggerStep(), before capture-rule execution:
switch (_fractalTrack.recordTrigger()) {
case RecordTrigger::LoopBoundary:
    if (_loopBoundaryFlag && !captureRuleActive()) {
        armCaptureRule();
        setCaptureOnce(); // auto-disable writes after one pass
    }
    break;
case RecordTrigger::SourceGate:
    setCaptureRuleActive(sourceGateA || sourceGateB);
    break;
// ... etc
}
```

### Key Decision

Should Fractal-local capture triggers reuse the existing Routing infrastructure (so any CV input can arm writes via `Routing::Target::FractalRecord`), or should they be internal-only? Recommendation: add `FractalRecord` and `FractalRecordToggle` to `Routing::Target` so external CV/gate can arm Fractal capture independently of the global record state. This gives maximum flexibility without architectural changes.

---

## 2. Math Operations on the Loop Buffer

### What Exists: IndexedTrack Route Modulation Math

The IndexedTrack engine has a modular CV math system for live parameter modulation:

| Operation | Formula | Application |
|-----------|---------|-------------|
| **Multiplicative scale** | `value × (1 + mod)` | Duration scaling |
| **Additive offset** | `value + mod` | Note transposition |
| **Clamp** | `clamp(result, min, max)` | Bounding |
| **Combine Mux** | `(a + b) / 2` | Average of two sources |
| **Combine Min/Max** | `min(a, b)` / `max(a, b)` | Bounding |
| **Combine AtoB** | `a + b` | Cascading |

### Proposed Buffer Math Operations

Apply these as **destructive transforms** on the captured CV buffer, triggered manually or at loop boundaries:

| Operation | Formula | Musical Effect |
|-----------|---------|---------------|
| **Transpose** | `cv[i] += offset` | Shift entire loop pitch |
| **Scale** | `cv[i] = center + (cv[i] - center) × factor` | Expand/compress pitch range around a center note |
| **Quantize** | `cv[i] = scale.snapToDegree(cv[i])` | Snap buffer to current scale |
| **Invert** | `cv[i] = center - (cv[i] - center)` | Invert melody around a pivot |
| **Min/Max clamp** | `cv[i] = clamp(cv[i], minV, maxV)` | Limit pitch range |
| **Reverse** | Reverse `cv[i]` array order | Play melody backwards |
| **Rotate** | Circular shift of buffer indices | Phase-shift the loop |
| **Thin** | Clear gate bits at lowest-priority indices | Remove notes (density masking) |
| **Densify** | Set gate bits at rest-priority indices | Add notes where rests were |

### Implementation

These are pure array operations on `_cvBuffer[]` and `_gateBitmap[]`. They can be:
- **Manual**: Triggered from UI (context action) or routing target.
- **Automatic**: Applied at loop boundaries as part of the mutation lifecycle (Phase 4 extension).

The reverse operation already exists in the codebase (`CurveSequence::transformReverse`, `IndexedSequence::transformReverse`). The quantize operation exists in `StochasticTrackEngine::evalStepNote`. All others are trivial float math.

### Key Decision

Should math operations be destructive (modify the buffer in place) or non-destructive (apply as a transform layer on replay)? Recommendation: MVP is destructive (simpler). Post-MVP could add a transform stack that layers operations non-destructively, like a per-buffer FX chain.

---

## 3. Stretch, Twist, and Mangle

### What Exists

| Technique | Location | Mechanism |
|-----------|----------|-----------|
| **Linear interpolation** | `CurveTrackEngine:270`, `TeletypeTrackEngine:218` | `_tickPhase`-based lerp between tick values |
| **Polyrhythmic time warp** | `TuesdayTrackEngine:953` | `(stepIndex × subdivisions) / 4` — fractional step mapping |
| **Slide/slew** | `Slide::applySlide()` (6 engines) | Exponential CV smoothing over time |
| **Wavefolder** | `CurveTrackEngine:426` | `applyWavefolder(input, fold, gain)` — harmonic distortion |
| **Routing shapers** | `RoutingEngine:525` | 9 shaper types: Crease, TriangleFold, Envelope, etc. |
| **Reverse playback** | `CurveTrackEngine:360` | Direction < 0 inverts step fraction |
| **Pendulum** | `Accumulator:72` | Ping-pong direction at boundaries |
| **Global phase offset** | `CurveTrackEngine:404` | Phase-shifts the loop start position |

### Proposed Stretch/Twist/Mangle Operations

#### Stretch — Time Domain Manipulation

| Mode | How | Effect |
|------|-----|--------|
| **Half-speed** | Play every buffer step for 2 divisor divisions | Loop takes twice as long, pitches unchanged |
| **Double-speed** | Play every other buffer step | Loop plays in half the time |
| **Stretch-to-fill** | Resample buffer to fit different loop length | e.g., 32-step recording in a 64-step loop via linear interpolation |
| **Swing** | Alternate early/late step timing by ±N% | Groove feel |
| **Scramble** | Random permutation of step order | Preserve all notes, destroy sequence order |

**Implementation of stretch-to-fill** (most interesting):

```cpp
// Read from buffer at fractional index, interpolate between neighbors
float srcPos = float(stepIndex) * float(sourceLength) / float(targetLength);
int low = int(srcPos);
float frac = srcPos - float(low);
int high = (low + 1) % sourceLength;
float interpolated = cvBuffer[low] * (1.f - frac) + cvBuffer[high] * frac;
```

This is the same linear interpolation used by CV rotation in `Engine::updateTrackOutputs()`.

#### Twist — CV Domain Manipulation

| Mode | How | Effect |
|------|-----|--------|
| **Wavefold** | Apply `applyWavefolder()` from CurveTrack | Harmonic distortion on CV = add overtones |
| **Shaper** | Apply routing shapers (Crease, TriangleFold, etc.) | Various nonlinear CV warps |
| **Slew smear** | Run `Slide::applySlide()` across the buffer | Smooth/blur the melody |
| **Bitcrush** | Quantize CV to N discrete levels | Lo-fi stepping |
| **Chaos drift** | Lorenz attractor offset on CV values | Organic continuous drift |

**Bitcrush** is the simplest novel operation:
```cpp
float levels = powf(2, bitDepth); // e.g., 2^4 = 16 levels
cvBuffer[i] = roundf(cvBuffer[i] * levels / 5.f) * 5.f / levels;
```

#### Mangle — Structural Manipulation

| Mode | How | Effect |
|------|-----|--------|
| **Pendulum** | Ping-pong loop direction (Accumulator pattern) | Forward-reverse playback |
| **Stutter** | Repeat a subsection of the loop N times | Glitch/repeat effect |
| **Tape stop** | Gradually slow playback rate to zero | Deceleration effect |
| **Shuffle** | Swap adjacent pairs of steps | Subtle reordering |

**Tape stop** implementation sketch:
```cpp
// Modify the effective divisor over time
_playbackRate *= 0.98f; // exponential slowdown
effectiveDivisor = baseDivisor / _playbackRate;
// When _playbackRate < 0.1f, stop playback
```

### Key Decision

Stretch operations change the relationship between buffer length and step count. Should the engine support variable-rate playback (complex but powerful), or should stretch be a destructive buffer resampling (simpler, buffer size changes)? Recommendation: Start with destructive resampling (rewrites the buffer to new length). Variable-rate playback is a deeper architectural change that affects sequence state advancement.

---

## 4. Recording Final DAC Output (Post-Track Pipeline)

### What Exists: The CV Output Chain

```
TrackEngine::tick() → trackEngine.cvOutput(0) [float volts]
        ↓
Engine::updateTrackOutputs() → rotation, mixing, modulator offset
        ↓
CvOutput::setChannel(i, value) → _channels[i] = value [float volts]
        ↓
CvOutput::update() → calibration.voltsToValue() → DAC SPI write
```

The MonitorPage scope reads from `_engine.cvOutput().channel(i)` — this is the **post-mix, post-rotation, post-modulator-offset** value. It is NOT the actual DAC output (the DAC8568 is write-only SPI with no readback).

### Three Recording Points

| Point | What You Get | Pros | Cons |
|-------|-------------|------|------|
| **A: Track engine output** | `trackEngine.cvOutput(0)` before Engine mixing | Isolated source, no crosstalk. Current MVP design. | Misses routing modulator offsets, CV rotation, bus mixing |
| **B: Final CvOutput channel** | `_engine.cvOutput().channel(i)` after all mixing | Full post-processing chain. What the DAC actually receives. | Includes other tracks' rotation/mixing — not isolated to source track |
| **C: Per-track post-mix** | Track engine output + per-track modulator offset only | Best of both: isolated + track-level processing | Requires new engine API to expose post-modulator-per-track values |

### Proposed: Dual Recording Mode

Add a `RecordSource` enum to `FractalTrack`:

| Mode | Source | Use Case |
|------|--------|----------|
| **TrackEngine** | `trackEngine.cvOutput(0)` (current MVP) | Record a single source track's output cleanly |
| **CvOutput** | `_engine.cvOutput().channel(trackIndex)` | Record the final mixed/rotated/modulated output for this track's CV channel |

**Why not all three:** Point C (per-track post-modulator) doesn't exist as a distinct value in the pipeline. The modulator offset is applied globally in `Engine::applyModulatorOffset()` to the final CvOutput channel, not per-track. Adding a per-track post-modulator tap would require restructuring `updateTrackOutputs()`. Not worth it for MVP.

### CvOutput Recording Implementation

In `tick()`, when recording in CvOutput mode:
```cpp
if (_fractalTrack.recordSource() == RecordSource::CvOutput) {
    // Read the final output value for this track's assigned CV channel
    float finalCv = _engine.cvOutput().channel(_track.trackIndex());
    bool finalGate = (_engine.gateOutput() >> _track.trackIndex()) & 1;
    _cvBuffer[_currentStep] = finalCv;
    setGateBit(_gateBitmap, _currentStep, finalGate);
}
```

**Timing concern:** `CvOutput` values are written during `Engine::update()`, which runs after all `tick()` calls. So during `tick()`, `_engine.cvOutput().channel(i)` still holds the **previous update's** value. This is one frame behind. For step-rate recording this is acceptable (the lag is one UI frame, ~1ms at 1kHz). If exact synchronization matters, the Fractal engine would need a post-update hook — but this requires Engine architectural changes.

### Key Decision

For MVP: keep `TrackEngine` mode only. Add `CvOutput` mode as a post-MVP option. The one-frame timing lag is a known limitation that should be documented in the UI (e.g., "DAC mode: output sampled at previous frame").

---

## Summary: Post-MVP Feature Gate Priority

| Priority | Feature | Source | Complexity |
|----------|---------|--------|-----------|
| **P1** | Record trigger variants (Gate, Fill, LoopBoundary, SourceGate) | Existing Routing infrastructure | Low — enum + switch in `tick()` |
| **P1** | Buffer math (Transpose, Scale, Quantize, Invert, Reverse) | Existing `Scale::`, `transformReverse()` | Low — pure array operations |
| **P2** | Stretch (half/double speed, stretch-to-fill) | Existing linear interpolation | Medium — buffer resampling changes length |
| **P2** | Twist (wavefold, bitcrush, slew smear) | Existing `applyWavefolder()`, `Slide::` | Low — per-value float operations |
| **P2** | Mangle (pendulum, stutter) | Existing `Accumulator` pendulum | Medium — affects sequence advancement |
| **P3** | CvOutput recording mode | `_engine.cvOutput()` tap | Low code, one-frame timing caveat |
| **P3** | Variable-rate playback (tape stop) | New | Medium — divisor modulation at runtime |
| **P3** | Chaos drift (Lorenz on buffer) | `LowerRenz` / `streams_lorenz_generator` | Medium — float state per engine |
| **P4** | Scramble (random step permutation) | New | Low — Fisher-Yates shuffle |
| **P4** | Non-destructive transform stack | New | High — layered FX chain architecture |

---

## 5. Teletype Pattern Ops — Reusable for Fractal Buffer

### Source: `teletype/src/ops/patterns.c`

The Teletype VM provides a mature pattern operation library operating on 4 banks × 64 steps of `int16_t` values. The Fractal buffer (`_cvBuffer[]` float, `_gateBitmap[]` bitpacked) has the same shape but different element types. Below is a mapping of which Teletype ops port directly to Fractal buffer operations.

### Directly Portable — Same Algorithm, Different Type

| Teletype Op | Algorithm | Fractal Equivalent | Porting Effort |
|-------------|-----------|-------------------|----------------|
| **P.REV** | Swap at midpoint `start+i ↔ end-i` | Reverse buffer CV + gate arrays | Trivial — already exists as `CurveSequence::transformReverse()` |
| **P.ROT** | 3-reverse trick for circular shift | Rotate loop window | Trivial — already exists as `SequenceUtils::rotateStep()` |
| **P.SHUF** | Fisher-Yates shuffle | Randomize step order | Low — `random_next()` → `Random::nextRange()` |
| **P.PA** (add) | `val[i] += delta` for each i | Transpose all CV by delta volts | Trivial |
| **P.PS** (sub) | `val[i] -= delta` | Transpose all CV down by delta | Trivial |
| **P.PM** (mul) | `val[i] *= factor` | Scale CV range around center | Low — `(val - center) * factor + center` |
| **P.PD** (div) | `val[i] /= divisor` | Quantize to N equal divisions | Low |
| **P.PMOD** (mod) | `val[i] %= divisor` | Wrap CV to N-semitone range | Low |
| **P.SCALE** | Linear map `[in_min..in_max] → [out_min..out_max]` | Rescale CV to arbitrary range | Low — same `scale_val()` formula |
| **P.SUM** | Sum all values | Total voltage budget | Trivial |
| **P.AVG** | Integer average | Center-of-mass pitch | Trivial |
| **P.MINV / P.MAXV** | Min/max value scan | Pitch range detection | Trivial |
| **P.FND** | Find first index with target value | Locate specific CV in buffer | Trivial |
| **P.INS** | Insert value at index, shift rest right | Insert step, push later steps | Medium — gate bitmap shift |
| **P.RM** | Remove at index, shift rest left | Delete step, pull earlier steps | Medium — gate bitmap shift |
| **P.PUSH / P.POP** | Append to end / remove from end | Add/remove step at loop boundary | Low |
| **P.CYC** | Repeat values `val[i % len]` | Cycle-shorten loop | Low |

### The P.MAP Pattern — Most Important for Fractal

`P.MAP` is a higher-order operation: iterate over every value in the pattern, set `I` to the current value, execute a sub-command, write the result back. This is the **universal buffer transform** mechanism.

For Fractal, this translates to:

```cpp
// Apply a transform function to every CV value in the loop
void FractalTrackEngine::applyBufferTransform(std::function<float(float, int)> fn) {
    for (int i = _fractalTrack.loopFirst(); i <= _fractalTrack.loopLast(); ++i) {
        if (getGateBit(_validBitmap, i)) {
            _cvBuffer[i] = fn(_cvBuffer[i], i);
        }
    }
}

// Usage examples:
applyBufferTransform([](float cv, int idx) { return cv + 0.5f; });                    // transpose up
applyBufferTransform([](float cv, int idx) { return center - (cv - center); });        // invert
applyBufferTransform([](float cv, int idx) { return scale.snapToVolts(cv); });           // quantize
applyBufferTransform([](float cv, int idx) { return std::fmod(cv + 1.f, 5.f) - 5.f; }); // fold
```

### New Teletype Ops from Live-Sequencer Research

The `docs/new-teleops.md` research identifies operations absent from the current VM. Several map directly to Fractal buffer operations:

#### Algorithmic Buffer Fill (replace current content)

| Op | What It Does | Fractal Application |
|----|-------------|---------------------|
| **P.LSYS** | L-system string rewriting (Fibonacci word, etc.) | Fill buffer with self-similar fractal pitch sequences. Rule `A→AB, B→A` = Fibonacci word mapped to scale degrees. |
| **P.THUE** | Thue-Morse binary sequence | Fill gate bitmap with aperiodic balanced rhythm. `popcount(n) mod 2` per step. |
| **P.CA1D** | 1D Wolfram cellular automaton (rules 0–255) | Evolve gate bitmap each loop boundary. Rule 30 = structured chaos, Rule 90 = fractal, Rule 110 = complex. |
| **P.MARKOV** | First-order Markov chain | Generate pitch sequences with learned transition probabilities from source track. |
| **P.DBRU** | De Bruijn sequence | Exhaustive N-gram coverage: every interval transition appears exactly once before repeating. |
| **P.FIB** | Golden angle spacing | Distribute N gate-onsets across loop using `(i × φ) mod loopLength`. Irrational, off-grid timing. |

#### Rhythm Logic (gate bitmap operations)

| Op | What It Does | Fractal Application |
|----|-------------|---------------------|
| **P.TR.AND / OR / XOR / NOT** | Bitwise combine two gate patterns | Combine Fractal's gate bitmap with a generated rhythm pattern. AND = thin, OR = thicken, XOR = toggle. |
| **P.BRES** | Bresenham line rhythm | Alternative to Euclidean for even spacing. Poly variant = weighted interlocking voices in one buffer. |
| **P.GHOST** | Probability-fill with rhythmic bias (8 named shapes: uniform, offbeat, downbeat, etc.) | Fill empty gate slots with context-aware probability. Musical variant of density masking. |
| **P.THIN** | Rhythm-aware note removal | Inverse of GHOST: remove notes at specific rhythmic positions. 8 bias shapes. |

#### Timing / Phase (playback manipulation)

| Op | What It Does | Fractal Application |
|----|-------------|---------------------|
| **INTOX BEER** | Per-step duration jitter, renormalized to preserve total loop length | "Drunk timing" — wobble step durations but keep total loop period constant. |
| **INTOX HASH** | Random positive timing offset | Shifts notes later without renormalization. Creates swing-like feel. |
| **INTOX COFFEE** | Negative timing offset | Counterpart to HASH — tightens timing. |
| **INTOX LSD** | Duration expansion (last step absorbs remainder) | Stretches the end of the loop. |
| **FLIP** | A/B time split at ratio | True for first N% of loop, false for rest. Creates two-section loop structure. |

#### Pattern Transforms (new-teleops additions)

| Op | What It Does | Fractal Application |
|----|-------------|---------------------|
| **P.PAL** | Palindrome — alternate forward/reverse each cycle | Ping-pong loop. Already exists as `Accumulator::tickWithPendulum()`. |
| **P.WTH** | Sub-range transform — apply operation to only steps `[start..end]` | Partial loop mutation: mutate only a window of the buffer, leave anchor notes untouched. |
| **P.STR** | Stretch/resample to target length | Change buffer length with interpolation. Uses same linear interpolation as `Engine::updateTrackOutputs()`. |
| **P.URN** | Sample without replacement | Pick N random non-repeating CV values from pool. Guarantees no pitch repeats until exhausted. |
| **P.INV** | Invert/mirror around center | `center - (val - center)`. Melody inversion. |
| **P.WALK** | Random walk fill | Brownian CV sequence. Step size controls smoothness. |
| **P.SAWALK** | Self-avoiding walk | No consecutive repeats. Natural phrase boundaries when all neighbors visited. |
| **P.COMP** | Compress/select mask | Keep values only where mask = 1. Non-destructive density control. |
| **P.UNDUP** | Remove consecutive duplicates | Prevent same CV on successive steps. Creates more varied output. |
| **P.ACCUM** | Cumulative sum | Running total of CV differences. Builds envelope shapes. |
| **P.STUTT** | Ratchet/stutter — repeat each value N times | Expand loop by repeating each step N times. N can be per-step variable. |
| **P.OFF** | Delayed copy with transform (add/mul) | Canon/echo effect: blend original buffer with delayed+transposed copy. |

#### CV Mapping Curves

| Op | Formula | Fractal Application |
|----|---------|---------------------|
| **LINLIN** | `out_min + (v - in_min) × (out_max - out_min) / (in_max - in_min)` | Linear remap. Already exists as `P.SCALE`. |
| **LINEXP** | Same but exponential interpolation | Exponential pitch curves, filter sweeps. |
| **EXPLIN** | Exponential input, linear output | Logarithmic response to linear parameter. |
| **EXPEXP** | Exponential in and out | Pure exponential warping. |
| **SMOOTHSTEP** | Cubic Hermite `3t² - 2t³` | Smooth S-curve interpolation between two buffer states. Crossfade without discontinuity. |

### Signal Generators (CV source ops)

| Op | What It Does | Fractal Application |
|----|-------------|---------------------|
| **PERLIN** | Smooth Perlin noise (0–16383) | Fill buffer with organic modulation curve. Not random, not sine — natural drift. |
| **NORMAL** | Gaussian distribution via Box-Muller | Fill buffer with statistically natural pitch distribution. More values near center, rare extremes. |
| **PINK** | 1/f pink noise (Voss-McCartney) | Fill with slow drift + fast jitter in one signal. Matches natural parameter variation. |
| **WALK** | Bounded random walk (Brownian CV) | Continuous CV source with step size and bounds control. |
| **FIB** | Fibonacci number sequence | `0,1,1,2,3,5,8,13...` as CV values mapped to scale degrees. |

### Highest-Value Fractal Ports (Prioritized)

Based on musical value × implementation simplicity:

| Priority | Op(s) | Why |
|----------|-------|-----|
| **P1** | P.REV, P.ROT, P.SHUF | Already have implementations in codebase. Zero new logic. |
| **P1** | P.PA/PS/PM (transpose, scale) | Trivial float math on buffer. |
| **P1** | P.MAP pattern | Universal transform mechanism. One function, infinite operations. |
| **P1** | P.PAL (palindrome) | Already have `Accumulator` pendulum pattern. Trivial to adapt. |
| **P1** | P.WTH (sub-range transform) | Essential for partial loop mutation — mutate only a window. |
| **P2** | P.CA1D (1D cellular automaton) | 8 bytes of state (uint64_t), evolves gate bitmap each loop. Rule 30/90/110 are musically proven. |
| **P2** | P.THUE (Thue-Morse) | 0 lines of new state — compute `popcount(step) % 2` per step. Balanced aperiodic rhythm. |
| **P2** | P.LSYS (L-system) | ~50 lines. Fibonacci word mapped to scale degrees = instant self-similar melody. |
| **P2** | P.GHOST / P.THIN | Rhythm-aware fill/thin with 8 named bias shapes. Musical upgrade to density masking. |
| **P2** | INTOX BEER/HASH/COFFEE | Timing jitter with renormalization. Maps to per-step gate length offsets in the buffer. |
| **P3** | P.STR (stretch) | Buffer resampling with interpolation. Changes buffer length — architectural impact. |
| **P3** | P.OFF (delayed copy + blend) | Canon/echo effect. Requires double-buffer or temp buffer for the delayed copy. |
| **P3** | PERLIN, NORMAL, PINK | Signal generators for buffer fill. Each needs its own state (~16-32 B). |
| **P3** | P.STUTT (ratchet) | Expands buffer by N×. Changes loop length. |
| **P3** | P.MARKOV | Needs transition matrix storage. ~128 B for 8×8 state table. |
| **P4** | LINEXP, EXPLIN, EXPEXP | Curve mapping. Requires `powf()` — available but float-heavy on STM32. |
| **P4** | P.DBRU, P.SAWALK | Niche generators. Low musical ROI for the implementation complexity. |
| **P4** | P.REAC (reaction-diffusion) | 1D PDE simulation. Computationally heavy for a step-rate buffer op. |
