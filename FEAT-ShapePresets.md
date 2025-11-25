# FEAT-ShapePresets.md

## Additional LFO Shape Presets for CurveTrack

This document outlines additional complex LFO shape presets that can be added to the CurveTrack's context menu for more expressive modulation options. These presets use sequences of predefined Curve::Type values that create interesting and musically useful patterns.

### 4-Step Sequences (Cycle-Complete)

1. **DIAMOND-LFO**
   - Sequence: RampUp → RampDown → RampDown → RampUp
   - Description: Creates a diamond-shaped waveform with symmetrical rises and falls

2. **EXP-LOG-LFO** 
   - Sequence: ExpUp → ExpDown → LogDown → LogUp
   - Description: Combines exponential and logarithmic curves for organic asymmetrical patterns

3. **SMOOTH-ROUND-LFO**
   - Sequence: SmoothUp → SmoothDown → SmoothDown → SmoothUp
   - Description: Uses smooth curves for gentle, rounded patterns without sharp transitions

4. **TRI-BELL-LFO**
   - Sequence: Triangle → RevBell → Triangle → Bell
   - Description: Alternates between triangular and bell curves for complex harmonic content

### 6-Step Sequences (Extended Patterns)

1. **WAVEFORM-EVOLVER**
   - Sequence: RampUp → Triangle → RampDown → RevBell → Triangle → Bell
   - Description: Evolves through multiple shape types for complex modulation

2. **GRADIENT-SLOPE**
   - Sequence: ExpUp → SmoothUp → RampUp → RampDown → SmoothDown → ExpDown
   - Description: Gradual slope changes from exponential to linear to exponential

3. **WAVE-EVOLUTION**
   - Sequence: Bell → SmoothUp → Triangle → SmoothDown → RevBell → Triangle
   - Description: Progressive waveform evolution with smooth transitions

### 3-Step Sequences (Triangular Patterns)

1. **INCOMPLETE-TRI**
   - Sequence: RampUp → RampDownHalf → RampUpHalf
   - Description: Creates an incomplete triangular pattern with half-curves

2. **ASYMMETRIC-TRI**
   - Sequence: Triangle → ExpDown → LogUp
   - Description: Asymmetrical triangular pattern with different rise and fall characteristics

3. **ROUND-TRIP**
   - Sequence: Bell → ExpDown → SmoothUp
   - Description: A round-trip pattern starting from bell curve

### Implementation Notes

All these patterns are designed to:
- Have continuous values at step boundaries (no jumps between steps)
- Use only existing Curve::Type values
- Create musically useful and expressive LFO patterns
- Be easily implemented as additional options in the CurveTrack context menu

These presets would expand the creative possibilities for users by providing more complex and interesting LFO shapes beyond the basic sine, triangle, sawtooth, and square waves.