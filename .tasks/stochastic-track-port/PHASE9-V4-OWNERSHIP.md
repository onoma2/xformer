# Phase 9: V4 Ownership Correction Plan

> Historical note: V4 ownership is still relevant, but V5 supersedes V4 naming
> where terms conflict. In particular, V4 `Density/Tilt` deterministic thinning
> should now be implemented and shown as `Mask/Tilt`; V5 `Density` is
> generator-level sound/rest amount.

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
| **Live Overrides** | Track | Track | Track | **Sequence** (Density/Tilt) |
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
| `density`, `tilt`, `rotate` | Track | Performance | Live thinning / rank shape / offset | Currently global, but should be pattern-specific. |
| `sleep`, `patience`, `mutate`, `jump`| Track | Performance | Evolution speed | Currently global, but belong in the pattern snapshot. |

---

## Proposed Ownership Table

| Field/Control | New Owner | Reason | RAM Impact | Serialization Risk |
| :--- | :--- | :--- | :--- | :--- |
| **Generator Params** (29 fields) | **Sequence** | Defines the rhythmic/melodic "snapshot" of a pattern. | +986 B | Layout change |
| **Pitch Tickets** (`degreeTickets[32]`) | **Sequence** | Melodic material is fundamental to pattern identity. | (incl above)| Layout change |
| **Melodic Boundaries** (`range`, `min/maxDegree`, `rotation`) | **Sequence** | Pattern-specific harmonic state. | (incl above)| Layout change |
| **Evolution Params** (`sleep`, `patience`, `mutate`, `jump`) | **Sequence** | The pattern remembers its own evolution rules. | (incl above)| Layout change |
| **Pattern Offset** (`rotate`) | **Sequence** | The pattern remembers its own shifted state. | (incl above)| Layout change |
| **Pattern Density** (`density`, `tilt`)| **Sequence** | Deterministic thinning/rest-priority shape is part of pattern identity. | (incl above)| Layout change |
| **Global Offsets** (`octave`, `transpose`) | **Track** | Standard Performer track-level offset. | 0 | Safe |
| **Runtime Lock** | **Engine** | Do not serialize. Transient freeze. | 0 | Safe |

### Fields moving to Sequence:
Do not use a magic count here; implementation must move exact C++ members and then prove size on STM32.

Pattern-owned pitch/material:
`degreeTickets[32]`, `degreeRotation`, `maskRotation`, `range`, `minDegree`, `maxDegree`.

Pattern-owned source topology:
`rhythmMode`, `melodyMode`, `size`, `first`, `last`, source validity flags, source seeds, source event cache.

Pattern-owned generator controls:
`complexity`, `contour`, `linearity`, `rate`, `variation`, `rest`, `slide`, `legatoProb`, `accentProb`, `marblesMode`, `marblesSpread`, `marblesBias`, `marblesSteps`, `burst`, `burstRate`, `burstCount`, `burstPitch`.

Pattern-owned evolution, thinning, and offset:
`density`, `tilt`, `sleep`, `patience`, `mutate`, `jump`, `rotate`.

### The Fields kept on Track:
`cvUpdateMode`, `slideTime`, `octave`, `transpose`, `lock` (runtime-only), `fillMode`, `fillMuted`, legacy biases.

`density` and `tilt` are pattern-owned because they define the deterministic ranked rest-priority skeleton. Switching patterns must recall the same thinning shape that created/plays that pattern.

---

## RAM Analysis & The 48-bit Event Enabler

**The Problem:** 
Moving parameters to `StochasticSequence` multiplies their RAM footprint by 17 (16 patterns + 1 snapshot). The `NoteTrack` container limits tracks to `9544 bytes`. With the current 64-bit (8-byte) `StochasticSourceEvent` cache, moving all these fields would push the track to ~10,300 bytes, breaking the limit.

**The Solution:**
We will aggressively bit-pack the existing `StochasticSourceEvent` fields down to exactly **48 bits (6 bytes)**.

### Existing Rate Cache Bug

The current implementation already has a range bug:
- `StochasticTrack::setRate()` clamps the control/base value to `1..400`.
- `StochasticGenerator::generateRhythmEvent()` writes `track.rate()` into `event.d0.rate`.
- `StochasticSourceEvent::d0.rate` is currently only 8 bits.
- `BitField::operator=` masks, it does not clamp, so values above `255` truncate/wrap.
- `StochasticTrackEngine::triggerStep()` later reads the truncated cached value to choose duration.

Therefore V4 must not store raw UI/control `rate` in the event cache. It must store an evaluated duration bucket.

### Event Cache Semantics

The source event cache stores evaluated loop material, not raw control positions.

For rhythm, store:
- `durationIndex`: evaluated duration bucket, e.g. `0..7` for `1 bar`, `1/2`, `1/4`, `1/8`, `1/16`, `1/32`, `1/64`, `1/128`.
- optional `lengthIndex` or gate-length bucket only if it is actually used by the engine. Do not carry a fake `length = 128` field forward just because the current stub writes it.
- `densityRank`: deterministic rest-priority rank for loop thinning.
- `childCount`
- `burstRate`
- flags: `rest`, `legato`, `slide`, `accent`, `rhythmValid`, `melodyValid`.

For melody, store:
- `degree`
- `octave`

Initial 48-bit allocation target:
- `durationIndex` (4 bits, 0-15)
- `lengthIndex` (4 bits, 0-15, only if needed; otherwise reserve)
- `densityRank` (6 bits, 0-63 max steps)
- `childCount` (3 bits, 0-7)
- `burstRate` (7 bits, 0-127)
- `degree` (7 bits, 0-127)
- `octave` (4 bits, mapped to -8..7)
- `flags` (rest, legato, slide, accent, rhythmValid, melodyValid) (6 bits)
*Total: 41 bits used if `lengthIndex` is kept, 7 free bits.*

`densityRank` no longer uses `255` as an invalid sentinel. Validity must come from `rhythmValid`, `melodyValid`, and sequence `size`. Events outside `size` should be cleared and ignored.

**Concrete Packed Representation:**
Native bitfields inside a `uint64_t` or across multiple `uint32_t`s can easily pad out to 8 bytes depending on compiler alignment. We must require an explicit byte-level layout to guarantee 6 bytes:
```cpp
struct StochasticSourceEvent {
    uint8_t bytes[6];
    
    // Internal accessors will bit-shift into/out of bytes array to guarantee packing
    // ...
};
static_assert(sizeof(StochasticSourceEvent) == 6, "Event must be exactly 6 bytes");
```

**RAM Math (Estimate Only):**
- 48-bit events: 6 bytes * 64 steps = 384 bytes/sequence.
- Sequence metadata (seeds, valid flags): 10 bytes/sequence.
- Sequence-owned fields (incl. 32-byte tickets): ~60 bytes/sequence.
- **Estimated `StochasticSequence` size:** ~454 bytes.
- **Estimated Track size (17 sequences):** ~7,718 bytes.

*Note: This is an estimate only. It omits remaining StochasticTrack fields, padding, container alignment, and any Routable<> storage overhead. This step requires an STM32 `sizeof` proof before full implementation.*

---

## Routing Consequences

Many fields being moved to `StochasticSequence` are currently `Routable<>` on the Track (e.g., `density`, `tilt`, `rate`, `variation`, `patience`, `mutate`, `jump`).
When these controls become Sequence-owned, we must define their modulation semantics. Performer's standard is that modulation applies to the *currently active sequence*.
We must ensure `RoutingEngine` correctly targets `track.sequence(selectedPatternIndex)` for these fields, so CV modulation tracks pattern changes properly and doesn't silently break or write to global track state.

Implementation rule:
- Base values for moved controls live in `StochasticSequence`.
- Existing routing target IDs are preserved.
- Routing must apply as a live overlay to the active selected sequence value, matching the current `Routable<>` behavior where possible.
- Do not store routed/modulated values into all patterns.
- Do not silently keep a track-global routed value for a sequence-owned parameter.
- Before embedding `Routable<>` per sequence, measure RAM. If too expensive, use compact base fields in `StochasticSequence` plus routing overlay evaluation in `StochasticTrack`/routing layer.

---

## Implementation Phases

1. **Event Re-packing (The Enabler):** 
   - Redefine `StochasticSourceEvent` in `StochasticTypes.h` to `uint8_t bytes[6]` with a `static_assert(sizeof(...) == 6)`.
   - Update read/write logic with strict bit-shifting to process exactly 6 bytes per event.
   - Replace cached raw `rate` with evaluated `durationIndex`.
   - Remove or redefine the current fake `length = 128` cache field.
2. **Move Fields to Sequence:** 
   - Move `degreeTickets`, generator controls, source modes, `rotate`, and evolution controls (`sleep`, `patience`, `mutate`, `jump`) into `StochasticSequence.h`.
3. **Redirect Engine Reads & Routing:** 
   - Update `StochasticTrackEngine` and `StochasticGenerator` to read these values from `sequence` rather than `track`.
   - Update `Routing` to target the active sequence for Sequence-owned routable parameters.
4. **Move UI Pointers:** 
   - Update `StochasticPerformanceListModel.h` and `StochasticConfigListModel.h` to point to `_track->sequence(selected)`.
5. **Clean Track:** 
   - Remove the old track-owned duplicates.
6. **Verify Pattern Switching & RAM:** 
   - **Must check STM32 aggregate BSS to verify `StochasticTrack <= NoteTrack`.**
   - Confirm changing patterns correctly recalls independent rhythms, complexities, and tickets.

---

## Serialization Policy

- **No version bump during development.**
- V4 will alter the internal byte layout of `StochasticTrack` and `StochasticSequence`.
- Read/write order must remain strictly symmetric. 
- *Warning:* Existing development save files containing Stochastic tracks from Phase 7/8 will load incorrectly and must be cleared or reset manually.

---

## User Decision Checks (Resolved)

1. **Tickets/Mask/Range:** Sequence-owned. (Enabled by 48-bit packing).
2. **Event Cache Semantics:** Preserve evaluated loop semantics, not raw control values. Store evaluated duration bucket, not raw `Rate 1..400`.
3. **Rhythm/Melody Modes:** Sequence-owned.
4. **Density/Tilt:** Sequence-owned (pattern-specific deterministic thinning).
5. **Sleep/Patience/Mutate/Jump:** Sequence-owned (Pattern-specific evolution).
6. **Rotate:** Sequence-owned (Pattern-specific shift).
7. **Lock:** Engine-only (Runtime transient state).

---

## Fixes Required Before Implementation

1. Fix the current `Rate` cache bug as part of V4: raw `rate` values `256..400` must not be written into an 8-bit or 7-bit event field.
2. Replace cached raw `rate` with an evaluated `durationIndex`.
3. Remove or redefine the unused/fake cached `length` field; current `length = 128` cannot fit in the proposed small field and is not meaningful unless the engine consumes it.
4. Move `density` and `tilt` together. They are sequence-owned because density thinning and tilt-biased rank shape are part of the pattern's recalled rhythmic identity.
5. Use exact member lists and STM32 `sizeof` probes instead of estimated field counts.
6. Define `densityRank` validity via flags and sequence size; do not rely on a `255` sentinel after packing to 6 bits.
7. Define routing overlay semantics before embedding `Routable<>` in every pattern, because per-sequence `Routable<>` can erase the RAM win.
8. Preserve the packed-event write invariant from commit `88e28621`: rhythm and melody domains must be written through raw-preserving setters/merge helpers only. Do not assign packed bitfield members directly. The Phase 8 silent-gate regression happened because melody writes into `d1.raw` cleared `rhythmValid`, so every generated event evaluated as a rest. Current engine reference: `StochasticTrackEngine::triggerStep()` must combine domains with `StochasticSourceEvent::mergeRhythmFrom()` and `mergeMelodyFrom()`.
