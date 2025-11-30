# Curve Track Wavefolder Enhancement Session - Status Update

## 1. Major Issues Identified

### 1.1 Severe Aliasing with Fold-F Feedback
**Problem:** Any amount of Fold-F feedback introduces significant aliasing to the output, particularly noticeable during complex modulation.

**Root Cause:** The sine-based wavefolder combined with feedback creates high-frequency content that exceeds the sampling frequency, causing aliasing artifacts. When Fold-F feeds processed (potentially aliased) output back into the wavefolder, it compounds the problem.

**Current Impact:** Makes Fold-F feedback impractical for many musical applications due to audible distortion.

## 2. Proposed Solutions for Aliasing Issue

### 2.1 Oversampling Approach
- Process the wavefolder section at higher sample rate (e.g. 2x or 4x)
- Apply low-pass filtering before downsampling
- **Pros:** Effective aliasing reduction
- **Cons:** High computational cost on STM32

### 2.2 Band-Limited Wavefolder
- Use band-limited approximation of the sine wavefolder
- Limit harmonic content to below Nyquist frequency
- **Pros:** More computationally efficient than oversampling
- **Cons:** May affect harmonic character of wavefolder

### 2.3 Feedback Filtering
- Add gentle low-pass filtering in the feedback path
- Reduce high-frequency content before feedback
- **Pros:** Simple to implement, computationally light
- **Cons:** Slightly alters the feedback character

## 3. Implemented Features During This Session

### 3.1 Amplitude Compensation
- Added compensation for amplitude reduction when wavefolder harmonics are attenuated by the filter
- Compensation only applies when filter is active (outside dead zone)
- Also compensates for filtering effects when fold=0

### 3.2 Feedback Control Improvements
- Made Fold-F and Filter-F controls logarithmic for smoother low-end behavior
- Fold-F feedback now only applies when fold > 0 (prevents signal getting "stuck" when fold=0)
- Added resonance feedback limiting when filter control is at extreme settings

### 3.3 Gain Parameter Remapping
- Remapped UI gain from 0.0-2.0 to internal 1.0-5.0 range
- More intuitive mapping: 0.0='standard', 1.0='1 extra', 2.0='2 extreme'
- Maintains all mathematical properties of the wavefolder algorithm

### 3.4 Output Protection
- Added proper ±5V limiting to prevent output overloads
- Added internal feedback state limiting to prevent runaway feedback
- Reduced maximum resonance feedback scaling to prevent instability

### 3.5 Crossfading Functionality (xFade)
- Added xFade parameter to blend between original phased shape and processed signal
- Default value of 1.0 preserves original behavior
- Crossfader operates on final output, feedback path uses processed signal before crossfading

### 3.6 UI Updates
- Added xFade parameter to Wavefolder2 UI (after Filter-F)
- Updated Gain parameter display to show new 0.0-2.0 range properly
- All new parameters have proper serialization support

## 4. Signal Path Updates
- Original shape → Phase offset → [stored for crossfading] → Fold-F feedback → Wavefolder → Denormalize → Filter + Filter-F → Compensation → Crossfading → Final limiting → CV output
- Feedback path: Processed signal (before xFade) → _feedbackState → Fold-F feedback input

## 5. Performance Considerations
- All changes maintain real-time performance on STM32
- Minimal additional computational overhead (crossfade: 3 mults + 2 adds, limiting: min/max ops)
- Memory usage increased by only a few bytes per CurveTrack engine instance

## 6. Backward Compatibility
- All parameters maintain backward compatibility
- Gain parameter automatically converted from old (1.0-5.0) to new (0.0-2.0) range when loading older projects
- Default xFade value maintains original behavior in existing projects