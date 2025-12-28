/*
 * Complete Explanation: How Routing Shapers and Bias/Depth Influence Incoming Signals
 * 
 * This document explains how the routing system in the PEW|FORMER sequencer processes
 * incoming signals (LFOs, envelopes, CV inputs) using bias, depth, and shapers.
 */

/*
 * SIGNAL PROCESSING FLOW
 * 
 * The routing system processes signals in this order:
 * 
 * 1. SOURCE SIGNAL (0.0 to 1.0 normalized)
 *    - CV Input (e.g., LFO, envelope, random voltage)
 *    - MIDI CC, Pitch Bend, Note values
 *    - Internal sequencer parameters
 * 
 * 2. BIAS & DEPTH PROCESSING
 *    - applyBiasDepthToSource() function
 *    - Formula: 0.5 + (srcNormalized - 0.5) * depth + bias
 * 
 * 3. SHAPER PROCESSING
 *    - Various non-linear transformations
 *    - Some shapers are stateful (maintain memory)
 * 
 * 4. FINAL RANGE MAPPING
 *    - Scaled to target's min/max range
 *    - Applied to destination parameter
 */

/*
 * BIAS & DEPTH EXPLANATION
 * 
 * Bias: Shifts the entire signal up or down
 * - Range: -100% to +100%
 * - Effect: Adds a constant offset to all signal values
 * - Example: A sine wave centered at 0.5 with +25% bias becomes centered at 0.75
 * 
 * Depth: Controls the amplitude of the signal around the center point (0.5)
 * - Range: -100% to +100%
 * - Effect: Scales the signal's deviation from 0.5
 * - Example: A sine wave with 50% depth has half the original amplitude
 * 
 * Formula: result = 0.5 + (srcNormalized - 0.5) * depth + bias
 * 
 * This preserves the center point (0.5) when bias=0, and scales around it
 */

/*
 * SHAPER EXPLANATION
 * 
 * Shapers apply non-linear transformations to the signal after bias/depth:
 * 
 * 1. None: Passes signal unchanged
 * 
 * 2. Crease: Creates a discontinuity at 0.5 by folding
 *    - Values <= 0.5 shift up by 0.5
 *    - Values > 0.5 shift down by 0.5
 *    - Creates sharp transitions, useful for rhythmic modulation
 * 
 * 3. Location: Integrates the input to create a position accumulator
 *    - Slowly accumulates positive/negative changes
 *    - Acts like a position tracker for the signal
 *    - Useful for morphing between states
 * 
 * 4. Envelope: Creates an envelope follower based on input amplitude
 *    - Full-wave rectifies the signal (distance from center)
 *    - Applies different attack/release rates
 *    - Tracks the "envelope" of the input signal
 * 
 * 5. TriangleFold: Applies a triangular folding pattern
 *    - Creates complex harmonic content
 *    - Folds the signal in a triangular wave pattern
 * 
 * 6. FrequencyFollower: Detects frequency by counting zero crossings
 *    - Increases output value with more zero crossings
 *    - Useful for detecting signal frequency content
 * 
 * 7. Activity: Measures signal activity based on changes
 *    - Tracks how much the signal changes over time
 *    - Higher values for more active/complex signals
 * 
 * 8. ProgressiveDivider: Creates binary output that divides based on input
 *    - Binary output that toggles based on input crossings
 *    - Threshold increases with each toggle (progressive division)
 * 
 * 9. VcaNext: Uses next route as a VCA for this route
 *    - Amplitude modulates this signal with another route's output
 */

/*
 * PRACTICAL EXAMPLES FOR DESIGNERS
 * 
 * LFO MODULATION:
 * - Source: LFO output (sine, triangle, etc.)
 * - Bias: Shifts the LFO's center point
 *   - Positive bias: LFO spends more time in upper range
 *   - Negative bias: LFO spends more time in lower range
 * - Depth: Controls the LFO's modulation amount
 *   - Less than 100%: Reduces LFO's effect
 *   - Greater than 100%: Increases LFO's effect (may clip)
 * - Shapers: Add complexity to the LFO shape
 *   - Envelope: Creates envelope-following effect from LFO
 *   - Crease: Adds rhythmic steps to smooth LFO
 * 
 * ENVELOPE MODULATION:
 * - Source: ADSR or other envelope generator
 * - Bias: Shifts the entire envelope up/down
 *   - Positive bias: Adds constant value to entire envelope
 *   - Can be used to ensure envelope never goes below a threshold
 * - Depth: Controls envelope's dynamic range
 *   - Less than 100%: Reduces envelope's peak-to-sustain difference
 *   - Greater than 100%: Exaggerates envelope's shape
 * - Shapers: Transform envelope character
 *   - TriangleFold: Creates folded harmonics in envelope
 *   - Activity: Measures how "busy" the envelope is
 * 
 * CV INPUT MODULATION:
 * - Source: External CV input (LFO, envelope, random voltage)
 * - Bias: DC offset to the incoming CV
 * - Depth: Attenuation/gain for the incoming CV
 * - Shapers: Apply creative transformations to external CV
 */

/*
 * DESIGNER VISUALIZATION GUIDE
 * 
 * When drawing how these affect signals:
 * 
 * 1. Draw the original signal as a baseline (e.g., sine wave)
 * 2. Show bias as a vertical shift of the entire waveform
 * 3. Show depth as a vertical scaling around the center line
 * 4. Show shapers as non-linear transformations of the waveform
 * 
 * For example, with a sine wave:
 * - Original: Smooth sine wave oscillating around 0.5
 * - With +25% bias: Same shape but oscillating around 0.75
 * - With 50% depth: Shallower sine wave around same center
 * - With Crease shaper: Sharp transitions where original crossed 0.5
 */

/*
 * IMPLEMENTATION NOTES FOR DESIGNERS
 * 
 * The routing system is per-track for compatible targets, meaning:
 * - Each track can have different bias/depth/shaper settings
 * - Same source signal can be processed differently for different tracks
 * - Useful for creating evolving patterns across tracks
 * 
 * Stateful shapers (Location, Envelope, FrequencyFollower, Activity, ProgressiveDivider)
 * maintain their state independently per track, allowing for complex interactions.
 * 
 * The creaseEnabled flag can be applied in addition to other shapers,
 * creating an extra crease transformation on top of the selected shaper.
 */
