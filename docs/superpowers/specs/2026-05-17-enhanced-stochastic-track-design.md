# Enhanced Stochastic Track Design

## Overview
The Enhanced Stochastic Track is a probability-driven sequencer that combines the power of step-based probability (Vinx port) with the musical performativity of global stochastic constraints (Vermona Melodicer / SIG) and Marbles-style pitch distribution shaping. The first implementation should focus on a coherent mono track: generate musical pitch/rhythm variation, capture exact evaluated events into a locked loop, and keep RAM/timing behavior suitable for STM32.

## MVP Scope

The first implementation includes:
- Vinx Stochastic model, engine, and core grid UI.
- Compact captured-event lock buffer.
- Degree Ticket Table.
- Linearity.
- Min/Max degree range.
- Marbles spread/bias/steps pitch shaping.
- ProbMeloD-style signed tickets, scale mask exclusion, and degree/mask rotation generalized to Performer scales.
- Accent and slew/legato probability if they can be represented without extra event queues or large model growth.

Deferred feature gates:
- Split rhythm/melody locks.
- Generative drift/mutation of locked buffers.
- Reverse locked-loop playback.
- Polyphonic ghost voices and neighboring-track spillover.

## Core Features

### 1. Step-Based Probability (Vinx Foundation)
- 64 steps per sequence, 16 patterns per track.
- 16 layers of step data:
    - **Gate & Gate Probability:** weighted chance for a step to trigger.
    - **Note, Note Variation & Octave Probability:** range and likelihood of pitch shifts.
    - **Length & Length Variation:** probability-based duration shifts.
    - **Retrigger (Ratchet) & Retrigger Probability.**
    - **Conditionals:** Fill, Not Fill, Pre, Not Pre, etc.
    - **Stage Repeats:** Each, First, Middle, Last, Odd, Even, Triplets, Random.

### 2. Global Stochastic Constraints (NEW - Melodicer/SIG Inspired)
- **Degree Ticket Table:** `CONFIG_USER_SCALE_SIZE` signed ticket values at the track level. The engine uses only the active scale's `notesPerOctave()` entries, so built-in and user scales up to 32 degrees are first-class. `-1` means excluded from the scale-degree mask, `0` means included but zero tickets, and positive values are raffle tickets.
- **Linearity (Melodic Smoothing):** A "Linearity" parameter (0-100%) that biases pitch selection toward notes closer to the current pitch, creating more "singable" melodies vs. wild jumps.
- **Degree Range:** Hard Min/Max scale-degree boundaries for the entire track. Range is a pre-selection filter: degrees outside range do not enter the raffle pool.
- **Degree/Mask Rotation:** ProbMeloD-inspired rotation axes generalized from 12 semitones to active scale degrees:
    - **Degree Rotation:** rotates all active degree ticket values.
    - **Mask Rotation:** rotates only included scale-mask degrees while `-1` excluded notes remain fixed, enabling chord movement within a fixed key mask.
    - Manual/context transforms may destructively rotate the stored table; live performance/routing should prefer non-destructive `degreeRotation` and `maskRotation` offsets.
- **Marbles Distribution Shape:** Optional pitch-selection shaping inspired by Mutable Instruments Marbles X outputs:
    - **Spread:** controls whether generated pitch choices collapse toward the bias point, occupy the full distribution, or jump toward extremes.
    - **Bias:** shifts the center of gravity low/high across the enabled pitch slots.
    - **Steps:** controls how strongly the shaped value is bucketed to discrete pitch slots.
    - The first implementation should adapt the Marbles control law only, not import Marbles' full `OutputChannel`, quantizer, lag processor, register mode, or lookup-table stack.
- **Stochastic Dynamics:** 
    - **Accent Probability:** Chance for a note to be played at high velocity.
    - **Slew/Legato Probability:** Chance for a note to slide or tie to the next.

### 3. Looping & Capture
- **Captured Event Lock:** Transitioning from realtime to locked freezes the evaluated playback events into the lock buffer.
- **Loop Windowing:** Define First and Last steps of the 64-step loop.
- **Loop Rotation:** Shift/Rotate the loop window across the captured buffer. UI should use the **Context Menu + Encoder**, matching the workflow of the GeneratorPage.
- **Locked-loop invariant:** Locked events replay captured gate, CV, length, retrigger, slide, and gate offset. Later edits to source step data must not change the captured event until the buffer is cleared or regenerated.

### 4. Deferred Advanced Features
- **Split Rhythm/Melody Locks:** Independent lock ownership for rhythm and melody is useful, but it changes cache semantics and should wait until the mono captured-event buffer is verified.
- **Generative Drift:** Mutation can be added as an explicit unlocked/mutating mode. It must not blur the base locked-loop invariant.
- **Reverse Playback:** Defer until direct indexed replay is implemented and measured.
- **Polyphonic Ghost Voices:** Additional voices introduce routing, scheduling, and RAM risk. Treat them as a separate feature gate after the mono track passes hardware verification.

---

## Architecture

### Model Layer

#### `StochasticSequence`
- 64 Steps. Each step is 8 bytes (bit-packed).
- `_data0`: Gate(1), Slide(1), Length(4), LenVarRange(4), LenVarProb(4), Note(7), NoteOctave(3), NoteVarProb(4), NoteOctaveProb(4).
- `_data1`: BypassScale(1), Retrigger(3), GateProb(4), RetriggerProb(4), GateOffset(4), Condition(7), StageRepeats(3), StageRepeatMode(3), 5 bits free.
- Sequence-owned scale/root follows existing Performer conventions: `scale = -1` uses the project scale, and `rootNote = -1` uses the project root.

#### `StochasticTrack`
- Contains 16 `StochasticSequence` patterns + 1 Snapshot.
- **Track Settings:**
    - `degreeTickets[CONFIG_USER_SCALE_SIZE]`: int8_t array, where `-1` is excluded from mask and `0..127` are ticket counts.
    - `linearity`: uint8_t.
    - `degreeRotation`, `maskRotation`: int8_t runtime offsets for non-destructive degree/chord movement.
    - `lock`: bool.
    - `loopFirst`, `loopLast`: uint8_t.
    - `accentProb`, `legatoProb`: uint8_t.
    - `marblesMode`: bool or small enum controlling whether pitch selection uses normal weighted mode or Marbles-shaped mode.
    - `marblesSpread`, `marblesBias`, `marblesSteps`: uint8_t controls mapped to 0-100%.
    - `minDegree`, `maxDegree`: uint8_t.
    - Biases: Gate, Retrigger, Length, Note.

**Memory Estimate:**
- `StochasticSequence`: 512 B.
- `StochasticTrack`: (17 * 512) + ~150 B overhead ≈ **8,854 B**.
- Expected to fit within the `NoteTrack` gate (9,544 B), but acceptance requires an STM32 release `sizeof` probe. UI pages, lock buffers, and future advanced features are not covered by this model estimate.

### Engine Layer

#### `StochasticEngine`
- **RAM Optimization:** `_lockedSteps` (loop buffer) will be **heap-allocated** on demand to save CCMRAM.
- **Pitch Selection:** 
    1. Resolve `StochasticSequence::selectedScale(project.scale())` and `selectedRootNote(project.rootNote())`.
    2. Build the allowed degree pool from step gates/probabilities, signed degree-ticket mask, active `scale.notesPerOctave()`, and Min/Max degree range.
    3. Apply Degree/Mask rotation to the ticket table.
    4. Apply Linearity bias (distance from last note).
    5. Convert the selected degree with `Scale::noteToVolts(degree)` and apply root according to existing Performer scale semantics.
- **Marbles-Shaped Pitch Selection:**
    - When enabled, Marbles shaping replaces the final degree choice after the allowed degree set has been determined from step gates, note probabilities, degree tickets, active scale, and range.
    - Generate a uniform random value `u`, shape it with `spread` and `bias`, then bucket it with `steps` onto the enabled degrees.
    - Low spread should collapse toward the bias point; high spread should tend toward low/high edge choices; middle spread should behave like a broad beta-shaped distribution.
    - The evaluated CV is stored in the locked event cache, so locked loops replay the captured Marbles-shaped result and do not change when source step parameters are edited later.
    - Reference source: `../others/mutable/marbles/random/output_channel.cc` (`GenerateNewVoltage`), `../others/mutable/marbles/random/distributions.h` (`BetaDistributionSample`), and `../others/mutable/marbles/random/random_sequence.h` (`NextValue` / deja-vu loop behavior).
- **ProbMeloD Pitch Semantics:**
    - Reference source: `temp-ref/o_c/O_C-Phazerville/software/src/applets/ProbabilityMelody.h`, `temp-ref/o_c/O_C-Phazerville/docs/_applets/ProbMeloD.md`.
    - Adopt signed tickets and scale-mask exclusion.
    - Adopt mask rotation and raffle-ticket UI language.
    - Treat the original 12-slot semitone table as the chromatic case of the generalized degree-ticket table.
    - Do not adopt the Hemisphere linker or dual-clock/two-channel architecture.
    - Deterministic loop seeding is useful for preview/regeneration, but MVP lock remains a captured-event buffer because Stochastic locks gate, CV, length, retrigger, slide, offset, and rest decisions.
- **Rhythm Evaluation:** 
    - Evaluate GateProb, Condition, and global Rest probability.
    - Handle Stage Repeats and Ratcheting.
- **Advanced features excluded from MVP:** No ghost voices, no split rhythm/melody lock buffers, and no drift mutation until the mono event cache is verified.

### UI Layer

UI implementation plan: `.tasks/stochastic-track-port/UI-DESIGN.md`.

#### `StochasticSequenceEditPage`
- 64-step grid editor. Layer switching.
- Shortcuts for Loop Windowing (e.g., Step + Encoder).

#### Visual edit surfaces
- `StochasticSequenceEditPage`: Note/Curve-style 16-step source-programming grid.
- `StochasticPitchPage`: degree tickets, scale/root, Linearity, Min/Max degree range.
- `StochasticMarblesPage`: Spread/Bias/Steps distribution shape.
- `StochasticLockPage`: captured-event lock buffer, windowing, rotation.
- `StochasticTrackPage`: Tuesday-style compact track console for remaining global controls.
- `StochasticSequencePage`: minimal pattern/snapshot list.

Avoid making the primary workflow a `StochasticTrackListModel`. List models may exist for fallback/settings consistency, but degree tickets, Marbles, lock state, and probability overview should be visual pages.

---

## Implementation Phases

1.  **Phase 1: Model & Serialization.**
2.  **Phase 2: Engine Foundations.** Heap buffers, Random integration.
3.  **Phase 3: Global Logic.** Signed degree tickets, scale/root, scale mask, degree/mask rotation, Linearity, Range, Marbles spread/bias/steps pitch shaping.
4.  **Phase 4: Loop Controls.** Captured-event lock buffer, windowing, rotation.
5.  **Phase 5: Dynamics.** Accent/Legato rolls if RAM and queue behavior remain flat.
6.  **Phase 6: UI Development.** XFORMER-native visual pages from `.tasks/stochastic-track-port/UI-DESIGN.md`.
7.  **Phase 7: Validation.** STM32 release size probes and hardware verify.
8.  **Post-MVP Gates.** Split locks, drift, reverse playback, ghost voices.
