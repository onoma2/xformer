# Tuesday Track - Feasibility Evaluation & Phase 0 TDD Plan

## Intent & Vision

### What Tuesday Track Is

The Tuesday track represents a **paradigm shift** from Performer's existing manual-programming model to **generative/algorithmic sequencing**. Instead of programming each step by hand, users set high-level parameters and let algorithms generate musical patterns in real-time.

**Existing Track Types (Manual Programming):**
- **NoteTrack**: User programs each of 64 steps with note, gate, length, etc.
- **CurveTrack**: User draws CV curves point by point
- **MidiCvTrack**: Converts external MIDI input to CV

**Tuesday Track (Generative):**
- User selects an **algorithm** and tunes **4 musical parameters**
- Algorithm generates CV/Gate patterns based on parameters
- Output evolves based on mathematical/musical rules
- Less predictable, more "alive" - the sequencer becomes a performer

### Design Philosophy

The Tuesday track is inspired by generative Eurorack modules like:
- **Mutable Instruments Marbles** - random/structured CV generation
- **Noise Engineering Mimetic Digitalis** - algorithmic sequencing
- **Intellijel Metropolix** - probability-based step modulation

The 5 parameters are intentionally **high-level musical controls** rather than technical settings:

| Parameter | Range | Musical Meaning |
|-----------|-------|-----------------|
| **Algorithm** | 0-31 | The "personality" - MARKOV (probabilistic), STOMPER (rhythmic), SCALEWALK (melodic), etc. |
| **Flow** | 0-16 | How the sequence moves - 0=static, 16=chaotic |
| **Ornament** | 0-16 | Embellishments and fills - 0=none, 16=heavily decorated |
| **Power** | 0-16 | Activity/density - 0=silent, 16=maximum gates |
| **LoopLength** | Inf, 1-16, 19, 21, 24, 32, 35, 42, 48, 56, 64 | Pattern length (Inf = evolving/non-repeating) |
| **Glide** | 0-100% | Slide probability - 0=no slides, 100=always slide |
| **Scale** | Free/Project | Note quantization - Free=chromatic, Project=use project scale |
| **Skew** | -8 to +8 | Density curve across loop - 0=flat, positive=build-up, negative=fade-out |

**Default = 0**: All parameters default to 0, producing silence. Increase values to add activity.

### Skew Parameter Behavior

The Skew parameter creates density variations across the loop length:

**Positive Skew (build-up):**
- **Skew 8**: First 50% uses Power setting, last 50% at power 16 (maximum density)
- **Skew 4**: First 75% uses Power setting, last 25% at power 16
- **Skew 2**: First 87.5% uses Power setting, last 12.5% at power 16

**Negative Skew (fade-out):**
- **Skew -8**: First 50% at power 16, last 50% uses Power setting
- **Skew -4**: First 25% at power 16, last 75% uses Power setting

**Formula:** `|skew|/16 = fraction at maximum density`

### Reseed Functionality

**Shift+F5**: Reseeds the loop with new random values while preserving current parameters.

**Context Menu**: TRACK page context menu includes RESEED option for Tuesday tracks.

The reseed operation:
1. Resets step index to 0
2. Generates new random seeds from current RNG state
3. Reinitializes algorithm with new seeds
4. Produces completely new pattern with same Flow/Ornament values

### Why F0-F4 + Encoder Control

Unlike NoteTrack which uses step buttons (S1-S16) to select steps for editing, Tuesday track uses **F0-F4 to select parameters** because:

1. **No steps to edit** - patterns are generated, not programmed
2. **Real-time tweaking** - parameters should be adjustable during playback like synthesizer knobs
3. **5 parameters map directly** to F0-F4 function buttons
4. **Performance-oriented** - encourages live manipulation

### Use Cases

1. **Generative bass lines** - Algorithm generates evolving bass patterns
2. **Random melodies** - Constrained randomness within musical scales
3. **Rhythmic variations** - Algorithmically varied drum triggers
4. **Ambient CV** - Slowly evolving modulation sources
5. **Live performance** - Tweak parameters to "play" the algorithm

### UI Mockup - What It Looks Like

**Main TuesdayPage (OLED 256x64):**

```
┌────────────────────────────────────────────────────────────┐
│ T1  TUESDAY                              120.0 BPM  ▶ P01  │
├────────────────────────────────────────────────────────────┤
│                                                            │
│  MARKOV       8          5         12         16           │
│             ████░░░░   ███░░░░░   ████████░               │
│                                                            │
│                                                            │
│                                                            │
├────────────────────────────────────────────────────────────┤
│  ALGO       FLOW       ORN        POWER      LOOP          │
└────────────────────────────────────────────────────────────┘
```

Algorithm shows name only. Flow/Ornament/Power show value + bar graph. Loop shows value only.

**Interaction Flow:**

1. **Press F1** → Select ALGO parameter
   - Turn encoder → Cycle through: MARKOV → STOMPER → SCALEWALK → WOBBLE → ...

2. **Press F2** → Select FLOW parameter
   - Turn encoder → Adjust 0-16
   - Bar graph updates in real-time

3. **Press F3** → Select ORN (Ornament) parameter
   - Turn encoder → Adjust 0-16
   - Bar graph updates in real-time

4. **Press F4** → Select POWER parameter
   - Turn encoder → Adjust 0-16
   - Bar graph updates in real-time

5. **Press F5** → Select LOOP parameter
   - Turn encoder → Cycle: Inf, 1-16, 19, 21, 24, 32, 35, 42, 48, 64
   - Sets pattern loop length

**Visual Feedback During Playback:**

```
┌────────────────────────────────────────────────────────────┐
│ T1  TUESDAY                              120.0 BPM  ▶ P01  │
├────────────────────────────────────────────────────────────┤
│                                              ┌──────────┐  │
│  STOMPER      16         3         14        │ ●  C4    │  │
│             ████████   ██░░░░░░   █████████  │ CV: 2.1V │  │
│                                        Inf   │ GT: HIGH │  │
│                                              └──────────┘  │
│                                                            │
├────────────────────────────────────────────────────────────┤
│ >ALGO       FLOW       ORN        POWER      LOOP          │
└────────────────────────────────────────────────────────────┘
```

The `>` indicator shows currently selected parameter. Activity box (top-right) shows:
- Current note (C4)
- CV output voltage
- Gate state (HIGH/LOW)

**Comparison with NoteSequenceEditPage:**

| Aspect | NoteTrack | Tuesday Track |
|--------|-----------|---------------|
| Main display | 16 step grid with notes | 5 parameter columns with values + bars |
| F1-F5 | Gate/Retrig/Length/Note/Cond layers | ALGO/FLOW/ORN/POWER/LOOP params |
| S1-S16 | Select/toggle steps | Not used (no steps) |
| Encoder | Edit selected step value | Edit selected parameter |
| Visual focus | Step data | Parameter values + output monitor |

**Track Selection View (OverviewPage):**

```
┌────────────────────────────────────────────────────────────┐
│ OVERVIEW                                                   │
├────────────────────────────────────────────────────────────┤
│ T1 ████████████████  Note    C maj                         │
│ T2 ████░░░░████░░░░  Note    A min                         │
│ T3 ▀▀▀▀▀▄▄▄▀▀▀▀▀▄▄▄  Curve   LFO                           │
│ T4 ░░██░░██░░██░░██  Tuesday MARKOV                        │  ← Tuesday shows algorithm
│ T5 ────────────────  --                                    │
│ T6 ────────────────  --                                    │
│ T7 ────────────────  --                                    │
│ T8 ────────────────  --                                    │
└────────────────────────────────────────────────────────────┘
```

Tuesday track in Overview shows algorithm name instead of scale/sequence info.

### Why "Tuesday"

The name references the **Tuesday** Eurorack module - a generative algorithmic sequencer that produces tightly coupled gate+pitch pairs using various musical algorithms.

---

## Architecture Overview

The Performer follows strict **Model → Engine → UI** layered architecture:

```
┌─────────────────────────────────────┐
│            UI Layer                 │  ← Step 3: TuesdayPage
│  (pages, controllers, painters)     │
├─────────────────────────────────────┤
│          Engine Layer               │  ← Step 2: TuesdayTrackEngine
│  (tick, gate/CV output, routing)    │
├─────────────────────────────────────┤
│          Model Layer                │  ← Step 1: TuesdayTrack
│  (data storage, serialization)      │
└─────────────────────────────────────┘
```

Each layer only depends on layers below it. Engine references Model. UI references both.

---

## FEASIBILITY EVALUATION

### Technical Assessment: ✅ FULLY SUPPORTED

The architecture is well-designed for new track types:

| Layer | Pattern | Tuesday Integration |
|-------|---------|---------------------|
| **Model** | `Container<NoteTrack, CurveTrack, MidiCvTrack>` | Add `TuesdayTrack` to template |
| **Engine** | `Container<NoteTrackEngine, CurveTrackEngine, MidiCvTrackEngine>` | Add `TuesdayTrackEngine` |
| **UI** | `Pages` struct with track-specific pages | Add `TuesdayPage` |

**Key Advantages:**
- Tuesday track is simpler than existing tracks (5 parameters vs 17 sequences)
- No sequence array needed - follows MidiCvTrack pattern (simplest track type)
- Memory footprint ~20 bytes vs ~50KB for NoteTrack
- Well-documented existing patterns to follow

### Effort Estimation

| Category | Count | Complexity |
|----------|-------|------------|
| Files to create | 7 | Low |
| Files to modify | 13 | Medium (mechanical switch statements) |
| Total switch statements to update | 25+ | Medium |

**Overall Complexity: MEDIUM**

The complexity is medium because:
- The patterns are well-established and documented
- Tuesday track is simpler than existing tracks (no sequences)
- Many files need modification but changes are mechanical (adding switch cases)
- Algorithm implementation (the actual musical logic) adds complexity but is isolated

### Risk Assessment

| Risk | Level | Mitigation |
|------|-------|------------|
| Missing switch statements | Medium | Grep for `TrackMode::` before completion |
| Serialization compatibility | Low | Version35 follows established pattern |
| Memory constraints | Low | TuesdayTrack is smallest track type |
| Algorithm real-time performance | Medium | Profile early, must be <50μs/tick |
| LaunchpadController integration | Medium | Has 12+ switch statements - easy to miss one |

### Dependencies

**Must Exist First:**
- Build environment (simulator minimum)
- Understanding of algorithm logic (MARKOV, STOMPER, etc.)
- Familiarity with UnitTest.h framework (NOT Catch2/gtest)

**External Dependencies:**
- None - all required libraries already integrated

---

