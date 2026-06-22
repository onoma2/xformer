# Physical Track Specification (Spring)

> A physics-based track engine where sequenced hits excite a virtual spring-mass system. Outputs are derived from the resulting motion via a modulation and threshold matrix.

## 1. Class shape
- **Model**: `PhysicalTrack`, `PhysicalSequence` (New classes).
- **Engine**: `PhysicalTrackEngine` (New class).
- **UI**: `PhysicalSequencePage` (Grid), `PhysicalSequenceEditPage` (Settings), `PhysicalTrackListModel` (Shared).
- **Identity**: This is a **Recipe Replay** model. Stored hits in the sequence trigger real-time physics calculations in the engine.

## 2. Grid / traversal
- **Steps**: 64 steps per sequence.
- **Traversal**: Standard linear traversal, matching `NoteTrack`.
- **Skip behavior**: Skipped steps do not trigger a hit, but the spring continues to move based on its current momentum and forces.

## 3. Timing model
- **Resolution**: 192 PPQN (standard).
- **Reset**: Resets physics state (Position = RestHeight, Velocity = 0) on sequence start or explicit reset.
- **Physics Tick**: Calculated at engine rate (sub-stepped for stability).

## 4. Identity / storage model
- **Track**: Stores global physics params (Tension, Damping, Mass, Gravity) and output mappings (Mod Matrix, Thresholds).
- **Sequence**: Stores a 64-step grid of "Hits".
- **Engine Transient State**: Position (X, Y, Z), Velocity (X, Y, Z), Kinetic Energy, Potential Energy, Total Energy.

## 5. Per-cell / per-step record
- **Hit (1 bit)**: Whether the step triggers the spring.
- **Force (7 bits)**: The intensity of the hit (velocity/impulse).
- **Direction (3 bits)**: X/Y/Z vector preference for the hit (or random).
- **Total**: 11 bits per step. Matches `DiscreteMap` efficiency.

## 6. Physics Math
- **Integrator**: Sub-stepped Euler or semi-implicit Euler.
- **Formula**: `F = -k * (pos - rest) - damping * vel + gravity`.
- **Stability**: Clamp `pos` and `vel` at extreme boundaries to prevent numerical explosion.

## 7. Outputs
- **Jack count**: 2 CV, 1 Gate (Standard Track).
- **Mapping**:
    - **CV 1 & 2**: Mapped via **Mod Matrix**.
    - **Gate**: Mapped via **Threshold Matrix** (e.g., fires when Y > threshold).

## 8. Modulation inputs
- **Routing**: Parameters like Tension and Damping are routable via the global Routing system.

## 9. Spec Open Questions & Options

### Q1: Physics Integration Method
- **Option A (Sub-Stepped)**: Run physics 4-8x per engine tick. Highest quality, most "organic" bounciness. (Recommended)
- **Option B (Standard)**: Once per tick. Lowest CPU, potential for "jitter" at high tension.
- **Option C (Synthetic/Wobbler)**: Damped sine envelope. Zero instability, but less "reactive" to complex hits.

### Q2: Modulation Matrix Complexity
- **Option A (Fixed)**: CV1 = Position Y, CV2 = Velocity Y. Gate = Threshold Crossing.
- **Option B (Flexible)**: User chooses source (Pos X/Y/Z, Vel, Energy) for each output.
- **Option C (Advanced)**: Full matrix allowing multiple physics sources to be summed into a single CV output.

### Q3: Hit Directionality
- **Option A (1D)**: Only Vertical (Y) hits. Simplifies UI.
- **Option B (3D)**: Hits can push the spring in X, Y, or Z. Matches the HTML demo's chaotic feel.

### Q4: Threshold Gate Behavior
- **Option A (Zero Crossing)**: Gate goes high when Y crosses equilibrium.
- **Option B (User Threshold)**: User sets a voltage/position level; gate is high while above.
- **Option C (Trigger on Hit)**: Standard sequencer gate, ignoring physics.

## 10. Engineering Punch list
- [ ] Implement `PhysicalTrack` model with bit-packed fields.
- [ ] Port C++ physics logic from `isometric_spring.html`.
- [ ] Add `PhysicalTrackEngine` to `Engine.h` container.
- [ ] Create UI pages modeled on `NoteTrack`.

## 11. Testing & MVP acceptance
- **Phase A**: Verify physics stability on STM32 using debug prints (no "explosions").
- **Phase B**: Audible check via CV output. Does it "feel" like a spring?
- **Phase C**: Verify Mod Matrix and Threshold Gate reliability.

## 12. RAM Impact & Hardware Gates
The Physical Track must stay within the existing container ceilings to maintain "SRAM-Neutral" status.

- **Model RAM (Main SRAM)**:
    - **Estimated Size**: ~4.5 KB (64 steps @ 11-bits bit-packed, 16 patterns).
    - **Ceiling**: 10.1 KB (`CurveTrack` / `NoteTrack` gate).
    - **Status**: **PASS**. Implementation must not exceed the current `NoteTrack` footprint.
- **Engine RAM (CCMRAM)**:
    - **Estimated Size**: ~120 B (Physics state, metrics, mod matrix cache).
    - **Ceiling**: 912 B (`TeletypeTrackEngine` gate).
    - **Status**: **PASS**. Implementation fits within the existing engine slot waste.
