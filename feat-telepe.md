Teletype ↔ Performer: Exposure & Emergent Behavior Proposal

Summary
- Goal: expose minimal shared signals so Teletype can modulate and react to other tracks, not just run as a VM.

Current Exposure (Reality Check)
- TIME/M in Clock mode already uses Performer clock ticks.
- TIME/M in Ms mode uses wallclock (os::ticks via bridge).
- TI-TR inputs can read physical gate outputs (GateOut1-8).
- CV inputs only read physical CV In 1-4; CV-out-as-input is not enabled.

Phase 1: Low-Risk, High-Impact
- Add shared bus: ✅ done (BUS 1–4 CV slots)
  - BUS n read/write shared CV slots.
  - Purpose: clean, safe cross-track modulation without touching track internals.
- Add track output readback:
  - TK.CV t / TK.TR t read current outputs for feedback loops.
  - Optional write aliases if not already routed.
- Add transport/clock read ops:
  - TP.RUN / TP.STOP / TP.RESET / TP.TICK (read only).
  - Lets scripts react to transport changes.

Phase 2: Medium Risk
- Pattern read/write:
  - PAT.G t p i / PAT.S t p i x for track pattern data.
  - Guard by track type (Note/Curve/Teletype only).
  - Enables algorithmic pattern generation.

Phase 3: Higher Risk
- Track parameter access:
  - TK.P t p / TK.P t p x for limited param set.
  - Keep scope tight (length, transpose, probability, etc.).

Concrete Emergent Behaviors
- Teletype as meta-modulator:
  - Drives multiple track CV/Gate outputs from algorithmic scripts.
- Polymetric modulation:
  - Teletype runs in Clock mode at its own divisor/multiplier and modulates other tracks.
- Cross-track feedback:
  - Read track outputs, compute logic, write to other tracks (reactive sequencing).
- Algorithmic pattern writing:
  - Generate/transform Performer patterns on the fly.

Design Notes
- Prefer small shared bus + explicit readback over deep track poking.
- Keep UI minimal: bus size + optional track mapping in list model.
- Document differences from Teletype hardware behavior (accumulation vs per-message, etc.).

Phase 4: Direct Engine Fusion (Emergent Hooks)

- **Modulation In (Performer → Teletype)**
  - Add `TeletypeX`, `TeletypeY`, `TeletypeZ`, `TeletypeT` to `Routing::Target`.
  - Allows Performer LFOs/Envelopes to modulate Teletype variables directly.
  - Emergent Result: A Note Track LFO can drive a Teletype algorithmic sequence.

- **Phase Variable (Clock → Teletype)**
  - Expose `Engine::syncFraction()` as Teletype variable `PHS` (0-100% of the bar).
  - Emergent Result: Teletype becomes a tempo-locked function generator/wavetable oscillator.

- **Direct Track Remote (Teletype → Performer)**
  - Add Opcodes: `TRK.MUTE i v` (Mute/Unmute) and `TRK.P i v` (Switch Pattern).
  - Emergent Result: Teletype acts as the "Conductor," switching patterns on other tracks based on algorithmic logic.

- **Variable Export (Teletype → Performer Source)**
  - Add `TeletypeX`... to `Routing::Source`.
  - Emergent Result: Teletype scripts become custom, complex modulators for any other track parameter.

## Phase 5: Advanced Emergent Behaviors

### 1. **Adaptive Harmony Engine**
- **Feature**: Add `HARMONY` opcodes that can read chord progressions from other tracks and generate complementary sequences
- **Implementation**: `HARMONY.GET track scale root` and `HARMONY.GEN scale root degree`
- **Emergent Behavior**: Teletype can generate harmonically correct sequences that adapt to chord changes in other tracks

### 2. **Dynamic Probability Fields**
- **Feature**: `PROB.FIELD` that creates probability maps based on other track states
- **Implementation**: `PROB.FIELD x y` where x and y are values from other tracks
- **Emergent Behavior**: Probability of teletype events changes based on the activity of other tracks, creating organic interdependence

### 3. **Cross-Track Accumulator**
- **Feature**: Shared accumulator that responds to events from multiple tracks
- **Implementation**: `ACCUM.TRK track value` to add to shared accumulator
- **Emergent Behavior**: Complex rhythmic interactions where multiple tracks contribute to a shared state that influences teletype behavior

### 4. **Meta-Pattern System**
- **Feature**: Patterns that contain references to other track patterns
- **Implementation**: `MPAT.REF track pattern_index` to reference another track's pattern
- **Emergent Behavior**: Complex polyrhythmic and polyphonic relationships that evolve based on multiple track states

### 5. **Adaptive Timing Modulation**
- **Feature**: `TIME.SCALE` that adjusts timing based on other track parameters
- **Implementation**: `TIME.SCALE factor` where factor is influenced by other track outputs
- **Emergent Behavior**: Time stretching and compression that responds to musical context

## Phase 6: Advanced Integration Features

### 6. **Track State Observer**
- **Feature**: `TK.STATE track` returns a composite value representing the current state of a track
- **Implementation**: Returns a value based on active steps, current position, gate states, etc.
- **Emergent Behavior**: Teletype can make decisions based on the "mood" or state of other tracks

### 7. **Cross-Track Quantization**
- **Feature**: `QT.TRK track value` quantizes a value to another track's scale or pattern
- **Implementation**: Quantize teletype CV output to match another track's musical key
- **Emergent Behavior**: Ensures harmonic coherence across tracks automatically

### 8. **Dynamic Track Linking**
- **Feature**: `LINK.TRK track1 track2 mode` creates dynamic relationships between tracks
- **Implementation**: Different modes like "follow", "counterpoint", "complement"
- **Emergent Behavior**: Tracks can dynamically change their relationship based on teletype logic

### 9. **Shared Memory System**
- **Feature**: `SHMEM` opcodes for sharing values between tracks through teletype
- **Implementation**: `SHMEM.PUT key value` and `SHMEM.GET key` with 16-32 shared slots
- **Emergent Behavior**: Complex inter-track communication without direct routing

### 10. **Adaptive Algorithm Selection**
- **Feature**: `ALGO.SWITCH` that changes teletype behavior based on other track states
- **Implementation**: Switch between different algorithmic approaches based on input from other tracks
- **Emergent Behavior**: Teletype becomes a dynamic orchestrator that adapts its approach based on musical context

## Phase 7: Advanced Emergent Behaviors

### 11. **Cross-Track Probability Propagation**
- **Feature**: `PROB.SPREAD` that propagates probability changes across tracks
- **Implementation**: When one track's probability changes, it influences others
- **Emergent Behavior**: Probability "waves" that flow through the system creating organic evolution

### 12. **Meta-Sequence Generator**
- **Feature**: `META.SEQ` that generates sequences for other tracks based on algorithmic rules
- **Implementation**: `META.SEQ track algorithm params` where algorithm is a complex procedure
- **Emergent Behavior**: Teletype becomes a composer that writes music for other tracks in real-time

### 13. **Dynamic Track Morphing**
- **Feature**: `MORPH.TRK` that gradually changes one track's pattern toward another's
- **Implementation**: Interpolate between track patterns over time
- **Emergent Behavior**: Smooth transitions between different musical ideas across tracks

### 14. **Feedback Loop Controller**
- **Feature**: `FB.CTRL` that manages complex feedback loops between tracks
- **Implementation**: Prevents runaway feedback while maintaining interesting interactions
- **Emergent Behavior**: Stable but complex musical interactions that evolve over time

### 15. **Cross-Track Pattern Evolution**
- **Feature**: `PAT.EVOLVE` that allows patterns to evolve based on other track patterns
- **Implementation**: Genetic algorithm-style pattern mutation influenced by other tracks
- **Emergent Behavior**: Self-evolving musical compositions that respond to their own output

## Implementation Considerations

### Safety Mechanisms
- **Limit Controls**: Prevent infinite loops or runaway processes
- **Resource Management**: Monitor CPU and memory usage of complex interactions
- **Recovery Systems**: Allow for graceful degradation when systems become too complex

### User Interface
- **Visualization**: Show inter-track relationships in the UI
- **Monitoring**: Display shared state and cross-track influences
- **Debugging**: Tools to understand complex emergent behaviors

These additional features would significantly expand the creative possibilities of the teletype-performer integration, allowing for complex, adaptive, and emergent musical behaviors that go far beyond simple script execution.

Ratings (1=low, 5=high)

- BUS.CV/TR shared bus: Impact 4, Complexity 2, Feasibility 4
  - Note: high impact for cross-track modulation; low complexity since it's a small shared array; feasible without touching track internals.
- Track output readback (TK.CV/TR): Impact 4, Complexity 3, Feasibility 3
  - Note: enables feedback loops; medium complexity due to cross-track access and potential timing issues.
- Transport/clock read ops: Impact 3, Complexity 2, Feasibility 4
  - Note: useful for script sync; simple read-only bridge data.
- Pattern read/write (PAT.G/S): Impact 4, Complexity 4, Feasibility 3
  - Note: strong creative payoff but touches pattern storage and edit safety.
- Track parameter access (TK.P): Impact 3, Complexity 4, Feasibility 2
  - Note: useful but risky due to wide param surface and track-specific semantics.
- Modulation In (Routing -> Teletype vars): Impact 5, Complexity 3, Feasibility 4
  - Note: big creative lift; moderate complexity using existing routing plumbing.
- Phase variable (PHS): Impact 3, Complexity 2, Feasibility 4
  - Note: straightforward clock read; moderate impact for sync-based scripts.
- Direct Track Remote (TRK.MUTE/TRK.P): Impact 4, Complexity 3, Feasibility 3
  - Note: powerful live control; moderate risk of conflicting with UI state.
- Variable Export (Teletype vars -> Routing): Impact 4, Complexity 3, Feasibility 3
  - Note: turns Teletype into a mod source; medium integration work.
- Adaptive Harmony Engine: Impact 4, Complexity 5, Feasibility 2
  - Note: strong musical payoff but heavy theory + cross-track access needed.
- Dynamic Probability Fields: Impact 3, Complexity 4, Feasibility 2
  - Note: interesting behavior; complex tuning and UI feedback required.
- Cross-Track Accumulator: Impact 3, Complexity 3, Feasibility 3
  - Note: useful shared state; medium plumbing and reset semantics.
- Meta-Pattern System: Impact 4, Complexity 5, Feasibility 2
  - Note: deep integration with pattern engine; high complexity.
- Adaptive Timing Modulation: Impact 3, Complexity 4, Feasibility 2
  - Note: musically interesting but risks clock/transport coherence.
- Track State Observer: Impact 3, Complexity 3, Feasibility 3
  - Note: moderate value; requires defining a stable "state" metric.
- Cross-Track Quantization: Impact 4, Complexity 4, Feasibility 3
  - Note: good musical utility; needs scale/pattern access coordination.
- Dynamic Track Linking: Impact 4, Complexity 5, Feasibility 2
  - Note: strong emergent behavior; high complexity and edge cases.
- Shared Memory System: Impact 3, Complexity 3, Feasibility 4
  - Note: flexible glue feature; easy to implement safely.
- Adaptive Algorithm Selection: Impact 3, Complexity 4, Feasibility 2
  - Note: interesting but adds meta-logic and UI complexity.
- Cross-Track Probability Propagation: Impact 3, Complexity 4, Feasibility 2
  - Note: requires careful stabilization to avoid chaos.
- Meta-Sequence Generator: Impact 4, Complexity 5, Feasibility 2
  - Note: high creative payoff; heavy CPU + integration cost.
- Dynamic Track Morphing: Impact 4, Complexity 5, Feasibility 2
  - Note: powerful for transitions; complex interpolation semantics.
- Feedback Loop Controller: Impact 3, Complexity 4, Feasibility 2
  - Note: stability tooling needed; significant edge-case handling.
- Cross-Track Pattern Evolution: Impact 4, Complexity 5, Feasibility 1
  - Note: highest complexity; risky for performance and predictability.
