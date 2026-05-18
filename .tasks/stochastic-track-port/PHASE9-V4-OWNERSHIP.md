# Phase 9: V4 Ownership Correction Plan

## Why V4 is Required

Current UI testing and architecture review reveals a critical mismatch in the Stochastic track: almost all generative controls—most notably the degree tickets, rhythms, and structural modes—are **Track-owned** rather than **Sequence-owned**. 

In Performer's native design (Note, Curve, Tuesday), the `Sequence` represents the pattern snapshot (what changes when you switch patterns 1-16), while the `Track` represents the global performance state. Because Stochastic currently stores its pitch tickets, complexity, rate, and rhythm/melody modes on the `Track`, **switching patterns does not recall different harmonic masks or rhythm styles**. A sequence currently acts only as an arbitrary 64-step loop window.

V4 corrects this to align with Performer/Vinx conventions: **thin Track, pattern-defining Sequence.**

## Reference Ownership Comparison

| Control Type | Vinx Stochastic | XFORMER Note/Indexed | Current XFORMER Stochastic | Proposed V4 Stochastic |
| :--- | :--- | :--- | :--- | :--- |
| **Base Clock/Scale** | Sequence | Sequence | Sequence | **Sequence** |
| **Pattern Material** | Sequence | Sequence | **Track (BUG)** | **Sequence** |
| **Loop Window** | Sequence | Sequence | Sequence | **Sequence** |
| **Live Overrides** | Track | Track | Track | **Sequence** (Density/Rotate) |
| **Global Offsets** | Track | Track | Track | **Track** |

---

## Current Ownership Table

| Field/Control | Current Owner | Current UI | Engine Use | Issue |
| :--- | :--- | :--- | :--- | :--- |
| `degreeTickets[32]` | Track | Tickets Page | Pitch weight | Global; cannot have "Major" vs "Minor" patterns. |
| `rhythm/melodyMode` | Track | Performance | Source resolution | Patterns cannot independently switch Loop/Live. |
| `complexity`, `contour` | Track | Performance | Pitch generation | Switching patterns doesn't change personality. |
| `rate`, `variation`, `rest` | Track | Performance | Rhythm generation | Switching patterns doesn't change rhythmic identity. |
| `burstRate/Count/Pitch` | Track | Performance | Burst generation | All patterns share burst/ratchet style. |
| `range`, `min/maxDegree` | Track | Config/Perf | Pitch filtering | Patterns cannot have different melodic boundaries. |
| `degree/maskRotation` | Track | Performance | Pitch transform | Transpositions are global instead of pattern-specific. |
| `density`, `rotate` | Track | Performance | Live thinning | Currently global, but should be pattern-specific. |
| `sleep`, `patience`, `mutate`, `jump`| Track | Performance | Evolution speed | Currently global, but most belong in the pattern snapshot. |

---

## Proposed Ownership Table

| Field/Control | New Owner | Reason | RAM Impact | Serialization Risk |
| :--- | :--- | :--- | :--- | :--- |
| **Generator Params** (29 fields) | **Sequence** | Defines the rhythmic/melodic "snapshot" of a pattern. | +986 B | Layout change |
| **Pitch Tickets** (`degreeTickets[32]`) | **Sequence** | Melodic material is fundamental to pattern identity. | (incl above)| Layout change |
| **Melodic Boundaries** (`range`, `min/maxDegree`) | **Sequence** | Pattern-specific harmonic state. | (incl above)| Layout change |
| **Performance Overrides** (`density`, `tilt`, `rotate`)| **Sequence** | The pattern remembers its own density and shift state. | (incl above)| Layout change |
| **Evolution Params** (`patience`, `mutate`, `jump`) | **Sequence** | The pattern remembers its own evolution rules. | (incl above)| Layout change |
| **Global Offsets** (`octave`, `transpose`) | **Track** | Standard Performer track-level offset. | 0 | Safe |
| **Global Evolution** (`sleep`) | **Track** | Global behavior over time. | 0 | Safe |
| **Runtime Lock** | **Engine** | Do not serialize. Transient freeze. | 0 | Safe |

### The 29 Fields moving to Sequence:
`degreeTickets[32]`, `rhythmMode`, `melodyMode`, `complexity`, `contour`, `linearity`, `rate`, `variation`, `rest`, `slide`, `legatoProb`, `accentProb`, `marbles`, `burst`, `patience`, `mutate`, `jump`, `tilt`, `range`, `minDegree`, `maxDegree`, `degreeRotation`, `maskRotation`, `density`, `rotate`, `burstRate`, `burstCount`, `burstPitch`.

### The Fields kept on Track:
`cvUpdateMode`, `slideTime`, `octave`, `transpose`, `sleep`, `lock` (runtime-only), `fillMode`, `fillMuted`, legacy biases.

---

## RAM Analysis & The 48-bit Event Enabler

**The Problem:** 
Moving 29 parameters to `StochasticSequence` multiplies their RAM footprint by 17 (16 patterns + 1 snapshot). The `NoteTrack` container limits tracks to `9544 bytes`. With the current 64-bit (8-byte) `StochasticSourceEvent` cache, moving all these fields would push the track to ~10,300 bytes, breaking the limit.

**The Solution:**
We do not rename the `_events[]` array, but we redefine its contents. The current 64-bit struct stores `rate` and `length`, which are completely redundant (they are constant sequence-level parameters). 

By dropping `rate` and `length`, but **keeping `childCount` and `burstRate`**, we can shrink `StochasticSourceEvent` from 8 bytes to **6 bytes (48 bits)**:
- `degree` (7 bits)
- `octave` (4 bits)
- `densityRank` (6 bits)
- `childCount` (3 bits)
- `burstRate` (8 bits)
- `flags` (rest, legato, slide, accent, rhythmValid, melodyValid) (6 bits)
*Total: 34 bits used, 14 free for alignment/expansion inside 48 bits.*

**RAM Math:**
- 48-bit events: 6 bytes * 64 steps = 384 bytes/sequence.
- Sequence metadata (seeds, valid flags): 10 bytes/sequence.
- 29 Sequence-owned fields (incl. 32-byte tickets): ~58 bytes/sequence.
- **Total `StochasticSequence` size:** ~452 bytes.
- **Total Track size (17 sequences):** ~7,684 bytes.

*Net RAM Impact: V4 remains comfortably under the 9544-byte limit. We achieve pure pattern-snapshot semantics without sacrificing child/burst data.*

---

## Implementation Phases

1. **Event Re-packing (The Enabler):** 
   - Redefine `StochasticSourceEvent` in `StochasticTypes.h` as a tightly packed struct (e.g. 48 bits / 6 bytes, or exactly sized bitfields) dropping `rate` and `length`.
   - Ensure `childCount` and `burstRate` are preserved.
2. **Move Fields to Sequence:** 
   - Move `degreeTickets`, generator controls, source modes, `density`, and `rotate` into `StochasticSequence.h`.
3. **Redirect Engine Reads:** 
   - Update `StochasticTrackEngine` and `StochasticGenerator` to read these values from `sequence` rather than `track`.
4. **Move UI Pointers:** 
   - Update `StochasticPerformanceListModel.h` and `StochasticConfigListModel.h` to point to `_track->sequence(selected)` for pattern-defining fields.
5. **Clean Track:** 
   - Remove the old track-owned duplicates.
6. **Verify Pattern Switching:** 
   - Confirm changing patterns correctly recalls independent rhythms, complexities, densities, rotations, and tickets.
7. **Rebuild Visual UI:** 
   - Construct custom Ticket/Generator pages targeting the Sequence.

---

## Serialization Policy

- **No version bump during development.**
- V4 will alter the internal byte layout of `StochasticTrack` and `StochasticSequence`.
- Read/write order must remain strictly symmetric. 
- *Warning:* Existing development save files containing Stochastic tracks from Phase 7/8 will load incorrectly and must be cleared or reset manually.